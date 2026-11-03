// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "Core/Helpers/SFBuildEffectHelper.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "FGBuildableSubsystem.h"
#include "FGConveyorChainActor.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Helpers/SFExtendChainHelper.h"
#include "Data/SFHologramDataRegistry.h"
#include "Features/Extend/SFExtendService.h"
#include "Services/SFHudService.h"
#include "Subsystem/SFSubsystem.h"
#include "FGConstructDisqualifier.h"
#include "FGFactoryConnectionComponent.h"
#include "UObject/UnrealType.h"

ASFConveyorLiftHologram::ASFConveyorLiftHologram()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ASFConveyorLiftHologram::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 LIFT HOLOGRAM BeginPlay: %s"), *GetName());
    
    // Log mesh components for debugging
    TArray<UStaticMeshComponent*> MeshComps;
    GetComponents<UStaticMeshComponent>(MeshComps);
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Found %d StaticMeshComponents"), MeshComps.Num());
}

void ASFConveyorLiftHologram::Destroyed()
{
    // Clear lift height from HUD when hologram is destroyed
    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
    {
        if (USFHudService* HudService = SmartSubsystem->GetHudService())
        {
            HudService->ClearLiftHeight();
        }
    }
    
    Super::Destroyed();
}

void ASFConveyorLiftHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    // #497: extend children are positioned by Smart (SetActorLocation from the clone topology) and
    // must be DRIFT-PROOF: vanilla parent propagation calls this on every mChildren entry per frame,
    // and letting Super run repositioned the lift toward the parent hit result — which is why the
    // extend service needed a per-frame position reapply. Same early-return contract as the belt
    // (SFConveyorBeltHologram) and pipe (SFPipelineHologram) SF_ExtendChild guards; the lift was the
    // one conduit class still calling Super unconditionally.
    if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
    {
        return;
    }

    Super::SetHologramLocationAndRotation(hitResult);

    // Get lift height from mTopTransform (relative Z position)
    FTransform TopTransform = GetTopTransform();
    float CurrentHeight = TopTransform.GetTranslation().Z;
    
    // Only update HUD if height changed (avoid spamming updates)
    if (FMath::Abs(CurrentHeight - CachedLiftHeight) > 1.0f)
    {
        CachedLiftHeight = CurrentHeight;
        
        // Calculate world height (top of lift relative to world origin)
        float WorldHeight = CurrentHeight + GetActorLocation().Z;
        
        // Update HUD
        if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
        {
            if (USFHudService* HudService = SmartSubsystem->GetHudService())
            {
                HudService->UpdateLiftHeight(CurrentHeight, WorldHeight);
            }
        }
    }
}

void ASFConveyorLiftHologram::RebuildExtendPreviewMeshes(const FHitResult& hitResult)
{
    // [#497] The SF_ExtendChild early-return above drift-proofs this lift against vanilla's
    // per-frame parent propagation — but it also swallowed the clone spawner's own one-shot
    // "force mesh rebuild" call, leaving extend lift previews as default unraised stubs
    // (mTopTransform set, mesh stack never rebuilt from it; builds were unaffected). This
    // is the deliberate bypass for that one Smart-driven call: vanilla rebuilds the
    // bottom/mid/top mesh stack from the current mTopTransform. The spawner re-asserts
    // mTopTransform and restores the actor transform right after, as it always has.
    Super::SetHologramLocationAndRotation(hitResult);
}

TArray<FItemAmount> ASFConveyorLiftHologram::GetBaseCost() const
{
    // #497: Preview lift children can carry a null mRecipe. Vanilla AFGHologram::GetBaseCost would call
    // UFGRecipe::GetIngredients(nullptr), which logs "FGRecipe::GetIngredients: class was nullpeter" once
    // per child per frame — each line a synchronous disk write (UE log + Sentry breadcrumb) that stutters
    // the game. A null recipe has no base cost, so skip vanilla (which returns empty anyway, just noisily).
    if (!GetRecipe())
    {
        return TArray<FItemAmount>();
    }
    return Super::GetBaseCost();
}

