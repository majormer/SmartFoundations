#include "SFInputRegistry.h"
#include "SmartFoundations.h"
#include "Input/FGInputMappingContext.h"
#include "Input/FGEnhancedInputComponent.h"
#include "FGInputSettings.h"
#include "InputAction.h"
#include "Subsystem/SFSubsystem.h"
#include "GameplayTagsManager.h"
#include "Engine/Engine.h"

// Mod reference for SML (must match plugin name)
const FString USFInputRegistry::MOD_REFERENCE = TEXT("SmartFoundations");

// Gameplay Tags for Smart! Input Actions (SML 3.11.x Enhanced Input approach)
const FString USFInputRegistry::TAG_SCALE_X_POSITIVE = TEXT("Smart.Input.Scale.X.Positive");
const FString USFInputRegistry::TAG_SCALE_X_NEGATIVE = TEXT("Smart.Input.Scale.X.Negative");
const FString USFInputRegistry::TAG_SCALE_Y_POSITIVE = TEXT("Smart.Input.Scale.Y.Positive");
const FString USFInputRegistry::TAG_SCALE_Y_NEGATIVE = TEXT("Smart.Input.Scale.Y.Negative");
const FString USFInputRegistry::TAG_SCALE_Z_POSITIVE = TEXT("Smart.Input.Scale.Z.Positive");
const FString USFInputRegistry::TAG_SCALE_Z_NEGATIVE = TEXT("Smart.Input.Scale.Z.Negative");
const FString USFInputRegistry::TAG_SPACING = TEXT("Smart.Input.Spacing");
const FString USFInputRegistry::TAG_TOGGLE_ARROWS = TEXT("Smart.Input.ToggleArrows");
const FString USFInputRegistry::TAG_RESET_SCALING = TEXT("Smart.Input.ResetScaling");

USFInputRegistry::USFInputRegistry()
{
}

void USFInputRegistry::InitializeSmartInputSystem()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("Smart! Enhanced Input system initialization - SML 3.11.x Blueprint approach"));

	UE_LOG(LogSmartFoundations, Log, TEXT("📋 Smart! Gameplay Tags ready for Blueprint binding:"));
	UE_LOG(LogSmartFoundations, Log, TEXT("  - %s (Num8 - Scale Forward)"), *TAG_SCALE_X_POSITIVE);
	UE_LOG(LogSmartFoundations, Log, TEXT("  - %s (Num5 - Scale Backward)"), *TAG_SCALE_X_NEGATIVE);
	UE_LOG(LogSmartFoundations, Log, TEXT("  - %s (Num6 - Scale Right)"), *TAG_SCALE_Y_POSITIVE);
	UE_LOG(LogSmartFoundations, Log, TEXT("  - %s (Num4 - Scale Left)"), *TAG_SCALE_Y_NEGATIVE);
	UE_LOG(LogSmartFoundations, Log, TEXT("  - %s (Num9 - Scale Up)"), *TAG_SCALE_Z_POSITIVE);
	UE_LOG(LogSmartFoundations, Log, TEXT("  - %s (Num3 - Scale Down)"), *TAG_SCALE_Z_NEGATIVE);
	UE_LOG(LogSmartFoundations, Log, TEXT("  - %s (Num0 - Spacing Mode)"), *TAG_SPACING);
	UE_LOG(LogSmartFoundations, Log, TEXT("  - %s (Num1 - Toggle Arrows)"), *TAG_TOGGLE_ARROWS);
	UE_LOG(LogSmartFoundations, Log, TEXT("  - %s (Num7 - Reset Scaling)"), *TAG_RESET_SCALING);

	// Ensure gameplay tags exist even if no external config is present
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
	auto EnsureTag = [&TagManager](const FString& TagName, const TCHAR* Description)
	{
		const FName TagFName(*TagName);
		FGameplayTag ExistingTag = TagManager.RequestGameplayTag(TagFName, /*ErrorIfNotFound*/ false);
		if (!ExistingTag.IsValid())
		{
			TagManager.AddNativeGameplayTag(TagFName, Description);
			UE_LOG(LogSmartFoundations, Log, TEXT("Registered gameplay tag: %s"), *TagName);
		}
	};

	EnsureTag(TAG_SCALE_X_POSITIVE, TEXT("Smart! numpad scaling forward"));
	EnsureTag(TAG_SCALE_X_NEGATIVE, TEXT("Smart! numpad scaling backward"));
	EnsureTag(TAG_SCALE_Y_POSITIVE, TEXT("Smart! numpad scaling right"));
	EnsureTag(TAG_SCALE_Y_NEGATIVE, TEXT("Smart! numpad scaling left"));
	EnsureTag(TAG_SCALE_Z_POSITIVE, TEXT("Smart! numpad scaling up"));
	EnsureTag(TAG_SCALE_Z_NEGATIVE, TEXT("Smart! numpad scaling down"));
	EnsureTag(TAG_SPACING, TEXT("Smart! spacing mode toggle"));
	EnsureTag(TAG_TOGGLE_ARROWS, TEXT("Smart! arrow visibility toggle"));
	EnsureTag(TAG_RESET_SCALING, TEXT("Smart! reset hologram scaling"));

	UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ REQUIRED: Create Blueprint Input Actions in Unreal Editor"));
	UE_LOG(LogSmartFoundations, Warning, TEXT("📖 See docs/Input/SMART_INPUT_SYSTEM.md for step-by-step guide"));
}

