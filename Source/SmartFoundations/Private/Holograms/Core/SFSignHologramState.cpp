// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Holograms/Core/SFSignHologramState.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Buildables/FGBuildableSignBase.h"
#include "Hologram/FGStandaloneSignHologram.h"
#include "Hologram/FGPoleHologram.h"
#include "Holograms/Core/SFSignPoleChildHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "Subsystem/SFSubsystem.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Features/Scaling/SFSignSupportPolicy.h"
#include "Features/Scaling/Holograms/SFSignPreviewHologram.h"
#include "ItemAmount.h"
#include "UObject/UnrealType.h"

namespace
{
const FName SuppressedRootSupport(TEXT("SF_SignGridRootSupportSuppressed"));
const FName InvalidGridSupport(TEXT("SF_SignGridSupportInvalid"));
// SP/MP DIVERGENCE MAP — sign support construction
//  1. Local authority — native preview children build inside the scoped request;
//     freeze its quote until native payment and temporarily disable the excluded parent pole.
//  2. Staged client — authority expansion captures native pole state before parent suppression.
//  3. Legacy client — scope the native fire so the excluded pole's CustomSerialization
//     disabled state reaches authority; restore the preview if native input retains it.
// All paths use the same support-cell policy; native zoop and other features retain ownership.
thread_local const SFSignHologramState::FLocalConstructionScope* LocalConstruction = nullptr;

AFGSignPoleHologram* GetSupport(AFGStandaloneSignHologram* Sign)
{
	FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
		AFGStandaloneSignHologram::StaticClass(), TEXT("mChildSignPoleHologram"));
	return Property ? Cast<AFGSignPoleHologram>(Property->GetObjectPropertyValue_InContainer(Sign)) : nullptr;
}

void CopyProperty(UClass* Owner, FName Name, UObject* Source, UObject* Target)
{
	if (FProperty* Property = Owner->FindPropertyByName(Name))
	{
		Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Target),
			Property->ContainerPtrToValuePtr<void>(Source));
	}
}

AFGSignPoleHologram* EnsureSupport(AFGStandaloneSignHologram* Sign, AFGSignPoleHologram* Template)
{
	if (AFGSignPoleHologram* Existing = GetSupport(Sign); IsValid(Existing)) return Existing;
	UWorld* World = Sign->GetWorld();
	if (!World || !Template->GetRecipe() || !Template->GetBuildClass()) return nullptr;
	FActorSpawnParameters Parameters;
	Parameters.Owner = Sign->GetOwner();
	Parameters.Instigator = Sign->GetConstructionInstigator();
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.bDeferConstruction = true;
	ASFSignPoleChildHologram* Support = World->SpawnActor<ASFSignPoleChildHologram>(
		ASFSignPoleChildHologram::StaticClass(), Sign->GetActorLocation(), Sign->GetActorRotation(), Parameters);
	if (!Support) return nullptr;
	Support->SetChildBuildClass(Template->GetBuildClass());
	Support->SetRecipe(Template->GetRecipe());
	Support->SetConstructionInstigator(Sign->GetConstructionInstigator());
	Support->FinishSpawning(Sign->GetActorTransform());
	Sign->AddChild(Support, Template->GetNameWithinParentHologram());
	if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
		AFGStandaloneSignHologram::StaticClass(), TEXT("mChildSignPoleHologram")))
	{
		Property->SetObjectPropertyValue_InContainer(Sign, Support);
	}
	USFHologramDataService::DisableValidation(Support);
	USFHologramDataService::MarkAsChild(Support, Sign, ESFChildHologramType::CustomData);
	Support->SetActorTickEnabled(false);
	Support->SetActorEnableCollision(false);
	if (Sign->GetBlueprintDesigner()) Support->SetInsideBlueprintDesigner(Sign->GetBlueprintDesigner());
	return Support;
}
}

