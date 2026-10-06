// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Subsystem/SFSubsystemImpl.h"
#include "Core/Net/SFRCO.h"
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "UI/SmartSettingsFormWidget.h"
#include "FGHUD.h"
#include "UI/FGGameUI.h"

// SP/MP DIVERGENCE MAP: local cleanup/input belongs to this player's world subsystem.
// Authority opt-out and stale-spec removal are keyed by the RCO's owning controller.
// Disabling a listen host must not suppress another player's accepted Smart construction.

void USFSubsystem::ToggleSmartForSession()
{
    SetSmartEnabledForSession(!bSmartSessionEnabled);
}

void USFSubsystem::SetSmartEnabledForSession(bool bEnabled)
{
    AFGPlayerController* PC = GetLastController();
    if (!PC || !PC->IsLocalController() || bSmartSessionEnabled == bEnabled)
    {
        return;
    }

    bSmartSessionEnabled = bEnabled;
    bSmartSessionStatePending = true;
    SyncSmartSessionStateToAuthority(PC);

    if (!bEnabled)
    {
        if (SettingsFormWidget.IsValid()) SettingsFormWidget->CancelAndClose();
        if (UpgradePanelWidget.IsValid()) OnUpgradePanelCloseClicked();
        ExitWalkMode();
        AbortRestoreSession(TEXT("Smart disabled for session"));
        if (ActiveHologram.IsValid()) UnregisterActiveHologram(ActiveHologram.Get());
        if (ExtendService) ExtendService->ClearExtendState();
        if (PowerAutoConnectManager.IsValid()) PowerAutoConnectManager->ClearPowerLinePreviews();
        if (InputHandler)
        {
            InputHandler->SetSmartContextActive(false);
            InputHandler->ResetModeState();
        }
        bModifierScaleXActive = bModifierScaleYActive = false;
        bSpacingModeActive = bStepsModeActive = bStaggerModeActive = bRotationModeActive = bRecipeModeActive = false;
        bAutoConnectSettingsModeActive = false;
        bLockedByModifier = bAutoHoldActive = bAutoHoldUserOverrode = false;
        LastSpacingModeReleaseSeconds = LastStepsModeReleaseSeconds = LastStaggerModeReleaseSeconds = LastRotationModeReleaseSeconds = -1000.0;
        ResetSmartDisableFlag();
        ResetCounters();
        BlueprintSpacingDefaultAppliedFor.Empty();

        // Cancel the current native placement as a unit. This prevents queued Smart
        // preview children or a partially completed multi-step operation from being built
        // after opt-out. Selecting a recipe again starts an ordinary vanilla placement.
        if (AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(PC->GetPawn()))
        {
            if (AFGBuildGun* Gun = Player->GetBuildGun()) Gun->GotoNoneState();
        }
    }
    else
    {
        PollForActiveHologram();
    }
    UE_LOG(LogSmartFoundations, Log, TEXT("Smart building assistance %s for the local session."), bEnabled ? TEXT("enabled") : TEXT("disabled"));
    if (AFGHUD* HUD = Cast<AFGHUD>(PC->GetHUD()))
        if (UFGGameUI* GameUI = HUD->GetGameUI())
            GameUI->ShowTextNotification(bEnabled
                ? NSLOCTEXT("SmartSession", "Enabled", "Smart! building assistance enabled for this session.")
                : NSLOCTEXT("SmartSession", "Disabled", "Smart! building assistance disabled. Select a recipe to build normally; use Toggle Smart (Session) to re-enable."));
}

void USFSubsystem::SetSmartEnabledForPlayer(APlayerController* PC, bool bEnabled)
{
    if (!PC || IsSmartEnabledForPlayer(PC) == bEnabled) return;
    if (bEnabled)
    {
        SmartDisabledPlayers.Remove(PC);
    }
    else
    {
        SmartDisabledPlayers.Add(PC);
    }
    // Neither transition may inherit an abandoned staged construction.
    StagedScalingSpecs.Remove(PC);
    StagedExtendCommits.Remove(PC);
    StagedExtendCommitTimes.Remove(PC);
    StagedWalkCommits.Remove(PC);
    StagedWalkCommitTimes.Remove(PC);
}

bool USFSubsystem::IsSmartEnabledForPlayer(APlayerController* PC) const
{
    return PC && !SmartDisabledPlayers.Contains(PC);
}

void USFSubsystem::SyncSmartSessionStateToAuthority(AFGPlayerController* PC)
{
    if (!PC || !PC->IsLocalController()) return;
    if (SessionStateController != PC) bSmartSessionStatePending = true;
    if (!bSmartSessionStatePending) return;
    if (GetWorld()->GetNetMode() != NM_Client)
    {
        SetSmartEnabledForPlayer(PC, bSmartSessionEnabled); // [MP-AUTH]
        bSmartSessionStatePending = false;
        SessionStateController = PC;
    }
    else if (USFRCO* RCO = PC->GetRemoteCallObjectOfClass<USFRCO>())
    {
        RCO->Server_SetSmartSessionEnabled(bSmartSessionEnabled); // [MP-SEAM] ordered before new staged plans
        bSmartSessionStatePending = false;
        SessionStateController = PC;
    }
}