// Static cache - shared across all calls
static UFGInputMappingContext* GSmartInputMappingContextCache = nullptr;

void USFInputRegistry::ClearInputCache()
{
	if (GSmartInputMappingContextCache)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Clearing Smart! Input Mapping Context cache (world cleanup)"));
		GSmartInputMappingContextCache = nullptr;
	}
}

UFGInputMappingContext* USFInputRegistry::GetSmartInputMappingContext()
{
	if (!GSmartInputMappingContextCache)
	{
		// Mapping context renamed to reflect broader input scope (not just numpad)
		const FSoftObjectPath CorrectPath(TEXT("/SmartFoundations/SmartFoundations/Input/Contexts/MC_Smart_BuildGunBuild.MC_Smart_BuildGunBuild"));

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 Loading Smart! Input Mapping Context from: %s"), *CorrectPath.ToString());

		// Try multiple loading methods
		UObject* LoadedObject = CorrectPath.TryLoad();

		if (!LoadedObject)
		{
			LoadedObject = LoadObject<UFGInputMappingContext>(nullptr, *CorrectPath.ToString());
		}

		if (!LoadedObject)
		{
			LoadedObject = StaticLoadObject(UFGInputMappingContext::StaticClass(), nullptr, *CorrectPath.ToString());
		}

		GSmartInputMappingContextCache = Cast<UFGInputMappingContext>(LoadedObject);

		if (GSmartInputMappingContextCache)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Smart! Input Mapping Context loaded successfully"));
		}
		else
		{
			UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to load Smart! Input Mapping Context"));
			UE_LOG(LogSmartFoundations, Error, TEXT("❌ Asset should be at: /SmartFoundations/SmartFoundations/Input/Contexts/MC_Smart_BuildGunBuild"));
			UE_LOG(LogSmartFoundations, Error, TEXT("❌ Verify Blueprint asset exists in Unreal Editor"));
		}
	}

	return GSmartInputMappingContextCache;
}