namespace
{
bool IsSupportCell(AFGSignPoleHologram* NativeSupport, const FIntVector& Cell, const FSFScalingSpec& Spec)
{
	return NativeSupport && !NativeSupport->IsDisabled()
		&& Cell.Z == SFSignSupportPolicy::GetSupportLayer(Spec.Counters.GridCounters.Z,
			Spec.ItemSize.Z + Spec.Counters.SpacingZ, NativeSupport->GetActorUpVector().Z);
}

void MergeCost(TArray<FItemAmount>& Total, const TArray<FItemAmount>& Items, int32 Copies)
{
	for (const FItemAmount& Item : Items)
	{
		FItemAmount* Existing = Total.FindByPredicate([&Item](const FItemAmount& Entry) { return Entry.ItemClass == Item.ItemClass; });
		if (Existing) Existing->Amount += Item.Amount * Copies;
		else Total.Add(FItemAmount(Item.ItemClass, Item.Amount * Copies));
	}
	Total.RemoveAll([](const FItemAmount& Item) { return Item.Amount <= 0; });
}
}

void SFSignHologramState::ApplyGridCell(AFGHologram* Parent, AFGHologram* Child, const FIntVector& Cell, const FSFScalingSpec& Spec)
{
	AFGStandaloneSignHologram* Source = Cast<AFGStandaloneSignHologram>(Parent);
	if (Source)
	{
		if (ASFSignPreviewHologram* Preview = Cast<ASFSignPreviewHologram>(Child))
		{
			AFGSignPoleHologram* NativeSupport = GetSupport(Source);
			const bool bWanted = IsSupportCell(NativeSupport, Cell, Spec);
			FTransform PolePose = FTransform::Identity;
			double Height = 0.0;
			if (IsValid(NativeSupport))
			{
				PolePose = NativeSupport->GetActorTransform().GetRelativeTransform(Source->GetActorTransform())
					* Preview->GetActorTransform();
				if (bWanted)
				{
					Height = SFSignSupportPolicy::GetSupportHeight(NativeSupport->GetPoleHeight(),
						Source->GetActorLocation().Z, Preview->GetActorLocation().Z, NativeSupport->GetActorUpVector().Z);
					FVector Location = PolePose.GetLocation();
					Location.Z = NativeSupport->GetActorLocation().Z;
					PolePose.SetLocation(Location);
				}
			}
			Preview->SyncSupport(NativeSupport, bWanted, PolePose, Height);
			return;
		}
	}
	AFGStandaloneSignHologram* Target = Cast<AFGStandaloneSignHologram>(Child);
	if (!Source || !Target || Source == Target) return;
	Sync(Source, Target);
	AFGSignPoleHologram* NativeSupport = GetSupport(Source);
	AFGSignPoleHologram* CellSupport = GetSupport(Target);
	Target->Tags.Remove(InvalidGridSupport);
	if (!IsValid(NativeSupport)) return;
	if (!IsValid(CellSupport))
	{
		if (!NativeSupport->IsDisabled()) Target->Tags.AddUnique(InvalidGridSupport);
		return;
	}
	const bool bWanted = IsSupportCell(NativeSupport, Cell, Spec);
	bool bValid = true;
	if (bWanted)
	{
		const double Height = SFSignSupportPolicy::GetSupportHeight(NativeSupport->GetPoleHeight(),
			Source->GetActorLocation().Z, Target->GetActorLocation().Z, NativeSupport->GetActorUpVector().Z);
		bValid = FMath::IsFinite(Height) && Height > 0.0;
		if (bValid)
		{
			CellSupport->SetPoleHeight(static_cast<float>(Height));
			bValid = FMath::IsNearlyEqual(static_cast<double>(CellSupport->GetPoleHeight()), Height, 0.1);
			FVector Location = CellSupport->GetActorLocation();
			Location.Z = NativeSupport->GetActorLocation().Z;
			CellSupport->SetActorLocation(Location);
		}
		if (!bValid) Target->Tags.AddUnique(InvalidGridSupport);
	}
	CellSupport->SetDisabled(!bWanted || !bValid);
}

