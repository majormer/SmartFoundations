// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Restore/SFRestoreService.h"
#include "Buildables/FGBuildable.h"
#include "Subsystem/SFSubsystem.h"

// SP/MP DIVERGENCE MAP - explicit source selection
// Local preview only; TryExtendFromBuilding retains the existing client preview and
// authoritative staged construction/payment seams. No UI action constructs or pays.
bool USFExtendService::CanOfferPanel(AFGHologram* Hologram) const
{
    return IsValid(Hologram) && Hologram->GetBuildClass() && DetectionService
        && DetectionService->IsValidExtendTarget(Hologram->GetBuildClass()->GetDefaultObject<AFGBuildable>());
}

bool USFExtendService::CanSelectPanelSource(AFGBuildable* Source, AFGHologram* Hologram) const
{
    return CanOfferPanel(Hologram) && IsValid(Source) && !Source->IsActorBeingDestroyed()
        && DetectionService->IsValidExtendTarget(Source)
        && DetectionService->DoesTypeMatch(Source, Hologram)
        && Subsystem.IsValid() && !Subsystem->IsExtendDisabled();
}

bool USFExtendService::SelectPanelSource(AFGBuildable* Source, AFGHologram* Hologram)
{
    // A miss/incompatible/stale source must never clear the previous draft.
    if (!CanSelectPanelSource(Source, Hologram)) return false;
    if (GetCurrentTarget() != Source) ClearExtendState();
    if (!TryExtendFromBuilding(Source, Hologram)) return false;
    if (USFRestoreService* Restore = Subsystem->GetRestoreService())
        Restore->ClearActiveRestoreSession(TEXT("Explicit Extend source selection"));
    // Explicit intent has the same lifetime as the existing Hold control.
    // Explicit selection can swap the native root while the modal panel is open.
    // Complete the normal identity-preserving registration before the form reads
    // capabilities and captures its draft context; the next poll is too late.
    if (CurrentExtendHologram.IsValid() && Subsystem->GetActiveHologram() != CurrentExtendHologram.Get())
        Subsystem->RegisterActiveHologram(CurrentExtendHologram.Get());
    SetExtendManualHold(true);
    return true;
}
