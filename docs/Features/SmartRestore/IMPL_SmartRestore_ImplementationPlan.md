---
title: Smart Restore Enhanced Implementation Plan
type: IMPL
date: 2026-04-30
status: Draft
category: Features
tags: [restore, presets, implementation, services, json]
related:
  - PLAN_SmartRestore_Enhanced.md
  - ../SmartPanel/IMPL_SmartPanel_CurrentFlow.md
  - ../Scaling/IMPL_Scaling_CurrentFlow.md
  - ../Extend/SFExtendService.h
---

# Smart Restore Enhanced — Implementation Plan

This document maps the feature specification to concrete classes, methods, and integration points in the SmartFoundations codebase. It covers the non-GUI backend first (data model, service, serialization, capture/restore logic) so the preset system can be wired into the Smart Panel UI afterward.

## Architecture Summary

```text
┌─────────────────────────────────────────────────┐
│                  Smart Panel UI                 │
│  (SmartSettingsFormWidget — future wiring)       │
│  Dropdown / Save / Apply / Delete / Export       │
└────────────┬───────────────────────┬─────────────┘
             │ Capture               │ Restore
             ▼                       ▼
┌─────────────────────────────────────────────────┐
│             USFRestoreService                   │
│  CapturePreset()   ApplyPreset()                │
│  SavePreset()      LoadPreset()                 │
│  DeletePreset()    ExportString()               │
│  ImportString()    GetPresetList()               │
│  ImportFromLastExtend()                          │
└────────────┬───────────────────────┬─────────────┘
             │ Read/Write            │ Read State
             ▼                       ▼
┌───────────────────────┐  ┌──────────────────────┐
│  FSFRestorePreset     │  │  USFSubsystem        │
│  (USTRUCT — data)     │  │  ├ CounterState       │
│  ├ BuildingClassPath  │  │  ├ AutoConnectRuntime │
│  ├ CaptureFlags       │  │  ├ ActiveRecipe       │
│  ├ Grid/Spacing/etc   │  │  └ GridStateService   │
│  ├ Recipe             │  ├──────────────────────┤
│  ├ AutoConnectState   │  │  USFExtendService     │
│  └ Version/Metadata   │  │  └ GetCurrentTopology │
└───────────────────────┘  └──────────────────────┘
             │
             ▼
┌───────────────────────┐
│  JSON File Storage    │
│  /Smart/Presets/*.json│
└───────────────────────┘
```

## Phase 1: Data Model

### New File: `Source/SmartFoundations/Public/Features/Restore/SFRestoreTypes.h`