void SFSignHologramState::SyncGridPreview(AFGHologram* Parent)
{
	AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(Parent);
	USFSubsystem* SS = Sign ? USFSubsystem::Get(Sign->GetWorld()) : nullptr;
	AFGSignPoleHologram* NativeSupport = Sign ? GetSupport(Sign) : nullptr;
	if (!SS || !IsValid(NativeSupport)) return;
	FSFScalingSpec Spec;
	Spec.Counters = SS->GetCounterState();
	Spec.ItemSize = USFBuildableSizeRegistry::GetProfile(Sign->GetBuildClass()).DefaultSize;
	for (AFGHologram* Child : Sign->GetHologramChildren())
	{
		FIntVector Cell;
		if (IsValid(Child) && USFGridCoordComponent::TryGetCell(Child, Cell)) ApplyGridCell(Sign, Child, Cell, Spec);
	}
	const bool bSuppressed = !NativeSupport->IsDisabled() && !IsSupportCell(NativeSupport, FIntVector::ZeroValue, Spec);
	if (bSuppressed) NativeSupport->Tags.AddUnique(SuppressedRootSupport);
	else NativeSupport->Tags.Remove(SuppressedRootSupport);
	// The native input path still needs this parent pole. Actor hiding or disabling it
	// prevents the sign from advancing/committing; hide only its mesh until construction.
	NativeSupport->SetActorHiddenInGame(NativeSupport->IsDisabled() || Sign->IsHidden());
	if (UStaticMeshComponent* Mesh = NativeSupport->GetPoleMesh()) Mesh->SetVisibility(!bSuppressed, true);
}

bool SFSignHologramState::AreGridSupportsValid(AFGHologram* Parent)
{
	AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(Parent);
	if (!Sign || Sign->GetZoopInstanceTransforms().Num() > 0) return true;
	for (AFGHologram* Child : Parent->GetHologramChildren())
	{
		if (IsValid(Child) && Child->Tags.Contains(InvalidGridSupport)) return false;
	}
	return true;
}

namespace
{
bool IsOwnedGridConstruction(AFGStandaloneSignHologram* Sign)
{
	if (!Sign || Sign->GetParentHologram()
		|| Sign->GetZoopInstanceTransforms().Num() > 0) return false;
	const APawn* Instigator = Sign->GetConstructionInstigator();
	USFSubsystem* SS = USFSubsystem::Get(Sign->GetWorld());
	if (!Instigator || !Instigator->IsLocallyControlled() || !SS
		|| !SS->IsSmartEnabledForSession() || SS->GetActiveHologram() != Sign
		|| SS->ShouldSuppressNormalGridChildren()) return false;
	for (AFGHologram* Child : Sign->GetHologramChildren())
	{
		FIntVector Cell;
		if (IsValid(Child) && USFGridCoordComponent::TryGetCell(Child, Cell)) return true;
	}
	return false;
}
}

bool SFSignHologramState::IsLocalGridConstruction(AFGHologram* Parent)
{
	AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(Parent);
	return Sign && Sign->GetWorld() && Sign->GetWorld()->GetNetMode() != NM_Client
		&& Sign->HasAuthority() && IsOwnedGridConstruction(Sign);
}

bool SFSignHologramState::ShouldRejectGridPreview(AFGHologram* Parent)
{
	AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(Parent);
	if (!IsOwnedGridConstruction(Sign) || AreGridSupportsValid(Sign)) return false;
	FProperty* StepProperty = FindFProperty<FProperty>(AFGStandaloneSignHologram::StaticClass(), TEXT("mBuildStep"));
	if (!StepProperty) return false;
	ESignHologramBuildStep Step = ESignHologramBuildStep::ESHBS_SnapToActor;
	StepProperty->CopyCompleteValue(&Step, StepProperty->ContainerPtrToValuePtr<void>(Sign));
	return Step == ESignHologramBuildStep::ESHBS_AdjustHeight;
}

bool SFSignHologramState::IsClientGridSerialization(AFGHologram* Parent)
{
	AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(Parent);
	// [MP-CLIENT] Locally spawned preview actors retain ROLE_Authority on a network
	// client. The world mode identifies serialization ownership, not the actor role.
	return Sign && Sign->GetWorld() && Sign->GetWorld()->GetNetMode() == NM_Client
		&& IsOwnedGridConstruction(Sign);
}

