# Smart! Blueprints — Requirements & Handoff Spec (#168)

**Feature brand:** Smart! Blueprints
**Issue:** [#168 Scaleable blueprints](https://github.com/majormer/SmartFoundations/issues/168) (`critical`, flagship)
**Branch:** `feature/168-scaleable-blueprints`
**This doc = the entry point.** It states WHAT must be true. For HOW, read the two companions:
- `168-ScaleableBlueprints-Research.md` — vanilla API surface, root causes, Smart touchpoints.
- `168-SmartBlueprints-SeamAutoConnect-Design.md` — the locked seam auto-connect design.

Read all three before implementing. The design doc is a contract, not a suggestion.

---

## 1. What already works (DONE — do not rebuild)

Validated in-game (SP) on this branch:
- Scaling a held blueprint produces a grid of copies (adapter + `mLocalBounds` footprint).
- Copies preview aligned to the grid (measured content-convention correction).
- Fire constructs **all** copies with their full contents (blueprint Construct hook).
- Copies are real blueprint **instances** — vanilla snap / auto-connect / dismantle recognize
  them (identity fix: `mBlueprintDescName` carried to clones). Confirmed: a hand-held blueprint
  in Auto-Connect mode wires ITSELF to our built copies.

Commits `10f29c0..741a8cd`.

---

## 2. The remaining feature: seam auto-connect (THIS is the build)

**Goal:** when a blueprint is scaled, the belts and pipes that terminate at the blueprint's edges
automatically connect **copy-to-copy across the grid seams**, previewed at scale time, charged
for, and built on fire — using Smart's own auto-connect machinery (vanilla's is interactive-only
and cannot initiate from programmatic clones; that gap is proven).

### Functional requirements (v1)

- **FR1 — Pair search.** Determine seam connector pairs ONCE per blueprint, in local space, in the
  untransformed flush-tiled configuration. Pairs are identified by connection-component **index**
  (not position), so they are transform-invariant. Covers +X/−X and +Y/−Y faces in v1 (compute
  and cache Z too, but do not spawn Z conduits in v1).
- **FR2 — Openness.** Only pair connectors that are OPEN (their duplicated component's original
  blueprint-world connector is unconnected). Via `mDuplicateConnectionToOriginalMap`.
- **FR3 — Hold through transforms.** As spacing/steps/stagger/rotation change, the SAME index
  pairs are re-previewed with moved endpoints. No re-search. A pair whose current geometry is
  invalid goes dormant and returns when geometry allows.
- **FR4 — Vanilla judges validity.** Each seam conduit's shape is validated by the game
  (#466 arbiter: `FGCDConveyorTooSteep`/`InvalidShape` for belts, `IsRoutedShapeInvalid` for
  pipes). Declined conduits are hidden and counted in the skip-summary HUD — inherited, not rebuilt.
- **FR5 — Preview + cost.** Seam conduits render as Smart previews at scale time and their cost
  aggregates into the build cost (via child-hologram AddChild, as all Smart conduits do).
- **FR6 — Build.** On fire, seam conduits construct and wire geometrically against the just-built
  copies (existing `SF_BeltAutoConnectChild` / pipe construct paths).
- **FR7 — Belts and pipes.** Both, from ONE conduit-agnostic pair search. Belts first for
  test clarity, pipes immediately behind on the same table, both before the feature ships.

### Acceptance criteria (v1)

- Scale a blueprint whose contents have edge belts (e.g. the TestBP used all session): adjacent
  copies show connecting belt previews at the seams; fire builds them; items flow copy-to-copy.
- Same for a blueprint with edge pipes.
- A 3×3 (or 3×2) grid wires on BOTH X and Y seams simultaneously — the 2D-fabric case vanilla
  cannot do. This is the headline demo.
- Widen spacing past the belt's buildable limit → seam belts vanish + skip HUD reports "too
  steep/invalid"; narrow it back → they reappear. (Pairs held, validity separate.)
- Cost shown/charged reflects the copies AND their seam conduits.
- No double-wiring with vanilla parent auto-connect (domains are disjoint; verify, don't assume).

---

## 3. Hard constraints (non-negotiable)

1. **The model is fixed:** pairs by index, computed untransformed, held through transforms,
   vanilla judges per-evaluation validity. Do NOT re-derive pairs by proximity every frame
   (that's the distributor model; it cannot express dormant-pair-returns and is the wrong fit).
2. **No new Smart wiring logic** — reuse the existing belt/pipe preview + construct + skip-HUD
   machinery. The new code is the pair SEARCH and its cache, not a connection engine.
3. **Reuse, don't diverge:** one conduit-agnostic search feeds both belt and pipe spawners.
4. Follow session-standard practice: validate-in-game-then-commit, `[#168]` Log-level (NOT
   Verbose) diagnostics, AccessTransformers for protected members, honest known-gap notes in
   commits.

---

## 4. Scope

**In (v1):** X and Y seams, belts and pipes, SP. Preview + cost + build + skip HUD.
**Deferred (documented, not in v1):**
- **Z seams:** pipes are cheap (v1.5 — vertical native); belts need conveyor-LIFT preview
  machinery Smart lacks (v2). Pair table carries Z from day 1; spawner ignores it in v1.
- **Multiplayer:** spec-expansion staging exists; construct-messaging + seam-plan capture for
  blueprint grids is untested/unbuilt.
- **Arc-rotation** content-delta edge case; large-blueprint staging hitch.

---

## 5. Pre-ship checklist (whole feature, not just seams)

- [ ] Seam auto-connect (this build): FR1–FR7 + acceptance criteria above.
- [ ] Cost verification: scaling N copies charges for N (untested to date).
- [ ] Blueprint Dismantle groups a clone correctly (should pass post-identity-fix — quick check).
- [ ] Changelog / ficsit / wiki + localization (release workflow, AGENTS.md).
- [ ] Announcement beat: the 2D-fabric capability vanilla can't do (flagship framing).
- [ ] MP decision: ship SP-only with a clear note, or hold for the MP slice.

---

## 6. Start here (fresh session)

1. Read this doc, then Research, then Seam Design.
2. Build FR1 (pair search + cache) first — separable, testable in isolation via a `[#168]` dump
   of the computed pair table before any preview work.
3. Then belt previews (FR5/FR6 for belts), validate in-game, then pipes.
