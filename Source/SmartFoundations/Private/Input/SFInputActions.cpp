#include "SFInputActions.h"
#include "SmartFoundations.h"

USFInputActions::USFInputActions()
{
	// Initialize input actions will be done in PostLoad after assets are available
}

void USFInputActions::PostLoad()
{
	Super::PostLoad();
	InitializeGameplayTags();
}

void USFInputActions::InitializeGameplayTags()
{
	// Initialize gameplay tag bindings for easy access
	// These tags follow the Smart! namespace convention
	
	if (IA_SmartScaleAxis)
	{
		InputActionTagBindings.Add(IA_SmartScaleAxis, 
			FGameplayTag::RequestGameplayTag(TEXT("Smart.Input.Scale.Axis")));
	}

	if (IA_SmartScaleMouseWheel)
	{
		InputActionTagBindings.Add(IA_SmartScaleMouseWheel, 
			FGameplayTag::RequestGameplayTag(TEXT("Smart.Input.Scale.MouseWheel")));
	}

	if (IA_SmartScaleArrowKeys)
	{
		InputActionTagBindings.Add(IA_SmartScaleArrowKeys, 
			FGameplayTag::RequestGameplayTag(TEXT("Smart.Input.Scale.ArrowKeys")));
	}

	if (IA_SmartScaleShiftModifier)
	{
		InputActionTagBindings.Add(IA_SmartScaleShiftModifier, 
			FGameplayTag::RequestGameplayTag(TEXT("Smart.Input.Scale.ShiftModifier")));
	}

	if (IA_SmartScaleAltModifier)
	{
		InputActionTagBindings.Add(IA_SmartScaleAltModifier, 
			FGameplayTag::RequestGameplayTag(TEXT("Smart.Input.Scale.AltModifier")));
	}

	if (IA_SmartSpacing)
	{
		InputActionTagBindings.Add(IA_SmartSpacing, 
			FGameplayTag::RequestGameplayTag(TEXT("Smart.Input.Spacing")));
	}

	if (IA_SmartNudge)
	{
		InputActionTagBindings.Add(IA_SmartNudge, 
			FGameplayTag::RequestGameplayTag(TEXT("Smart.Input.Nudge")));
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("Initialized Smart! input action gameplay tags"));
}
