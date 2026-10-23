// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Holograms/Core/SFSignSupportMode.h"
#include "SmartFoundations.h"
#include "Equipment/FGBuildGunBuild.h"
#include "Hologram/FGStandaloneSignHologram.h"
#include "Hologram/FGSignPoleHologram.h"
#include "Hologram/FGPoleHologram.h"
#include "FGPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Features/Scaling/SFScalingFootprint.h"
#include "Holograms/Core/SFSignHologramState.h"
#include "UObject/UnrealType.h"

namespace
{
// SP/MP DIVERGENCE MAP — per-placement sign support choice
//  1. Local standalone/listen/client preview — weak-root choice controls native pose/support state.
//  2. Dedicated/remote authority — no UI choice is inferred from another player's local state;
//     native construction serialization and the existing staged grid spec carry pose/disabled support.
//  3. Local success acknowledgement — reset only the captured root/revision/recipe; a new root defaults Auto.
struct FSFSignSupportChoice
{
	SFSignSupportMode::EMode Mode = SFSignSupportMode::EMode::Auto;
	uint64 Revision = 0;
	bool bApplied = false;
	FTransform NativePose;
	FTransform NativeSupportPose;
	FTransform AppliedPose;
	ESignHologramBuildStep NativeStep = ESignHologramBuildStep::ESHBS_SnapToActor;
	FIntVector AppliedGrid = FIntVector::ZeroValue;
	FIntVector AppliedVerticalTransforms = FIntVector::ZeroValue;
};
struct FSFSignSupportRequest
{
	TWeakObjectPtr<AFGHologram> Hologram;
	TWeakObjectPtr<UClass> Recipe;
	uint64 Revision = 0;
};
TMap<TWeakObjectPtr<AFGHologram>, FSFSignSupportChoice> Choices;
TMap<TWeakObjectPtr<UFGBuildGunStateBuild>, FSFSignSupportRequest> Requests;
uint64 NextRevision = 0;

AFGSignPoleHologram* GetModeSupport(AFGStandaloneSignHologram* Sign)
{
	const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
		AFGStandaloneSignHologram::StaticClass(), TEXT("mChildSignPoleHologram"));
	return Property ? Cast<AFGSignPoleHologram>(Property->GetObjectPropertyValue_InContainer(Sign)) : nullptr;
}
ESignHologramBuildStep* GetModeStep(AFGStandaloneSignHologram* Sign)
{
	FProperty* Property = AFGStandaloneSignHologram::StaticClass()->FindPropertyByName(TEXT("mBuildStep"));
	return Property ? Property->ContainerPtrToValuePtr<ESignHologramBuildStep>(Sign) : nullptr;
}
void PruneSignChoices()
{
	for (auto It = Choices.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
	for (auto It = Requests.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
}
bool SetAlignedSignLocation(AFGStandaloneSignHologram* Sign, const FVector& Location)
{
	FVector* LockLocation = nullptr;
	if (Sign->IsHologramLocked())
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(
			AFGHologram::StaticClass(), TEXT("mHologramLockLocation"));
		if (!Property || Property->Struct != TBaseStructure<FVector>::Get()) return false;
		LockLocation = Property->ContainerPtrToValuePtr<FVector>(Sign);
	}
	const FVector Delta = Location - Sign->GetActorLocation();
	// Native locked placement reapplies this serialized anchor plus its nudge offset.
	// Move the anchor with our alignment while retaining the player's nudge unchanged.
	if (LockLocation) *LockLocation += Delta;
	Sign->SetActorLocation(Location);
	return true;
}
void SynchronizeSignTargetNudge(AFGStandaloneSignHologram* Sign, AFGHologram* PreviousTarget, AFGHologram* Target)
{
	if (!Sign->IsHologramLocked() || !IsValid(PreviousTarget) || !IsValid(Target) || PreviousTarget == Target) return;
	const FStructProperty* Property = FindFProperty<FStructProperty>(
		AFGHologram::StaticClass(), TEXT("mHologramLockLocation"));
	if (!Property || Property->Struct != TBaseStructure<FVector>::Get()) return;
	const FVector Nudge = PreviousTarget->GetHologramNudgeOffset();
	const FTransform TargetPose = Target->GetActorTransform();
	// Native position locking includes the root and its owned companion even when
	// the pole is disabled. Keep that lock tree intact when the nudge target changes.
	// The new target must reproduce the visible pose as anchor + the existing nudge.
	if (!Target->IsHologramLocked()) Target->LockHologramPosition(true);
	*Property->ContainerPtrToValuePtr<FVector>(Target) = TargetPose.GetLocation() - Nudge;
	Target->SetNudgeOffset(Nudge);
	Target->SetActorTransform(TargetPose);
}
void RestoreNativeSignPose(AFGStandaloneSignHologram* Sign, FSFSignSupportChoice& Choice)
{
	if (!Choice.bApplied) return;
	// Undo only our alignment displacement, preserving a subsequent native nudge or rotation.
	if (AFGSignPoleHologram* Support = GetModeSupport(Sign); IsValid(Support))
	{
		AFGHologram* PreviousTarget = Sign->GetNudgeHologramTarget();
		const FVector NudgeDelta = Sign->GetActorLocation() - Choice.AppliedPose.GetLocation();
		const FQuat RotationDelta = Sign->GetActorQuat() * Choice.AppliedPose.GetRotation().Inverse();
		if (!SetAlignedSignLocation(Sign, Choice.NativePose.GetLocation() + NudgeDelta)) return;
		FTransform SupportPose = Choice.NativeSupportPose;
		SupportPose.AddToTranslation(NudgeDelta);
		SupportPose.SetRotation(RotationDelta * SupportPose.GetRotation());
		Support->SetActorTransform(SupportPose);
		Support->SetDisabled(false);
		if (ESignHologramBuildStep* Step = GetModeStep(Sign)) *Step = Choice.NativeStep;
		SynchronizeSignTargetNudge(Sign, PreviousTarget, Sign->GetNudgeHologramTarget());
	}
	Choice.bApplied = false;
}
void ApplySignSupportChoice(AFGStandaloneSignHologram* Sign, FSFSignSupportChoice& Choice)
{
	if (Choice.Mode != SFSignSupportMode::EMode::None || Choice.bApplied) return;
	AFGSignPoleHologram* Support = GetModeSupport(Sign);
	ESignHologramBuildStep* Step = GetModeStep(Sign);
	// Disabled native companions belong to wall/flat/direct mounts. Their old pole origin is not a surface.
	if (!IsValid(Support) || Support->IsDisabled() || !Step) return;
	const double SurfaceDirection = Support->GetActorUpVector().Z;
	if (FMath::Abs(SurfaceDirection) < 0.999 || FMath::Abs(Sign->GetActorUpVector().Z) < 0.999) return;
	const double Height = Sign->GetSignHologramWorldDimensions().Y;
	if (!FMath::IsFinite(Height) || Height <= 0.0) return;
	Choice.NativePose = Sign->GetActorTransform();
	Choice.NativeSupportPose = Support->GetActorTransform();
	Choice.NativeStep = *Step;
	AFGHologram* PreviousTarget = Sign->GetNudgeHologramTarget();
	// Grid Z, spacing and steps can put the mounting edge on a child rather than the parent.
	// The shared position calculator's vertical component is affine, so its eight corners
	// contain the extreme even for negative counters, negative pitch and curved/staggered rows.
	double EdgeOffset = 0.0;
	if (USFSubsystem* Subsystem = USFSubsystem::Get(Sign->GetWorld()))
	{
		FSFCounterState Counters = Subsystem->GetCounterState();
		// Grid regeneration updates the live grid before its CounterState mirror.
		Counters.GridCounters = Subsystem->GetGridCounters();
		Choice.AppliedGrid = Counters.GridCounters;
		Choice.AppliedVerticalTransforms = FIntVector(Counters.SpacingZ, Counters.StepsX, Counters.StepsY);
		FVector ItemSize = USFBuildableSizeRegistry::GetProfile(Sign->GetBuildClass()).DefaultSize;
		SFScalingFootprint::TryGetDimensions(Sign, ItemSize);
		const FIntVector Last((FMath::Abs(Counters.GridCounters.X) - 1) * (Counters.GridCounters.X < 0 ? -1 : 1),
			(FMath::Abs(Counters.GridCounters.Y) - 1) * (Counters.GridCounters.Y < 0 ? -1 : 1),
			(FMath::Abs(Counters.GridCounters.Z) - 1) * (Counters.GridCounters.Z < 0 ? -1 : 1));
		if (FSFPositionCalculator* Calculator = Subsystem->GetPositionCalculator())
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const double Offset = Calculator->CalculateChildPosition((Corner & 1) ? Last.X : 0,
					(Corner & 2) ? Last.Y : 0, (Corner & 4) ? Last.Z : 0, FVector::ZeroVector,
					Sign->GetActorRotation(), ItemSize, Counters).Z;
				EdgeOffset = SurfaceDirection > 0.0 ? FMath::Min(EdgeOffset, Offset) : FMath::Max(EdgeOffset, Offset);
			}
	}
	FVector Location = Sign->GetActorLocation();
	Location.Z = Support->GetActorLocation().Z + (SurfaceDirection > 0.0 ? Height * 0.5 : -Height * 0.5) - EdgeOffset;
	if (!SetAlignedSignLocation(Sign, Location)) return;
	// Retain the native mounting point even if the child is attached to the moving parent.
	Support->SetActorTransform(Choice.NativeSupportPose);
	Support->SetDisabled(true);
	*Step = ESignHologramBuildStep::ESHBS_SnapToActor;
	SynchronizeSignTargetNudge(Sign, PreviousTarget, Sign->GetNudgeHologramTarget());
	Choice.AppliedPose = Sign->GetActorTransform();
	Choice.bApplied = true;
}
void RefreshSignChoiceGrid(AFGHologram* Hologram)
{
	if (USFSubsystem* Subsystem = USFSubsystem::Get(Hologram->GetWorld()))
	{
		Subsystem->UpdateChildrenForCurrentTransform();
		SFSignHologramState::SyncGridPreview(Hologram);
	}
}
}

