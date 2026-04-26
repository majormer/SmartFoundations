#include "Subsystem/SFInputHandler.h"
#include "SFSubsystem.h"
#include "SmartFoundations.h"
#include "FGPlayerController.h"
#include "Input/FGEnhancedInputComponent.h"
#include "Input/SFInputRegistry.h"
#include "Input/FGInputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Hologram/FGHologram.h"

FSFInputHandler::FSFInputHandler()
{
}

FSFInputHandler::~FSFInputHandler()
{
}

void FSFInputHandler::Initialize(USFSubsystem* InOwnerSubsystem)
{
	OwnerSubsystem = InOwnerSubsystem;
	UE_LOG(LogSmartFoundations, Log, TEXT("InputHandler: Initialized"));
}

void FSFInputHandler::Shutdown()
{
	// TODO: Unbind all input actions
	OwnerSubsystem.Reset();
	LastController.Reset();
	bInputSetupCompleted = false;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("InputHandler: Shutdown complete"));
}

void FSFInputHandler::SetupPlayerInput(AFGPlayerController* PlayerController)
{
	if (!PlayerController || !IsValid(PlayerController))
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("InputHandler::SetupPlayerInput: Invalid player controller"));
		return;
	}

	USFSubsystem* Subsystem = OwnerSubsystem.Get();
	if (!Subsystem)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("InputHandler::SetupPlayerInput: No owner subsystem"));
		return;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("Setting up Smart! Enhanced Input system for player controller"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 Player controller: %s"), *PlayerController->GetName());

	// Ensure input is enabled on the controller (some contexts require this for non-actor receivers)
	PlayerController->EnableInput(PlayerController);

	// Remember controller and set a short delay to ensure post-initialization state
	LastController = PlayerController;
	if (UWorld* WorldForDelay = Subsystem->GetWorld())
	{
		// Schedule deferred rebind via subsystem timer
		// (Timer management stays in subsystem for UObject lifecycle reasons)
		FTimerDelegate RebindDelegate = FTimerDelegate::CreateRaw(this, &FSFInputHandler::RebindAfterDelay);
		WorldForDelay->GetTimerManager().SetTimer(DeferredRebindTimer, RebindDelegate, 0.15f, false);
	}

	// Get the enhanced input component
	if (UFGEnhancedInputComponent* EnhancedInputComp = Cast<UFGEnhancedInputComponent>(PlayerController->InputComponent))
	{
		// Bind our input actions to subsystem methods using the modern SML approach
		USFInputRegistry::BindInputActionsToSubsystem(Subsystem, EnhancedInputComp);

		// Load and apply our input mapping context
		// BUG FIX (Issue #148): Cache is cleared during world cleanup, ensuring fresh load for new worlds
		if (UFGInputMappingContext* SmartContext = USFInputRegistry::GetSmartInputMappingContext())
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("✅ Smart! Mapping Context loaded: %s"), *SmartContext->GetName());
			
			// Get the Enhanced Input Subsystem to add our mapping context
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = PlayerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				// Add with HIGH priority (100) to ensure we consume input before base game
				// This is critical for input consumption to work (prevents wheel from rotating hologram)
				InputSubsystem->AddMappingContext(SmartContext, 100);
				UE_LOG(LogSmartFoundations, Log, TEXT("Smart! Input Mapping Context added to Enhanced Input Subsystem (Priority: 100)"));
			}
			else
			{
				UE_LOG(LogSmartFoundations, Error, TEXT("Failed to get Enhanced Input Local Player Subsystem"));
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, Error, TEXT("❌ Smart! Input Mapping Context not loaded - Blueprint assets required"));
			UE_LOG(LogSmartFoundations, Error, TEXT("📖 Create assets in Unreal Editor per docs/Input/SMART_INPUT_SYSTEM.md"));
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Player controller does not have UFGEnhancedInputComponent"));
	}
	
	bInputSetupCompleted = true;
}

void FSFInputHandler::CheckForPlayerController()
{
	// Don't run if input is already set up
	if (bInputSetupCompleted)
	{
		return;
	}

	USFSubsystem* Subsystem = OwnerSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}

	// Look for the local player controller
	if (UWorld* World = Subsystem->GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AFGPlayerController* FGController = Cast<AFGPlayerController>(PC))
			{
				UE_LOG(LogSmartFoundations, Log, TEXT("Player controller found! Setting up Smart! input system..."));
				SetupPlayerInput(FGController);
			}
		}
	}
}

