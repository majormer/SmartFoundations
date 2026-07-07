# Smart! Blueprints — Seam Auto-Connect Design (#168, flagship)

**Status:** Design LOCKED with maintainer 2026-07-06. Implementation next (fresh session recommended; this doc is the spec).
**Branch:** `feature/168-scaleable-blueprints` — scaling/construction/identity already work (see commits `10f29c0..741a8cd` and `168-ScaleableBlueprints-Research.md`).

## Decision: Smart wires the seams, not vanilla

The vanilla gap is proven: vanilla blueprint auto-connect is interactive-only — its
`FGBlueprintOpenConnectionManager` state is built by aim-time overlap + per-frame
`UpdateAutomaticConnections(hitResult)`, so programmatic clones can never INITIATE connections
(live-confirmed: after the identity fix, hand-held blueprints connect TO our clones; clones
initiate nothing). Per the original constraint ("no Smart wiring unless a vanilla gap"), the
gap is found → Smart wires the seams with its own proven machinery. Bonus over vanilla:
preview at scale time + honest cost aggregation + the skip-summary HUD.

Vanilla/Smart domains are DISJOINT — no forced disable needed in v1: vanilla AC (parent build
mode) wires parent ↔ pre-existing world (it only ever targets already-BUILT actors seen at aim);
Smart wires grid-member ↔ grid-member seams. If testing shows a collision, gate then.

## The model (maintainer's): pairs by index, computed untransformed, held through transforms

- **Pair identity = connector indices, not positions.** Vanilla spawns blueprint contents in
  deterministic order (its own manager relies on this), so every clone duplicates its connection
  components in the same order. A seam pair is `(outIndex k on copy i) ↔ (inIndex m on adjacent copy)`.
  Index pairs survive every transform by construction — nothing re-searches, nothing drifts.
- **Pair search runs ONCE per blueprint, in LOCAL space, in the untransformed (flush-tiled)
  configuration:** for the +X seam, open connectors near the +X bounds face facing +X matched
  against −X-face connectors facing −X in the same local (Y,Z) lane, direction-compatible
  (belt OUT→IN; pipe types compatible). Same independently for Y (and Z, see below). Ties break
  deterministically (quantized keys — the #464 lesson).
- **Transforms move endpoints, never pairs.** Per evaluation, for each adjacent copy pair along
  each axis, look up components BY INDEX on the two clone holograms and hand them to the preview
  machinery. Spacing widens → same pair, longer belt. Stagger → same pair, curved belt. Too
  far/steep → **vanilla declines the shape** (#466 arbiter) + skip-summary HUD reports it; the
  pair persists dormant and reappears when transforms allow. Pairing and validity are separate
  concerns — the distributor system cannot do this; this can.
- **2D fabric (the flagship differentiator):** every interior copy participates in up to 4 seams
  (±X, ±Y) simultaneously. Vanilla AC is a 1-neighbor snap; a Smart 3×3 grid wires into a true
  2D fabric in one placement.

## Where the open ends come from

Blueprint holograms DUPLICATE real connection components from their contents
(`DuplicateConnectionComponent<T>`, header-confirmed): every clone carries actual
`UFGFactoryConnectionComponent`/`UFGPipeConnectionComponent` instances at exact world positions.
Open-ness = the duplicated component's ORIGINAL (blueprint-world buildable connector) is
unconnected: `mDuplicateConnectionToOriginalMap` (private → AccessTransformers Friend, same
pattern as `mLocalBounds`/`FSFBlueprintAdapter`).

## Z seams (deferred; thought through)

- **Pipes:** near-free — pipes run vertical; bottom copy's up-facing port ↔ upper copy's
  down-facing port, same local (X,Y) lane; existing pipe spawner routes it. v1.5.
- **Belts:** vertical belt transport = conveyor LIFTS. Pair model unchanged (top-face ↔
  bottom-face, same lane) but the spawned conduit must be a LIFT preview — machinery Smart does
  not have yet (lifts live in Extend/Upgrade, not auto-connect). Real v2 work item.
- The pair TABLE carries Z pairs from day one (axis-uniform search, free to compute); the
  SPAWNER services X/Y only in v1. Blueprint-design pattern to document for players: leave
  lift stubs / floor-hole ports at fixed (X,Y) on top/bottom faces → towers self-connect (v2).

## Implementation map

| Piece | Where | Notes |
|---|---|---|
| Pair search + cache | new `SFBlueprintSeamService` (or module in AutoConnect/) | Runs lazily on first grid change per blueprint; cache next to `BlueprintChildContentDelta` on the subsystem. Table: `{axis, outIndex, inIndex, kind}`. Conduit-agnostic. |
| Openness check | AccessTransformers Friend for `mDuplicateConnectionToOriginalMap` | Filter dup components whose original is unconnected. |
| Belt previews (v1) | existing `CreateOrUpdateBeltPreview` | Endpoints = dup components by index on adjacent clones. Facing sanity + vanilla shape arbiter + skip HUD all inherited. Cost aggregates via AddChild (proven). |
| Pipe previews (v1, right behind belts) | existing pipe child spawner (`SpawnPipeChild`-family) | Same table, same endpoints; `IsRoutedShapeInvalid` arbiter inherited. |
| Evaluation trigger | orchestrator grid-change/debounce path | Same cadence as distributor evaluations. |
| Fire/build | existing `SF_BeltAutoConnectChild` / pipe construct paths | Wire geometrically against just-built actors; conduit-plan capture covers MP (when blueprint MP slice lands). |
| Skip reporting | existing `FSFAutoConnectSkipSummary` | Declined seam conduits count as too steep / invalid shape. |

## Punch list before feature ships (beyond seams)

- Cost verification for clones (charged N copies?) — untested.
- Blueprint Dismantle grouping on clones (should work post-identity-fix) — 10s check.
- MP slice: spec-expansion staging exists; construct-messaging + seam plan capture untested.
- Arc-rotation mode: content-delta rotates by parent frame only (edge case).
- Staging hitch on large blueprints (LoadBlueprintToOtherWorld per clone) — measure, maybe budget.
- Z seams: pipes v1.5, belt-lifts v2 (above).
