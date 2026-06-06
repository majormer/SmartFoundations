// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "SmartFoundationsModConfiguration.generated.h"

/**
 * C++ archetype for the Smart! mod configuration.
 *
 * Why this exists: Smart_Config is a Blueprint-authored config. A purely BP-authored
 * config whose RootSection tree is created programmatically (e.g. via the editor
 * scripting API) does NOT survive cooking - the instanced sub-objects get stripped,
 * so the shipped config tree is empty and settings fall back to defaults. Giving the
 * config a C++ class whose constructor builds the section tree as default sub-objects
 * provides the archetype the Blueprint override is serialized against, so the cooked
 * asset retains the full tree. The Blueprint override re-skins the same keys with the
 * renderable BP_ConfigProperty* classes for the Mods menu (the C++ base classes have
 * no editor widget). Keys here MUST match the Blueprint override and FSmart_ConfigStruct_Sections.
 */
UCLASS()
class SMARTFOUNDATIONS_API USmartFoundationsModConfiguration : public UModConfiguration
{
	GENERATED_BODY()

public:
	USmartFoundationsModConfiguration();

private:
	class UConfigPropertySection* CreateSection(const FName& Name, const FText& DisplayName, const FText& Tooltip);
	class UConfigPropertyBool* CreateBoolProperty(const FName& Name, const FText& DisplayName, const FText& Tooltip, bool Value);
	class UConfigPropertyInteger* CreateIntegerProperty(const FName& Name, const FText& DisplayName, const FText& Tooltip, int32 Value);
	class UConfigPropertyFloat* CreateFloatProperty(const FName& Name, const FText& DisplayName, const FText& Tooltip, float Value);
};
