#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Components/InputComponent.h"
#include "SFInputRegistry.generated.h"

/**
 * Smart! Input Registry - SML 3.11.x Enhanced Input + Gameplay Tags approach
 * Creates Input Actions and Mapping Context using modern Satisfactory input system
 * 
 * DOCUMENTATION: See docs/Input/SMART_INPUT_SYSTEM.md for complete input documentation
 * CRITICAL: Update docs when making ANY input changes
 */
UCLASS(BlueprintType, Blueprintable)
class SMARTFOUNDATIONS_API USFInputRegistry : public UObject
{
	GENERATED_BODY()
public:
	USFInputRegistry();

	/** Initialize Smart! input system using SML 3.11.x Enhanced Input approach */
	UFUNCTION(BlueprintCallable, Category = "Smart! Input")
	static void InitializeSmartInputSystem();

	/** Gameplay Tags for Smart! Input Actions (SML 3.11.x approach) */
	// Numpad scaling action tags
	static const FString TAG_SCALE_X_POSITIVE;
	static const FString TAG_SCALE_X_NEGATIVE;
	static const FString TAG_SCALE_Y_POSITIVE;
	static const FString TAG_SCALE_Y_NEGATIVE;
	static const FString TAG_SCALE_Z_POSITIVE;
	static const FString TAG_SCALE_Z_NEGATIVE;
	
	// Feature action tags
	static const FString TAG_SPACING;
	static const FString TAG_TOGGLE_ARROWS;
	static const FString TAG_RESET_SCALING;

	/** Get Smart! Input Mapping Context (Blueprint asset) */
	UFUNCTION(BlueprintCallable, Category = "Smart! Input")
	static class UFGInputMappingContext* GetSmartInputMappingContext();

	/** Bind subsystem methods to input actions using gameplay tags */
	UFUNCTION(BlueprintCallable, Category = "Smart! Input")
	static void BindInputActionsToSubsystem(class USFSubsystem* Subsystem, class UFGEnhancedInputComponent* InputComponent);

	/** Clear cached input mapping context (called during world cleanup) */
	static void ClearInputCache();

private:
	// No programmatic creation methods - Blueprint assets required for SML 3.11.x

	/** Mod reference for SML registration */
	static const FString MOD_REFERENCE;
};
