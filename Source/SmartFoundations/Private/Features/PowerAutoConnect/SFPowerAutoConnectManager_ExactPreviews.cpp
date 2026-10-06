// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Holograms/Power/SFWireHologram.h"

void FSFPowerAutoConnectManager::UpdateExactPreviews(AFGHologram* Parent,
    const TArray<TPair<UFGPowerConnectionComponent*, UFGPowerConnectionComponent*>>& Desired)
{
    // Reuse unchanged endpoint pairs and geometry. A stationary grid never re-locks or repaints wires.
    TArray<TSharedPtr<FPowerLinePreviewHelper>> Existing;
    for (const auto& Group : PowerLinePreviews) Existing.Append(Group.Value);
    TMap<UFGPowerConnectionComponent*, TArray<TSharedPtr<FPowerLinePreviewHelper>>> BySource;
    for (const auto& Preview : Existing)
        if (Preview.IsValid()) BySource.FindOrAdd(Preview->GetStartConnection()).Add(Preview);
    TArray<TSharedPtr<FPowerLinePreviewHelper>> Retained;
    TSet<FPowerLinePreviewHelper*> RetainedSet;
    for (const auto& Span : Desired)
    {
        TSharedPtr<FPowerLinePreviewHelper> Preview;
        if (const auto* Matches = BySource.Find(Span.Key))
            for (const auto& Match : *Matches)
                if (Match->IsPreviewValid() && Match->GetEndConnection() == Span.Value) { Preview = Match; break; }
        if (!Preview.IsValid()) Preview = MakeShared<FPowerLinePreviewHelper>(Parent->GetWorld(), Parent);
        const TPair<FVector, FVector> Positions(Span.Key->GetComponentLocation(), Span.Value->GetComponentLocation());
        const auto* Previous = WallPreviewPositions.Find(Preview.Get());
        if (!Previous || !Previous->Key.Equals(Positions.Key, 0.01) || !Previous->Value.Equals(Positions.Value, 0.01))
        {
            if (!Preview->UpdatePreview(Span.Key, Span.Value)) continue;
            Preview->GetHologram()->Tags.AddUnique(TEXT("SF_ExactPowerPlan"));
            WallPreviewPositions.Add(Preview.Get(), Positions);
        }
        if (Preview->GetHologram()->GetBlueprintDesigner() != Parent->GetBlueprintDesigner())
            Preview->GetHologram()->SetInsideBlueprintDesigner(Parent->GetBlueprintDesigner());
        Retained.Add(Preview);
        RetainedSet.Add(Preview.Get());
    }
    for (const auto& Preview : Existing)
        if (Preview.IsValid() && !RetainedSet.Contains(Preview.Get()))
        {
            WallPreviewPositions.Remove(Preview.Get());
            Preview->DestroyPreview();
        }
    PowerLinePreviews.Empty();
    PowerLinePreviews.Add(Parent, MoveTemp(Retained));
}

