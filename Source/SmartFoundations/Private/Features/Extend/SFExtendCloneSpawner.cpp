// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

// Spawner implementation split from SFExtendCloneTopology.cpp as part of T2.
// Keep this file focused on turning clone topology entries into preview holograms.

#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Extend/SFExtendService.h"
#include "Misc/DateTime.h"

// Satisfactory includes
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildablePipelinePump.h"  // Issue #288: valves/pumps
#include "Buildables/FGBuildablePassthrough.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "FGPowerConnectionComponent.h"  // Issue #288: capture pump's power-pole linkage
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGConveyorChainActor.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "FGPlayerController.h"
#include "Components/SplineComponent.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "FGFactoryColoringTypes.h"
#include "EngineUtils.h"

// Smart includes
#include "SmartFoundations.h"
#include "Constants/SFAssetPaths.h"
#include "Holograms/Logistics/SFConveyorAttachmentChildHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Logistics/SFPassthroughChildHologram.h"
#include "Holograms/Logistics/SFPipeAttachmentChildHologram.h"  // Issue #288: valves/pumps
#include "Holograms/Logistics/SFWallHoleChildHologram.h"
#include "Holograms/Power/SFPowerPoleChildHologram.h"
#include "Holograms/Power/SFWireHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "Subsystem/SFSubsystem.h"
#include "Data/SFHologramDataRegistry.h"

// [#365] Every Extend clone hologram must carry the Blueprint Designer context of the source
// hologram: vanilla copies hologram->buildable at construct, so a clone spawned without it
// builds buildables the designer never tracks (invisible to blueprint capture, left behind
// on designer clear). Called at every clone spawn site, right where MarkAsChild runs.
static void SFPropagateDesignerToClone(AFGHologram* Clone, AFGHologram* ParentHologram)
{
	if (Clone && ParentHologram)
	{
		if (AFGBuildableBlueprintDesigner* Designer = ParentHologram->GetBlueprintDesigner())
		{
			Clone->SetInsideBlueprintDesigner(Designer);
		}
	}
}

// ============================================================================
// FSFCloneTopology - Spawn child holograms
// ============================================================================

namespace SpawnHelpers
{
    // Find recipe by class name
    TSubclassOf<UFGRecipe> FindRecipeByName(UWorld* World, const FString& RecipeClassName)
    {
        if (RecipeClassName.IsEmpty() || !World)
        {
            return nullptr;
        }
        
        // Get recipe manager
        AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
        if (!RecipeManager)
        {
            return nullptr;
        }
        
        // Search available recipes
        TArray<TSubclassOf<UFGRecipe>> AllRecipes;
        RecipeManager->GetAllAvailableRecipes(AllRecipes);
        
        for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
        {
            if (Recipe && Recipe->GetName() == RecipeClassName)
            {
                return Recipe;
            }
        }
        
        return nullptr;
    }
    
    // Find build class by name
    TSubclassOf<AFGBuildable> FindBuildClassByName(const FString& BuildClassName)
    {
        if (BuildClassName.IsEmpty())
        {
            return nullptr;
        }
        
        // Try to find the class by name using FindFirstObject (replaces deprecated ANY_PACKAGE)
        UClass* FoundClass = FindFirstObject<UClass>(*BuildClassName, EFindFirstObjectOptions::ExactClass);
        if (!FoundClass)
        {
            // Try without ExactClass flag
            FoundClass = FindFirstObject<UClass>(*BuildClassName, EFindFirstObjectOptions::None);
        }
        
        if (FoundClass && FoundClass->IsChildOf(AFGBuildable::StaticClass()))
        {
            return TSubclassOf<AFGBuildable>(FoundClass);
        }
        
        return nullptr;
    }
    
    // Convert FSFSplineData to TArray<FSplinePointData>
    TArray<FSplinePointData> ConvertSplineData(const FSFSplineData& SplineData)
    {
        TArray<FSplinePointData> Result;
        for (const FSFSplinePoint& Point : SplineData.Points)
        {
            FSplinePointData PointData;
            PointData.Location = Point.Local.ToFVector();
            PointData.ArriveTangent = Point.ArriveTangent.ToFVector();
            PointData.LeaveTangent = Point.LeaveTangent.ToFVector();
            Result.Add(PointData);
        }
        return Result;
    }
}

