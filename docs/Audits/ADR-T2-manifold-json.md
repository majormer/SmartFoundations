# ADR — T2: `SFManifoldJSON.cpp` (3,694 lines)

Status: **Accepted and implemented** — Option A selected by maintainer and executed.
Charter: [`Simplification-GOAL.md`](Simplification-GOAL.md) · Tracker: [`SimplificationAudit.md`](SimplificationAudit.md)

## Context — the survey premise was WRONG

The charter/survey described T2 as: *"`SFManifoldJSON.cpp` (~99% hand-rolled JSON) → walk-once / `FArchive`."*
Reading the file disproves that:

- The structs (`FSFSourceTopology`, `FSFCloneTopology`, `FSFCloneHologram`, …) are `USTRUCT(BlueprintType)`
  with `UPROPERTY()` fields — already engine-serializable; nothing hand-rolled about them.
- **There is no JSON *deserialization* anywhere.** Nothing ever reads a manifold JSON back.
- The only JSON *writing* is two `SaveToFile()` methods (~470 lines total) that dump
  `ProjectLogDir/SmartExtend/*.json` **for debugging only** — confirmed by code comments
  ("the manifold built JSON is for debugging only") and a **"DUAL-PATH WARNING"** noting the
  debug dump re-runs a *second* capture that duplicates the live path.

The file is misnamed. It is really the **Extend clone-topology engine**, with a JSON debug-dump bolted on. Function breakdown (live vs debug):

| Lines | Function | Role | Live? |
|------:|----------|------|-------|
| ~59  | `FSFSourceTopology::SaveToFile` | JSON debug dump | **debug-only** |
| ~872 | `FSFCloneTopology::FromSource` | source→clone offset transform (in-memory) | LIVE |
| ~414 | `FSFCloneTopology::SaveToFile` | JSON debug dump | **debug-only** |
| ~121 | `FSFSourceTopology::CaptureFromTopology` | populate structs from topology | LIVE |
| ~449 | `FSFSourceTopology::CaptureFromBuiltFactory` | populate from built actors | debug-compare only (verify) |
| ~1280| `FSFCloneTopology::SpawnChildHolograms` | spawn child holograms | LIVE (load-bearing) |
| ~240 | `FSFCloneTopology::WireChildHologramConnections` | wire connections | LIVE |

So the real T2 is not a serialization rewrite. The over-size comes from a genuinely large in-memory
engine (`SpawnChildHolograms` 1,280; `FromSource` 872) plus removable debug JSON.

## Options

### Option A — Remove debug JSON, then rename + split (recommended, staged)
1. **Delete the JSON debug-dump path** (~470 lines of `SaveToFile` + their call sites in
   `SFExtendHologramService.cpp` 108–118 and `SFExtendService.cpp` ~7071/7683). Confirm
   `CaptureFromBuiltFactory` is used *only* by the built-debug dump; if so, delete it too (~449
   more). Net: ~900 lines gone with zero gameplay change (only debug files stop being written).
   Removes the `Json`/`JsonUtilities` dependency if nothing else uses it.
2. **Rename** `SFManifoldJSON.*` → `SFExtendCloneTopology.*` (it has no JSON left). Pure rename.
3. **Split** the remaining ~2,300-line engine: move `SpawnChildHolograms` (+ its hologram-spawn
   helpers) into `SFExtendCloneSpawner.cpp` and keep capture/transform in the topology file. Lands
   both under ~1,200 lines.
   - Each step build-validates; the debug-removal + rename are low-risk, the split is the careful part.
   - Smoke (all steps, since it's the Extend path): capture a manifold → Extend preview → build →
     confirm clones spawn + wire correctly; scaled Extend (2×); pump/power wiring case.

### Option B — Debug-removal only (smallest safe win)
Just step 1 above (~900 lines, no rename/split). Lowest risk; still leaves a ~2,800-line file but
removes the dead serialization the survey was worried about. Defer rename/split.

### Option C — Keep JSON dumps behind a cvar instead of deleting
If the JSON debug dumps are valued for field diagnostics, gate them behind an existing
`SF.Log`/debug cvar rather than deleting. Keeps the capability, removes the always-on cost. Smaller
line savings.

**Recommended: A**, executed as 3 separate build-validated commits (debug-removal → rename → split),
so each is independently revertable and the risky split is isolated. If you want minimal disruption,
**B** now and split later.

## Risks
- `SpawnChildHolograms`/`FromSource`/`WireChildHologramConnections` are **load-bearing** Extend
  logic — the split must be pure-move (verbatim), behavior verified by the Extend smoke.
- Confirm no non-debug caller of `SaveToFile`/`CaptureFromBuiltFactory` before deleting (grep done:
  only debug-dump call sites found; re-verify at edit time).
- The structs are `BlueprintType` — check no Blueprint/asset references the JSON structs by name
  before renaming the file (rename the file, keep struct names, to be safe).

## Decision
Option A.

Implemented changes:

- Removed the debug-only source/clone topology `SaveToFile()` methods and the built-factory comparison capture.
- Removed the always-written `ExtendSourceTopology.json`, `ExtendClonedTopology.json`, and `ManifoldBuilt.json` dump call sites.
- Renamed `SFManifoldJSON.*` to `SFExtendCloneTopology.*`.
- Split `FSFCloneTopology::SpawnChildHolograms` and its helpers into `SFExtendCloneSpawner.cpp`.
- Removed the unused `JsonUtilities` module dependency; `Json` remains because Restore and `SFWiringManifest` still use it.

Post-change shape:

| File | Role |
|------|------|
| `SFExtendCloneTopology.h` | Source/clone topology structs and public operations |
| `SFExtendCloneTopology.cpp` | Source capture, source-to-clone transform, preview-time connection wiring |
| `SFExtendCloneSpawner.cpp` | Child hologram spawning from clone topology entries |

Build/package validation remains maintainer-gated per project rules; required smoke is Extend preview/build, Scaled Extend, and pump/power wiring.