SFSignHologramState::FLocalConstructionScope::FLocalConstructionScope(AFGHologram* InParent)
	: Parent(InParent), Previous(LocalConstruction)
{
	Cost = Parent->GetCost(true);
	LocalConstruction = this;
	AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(InParent);
	AFGSignPoleHologram* NativeSupport = Sign ? GetSupport(Sign) : nullptr;
	Support = NativeSupport;
	if (IsValid(NativeSupport))
	{
		bWasDisabled = NativeSupport->IsDisabled();
		FSFScalingSpec Spec;
		Spec.Counters = USFSubsystem::Get(Sign->GetWorld())->GetCounterState();
		Spec.ItemSize = USFBuildableSizeRegistry::GetProfile(Sign->GetBuildClass()).DefaultSize;
		ApplyParentConstruction(Sign, Spec);
	}
}

SFSignHologramState::FLocalConstructionScope::~FLocalConstructionScope()
{
	if (Support.IsValid()) Support->SetDisabled(bWasDisabled);
	LocalConstruction = Previous;
}

bool SFSignHologramState::GetLocalConstructionCost(const AFGHologram* Parent, TArray<FItemAmount>& Cost)
{
	for (const FLocalConstructionScope* Scope = LocalConstruction; Scope; Scope = Scope->Previous)
	{
		if (Scope->Parent == Parent)
		{
			Cost = Scope->Cost;
			return true;
		}
	}
	return false;
}

bool SFSignHologramState::AdjustPreviewCost(const AFGHologram* Parent, TArray<FItemAmount>& Cost)
{
	const AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(Parent);
	if (IsOwnedGridConstruction(const_cast<AFGStandaloneSignHologram*>(Sign))
		&& !AreGridSupportsValid(const_cast<AFGStandaloneSignHologram*>(Sign)))
	{
		// An unavailable native height disables the preview pole, but the grid still
		// requires its support boundary. Retain that cost while placement is invalid.
		USFSubsystem* SS = USFSubsystem::Get(Sign->GetWorld());
		FSFScalingSpec Spec;
		Spec.Counters = SS->GetCounterState();
		return GetGridCost(Parent, Spec, Cost);
	}
	AFGSignPoleHologram* Support = Sign ? GetSupport(const_cast<AFGStandaloneSignHologram*>(Sign)) : nullptr;
	if (!IsValid(Support) || Support->IsDisabled() || !Support->Tags.Contains(SuppressedRootSupport)) return false;
	MergeCost(Cost, Support->GetCost(false), -1);
	return true;
}

bool SFSignHologramState::GetGridCost(const AFGHologram* Parent, const FSFScalingSpec& Spec, TArray<FItemAmount>& Cost)
{
	const AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(Parent);
	if (!Sign) return false;
	Cost.Reset();
	MergeCost(Cost, Sign->GetCost(false), Spec.CellCount());
	AFGSignPoleHologram* Support = GetSupport(const_cast<AFGStandaloneSignHologram*>(Sign));
	if (IsValid(Support) && !Support->IsDisabled())
	{
		const int32 Columns = FMath::Max(1, FMath::Abs(Spec.Counters.GridCounters.X)) * FMath::Max(1, FMath::Abs(Spec.Counters.GridCounters.Y));
		MergeCost(Cost, Support->GetCost(false), Columns);
	}
	return true;
}

void SFSignHologramState::ApplyParentConstruction(AFGHologram* Parent, const FSFScalingSpec& Spec)
{
	AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(Parent);
	AFGSignPoleHologram* Support = Sign ? GetSupport(Sign) : nullptr;
	if (IsValid(Support) && !Support->IsDisabled() && !IsSupportCell(Support, FIntVector::ZeroValue, Spec)) Support->SetDisabled(true);
}

bool SFSignHologramState::IsDirectMount(AFGHologram* Hologram)
{
	AFGStandaloneSignHologram* Sign = Cast<AFGStandaloneSignHologram>(Hologram);
	if (!Sign) return false;
	FProperty* StepProperty = FindFProperty<FProperty>(AFGStandaloneSignHologram::StaticClass(), TEXT("mBuildStep"));
	if (!StepProperty) return false;
	ESignHologramBuildStep Step = ESignHologramBuildStep::ESHBS_AdjustHeight;
	StepProperty->CopyCompleteValue(&Step, StepProperty->ContainerPtrToValuePtr<void>(Sign));
	AFGSignPoleHologram* Support = GetSupport(Sign);
	return Step == ESignHologramBuildStep::ESHBS_SnapToActor
		&& IsValid(Support) && Support->IsDisabled();
}

