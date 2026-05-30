# ADR — T3: Buildable Size Registry format

Status: **Proposed** (awaiting the collaborative T3 session: build-validate + in-game smoke).
Charter: [`Simplification-GOAL.md`](Simplification-GOAL.md) · Tracker: [`SimplificationAudit.md`](SimplificationAudit.md)

## Context

Building footprint sizes live in **14 `SFBuildableSizeRegistry_*.cpp`** files (~5,500 lines) that
do nothing but call `RegisterProfile(...)` once per buildable from `RegisterDefaultProfiles()`.
The data is pure and declarative; the 14-file split exists only because the list grew too long
for one file. Each row is:

```cpp
RegisterProfile(
    TEXT("Build_Foundation_8x4_01_C"),   // class name (key)
    FVector(800, 800, 400),              // size cm (X,Y,Z)
    false,                                // bSwapXYOnRotation
    true,                                 // bSupportsScaling
    TEXT("Holo_... -> FGHologram"),      // inheritance note (documentation only)
    true,                                 // bValidated
    FVector::ZeroVector);                 // AnchorOffset (optional)
```

Consumers call `USFBuildableSizeRegistry::GetProfile/GetSizeForHologram/...`, backed by a
`TMap<FString, FSFBuildableSizeProfile> KnownProfiles` populated at `Initialize()`. Variant
inheritance (`ResolveVariantBaseName`) and CDO fallbacks (`TryGetSizeFromClearanceBox/MeshBounds`)
are **logic, not data** — they stay in C++ regardless.

## Decision

Move the **data** out of the 14 `.cpp` files into a single source of truth, keeping the public
API and all resolution logic unchanged. Two candidate formats were considered:

### Option A — UE DataTable (`.uasset` + a `FSFBuildableSizeRow : FTableRowBase`)
- Pros: native, editor-viewable/editable, designer-friendly, hot-reloadable.
- Cons: it's a **binary `.uasset`** — bad git diffs, the very "edit in the editor only" friction
  the charter wants to reduce; needs a content-asset created in-editor; another packaged asset.

### Option B — CSV in `Config/` parsed at `Initialize()` (CHOSEN)
- One `Config/BuildableSizes.csv` (or `.tsv`), columns:
  `ClassName,SizeX,SizeY,SizeZ,SwapXYOnRotation,SupportsScaling,Validated,AnchorX,AnchorY,AnchorZ`.
  (Inheritance note dropped from data — it was documentation only; preserve as an optional trailing
  comment column if desired.)
- `RegisterDefaultProfiles()` becomes: load the CSV (via `FFileHelper::LoadFileToStringArray` +
  manual split, or `FString::ParseIntoArray`), parse each row, call the existing private
  `RegisterProfile(...)`. The 14 `Register*()` methods and their files are deleted.
- Pros: **plain-text, great git diffs, one edit point** (the charter's "single edit point for
  building sizes" criterion), no editor needed, trivially scriptable. Generating the CSV from the
  current files is a mechanical transform (a `scripts/` extractor), making the migration verifiable
  by round-trip (parse CSV → compare to old registered set).
- Cons: parsed at runtime startup (negligible — it's ~hundreds of rows, once); CSV must ship in the
  packaged mod (add to staged `Config/`).

**Chosen: Option B (CSV).** It directly serves the charter's plain-text/single-edit-point goal and
keeps diffs reviewable, which a `.uasset` cannot.

## Migration plan (for the collaborative session)

1. **Extract** (build-free, scriptable): a `scripts/extract_size_registry.py` parses the 14
   `RegisterProfile(...)` calls → emits `Config/BuildableSizes.csv`. Print a count; assert it
   equals the number of `RegisterProfile` calls across the 14 files (no row lost).
2. **Loader**: implement CSV parsing in `RegisterDefaultProfiles()` (new small helper), behind the
   unchanged `Initialize()`. Keep `RegisterProfile` private signature as-is.
3. **Delete** the 14 `SFBuildableSizeRegistry_*.cpp` files and their `Register*()` declarations.
4. **Build-validate** (game closed): package compiles; the registry initializes.
5. **In-game smoke** (game open — the gate): place a foundation 8x4, an 8x1, a wall, a ramp, and a
   non-symmetric building (rotation-swap case); confirm grid spacing/scaling is unchanged vs the
   `refactor-baseline` behavior. `scripts/smoke_test.py` for readback.
6. Net: −~5,500 lines of `.cpp`, +1 CSV, building sizes now one editable file. Satisfies the
   "single edit point for building sizes" half of that success criterion.

## Risks & mitigations

- **Data-accuracy (primary risk):** a mis-parsed row = a building scales wrong. Mitigation: the
  extractor round-trips and counts; the smoke covers the common shapes incl. the rotation-swap path.
- **Parse robustness:** class names and floats only; reject malformed rows loudly at startup
  (log `LogSmartGrid` Error) rather than silently dropping — a dropped row falls back to 800³,
  which the smoke would catch for tested shapes but not obscure ones, so fail-loud matters.
- **Variant inheritance & CDO fallback unchanged** — they operate on the populated map, so they're
  format-agnostic. No behavior change there.

## Out of scope

The resolution logic (`ResolveVariantBaseName`, CDO queries, `GetRampUnitHeight`) stays in C++.
This ADR is data-format only.
