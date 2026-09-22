// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFPipeColorSnapshot.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Hologram/FGBuildableHologram.h"
#include "FGPipeConnectionComponent.h"
#include "FGRecipeManager.h"
#include "FGCustomizationRecipe.h"
#include "Holograms/Logistics/SFPipelineHologram.h"

namespace
{
	bool IsFiniteColor(const FLinearColor& Color)
	{
		return FMath::IsFinite(Color.R) && FMath::IsFinite(Color.G) && FMath::IsFinite(Color.B) && FMath::IsFinite(Color.A);
	}
}

FSFPipeColorSnapshot FSFPipeColorSnapshot::Capture(const FFactoryCustomizationData& Data)
{
	FSFPipeColorSnapshot Result;
	Result.bCaptured = IsFiniteColor(Data.OverrideColorData.PrimaryColor) && IsFiniteColor(Data.OverrideColorData.SecondaryColor);
	Result.Swatch = Data.SwatchDesc;
	Result.Colors = Data.OverrideColorData;
	return Result;
}

FFactoryCustomizationData FSFPipeColorSnapshot::ToCustomization() const
{
	FFactoryCustomizationData Result;
	if (bCaptured)
	{
		Result.SwatchDesc = Swatch;
		Result.OverrideColorData = Colors;
	}
	return Result;
}

void FSFPipeColorSnapshot::Apply(AFGBuildableHologram* Hologram) const
{
	if (!Hologram || !bCaptured || !IsFiniteColor(Colors.PrimaryColor) || !IsFiniteColor(Colors.SecondaryColor)) return;
	FFactoryCustomizationData Data = ToCustomization();
	if (AFGRecipeManager* Recipes = AFGRecipeManager::Get(Hologram->GetWorld()))
	{
		const auto IsLocked = [Recipes](TSubclassOf<UFGFactoryCustomizationDescriptor> Desc)
		{
			const auto Recipe = Desc ? Recipes->GetCustomizationRecipeFromDesc(Desc) : nullptr;
			return Recipe && !Recipes->IsCustomizationRecipeAvailable(Recipe);
		};
		if (IsLocked(Data.SwatchDesc)) Data.SwatchDesc = nullptr;
		if (IsLocked(Data.OverrideColorData.PaintFinish)) Data.OverrideColorData.PaintFinish = nullptr;
	}
	Hologram->SetCustomizationData(Data);
}

void FSFPipeColorSnapshot::Inherit(ASFPipelineHologram* Hologram, UFGPipeConnectionComponent* Start, UFGPipeConnectionComponent* End)
{
	if (!IsValid(Hologram)) return;
	Hologram->ResetAutoConnectCustomization();
	for (UFGPipeConnectionComponent* Endpoint : {Start, End})
	{
		if (AFGBuildablePipeline* Pipe = Endpoint ? Cast<AFGBuildablePipeline>(Endpoint->GetOwner()) : nullptr)
		{
			Capture(Pipe->GetCustomizationData_Implementation()).Apply(Hologram);
			return;
		}
	}
	for (UFGPipeConnectionComponent* Endpoint : {Start, End})
	{
		AActor* Owner = Endpoint ? Endpoint->GetOwner() : nullptr;
		if (!Owner || !Owner->IsA<AFGBuildablePipelineAttachment>()) continue;
		FSFPipeColorSnapshot Consensus;
		bool bConflict = false;
		TInlineComponentArray<UFGPipeConnectionComponent*> Ports(Owner);
		for (UFGPipeConnectionComponent* Port : Ports)
		{
			auto* Other = Port ? Port->GetConnection() : nullptr;
			auto* Pipe = Other ? Cast<AFGBuildablePipeline>(Other->GetOwner()) : nullptr;
			if (!Pipe) continue;
			const auto Candidate = Capture(Pipe->GetCustomizationData_Implementation());
			if (!Candidate.bCaptured) { bConflict = true; continue; }
			if (Consensus.bCaptured && (Consensus.Swatch != Candidate.Swatch || Consensus.Colors != Candidate.Colors)) bConflict = true;
			Consensus = Candidate;
		}
		if (Consensus.bCaptured && !bConflict) { Consensus.Apply(Hologram); return; }
	}
}
