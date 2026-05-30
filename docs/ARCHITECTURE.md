# SmartFoundations — Architecture Map

A 10-minute orientation for maintainers, contributors, and LLMs. Smart! is a Satisfactory
build-gun enhancement mod (UE5.3 + Satisfactory Mod Loader). This describes *how the code is
organized and how the pieces talk* — not every detail. It is a living doc; correct it when you
find drift. For the in-flight simplification effort see [`Audits/Simplification-GOAL.md`](Audits/Simplification-GOAL.md).

## Big picture

The build gun spawns **holograms** (placement previews). Smart! subclasses the vanilla
holograms to add grid scaling, auto-connect, and Extend, and centralizes shared state and
feature logic in a world **subsystem** that the holograms and UI talk to.

```
Player build gun
   │ spawns
   ▼
Smart hologram (ASFFactoryHologram / ASFConveyorBeltHologram / ASFPipelineHologram / ASFWireHologram …)
   │ uses ──────────────► USFSubsystem (facade: shared state + feature services)
   ▼                          ├── Extend          (Features/Extend/)
child holograms (grid)        ├── AutoConnect      (belts/pipes/power)
                              ├── Upgrade          (Features/Upgrade/)
Smart Panel / Upgrade Panel ──┤   Restore          (Features/Restore/)
(UI/) ────────────────────────┘   HUD, ChainActor, Recipe, size & hologram data registries
```

## Source layout (`Source/SmartFoundations/`)

UE split: `Public/` (headers other code includes) and `Private/` (implementations). By area:

- **`Subsystem/`** — `USFSubsystem` (the central facade/owner) plus low-level helpers (input handling, validation, grid spawning, hologram helpers). *This is the largest, most overloaded area — the #1 decomposition target (T1).*
- **`Features/<Feature>/`** — self-contained feature services, each owned by the subsystem:
  - `Extend/` — clone/extend factories: detection, topology walk, clone-topology transform, hologram preview, and post-build wiring. *Largest feature; T1 targets remain after T2 split.*
  - `AutoConnect/`, `PipeAutoConnect/`, `PowerAutoConnect/` — auto-route belts / pipes / power between placed buildings.
  - `Upgrade/` — Smart Upgrade (radius/network audit, traversal, execution, chain-actor repair).
  - `Restore/` — preset save/apply/share (Smart Restore Enhanced).
- **`Holograms/`** — Smart subclasses of vanilla `AFGHologram` types (factory, conveyor belt, conveyor lift, pipeline, power pole/wire). They derive grid/scale behavior and spawn **child holograms** for the grid.
- **`Services/`** — cross-cutting runtime services (HUD, chain-actor invalidation/rebuild, recipe management).
- **`Data/`** — declarative data: `SFBuildableSizeRegistry_*` (building footprint profiles — 14 files, T3 target) and `SFHologramDataRegistry`.
- **`UI/`, `HUD/`** — `SmartSettingsFormWidget` (the Smart Panel), `SmartUpgradePanel`, the HUD widget, and `SFFontLibrary` (multi-script UI font, see [`memory`/font notes]). `Constants/SFAssetPaths.h` holds shared FactoryGame asset paths.
- **`Module/`** — module startup (`SFGameInstanceModule`); `SFRCO` is the remote-call object for server RPCs (multiplayer-relevant; mostly placeholder today).

## Runtime model & conventions

- **Facade + services.** `USFSubsystem::Get(World)` is the entry point; it constructs/owns feature services and shared state. Holograms and UI reach features via the subsystem (e.g. `GetExtendService()`). Service init is currently inconsistent (constructor vs `Initialize()` vs lazy) — the T6 service-context epic addresses the resulting ordering bugs.
- **Hologram + child-hologram pattern.** A Smart hologram represents the "parent"; grid copies are spawned as child holograms. Affordability/validity (red/cyan) is derived from vanilla **construct disqualifiers**, not `SetPlacementMaterialState` — see the Extend affordability fix in the changelog.
- **Naming.** C++ types use the `SF`/`USF`/`ASF`/`FSF` prefix; assets use `Smart_`/`SF`. Log via `LogSmartFoundations` today (T7 will introduce per-feature categories declared in `Config/SmartFoundationsLogging.ini`).
- **Access to private engine members** is granted via SML AccessTransformers (`Config/AccessTransformers.ini`), not engine edits.
- **Localization.** `.po` → `.archive` → `.locres` via `scripts/compile_localization.ps1`; UI uses the runtime multi-script `DescriptionText` font. See [`Audits/Pre1.2CleanupAudit.md`] and the localization dev notes.

## Build & dev workflow

- **Package/deploy:** Alpakit (`RunUAT PackagePlugin … -DLCName=SmartFoundations -build -CopyToGameDirectory_Windows=<game>`). Close the running game first (it locks the mod DLL).
- **Editor introspection:** the SMLMCP server runs Python in the live editor (see [`Development/SMLMCP-Python.md`](Development/SMLMCP-Python.md)); SmartMCP exposes an in-game HTTP API used for diagnostics and (planned) smoke tests.
- **Tooling:** `scripts/` holds the localization compile/sync and the loc audit tools.

## Where to start reading

- A feature → its `Features/<Feature>/` folder (the service header documents intent).
- Placement behavior → the relevant `Holograms/` subclass.
- Panel/HUD → `UI/` + `HUD/`.
- The big, hard files (today): `SFSubsystem.cpp` and `SFExtendService.cpp` — being decomposed under the simplification charter.
