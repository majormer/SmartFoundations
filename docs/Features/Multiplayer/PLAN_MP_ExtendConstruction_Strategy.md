---
title: Smart Multiplayer — Extend Construction Strategy
type: PLAN
date: 2026-06-09
status: Draft
category: Features
branch: feature/mp-server-construction
related:
  - ./DESIGN_MP_ConstructionModel.md
  - ./PLAN_MP_ScalingConstruction_Impl.md
  - ../Extend/IMPL_Extend_CurrentFlow.md
issues:
  - 176
  - 334
  - 309
---

# Smart Multiplayer — Extend Construction Strategy

How to bring Extend onto the same client/server construction model as scaling
(`DESIGN_MP_ConstructionModel.md`). Strategy only — sequenced after the scaling slice validates the
core mechanics.

## Why Extend is the harder case

Scaling places a **uniform** grid (one build class + one recipe), so its spec is tiny and the
server expansion is a single loop. Extend is **heterogeneous and relational**:

- It clones a captured subgraph: factory building(s) + their **distributors** + **belts**
  (`belt_segment`, factory↔distributor), **pipes** (`pipe_segment`), **lifts** (`lift_segment`),
  and the between-distributor **manifold lanes**.
- The clones must be **wired to each other** (auto-connect belts/pipes/power) — the inter-object
  logistics that scaling has none of. This is the part copy-placement can never do and the reason
  Extend is Smart's signature feature.
- Placement is **topology-aware**: it depends on walking the source building's connection graph.

So Extend needs (a) a richer spec, (b) a server-side topology walk, and (c) server-authoritative
**wiring** after construction.

## Two advantages Extend already has

1. **The parent is already swapped.** Extend already swaps the active hologram to
   `ASFFactoryHologram` via the proven `USFExtendService::SwapToSmartFactoryHologram`. So the
   risky "swap the plain-scaling parent + re-register" step that blocks the scaling slice **does
   not exist for Extend** — the custom parent (with `CustomSerialization` + `Construct` overrides)
   is already in place.
2. **The spec already exists in all but name.** `SFExtendCloneSpawner` already builds the clones
   from a JSON-ish description (the captured topology + per-role clone data, `FSFExtendTopology`,
   `JsonCloneId`, the role tags `belt_segment`/`pipe_segment`/`lift_segment`/lanes). That captured
   description **is** the reconstruction spec; today it is executed client-side into child
   holograms that are then serialized. The model change is to ship the description and execute it
   server-side.

## The blocker Extend must solve first (already planned)

`USFExtendTopologyService::WalkTopology` follows `UFGFactoryConnectionComponent::GetConnection()`,
but `mConnectedComponent` is `UPROPERTY(SaveGame)` — **server-only, not replicated**. On a client
`GetConnection()` is null, so the topology walk finds no chains and Extend never activates. The
already-chosen fix (unblocked by the `USFRCO` replication fix) is a **server-side topology walk via
RCO**: the client asks the server to `WalkTopology` once on activation; the server returns the
topology; the client caches it and builds the local preview. This is the front half of the model
for Extend and must land before Extend construction can be ported.

## Target flow (mirrors scaling, with topology + wiring)

1. **Activation / topology** — client aims at a building → RCO `Server_WalkExtendTopology(building)`
   → server walks the connection graph (it has the authoritative `mConnectedComponent`) → returns a
   compact `FSFExtendTopology` (distributors, chains, roles, counts). Client caches it.
2. **Preview — client-local.** Client builds the clone/belt/pipe preview from the cached topology
   exactly as today (`SFExtendCloneSpawner`), purely cosmetic.
3. **Spec.** The Extend spec = `{ cached FSFExtendTopology, clone count, direction/spacing/rotation,
   per-role plan }`. Compact relative to N serialized child holograms (it is counts + topology, not
   geometry-per-cell). Rides the already-swapped `ASFFactoryHologram` (Option A) or the RCO
   (Option B) — prefer the RCO here since activation already round-trips to the server.
4. **Server construction.** On commit, the server reconstructs from the spec:
   - construct the clone factory building(s) individually (real replicated buildables);
   - construct the per-role logistics (distributors, `belt_segment`, `pipe_segment`,
     `lift_segment`, lanes) individually via the same vanilla spawn path scaling uses
     (`SpawnChildHologramFromRecipe` per element, transforms reconstructed deterministically).
