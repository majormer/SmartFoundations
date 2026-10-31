// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Upgrade/SFUpgradeExecutionServiceImpl.h"
#include "Core/Construction/SFReplacementGroupTransfer.h"
#include "Core/Construction/SFInventoryItemSnapshot.h"
#include "UObject/StrongObjectPtr.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "FGBlueprintProxy.h"
#include "Features/Upgrade/SFUpgradeMinerFamily.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Hologram/FGResourceExtractorHologram.h"
#include "FGDismantleInterface.h"
#include "FGConstructDisqualifier.h"
#include "FGInventoryComponent.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

// SP/MP DIVERGENCE MAP — Miner replacement
//  1. Request/settlement — inherited [MP-AUTH] execution; requester owns inventory and gun.
//  2. Materialization — native hologram owns resource binding, geometry and factory state.
//  3. Membership — shared group snapshot is applied on authority; native proxy replicates.

int32 USFUpgradeExecutionService::ProcessMinerReplacement(AFGBuildable* Buildable,
    TSubclassOf<UFGRecipe> TargetRecipe, AFGBuildGun* BuildGun, AFGCharacterPlayer* Player,
    FSFReplacementGroupTransfer& GroupTransfer)
{
    auto Reject = [this](const FString& Message)
    {
        // Keep the first actionable rejection for the requesting player's result.
        if (WorkingResult.ErrorMessage.IsEmpty()) WorkingResult.ErrorMessage = Message;
        return 0;
    };
    if (!IsExecutingFactoryUpgrade(Player))
        return Reject(TEXT("Miner replacement requires its retained native factory mutation owner."));
    auto Uncertain = [&](const FString& Reason)
    {
        WorkingResult.bUncertain = true;
        return Reject(Reason);
    };
    AFGBuildableResourceExtractor* Old = Cast<AFGBuildableResourceExtractor>(Buildable);
    if (!IsValid(Old) || !SFUpgradeMinerFamily::IsMinerClass(Old->GetClass()) || !Old->HasAuthority()
        || !IsValid(Old->GetExtractableResource().GetObject()) || !BuildGun || !Player)
        return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerMissingContext", "The selected Miner is missing its native resource or construction context.").ToString());

    AFGHologram* Preview = AFGHologram::SpawnHologramFromRecipe(TargetRecipe, BuildGun, Old->GetActorLocation(), Player);
    ON_SCOPE_EXIT { if (IsValid(Preview)) Preview->Destroy(); };
    AFGResourceExtractorHologram* Hologram = Cast<AFGResourceExtractorHologram>(Preview);
    if (!Hologram) return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerMissingPreview", "The native Miner replacement preview could not be created.").ToString());
    Hologram->SetInsideBlueprintDesigner(Old->GetBlueprintDesigner());

    // Obtain an ordinary blocking hit on this exact extractor. A synthetic actor hit
    // and TryUpgrade alone leave the standalone preview's native aim/init state unset.
    const FVector Aim = Old->GetActorLocation() + FVector(0.0, 0.0, 300.0);
    const FVector Start = Old->GetActorLocation() + Old->GetActorForwardVector() * 1200.0
        + FVector(0.0, 0.0, 300.0);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SmartMinerReplacement), false, Hologram);
    Query.AddIgnoredActor(Player);
    Query.AddIgnoredActor(BuildGun);
    FHitResult Hit;
    if (!Old->GetWorld()->LineTraceSingleByChannel(Hit, Start, Aim, ECC_Visibility, Query)
        || Hit.GetActor() != Old || !Hologram->IsValidHitResult(Hit))
        return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerInvalidAim", "The selected Miner has no valid native replacement aim.").ToString());

    // Start the same fresh placement pass used by the build gun, then run the full
    // native lifecycle and validation. Never clear disqualifiers after validation,
    // populate resource ownership fields, or force visibility/attachment permission.
    Hologram->ResetConstructDisqualifiers();
    Hologram->UpdateHologramPlacement(Hit);
    if (!Hologram->IsUpgrade() || Hologram->GetUpgradedActor() != Old)
        return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerCannotBind", "The native Miner replacement could not bind to the selected extractor.").ToString());

    // Classify exhausted materials through the shared settlement contract before
    // native cost validation can turn them into an ordinary placement rejection.
    // Returning -1 lets the batch retain and skip every remaining original.
    FSFUpgradeSettlementLedger Ledger;
    const int32 Prepared = PrepareUpgradeSettlement(Old, TargetRecipe, Player, Ledger);
    if (Prepared != 1) return Prepared == -1 ? -1 : 0;
    // Native build steps consume an already validated placement, as on the gun.
    Hologram->ValidatePlacementAndCost(Player->GetInventory());
    int32 Steps = 0;
    while (Hologram->CanTakeNextBuildStep() && !Hologram->DoMultiStepPlacement(false))
    {
        if (++Steps >= 8) return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerPlacementIncomplete", "The native Miner replacement placement could not finish.").ToString());
    }
    Hologram->ValidatePlacementAndCost(Player->GetInventory());
    if (!Hologram->IsUpgrade() || Hologram->GetUpgradedActor() != Old)
        return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerSelectionLost", "The native Miner replacement lost its selected extractor during placement.").ToString());
    if (!Hologram->CanConstruct())
    {
        TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
        Hologram->GetConstructDisqualifiers(Disqualifiers);
        FString Message = NSLOCTEXT("SmartFoundations", "Upgrade_MinerPlacementRejected", "Native placement rejected the Miner replacement.").ToString();
        for (const TSubclassOf<UFGConstructDisqualifier>& Disqualifier : Disqualifiers)
        {
            if (!UFGConstructDisqualifier::GetIsSoftDisqualifier(Disqualifier))
                Message += TEXT(" ") + UFGConstructDisqualifier::GetDisqualifyingText(Disqualifier).ToString();
        }
        return Reject(Message);
    }

    // Native extractor Upgrade migrates connections, but does not migrate factory
    // cargo or standby. Retain the existing factory state independently of callbacks.
    UFGInventoryComponent* Output = Old->GetOutputInventory();
    UFGInventoryComponent* Potential = Old->GetPotentialInventory();
    if (!IsValid(Output) || !IsValid(Potential))
        return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerMissingInventory", "The selected Miner is missing its native factory inventories.").ToString());
    const auto OriginalResource = Old->GetExtractableResource().GetObject();
    auto* OriginalDesigner = Old->GetBlueprintDesigner();
    auto* OriginalGroup = Old->GetBlueprintProxy();
    TArray<FInventoryStack> OutputStacks, PotentialStacks;
    Output->GetInventoryStacks(OutputStacks, true); Potential->GetInventoryStacks(PotentialStacks, true);
    TArray<FSFInventoryStackSnapshot> OutputBefore, PotentialBefore;
    if (!SFInventorySnapshots::Capture(OutputStacks, OutputBefore) || !SFInventorySnapshots::Capture(PotentialStacks, PotentialBefore))
        return Reject(TEXT("The Miner inventory payload cannot be independently captured before replacement."));
    TArray<AActor*> OriginalChildren;
    IFGDismantleInterface::Execute_GetChildDismantleActors(Old, OriginalChildren);
    // Native Miner tiers have no conveyor retirement companions. A foreign child
    // must not bypass the conveyor cargo/cohort retirement protocol through Destroy.
    for (auto* Child : OriginalChildren)
        if (IsValid(Child) && Child->IsA<AFGBuildableConveyorBase>())
            return Reject(TEXT("This Miner has a conveyor dismantle child outside its native replacement contract."));
    const bool bPaused = Old->IsProductionPaused();
    const float CurrentPotential = Old->GetCurrentPotential();
    const float PendingPotential = Old->GetPendingPotential();
    const float CurrentBoost = Old->GetCurrentProductionBoost();
    const float PendingBoost = Old->GetPendingProductionBoost();

    if (!ReserveFactoryCharge(Ledger, Player))
    {
        RollbackFactoryCharge(Ledger, Player);
        return Reject(TEXT("The Miner replacement could not reserve its exact material charge."));
    }
    TArray<AActor*> Children;
    AFGBuildableSubsystem* Builds = AFGBuildableSubsystem::Get(Old->GetWorld());
    const FNetConstructionID ID = Builds ? Builds->GetNewNetConstructionID() : FNetConstructionID();
    AActor* Main = Hologram->Construct(Children, ID);
    auto* New = Cast<AFGBuildableResourceExtractor>(Main);
    if (!AdoptFactoryOutputs(Main, Children) || !IsValid(New) || New->GetClass() != Hologram->GetBuildClass().Get())
        return Uncertain(TEXT("Native Miner construction returned incomplete outputs; actors and charge retained for reconciliation."));
    OldToNewBuildableMap.Add(Old, New); // Retain identity before any inventory/native callback.
    TStrongObjectPtr<AFGBuildableResourceExtractor> KeepReplacement(New);
    if (!IsValid(New->GetOutputInventory()) || !IsValid(New->GetPotentialInventory()))
        return Uncertain(TEXT("Native Miner replacement has no matching factory inventories."));

    // Leave the original intact until native construction has succeeded. Dispatch
    // the interface events so recipe Blueprint overrides participate in migration.
    // The three native Miner tiers share output/shard inventory contracts. Copy
    // before callbacks can clear the source; retiring it below prevents duplication.
    New->GetOutputInventory()->CopyFromOtherComponent(Output);
    if (!IsValid(New) || !IsValid(Old) || !IsValid(Potential))
        return Uncertain(TEXT("Miner inventory callbacks retired a replacement participant."));
    New->GetPotentialInventory()->CopyFromOtherComponent(Potential);
    if (!IsValid(New) || !IsValid(Old)) return Uncertain(TEXT("Miner inventory callbacks retired a replacement participant."));
    IFGDismantleInterface::Execute_PreUpgrade(Old);
    if (!IsValid(New) || !IsValid(Old)) return Uncertain(TEXT("Miner PreUpgrade retired a replacement participant."));
    GroupTransfer.Apply(New);
    IFGDismantleInterface::Execute_Upgrade(Old, New);
    if (!IsValid(New) || !IsValid(Old) || !IsExecutingFactoryUpgrade(Player))
        return Uncertain(TEXT("Miner Upgrade changed the admitted participant or payer context."));
    New->SetCurrentPotential(CurrentPotential);
    New->SetPendingPotential(PendingPotential);
    New->SetCurrentProductionBoost(CurrentBoost);
    New->SetPendingProductionBoost(PendingBoost);
    New->SetIsProductionPaused(bPaused);
    GroupTransfer.Apply(New);
    if (!IsValid(New) || !IsValid(New->GetOutputInventory()) || !IsValid(New->GetPotentialInventory()))
        return Uncertain(TEXT("Miner factory settings retired a replacement inventory."));
    TArray<FInventoryStack> ActualOutput, ActualPotential;
    New->GetOutputInventory()->GetInventoryStacks(ActualOutput, true);
    New->GetPotentialInventory()->GetInventoryStacks(ActualPotential, true);
    if (!SFInventorySnapshots::SameContents(OutputBefore, ActualOutput) || !SFInventorySnapshots::SameContents(PotentialBefore, ActualPotential)
        || New->IsProductionPaused() != bPaused || New->GetCurrentPotential() != CurrentPotential
        || New->GetPendingPotential() != PendingPotential || New->GetCurrentProductionBoost() != CurrentBoost
        || New->GetPendingProductionBoost() != PendingBoost || New->GetExtractableResource().GetObject() != OriginalResource)
        return Uncertain(TEXT("Native Miner replacement did not preserve typed output/shard contents or factory state."));
    TArray<AActor*> OldChildren;
    IFGDismantleInterface::Execute_GetChildDismantleActors(Old, OldChildren);
    for (AActor* Child : OldChildren)
    {
        if (!IsValid(Child) || Child == New || Children.Contains(Child)) continue;
        if (!OriginalChildren.Contains(Child) || Child->IsA<AFGBuildableConveyorBase>())
            return Uncertain(TEXT("Miner native retirement produced an unexpected dismantle child."));
        Child->Destroy();
    }
    Old->Destroy();
    if (IsValid(Old) && !Old->IsActorBeingDestroyed())
        return Uncertain(TEXT("Miner original did not enter native retirement."));
    if (!IsValid(New) || New->IsActorBeingDestroyed() || !IsExecutingFactoryUpgrade(Player)
        || !IsValid(New->GetOutputInventory()) || !IsValid(New->GetPotentialInventory())
        || (OriginalDesigner && !IsValid(OriginalDesigner)) || (OriginalGroup && !IsValid(OriginalGroup))
        || New->GetBlueprintDesigner() != OriginalDesigner || New->GetBlueprintProxy() != OriginalGroup)
        return Uncertain(TEXT("Miner retirement changed replacement, payer or native group ownership."));
    ActualOutput.Reset(); ActualPotential.Reset();
    New->GetOutputInventory()->GetInventoryStacks(ActualOutput, true);
    New->GetPotentialInventory()->GetInventoryStacks(ActualPotential, true);
    if (!SFInventorySnapshots::SameContents(OutputBefore, ActualOutput) || !SFInventorySnapshots::SameContents(PotentialBefore, ActualPotential))
        return Uncertain(TEXT("Miner original retirement changed typed replacement inventory contents."));
    if (OriginalDesigner)
    {
        int32 Count = 0;
        for (auto* Member : OriginalDesigner->GetBuildablesInBlueprintDesigner())
        { if (Member == Old) return Uncertain(TEXT("Miner original retained Designer membership after retirement.")); Count += Member == New; }
        if (Count != 1) return Uncertain(TEXT("Miner replacement Designer membership is not unique."));
    }
    if (OriginalGroup)
    {
        int32 Count = 0;
        for (auto* Member : OriginalGroup->GetBuildables())
        { if (Member == Old) return Uncertain(TEXT("Miner original retained dismantle-group membership after retirement.")); Count += Member == New; }
        if (Count != 1) return Uncertain(TEXT("Miner replacement dismantle-group membership is not unique."));
    }
    GrantUpgradeRefund(Ledger, Player);
    CommitFactoryCharge();
    return 1;
}
