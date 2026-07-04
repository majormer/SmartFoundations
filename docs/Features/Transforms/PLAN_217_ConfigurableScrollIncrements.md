---
title: "#217 — Configurable Scroll-Wheel Increments (Spec)"
type: PLAN
date: 2026-07-04
status: Spec (decisions locked with maintainer; implementation not started)
category: Features
tags: [transforms, scroll, increments, config, smart-panel, smart-config]
issue: 217
related: [SFGridStateService.h, SmartFoundationsModConfiguration.cpp, SFSubsystem_Config.cpp]
---

# #217 — Configurable Scroll-Wheel Increments

Let the player set how much each scroll-wheel notch changes the grid transforms, instead of the hardcoded 50 cm / 5°.

## 0. Implementation status (2026-07-04)

**PHASE 1 — LANDED (code, both targets, deployed):** the whole runtime is in and reads config.
- Flat config fields `SpacingIncrement / StepsIncrement / StaggerIncrement / RotationIncrement` (`Smart_ConfigStruct.h`, defaults 0.5 / 0.5 / 0.5 / 5.0) — **not yet section-filled**, so they read as defaults (= previous grid behavior), same mechanism as `AutoConnectMode`.
- `FSFScrollIncrements` (`SFGridStateService.h`); `AdjustSpacing/Steps/Stagger/Rotation` + `DispatchValueAdjust` take the increment (hardcoded `constexpr` removed).
- `USFSubsystem::RefreshScrollIncrements()` resolves config → quantize/floor/clamp → `CachedScrollIncrements`, called after both `CachedConfig` refreshes.
- Wired: the 3 grid `DispatchValueAdjust` sites + the Walk path (`RouteWalkValueAdjust`). **Walk now uses the shared 0.5 m / 5° (was 1 m / 15°).**
- Visible effect now: grid/extend/restore unchanged (still 0.5 m / 5°); **Walk is finer (0.5 m / 5°)**. Not menu-editable yet — that's Phase 2.

**PHASE 2 — DONE (2026-07-04; maintainer opted to do it now, not batch).** Section named **"Scaling Settings"** (key `ScalingSettings`), inserted after `PowerAutoConnect`:
1. `FSmart_ScalingSettingsConfigSection` + field on `FSmart_ConfigStruct_Sections` + `GetActiveConfig` copy-down into the flat struct — done.
2. C++ archetype section (4 × `CreateFloatProperty`) in `SmartFoundationsModConfiguration.cpp`; `CreateFloatProperty` now sets `DefaultValue` (fixed the latent reset-to-default bug; also helps HUDScale etc.).
3. `Smart_Config` Blueprint: 4 renderable `BP_ConfigPropertyFloat_C` in a new `BP_ConfigPropertySection_C` — edited via the editor's Python scripting API (clone live classes, copy `WidgetType`, **NO `compile_blueprint`**, `save_loaded_asset(only_if_is_dirty=False)`). **Widget = `CPF_SPINBOX`** (maintainer: spinboxes, not sliders); `MinValue/MaxValue` = 0.1–8 (distance) / 0.5–90 (rotation); **`bRequiresWorldReload=False`** on the section + all four; `bAllowUserReset=True`. Integrity verified: source 27.8 KB → **30.7 KB** (no collapse), all 8 sections non-null. Backup at `Content/SmartFoundations/Config/Smart_Config.uasset.bak_217_*`.

**DEPLOY:** a config-asset change must be cooked — deploy via the editor's **Alpakit "dev"** (rebuilds DLL + cooks the pak with a matching BuildId). Do NOT DLL-only rebuild + hand-copy (the BP's serialized section map won't surface, and a CLI cook + copy causes a BuildId mismatch). See [[smart-config-sections]].

## 1. Decisions (locked with maintainer 2026-07-04)