void ASFConveyorLiftHologram::CheckValidPlacement()
{
    // For child lift previews: Skip validation and force valid state
    // Lift children should never block parent placement
    // Check multiple indicators since GetParentHologram() may not be set during spawn
    bool bShouldSkipValidation = GetParentHologram() != nullptr;
    
    // Also check our custom data registry
    if (!bShouldSkipValidation)
    {
        if (FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this))
        {
            // Skip if marked as child OR if validation was explicitly disabled
            bShouldSkipValidation = HoloData->bIsChildHologram || 
                                    HoloData->ParentHologram != nullptr ||
                                    !HoloData->bNeedToCheckPlacement;
        }
    }
    
    // Also check for SF_ExtendChild tag
    if (!bShouldSkipValidation)
    {
        bShouldSkipValidation = Tags.Contains(FName(TEXT("SF_ExtendChild")));
    }
    
    if (bShouldSkipValidation)
    {
        // Child previews don't add their own disqualifiers, but still mirror the Extend
        // preview's authoritative state so insufficient-materials red propagates here.
        // Reading the parent directly can still see HMS_OK before the build gun applies
        // the parent's red this frame; the service flag is frame-order independent.
        // The build gun derives preview red/cyan from construct disqualifiers, not from
        // SetPlacementMaterialState. Carry the parent's "unaffordable" disqualifier when broke.
        const EHologramMaterialState ChildState = USFExtendService::ResolveChildPreviewMaterialState(this);
        ResetConstructDisqualifiers();
        if (ChildState == EHologramMaterialState::HMS_ERROR)
        {
            AddConstructDisqualifier(UFGCDUnaffordable::StaticClass());
        }
        SetPlacementMaterialState(ChildState);
        ForceApplyHologramMaterial();
        return;
    }
    
    // For non-child lifts: Use normal validation
    Super::CheckValidPlacement();
}

void ASFConveyorLiftHologram::SetSnappedConnections(UFGFactoryConnectionComponent* Connection0, UFGFactoryConnectionComponent* Connection1)
{
    const auto* Property = FindFProperty<FObjectPropertyBase>(
        AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedConnectionComponents"));
    if (!Property || Property->ArrayDim != 2)
    {
        UE_LOG(LogSmartHologram, Warning, TEXT("Native lift snapped-connection schema is unavailable; selected peers were not modified."));
        return;
    }
    Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(this, 0), Connection0);
    Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(this, 1), Connection1);
}

AActor* ASFConveyorLiftHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    if (auto* Sub = USFSubsystem::Get(GetWorld()))
        if (auto* Extend = Sub->GetExtendService(); Extend && Extend->IsExecutingOwnedNativeInput())
            if (!Extend->PrepareOwnedLiftAttachments(this)) return nullptr;

    // Check if this is an EXTEND child hologram
    if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Warning, TEXT("🔧 EXTEND LIFT CONSTRUCT: %s entering EXTEND path (JsonCloneId will be set by spawn)"),
            *GetName());
        
        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
        auto* Smart = USFSubsystem::Get(GetWorld());
        auto* ExtendService = Smart ? Smart->GetExtendService() : nullptr;
        if (!ExtendService || !ExtendService->IsExecutingOwnedNativeInput())
        {
            UE_LOG(LogSmartHologram, Warning, TEXT("Extend lift construction rejected without its retained native operation."));
            return nullptr;
        }
        // Native passthrough attachment preparation above remains intact; only
        // factory peers wait for the owned immutable role graph after materialization.
        SetSnappedConnections(nullptr, nullptr);

        // Build the lift via vanilla mechanism
        AActor* BuiltActor = Super::Construct(out_children, constructionID);

        if (BuiltActor)
        {
            // Register hologram → buildable mapping for post-build wiring
            if (!HoloData)
            {
                HoloData = USFHologramDataRegistry::GetData(this);
            }
            if (HoloData)
            {
                // Register with ExtendService for post-build wiring using shared helper
                if (AFGBuildableConveyorBase* ConveyorBase = Cast<AFGBuildableConveyorBase>(BuiltActor))
                {
                    if (!ExtendService && FSFExtendChainHelper::IsExtendChainMember(HoloData))
                    {
                        if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
                        {
                            ExtendService = SmartSubsystem->GetExtendService();
                        }
                    }
                    FSFExtendChainHelper::RegisterBuiltConveyor(HoloData, ConveyorBase, ExtendService);
                }

                // Register with ExtendService for JSON-based post-build wiring
                if (!HoloData->JsonCloneId.IsEmpty())
                {
                    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
                    {
                        if (USFExtendService* LocalExtendService = SmartSubsystem->GetExtendService())
                        {
                            LocalExtendService->RegisterJsonBuiltActor(HoloData->JsonCloneId, BuiltActor);
                        }
                    }
                }
            }

            // Ensure the lift is properly finalized (matches belt child pattern)
            if (AFGBuildable* Buildable = Cast<AFGBuildable>(BuiltActor))
            {
                FSFBuildEffectHelper::Finish(Buildable);
            }

            // Log the mapping for debugging
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND: ✅ Lift hologram %s → Buildable %s (ID: %u, ChainId: %d)"),
                *GetName(), *BuiltActor->GetName(), BuiltActor->GetUniqueID(),
                HoloData ? HoloData->ExtendChainId : -1);

            // Log lift connection info
            if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(BuiltActor))
            {
                UFGFactoryConnectionComponent* Conn0 = Lift->GetConnection0();
                UFGFactoryConnectionComponent* Conn1 = Lift->GetConnection1();
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND:   Conn0=%s @ %s, Conn1=%s @ %s, Height=%.1f"),
                    Conn0 ? *Conn0->GetName() : TEXT("null"),
                    Conn0 ? *Conn0->GetComponentLocation().ToString() : TEXT("N/A"),
                    Conn1 ? *Conn1->GetName() : TEXT("null"),
                    Conn1 ? *Conn1->GetComponentLocation().ToString() : TEXT("N/A"),
                    Lift->GetHeight());
            }
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Warning, TEXT("🔧 EXTEND: ❌ Lift Construct returned nullptr!"));
        }

        return BuiltActor;
    }
    
    // For non-EXTEND lifts: Build normally
    return Super::Construct(out_children, constructionID);
}