```cpp
#pragma once
#include "CoreMinimal.h"
#include "SFRestoreTypes.generated.h"

/**
 * Capture flags — which field groups the user chose to include when saving.
 */
USTRUCT(BlueprintType)
struct FSFRestoreCaptureFlags
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) bool bGrid        = true;
    UPROPERTY(BlueprintReadWrite) bool bSpacing      = true;
    UPROPERTY(BlueprintReadWrite) bool bSteps        = true;
    UPROPERTY(BlueprintReadWrite) bool bStagger      = true;
    UPROPERTY(BlueprintReadWrite) bool bRotation     = true;
    UPROPERTY(BlueprintReadWrite) bool bRecipe       = true;
    UPROPERTY(BlueprintReadWrite) bool bAutoConnect  = true;
};

/**
 * Auto-connect settings snapshot (mirrors FAutoConnectRuntimeSettings).
 * Stored only when CaptureFlags.bAutoConnect is true.
 */
USTRUCT(BlueprintType)
struct FSFRestoreAutoConnectState
{
    GENERATED_BODY()

    UPROPERTY() bool bBeltEnabled          = true;
    UPROPERTY() int32 BeltTierMain         = 0;
    UPROPERTY() int32 BeltTierToBuilding   = 0;
    UPROPERTY() bool bChainDistributors    = true;
    UPROPERTY() int32 BeltRoutingMode      = 0;

    UPROPERTY() bool bPipeEnabled          = true;
    UPROPERTY() int32 PipeTierMain         = 0;
    UPROPERTY() int32 PipeTierToBuilding   = 0;
    UPROPERTY() bool bPipeIndicator        = true;
    UPROPERTY() int32 PipeRoutingMode      = 0;

    UPROPERTY() bool bPowerEnabled         = true;
    UPROPERTY() int32 PowerGridAxis        = 0;
    UPROPERTY() int32 PowerReserved        = 0;
};

/**
 * A single Smart Restore preset.
 *
 * BuildingClassPath is always captured. All other field groups are
 * optional based on CaptureFlags — fields whose flag is false are
 * not serialized and are ignored on restore.
 */
USTRUCT(BlueprintType)
struct FSFRestorePreset
{
    GENERATED_BODY()

    // === Identity ===
    UPROPERTY(BlueprintReadWrite) FString Name;
    UPROPERTY(BlueprintReadWrite) FString BuildingClassPath;

    // === Capture Flags ===
    UPROPERTY(BlueprintReadWrite) FSFRestoreCaptureFlags CaptureFlags;

    // === Grid (when bGrid) ===
    UPROPERTY() FIntVector GridCounters = FIntVector(1, 1, 1);

    // === Spacing (when bSpacing) ===
    UPROPERTY() int32 SpacingX = 0;
    UPROPERTY() int32 SpacingY = 0;
    UPROPERTY() int32 SpacingZ = 0;

    // === Steps (when bSteps) ===
    UPROPERTY() int32 StepsX = 0;
    UPROPERTY() int32 StepsY = 0;

    // === Stagger (when bStagger) ===
    UPROPERTY() int32 StaggerX  = 0;
    UPROPERTY() int32 StaggerY  = 0;
    UPROPERTY() int32 StaggerZX = 0;
    UPROPERTY() int32 StaggerZY = 0;

    // === Rotation (when bRotation) ===
    UPROPERTY() float RotationZ = 0.0f;

    // === Recipe (when bRecipe) ===
    UPROPERTY() FString RecipeClassPath;

    // === Auto-Connect (when bAutoConnect) ===
    UPROPERTY() FSFRestoreAutoConnectState AutoConnect;

    // === Metadata ===
    UPROPERTY() int32 Version = 1;
    UPROPERTY() FString CreatedAt;
    UPROPERTY() FString UpdatedAt;
};
```

### Design Notes

- `BuildingClassPath` stores the full class path string (e.g., `/Game/FactoryGame/Buildable/Factory/Constructor/Build_Constructor.Build_Constructor_C`). This is what the Satisfactory recipe system uses to identify buildings, and it is serializable to JSON without special handling.
- `RecipeClassPath` stores the recipe's class path (e.g., `/Game/FactoryGame/Recipes/Constructor/Recipe_Screw.Recipe_Screw_C`). On restore, this is validated against the current game state's unlocked recipes via the existing `USFRecipeManagementService`.
- Auto-connect state mirrors `FAutoConnectRuntimeSettings` from `SFSubsystem.h:1039` but uses a standalone USTRUCT so it can be serialized independently.
- Field types match `FSFCounterState` exactly (int32 for spacing/steps/stagger in cm, float for rotation in degrees, FIntVector for grid).

## Phase 2: Service

### New File: `Source/SmartFoundations/Public/Features/Restore/SFRestoreService.h`