bool SFSignSupportMode::IsAvailable(const AFGHologram* Hologram)
{
	if (!IsValid(Hologram) || !Hologram->IsA<AFGStandaloneSignHologram>() || Hologram->GetParentHologram()
		|| Cast<AFGStandaloneSignHologram>(Hologram)->GetZoopInstanceTransforms().Num() > 0) return false;
	const APawn* Pawn = Hologram->GetConstructionInstigator();
	const AFGPlayerController* Controller = Pawn ? Cast<AFGPlayerController>(Pawn->GetController()) : nullptr;
	const USFSubsystem* Subsystem = USFSubsystem::Get(Hologram->GetWorld());
	// [MP-CLIENT] Only the owning local preview has placement controls, including a listen host.
	return Controller && Controller->IsLocalController() && Subsystem && Subsystem->IsSmartEnabledForSession();
}
SFSignSupportMode::EMode SFSignSupportMode::Get(const AFGHologram* Hologram)
{
	const FSFSignSupportChoice* Choice = Choices.Find(const_cast<AFGHologram*>(Hologram));
	return Choice ? Choice->Mode : EMode::Auto;
}
FText SFSignSupportMode::GetLabel(const AFGHologram* Hologram)
{
	return Get(Hologram) == EMode::None
		? NSLOCTEXT("SmartFoundations", "SignSupports_None", "Sign supports: None")
		: NSLOCTEXT("SmartFoundations", "SignSupports_Auto", "Sign supports: Auto");
}
void SFSignSupportMode::Toggle(AFGHologram* Hologram)
{
	if (!IsAvailable(Hologram)) return;
	PruneSignChoices();
	AFGStandaloneSignHologram* Sign = CastChecked<AFGStandaloneSignHologram>(Hologram);
	FSFSignSupportChoice& Choice = Choices.FindOrAdd(Hologram);
	RestoreNativeSignPose(Sign, Choice);
	Choice.Mode = Choice.Mode == EMode::Auto ? EMode::None : EMode::Auto;
	Choice.Revision = ++NextRevision;
	ApplySignSupportChoice(Sign, Choice);
	RefreshSignChoiceGrid(Hologram);
}
void SFSignSupportMode::BeforeNativePlacement(AFGHologram* Hologram)
{
	if (IsValid(Hologram) && Hologram->IsA<AFGStandaloneSignHologram>() && !IsAvailable(Hologram))
	{
		if (FSFSignSupportChoice* Choice = Choices.Find(Hologram)) RestoreNativeSignPose(CastChecked<AFGStandaloneSignHologram>(Hologram), *Choice);
		Choices.Remove(Hologram);
		return;
	}
	if (IsAvailable(Hologram) && !Hologram->IsHologramLocked())
		if (FSFSignSupportChoice* Choice = Choices.Find(Hologram)) RestoreNativeSignPose(CastChecked<AFGStandaloneSignHologram>(Hologram), *Choice);
}
void SFSignSupportMode::AfterNativePlacement(AFGHologram* Hologram)
{
	if (!IsAvailable(Hologram)) return;
	if (FSFSignSupportChoice* Choice = Choices.Find(Hologram))
	{
		USFSubsystem* Subsystem = USFSubsystem::Get(Hologram->GetWorld());
		FSFCounterState Counters = Subsystem->GetCounterState();
		Counters.GridCounters = Subsystem->GetGridCounters();
		const bool bGridChanged = Choice->AppliedGrid != Counters.GridCounters
			|| Choice->AppliedVerticalTransforms != FIntVector(Counters.SpacingZ, Counters.StepsX, Counters.StepsY);
		if (Hologram->IsHologramLocked() && !bGridChanged) return;
		if (Choice->bApplied) RestoreNativeSignPose(CastChecked<AFGStandaloneSignHologram>(Hologram), *Choice);
		ApplySignSupportChoice(CastChecked<AFGStandaloneSignHologram>(Hologram), *Choice);
	}
}
void SFSignSupportMode::BeforeGridUpdate(AFGHologram* Hologram)
{
	if (!IsAvailable(Hologram)) return;
	FSFSignSupportChoice* Choice = Choices.Find(Hologram);
	if (!Choice || Choice->Mode != EMode::None) return;
	USFSubsystem* Subsystem = USFSubsystem::Get(Hologram->GetWorld());
	const FSFCounterState& Counters = Subsystem->GetCounterState();
	if (Choice->bApplied && Choice->AppliedGrid == Subsystem->GetGridCounters()
		&& Choice->AppliedVerticalTransforms == FIntVector(Counters.SpacingZ, Counters.StepsX, Counters.StepsY)) return;
	// A locked hologram skips native placement callbacks. Recompute before the
	// spawner captures its root transform, without recursively refreshing children.
	RestoreNativeSignPose(CastChecked<AFGStandaloneSignHologram>(Hologram), *Choice);
	ApplySignSupportChoice(CastChecked<AFGStandaloneSignHologram>(Hologram), *Choice);
}
void SFSignSupportMode::BeginConstruction(UFGBuildGunStateBuild* BuildState)
{
	AFGHologram* Hologram = BuildState ? BuildState->GetHologram() : nullptr;
	if (!IsAvailable(Hologram)) return;
	PruneSignChoices();
	if (const FSFSignSupportChoice* Choice = Choices.Find(Hologram); Choice && Choice->Mode == EMode::None)
		Requests.Add(BuildState, {Hologram, Hologram->GetRecipe().Get(), Choice->Revision});
}
void SFSignSupportMode::OnRecipeBuilt(UFGBuildGunStateBuild* BuildState, TSubclassOf<UFGRecipe> Recipe, int32 Count)
{
	FSFSignSupportRequest Request;
	const FSFSignSupportRequest* Pending = Requests.Find(BuildState);
	if (Count <= 0 || !Pending || Pending->Recipe.Get() != Recipe.Get()) return;
	Requests.RemoveAndCopyValue(BuildState, Request);
	AFGHologram* Hologram = Request.Hologram.Get();
	FSFSignSupportChoice* Choice = Choices.Find(Hologram);
	// A late acknowledgement must not reset a later choice, another recipe, or another player's preview.
	if (!IsAvailable(Hologram) || !Choice || Choice->Revision != Request.Revision) return;
	RestoreNativeSignPose(CastChecked<AFGStandaloneSignHologram>(Hologram), *Choice);
	Choices.Remove(Hologram);
	RefreshSignChoiceGrid(Hologram);
}
