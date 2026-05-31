# SmartFoundations 1.2 Upgrade — Refactor Re-Prioritization

**Status:** strategy / living plan. Branch `refactor/simplification-audit`.
**Why this doc exists:** the simplification refactor's *real* purpose is now explicit — survive the
**Satisfactory 1.2 upgrade** (releases **Tue 2026-06-02 ~09:00**, <3 days out) and then add
**multiplayer**. This re-frames the remaining/optional refactor work by **upgrade value** and
**multiplayer value** instead of the original tidiness criteria (which are met).

> Supersedes the "optional architecture" framing in
> [`Simplification-RemainingWork.md`](Simplification-RemainingWork.md) for sequencing purposes.
> The charter ([`Simplification-GOAL.md`](Simplification-GOAL.md)) is unchanged; criteria #1-#6 stay met.

---

## 1. What 1.2 actually changes (grounded, not assumed)

Source of truth: `L:\Personal\Repos\SatisfactoryModLoader-dev-1.2` — **but** its checked-out
`dev-1.2` branch is a stale snapshot (headers CL455399). The remote `dev-1.2` branch is **gone**;
the live 1.2 work is now **`origin/dev`** (verify each session: `git -C <repo> log origin/dev --oneline`).

| Axis | 1.0/1.1 (current) | 1.2 (`origin/dev`) | Impact on us |
|---|---|---|---|
| Engine | UE 5.3.2-CSS | **UE 5.6**, .NET **8.0** | new build env; API/ABI churn |
| Game headers | ~CL416835 era | **CL491125** | **primary compile-break source** — see §2 |
| Mod model | plain `Runtime` `.uplugin` | **Game Feature Plugin** under `Mods/GameFeatures/` | structural conversion of our mod |
| Build tooling | Alpakit/PackagePlugin (5.3) | reworked Alpakit; GameFeature copy step; Linux targets | our `RunUAT PackagePlugin` flow changes |
| SML | 3.11.3 | 3.11.x ported (new hooking on `dev-new-hooking`) | hook/API surface may shift |

**Key unknown — flag, don't guess:** the new ExampleMod GameFeature is *content-only* (no
`Build.cs`). We are a **89k-LOC C++ runtime mod**. *How a C++-heavy mod becomes a GameFeature*
(does the C++ stay a normal runtime module the GameFeature depends on? does it move inside the
GameFeature plugin? activation/loading-phase implications?) is the **single most important open
question** and must come from the **pending Discord guidance** / SML team / a real C++ GameFeature
example before we commit to a conversion shape.

## 2. Our exposure surface (what the header bump will hit)

Measured live on current `Source/` (224 files, ~88,954 LOC):

- **110 distinct FactoryGame headers** `#include`d.
- **100 distinct `AFG*/UFG*/FFG*` game types** referenced.
- **~40 module deps** in `SmartFoundations.Build.cs` (FactoryGame, SML, + ~38 engine modules —
  several renamed/moved between UE 5.3 and 5.6).

Each of those 100 game types is a place CSS can have changed a signature, renamed a member, moved a
header, or deleted the symbol. **The upgrade cost is roughly proportional to how *scattered* those
100 usages are.** Funnelling them through thin seams is the highest-leverage upgrade prep we can do
on 5.3 *today*, because it converts "fix the same broken call in 30 files" into "fix it in 1."

## 3. Re-prioritized work (by 1.2 + MP value, not tidiness)

| Item | Original framing | Value NOW | Do when |
|---|---|---|---|
| **API contact-surface map** (the 110 headers + 100 types → where used, how widely) | (n/a) | **CRITICAL** — the upgrade checklist + finds the scatter hot-spots | **Phase 0, now** |
| **Encapsulate wide-scatter game APIs** behind thin adapters | T6-adjacent / "also consider" | **HIGH** — shrinks break points for the port | Phase 0 (the worst offenders only) |
| **GameFeature-conversion gap analysis** (our `.uplugin`/`Build.cs`/module vs new template) | (n/a) | **HIGH** — scopes the structural migration | Phase 0 (analysis) / Phase 1 (execute) |
| **Smoke harness + build-validate reliability** | criterion #2 | **HIGH** — the only regression net during a blind port | Phase 0 (confirm green) |
| **T6 — DI service context + init phases** | "optional architecture" | **HIGH for MP** (server authority, deterministic init, ends reach-back nullptrs) | Phase 2 (post-port) — design ADR can start earlier |
| **Multiplayer-safety audit** (`GetFirstPlayerController`, client-spawn, cost-charge) | charter "flag don't fix" | **HIGH for MP** | Phase 0 catalog (cheap) / Phase 2 fix |
| Relocate subsystem hologram lifecycle/creation into `FSFHologramHelperService` | cards' careful moves | **LOW** unless it reduces API scatter | defer |
| Further cosmetic splits / `FSFHologramCostCalculator` dedup | T8 tail | **LOW** | defer |