bool ASFConveyorLiftHologram::ApplyCapturedNativeState(const FTransform& Top, bool bFlowUpwards)
{
    auto* TopProperty = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform"));
    auto* Reverse = FindFProperty<FBoolProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mIsReversed"));
    auto* Arrow = FindFProperty<FEnumProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mArrowDirection"));
    auto* Step = FindFProperty<FIntProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mActivePointIdx"));
    auto* Connections = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), TEXT("mConnectionComponents"));
    if (Top.ContainsNaN() || FMath::IsNearlyZero(Top.GetTranslation().Z)
        || bFlowUpwards != (Top.GetTranslation().Z > 0)
        || !TopProperty || TopProperty->Struct != TBaseStructure<FTransform>::Get()
        || !Reverse || !Arrow || !Step || !Connections || Connections->ArrayDim != 2) return false;
    auto* Origin = Cast<UFGFactoryConnectionComponent>(Connections->GetObjectPropertyValue(Connections->ContainerPtrToValuePtr<void>(this, 0)));
    auto* Remote = Cast<UFGFactoryConnectionComponent>(Connections->GetObjectPropertyValue(Connections->ContainerPtrToValuePtr<void>(this, 1)));
    if (!IsValid(Origin) || !IsValid(Remote)) return false;
    // THESIS lift flow / IT-L1, IT-L2, IT-L5: GetIsReversed on a built
    // lift means IsFlowUpwards. Its signed top already describes canonical
    // input->output geometry. Reversing that preview again swaps its ends.
    const auto* BuildableCDO = GetBuildClass() ? Cast<AFGBuildable>(GetBuildClass()->GetDefaultObject()) : nullptr;
    const auto* NativeDefaults = BuildableCDO && BuildableCDO->mHologramClass
        ? Cast<AFGConveyorLiftHologram>(BuildableCDO->mHologramClass->GetDefaultObject()) : nullptr;
    auto* ReverseMode = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), TEXT("mBuildModeReverse"));
    auto* ReverseClass = NativeDefaults && ReverseMode
        ? Cast<UClass>(ReverseMode->GetObjectPropertyValue_InContainer(NativeDefaults)) : nullptr;
    if (!NativeDefaults || !ReverseClass) return false;
    TSubclassOf<UFGHologramBuildModeDescriptor> NormalMode = NativeDefaults->GetDefaultBuildGunMode();
    if (!NormalMode || NormalMode.Get() == ReverseClass)
    {
        TArray<TSubclassOf<UFGBuildGunModeDescriptor>> Modes;
        NativeDefaults->GetSupportedBuildModes(Modes);
        for (const auto& Mode : Modes)
            if (Mode && Mode.Get() != ReverseClass && Mode->IsChildOf(UFGHologramBuildModeDescriptor::StaticClass()))
            { NormalMode = Mode.Get(); break; }
    }
    if (!NormalMode || NormalMode.Get() == ReverseClass) return false;
    // Like belt route modes, these are asset defaults absent on synthetic C++
    // previews. Install real native descriptors before native mode callbacks.
    SetSnappedConnections(nullptr, nullptr); // Captured role graph owns later transport peers.
    ReverseMode->SetObjectPropertyValue_InContainer(this, ReverseClass);
    SetBuildModeOverride(NormalMode);
    if (!IsCurrentBuildMode(NormalMode) || IsCurrentBuildMode(ReverseClass)) return false;
    *TopProperty->ContainerPtrToValuePtr<FTransform>(this) = Top;
    Reverse->SetPropertyValue_InContainer(this, false);
    Arrow->GetUnderlyingProperty()->SetIntPropertyValue(Arrow->ContainerPtrToValuePtr<void>(this),
        static_cast<int64>(EFactoryConnectionDirection::FCD_OUTPUT));
    Step->SetPropertyValue_InContainer(this, 1);
    Origin->SetRelativeTransform(FTransform::Identity);
    Remote->SetRelativeTransform(Top);
    UpdateConnectionDirections();
    return Origin->GetDirection() == EFactoryConnectionDirection::FCD_INPUT
        && Remote->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT
        && TopProperty->ContainerPtrToValuePtr<FTransform>(this)->Equals(Top, .001f);
}