```cpp
UCLASS()
class SMARTFOUNDATIONS_API USFRestoreService : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(USFSubsystem* InSubsystem);

    // === Capture ===

    /** Capture the current Smart Panel state into a new preset.
     *  @param Name          Preset name
     *  @param CaptureFlags  Which field groups to include
     *  @return The captured preset (not yet persisted)
     */
    FSFRestorePreset CaptureCurrentState(
        const FString& Name,
        const FSFRestoreCaptureFlags& CaptureFlags) const;

    // === Apply ===

    /** Apply a preset to the current Smart Panel state.
     *  Auto-switches the build gun to the preset's BuildingClassPath.
     *  Fields whose CaptureFlag is false are left untouched.
     *  @return true if applied, false if building class unavailable
     */
    bool ApplyPreset(const FSFRestorePreset& Preset);

    // === Persistence (JSON) ===

    bool SavePreset(const FSFRestorePreset& Preset);
    bool DeletePreset(const FString& Name);
    TArray<FSFRestorePreset> LoadAllPresets() const;
    TOptional<FSFRestorePreset> LoadPreset(const FString& Name) const;

    // === Export / Import (compact string) ===

    FString ExportToString(const FSFRestorePreset& Preset) const;
    TOptional<FSFRestorePreset> ImportFromString(const FString& Encoded) const;

    // === Extend Integration ===

    /** Import the last Extend topology as a preset source.
     *  Reads USFExtendService::GetCurrentTopology() to populate
     *  BuildingClassPath and (if available) recipe from the source factory.
     *  Grid/transform fields come from the current CounterState.
     *  @return A preset pre-populated from the last Extend, or empty if none cached
     */
    TOptional<FSFRestorePreset> ImportFromLastExtend(
        const FString& Name,
        const FSFRestoreCaptureFlags& CaptureFlags) const;

private:
    TWeakObjectPtr<USFSubsystem> Subsystem;

    /** Get the preset storage directory */
    FString GetPresetsDir() const;

    /** Serialize a preset to a JSON object */
    TSharedPtr<FJsonObject> PresetToJson(const FSFRestorePreset& Preset) const;

    /** Deserialize a preset from a JSON object */
    TOptional<FSFRestorePreset> JsonToPreset(const TSharedPtr<FJsonObject>& JsonObj) const;

    /** Sanitize a preset name for use as a filename */
    FString SanitizeFileName(const FString& Name) const;
};
```

### Key Implementation Details

#### CaptureCurrentState

Reads from:
- `USFSubsystem::GetCounterState()` → grid, spacing, steps, stagger, rotation (via `USFGridStateService`)
- `USFSubsystem::GetActiveRecipe()` → recipe class path
- `USFSubsystem::GetAutoConnectRuntimeSettings()` → auto-connect state
- Active hologram's building class → `BuildingClassPath`

The active hologram's building class can be obtained from the build gun's current recipe. The Subsystem already tracks the current hologram class through the recipe management service. The class path is extracted via `GetPathNameSafe(RecipeClass)`.

```cpp
FSFRestorePreset USFRestoreService::CaptureCurrentState(
    const FString& Name,
    const FSFRestoreCaptureFlags& CaptureFlags) const
{
    FSFRestorePreset Preset;
    Preset.Name = Name;
    Preset.CaptureFlags = CaptureFlags;
    Preset.Version = 1;
    Preset.CreatedAt = FDateTime::UtcNow().ToIso8601();
    Preset.UpdatedAt = Preset.CreatedAt;

    // Building class — always captured
    // Get from current hologram's recipe producer class
    if (USFRecipeManagementService* RecipeSvc = Subsystem->GetRecipeManagementService())
    {
        TSubclassOf<UFGRecipe> CurrentRecipe = Subsystem->GetActiveRecipe();
        if (CurrentRecipe)
        {
            // Extract the building class from the recipe's producers
            TArray<TSubclassOf<UObject>> Products;
            // Use recipe's building descriptor or the hologram's buildable class
            Preset.BuildingClassPath = GetPathNameSafe(CurrentRecipe);
        }
    }

    // Grid, spacing, steps, stagger, rotation — from CounterState
    const FSFCounterState& State = Subsystem->GetCounterState();

    if (CaptureFlags.bGrid)
        Preset.GridCounters = State.GridCounters;

    if (CaptureFlags.bSpacing)
    {
        Preset.SpacingX = State.SpacingX;
        Preset.SpacingY = State.SpacingY;
        Preset.SpacingZ = State.SpacingZ;
    }

    if (CaptureFlags.bSteps)
    {
        Preset.StepsX = State.StepsX;
        Preset.StepsY = State.StepsY;
    }

    if (CaptureFlags.bStagger)
    {
        Preset.StaggerX  = State.StaggerX;
        Preset.StaggerY  = State.StaggerY;
        Preset.StaggerZX = State.StaggerZX;
        Preset.StaggerZY = State.StaggerZY;
    }

    if (CaptureFlags.bRotation)
        Preset.RotationZ = State.RotationZ;

    // Recipe
    if (CaptureFlags.bRecipe)
    {
        TSubclassOf<UFGRecipe> ActiveRecipe = Subsystem->GetActiveRecipe();
        if (ActiveRecipe)
            Preset.RecipeClassPath = GetPathNameSafe(ActiveRecipe);
    }

    // Auto-connect
    if (CaptureFlags.bAutoConnect)
    {
        const auto& AC = Subsystem->GetAutoConnectRuntimeSettings();
        Preset.AutoConnect.bBeltEnabled        = AC.bEnabled;
        Preset.AutoConnect.BeltTierMain        = AC.BeltTierMain;
        Preset.AutoConnect.BeltTierToBuilding  = AC.BeltTierToBuilding;
        Preset.AutoConnect.bChainDistributors  = AC.bChainDistributors;
        Preset.AutoConnect.BeltRoutingMode     = AC.BeltRoutingMode;
        Preset.AutoConnect.bPipeEnabled        = AC.bPipeAutoConnectEnabled;
        Preset.AutoConnect.PipeTierMain        = AC.PipeTierMain;
        Preset.AutoConnect.PipeTierToBuilding  = AC.PipeTierToBuilding;
        Preset.AutoConnect.bPipeIndicator      = AC.bPipeIndicator;
        Preset.AutoConnect.PipeRoutingMode     = AC.PipeRoutingMode;
        Preset.AutoConnect.bPowerEnabled       = AC.bConnectPower;
        Preset.AutoConnect.PowerGridAxis       = AC.PowerGridAxis;
        Preset.AutoConnect.PowerReserved       = AC.PowerReserved;
    }

    return Preset;
}
```

