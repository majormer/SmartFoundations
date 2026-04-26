#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "InputTriggers.h"
#include "InputAction.h"
#include "SFInputActions.generated.h"

/**
 * Smart! Input Actions - Modern SML 3.11.x Enhanced Input system
 * Defines scaling and other Smart! input actions with gameplay tags
 */
UCLASS(BlueprintType, Blueprintable)
class SMARTFOUNDATIONS_API USFInputActions : public UDataAsset
{
	GENERATED_BODY()

public:
	USFInputActions();

	/** Scaling Input Actions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart! Scaling")
	class UInputAction* IA_SmartScaleAxis;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart! Scaling")
	class UInputAction* IA_SmartScaleMouseWheel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart! Scaling")
	class UInputAction* IA_SmartScaleArrowKeys;

	/** Scaling modifier keys */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart! Scaling")
	class UInputAction* IA_SmartScaleShiftModifier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart! Scaling")
	class UInputAction* IA_SmartScaleAltModifier;

	/** Future Smart! actions (prepared for other features) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart! Features")
	class UInputAction* IA_SmartSpacing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart! Features")
	class UInputAction* IA_SmartNudge;

	/** Gameplay Tag bindings for easier access */
	UPROPERTY(EditDefaultsOnly, Category = "Advanced | Input")
	TMap<UInputAction*, FGameplayTag> InputActionTagBindings;

	/** Initialize gameplay tags */
	virtual void PostLoad() override;

private:
	void InitializeGameplayTags();
};