void ASFConveyorLiftHologram::SetTopTransform(const FTransform& InTopTransform)
{
    // Access mTopTransform via reflection (it's protected in base class)
    if (FStructProperty* TopTransformProp = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform")))
    {
        FTransform* TopTransformPtr = TopTransformProp->ContainerPtrToValuePtr<FTransform>(this);
        if (TopTransformPtr)
        {
            *TopTransformPtr = InTopTransform;
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 LIFT: Set mTopTransform to %s"), 
                *InTopTransform.GetLocation().ToString());
        }
    }
}

FTransform ASFConveyorLiftHologram::GetTopTransform() const
{
    // Access mTopTransform via reflection
    if (FStructProperty* TopTransformProp = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform")))
    {
        const FTransform* TopTransformPtr = TopTransformProp->ContainerPtrToValuePtr<FTransform>(this);
        if (TopTransformPtr)
        {
            return *TopTransformPtr;
        }
    }
    return FTransform::Identity;
}

void ASFConveyorLiftHologram::ConfigureComponents(AFGBuildable* inBuildable) const
{
    // Native placement owns lift geometry, passthrough faces and BeginPlay.
    // Additional Extend/Restore edges belong to the retained operation, never
    // source-name or partially built-role lookups in this callback.
    Super::ConfigureComponents(inBuildable);
}

bool ASFConveyorLiftHologram::SetupUpgradeTarget(AFGBuildableConveyorLift* InUpgradeTarget)
{
    if (!InUpgradeTarget)
    {
        return false;
    }
    
    // Create a fake hit result pointing at the target lift
    FHitResult HitResult;
    HitResult.bBlockingHit = true;
    HitResult.Location = InUpgradeTarget->GetActorLocation();
    HitResult.ImpactPoint = HitResult.Location;
    HitResult.ImpactNormal = FVector::UpVector;
    HitResult.Normal = FVector::UpVector;
    HitResult.HitObjectHandle = FActorInstanceHandle(InUpgradeTarget);
    
    // Call TryUpgrade which will set mUpgradedConveyorLift internally
    return TryUpgrade(HitResult);
}

void ASFConveyorLiftHologram::ForceApplyHologramMaterial()
{
    // Get the current material state
    EHologramMaterialState CurrentState = GetHologramMaterialState();

    // #497 set-once: re-applying an unchanged material to every mesh slot + MarkRenderStateDirty per
    // frame forced continuous render-proxy rebuilds. Lift meshes are static (not regenerated like
    // belt/pipe spline meshes), so once a state is applied it holds until the state changes.
    if (bLiftMaterialStateApplied && CurrentState == LastAppliedLiftMaterialState)
    {
        return;
    }

    // Apply to all static mesh components
    TArray<UStaticMeshComponent*> MeshComps;
    GetComponents<UStaticMeshComponent>(MeshComps);
    
    UMaterialInterface* Material = nullptr;
    switch (CurrentState)
    {
        case EHologramMaterialState::HMS_OK:
        case EHologramMaterialState::HMS_WARNING:
            Material = mValidPlacementMaterial;
            break;
        case EHologramMaterialState::HMS_ERROR:
            Material = mInvalidPlacementMaterial;
            break;
    }
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🎨 LIFT ForceApplyHologramMaterial: State=%d, MeshComponents=%d"), 
        (int32)CurrentState, MeshComps.Num());
    
    if (Material)
    {
        for (UStaticMeshComponent* MeshComp : MeshComps)
        {
            if (MeshComp)
            {
                const int32 SlotCount = MeshComp->GetNumMaterials();
                for (int32 Slot = 0; Slot < SlotCount; ++Slot)
                {
                    MeshComp->SetMaterial(Slot, Material);
                }
                MeshComp->MarkRenderStateDirty();
            }
        }

        LastAppliedLiftMaterialState = CurrentState;
        bLiftMaterialStateApplied = true;
    }
}

void ASFConveyorLiftHologram::SetHologramNudgeLocation()
{
	// [#497] Extend children: block vanilla's locked-parent nudge cascade, which bypasses the
	// SF_ExtendChild SetHologramLocationAndRotation guard via plain SetActorLocation and dragged
	// every extend child to world origin each tick (origin-trap stack: FGBuildGunBuild.cpp:320 ->
	// FGHologram.cpp:440 -> :2120). Non-extend instances keep vanilla behavior.
	if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
	{
		return;
	}
	Super::SetHologramNudgeLocation();
}