#### ApplyPreset

Writes to:
- `USFSubsystem::UpdateCounterState()` → apply grid, spacing, steps, stagger, rotation to `FSFCounterState`
- `USFRecipeManagementService::SetActiveRecipeByIndex()` → set recipe (after validating it exists in the filtered list)
- Auto-connect runtime settings → `FAutoConnectRuntimeSettings` fields on the Subsystem
- Build gun switch → requires setting the active recipe on the build gun, which triggers hologram creation. The Subsystem's `OnBuildGunRecipeSampled()` or an equivalent path can be used.

```cpp
bool USFRestoreService::ApplyPreset(const FSFRestorePreset& Preset)
{
    // 1. Switch build gun to the preset's building class
    //    Find the recipe that produces this building class
    //    Validate it is unlocked in current game state
    //    If not unlocked, warn and return false

    // 2. Apply counter state fields (only those with capture flag set)
    FSFCounterState State = Subsystem->GetCounterState();

    if (Preset.CaptureFlags.bGrid)
        State.GridCounters = Preset.GridCounters;

    if (Preset.CaptureFlags.bSpacing)
    {
        State.SpacingX = Preset.SpacingX;
        State.SpacingY = Preset.SpacingY;
        State.SpacingZ = Preset.SpacingZ;
    }

    if (Preset.CaptureFlags.bSteps)
    {
        State.StepsX = Preset.StepsX;
        State.StepsY = Preset.StepsY;
    }

    if (Preset.CaptureFlags.bStagger)
    {
        State.StaggerX  = Preset.StaggerX;
        State.StaggerY  = Preset.StaggerY;
        State.StaggerZX = Preset.StaggerZX;
        State.StaggerZY = Preset.StaggerZY;
    }

    if (Preset.CaptureFlags.bRotation)
        State.RotationZ = Preset.RotationZ;

    Subsystem->UpdateCounterState(State);

    // 3. Apply recipe (validate first)
    if (Preset.CaptureFlags.bRecipe && !Preset.RecipeClassPath.IsEmpty())
    {
        // Find recipe in filtered list by class path
        // If found, call SetActiveRecipeByIndex()
        // If not found, log warning and skip
    }

    // 4. Apply auto-connect settings
    if (Preset.CaptureFlags.bAutoConnect)
    {
        // Write directly to AutoConnectRuntimeSettings on Subsystem
        // This requires a new setter or friend access
    }

    // 5. Refresh hologram
    // Trigger the same refresh path used by the Settings Form's Apply button

    return true;
}
```

**Build gun switching** is the trickiest part. The vanilla build gun uses recipes to determine what to build — when you select "Constructor" in the build menu, you're selecting a recipe whose product is a Constructor. To switch the build gun:

1. Find the recipe whose produced building matches `BuildingClassPath`.
2. Verify the recipe is unlocked via `AFGRecipeManager`.
3. Set it as the active recipe on the build gun state via the same path the build menu uses.

