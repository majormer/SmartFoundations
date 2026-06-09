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