void FSFInputHandler::RebindAfterDelay()
{
	AFGPlayerController* PC = LastController.IsValid() ? LastController.Get() : nullptr;
	if (!PC)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("InputHandler: Deferred rebind: No player controller available"));
		return;
	}

	USFSubsystem* Subsystem = OwnerSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}

	if (UFGEnhancedInputComponent* EnhancedInputComp = Cast<UFGEnhancedInputComponent>(PC->InputComponent))
	{
		// Bind a base-game action to validate the input pipeline end-to-end
		const TCHAR* PrimaryFirePath = TEXT("/Game/FactoryGame/Inputs/Player/Actions/IA_PrimaryFire.IA_PrimaryFire");
		const FSoftObjectPath P(PrimaryFirePath);
		UObject* Obj = P.TryLoad();
		if (!Obj) Obj = LoadObject<UInputAction>(nullptr, PrimaryFirePath);
		if (UInputAction* IA = Cast<UInputAction>(Obj))
		{
			EnhancedInputComp->BindAction(IA, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnDebugPrimaryFire);
			EnhancedInputComp->BindAction(IA, ETriggerEvent::Triggered, Subsystem, &USFSubsystem::OnDebugPrimaryFire);
			EnhancedInputComp->BindAction(IA, ETriggerEvent::Completed, Subsystem, &USFSubsystem::OnDebugPrimaryFire);
			UE_LOG(LogSmartFoundations, Log, TEXT("Deferred rebind: Bound base action PrimaryFire -> %s"), *IA->GetPathName());
		}
		else
		{
			UE_LOG(LogSmartFoundations, Error, TEXT("Deferred rebind: Failed to load base action IA_PrimaryFire"));
		}
	}

	// Initialize the HUD with current counters (1x1x1 -> hidden)
	Subsystem->UpdateCounterDisplay();
}

bool FSFInputHandler::IsAnyModalFeatureActive() const
{
	return bModifierScaleXActive || bModifierScaleYActive ||
	       bSpacingModeActive || bStepsModeActive || bStaggerModeActive || bRotationModeActive;
}

// ========================================
// Native Nudge Compatibility (PRD Requirement - Task #61.6)
// ========================================

bool FSFInputHandler::IsNativeVerticalNudgeActive(AFGHologram* Hologram) const
{
	if (!Hologram || !IsValid(Hologram))
	{
		return false;
	}

	// Get native nudge offset from Satisfactory's hologram system
	// PageUp/PageDown keys modify the Z component
	const FVector NudgeOffset = Hologram->GetHologramNudgeOffset();
	const float VerticalNudge = NudgeOffset.Z;

	// Threshold: 0.1f cm tolerance for floating point comparison
	// Pattern from existing implementation at SFSubsystem.cpp lines 2043-2061
	return FMath::Abs(VerticalNudge) > 0.1f;
}

bool FSFInputHandler::IsNativeHorizontalNudgeActive(AFGHologram* Hologram) const
{
	if (!Hologram || !IsValid(Hologram))
	{
		return false;
	}

	// Get native nudge offset from Satisfactory's hologram system
	// Arrow keys modify the X and Y components
	const FVector NudgeOffset = Hologram->GetHologramNudgeOffset();
	const FVector2D HorizontalNudge(NudgeOffset.X, NudgeOffset.Y);

	// Threshold: 0.1f cm tolerance for floating point comparison
	return HorizontalNudge.Size() > 0.1f;
}

FVector FSFInputHandler::GetNativeNudgeOffset(AFGHologram* Hologram) const
{
	if (!Hologram || !IsValid(Hologram))
	{
		return FVector::ZeroVector;
	}

	// Get native nudge offset vector from Satisfactory's hologram system
	// This is the offset applied by Arrow keys (X,Y) and PageUp/PageDown (Z)
	// Uses AFGHologram::GetHologramNudgeOffset() API
	return Hologram->GetHologramNudgeOffset();
}