int32 FSFCloneTopology::SpawnChildHolograms(
    AFGHologram* ParentHologram,
    USFExtendService* ExtendService,
    TMap<FString, AFGHologram*>& OutSpawnedHolograms) const
{
    using namespace SpawnHelpers;
    
    if (!ParentHologram || !ExtendService)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 JSON SPAWN: Invalid parent hologram or extend service"));
        return 0;
    }
    
    UWorld* World = ParentHologram->GetWorld();
    if (!World)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 JSON SPAWN: No world available"));
        return 0;
    }
    
    int32 SpawnedCount = 0;
    static int32 JsonSpawnCounter = 0;
    const EHologramMaterialState ParentMaterialState = ParentHologram->GetHologramMaterialState();
    
    // Build customization lookup from source actors so clones inherit source colors, not parent's
    TMap<FString, FFactoryCustomizationData> SourceCustomizationMap;
    const FSFExtendTopology& Topology = ExtendService->GetCurrentTopology();
    auto CaptureCustomization = [&](AFGBuildable* Actor)
    {
        if (Actor && IsValid(Actor))
        {
            SourceCustomizationMap.Add(Actor->GetName(), Actor->GetCustomizationData_Implementation());
        }
    };
    for (const FSFConnectionChainNode& Chain : Topology.InputChains)
    {
        if (Chain.Distributor.IsValid()) CaptureCustomization(Chain.Distributor.Get());
        for (const auto& Conv : Chain.Conveyors) { if (Conv.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Conv.Get())); }
    }
    for (const FSFConnectionChainNode& Chain : Topology.OutputChains)
    {
        if (Chain.Distributor.IsValid()) CaptureCustomization(Chain.Distributor.Get());
        for (const auto& Conv : Chain.Conveyors) { if (Conv.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Conv.Get())); }
    }
    for (const FSFPipeConnectionChainNode& Chain : Topology.PipeInputChains)
    {
        if (Chain.Junction.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Chain.Junction.Get()));
        for (const auto& Pipe : Chain.Pipelines) { if (Pipe.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Pipe.Get())); }
    }
    for (const FSFPipeConnectionChainNode& Chain : Topology.PipeOutputChains)
    {
        if (Chain.Junction.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Chain.Junction.Get()));
        for (const auto& Pipe : Chain.Pipelines) { if (Pipe.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Pipe.Get())); }
    }
    for (const FSFPowerChainNode& PoleNode : Topology.PowerPoles)
    {
        if (PoleNode.PowerPole.IsValid()) CaptureCustomization(Cast<AFGBuildable>(PoleNode.PowerPole.Get()));
    }
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🎨 JSON SPAWN: Captured customization data from %d source actors"), SourceCustomizationMap.Num());
    
    // Build lane segment color lookup: distributor name → customization from the first belt/pipe
    // on the "other side" of the distributor (away from factory). Lane segments should inherit
    // colors from existing infrastructure, NOT from the factory building. If no belt/pipe exists
    // on the other side, we leave the entry absent so lane segments get default colors.
    TMap<FString, FFactoryCustomizationData> LaneColorMap;
    for (const FSFConnectionChainNode& Chain : Topology.InputChains)
    {
        if (Chain.Distributor.IsValid() && Chain.Conveyors.Num() > 0)
        {
            if (AFGBuildableConveyorBase* FirstConv = Chain.Conveyors[0].Get())
            {
                LaneColorMap.Add(Chain.Distributor->GetName(), Cast<AFGBuildable>(FirstConv)->GetCustomizationData_Implementation());
            }
        }
    }
    for (const FSFConnectionChainNode& Chain : Topology.OutputChains)
    {
        if (Chain.Distributor.IsValid() && Chain.Conveyors.Num() > 0)
        {
            if (AFGBuildableConveyorBase* FirstConv = Chain.Conveyors[0].Get())
            {
                LaneColorMap.Add(Chain.Distributor->GetName(), Cast<AFGBuildable>(FirstConv)->GetCustomizationData_Implementation());
            }
        }
    }
    for (const FSFPipeConnectionChainNode& Chain : Topology.PipeInputChains)
    {
        if (Chain.Junction.IsValid() && Chain.Pipelines.Num() > 0)
        {
            if (AFGBuildablePipeline* FirstPipe = Chain.Pipelines[0].Get())
            {
                LaneColorMap.Add(Chain.Junction->GetName(), Cast<AFGBuildable>(FirstPipe)->GetCustomizationData_Implementation());
            }
        }
    }
    for (const FSFPipeConnectionChainNode& Chain : Topology.PipeOutputChains)
    {
        if (Chain.Junction.IsValid() && Chain.Pipelines.Num() > 0)
        {
            if (AFGBuildablePipeline* FirstPipe = Chain.Pipelines[0].Get())
            {
                LaneColorMap.Add(Chain.Junction->GetName(), Cast<AFGBuildable>(FirstPipe)->GetCustomizationData_Implementation());
            }
        }
    }
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🎨 JSON SPAWN: Built lane color map from %d distributors with existing infrastructure"), LaneColorMap.Num());
    
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Starting spawn of %d child holograms"), ChildHolograms.Num());
    
    for (const FSFCloneHologram& ChildData : ChildHolograms)
    {
        AFGHologram* SpawnedHologram = nullptr;
        
        // Get transform
        FVector Location = ChildData.Transform.Location.ToFVector();
        FRotator Rotation = ChildData.Transform.Rotation.ToFRotator();
        
        // Find recipe
        TSubclassOf<UFGRecipe> Recipe = FindRecipeByName(World, ChildData.RecipeClass);
        
        // Find build class
        TSubclassOf<AFGBuildable> BuildClass = FindBuildClassByName(ChildData.BuildClass);
        
        if (ChildData.Role == TEXT("distributor"))
        {
            // Spawn distributor child hologram
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 JSON SPAWN: Missing recipe/build class for distributor %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonDistributor_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFConveyorAttachmentChildHologram* DistributorChild = World->SpawnActor<ASFConveyorAttachmentChildHologram>(
                ASFConveyorAttachmentChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (DistributorChild)
            {
                // Match working EXTEND code order exactly
                DistributorChild->SetBuildClass(BuildClass);
                DistributorChild->SetRecipe(Recipe);
                
                DistributorChild->FinishSpawning(FTransform(Rotation, Location));
                
                // Add as child IMMEDIATELY after FinishSpawning (matches working EXTEND code)
                ParentHologram->AddChild(DistributorChild, ChildName);
                
                // NOTE: Do NOT call ProvideFloorHitResult here - it calls SetHologramLocationAndRotation
                // which adjusts the Z position and causes distributors to be raised ~100 units
                
                // Disable validation AFTER AddChild
                USFHologramDataService::DisableValidation(DistributorChild);
                USFHologramDataService::MarkAsChild(DistributorChild, ParentHologram, ESFChildHologramType::ExtendClone);
                SFPropagateDesignerToClone(DistributorChild, ParentHologram);
                
                // Configure visibility
                if (DistributorChild->IsHologramLocked())
                {
                    DistributorChild->LockHologramPosition(false);
                }
                DistributorChild->SetActorHiddenInGame(false);
                DistributorChild->SetActorEnableCollision(false);
                
                // Disable collision on ALL primitive components
                TArray<UPrimitiveComponent*> Primitives;
                DistributorChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
                
                // Disable tick to prevent validation from running
                DistributorChild->SetActorTickEnabled(false);
                DistributorChild->RegisterAllComponents();
                DistributorChild->SetPlacementMaterialState(ParentMaterialState);
                
                DistributorChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                SpawnedHologram = DistributorChild;
                SpawnedCount++;
                
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Spawned distributor %s at %s"), 
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("belt_segment"))
        {
            // Spawn belt child hologram
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Processing belt_segment %s, bHasSplineData=%d, SplinePoints=%d"), 
                *ChildData.HologramId, ChildData.bHasSplineData, ChildData.SplineData.Points.Num());
            
            if (!BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 JSON SPAWN: Missing build class for belt %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonBelt_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFConveyorBeltHologram* BeltChild = World->SpawnActor<ASFConveyorBeltHologram>(
                ASFConveyorBeltHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );

            if (BeltChild)
            {
                BeltChild->SetReplicates(false);
                BeltChild->SetReplicateMovement(false);
                BeltChild->SetBuildClass(BuildClass);

                // CRITICAL: Add tag BEFORE FinishSpawning so CheckValidPlacement can detect it
                BeltChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));

                USFHologramDataService::DisableValidation(BeltChild);
                USFHologramDataService::MarkAsChild(BeltChild, ParentHologram, ESFChildHologramType::ExtendClone);
                SFPropagateDesignerToClone(BeltChild, ParentHologram);


                BeltChild->FinishSpawning(FTransform(Rotation, Location));
                BeltChild->SetActorLocation(Location);
                BeltChild->SetActorRotation(Rotation);
                
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Belt %s spawned, checking spline data..."), *ChildName.ToString());
                
                // Set spline data AFTER FinishSpawning (mSplineComponent now exists)
                if (ChildData.bHasSplineData)
                {
                    TArray<FSplinePointData> SplinePoints = ConvertSplineData(ChildData.SplineData);
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Converted %d spline points for belt %s"), 
                        SplinePoints.Num(), *ChildName.ToString());
                    
                    BeltChild->SetSplineDataAndUpdate(SplinePoints);
                    
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: SetSplineDataAndUpdate completed for belt %s"), 
                        *ChildName.ToString());
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 JSON SPAWN: bHasSplineData=FALSE for belt %s - NO SPLINE DATA!"),
                        *ChildName.ToString());
                }
                
                // Force unlock to ensure visibility
                if (BeltChild->IsHologramLocked())
                {
                    BeltChild->LockHologramPosition(false);
                }
                BeltChild->SetActorHiddenInGame(false);
                BeltChild->SetActorEnableCollision(false);
                BeltChild->SetActorTickEnabled(false);
                BeltChild->RegisterAllComponents();
                BeltChild->SetPlacementMaterialState(ParentMaterialState);
                
                // Add as child for vanilla construction (belt Construct() calls Super::Construct when tagged)
                ParentHologram->AddChild(BeltChild, ChildName);
                
                // CRITICAL: Trigger mesh generation AFTER AddChild and spline data set
                // This creates the visible spline mesh components
                if (ChildData.bHasSplineData)
                {
                    // DEFENSIVE: Re-apply spline data AFTER AddChild in case vanilla code reset it
                    // This mirrors the pipe hologram's backup restoration mechanism
                    TArray<FSplinePointData> SplinePointsAgain = ConvertSplineData(ChildData.SplineData);
                    BeltChild->SetSplineDataAndUpdate(SplinePointsAgain);
                    
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Re-applied spline data after AddChild, triggering mesh generation for belt %s"), *ChildName.ToString());
                    BeltChild->TriggerMeshGeneration();
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Mesh generation complete for belt %s"), *ChildName.ToString());
                }
                
                // Force hologram material (required for spline mesh visibility)
                BeltChild->ForceApplyHologramMaterial();

                SpawnedHologram = BeltChild;
                SpawnedCount++;
                
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Spawned belt %s at %s"), 
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("lift_segment"))
        {
            // Spawn lift child hologram
            if (!BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 JSON SPAWN: Missing build class for lift %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonLift_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFConveyorLiftHologram* LiftChild = World->SpawnActor<ASFConveyorLiftHologram>(
                ASFConveyorLiftHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (LiftChild)
            {
                LiftChild->SetReplicates(false);
                LiftChild->SetReplicateMovement(false);
                LiftChild->SetBuildClass(BuildClass);
                
                // CRITICAL: Add tag BEFORE FinishSpawning so CheckValidPlacement can detect it
                LiftChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                USFHologramDataService::DisableValidation(LiftChild);
                USFHologramDataService::MarkAsChild(LiftChild, ParentHologram, ESFChildHologramType::ExtendClone);
                SFPropagateDesignerToClone(LiftChild, ParentHologram);
                
                LiftChild->FinishSpawning(FTransform(Rotation, Location));
                
                // Apply lift top transform and force mesh rebuild (critical for visibility)
                if (ChildData.bHasLiftData)
                {
                    FTransform TopTransform = ChildData.LiftData.TopTransform.ToFTransform();
                    LiftChild->SetTopTransform(TopTransform);
                    
                    // Force mesh rebuild by simulating a location update
                    FHitResult DummyHit;
                    DummyHit.Location = Location;
                    DummyHit.ImpactPoint = Location;
                    DummyHit.ImpactNormal = FVector::UpVector;
                    LiftChild->SetHologramLocationAndRotation(DummyHit);
                    
                    // SetHologramLocationAndRotation may have reset mTopTransform - restore it
                    LiftChild->SetTopTransform(TopTransform);
                    
                    // CRITICAL: SetHologramLocationAndRotation adjusts Z position - restore exact location
                    LiftChild->SetActorLocation(Location);
                    LiftChild->SetActorRotation(Rotation);
                    
                    // Call UpdateConnectionDirections to update the visual mesh orientations
                    if (UFunction* UpdateFunc = AFGConveyorLiftHologram::StaticClass()->FindFunctionByName(TEXT("UpdateConnectionDirections")))
                    {
                        LiftChild->ProcessEvent(UpdateFunc, nullptr);
                    }
                }
                
                // Store lift data in registry BEFORE AddChild (matches working EXTEND code order)
                if (ChildData.bHasLiftData)
                {
                    FSFHologramData* HoloData = USFHologramDataRegistry::GetData(LiftChild);
                    if (!HoloData)
                    {
                        HoloData = USFHologramDataRegistry::AttachData(LiftChild);
                    }
                    if (HoloData)
                    {
                        HoloData->bHasLiftData = true;
                        HoloData->LiftHeight = ChildData.LiftData.Height;
                        HoloData->bLiftIsReversed = ChildData.LiftData.bIsReversed;
                        HoloData->LiftTopTransform = ChildData.LiftData.TopTransform.ToFTransform();
                        HoloData->LiftBottomTransform = ChildData.LiftData.BottomTransform.ToFTransform();
                        
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Stored lift data for %s (height=%.1f, reversed=%d)"), 
                            *ChildData.HologramId, 
                            ChildData.LiftData.Height, 
                            ChildData.LiftData.bIsReversed ? 1 : 0);
                    }
                    
                    // Issue #260: Passthrough linking is now done in SECOND PASS after all holograms are spawned
                    // (see end of SpawnChildHolograms function)
                }
                
                // Configure visibility and state (matches working EXTEND code order)
                if (LiftChild->IsHologramLocked())
                {
                    LiftChild->LockHologramPosition(false);
                }
                LiftChild->SetActorHiddenInGame(false);
                LiftChild->SetActorTickEnabled(false);
                LiftChild->SetPlacementMaterialState(ParentMaterialState);
                LiftChild->SetActorEnableCollision(false);
                
                LiftChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                // Add as child (matches working EXTEND code order - after visibility, before ForceApplyHologramMaterial)
                ParentHologram->AddChild(LiftChild, ChildName);
                
                // Force hologram material AFTER AddChild (required for lift visibility)
                LiftChild->ForceApplyHologramMaterial();
                
                SpawnedHologram = LiftChild;
                SpawnedCount++;
                
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Spawned lift %s at %s"), 
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("pipe_segment"))
        {
            // Spawn pipe child hologram
            if (!BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 JSON SPAWN: Missing build class for pipe %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonPipe_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFPipelineHologram* PipeChild = World->SpawnActor<ASFPipelineHologram>(
                ASFPipelineHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (PipeChild)
            {
                PipeChild->SetReplicates(false);
                PipeChild->SetReplicateMovement(false);
                PipeChild->SetBuildClass(BuildClass);
                
                // CRITICAL: Add tag BEFORE FinishSpawning so CheckValidPlacement can detect it
                PipeChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                USFHologramDataService::DisableValidation(PipeChild);
                USFHologramDataService::MarkAsChild(PipeChild, ParentHologram, ESFChildHologramType::ExtendClone);
                SFPropagateDesignerToClone(PipeChild, ParentHologram);
                
                PipeChild->FinishSpawning(FTransform(Rotation, Location));
                PipeChild->SetActorLocation(Location);
                PipeChild->SetActorRotation(Rotation);
                
                // Set spline data AFTER FinishSpawning (mSplineComponent now exists)
                if (ChildData.bHasSplineData)
                {
                    TArray<FSplinePointData> SplinePoints = ConvertSplineData(ChildData.SplineData);
                    PipeChild->SetSplineDataAndUpdate(SplinePoints);
                    
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Set %d spline points for pipe %s"), 
                        SplinePoints.Num(), *ChildData.HologramId);
                }
                
                // Force unlock to ensure visibility
                if (PipeChild->IsHologramLocked())
                {
                    PipeChild->LockHologramPosition(false);
                }
                PipeChild->SetActorHiddenInGame(false);
                PipeChild->SetActorEnableCollision(false);
                PipeChild->SetActorTickEnabled(false);
                PipeChild->RegisterAllComponents();
                PipeChild->SetPlacementMaterialState(ParentMaterialState);
                
                // Add as child for vanilla construction (pipe Construct() calls Super::Construct when tagged)
                ParentHologram->AddChild(PipeChild, ChildName);
                
                // CRITICAL: Trigger mesh generation AFTER AddChild and spline data set
                if (ChildData.bHasSplineData)
                {
                    PipeChild->TriggerMeshGeneration();
                }
                
                // Force hologram material (required for spline mesh visibility)
                PipeChild->ForceApplyHologramMaterial();
                
                SpawnedHologram = PipeChild;
                SpawnedCount++;
                
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Spawned pipe %s at %s"), 
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("passthrough"))
        {
            // Issue #260: Spawn pipe floor hole hologram for extend cloning
            // Pipe passthroughs need explicit spawning (unlike lift floor holes which are
            // auto-created by conveyor lift construction)
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 JSON SPAWN: Missing recipe/build class for passthrough %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonPassthrough_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFPassthroughChildHologram* PassChild = World->SpawnActor<ASFPassthroughChildHologram>(
                ASFPassthroughChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (PassChild)
            {
                PassChild->SetBuildClass(BuildClass);
                PassChild->SetRecipe(Recipe);
                
                PassChild->FinishSpawning(FTransform(Rotation, Location));
                
                ParentHologram->AddChild(PassChild, ChildName);
                
                USFHologramDataService::DisableValidation(PassChild);
                USFHologramDataService::MarkAsChild(PassChild, ParentHologram, ESFChildHologramType::ExtendClone);
                SFPropagateDesignerToClone(PassChild, ParentHologram);
                
                if (PassChild->IsHologramLocked())
                {
                    PassChild->LockHologramPosition(false);
                }
                PassChild->SetActorHiddenInGame(false);
                PassChild->SetActorEnableCollision(false);
                
                TArray<UPrimitiveComponent*> Primitives;
                PassChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
                
                PassChild->SetActorTickEnabled(false);
                PassChild->RegisterAllComponents();
                PassChild->SetPlacementMaterialState(ParentMaterialState);
                
                PassChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                // Issue #260: Set foundation thickness captured from source passthrough
                if (ChildData.Thickness > 0.0f)
                {
                    PassChild->SetSnappedThickness(ChildData.Thickness);
                }
                else
                {
                    PassChild->SetSnappedThickness(400.0f); // Fallback default 4m
                }
                
                SpawnedHologram = PassChild;
                SpawnedCount++;
                
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 JSON SPAWN: Spawned passthrough %s at %s"),
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("pipe_attachment"))
        {
            // Issue #288: Clone inline pipe attachments (valves, pumps). Uses
            // ASFPipeAttachmentChildHologram → AFGPipelineAttachmentHologram. The
            // source UserFlowLimit was captured during topology walk and is applied
            // to the built AFGBuildablePipelinePump post-Construct (via reflection
            // on the hologram registry data).
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 JSON SPAWN: Missing recipe/build class for pipe attachment %s (recipe=%s, build=%s)"),
                    *ChildData.HologramId, *ChildData.RecipeClass, *ChildData.BuildClass);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonPipeAttachment_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFPipeAttachmentChildHologram* AttChild = World->SpawnActor<ASFPipeAttachmentChildHologram>(
                ASFPipeAttachmentChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (AttChild)
            {
                AttChild->SetBuildClass(BuildClass);
                AttChild->SetRecipe(Recipe);
                
                AttChild->FinishSpawning(FTransform(Rotation, Location));
                
                ParentHologram->AddChild(AttChild, ChildName);
                
                USFHologramDataService::DisableValidation(AttChild);
                USFHologramDataService::MarkAsChild(AttChild, ParentHologram, ESFChildHologramType::ExtendClone);
                SFPropagateDesignerToClone(AttChild, ParentHologram);
                
                // Stash the clone metadata (UserFlowLimit + JsonCloneId) on the
                // registry so Construct can apply the flow limit after the
                // AFGBuildablePipelinePump actor is spawned.
                if (FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(AttChild))
                {
                    HoloData->JsonCloneId = ChildData.HologramId;
                    HoloData->PipeAttachmentUserFlowLimit = ChildData.UserFlowLimit;
                    HoloData->bIsPipeAttachmentClone = true;
                }
                
                if (AttChild->IsHologramLocked())
                {
                    AttChild->LockHologramPosition(false);
                }
                AttChild->SetActorHiddenInGame(false);
                AttChild->SetActorEnableCollision(false);
                
                TArray<UPrimitiveComponent*> Primitives;
                AttChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
                
                AttChild->SetActorTickEnabled(false);
                AttChild->RegisterAllComponents();
                AttChild->SetPlacementMaterialState(ParentMaterialState);
                
                AttChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                SpawnedHologram = AttChild;
                SpawnedCount++;
                
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 JSON SPAWN: Spawned pipe attachment %s (class=%s) UserFlowLimit=%.3f at %s"),
                    *ChildData.HologramId, *ChildData.BuildClass, ChildData.UserFlowLimit, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("power_pole"))
        {
            // Issue #229: Spawn power pole child hologram for extend
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ JSON SPAWN: Missing recipe/build class for power pole %s (recipe=%s, build=%s)"),
                    *ChildData.HologramId, *ChildData.RecipeClass, *ChildData.BuildClass);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonPowerPole_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFPowerPoleChildHologram* PoleChild = World->SpawnActor<ASFPowerPoleChildHologram>(
                ASFPowerPoleChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (PoleChild)
            {
                PoleChild->SetBuildClass(BuildClass);
                PoleChild->SetRecipe(Recipe);
                
                PoleChild->FinishSpawning(FTransform(Rotation, Location));
                
                // Add as child IMMEDIATELY after FinishSpawning
                ParentHologram->AddChild(PoleChild, ChildName);
                
                // Disable validation AFTER AddChild
                USFHologramDataService::DisableValidation(PoleChild);
                USFHologramDataService::MarkAsChild(PoleChild, ParentHologram, ESFChildHologramType::ExtendClone);
                SFPropagateDesignerToClone(PoleChild, ParentHologram);
                
                // Store JsonCloneId for post-build registration
                FSFHologramData* HoloData = USFHologramDataRegistry::GetData(PoleChild);
                if (!HoloData)
                {
                    HoloData = USFHologramDataRegistry::AttachData(PoleChild);
                }
                if (HoloData)
                {
                    HoloData->JsonCloneId = ChildData.HologramId;
                }
                
                // Configure visibility
                if (PoleChild->IsHologramLocked())
                {
                    PoleChild->LockHologramPosition(false);
                }
                PoleChild->SetActorHiddenInGame(false);
                PoleChild->SetActorEnableCollision(false);
                
                // Disable collision on ALL primitive components
                TArray<UPrimitiveComponent*> Primitives;
                PoleChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
                
                PoleChild->SetActorTickEnabled(false);
                PoleChild->RegisterAllComponents();
                PoleChild->SetPlacementMaterialState(ParentMaterialState);
                
                PoleChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                SpawnedHologram = PoleChild;
                SpawnedCount++;
                
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ JSON SPAWN: Spawned power pole %s at %s"),
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("wire_cost"))
        {
            // Issue #229: Wire hologram for cable cost — uses ASFWireHologram (same as PowerAutoConnect)
            // Construct() returns a real wire actor → no nullptr crash in InternalConstructHologram
            FName ChildName(*FString::Printf(TEXT("JsonWire_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFWireHologram* WireChild = World->SpawnActor<ASFWireHologram>(
                ASFWireHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (WireChild)
            {
                // Set power line build class and recipe (matches PowerLinePreviewHelper pattern)
                UClass* PowerLineClass = LoadObject<UClass>(nullptr, SFAssetPaths::PowerLineBuildClass);
                if (PowerLineClass)
                {
                    WireChild->SetBuildClass(PowerLineClass);
                }
                
                TSubclassOf<UFGRecipe> WireRecipe = LoadClass<UFGRecipe>(nullptr, 
                    TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"));
                if (WireRecipe)
                {
                    WireChild->SetRecipe(WireRecipe);
                }
                
                WireChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                WireChild->FinishSpawning(FTransform(Rotation, Location));
                
                ParentHologram->AddChild(WireChild, ChildName);
                
                USFHologramDataService::DisableValidation(WireChild);
                USFHologramDataService::MarkAsChild(WireChild, ParentHologram, ESFChildHologramType::ExtendClone);
                SFPropagateDesignerToClone(WireChild, ParentHologram);
                
                // Store JsonCloneId so the wire registers on build
                FSFHologramData* HoloData = USFHologramDataRegistry::GetData(WireChild);
                if (!HoloData)
                {
                    HoloData = USFHologramDataRegistry::AttachData(WireChild);
                }
                if (HoloData)
                {
                    HoloData->JsonCloneId = ChildData.HologramId;
                }
                
                // Issue #345: render a VISIBLE preview catenary from the captured world endpoints
                // (Points[0]=start, Points.Last()=end). Falls back to the old cost-only hidden wire if
                // endpoints were not captured. Endpoints also drive GetWireLength() -> GetCost().
                const float WireDistance = ChildData.bHasSplineData ? ChildData.SplineData.Length : 0.0f;
                if (ChildData.bHasSplineData && ChildData.SplineData.Points.Num() >= 2)
                {
                    const FVector StartW = ChildData.SplineData.Points[0].World.ToFVector();
                    const FVector EndW   = ChildData.SplineData.Points.Last().World.ToFVector();
                    WireChild->SetupWirePreviewFromPositions(StartW, EndW);
                }
                else
                {
                    WireChild->SetWireEndpoints(FVector::ZeroVector, FVector(WireDistance, 0, 0));
                    WireChild->SetActorHiddenInGame(true);  // cost-only fallback
                }

                WireChild->SetActorEnableCollision(false);
                WireChild->SetActorTickEnabled(false);
                WireChild->SetPlacementMaterialState(ParentMaterialState);
                
                SpawnedHologram = WireChild;
                SpawnedCount++;
                
                int32 CableCount = FMath::Max(1, FMath::CeilToInt((WireDistance / 100.0f) / 25.0f));
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ JSON SPAWN: Spawned wire %s (distance=%.0fcm, cables=%d)"),
                    *ChildData.HologramId, WireDistance, CableCount);
            }
        }
        else if (ChildData.Role == TEXT("lane_segment"))
        {
            // Spawn lane segment - generated belt/lift/pipe connecting adjacent clones' distributors
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🛤️ LANE SPAWN: %s type=%s at (%.0f,%.0f,%.0f) rot=(%.0f)"),
                *ChildData.HologramId, *ChildData.LaneSegmentType,
                Location.X, Location.Y, Location.Z, Rotation.Yaw);
            if (ChildData.bHasSplineData && ChildData.SplineData.Points.Num() >= 2)
            {
                FVector StartW = ChildData.SplineData.Points[0].World.ToFVector();
                FVector EndW = ChildData.SplineData.Points.Last().World.ToFVector();
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🛤️ LANE SPAWN: %s spline Start(%.0f,%.0f,%.0f)→End(%.0f,%.0f,%.0f) len=%.0f"),
                    *ChildData.HologramId,
                    StartW.X, StartW.Y, StartW.Z, EndW.X, EndW.Y, EndW.Z,
                    ChildData.SplineData.Length);
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonLane_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            // Get belt/pipe tier from auto-connect settings
            USFSubsystem* Subsystem = USFSubsystem::Get(World);
            AFGPlayerController* PC = World ? World->GetFirstPlayerController<AFGPlayerController>() : nullptr;
            
            if (ChildData.LaneSegmentType == TEXT("pipe"))
            {
                // Spawn pipe lane using auto-connect global config setting for manifold lanes
                TSubclassOf<AFGBuildable> PipeBuildClass = nullptr;
                if (Subsystem && PC)
                {
                    // Use GetPipeClassFromConfig which handles Auto mode (0) and validates unlocks
                    const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
                    PipeBuildClass = Subsystem->GetPipeClassFromConfig(Settings.PipeTierMain, Settings.bPipeIndicator, PC);
                }
                if (!PipeBuildClass)
                {
                    // Ultimate fallback: Mk1 is always available
                    PipeBuildClass = FindBuildClassByName(TEXT("Build_Pipeline_C"));
                }
                
                ASFPipelineHologram* PipeLane = World->SpawnActor<ASFPipelineHologram>(
                    ASFPipelineHologram::StaticClass(),
                    Location, Rotation, SpawnParams);
                
                if (PipeLane)
                {
                    PipeLane->SetReplicates(false);
                    PipeLane->SetReplicateMovement(false);
                    PipeLane->SetBuildClass(PipeBuildClass);
                    PipeLane->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                    PipeLane->Tags.AddUnique(FName(TEXT("SF_LaneSegment")));
                    
                    USFHologramDataService::DisableValidation(PipeLane);
                    USFHologramDataService::MarkAsChild(PipeLane, ParentHologram, ESFChildHologramType::ExtendClone);
                    SFPropagateDesignerToClone(PipeLane, ParentHologram);
                    
                    PipeLane->FinishSpawning(FTransform(Rotation, Location));
                    
                    // Use TryUseBuildModeRouting for pipe lanes (matches belt lane pattern)
                    if (ChildData.bHasSplineData && ChildData.SplineData.Points.Num() >= 2)
                    {
                        FVector StartPos = ChildData.SplineData.Points[0].World.ToFVector();
                        FVector EndPos = ChildData.SplineData.Points.Last().World.ToFVector();
                        FVector StartNormal = ChildData.LaneStartNormal.ToFVector();
                        FVector EndNormal = ChildData.LaneEndNormal.ToFVector();
                        
                        PipeLane->RoutePipeLaneWithConfiguredMode(StartPos, StartNormal, EndPos, EndNormal);  // [#383] honor pipe routing mode
                    }
                    
                    PipeLane->SetActorHiddenInGame(false);
                    PipeLane->SetActorEnableCollision(false);
                    PipeLane->SetActorTickEnabled(false);
                    PipeLane->RegisterAllComponents();
                    PipeLane->SetPlacementMaterialState(ParentMaterialState);
                    
                    ParentHologram->AddChild(PipeLane, ChildName);
                    
                    if (ChildData.bHasSplineData)
                    {
                        PipeLane->TriggerMeshGeneration();
                    }
                    PipeLane->ForceApplyHologramMaterial();
                    
                    SpawnedHologram = PipeLane;
                    SpawnedCount++;
                    
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ JSON SPAWN: Spawned pipe lane %s"), *ChildData.HologramId);
                }
            }
            else if (ChildData.LaneSegmentType == TEXT("lift"))
            {
                // Spawn lift lane
                TSubclassOf<AFGBuildable> LiftBuildClass = nullptr;
                if (Subsystem)
                {
                    const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
                    int32 BeltTier = Settings.BeltTierMain;
                    if (BeltTier == 0)
                    {
                        // Auto mode: use highest unlocked belt tier
                        BeltTier = Subsystem->GetHighestUnlockedBeltTier(PC);
                    }
                    // Use belt tier for lift tier (lifts follow belt tiers)
                    LiftBuildClass = Subsystem->GetBeltClassForTier(BeltTier, PC);
                    // Convert belt class to lift class
                    if (LiftBuildClass)
                    {
                        FString LiftClassName = LiftBuildClass->GetName().Replace(TEXT("ConveyorBelt"), TEXT("ConveyorLift"));
                        LiftBuildClass = FindBuildClassByName(LiftClassName);
                    }
                }
                if (!LiftBuildClass)
                {
                    LiftBuildClass = FindBuildClassByName(TEXT("Build_ConveyorLiftMk1_C"));
                }
                
                ASFConveyorLiftHologram* LiftLane = World->SpawnActor<ASFConveyorLiftHologram>(
                    ASFConveyorLiftHologram::StaticClass(),
                    Location, Rotation, SpawnParams);
                
                if (LiftLane)
                {
                    LiftLane->SetReplicates(false);
                    LiftLane->SetReplicateMovement(false);
                    LiftLane->SetBuildClass(LiftBuildClass);
                    LiftLane->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                    LiftLane->Tags.AddUnique(FName(TEXT("SF_LaneSegment")));
                    
                    USFHologramDataService::DisableValidation(LiftLane);
                    USFHologramDataService::MarkAsChild(LiftLane, ParentHologram, ESFChildHologramType::ExtendClone);
                    SFPropagateDesignerToClone(LiftLane, ParentHologram);
                    
                    LiftLane->FinishSpawning(FTransform(Rotation, Location));
                    
                    if (ChildData.bHasLiftData)
                    {
                        FTransform TopTransform = ChildData.LiftData.TopTransform.ToFTransform();
                        LiftLane->SetTopTransform(TopTransform);
                    }
                    
                    LiftLane->SetActorHiddenInGame(false);
                    LiftLane->SetActorEnableCollision(false);
                    LiftLane->SetActorTickEnabled(false);
                    LiftLane->RegisterAllComponents();
                    LiftLane->SetPlacementMaterialState(ParentMaterialState);
                    
                    ParentHologram->AddChild(LiftLane, ChildName);
                    LiftLane->ForceApplyHologramMaterial();
                    
                    SpawnedHologram = LiftLane;
                    SpawnedCount++;
                    
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ JSON SPAWN: Spawned lift lane %s (height=%.1f)"), 
                        *ChildData.HologramId, ChildData.LiftData.Height);
                }
            }
            else
            {
                // Spawn belt lane using auto-connect global config setting for manifold lanes
                TSubclassOf<AFGBuildable> BeltBuildClass = nullptr;
                if (Subsystem && PC)
                {
                    // Use GetBeltClassFromConfig which handles Auto mode (0) and validates unlocks
                    const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
                    BeltBuildClass = Subsystem->GetBeltClassFromConfig(Settings.BeltTierMain, PC);
                }
                if (!BeltBuildClass)
                {
                    // Ultimate fallback: Mk1 is always available
                    BeltBuildClass = FindBuildClassByName(TEXT("Build_ConveyorBeltMk1_C"));
                }
                
                ASFConveyorBeltHologram* BeltLane = World->SpawnActor<ASFConveyorBeltHologram>(
                    ASFConveyorBeltHologram::StaticClass(),
                    Location, Rotation, SpawnParams);
                
                if (BeltLane)
                {
                    BeltLane->SetReplicates(false);
                    BeltLane->SetReplicateMovement(false);
                    BeltLane->SetBuildClass(BeltBuildClass);
                    BeltLane->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                    BeltLane->Tags.AddUnique(FName(TEXT("SF_LaneSegment")));

                    USFHologramDataService::DisableValidation(BeltLane);
                    USFHologramDataService::MarkAsChild(BeltLane, ParentHologram, ESFChildHologramType::ExtendClone);
                    SFPropagateDesignerToClone(BeltLane, ParentHologram);


                    BeltLane->FinishSpawning(FTransform(Rotation, Location));
                    
                    // [#380] Route honoring the configured belt routing mode (Default/Curve/Straight),
                    // same as the stackable/auto-connect belt path - lane belts otherwise came out Default.
                    if (ChildData.bHasSplineData && ChildData.SplineData.Points.Num() >= 2)
                    {
                        FVector StartPos = ChildData.SplineData.Points[0].World.ToFVector();
                        FVector EndPos = ChildData.SplineData.Points.Last().World.ToFVector();
                        FVector StartNormal = ChildData.LaneStartNormal.ToFVector();
                        FVector EndNormal = ChildData.LaneEndNormal.ToFVector();
                        
                        BeltLane->RouteLaneWithConfiguredMode(StartPos, StartNormal, EndPos, EndNormal);  // [#380] honor belt routing mode
                    }
                    
                    BeltLane->SetActorHiddenInGame(false);
                    BeltLane->SetActorEnableCollision(false);
                    BeltLane->SetActorTickEnabled(false);
                    BeltLane->RegisterAllComponents();
                    BeltLane->SetPlacementMaterialState(ParentMaterialState);
                    
                    ParentHologram->AddChild(BeltLane, ChildName);
                    
                    // Re-apply spline routing after AddChild (which repositions the actor)
                    if (ChildData.bHasSplineData && ChildData.SplineData.Points.Num() >= 2)
                    {
                        FVector StartPos = ChildData.SplineData.Points[0].World.ToFVector();
                        FVector EndPos = ChildData.SplineData.Points.Last().World.ToFVector();
                        FVector StartNormal = ChildData.LaneStartNormal.ToFVector();
                        FVector EndNormal = ChildData.LaneEndNormal.ToFVector();
                        
                        BeltLane->RouteLaneWithConfiguredMode(StartPos, StartNormal, EndPos, EndNormal);  // [#380] honor belt routing mode
                        BeltLane->TriggerMeshGeneration();
                    }
                    BeltLane->ForceApplyHologramMaterial();

                    SpawnedHologram = BeltLane;
                    SpawnedCount++;
                    
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ JSON SPAWN: Spawned belt lane %s"), *ChildData.HologramId);
                }
            }
        }
        else if (ChildData.Role == TEXT("wall_hole"))
        {
            // Spawn wall hole child hologram (Build_ConveyorWallHole_C, Build_PipelineSupportWallHole_C,
            // any "*WallHole_C" variants). Pattern mirrors the passthrough handler above.
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🧱 JSON SPAWN: Missing recipe/build class for wall hole %s (recipe=%s, build=%s)"),
                    *ChildData.HologramId, *ChildData.RecipeClass, *ChildData.BuildClass);
                continue;
            }

            FName ChildName(*FString::Printf(TEXT("JsonWallHole_%d"), JsonSpawnCounter++));

            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;

            ASFWallHoleChildHologram* WallChild = World->SpawnActor<ASFWallHoleChildHologram>(
                ASFWallHoleChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );

            if (WallChild)
            {
                WallChild->SetBuildClass(BuildClass);
                WallChild->SetRecipe(Recipe);

                WallChild->FinishSpawning(FTransform(Rotation, Location));

                ParentHologram->AddChild(WallChild, ChildName);

                USFHologramDataService::DisableValidation(WallChild);
                USFHologramDataService::MarkAsChild(WallChild, ParentHologram, ESFChildHologramType::ExtendClone);
                SFPropagateDesignerToClone(WallChild, ParentHologram);

                if (WallChild->IsHologramLocked())
                {
                    WallChild->LockHologramPosition(false);
                }
                WallChild->SetActorHiddenInGame(false);
                WallChild->SetActorEnableCollision(false);

                TArray<UPrimitiveComponent*> Primitives;
                WallChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }

                WallChild->SetActorTickEnabled(false);
                WallChild->RegisterAllComponents();
                WallChild->SetPlacementMaterialState(ParentMaterialState);

                WallChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));

                SpawnedHologram = WallChild;
                SpawnedCount++;

                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🧱 JSON SPAWN: Spawned wall hole %s (class=%s) at %s"),
                    *ChildData.HologramId, *ChildData.BuildClass, *Location.ToString());
            }
            else
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🧱 JSON SPAWN: Failed to spawn ASFWallHoleChildHologram for %s"),
                    *ChildData.HologramId);
            }
        }

        // Store in output map for connection wiring
        if (SpawnedHologram && !ChildData.HologramId.IsEmpty())
        {
            OutSpawnedHolograms.Add(ChildData.HologramId, SpawnedHologram);
            
            // Apply customization (color/swatch) to clone hologram
            // Without this, children inherit the parent hologram's color (e.g., refinery's caterium swatch)
            if (ChildData.bIsLaneSegment)
            {
                // Lane segments: inherit color from existing belt/pipe on the other side of the
                // source distributor. If no infrastructure exists there, use defaults instead of
                // the factory building's color. Look up by the source distributor's actor name.
                if (AFGBuildableHologram* BuildableHolo = Cast<AFGBuildableHologram>(SpawnedHologram))
                {
                    // Extract source distributor name from LaneFromDistributorId or LaneToDistributorId
                    // (whichever refers to the source, not the clone)
                    FString SourceDistName;
                    if (!ChildData.LaneFromDistributorId.IsEmpty() && !ChildData.LaneFromDistributorId.Contains(TEXT("clone_")))
                        SourceDistName = ChildData.LaneFromDistributorId;
                    else if (!ChildData.LaneToDistributorId.IsEmpty() && !ChildData.LaneToDistributorId.Contains(TEXT("clone_")))
                        SourceDistName = ChildData.LaneToDistributorId;
                    
                    if (const FFactoryCustomizationData* LaneCustom = LaneColorMap.Find(SourceDistName))
                    {
                        BuildableHolo->SetCustomizationData(*LaneCustom);
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🎨 LANE COLOR: %s inherits color from belt/pipe near distributor %s"),
                            *ChildData.HologramId, *SourceDistName);
                    }
                    else
                    {
                        // No existing infrastructure to sample — use default customization
                        // (explicitly reset so lane doesn't inherit factory building's color)
                        BuildableHolo->SetCustomizationData(FFactoryCustomizationData());
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🎨 LANE COLOR: %s using defaults (no infrastructure near distributor %s)"),
                            *ChildData.HologramId, *SourceDistName);
                    }
                }
            }
            else if (!ChildData.SourceId.IsEmpty())
            {
                // Non-lane clones: inherit color from their specific source actor
                if (const FFactoryCustomizationData* SourceCustom = SourceCustomizationMap.Find(ChildData.SourceId))
                {
                    if (AFGBuildableHologram* BuildableHolo = Cast<AFGBuildableHologram>(SpawnedHologram))
                    {
                        BuildableHolo->SetCustomizationData(*SourceCustom);
                    }
                }
            }
            
            // Store JsonCloneId and connection targets in hologram data
            FSFHologramData* HoloData = USFHologramDataRegistry::GetData(SpawnedHologram);
            if (!HoloData)
            {
                HoloData = USFHologramDataRegistry::AttachData(SpawnedHologram);
            }
            if (HoloData)
            {
                HoloData->JsonCloneId = ChildData.HologramId;
                
                // ================================================================
                // PRE-TICK CONNECTION TARGETS
                // Store connection targets from CloneConnections so ConfigureComponents
                // can establish connections during construction (like AutoLink)
                // ================================================================
                
                // Conn0 target (ConveyorAny0)
                if (!ChildData.CloneConnections.ConveyorAny0.Target.IsEmpty() && 
                    ChildData.CloneConnections.ConveyorAny0.Target != TEXT("external"))
                {
                    HoloData->Conn0TargetCloneId = ChildData.CloneConnections.ConveyorAny0.Target;
                    HoloData->Conn0TargetConnectorName = FName(*ChildData.CloneConnections.ConveyorAny0.Connector);
                    
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: %s Conn0 target set: %s.%s"),
                        *ChildData.HologramId,
                        *HoloData->Conn0TargetCloneId,
                        *HoloData->Conn0TargetConnectorName.ToString());
                }
                
                // Conn1 target (ConveyorAny1)
                if (!ChildData.CloneConnections.ConveyorAny1.Target.IsEmpty() && 
                    ChildData.CloneConnections.ConveyorAny1.Target != TEXT("external"))
                {
                    HoloData->Conn1TargetCloneId = ChildData.CloneConnections.ConveyorAny1.Target;
                    HoloData->Conn1TargetConnectorName = FName(*ChildData.CloneConnections.ConveyorAny1.Connector);
                    
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: %s Conn1 target set: %s.%s"),
                        *ChildData.HologramId,
                        *HoloData->Conn1TargetCloneId,
                        *HoloData->Conn1TargetConnectorName.ToString());
                }
            }
        }
    }
    
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 JSON SPAWN: Completed - spawned %d/%d holograms"), 
        SpawnedCount, ChildHolograms.Num());
    
    // ==================== SECOND PASS: Link Lifts to Passthroughs (Issue #260) ====================
    // Now that ALL holograms are spawned (including passthroughs), we can link lifts to passthroughs.
    // This must be done BEFORE AddChild/Construct is called on the lifts.
    int32 LiftsLinked = 0;
    for (const FSFCloneHologram& ChildData : ChildHolograms)
    {
        if (ChildData.Role == TEXT("lift_segment") && ChildData.bHasLiftData && 
            ChildData.LiftData.PassthroughCloneIds.Num() > 0)
        {
            // Find the spawned lift hologram
            AFGHologram** LiftHoloPtr = OutSpawnedHolograms.Find(ChildData.HologramId);
            if (!LiftHoloPtr || !*LiftHoloPtr) continue;
            
            AFGConveyorLiftHologram* LiftHolo = Cast<AFGConveyorLiftHologram>(*LiftHoloPtr);
            if (!LiftHolo) continue;
            
            // World search for passthroughs near the lift position
            FVector LiftBottom = LiftHolo->GetActorLocation();
            FVector LiftTop = LiftBottom + FVector(0, 0, ChildData.LiftData.Height);
            const float SnapDistance = 100.0f;  // 1m tolerance
            
            TArray<AFGBuildablePassthrough*> PassthroughActors;
            PassthroughActors.SetNum(2);  // [0]=bottom, [1]=top
            
            for (TActorIterator<AFGBuildablePassthrough> It(World); It; ++It)
            {
                AFGBuildablePassthrough* PT = *It;
                if (!IsValid(PT)) continue;
                
                FVector PTLoc = PT->GetActorLocation();
                float DistBottom = FVector::Dist(PTLoc, LiftBottom);
                float DistTop = FVector::Dist(PTLoc, LiftTop);
                
                if (DistBottom < SnapDistance && !PassthroughActors[0])
                {
                    PassthroughActors[0] = PT;
                }
                if (DistTop < SnapDistance && !PassthroughActors[1])
                {
                    PassthroughActors[1] = PT;
                }
                
                if (PassthroughActors[0] && PassthroughActors[1])
                    break;
            }
            
            // Set mSnappedPassthroughs on the hologram using reflection
            int32 ValidCount = (PassthroughActors[0] ? 1 : 0) + (PassthroughActors[1] ? 1 : 0);
            if (ValidCount > 0)
            {
                FProperty* SnappedProp = AFGConveyorLiftHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedPassthroughs"));
                if (SnappedProp)
                {
                    TArray<AFGBuildablePassthrough*>* HoloPassthroughs = 
                        SnappedProp->ContainerPtrToValuePtr<TArray<AFGBuildablePassthrough*>>(LiftHolo);
                    
                    if (HoloPassthroughs)
                    {
                        *HoloPassthroughs = PassthroughActors;
                        LiftsLinked++;
                        
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 JSON SPAWN PASS 2: Linked %d passthroughs to lift %s (bottom=%s, top=%s)"),
                            ValidCount, *ChildData.HologramId,
                            PassthroughActors[0] ? *PassthroughActors[0]->GetName() : TEXT("none"),
                            PassthroughActors[1] ? *PassthroughActors[1]->GetName() : TEXT("none"));
                    }
                }
            }
        }
    }
    
    if (LiftsLinked > 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 JSON SPAWN PASS 2: Linked %d lifts to passthroughs"), LiftsLinked);
    }
    
    return SpawnedCount;
}

// ============================================================================
// FSFCloneTopology - Wire child hologram connections
// ============================================================================
