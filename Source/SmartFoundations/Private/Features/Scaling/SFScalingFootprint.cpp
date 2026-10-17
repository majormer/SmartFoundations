// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFScalingFootprint.h"
#include "Shared/SFPillarGridFrame.h"
#include "Features/Scaling/SFBeamHologramState.h"
#include "Features/Scaling/SFConveyorLiftHologramState.h"
#include "Features/Scaling/SFElevatorHologramState.h"
#include "Features/Scaling/SFLadderHologramState.h"
#include "Features/Scaling/SFHypertubeSupportHologramState.h"
#include "Features/Scaling/SFSnowCannonHologramState.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGPillarHologram.h"
#include "Hologram/FGStandaloneSignHologram.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "FGClearanceData.h"
#include "UObject/UnrealType.h"

namespace
{
    bool UsesFixedNativeHeight(const FName Name)
    {
        static const TSet<FName> Classes = {
            TEXT("Build_Beam_Connector_C"), TEXT("Build_Beam_Connector_Double_C"),
            TEXT("Build_Beam_Support_C"), TEXT("Build_LargeFan_C"),
            TEXT("Build_ConveyorWallHole_C"), TEXT("Build_Valve_C"),
            TEXT("Build_RailroadEndStop_C"), TEXT("Build_PipeHyperStart_C"),
            TEXT("Build_HyperTubeWallHole_C")
        };
        return Classes.Contains(Name);
    }

    bool TryFixedNativeHeight(AFGHologram* Hologram, FVector& OutDimensions)
    {
        if (!IsValid(Hologram) || !Hologram->GetBuildClass()
            || !UsesFixedNativeHeight(Hologram->GetBuildClass()->GetFName())) return false;
        const auto Profile = USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass());
        if (!Profile.bSupportsScaling) return false;
        OutDimensions = Profile.DefaultSize;
        if (!SFBeamHologramState::TryGetDimensions(Hologram, OutDimensions)
            && Hologram->GetBuildClass()->GetFName() == TEXT("Build_LargeFan_C"))
            OutDimensions = SFScalingFootprint::FromLocalDimensions(Profile.DefaultSize, Hologram->GetActorRotation());

        const FFloatProperty* GridProperty = FindFProperty<FFloatProperty>(
            AFGBuildableHologram::StaticClass(), TEXT("mGridSnapSize"));
        const double GridStep = GridProperty ? GridProperty->GetPropertyValue_InContainer(Hologram) : 100.0;
        OutDimensions = SFScalingFootprint::RoundHorizontalToGrid(OutDimensions, GridStep);

        // Horizontal repetition follows the profile's native snap/fallback pitch.
        // Vertical separation fits the selected mount on whole native nudge steps.
        // Placement arrows, wire guides and clearance visualizers are not buildings.
        FBox PhysicalBounds(ForceInit);
        TArray<UStaticMeshComponent*> Meshes;
        Hologram->GetComponents(Meshes);
        for (UStaticMeshComponent* Mesh : Meshes)
        {
            if (!IsValid(Mesh) || !Mesh->IsVisible() || Mesh->bHiddenInGame
                || !Mesh->GetStaticMesh() || !Mesh->GetStaticMesh()->GetName().StartsWith(TEXT("SM_"))) continue;
            PhysicalBounds += Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Mesh->GetComponentTransform());
        }
        if (!PhysicalBounds.IsValid) return true;
        TArray<const FFGClearanceData*> Clearances;
        Hologram->GetClearanceData(Clearances);
        for (const FFGClearanceData* Clearance : Clearances)
            if (Clearance && Clearance->IsValid())
                PhysicalBounds += Clearance->GetTransformedClearanceBox().TransformBy(Hologram->GetActorTransform());
        const double Height = PhysicalBounds.GetSize().Z;
        if (FMath::IsFinite(Height) && Height > 0.0)
            OutDimensions.Z = SFScalingFootprint::RoundHeightToNudge(Height, Hologram->GetNudgeDistance());
        return true;
    }
}

FVector SFScalingFootprint::FromLocalDimensions(const FVector& LocalDimensions, const FRotator& ParentRotation)
{
	// Grid positioning applies yaw separately. Including it here would rotate the footprint twice.
	const FQuat Tilt = FRotator(ParentRotation.Pitch, 0.0, ParentRotation.Roll).Quaternion();
	return Tilt.RotateVector(FVector(LocalDimensions.X, 0.0, 0.0)).GetAbs()
		+ Tilt.RotateVector(FVector(0.0, LocalDimensions.Y, 0.0)).GetAbs()
		+ Tilt.RotateVector(FVector(0.0, 0.0, LocalDimensions.Z)).GetAbs();
}

FVector SFScalingFootprint::RoundHorizontalToGrid(const FVector& Dimensions, double GridStep)
{
    if (!FMath::IsFinite(GridStep) || GridStep <= 0.0 || Dimensions.ContainsNaN()) return Dimensions;
    return FVector(
        FMath::CeilToDouble((Dimensions.X - KINDA_SMALL_NUMBER) / GridStep) * GridStep,
        FMath::CeilToDouble((Dimensions.Y - KINDA_SMALL_NUMBER) / GridStep) * GridStep,
        Dimensions.Z);
}

double SFScalingFootprint::RoundHeightToNudge(double Height, double NudgeStep)
{
    if (!FMath::IsFinite(Height) || Height <= 0.0) return Height;
    const double Step = FMath::IsFinite(NudgeStep) && NudgeStep > 0.0 ? NudgeStep : 100.0;
    return FMath::CeilToDouble((Height - KINDA_SMALL_NUMBER) / Step) * Step;
}

bool SFScalingFootprint::TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions)
{
	if (TryFixedNativeHeight(Hologram, OutDimensions)) return true;
	if (SFConveyorLiftHologramState::TryGetDimensions(Hologram, OutDimensions)) return true;
	if (SFElevatorHologramState::TryGetDimensions(Hologram, OutDimensions)) return true;
    if (SFBeamHologramState::TryGetDimensions(Hologram, OutDimensions)) return true;
	if (SFLadderHologramState::TryGetDimensions(Hologram, OutDimensions)) return true;
	if (SFHypertubeSupportHologramState::TryGetDimensions(Hologram, OutDimensions)) return true;
	if (SFSnowCannonHologramState::TryGetDimensions(Hologram, OutDimensions)) return true;
	if (!IsValid(Hologram) || !Hologram->GetBuildClass() || Hologram->GetActorRotation().ContainsNaN()) return false;
	const FName BuildName = Hologram->GetBuildClass()->GetFName();
	static const FName FanName(TEXT("Build_LargeFan_C"));
	static const FName VentName(TEXT("Build_LargeVent_C"));
	const bool bIsPillar = Hologram->IsA<AFGPillarHologram>();
	const bool bIsSign = Hologram->IsA<AFGStandaloneSignHologram>();
	if (!bIsPillar && !bIsSign && BuildName != FanName && BuildName != VentName) return false;
	const auto Profile = USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass());
	if (!Profile.bSupportsScaling) return false;
	FVector LocalDimensions = Profile.DefaultSize;
	if (bIsPillar)
	{
		FString SizeSource;
		USFBuildableSizeRegistry::GetSizeWithFallback(Hologram->GetBuildClass(), LocalDimensions, SizeSource);
	}
	OutDimensions = SFPillarGridFrame::Applies(Hologram->GetBuildClass())
		? LocalDimensions : FromLocalDimensions(LocalDimensions, Hologram->GetActorRotation());
	return true;
}
