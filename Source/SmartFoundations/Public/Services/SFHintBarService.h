#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SFHintBarService.generated.h"

// Forward declarations
class USFSubsystem;
class UUserWidget;
class UInputAction;
class UFGInputMappingContext;

/**
 * Issue #281: Manages Smart! keybind hints in the vanilla Widget_BuildMode hint bar.
 * Uses Struct_KeybindingHint and SetKeybindingHints on Widget_BuildMode (Blueprint function).
 * Uses the vanilla build-mode hint bar so Smart! keybinds appear beside native hints.
 *
 * Keybind text is resolved live from the Input Mapping Context so user rebinds are reflected.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFHintBarService : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USFSubsystem* InSubsystem);
	void Shutdown();

	void OnHologramRegistered();
	void OnHologramUnregistered();

private:
	// Find the Widget_BuildMode widget instance at runtime
	UUserWidget* FindWidgetBuildMode() const;

	// Inject Smart! hints into Widget_BuildMode via SetKeybindingHints
	void InjectSmartHints();

	// Timer callback: re-inject after vanilla rebuilds hints
	void OnHintTickTimer();

	// Load all Smart! input action assets used for hint keybind resolution
	void LoadInputActions();

	// Resolve the display key text for an input action from the mapping context
	FText ResolveKeyText(const UInputAction* Action) const;

private:
	TWeakObjectPtr<USFSubsystem> Subsystem;
	TWeakObjectPtr<UUserWidget> CachedBuildModeWidget;

	// Cached UScriptStruct and UFunction for Struct_KeybindingHint / SetKeybindingHints
	UScriptStruct* KeybindingHintStruct = nullptr;
	UFunction* SetKeybindingHintsFunc = nullptr;

	// Cached FProperty pointers for struct fields
	FProperty* ActionProperty = nullptr;
	FProperty* KeyBindingProperty = nullptr;

	// Cached input action assets for keybind resolution
	UPROPERTY()
	UInputAction* IA_ModifierScaleX = nullptr;
	UPROPERTY()
	UInputAction* IA_ModifierScaleY = nullptr;
	UPROPERTY()
	UInputAction* IA_SpacingMode = nullptr;
	UPROPERTY()
	UInputAction* IA_StepsMode = nullptr;
	UPROPERTY()
	UInputAction* IA_StaggerMode = nullptr;
	UPROPERTY()
	UInputAction* IA_RotationMode = nullptr;
	UPROPERTY()
	UInputAction* IA_CycleAxis = nullptr;
	UPROPERTY()
	UInputAction* IA_RecipeMode = nullptr;
	UPROPERTY()
	UInputAction* IA_ToggleSettingsForm = nullptr;

	// Cached mapping context for keybind resolution
	UPROPERTY()
	UFGInputMappingContext* SmartMappingContext = nullptr;

	FTimerHandle HintTickTimer;
	bool bHintsInjected = false;
	bool bActionsLoaded = false;

	// Context fingerprint: tracks upgrade flag + adapter feature bits to detect hint changes
	uint32 LastContextFingerprint = UINT32_MAX; // Init to impossible value to force first injection
};