This will likely require a new helper on `USFSubsystem` — something like `SetBuildGunToRecipe(TSubclassOf<UFGRecipe>)` — that replicates what the vanilla build menu does when you click a building.

## Phase 3: JSON Serialization

### Serialization Pattern

Follow the `ManifoldJSONHelpers` pattern from `SFManifoldJSON.cpp`:
- Manual `TSharedPtr<FJsonObject>` construction (not `FJsonObjectConverter`)
- Explicit field-by-field serialization for version control
- Only serialize fields whose capture flag is true

```cpp
TSharedPtr<FJsonObject> USFRestoreService::PresetToJson(const FSFRestorePreset& Preset) const
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("name"), Preset.Name);
    Root->SetStringField(TEXT("buildingClassPath"), Preset.BuildingClassPath);
    Root->SetNumberField(TEXT("version"), Preset.Version);
    Root->SetStringField(TEXT("createdAt"), Preset.CreatedAt);
    Root->SetStringField(TEXT("updatedAt"), Preset.UpdatedAt);

    // Capture flags
    TSharedPtr<FJsonObject> Flags = MakeShared<FJsonObject>();
    Flags->SetBoolField(TEXT("grid"),       Preset.CaptureFlags.bGrid);
    Flags->SetBoolField(TEXT("spacing"),    Preset.CaptureFlags.bSpacing);
    Flags->SetBoolField(TEXT("steps"),      Preset.CaptureFlags.bSteps);
    Flags->SetBoolField(TEXT("stagger"),    Preset.CaptureFlags.bStagger);
    Flags->SetBoolField(TEXT("rotation"),   Preset.CaptureFlags.bRotation);
    Flags->SetBoolField(TEXT("recipe"),     Preset.CaptureFlags.bRecipe);
    Flags->SetBoolField(TEXT("autoConnect"),Preset.CaptureFlags.bAutoConnect);
    Root->SetObjectField(TEXT("captureFlags"), Flags);

    // Conditional field groups
    if (Preset.CaptureFlags.bGrid)
    {
        TSharedPtr<FJsonObject> Grid = MakeShared<FJsonObject>();
        Grid->SetNumberField(TEXT("x"), Preset.GridCounters.X);
        Grid->SetNumberField(TEXT("y"), Preset.GridCounters.Y);
        Grid->SetNumberField(TEXT("z"), Preset.GridCounters.Z);
        Root->SetObjectField(TEXT("grid"), Grid);
    }

    // ... spacing, steps, stagger, rotation, recipe, autoConnect follow same pattern

    return Root;
}
```

### Storage Location

```text
{GameUserDir}/SmartFoundations/Presets/
```

Accessed via `FPaths::ProjectUserDir() / TEXT("SmartFoundations/Presets/")`. Each preset is a separate `.json` file named after the sanitized preset name. This is outside the SaveGame system so presets are globally available across all save files.

## Phase 4: Build Gun Switching

This is the highest-risk integration point. The build gun's building selection flows through the vanilla recipe system:

1. Player selects a building category in the build menu.
2. Build menu resolves the building's recipe.
3. Recipe is set on `UFGBuildGunStateBuild`.
4. Build gun state creates the appropriate hologram.

For Smart Restore to auto-switch the build gun:

### Option A: Recipe-Based Switch (Recommended)

```cpp
bool USFRestoreService::SwitchBuildGunToBuilding(const FString& BuildingClassPath)
{
    // 1. Find all recipes that produce this building
    AFGRecipeManager* RecipeMgr = AFGRecipeManager::Get(Subsystem->GetWorld());
    TArray<TSubclassOf<UFGRecipe>> AllRecipes;
    RecipeMgr->GetAllAvailableRecipes(AllRecipes);

    TSubclassOf<UFGRecipe> TargetRecipe = nullptr;
    for (auto& Recipe : AllRecipes)
    {
        // Check if this recipe's product matches our target building
        TArray<FItemAmount> Products = UFGRecipe::GetProducts(Recipe);
        for (const FItemAmount& Product : Products)
        {
            if (GetPathNameSafe(Product.ItemClass) == BuildingClassPath)
            {
                TargetRecipe = Recipe;
                break;
            }
        }
        if (TargetRecipe) break;
    }

    if (!TargetRecipe)
    {
        UE_LOG(LogSmartFoundations, Warning,
            TEXT("[SmartRestore] Building class %s not found in available recipes"),
            *BuildingClassPath);
        return false;
    }

    // 2. Set the recipe on the build gun
    // Use the build gun state's SetActiveRecipe or equivalent
    AFGBuildGun* BuildGun = /* get from player controller */;
    if (UFGBuildGunStateBuild* BuildState = BuildGun->GetBuildState())
    {
        BuildState->SetActiveRecipe(TargetRecipe);
    }

    return true;
}
```