- **Per-transform increments, split all the way out.** Four independent settings — Spacing, Steps, Stagger, Rotation — each with its own increment. (Not one global value; not per-axis. A builder may want Steps at 4 m, Stagger at 1 m, Spacing at 2 m.)
- **Rotation is its own unit** (degrees), separate from the distance settings by necessity.
- **Defaults = current behavior.** Spacing/Steps/Stagger default **0.5 m** (the current 50 cm); Rotation defaults **5.0°**. (The 4 m/1 m/2 m example above is illustrative, NOT a default.)
- **Ranges:** distance **0.1 m – 8 m** (8 m = one foundation); rotation **0.5° – 90°** (90° allows a one-notch quarter-turn).
- **Grid count (Scale X/Y/Z) is OUT OF SCOPE** — stays at ±1 building/notch, not configurable. Rationale: the count starts at 1, so a configurable step >1 would only ever produce odd counts (1 → 3 → 5 …); it isn't a clean thing to expose. Leave it.
- The issue text claims the current increment is 100 cm; **it is actually 50 cm** (`SFGridStateService.h`). Seed defaults from 50 cm to preserve behavior.

## 1a. Which modes these increments cover (verified in code)

The scroll transform-modals (Spacing / Steps / Stagger / Rotation) funnel through **one** path for the grid, and Extend + Restore ride it; Walk has its own.

- **Grid** — `DispatchValueAdjust` → `AdjustSpacing/Steps/Stagger/Rotation` in `SFGridStateService.h`, operating on `CounterState`. ← the four settings here.
- **Extend** — **shares the grid path.** There is no Extend interception before `DispatchValueAdjust` (only Walk intercepts, `SFSubsystem.cpp:1877`); Extend reads the same `CounterState`, so adjusting spacing/rotation during an Extend uses these same increments automatically.
- **Restore** — **shares the grid path.** Loading a preset writes values into `CounterState`; any subsequent scroll-adjust goes through the same `DispatchValueAdjust`. Nothing extra to wire.
- **Walk — SEPARATE, NOT covered by these four.** `RouteWalkValueAdjust` (`SFSubsystem.cpp:1378`) intercepts the *same modal keys* before the grid dispatcher and applies them to the active **walk segment** with its **own hardcoded, different** increments: Spacing→advance **1 m**, Rotation→turn **15°**, Steps→rise **1 m**, Stagger→shift **1 m** (`SFSubsystem.cpp:1390-1395`; the code already notes "Increments are tunable"). These are genuinely different quantities (segment advance/turn/rise/shift, not grid spacing/steps/stagger), and their defaults differ from the grid.

**DECISION (locked 2026-07-04): Walk SHARES the same four settings — aligned on the grid values and defaults.**
Walk was designed to map 1:1 to the grid transforms (Advance↔Spacing, Rise↔Steps, Shift↔Stagger, Turn↔Rotation), same modifier keys, same units. So there is **one** set of four config keys driving both Grid/Extend/Restore **and** Walk. Consequence: Walk adopts the grid defaults (**0.5 m / 5°**), replacing its current hardcoded 1 m / 15° — accepted (finer, consistent control; fast-scroll accumulation covers large advances/turns). No second set of keys; the `Smart_Config` surface stays at 4.
- **Walk wiring (§4d):** in `RouteWalkValueAdjust` (`SFSubsystem.cpp:1390-1395`) swap the hardcoded `Steps * 100.0f` (advance/rise/shift) and `Steps * 15.0f` (turn) for the shared cached increments — `dAdvance = Steps * SpacingCm`, `dRise = Steps * StepsCm`, `dShift = Steps * StaggerCm`, `dTurn = Steps * RotationDeg`. (`Steps` there already folds AccumulatedSteps × Direction, same shape as the grid path.)

## 1b. Reference: AirBuild's `ReachStep` (sibling mod, validated pattern)