void USFInputRegistry::BindInputActionsToSubsystem(USFSubsystem* Subsystem, UFGEnhancedInputComponent* InputComponent)
{
	if (!Subsystem || !InputComponent)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("BindInputActionsToSubsystem: Invalid subsystem or input component"));
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Binding NEW Smart! Axis1D input actions to subsystem"));

	// Helper to load Input Action assets
	auto LoadIA = [](const TCHAR* Path) -> UInputAction*
	{
		const FSoftObjectPath P(Path);
		UObject* Obj = P.TryLoad();
		if (!Obj) Obj = LoadObject<UInputAction>(nullptr, Path);
		return Cast<UInputAction>(Obj);
	};

	int32 BoundCount = 0;

	// === Grid Scaling Actions (Axis1D) ===
	// NOTE: Using Started event for NumPad keys (InputTriggerPressed)
	//   - NumPad keys: InputTriggerPressed → Started event
	//   - MouseWheel: Requires manual check (parent context consumes input first)

	if (UInputAction* IA_ScaleX = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_ScaleX.IA_Smart_ScaleX")))
	{
		InputComponent->BindAction(IA_ScaleX, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnScaleXChanged);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Scale X (Axis1D) - Started event"));
		++BoundCount;
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to load: IA_Smart_ScaleX"));
	}

	if (UInputAction* IA_ScaleY = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_ScaleY.IA_Smart_ScaleY")))
	{
		InputComponent->BindAction(IA_ScaleY, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnScaleYChanged);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Scale Y (Axis1D) - Started event"));
		++BoundCount;
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to load: IA_Smart_ScaleY"));
	}

	if (UInputAction* IA_ScaleZ = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_ScaleZ.IA_Smart_ScaleZ")))
	{
		InputComponent->BindAction(IA_ScaleZ, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnScaleZChanged);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Scale Z (Axis1D) - Started event"));
		++BoundCount;
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to load: IA_Smart_ScaleZ"));
	}

	// === Mouse Wheel (Unified context-aware handler) ===

	if (UInputAction* IA_MouseWheel = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_MouseWheel.IA_Smart_MouseWheel")))
	{
		InputComponent->BindAction(IA_MouseWheel, ETriggerEvent::Triggered, Subsystem, &USFSubsystem::OnMouseWheelChanged);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Mouse Wheel (Axis1D - context-aware) - Triggered event"));
		++BoundCount;
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to load: IA_Smart_MouseWheel"));
	}

	// === Modifier Actions (Boolean) ===

	if (UInputAction* IA_ModScaleX = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Modifier_ScaleX.IA_Smart_Modifier_ScaleX")))
	{
		InputComponent->BindAction(IA_ModScaleX, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnModifierScaleXPressed);
		InputComponent->BindAction(IA_ModScaleX, ETriggerEvent::Completed, Subsystem, &USFSubsystem::OnModifierScaleXReleased);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Modifier Scale X (Boolean - Started+Completed)"));
		BoundCount += 2;
	}

	if (UInputAction* IA_ModScaleY = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Modifier_ScaleY.IA_Smart_Modifier_ScaleY")))
	{
		InputComponent->BindAction(IA_ModScaleY, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnModifierScaleYPressed);
		InputComponent->BindAction(IA_ModScaleY, ETriggerEvent::Completed, Subsystem, &USFSubsystem::OnModifierScaleYReleased);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Modifier Scale Y (Boolean - Started+Completed)"));
		BoundCount += 2;
	}

	// === Spacing Actions ===

	if (UInputAction* IA_SpacingMode = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Spacing_Mode.IA_Smart_Spacing_Mode")))
	{
		InputComponent->BindAction(IA_SpacingMode, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnSpacingModeChanged);
		InputComponent->BindAction(IA_SpacingMode, ETriggerEvent::Completed, Subsystem, &USFSubsystem::OnSpacingModeChanged);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Spacing Mode (Boolean)"));
		BoundCount += 2;
	}

	// === Recipe Actions ===

	if (UInputAction* IA_RecipeMode = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_RecipeMode.IA_Smart_RecipeMode")))
	{
		InputComponent->BindAction(IA_RecipeMode, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnRecipeModeChanged);
		InputComponent->BindAction(IA_RecipeMode, ETriggerEvent::Completed, Subsystem, &USFSubsystem::OnRecipeModeChanged);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Recipe Mode (Boolean)"));
		BoundCount += 2;
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to load: IA_Smart_RecipeMode"));
	}

	// === UNIFIED VALUE ADJUSTMENT (replaces individual Spacing/Steps/Stagger adjust actions) ===

	if (UInputAction* IA_IncreaseValue = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_IncreaseValue.IA_Smart_IncreaseValue")))
	{
		InputComponent->BindAction(IA_IncreaseValue, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnValueIncreased);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Increase Value (Axis1D - context-aware) - Started event"));
		++BoundCount;
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to load: IA_Smart_IncreaseValue"));
	}

	if (UInputAction* IA_DecreaseValue = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_DecreaseValue.IA_Smart_DecreaseValue")))
	{
		InputComponent->BindAction(IA_DecreaseValue, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnValueDecreased);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Decrease Value (Axis1D - context-aware) - Started event"));
		++BoundCount;
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to load: IA_Smart_DecreaseValue"));
	}

	// Generic Cycle Axis - context-aware (works for Spacing, Steps, Stagger modes)
	if (UInputAction* IA_CycleAxis = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_CycleAxis.IA_Smart_CycleAxis")))
	{
		InputComponent->BindAction(IA_CycleAxis, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnCycleAxis);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Cycle Axis (Boolean - context-aware)"));
		++BoundCount;
	}

	// === Steps Actions ===

	if (UInputAction* IA_StepsMode = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Steps_Mode.IA_Smart_Steps_Mode")))
	{
		InputComponent->BindAction(IA_StepsMode, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnStepsModeChanged);
		InputComponent->BindAction(IA_StepsMode, ETriggerEvent::Completed, Subsystem, &USFSubsystem::OnStepsModeChanged);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Steps Mode (Boolean)"));
		BoundCount += 2;
	}

	// === Stagger Actions (lateral grid offset) ===

	if (UInputAction* IA_StaggerMode = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Stagger_Mode.IA_Smart_Stagger_Mode")))
	{
		InputComponent->BindAction(IA_StaggerMode, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnStaggerModeChanged);
		InputComponent->BindAction(IA_StaggerMode, ETriggerEvent::Completed, Subsystem, &USFSubsystem::OnStaggerModeChanged);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Stagger Mode (Boolean) - lateral grid offset"));
		BoundCount += 2;
	}

	// === Rotation Actions (radial/arc placement) ===

	if (UInputAction* IA_RotationMode = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Rotation_Mode.IA_Smart_Rotation_Mode")))
	{
		InputComponent->BindAction(IA_RotationMode, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnRotationModeChanged);
		InputComponent->BindAction(IA_RotationMode, ETriggerEvent::Completed, Subsystem, &USFSubsystem::OnRotationModeChanged);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Rotation Mode (Boolean) - radial/arc placement"));
		BoundCount += 2;
	}

	// === Toggle Arrows (Unchanged) ===

	if (UInputAction* IA_ToggleArrows = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_ToggleArrows.IA_Smart_ToggleArrows")))
	{
		InputComponent->BindAction(IA_ToggleArrows, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnToggleArrows);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Toggle Arrows (Boolean)"));
		++BoundCount;
	}

	// === Settings Form Interface (Phase 0 Validation) ===

	if (UInputAction* IA_ToggleSettingsForm = LoadIA(TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_ToggleSettingsForm.IA_Smart_ToggleSettingsForm")))
	{
		InputComponent->BindAction(IA_ToggleSettingsForm, ETriggerEvent::Started, Subsystem, &USFSubsystem::OnToggleSettingsForm);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Bound: Toggle Settings Form (Boolean) - Phase 0 validation"));
		++BoundCount;
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to load: IA_Smart_ToggleSettingsForm"));
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Smart! NEW input binding complete - %d action bindings registered"), BoundCount);
}