### Subsystem Integration Point

`USFSubsystem` will need a new method exposed for the Restore service:

```cpp
// In SFSubsystem.h — new public method
/** Set build gun to a specific recipe (for Smart Restore preset apply) */
bool SetBuildGunRecipe(TSubclassOf<UFGRecipe> Recipe);
```

This wraps the vanilla build gun interaction and handles Smart!'s hologram replacement pipeline (`ReplaceHologramInBuildGun`).

## Phase 5: Extend Integration — "Use Last Extend Target"

### How Extend Topology is Cached

`USFExtendService` maintains a cached topology via:
- `WalkTopology(AFGBuildable*)` → populates internal `FSFExtendTopology`
- `GetCurrentTopology()` → returns const ref to cached topology
- `ClearTopology()` → resets cached data

The topology persists until the next `WalkTopology` call or explicit clear. This means the last Extend operation's source building is available until the player does something else.

### Import Flow

```cpp
TOptional<FSFRestorePreset> USFRestoreService::ImportFromLastExtend(
    const FString& Name,
    const FSFRestoreCaptureFlags& CaptureFlags) const
{
    USFExtendService* ExtendSvc = Subsystem->GetExtendService();
    if (!ExtendSvc) return {};

    const FSFExtendTopology& Topology = ExtendSvc->GetCurrentTopology();
    if (!Topology.bIsValid || !Topology.SourceBuilding.IsValid())
        return {};

    // Start with a normal capture of current state
    FSFRestorePreset Preset = CaptureCurrentState(Name, CaptureFlags);

    // Override BuildingClassPath with the source building's class
    AFGBuildable* SourceBuilding = Topology.SourceBuilding.Get();
    Preset.BuildingClassPath = GetPathNameSafe(SourceBuilding->GetClass());

    // If recipe capture is enabled, try to get the source factory's recipe
    if (CaptureFlags.bRecipe)
    {
        if (AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(SourceBuilding))
        {
            TSubclassOf<UFGRecipe> FactoryRecipe = Factory->GetCurrentRecipe();
            if (FactoryRecipe)
                Preset.RecipeClassPath = GetPathNameSafe(FactoryRecipe);
        }
    }

    return Preset;
}
```

### UI Button

The Smart Panel will show an "Import from Last Extend" button (or similar wording) that is only enabled when `USFExtendService::GetCurrentTopology().bIsValid` is true. Clicking it opens the same capture checklist dialog but pre-populates `BuildingClassPath` and recipe from the Extend source.

## Phase 6: Compact Sharing Format

### Export

```cpp
FString USFRestoreService::ExportToString(const FSFRestorePreset& Preset) const
{
    TSharedPtr<FJsonObject> JsonObj = PresetToJson(Preset);

    // Remove metadata fields (not needed for sharing)
    JsonObj->RemoveField(TEXT("createdAt"));
    JsonObj->RemoveField(TEXT("updatedAt"));

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

    // Base64 encode
    FString Encoded = FBase64::Encode(JsonString);

    return FString::Printf(TEXT("SR1:%s"), *Encoded);
}
```

### Import

```cpp
TOptional<FSFRestorePreset> USFRestoreService::ImportFromString(const FString& Encoded) const
{
    // Check prefix
    if (!Encoded.StartsWith(TEXT("SR1:")))
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("[SmartRestore] Unknown format prefix"));
        return {};
    }

    // Strip prefix, decode Base64
    FString Base64Part = Encoded.Mid(4);
    FString JsonString;
    if (!FBase64::Decode(Base64Part, JsonString))
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("[SmartRestore] Base64 decode failed"));
        return {};
    }

    // Parse JSON
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("[SmartRestore] JSON parse failed"));
        return {};
    }

    return JsonToPreset(JsonObj);
}
```

