// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Subsystem/SFHologramDataService.h"
#include "SmartFoundations.h"
#include "Hologram/FGHologram.h"
#include "Logging/LogMacros.h"
#include "Components/SceneComponent.h"
#include "HAL/PlatformStackWalk.h"

namespace
{
	// [#497 ORIGIN-TRAP — TEMPORARY DIAGNOSTIC, remove after root cause] Something in vanilla drags
	// extend child holograms to world origin every frame even though every child class no-ops
	// SetHologramLocationAndRotation and the parent blocks propagation while extend is active
	// (SmartMCP: 67/68 previews at exactly 0,0,0 once the per-frame reapply was removed; the same
	// ASFBuildableChildHologram class holds position at 60K children in grid scaling). This trap
	// binds to each child's root TransformUpdated and, the first time a child's root lands within
	// 1 m of world origin, dumps the CALLER stack to the log — CSS ships PDBs for the modular
	// engine DLLs, so the vanilla frames symbolicate. Repro: hold a 2×1 Extend for a few seconds,
	// then grep the log for "ORIGIN-TRAP". One dump per actor; near-zero cost otherwise.
	void SFInstallOriginMoveTrap(AFGHologram* ChildHologram)
	{
		USceneComponent* Root = ChildHologram ? ChildHologram->GetRootComponent() : nullptr;
		if (!Root)
		{
			return;
		}

		Root->TransformUpdated.AddLambda([](USceneComponent* UpdatedComponent, EUpdateTransformFlags, ETeleportType)
		{
			if (!UpdatedComponent)
			{
				return;
			}
			const FVector Loc = UpdatedComponent->GetComponentLocation();
			if (Loc.SizeSquared() > 100.0 * 100.0)
			{
				return;  // only trips within 1 m of world origin
			}

			AActor* Owner = UpdatedComponent->GetOwner();
			static TSet<FName> DumpedOwners;
			const FName OwnerName = Owner ? Owner->GetFName() : NAME_None;
			if (DumpedOwners.Contains(OwnerName))
			{
				return;
			}
			DumpedOwners.Add(OwnerName);

			UE_LOG(LogSmartFoundations, Warning, TEXT("[#497 ORIGIN-TRAP] %s (%s) root moved to %s — caller stack:"),
				*GetNameSafe(Owner), Owner ? *Owner->GetClass()->GetName() : TEXT("?"), *Loc.ToString());

			constexpr SIZE_T BufSize = 32768;
			ANSICHAR* Buf = static_cast<ANSICHAR*>(FMemory::SystemMalloc(BufSize));
			if (Buf)
			{
				Buf[0] = 0;
				FPlatformStackWalk::StackWalkAndDump(Buf, BufSize, 1);
				FString Stack = FString(ANSI_TO_TCHAR(Buf));
				TArray<FString> Lines;
				Stack.ParseIntoArrayLines(Lines);
				for (const FString& Line : Lines)
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("[#497 ORIGIN-TRAP]   %s"), *Line);
				}
				FMemory::SystemFree(Buf);
			}
		});
	}
}

FSFHologramData* USFHologramDataService::GetOrCreateData(AFGHologram* Hologram) {
    if (!Hologram) return nullptr;
    
    // Try to get existing data
    FSFHologramData* Data = USFHologramDataRegistry::GetData(Hologram);
    
    // Create if doesn't exist
    if (!Data) {
        Data = USFHologramDataRegistry::AttachData(Hologram);
    }
    
    return Data;
}

void USFHologramDataService::DisableValidation(AFGHologram* Hologram) {
    if (FSFHologramData* Data = GetOrCreateData(Hologram)) {
        Data->bNeedToCheckPlacement = false;
        Data->bIgnoreLocationUpdates = true;
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("DisableValidation: Disabled validation for %s"), 
            *Hologram->GetName());
    }
}

void USFHologramDataService::EnableValidation(AFGHologram* Hologram) {
    if (FSFHologramData* Data = GetOrCreateData(Hologram)) {
        Data->bNeedToCheckPlacement = true;
        Data->bIgnoreLocationUpdates = false;
    }
}

void USFHologramDataService::MarkAsChild(AFGHologram* ChildHologram, AFGHologram* ParentHologram, ESFChildHologramType ChildType) {
    if (FSFHologramData* Data = GetOrCreateData(ChildHologram)) {
        Data->bIsChildHologram = true;
        Data->ParentHologram = ParentHologram;
        Data->ChildType = ChildType;

        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("MarkAsChild: Marked %s as child of %s (type: %d)"),
            *ChildHologram->GetName(), *ParentHologram->GetName(), (int32)ChildType);

        // [#497 ORIGIN-TRAP — TEMPORARY DIAGNOSTIC] Guard against double-binding on re-marks
        // (restore re-marks children); the trap itself dumps once per actor regardless.
        if (!Data->bOriginTrapInstalled)
        {
            Data->bOriginTrapInstalled = true;
            SFInstallOriginMoveTrap(ChildHologram);
        }
    }
}

void USFHologramDataService::StoreRecipe(AFGHologram* Hologram, TSubclassOf<UFGRecipe> Recipe) {
    if (FSFHologramData* Data = GetOrCreateData(Hologram)) {
        Data->StoredRecipe = Recipe;
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("StoreRecipe: Stored recipe %s in hologram %s"), 
            *Recipe->GetName(), *Hologram->GetName());
    }
}

TSubclassOf<UFGRecipe> USFHologramDataService::GetStoredRecipe(AFGHologram* Hologram) {
    if (FSFHologramData* Data = USFHologramDataRegistry::GetData(Hologram)) {
        return Data->StoredRecipe;
    }
    return nullptr;
}

void USFHologramDataService::OnHologramDestroyed(AFGHologram* Hologram) {
    USFHologramDataRegistry::ClearData(Hologram);
}
