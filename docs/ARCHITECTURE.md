# SmartFoundations — Architecture Map

A 10-minute orientation for maintainers and contributors. Smart! is a Satisfactory 1.2
build-gun enhancement mod built with UE 5.6.1-CSS and Satisfactory Mod Loader 3.12. This
describes *how the code is organized and how the pieces talk* — not every detail. For the
rules governing new and relocated code, see the [code organization policy](Development/CodeOrganization.md).

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
   ▼                          ├── scaling, spacing, and grid transforms
child holograms (grid)        ├── belt, pipe, power, and hypertube auto-connect
                              ├── Extend, Restore, Upgrade, and Smart Walking
Smart Panel / Upgrade Panel ──┤
(UI/) ────────────────────────┘   HUD, input, recipes, chain actors, and data registries
```

## Source layout (`Source/SmartFoundations/`)

UE split: `Public/` (headers other code includes) and `Private/` (implementations). By area:

- **`Subsystem/`** — `USFSubsystem`, the central facade and owner, plus shared hologram,
   validation, position, input, and performance helpers.
- **`Features/<Feature>/`** — feature-owned logic. Current slices are `Arrows`,
   `AutoConnect`, `Extend`, `HypertubeAutoConnect`, `PipeAutoConnect`,
   `PowerAutoConnect`, `Restore`, `Scaling`, `Spacing`, `Upgrade`, and `Walk`.
- **`Holograms/`** — shared hologram behavior and adapters for vanilla hologram families.
   Smart! extends several branches of the vanilla hierarchy rather than routing every adapter
   through one Smart! base class. Grid placement uses parent and child holograms.
- **`Services/`** — cross-feature runtime services for grid state/transforms/spawning, HUD,
   hints, recipes, chain actors, and diagnostics.
- **`Data/`** — shared registries and hologram data. Building-size profiles are maintained in
   `Content/Data/BuildableSizes.csv` and generated into
   `Private/Data/SFBuildableSizeRegistry_Data.cpp` by `scripts/gen_size_registry.py`.
- **`Core/Net/`** — shared authority helpers and `SFRCO`, the active remote-call object used by
   shipped multiplayer paths.
- **`Input/`, `Logging/`, `Config/`, `Module/`** — input registration and transform state,
   runtime-filtered logging, configuration types, and game-instance startup.
- **`UI/`, `HUD/`** — the Smart Panel, Upgrade panel, shared widgets, and runtime HUD.
- **`Shared/`, `Constants/`** — code shared by multiple features and header-only constants.

## Runtime model & conventions

- **Facade + services.** `USFSubsystem::Get(World)` is the runtime entry point. The subsystem
   explicitly owns and initializes feature services and shared state; holograms and UI reach
   those capabilities through the subsystem (for example, `GetExtendService()`).
- **Hologram + child-hologram pattern.** A Smart hologram represents the "parent"; grid copies are spawned as child holograms. Affordability/validity (red/cyan) is derived from vanilla **construct disqualifiers**, not `SetPlacementMaterialState` — see the Extend affordability fix in the changelog.
- **Authority and replication.** World mutations run through server-authoritative paths.
   `SFRCO` carries explicit client-to-server requests, while vanilla construction messages and
   replicated state cover placement paths. Multiplayer support shipped in v32.0.0.
- **Naming and logging.** C++ types use the `SF`/`USF`/`ASF`/`FSF` prefix; assets use
   `Smart_`/`SF`. Logs use `LogSmartFoundations` plus runtime-filtered categories declared in
   `Config/SmartFoundationsLogging.ini` and implemented by `FSFLogRegistry`.
- **Access to private engine members** is granted via SML AccessTransformers (`Config/AccessTransformers.ini`), not engine edits.
- **Localization.** `.po` → `.archive` → `.locres` via
   `scripts/compile_localization.ps1`; localization audit and export helpers live in `scripts/`.

## Build & dev workflow

- **Package/deploy:** use Alpakit for content changes and release packages. Close the running
   game before deployment because it locks the mod DLL.
- **C++ iteration:** build the appropriate Shipping target and deploy its DLL/PDB when no
   content changed. Client/server networking changes require matching client and server builds.
- **Tooling:** `scripts/` contains localization, size-registry generation, configuration parity,
   and smoke-test helpers.

## Where to start reading

- A feature → its `Features/<Feature>/` folder and, when available, its `docs/Features/` reference.
- Placement behavior → the relevant `Holograms/` subclass.
- Panel/HUD → `UI/` + `HUD/`.
- Networking → `Core/Net/`, then any feature-local `Net/` folder and `[MP-*]` divergence map.
- Ownership and lifecycle → `Subsystem/SFSubsystem.h` and its concern-named implementation files.