## New Files Summary

| File | Type | Purpose |
|------|------|---------|
| `Source/SmartFoundations/Public/Features/Restore/SFRestoreTypes.h` | Header | `FSFRestorePreset`, `FSFRestoreCaptureFlags`, `FSFRestoreAutoConnectState` structs |
| `Source/SmartFoundations/Public/Features/Restore/SFRestoreService.h` | Header | `USFRestoreService` class declaration |
| `Source/SmartFoundations/Private/Features/Restore/SFRestoreService.cpp` | Implementation | Capture, apply, JSON serialization, export/import, Extend integration |

## Modified Files

| File | Change |
|------|--------|
| `SFSubsystem.h` | Add `USFRestoreService*` member, `GetRestoreService()` accessor, `SetBuildGunRecipe()` method |
| `SFSubsystem.cpp` | Create and initialize `USFRestoreService` in subsystem init; implement `SetBuildGunRecipe()` |
| `SmartFoundations.Build.cs` | Add `Restore` module path if needed (likely not — same module) |
| `SmartSettingsFormWidget.h/.cpp` | (Phase 2 — UI wiring) Add preset dropdown, save/apply/delete buttons, capture checklist dialog |

## Integration with Existing Services

| Existing Service | How Restore Uses It |
|-----------------|---------------------|
| `USFGridStateService` | Read `CounterState` for capture; write `CounterState` for restore via `UpdateCounterState()` |
| `USFRecipeManagementService` | Read `GetActiveRecipe()` for capture; find and set recipe by class path for restore |
| `USFExtendService` | Read `GetCurrentTopology()` for "Import from Last Extend" |
| `FAutoConnectRuntimeSettings` | Read for capture; write for restore (may need a new setter on Subsystem) |
| `ManifoldJSONHelpers` | Pattern reference for JSON serialization style |

## Subsystem Setter Requirements

The Restore service needs to write to state that is currently read-only or only writable through UI event handlers. New setters needed on `USFSubsystem`:

1. **`SetBuildGunRecipe(TSubclassOf<UFGRecipe>)`** — Switches the build gun to build a specific building. Wraps vanilla build gun state interaction.
2. **`SetAutoConnectRuntimeSettings(const FAutoConnectRuntimeSettings&)`** — Overwrites the current auto-connect runtime settings from a preset. The existing `ResetAutoConnectRuntimeSettings()` resets to config defaults, but we need a full setter.
3. **`SetActiveRecipeByClassPath(const FString&)`** — Convenience wrapper that finds a recipe by class path in the filtered list and calls `SetActiveRecipeByIndex()`. Alternative: the Restore service does this lookup itself.

## Implementation Order

1. **`SFRestoreTypes.h`** — Data model structs (no dependencies, can compile immediately)
2. **`SFRestoreService.h/.cpp`** — Service skeleton with `CaptureCurrentState()` and JSON serialization
3. **Subsystem integration** — Add service to `USFSubsystem`, add required setters
4. **`ApplyPreset()`** — Implement restore logic including build gun switch
5. **JSON file I/O** — `SavePreset()`, `LoadAllPresets()`, `DeletePreset()`
6. **Export/Import** — `ExportToString()`, `ImportFromString()`
7. **`ImportFromLastExtend()`** — Extend topology integration
8. **Unit validation** — Test capture → serialize → deserialize → apply roundtrip
9. **UI wiring** — Smart Panel preset controls (separate PR recommended)

## Risk Areas

| Risk | Mitigation |
|------|------------|
| Build gun recipe switching may have side effects (hologram lifecycle, cost recalculation) | Test with vanilla build gun state first; mirror what build menu does |
| Class paths may change between Satisfactory updates | Version field enables migration; validate on load |
| Auto-connect settings write-back may conflict with config system | Use runtime settings (same path as UI controls), not config write |
| Large preset files from many presets | One file per preset, not a single monolith; lazy loading |
| Extend topology may be cleared before user saves | Cache topology reference at service level when Extend completes; offer import promptly in UI |