AirBuild (`Mods/GameFeatures/AirBuild`) already ships this exact idea for its air-place reach, and its approach confirms + refines this spec:
- `ReachStep` is a **float config property in meters** (default 0.5, "how far each mouse-wheel notch moves the building") — identical `CreateFloatProperty` helper as Smart. In the Mods menu it renders as the **spinbox** the maintainer remembered. No min/max on the C++ property (matches our finding — bounds are BP-slider + read-clamp).
- Its **read path is worth copying** (`AirBuildSubsystem.cpp:220-243`, `ReadConfig`):
  - **Quantize** the config float to a clean granularity: `RoundToFloat(C.ReachStep * 10) / 10` → 0.1 m steps. (Adopt for the distance settings; quantize rotation to e.g. 0.5°.)
  - **Floor** so the step is never zero: `FMath::Max(1.f, Quantized * 100.f)` cm.
  - **Cache the converted value in a member at config-read time**, not per-scroll — refreshed on config change. Cleaner than reading `CachedConfig` on every notch (supersedes §4c's "read per scroll").
  - Applies with a value clamp per notch (`ReachCm = Clamp(ReachCm + Dir*ReachStepCm*Notches, Min, Max)`).

**Adopt into this spec:** quantize (0.1 m / 0.5°) + floor (never zero) + cache the converted increments at config-read time. Keep the read-time range clamp (§4c) as the hand-edited-file guard.

## 2. The four settings

| Setting | Config key | Unit | Range | Default | Applies to |
|---|---|---|---|---|---|
| Spacing increment | `SpacingIncrement` | meters | 0.1 – 8.0 | **0.5** | SpacingX/Y/Z |
| Steps increment | `StepsIncrement` | meters | 0.1 – 8.0 | **0.5** | StepsX/Y |
| Stagger increment | `StaggerIncrement` | meters | 0.1 – 8.0 | **0.5** | StaggerX/Y/ZX/ZY |
| Rotation increment | `RotationIncrement` | degrees | 0.5 – 90.0 | **5.0** | RotationZ (yaw) |

The distance settings are user-facing in **meters** (the Smart Panel already displays spacing in meters). Internally Spacing/Steps/Stagger are integer **cm** (`State.SpacingX` is `int32`), so the read path converts m→cm and rounds to int (0.1 m → 10 cm, 2.0 m → 200 cm — all clean integers). Rotation is float degrees end to end.

## 3. Where the increments live today

All in `SFGridStateService.h`, one hardcoded constant per Adjust function:

```cpp
void AdjustSpacing (…)  { constexpr int32 INCREMENT = 50;  … State.SpacingX += Direction*INCREMENT*AccumulatedSteps; … }
void AdjustSteps   (…)  { constexpr int32 INCREMENT = 50;  … }
void AdjustStagger (…)  { constexpr int32 INCREMENT = 50;  … }
void AdjustRotation(…)  { constexpr float INCREMENT = 5.0f; … State.RotationZ += Direction*INCREMENT*AccumulatedSteps; }
```

These are routed by `DispatchValueAdjust(...)` in the same header, called from the subsystem input path.

## 4. Implementation

### 4a. Config archetype (C++) — `SmartFoundationsModConfiguration.cpp`

Add a new section mirroring the existing ones (Belt/Pipe/Power/HUD/Arrows). `CreateFloatProperty` already exists (used by HUDScale etc.):

```cpp
// ── Scroll Increments ──
UConfigPropertySection* Inc = CreateSection(TEXT("ScrollIncrements"), LOCTEXT("Sec.Inc", "Scroll Increments"),
    LOCTEXT("Sec.Inc.TT", "How much each mouse-wheel notch changes the grid transforms."));
Inc->SectionProperties.Add(TEXT("SpacingIncrement"),  CreateFloatProperty(TEXT("SpacingIncrement"),  LOCTEXT("P.SpacingIncrement","Spacing Increment (m)"),
    LOCTEXT("P.SpacingIncrement.TT","Meters of spacing added per scroll notch (0.1–8)."), 0.5f));
Inc->SectionProperties.Add(TEXT("StepsIncrement"),    CreateFloatProperty(TEXT("StepsIncrement"),    LOCTEXT("P.StepsIncrement","Steps Increment (m)"),
    LOCTEXT("P.StepsIncrement.TT","Meters of stepping added per scroll notch (0.1–8)."), 0.5f));
Inc->SectionProperties.Add(TEXT("StaggerIncrement"),  CreateFloatProperty(TEXT("StaggerIncrement"),  LOCTEXT("P.StaggerIncrement","Stagger Increment (m)"),
    LOCTEXT("P.StaggerIncrement.TT","Meters of stagger added per scroll notch (0.1–8)."), 0.5f));
Inc->SectionProperties.Add(TEXT("RotationIncrement"), CreateFloatProperty(TEXT("RotationIncrement"), LOCTEXT("P.RotationIncrement","Rotation Increment (°)"),
    LOCTEXT("P.RotationIncrement.TT","Degrees of rotation added per scroll notch (0.5–90)."), 5.0f));
RootSection->SectionProperties.Add(TEXT("ScrollIncrements"), Inc);
```

Two details:
- **Set `DefaultValue` on the float props.** The existing `CreateFloatProperty` helper (Smart's *and* AirBuild's) only sets `Value`, not `DefaultValue` — so the Mods-menu "reset to default" would reset a float to 0, not the intended default. Fix `CreateFloatProperty` to also set `Property->DefaultValue = Value;` (the integer helper already does). Low-risk, benefits the existing float settings too (HUDScale etc.).
- **Tooltips mention both modes**, since these now drive Grid *and* Walk — e.g. "Meters added per scroll notch for grid spacing (and walk segment advance)."

### 4b. Config Blueprint override (Smart_Config) — **the hazardous part**

The keys above MUST be mirrored in the `Smart_Config` Blueprint override with the renderable `BP_ConfigProperty_Float` classes, in a matching `ScrollIncrements` section, or the Mods-menu UI won't render them. This is the risky asset (see [[smart-config-sections]] memory):

- **NEVER `compile_blueprint` on `Smart_Config`** (nulls the section sub-objects).
- Follow the section-header recipe from [[smart-config-sections]].
- The base `UConfigPropertyFloat` has **no MinValue/MaxValue** (verified in SML `ConfigPropertyFloat.h` — only `DefaultValue`/`Value`); the **slider bounds live on the renderable BP float property**, so set the 0.1–8 / 0.5–90 ranges there. C++ read-time clamp (below) is the authoritative backstop for hand-edited config files.
- Watch the 25 KB src / 15.6 KB cooked integrity gates from [[smart-config-sections]].

### 4c. Read path — cache the converted increments (AirBuild pattern)

The generated `FSmart_ConfigStruct` gains `SpacingIncrement` / `StepsIncrement` / `StaggerIncrement` / `RotationIncrement` fields automatically; read via `CachedConfig` (loaded by `FSmart_ConfigStruct::GetActiveConfig` in `SFSubsystem_Config.cpp:241`). Following AirBuild's `ReadConfig`, **quantize + floor + clamp + cache** the converted values into a subsystem member once at config-read time (not per scroll):

```cpp
struct FSFScrollIncrements   // cached member on the subsystem, refreshed in the config-read fn
{
    int32 SpacingCm  = 50;
    int32 StepsCm    = 50;
    int32 StaggerCm  = 50;
    float RotationDeg = 5.0f;
};

auto DistCm = [](float Meters)   // quantize to 0.1 m, clamp 0.1–8 m, →cm, never 0
{
    const float Q = FMath::RoundToFloat(FMath::Clamp(Meters, 0.1f, 8.0f) * 10.f) / 10.f;
    return FMath::Max(1, FMath::RoundToInt(Q * 100.0f));
};
CachedIncrements.SpacingCm   = DistCm(CachedConfig.SpacingIncrement);
CachedIncrements.StepsCm     = DistCm(CachedConfig.StepsIncrement);
CachedIncrements.StaggerCm   = DistCm(CachedConfig.StaggerIncrement);
CachedIncrements.RotationDeg = FMath::Max(0.5f,  // quantize to 0.5°, clamp 0.5–90°
    FMath::RoundToFloat(FMath::Clamp(CachedConfig.RotationIncrement, 0.5f, 90.0f) * 2.f) / 2.f);
```

Refresh `CachedIncrements` wherever `CachedConfig` is refreshed (config load + live change), so both consumers below just read the cached struct.

### 4d. Wire into both consumers

Keep `USFGridStateService` config-agnostic — pass the increment in rather than reading config from the header. Replace each `constexpr INCREMENT` with a parameter:

- `AdjustSpacing(State, Axis, AccumulatedSteps, Direction, int32 IncrementCm)`
- `AdjustSteps(…, int32 IncrementCm)`
- `AdjustStagger(…, int32 IncrementCm)`
- `AdjustRotation(…, float IncrementDeg)`
- `DispatchValueAdjust(…, const FSFScrollIncrements& Inc)` — forwards the right field to each branch.

**Two consumers, both read `CachedIncrements`:**
1. **Grid path** — the **three** `DispatchValueAdjust` call sites in `SFSubsystem.cpp` (~1882 / 1953 / 2021: value-increase, value-decrease, and the accumulated path). Pass `CachedIncrements` to each.
2. **Walk path** — `RouteWalkValueAdjust` (`SFSubsystem.cpp:1390-1395`): replace `Steps * 100.0f` / `Steps * 15.0f` with `Steps * CachedIncrements.SpacingCm` (advance), `* StepsCm` (rise), `* StaggerCm` (shift), `* RotationDeg` (turn).

### 4e. Localization

8 new LOCTEXT keys (4 display names + 4 tooltips) in the config .cpp, plus the section name/tooltip. English until the next loc re-gather.

## 5. Edge cases / guardrails

- **Grid count untouched** — do not route the grid-count step through this; it stays ±1 (§1 rationale).
- **Integer-cm rounding** — every in-range meters value maps to a clean integer cm; no drift.
- **Clamp is authoritative** — the BP slider bounds are UX only; the C++ clamp guards hand-edited `.cfg` files.
- **`AccumulatedSteps`** (scroll velocity) still multiplies the increment — unchanged; fast scrolling moves more, as today.
- **Config surface** — 4 new keys. If #427's config-persistence work also touches `Smart_Config`, batch the BP edits together to minimize passes on the hazardous asset (per [[next-feature-branch-scope]]).

## 6. Test plan

1. Fresh install (no config) → increments are 0.5 m / 0.5 m / 0.5 m / 5° (current behavior, regression check).
2. Set Spacing 2 m, Steps 4 m, Stagger 1 m, Rotation 15° → each scroll notch moves that transform by exactly the set amount; the three are independent.
3. Boundaries: 0.1 m and 8 m; 0.5° and 90° all apply correctly.
4. Hand-edit the `.cfg` beyond range (e.g. Spacing 999) → clamped to 8 m at read (no runaway).
5. Config change mid-session (Mods menu) → new increment takes effect on the next scroll without a reload.
5a. **Extend** — start an Extend, scroll spacing/rotation → uses the configured increments (shared `CounterState`).
5b. **Walk** — during a Smart Walk, the Spacing/Rotation/Steps/Stagger modifiers adjust segment advance/turn/rise/shift by the **same** configured increments (default 0.5 m / 5°, replacing the old 1 m / 15°).
6. MP: increments are a client-local input-feel setting (they change the client's counter deltas, which replicate as the resulting values) — confirm a client's configured increments drive its own scrolling; no server dependency expected.