**De-prioritized on purpose:** anything that only improves tidiness without reducing upgrade break
points or seeding MP seams. We do *not* start new behavior-changing extractions days before a port.

## 4. Phased timeline

### Phase 0 — Pre-release prep (now → Tue 06-02). Safe on 5.3, no game binaries needed.
Goal: walk into the port with a map and the fewest possible break points. **No actual porting yet**
(headers still in flux; game unreleased; `origin/dev` moving).
1. **Generate the API-surface map** — script over `Source/`: every FactoryGame header + game type,
   with usage counts and the files touching each. Output a checklist doc. *(highest priority)*
2. **Rank scatter hot-spots** — the game types/calls used in the most files. For the top offenders,
   add a thin local seam (free function / adapter) so the port fixes them once. Build-validate each.
3. **GameFeature gap analysis** — diff our `SmartFoundations.uplugin` + `Build.cs` + module layout
   against the new `Mods/GameFeatures/ExampleMod` template; write the conversion checklist. Do **not**
   convert yet (blocked on the C++-GameFeature unknown in §1).
4. **Confirm the safety net** — `scripts/smoke_test.py` runs; the consolidated post-refactor smoke
   (still owed) gets run by the maintainer so we branch into the port from a *known-good* baseline.
5. **MP-safety catalog** — cheap grep pass listing every single-player assumption; pure documentation.
6. **Read the Discord guidance** the moment it lands; fold it into §1 and Phase 1.

### Phase 1 — The port (release → building on 5.6). Turn-taking; game closed for builds.
1. Sync to `origin/dev` SML + headers (CL491125+); set up the 5.6 build env (.NET 8).
2. Convert SmartFoundations to a GameFeature per the resolved §1 shape.
3. Compile-fix header breaks **driven by the Phase-0 API map** (per-header, per-type checklist).
4. Get a clean `PackagePlugin` on 5.6, then the full in-game smoke (grid / auto-connect / extend /
   upgrade) on 1.2.

### Phase 2 — Multiplayer. After 1.2 is stable.
1. T6 DI context (ADR first) — CONSTRUCT/INITIALIZE/LAZY phases; replace `USFSubsystem`→sibling
   reach-back; this is also the natural place to enforce server authority.
2. Work the MP-safety catalog from Phase 0: replicate state, gate client-only spawns/cost-charges,
   resolve `GetFirstPlayerController` sites.

## 5. How the refactor-so-far already helps

The work already landed is not wasted under this reframe:
- **#5/#6 (no file >2k / no god-object)** — a header break in a 9k-line file is far harder to fix
  than in a focused 1.5k-line TU. The decomposition directly speeds the port.
- **Per-feature log categories + single edit points** (`SFAssetPaths.h`, `BuildableSizes.csv`) —
  fewer scattered literals to re-validate against new asset paths/IDs in 1.2.
- **Extend/Subsystem coupling reduction** (`friend`/`Owner->`, service seams) — the same seams that
  make the port localized are the seams MP authority will hook into.

## 6. Decisions needed from the maintainer

1. **Plan scope now:** should I start Phase 0 step 1 (generate the API-surface map) immediately, or
   review/adjust this plan first?
2. **C++ GameFeature shape (§1):** is there Discord/SML guidance or a real C++ GameFeature example to
   point me at, or do we treat it as an open question until the guidance lands?
3. **Pre-port baseline smoke:** worth running the still-owed consolidated post-refactor smoke before
   the port so Phase 1 starts from known-good?
