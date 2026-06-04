# SmartFoundations Refactor — Session Handoff

Paste this (or point the new session at this file) to resume the code-simplicity refactor with full context.

---

## Kickoff prompt

You are continuing an in-progress **code-simplicity refactor** of the SmartFoundations Satisfactory
mod (UE5.3 + Satisfactory Mod Loader), on branch `refactor/simplification-audit`. The goal is to make
the codebase materially easier to work in — smaller files, less duplication, less coupling — **without
changing behavior**.

Before doing anything, read these in order:
1. `docs/Audits/Simplification-GOAL.md` — the charter (6 success criteria, themes T1–T8, guardrails).
2. `docs/Audits/Simplification-RemainingWork.md` — the live status snapshot: scorecard, largest files,
   remaining epics in recommended order, and the build/smoke loop.
3. `docs/Audits/SimplificationAudit.md` — the living tracker (epic table + dated work log).
4. `docs/ARCHITECTURE.md` — the 10-minute architecture map.

Then propose the next step and wait for my go-ahead before editing.

## Where things stand (2026-05-30)

- Branch `refactor/simplification-audit`, ~43 commits ahead of `origin/main` (`15fe368`), tree clean
  (only untracked: two `scripts/loc_*.txt` reports — ignore them). Run `git log -1` for the exact HEAD;
  local `main` may be stale, so compare against `origin/main`. Rollback tag: `refactor-baseline` at `9b15ecc`.
- **5 of 6 charter criteria met.** Done: architecture map, smoke-test harness (`scripts/smoke_test.py`),
  single edit point for asset paths (`SFAssetPaths.h`) and building sizes (`Content/Data/BuildableSizes.csv`),
  per-feature log categories (`SFLogMacros.h`). Epics complete: **T3** (size registry → CSV+generated,
  smoke-passed), **T2** (Extend clone-topology split, smoke-passed), most of **T4/T7**.
- **The 2 open criteria — no `.cpp` >~2k lines, no god-object >3k — both require T1.**

## Next: T1 — decompose the god-objects

The only path to the last two criteria. Targets (live `wc -l`):
- `Features/Extend/SFExtendService.cpp` — 7,718 (**round 1 DONE + smoked**, `fd27261`: Diagnostics +
  Restore-replay split out; units F/G/I/J remain)
- `Subsystem/SFSubsystem.cpp` — 9,227 (not started)
- Scope-adds also >2k: `SFAutoConnectService.cpp` (4,771), `SFPipeAutoConnectManager.cpp` (2,789),
  `SFHologramHelperService.cpp` (2,144). (There are 9 `.cpp` files >2k total; full list in
  `Simplification-RemainingWork.md`.)

**Current state (2026-05-30):** T1 round 1 on Extend is complete and in-game-smoked. The full,
audited per-slice decomposition plan for ALL remaining targets is being built in
[`docs/Audits/RefactorCompletionPlan.md`](docs/Audits/RefactorCompletionPlan.md) under a `/goal`
planning loop (read-only audit; each slice pre-audits call sites + cross-unit shared state + runtime
coupling, to prevent the mid-flight surprises hit in slice B). **Resume code work from that plan once
it's complete.**

## How T1 work proceeds (the rules)

T1 is **NEEDS-CARE** per the charter, so each slice follows this loop:
1. **Pure-move first** — extract code verbatim into a focused unit behind a thin orchestrator. Behavior
   tweaks are separate follow-up PRs, never bundled into a move.
2. **Audit-before-edit** — read all call sites of a symbol before moving/renaming it.
3. **Compile-validate** with the game closed (see build notes), then commit. One cohesive slice per
   commit/PR, small and revertable.
4. **In-game smoke** with the maintainer (game open): grid, auto-connect, extend, upgrade as relevant
   to the slice. Mark the slice done in the tracker only after smoke passes.

## Build & smoke mechanics

- **Compile-validate (no deploy):**
  `RunUAT.bat -ScriptsForProject="L:/Personal/Repos/SatisfactoryModLoader/FactoryGame.uproject" PackagePlugin -project="<same>" -clientconfig=Shipping -serverconfig=Shipping -DLCName=SmartFoundations -build -platform=Win64 -nocompileeditor -installed`
  (omit `-CopyToGameDirectory_Windows` so it never touches the running game's DLL). `ExitCode=0` +
  `BUILD SUCCESSFUL` = clean. `ExitCode=6` + `error C####` = real compile error.
- **Deploy build** (adds `-CopyToGameDirectory_Windows="G:/SteamLibrary/steamapps/common/Satisfactory"`)
  needs the **game closed** — it locks `...SmartFoundations-Win64-Shipping.dll`; deploying while the
  game runs fails with `UnauthorizedAccessException` (`ExitCode=1`), which is a lock failure, not a code error.
- **Live Coding** (editor open) blocks editor-target builds; the Shipping `PackagePlugin` path above
  avoids that. Do not build concurrently with the maintainer's own build/game.
- **Smoke** needs the game running with the in-game diagnostic mod enabled (HTTP API at :5095); `scripts/smoke_test.py`
  reads live state. The maintainer drives in-game checks.

## Working discipline (large multi-file refactor)

- **Edit strictly sequentially: Read → Edit → verify → next file.** Don't batch Read/Edit/build/commit
  across many files in one shot — partial application is hard to unwind in files this size.
- **Ground every claim in actual tool output.** Quote the real lines a conclusion rests on; don't
  paraphrase counts or build results from memory. Re-read after large edits to confirm.
- **Verify include/line context and macro forms** before swaps. Wrapper macros differ: `SF_HOLOGRAM_LOG`
  hardcodes its category; `SF_*_DIAGNOSTIC_LOG` forwards a category arg. `SmartFoundations.h` includes
  `SFLogMacros.h`, so the per-feature `LogSmart*` categories resolve module-wide.
- **Conventions:** types use `SF`/`USF`/`ASF`/`FSF` prefixes; no emojis in code/docs; commits end with
  `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`. `.vscode/settings.json` stays blank in-repo.
- **Reusable tooling:** `scripts/migrate_log_category.py` (guarded log-category swapper),
  `scripts/gen_size_registry.py` / `extract_size_registry.py` (T3), `scripts/smoke_test.py`.

## Smaller follow-ups available (SAFE-NOW, solo with compile-validate)

- **T7 tail:** `SFFactoryHologram.cpp` (hardcoded `SF_HOLOGRAM_LOG` wrapper), `SFRecipeManagementService`,
  `SFRadarPulseService` still on the catch-all `LogSmartFoundations`.
- **T4 tail — PC-lookup helper:** ~28 `GetFirstPlayerController()` / `GetPlayerController(World,0)` sites
  in 3 idioms → `SFPlayerHelpers::GetFGPlayerController(World)`. Most sites live in the T1 files, so
  better folded into T1 than touched twice.

After T1, remaining epics: **T5** (thin UI widgets), **T8** (split hologram logic), **T6** (service-context DI).
See `Simplification-RemainingWork.md` for details on each.