void SFSignHologramState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
	AFGStandaloneSignHologram* Source = Cast<AFGStandaloneSignHologram>(Parent);
	AFGStandaloneSignHologram* Target = Cast<AFGStandaloneSignHologram>(Child);
	if (!Source || !Target || Source == Target) return;
	CopyProperty(AFGStandaloneSignHologram::StaticClass(), TEXT("mBuildStep"), Source, Target);
	CopyProperty(AFGStandaloneSignHologram::StaticClass(), TEXT("mDefaultSignSupportRecipe"), Source, Target);
	AFGSignPoleHologram* SourceSupport = GetSupport(Source);
	if (!IsValid(SourceSupport))
	{
		if (AFGSignPoleHologram* PreviousSupport = GetSupport(Target); IsValid(PreviousSupport))
		{
			PreviousSupport->SetDisabled(true);
		}
		return;
	}
	AFGSignPoleHologram* TargetSupport = EnsureSupport(Target, SourceSupport);
	if (!IsValid(TargetSupport)) return;
	CopyProperty(AFGPoleHologram::StaticClass(), TEXT("mBuildStep"), SourceSupport, TargetSupport);
	FIntProperty* Variation = FindFProperty<FIntProperty>(AFGPoleHologram::StaticClass(), TEXT("mPoleVariationIndex"));
	if (Variation && Variation->GetPropertyValue_InContainer(SourceSupport) != Variation->GetPropertyValue_InContainer(TargetSupport))
	{
		Variation->SetPropertyValue_InContainer(TargetSupport, Variation->GetPropertyValue_InContainer(SourceSupport));
		if (UFunction* Refresh = TargetSupport->FindFunction(TEXT("OnRep_PoleVariationIndex")))
		{
			TargetSupport->ProcessEvent(Refresh, nullptr);
		}
	}
	UClass* BuildClass = Target->GetBuildClass();
	if (AFGBuildableSignBase* Defaults = BuildClass ? Cast<AFGBuildableSignBase>(BuildClass->GetDefaultObject()) : nullptr)
	{
		TargetSupport->SetPoleScale(Defaults->GetPoleScale());
	}
	const FTransform LocalSupport = SourceSupport->GetActorTransform().GetRelativeTransform(Source->GetActorTransform());
	const FTransform WorldSupport = LocalSupport * Target->GetActorTransform();
	if (!TargetSupport->GetActorTransform().Equals(WorldSupport, 0.01f)) TargetSupport->SetActorTransform(WorldSupport);
	if (UStaticMeshComponent* SourceMesh = SourceSupport->GetPoleMesh())
	{
		if (UStaticMeshComponent* TargetMesh = TargetSupport->GetPoleMesh())
		{
			if (!TargetMesh->GetRelativeTransform().Equals(SourceMesh->GetRelativeTransform(), 0.01f))
			{
				TargetMesh->SetRelativeTransform(SourceMesh->GetRelativeTransform());
			}
		}
	}
	TargetSupport->SetDisabled(SourceSupport->IsDisabled());
}

bool SFSignHologramState::ApplyProxyConstruction(AFGHologram* Parent, AFGHologram* Child,
	bool bSupportWanted, double SupportHeight, const FTransform& SupportPose)
{
	AFGStandaloneSignHologram* Target = Cast<AFGStandaloneSignHologram>(Child);
	if (!Target) return false;
	Sync(Parent, Target);
	AFGSignPoleHologram* Support = GetSupport(Target);
	if (!IsValid(Support)) return !bSupportWanted;
	if (bSupportWanted)
	{
		if (!FMath::IsFinite(SupportHeight) || SupportHeight <= 0.0) return false;
		Support->SetPoleHeight(static_cast<float>(SupportHeight));
		if (!FMath::IsNearlyEqual(static_cast<double>(Support->GetPoleHeight()), SupportHeight, 0.1)) return false;
		Support->SetActorTransform(SupportPose);
		if (Target->GetBlueprintDesigner()) Support->SetInsideBlueprintDesigner(Target->GetBlueprintDesigner());
	}
	Support->SetDisabled(!bSupportWanted);
	return true;
}
