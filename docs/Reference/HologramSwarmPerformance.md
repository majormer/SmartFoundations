# Hologram-Swarm Performance Contract (#497 / #499 lessons)

Distilled from the 2026-07-17 stackable-scaling investigation (13 Very Sleepy captures, 4
diagnostic rounds, issue #497 comments -5000117025 → -5008711100). These are **contracts every
Smart preview-swarm system must satisfy** — Grid Scaling, Extend, Scaled Extend, Restore, Walk,
and every Auto-Connect family (stackable, belt, pipe/junction, hypertube, wall/ceiling,
blueprint seams, scale-daisy).

The one-line summary: **a hologram is cheap to create and catastrophically expensive to touch.**
Vanilla assumes ONE hologram with a handful of children; every per-frame or per-eval vanilla
call we let through multiplies by our N (thousands) and often by a hidden vanilla O(N) inside.

## The six lessons

### L1 — Never toggle `LockHologramPosition` per eval
`AFGHologram::LockHologramPosition` broadcasts an actor-representation update whose UI listener
**creates a UMG map widget per call** (FGHologram.cpp:547). Per-eval unlock→move→relock at
hundreds of pairs = allocation storm → 2.1M UObject cap fatal (#499).
**Contract:** lock state changes only on TRANSITION (`IsHologramLocked() != desired`), and
update-in-place paths skip the whole unlock/reroute/relock when a route signature matches
(`FSFStackableRouteSig` pattern: endpoints + normals + routing mode + build class).

### L2 — Set-once material gates must precede EVERY sweep, including Super
`Super::SetPlacementMaterialState` re-applies materials to all components (render-proxy
rebuilds). A same-state early-out placed AFTER the Super call gates only your own sweep.
**Contract:** the early-out is the first statement; `Super` runs only when state actually
changes.

### L3 — Vanilla writes CONFLICTING material states every frame; reject, don't just dedupe
The parent cascade writes the PARENT state (red while unaffordable) and per-child
`ValidatePlacementAndCost` writes the child's own validity (OK) — red↔OK every frame defeats
any same-state gate and rebuilds all spline proxies.
**Contract (belt/pipe pattern):** reject an `HMS_OK` write on a parented child while the
parent's own state is not OK; non-OK writes always pass (preserves #437 self-red).

### L4 — `GetHologramMaterialState()` is O(children); use the raw reader
The vanilla getter AGGREGATES via `GetHologramsToShareMaterialStateWith` (walks the child
array) on **every call**. Per-child/per-pair use goes quadratic (13-second frames at 8K
children).
**Contract:** any per-child, per-pair, or per-frame path reads
`USFHologramDataService::GetRawPlacementMaterialState()` (cached-reflection read of
`mPlacementMaterialState`). The aggregate getter is for one-shot/root-level use only.
Corollary: audit every vanilla getter before putting it in a loop — vanilla getters are not
free.

### L5 — Clearance detectors form a quadratic overlap-pair database
Every hologram's BeginPlay runs `SetupClearanceDetector`, creating an overlap `UBoxComponent`.
Detector↔detector pairs grow O(N²); every spawn (`BeginComponentOverlap`, O(N) list scan per
pair), every teardown, and every collision toggle (vanilla hides/shows the root per frame on
invalid aim!) pays linear scans over the pair list.
**Contract (three parts):**
1. Preview children spawned deferred get `SetActorEnableCollision(false)` **BEFORE
   `FinishSpawning`** — BeginPlay then registers the detector inert (a post-spawn disable pays
   the storm TWICE: registration + teardown).
2. Children that must keep mesh collision (vanilla-delegate supports feeding AC) get
   `USFHologramDataService::DisableClearanceDetector()` — kills only the detector's overlap
   events.
3. Strip `mClearanceData` too (`9bd9c2f` pattern) so the root's combined clearance volume
   stays root-sized — but know that the strip alone does NOT stop the detector (BeginPlay
   already sized it).

### L6 — Vanilla per-frame validation and placement cascades must be cancelled for swarm members
`UFGBuildGunStateBuild::TickState` runs `ValidatePlacementAndCost` (→ `CheckValidPlacement` →
`CheckClearance` → Chaos overlap per hologram) and the locked-parent nudge cascade
(`SetHologramNudgeLocation` → `SetActorLocation(lockLocation)` = origin drag) every frame.
Smart classes no-op these virtuals; vanilla-delegate classes can't be overridden, so they are
cancelled at the SML hook seam. Additionally the ROOT's own clearance query degrades with
scene density (its query region processes every un-ignorable swarm body as a foreign hit) —
cancelled above a swarm-size threshold (`SFIsSmartGridMegaRoot`).
**Hook-seam rules (hard-won, in order):**
1. A base-class body may be an unhookable import thunk (dedi FATAL) — hook an override body.
2. SML virtual hooks bind ONE function body, not the vtable slot — hook the override the
   target class actually DISPATCHES to (check the class hierarchy).
3. Predicates must cover vanilla-spawned DESCENDANTS (walk ancestor tags), and remember the
   held ROOT is unparented and untagged BY DEFINITION.
4. Prefer authoritative Smart state (subsystem active hologram, tracked-children lists) over
   probing vanilla arrays.

## Diagnostic playbook (what actually worked)
- Very Sleepy (portable install), per-thread classification, full engine PDBs from CSS; trust
  call CHAINS not leaf names (unexported Smart frames mis-symbolicate to the nearest export).
- The EOSSDK `TickTracker` log line prints the TRUE frame period — a free frame-time probe in
  every shipping log (prints only when delayed).
- Counting diagnostics beat naming diagnostics: budgets get eaten by the hot common case (the
  root passes any "2nd+ per frame" filter; log the 3rd+, or count and summarize).
- Version-tag diagnostic output (`STATSv2`, `v3`) — a stale tag exposes a stale binary
  ("Targets are up to date" trap: game holding the DLL fails Alpakit deploy silently).
- `TransformUpdated` + `FPlatformStackWalk::StackWalkAndDump` finds any mystery mover in one
  repro — but REMOVE it immediately; each dump is tens of ms and becomes the hitch (and a
  restored-later transform still transiently trips a location trap).
- SmartMCP reads `mChildren` via reflection — compare like-for-like moments before drawing
  architecture conclusions from count mismatches.

## Application status (2026-07-17, branch perf/497-perframe-performance)

| Lesson | Stackable AC | Belt AC | Pipe/Junction AC | Hypertube AC | Extend | Scaled Extend | Restore | Walk | Grid |
|---|---|---|---|---|---|---|---|---|---|
| L1 lock transitions | DONE (741c39e) | audit | audit | finalize gated | n/a (Item 5) | n/a | DONE (Item 5) | audit | once-at-spawn |
| L2 gate-before-Super | DONE (belt/pipe classes cover all) | inherited | inherited | inherited | inherited | inherited | inherited | inherited | n/a |
| L3 oscillation reject | DONE (9b33125) | inherited | inherited | inherited | inherited | inherited | inherited | inherited | n/a |
| L4 raw state reader | DONE (b6e66c3) | TODO | TODO (mgr sites) | DONE | TODO (spawner) | TODO | TODO | TODO | TODO |
| L5 detector/spawn order | DONE (08b2059) | TODO | TODO | DONE (spans) | TODO | TODO | TODO | TODO | delegates DONE; T1/T2 audit |
| L6 hooks/cancels | DONE | tag-covered | tag-covered | tag-covered | class no-ops | class no-ops | class no-ops | standalone | DONE |
| route-sig skip | DONE | TODO | TODO (junction eval) | TODO | n/a (Item 3 analog) | Item 3 DONE | n/a | spans reused | coordinate keys |

L2/L3 "inherited": all conduit previews are ASFConveyorBeltHologram / ASFPipelineHologram /
ASFWireHologram instances, so the class-level guards cover every family automatically.
