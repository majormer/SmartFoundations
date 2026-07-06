# #168 Scaleable Blueprints — Research / Homework

**Status:** Research complete, pre-design. Implementation/design to be led by Fable.
**Issue:** [#168 Scaleable blueprints](https://github.com/majormer/SmartFoundations/issues/168) (`critical`, `future-feature`, `scaling`, `blueprints`) — originally ShadedPL.
**Branch:** `feature/168-scaleable-blueprints`

> This document is homework: the vanilla API surface, the root cause, the Smart touchpoints, and the open questions — so design starts from facts, not spelunking. It deliberately stops short of prescribing the implementation.

---

## 1. The feature & the maintainer's constraints

Scaling a blueprint with Smart! today places **only the parent** — the blueprint's contents never appear. Goal: Smart Scaling produces a **grid of blueprint copies**, and the connections between them are handled by **the game's own blueprint auto-connect**, not by Smart's belt/pipe auto-connect.

Maintainer constraints (explicit):
- **Auto-connect is in scope** — it was a Smart 1.1 feature; 1.2 should be stable enough to leverage the game's tools.
- **Do NOT build Smart wiring unless we find a vanilla gap.** Lean on vanilla; only fill holes it leaves.
- **Spacing/boundary is the hard part.** Smart's spacing is defined per-hologram via the size registry; blueprints need a *different* boundary source.
- **Do not copy BlueprintZooper's solution.** Its C++ was used only to *identify* the vanilla APIs; its Blueprint orchestration is not to be reproduced.

---

## 2. Root cause — why only the parent places

Blueprints are **hard-excluded** from Smart, on purpose. In `Private/Subsystem/SFSubsystem_HologramLifecycle.cpp` (~line 1628), the adapter factory does:

```cpp
// CRITICAL: Detect vanilla blueprint holograms FIRST (Issue #166)
// Blueprint placement must not be scaled - it would break the blueprint system
if (Cast<AFGBlueprintHologram>(Hologram))
{
    return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("Blueprint"));
}
```

Smart wraps every held hologram in an **adapter** (`ISFHologramAdapter`) that tells the scaling system how to size/transform/expand it. Blueprints get `FSFUnsupportedAdapter` → no scaling at all. So #168 is not a bug in expansion; it's a deliberate gate. The work is to replace that gate with a real blueprint adapter + a blueprint expansion path. (Note the #166 reason it was added: naive scaling *broke* blueprint placement — so whatever replaces it must handle the blueprint lifecycle correctly, not just remove the guard.)

---

## 3. Vanilla capability map (verified present in the 1.2 headers)

Source of truth: `Mods/GameFeatures/SmartMCP/reference/FactoryGame/Public/Hologram/FGBlueprintHologram.h` and `FGBlueprintOpenConnectionManager.h`. (BlueprintZooper's C++ shim — `Almine2/Almine_SatisfactoryMods/BlueprintZooper` — is just a Blueprint-callable passthrough to these; it confirmed the surface, nothing more.)

### `AFGBlueprintHologram` (: `AFGFactoryHologram`)
| Member / method | Why it matters |
|---|---|
| `AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID)` | **The gap-closer.** Places the blueprint's buildings and returns them. Overrides the hologram Construct. |
| `FBox mLocalBounds` / `FVector mLocalBoundsOffset` (protected) | **The blueprint boundary.** Footprint used to tile copies edge-to-edge so vanilla connects them. Replaces the size registry for blueprints. |
| `void SetBlueprintDescriptor(UFGBlueprintDescriptor*)` | Point a hologram at a specific blueprint. |
| `void LoadBlueprintToOtherWorld()` | Stage the blueprint's buildables into the off-world blueprint world (vanilla preview mechanism). |
| `void SetHologramLocationAndRotation(const FHitResult&)` | Position a copy. |
| `void AlignBuildableRootWithBounds()` | Bounds-alignment helper. |
| `mBlueprintDescriptor` (UPROPERTY) | The descriptor currently loaded. |
| rotation/scroll: `GetRotationStep`, `Scroll`, `ApplyScrollRotationTo`, `UpdateRotationValuesFromTransform` | Per-copy rotation (zoop-style). |

### `AreAutomaticConnectionsEnabled()` + build modes (protected)
- `mBlueprintSnapBuildMode` — snap to blueprint proxies
- `mBlueprintAutoConnectBuildMode` — **automatic connections**
- `mBlueprintSnapAutoConnectBuildMode` — snap **and** auto-connect

These are the switch for the game's blueprint auto-connect. The build mode active on the hologram determines whether vanilla wires the boundaries.

---

## 4. The game's auto-connect IS real — `FGBlueprintOpenConnectionManager`

This is the most important finding: **the maintainer's vision is directly supported by vanilla.** `FGBlueprintOpenConnectionManager<ConnectionClass, BridgeHologramClass>` (belt / pipe / railroad variants) manages connecting a blueprint's **open-ended connectors** to buildables in the world, and it even **constructs bridge buildables** (the connecting belts/pipes) itself:

```
virtual void UpdateAutomaticConnections(const FHitResult&, bool& out_PlaySnapEffects) = 0;
virtual bool AttemptConnectionStateSnap() = 0;
virtual void Construct(TArray<AFGBuildable*>& out_ConstructedBridgeBuildables, FNetConstructionID) = 0;
```

`AFGBlueprintHologram` owns `TArray<TUniquePtr<FGBlueprintOpenConnectionManagerBase>> mOpenConnectionManagers` and calls into them during its own `Construct`. So — **hypothesis to verify (§8):** calling `blueprintHologram->Construct(out_children, netId)` with an auto-connect build mode active places the buildings *and* the bridge connections in one shot. If true, **Smart builds no wiring at all** — it places copies with the right build mode and vanilla does the rest. This is the "don't build Smart wiring unless there's a vanilla gap" path, and it looks achievable.

---

## 5. The boundary problem (the maintainer's flagged hard part)

Smart's per-hologram spacing comes from `USFBuildableSizeRegistry` (`Public/Data/SFBuildableSizeRegistry.h`): a profile table keyed by buildable class, with a clearance-box CDO fallback for modded buildables — `GetSizeForHologram(hologram)`. **Blueprints have no build class in that table and no single clearance box** — they're composites. So the size registry is the wrong source.

The right source is the hologram's own `mLocalBounds` / `mLocalBoundsOffset`. The adapter interface already has the seam for this:

```cpp
// ISFHologramAdapter.h
virtual FBoxSphereBounds GetBuildingBounds() const = 0;   // <-- blueprint adapter returns mLocalBounds
virtual FTransform GetBaseTransform() const = 0;
virtual void ApplyTransformOffset(const FVector& Offset) = 0;
virtual bool SupportsFeature(ESFFeature Feature) const = 0;
virtual TWeakObjectPtr<AFGHologram> GetHologram() const = 0;
virtual bool IsValid() const = 0;
```

So "another way to define a boundary" = a `FSFBlueprintAdapter` whose `GetBuildingBounds()` returns `mLocalBounds` (transformed), instead of delegating to the size registry. **Caveat:** `mLocalBounds`/`mLocalBoundsOffset` are protected → needs AccessTransformers.ini or reflection (Smart already uses reflection for vanilla members, e.g. `mSnappedConnectionComponents` in the pipe path — precedent exists). Also: bounds may be zero until the blueprint is loaded/staged (`LoadBlueprintToOtherWorld`), so timing matters.

---

## 6. Smart architecture touchpoints (the map for Fable)

| Concern | Where | Note |
|---|---|---|
| **Hook / gate** | `SFSubsystem_HologramLifecycle.cpp:~1628` | Replace `FSFUnsupportedAdapter` for `AFGBlueprintHologram` with a new blueprint adapter (guarded so it can't reintroduce the #166 break). |
| **Adapter interface** | `Public/Holograms/Adapters/ISFHologramAdapter.h` | New `FSFBlueprintAdapter : FSFHologramAdapterBase`; `GetBuildingBounds` ← `mLocalBounds`. |
| **Boundary/spacing** | `Public/Data/SFBuildableSizeRegistry.h` | Bypassed for blueprints; bounds come from the adapter. |
| **Grid expansion / child spawning** | `Private/Holograms/Core/SFScalingSpecExpansion.cpp` | Today it clones single-buildable holograms by recipe/build class. Needs a blueprint path: per cell, spawn a blueprint hologram copy (`SetBlueprintDescriptor` + `LoadBlueprintToOtherWorld`), position via bounds, `Construct(out_children)`. |
| **MP authority construction** | Smart's spec-construction model (`SFGameInstanceModule_SpecHooks.cpp`, the `sf.MP.SpecConstruction` path) | Blueprint `Construct` takes a `FNetConstructionID`; per-cell construction must route through the authority like other Smart children. See the multiplayer memory notes. |
| **Existing blueprint plumbing** | `SFScalingSpecExpansion.cpp` (`AFGBlueprintProxy`, `SetInsideBlueprintDesigner`, `GetBlueprintDesigner`) | This is the *reverse* direction (Smart-scaling buildings **inside** a designer). Useful reference for proxy/grouping APIs, not the same feature. |

---

## 7. Proposed shape (NOT a design — a starting frame for Fable)

1. `FSFBlueprintAdapter` — bounds from `mLocalBounds`, `SupportsFeature` gates which Smart features apply (likely X/Y/Z scale + spacing; probably not stagger/steps/arrows initially).
2. Blueprint expansion path in `SFScalingSpecExpansion` — per grid cell: stage + position + `Construct(out_children)`.
3. **Spacing default derived from bounds** so cells tile edge-to-edge → vanilla boundary auto-connect fires. Spacing being *exact* is what makes the auto-connect work; it is not a cosmetic nicety here.
4. Auto-connect: rely on `mBlueprintAutoConnectBuildMode` + `FGBlueprintOpenConnectionManager`. Build Smart wiring **only** if §8 shows a vanilla gap.
5. MP: route per-cell `Construct` through the authority spec-construction model.

---

## 8. Open questions & de-risking experiments (run BEFORE committing the design)

**The load-bearing unknowns — verify in-game first:**

1. **Does one `Construct` call place buildings *and* auto-connect bridges?** Hand-place two copies of a simple belt-carrying blueprint edge-to-edge with the auto-connect build mode on; confirm vanilla spawns the connecting belt across the seam. If yes → Smart builds no wiring. If no → scope a Smart bridge pass (the "vanilla gap").
2. **What triggers the auto-connect — proximity, the build mode, or the hit-result snap?** i.e. can we set the build mode + position programmatically and get connections, or does it require the interactive snap flow (`UpdateAutomaticConnections(hitResult, ...)`)? Determines how much of the interactive build-gun flow we must reproduce vs. can bypass.
3. **When are `mLocalBounds` valid?** Before or only after `LoadBlueprintToOtherWorld`? Governs adapter timing.
4. **Copy strategy:** clone the held `AFGBlueprintHologram`, or spawn fresh ones per cell via `SetBlueprintDescriptor` + stage? BlueprintZooper spawns/stages; cloning may be cheaper but riskier.
5. **MP:** does blueprint `Construct` + open-connection `Construct` replicate correctly through Smart's existing authority path, or does the open-connection manager need its own `SerializeConstructMessage` handling? (The manager has `SerializeConstructMessage`/`PostConstructMessageDeserialization` — MP-aware, but integration with Smart's model is unverified.)
6. **Cost / dismantle:** does each placed copy aggregate cost and register for Smart Dismantle like other Smart children, or does blueprint construction bypass Smart's cost/proxy grouping?

**Editor/AdaMCP note:** BlueprintZooper's *Blueprint* subsystem (its orchestration) was **not** inspected and does not need to be — the C++ shim exposed the full vanilla surface, and we are not copying their solution. Only load it in the editor if a specific unknown above needs their reference flow.

---

## 9. Explicitly out of scope / do-not

- Do **not** reproduce BlueprintZooper's Blueprint graph.
- Do **not** build Smart belt/pipe auto-connect for blueprints unless experiment §8.1/§8.2 proves a vanilla gap.
- Do **not** simply delete the #166 guard — whatever replaces it must not reintroduce the blueprint-placement break that guard was added for.
