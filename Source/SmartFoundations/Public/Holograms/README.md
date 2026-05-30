# Smart! Hologram System

Smart! replaces vanilla build-gun holograms with its own classes so it can add grid
scaling, spacing, auto-connect, extend, and related behavior. Building-specific behavior
is driven by **adapters**, not by per-building hologram subclasses.

## Core hierarchy

```
AFGHologram (engine)
└── ASFSmartHologram          (Core/SFSmartHologram)        — Smart metadata + logging
    └── ASFBuildableHologram  (Core/SFBuildableHologram)    — registration + ConfigureActor
        ├── ASFFactoryHologram      (Core/SFFactoryHologram)    — production buildings (recipe copy)
        ├── ASFFoundationHologram   (Core/SFFoundationHologram) — foundations
        └── ASFLogisticsHologram    (Core/ASFLogisticsHologram) — belts/pipes/attachments base
```

Production buildings (constructor, smelter, blender, etc.) use the shared
`ASFFactoryHologram` directly; there are no per-building hologram subclasses. The only
specialized production class is `Production/ASFResourceExtractorHologram` for miners/extractors.

## Building-specific behavior: adapters

`Holograms/Adapters/` holds the adapter implementations selected per buildable
(`ISFHologramAdapter` + `USFSubsystem::CreateHologramAdapter`). Adapters — e.g.
`SFFactoryAdapter`, `SFGenericAdapter`, `SFResourceExtractorAdapter`, `SFRampAdapter`,
`SFWallAdapter`, `SFPassthroughAdapter`, `SFElevatorAdapter`, `SFUnsupportedAdapter` —
provide size/offset/validation specialization without a class per building.

## Child holograms

Auto-Connect and Extend spawn child holograms for the connecting infrastructure:

- `Logistics/` — belts, conveyor lifts, pipelines, splitter/merger attachments, pipe
  junctions, passthroughs, wall holes, water pump attachments, and their child variants.
- `Power/` — power pole child holograms and wires.
- `Core/` — shared child-hologram bases (`SFBuildableChildHologram`,
  `SFSmartChildHologram`, etc.) plus floodlight/standalone-sign child holograms.

## History

The pre-1.2 cleanup removed the empty per-building hologram subclasses
(`SFFactoryHologram_*`, `SFFoundationHologram_Standard`, `SFStorageHologram`,
`SFPowerHologram`, `SFTransportHologram`, `SFSpecialHologram`). They were never selected
by the spawn/adapter path — building-specific behavior lives in the adapters above.