void FSFInputHandler::DisableVanillaBuildGunContext()
{
	USFSubsystem* Subsystem = OwnerSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}

	AFGPlayerController* PlayerController = Subsystem->GetWorld()->GetFirstPlayerController<AFGPlayerController>();
	if (!PlayerController)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = PlayerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	// Find and remove vanilla Build Gun context (the parent context that handles rotation)
	// Try standard path
	FSoftObjectPath VanillaBuildGunPath(TEXT("/Game/FactoryGame/Inputs/Equipment/Buildgun/Build/MC_BuildGunBuild.MC_BuildGunBuild"));
	UFGInputMappingContext* VanillaContext = Cast<UFGInputMappingContext>(VanillaBuildGunPath.TryLoad());
	
	if (!VanillaContext)
	{
		// Try alternative path (just in case)
		VanillaBuildGunPath = FSoftObjectPath(TEXT("/Game/FactoryGame/Inputs/Equipment/Buildgun/MC_BuildGun.MC_BuildGun"));
		VanillaContext = Cast<UFGInputMappingContext>(VanillaBuildGunPath.TryLoad());
	}

	if (VanillaContext)
	{
		// Issue #272: Query current priority BEFORE removing so we can restore at the same priority
		int32 FoundPriority = 0;
		if (InputSubsystem->HasMappingContext(VanillaContext, FoundPriority))
		{
			CachedVanillaContextPriority = FoundPriority;
			bVanillaContextRemoved = true;
			InputSubsystem->RemoveMappingContext(VanillaContext);
			UE_LOG(LogSmartFoundations, Log, TEXT("🚫 Disabled vanilla Build Gun context (priority=%d, prevents rotation while Smart! active) - %s"), FoundPriority, *VanillaContext->GetName());
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ DisableVanillaBuildGunContext: Context found but not active: %s"), *VanillaContext->GetName());
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ DisableVanillaBuildGunContext: Failed to load vanilla context! Paths checked: MC_BuildGunBuild, MC_BuildGun"));
	}
}

void FSFInputHandler::EnableVanillaBuildGunContext()
{
	USFSubsystem* Subsystem = OwnerSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}

	AFGPlayerController* PlayerController = Subsystem->GetWorld()->GetFirstPlayerController<AFGPlayerController>();
	if (!PlayerController)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = PlayerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	// Re-add vanilla Build Gun context at its normal priority
	FSoftObjectPath VanillaBuildGunPath(TEXT("/Game/FactoryGame/Inputs/Equipment/Buildgun/Build/MC_BuildGunBuild.MC_BuildGunBuild"));
	UFGInputMappingContext* VanillaContext = Cast<UFGInputMappingContext>(VanillaBuildGunPath.TryLoad());
	
	if (!VanillaContext)
	{
		VanillaBuildGunPath = FSoftObjectPath(TEXT("/Game/FactoryGame/Inputs/Equipment/Buildgun/MC_BuildGun.MC_BuildGun"));
		VanillaContext = Cast<UFGInputMappingContext>(VanillaBuildGunPath.TryLoad());
	}

	if (VanillaContext)
	{
		if (!InputSubsystem->HasMappingContext(VanillaContext))
		{
			// Issue #272: Restore at the SAME priority we found it at before removal
			// Re-adding at a different priority changes the priority stack, which can
			// cause user-rebound keys (e.g., RMB as Hold) to be overridden by build menu actions
			const int32 RestorePriority = (bVanillaContextRemoved && CachedVanillaContextPriority >= 0) ? CachedVanillaContextPriority : 0;
			InputSubsystem->AddMappingContext(VanillaContext, RestorePriority);
			bVanillaContextRemoved = false;
			UE_LOG(LogSmartFoundations, Log, TEXT("✅ Re-enabled vanilla Build Gun context (priority=%d, rotation restored) - %s"), RestorePriority, *VanillaContext->GetName());
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ EnableVanillaBuildGunContext: Failed to load vanilla context"));
	}
}

// ========================================
// Enhanced Input Action Handlers
// ========================================

void FSFInputHandler::OnScaleXChanged(const FInputActionValue& Value)
{
	// Input processing handled in subsystem for complex modifier logic
	// This method exists for future extraction of input validation logic
	// Currently: No-op, subsystem handles all logic
}

void FSFInputHandler::OnScaleYChanged(const FInputActionValue& Value)
{
	// Input processing handled in subsystem for complex modifier logic
	// This method exists for future extraction of input validation logic
	// Currently: No-op, subsystem handles all logic
}

void FSFInputHandler::OnScaleZChanged(const FInputActionValue& Value)
{
	// Input processing handled in subsystem for complex modifier logic
	// This method exists for future extraction of input validation logic
	// Currently: No-op, subsystem handles all logic
}

void FSFInputHandler::OnModifierScaleXPressed(const FInputActionValue& Value)
{
	bModifierScaleXActive = true;
	UE_LOG(LogSmartFoundations, Warning, TEXT("[KEY] Modifier Scale X: ACTIVE (X key held) - Wheel should now work for X-axis scaling"));
	if (USFSubsystem* Subsystem = OwnerSubsystem.Get())
	{
		Subsystem->UpdateCounterDisplay();
	}
}