5. **Server-authoritative wiring (#334).** After the elements exist server-side, the server runs
   auto-connect / belt / pipe / power connection **behind `HasAuthority`** — the post-build hooks
   that today run on every peer get gated to the server. Connections are real and replicate.
6. **Cost / validation / failure** — as scaling: spec-based server cost (the depot-aware
   `CanAffordExtendCost` already exists), per-element failure isolation, no oversized blob.

## What to reuse vs build

| Need | Reuse | Build |
|---|---|---|
| Custom parent hologram | `ASFFactoryHologram` (already swapped for Extend) + the scaling spec hooks | An Extend spec variant (topology-bearing) |
| Topology | `FSFExtendTopology`, `WalkTopology` | `Server_WalkExtendTopology` RCO (client→server walk) |
| Per-element construction | The scaling server-expansion pattern (`SpawnChildHologramFromRecipe` + position math) | Per-role transform reconstruction from topology (vs grid counters) |
| Wiring | Existing auto-connect / chain-rebuild services | `HasAuthority` gating on the post-build hooks (#334) |
| Cost | `CanAffordExtendCost` | — |

## Determinism requirement

The server must reconstruct the same clone arrangement + topology the client previewed. Because the
topology now originates **on the server** (step 1), determinism is easier than scaling: the server
is the source of truth for the graph, and the client preview is built from the server-returned
topology. Spacing/direction/rotation math must match (reuse the same calculators on both sides).

## Slice order (after the scaling slice validates the core)

1. **Server-side topology walk RCO** — `Server_WalkExtendTopology`; client builds preview from the
   returned topology. (Already the chosen Extend-MP fix; unblocks activation on clients.)
2. **Extend spec + server reconstruction** — ship the topology+clone spec; server constructs the
   clone factory + per-role elements individually (reusing the scaling expansion pattern).
3. **Server-authoritative wiring (#334)** — gate auto-connect/belt/pipe/power post-build hooks
   behind `HasAuthority`; wire server-side; confirm replication.
4. **Heterogeneous role coverage** — belts, pipes, lifts, lanes, passthroughs, distributors,
   power poles — each reconstructed + wired server-side; per-role failure isolation.

## Status 2026-06-10

**Slice 1 DONE (commit eebc0ac): client Extend previews work in MP for the first time.**
`USFRCO::Server_RequestExtendTopology` / `Client_ReceiveExtendTopology`; WalkTopology's client
branch fires a throttled request and returns false - activation's per-tick retry makes the flow
async with zero restructuring. `FSFExtendTopology` crosses the wire as-is: it references
replicated actors + their default-subobject components, which resolve against client proxies
(only the `mConnectedComponent` VALUES needed to DISCOVER the graph are server-only).
`FSFPowerChainNode`'s four naked members gained UPROPERTY() to actually serialize. Negative
replies cached briefly. Live-validated: previews show on a client.

**Slice 3 ALREADY DONE** by the #334 conduit-plan work (see PLAN_MP_AutoConnect_334.md): the
post-build wiring paths run server-side with authority and are live-validated across all families.

**Interim guard (commit 914bcee):** a client Extend FIRE is refused with an on-screen notice.
Firing the unported commit serializes the clone children into the construct message and the
recipe-less belt/lift clones assert the CLIENT in `SerializeConstructMessage` (live crash
2026-06-10: "Assertion failed: hologramRecipe" via `SetupPendingConstructionHologram`'s
pending-ghost path - SP never runs that path, which is why SP Extend fires fine). The guard is
removed by slice 2.

### Slice 2 concrete design (the remaining work)

**Key discovery: the clone description is wire-safe by construction.** `FSFSourceTopology` and
`FSFCloneTopology` (SFExtendCloneTopology.h) are fully reflected, VALUE-ONLY structs - strings,
floats, transforms, spline points, zero object pointers (designed as a JSON-ish schema). And
`FSFCloneTopology` carries its own executor: `SpawnChildHolograms(Parent, ExtendService, Out)` +
`WireChildHologramConnections`. The pipeline `CaptureFromTopology(topology) -> FromSource(source,
offset) -> SpawnChildHolograms` is deterministic given (topology, offset).

Flow (mirrors the scaling/conduit commit exactly):
1. Client fire with Extend active: capture the staged Extend commit, destroy the preview children,
   fire the childless parent (replaces the interim guard).
2. Stage via a new `USFRCO::Server_StageExtendCommit` into the subsystem per-player map (overwrite
   semantics like the scaling spec).
3. Server `Construct` hook (the existing seam): consume by instigator+BuildClass; per clone run
   `SpawnChildHolograms` + `WireChildHologramConnections` PRE-scope(); the vanilla child loop
   constructs everything; the Extend built-child registries + post-build wiring run server-side
   with authority (slice 3, already validated).
4. Cost: staged cost array captured client-side from the preview (the ConduitPlanCost pattern) -
   the GetCost hook appends it.

**Payload decision** (make at implementation time):
- Option 1: ship `TArray<FSFCloneTopology>` (one per clone, as the preview emitted them -
  includes Scaled Extend's post-FromSource rotation fix-ups). Faithful but O(clones) bytes;
  large manifolds x many clones can approach the reliable-RPC ceiling - needs the byte-estimate
  refusal guard like the conduit plan.
- Option 2: ship ONE `FSFSourceTopology` + clone offsets/rotations and re-run
  `FromSource` (+ the Scaled Extend fix-ups, which are plain code) server-side. O(1) in clone
  count; requires the fix-up code to be callable server-side deterministically.
Start with Option 1 + the guard (simplest, most faithful); fall back to Option 2 if sizes bite.

Scaled Extend rides the same commit (it already builds per-clone `FSFCloneTopology` instances -
`SFExtendScaledService.cpp:698`). Restore replays captured clone topologies through the same
spawner, so it inherits the ported commit too.

## Slice 2 LIVE-VALIDATED (2026-06-10, commits d61c7b1..68ba8d5)

Normal Extend AND Scaled Extend work end-to-end in MP: 21-clone scaled constructor chain built,
every clone set wired 8/8 manifest connections, daisy power, dismantle groups, and EXTENDING OFF
the built chain's end clone works (topology walk valid on built clones). Five root causes fixed
en route - see the workstream memory / commit log: (1) wiring anchors lived in the client-only
swapped hologram class; (2) the large commit RPC lost the cross-channel race (fix: parameters-only
spec + continuous pre-staging + TTL); (3) deferred wiring races the post-construct cleanup (fix:
synchronous pass at the Construct seam with the built parent); (4) server vanilla clearance
rejects Extend placement (fix: CheckValidPlacement leniency hook - on the AFGBuildableHologram
OVERRIDE, the base member pointer is an unhookable thunk); (5) THE ROOT: the commit's clone
topology was client-captured and CaptureBeltChain/CapturePipeChain read GetConnection() (null on
clients) - every MP Extend built unwired. Fix: the spec carries ONLY parameters {ParentOffset,
Cost, SourceBuilding, ScaledClones[]}; the server derives the topology at the construct seam
(walk -> CaptureFromTopology -> FromSource -> ReconstructCommitOnServer). BONUS: fixed a latent
SP bug in the scaled clone wiring loop (prefix-stripped map keys vs prefixed connection targets =
empty per-clone manifests; now generated from the prefixed topology against the global id map).

Remaining for Extend-MP hardening: rotated scaled extend (clearance leniency untested), pipe/lift
heterogeneous topologies (refineries), Restore, costs in non-creative, diagnostics strip.

## Dependencies / ordering

- Depends on the **scaling slice** proving the spec round-trip + server expansion + (for Option A)
  the swap/re-registration mechanics in a live MP session.
- Depends on the **`USFRCO` replication fix** (done) for both the topology-walk RCO and any
  spec-bearing RCO.
- #334 (server-authoritative wiring) is shared infrastructure between scaling's "no inter-object
  logistics" case and Extend's manifolds; doing it for Extend also hardens any future scaling
  auto-connect.

Per the workstream rule, nothing ships until **all** features work in MP; the oversized-grid guard
remains the interim safety measure.

## OPEN: dedi factory-tick crash after blender scaled extend (2026-06-10 07:22)

Repro context: blender scaled extend (7 clone sets, 151 children, every clone wired 23 incl.
12 pipe connections) built clean at 07:20:10 and ticked for ~2.5 minutes. At 07:22:42 the player
aimed at the END clone (walk valid=1 - extend-off-chain ACTIVATED, the feature works) and fired a
normal extend off it at 07:22:43. One frame later: EXCEPTION_ACCESS_VIOLATION on a heap address in
`AFGBuildableSubsystem::TickFactoryActors` (FGBuildableSubsystem.cpp:644, ParallelFor lambda) -
a freed/garbage actor in the factory tick arrays. NO Smart logs in the crash frame: the new fire's
construct RPC had NOT yet been processed (staged-commit log only), so the leading hypothesis is a
TIMEBOMB from the blender build (poisoned factory-tick entry) rather than the new construct.

Evidence preserved: crash folder UECC-Windows-B806A0C64D3B58734FC0E38F04B60288_0000 (UEMinidump.dmp
+ log), WiringManifest.json from the build. No cdb/windbg installed yet - install Windows
Debugging Tools and open the dump with the FactoryServer-SmartFoundations PDB to identify the
faulting actor type.

Diagnosis plan: (1) dump analysis; (2) repro the blender chain and observe WHEN it dies (idle vs
production start vs player action) - if reproducible, bisect the commit wiring pass by disabling
CreateChainActors / RebuildPipeNetworks for the commit path (the manifest reported chains=0,
suggesting the connected belts may lack unified chain registration - the THESIS hazard class);
(3) compare against an identical SP blender scaled extend for baseline.

Also pending from the same round: end-blender showed 2/6 factory connections (1 belt in, 1 pipe
in) - CONFIRM whether the source blender had only those connected (Extend mirrors the source's
chains; partial source = partial clone is correct behavior).