void FSFInputHandler::OnModifierScaleXReleased(const FInputActionValue& Value)
{
	bModifierScaleXActive = false;
	UE_LOG(LogSmartFoundations, Warning, TEXT("[KEY] Modifier Scale X: Inactive (X released)"));
	if (USFSubsystem* Subsystem = OwnerSubsystem.Get())
	{
		Subsystem->UpdateCounterDisplay();
	}
}

void FSFInputHandler::OnModifierScaleYPressed(const FInputActionValue& Value)
{
	bModifierScaleYActive = true;
	UE_LOG(LogSmartFoundations, Warning, TEXT("[KEY] Modifier Scale Y: ACTIVE (Z held)"));
	if (USFSubsystem* Subsystem = OwnerSubsystem.Get())
	{
		Subsystem->UpdateCounterDisplay();
	}
}

void FSFInputHandler::OnModifierScaleYReleased(const FInputActionValue& Value)
{
	bModifierScaleYActive = false;
	UE_LOG(LogSmartFoundations, Warning, TEXT("[KEY] Modifier Scale Y: Inactive (Z released)"));
	if (USFSubsystem* Subsystem = OwnerSubsystem.Get())
	{
		Subsystem->UpdateCounterDisplay();
	}
}

void FSFInputHandler::OnSpacingModeChanged(const FInputActionValue& Value)
{
	bool bPressed = Value.Get<bool>();
	bSpacingModeActive = bPressed;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("InputHandler: Spacing mode %s"), 
		bPressed ? TEXT("activated") : TEXT("deactivated"));
	if (USFSubsystem* Subsystem = OwnerSubsystem.Get())
	{
		Subsystem->UpdateCounterDisplay();
	}
}

void FSFInputHandler::OnSpacingCycleAxis()
{
	// Input processing handled in subsystem for complex logic
	// This method exists for future extraction of input validation logic
	// Currently: No-op, subsystem handles all logic
}

void FSFInputHandler::OnStepsModeChanged(const FInputActionValue& Value)
{
	bool bPressed = Value.Get<bool>();
	bStepsModeActive = bPressed;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("InputHandler: Steps mode %s"), 
		bPressed ? TEXT("activated") : TEXT("deactivated"));
	if (USFSubsystem* Subsystem = OwnerSubsystem.Get())
	{
		Subsystem->UpdateCounterDisplay();
	}
}

void FSFInputHandler::OnCycleAxis()
{
	// Input processing handled in subsystem for complex logic
	// This method exists for future extraction of input validation logic
	// Currently: No-op, subsystem handles all logic
}

void FSFInputHandler::OnStaggerModeChanged(const FInputActionValue& Value)
{
	bool bPressed = Value.Get<bool>();
	bStaggerModeActive = bPressed;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("InputHandler: Stagger mode %s"), 
		bPressed ? TEXT("activated") : TEXT("deactivated"));
	if (USFSubsystem* Subsystem = OwnerSubsystem.Get())
	{
		Subsystem->UpdateCounterDisplay();
	}
}

void FSFInputHandler::OnRotationModeChanged(const FInputActionValue& Value)
{
	bool bPressed = Value.Get<bool>();
	bRotationModeActive = bPressed;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("InputHandler: Rotation mode %s"), 
		bPressed ? TEXT("activated") : TEXT("deactivated"));
	if (USFSubsystem* Subsystem = OwnerSubsystem.Get())
	{
		Subsystem->UpdateCounterDisplay();
	}
}

void FSFInputHandler::OnValueIncreased(const FInputActionValue& Value)
{
	// Input processing handled in subsystem for complex logic
	// This method exists for future extraction of input validation logic
	// Currently: No-op, subsystem handles all logic
}

void FSFInputHandler::OnValueDecreased(const FInputActionValue& Value)
{
	// Input processing handled in subsystem for complex logic
	// This method exists for future extraction of input validation logic
	// Currently: No-op, subsystem handles all logic
}

void FSFInputHandler::OnToggleArrows()
{
	// Input processing handled in subsystem for complex logic
	// This method exists for future extraction of input validation logic
	// Currently: No-op, subsystem handles all logic
}



void FSFInputHandler::OnDebugPrimaryFire()
{
	// Input processing handled in subsystem for complex logic
	// This method exists for future extraction of input validation logic
	// Currently: No-op, subsystem handles all logic
}

void FSFInputHandler::ApplyAxisScaling(ESFScaleAxis Axis, int32 StepDelta, const TCHAR* DebugLabel)
{
	// Forward to subsystem for actual scaling logic
	if (USFSubsystem* Subsystem = OwnerSubsystem.Get())
	{
		Subsystem->ApplyAxisScaling(Axis, StepDelta, DebugLabel);
	}
}
