# Localization pipeline

Smart! and Smart Camera have separate Unreal localization targets. Neither mod needs the
other installed to load its translations. Both use the culture list and protected-token
policy in `scripts/localization/policy.json` when preparing a coordinated release.

## Source text and terminology

Use `LOCTEXT`, `NSLOCTEXT`, or localized asset `FText` for player-facing text. Display
building and item names through their native localized descriptor text. Surround those names
with localized format strings, rather than translating an English object name independently.
Internal identifiers, paths, diagnostics, user-authored names and close-button glyphs are not
translation candidates.

Trace text through its display site as well as its declaration. HUD summary helpers and
validation results may return `FString`, but player-facing messages must originate from
localized `FText` before conversion. Raw English failure reasons must not bypass localization
through panel status, notifications or on-screen messages. Preserve unresolved asset IDs
as diagnostic arguments; use the native display name when the object can be resolved.

Keep dropdown state independent of its displayed caption. Routing/direction handlers must
resolve the option index, and tier selectors must resolve their populated tier values;
never parse English words such as `Auto`, `Backward`, or `Curve` from translated captions.
Fallback summaries also need localized format strings and native buildable display names.

Keep **Smart!**, **Smart Camera**, bare **Smart**, and **HUD** unchanged. Translate surrounding descriptions
and control labels. Do not assume old translations are correct merely because they exist.
Preserve the runtime multilingual fonts and test shaping, fallback glyphs and wrapping.

`game-terms` reads extracted official `AllStringTables.locres` files and matches namespace/key
identities to game string-table CSVs. Its output records the game version, resource hashes,
culture mapping and exact localized values. Supply the matching installed game's data;
missing resources are reported, never replaced by invented terminology. Game resources and
generated terminology evidence belong in an ignored workspace, not in the published wiki.

`term-bindings.json` binds unambiguous whole-label Smart identities to official game identities.
Phrase-level mentions, pluralization and ambiguous names require contextual review. A noun
substring replacement is not a safe way to translate instructions.

## Gather, reconcile and compile

The shared entry point is `scripts/localization/refresh.ps1`. Supply `Root`, `Target`,
`ProjectFile`, `UnrealEditorCmd`, `Python`, and a fresh `ReportDirectory`. Target is
`SmartFoundations` or `SmartCamera`. It refuses to run alongside a game, editor or build.
It gathers source and assets, reconciles PO files, validates/synchronizes archives, then
compiles resources. It does not package, install or publish either mod.

The report directory contains the original catalogs, full commandlet logs and a cumulative
translation recovery history. Use `ResumeAfterGather` only for the same successfully gathered
source after a later step failed. Do not gather again merely because an observer failed.

Reconciliation uses full **namespace and key**, not a bare key. Source changes, malformed or
fuzzy entries, removed identities and unsafe translations are retained in recovery history.
Current catalogs receive an empty translation until those entries are reviewed. Empty
translations also clear old archive values, preventing a stale compiled translation from
surviving after its source changed. Synchronization checks each PO identity against the
current manifest before updating an archive's source text. This also repairs archives that
still contain an older source for the same key; otherwise a valid new translation would
silently compile as English fallback. Missing archive identities require reconciliation
rather than an inferred source. File writes use atomic replacement.

`scripts/compile_localization.ps1`, `sync_po_to_archive.py` and `export_new_keys_to_po.py`
delegate to the shared pipeline for existing callers. Gather precedes reconciliation;
reconciliation precedes synchronization and compilation.

Run `python scripts/localization/manage.py --help` for individual commands:

- `audit`: current manifest coverage, source locations, malformed/stale/duplicate entries,
  protected tokens, format arguments, markup and literal-source review candidates.
- `reconcile`: align PO identities with the manifest, preserving recovery history.
- `sync`: import only source-matched, validated translations into archives.
- `reuse`: apply explicit official-term bindings and unambiguous source-identical prior
  translations. The report records every replacement and its origin.
- `game-terms`: extract the versioned official terminology reference.
- `export-review`: create per-language bundles with every current source string and its
  locations. Existing translations are included without being marked reviewed automatically.
- `import-review`: accept only entries explicitly marked reviewed, with a matching source
  hash, valid formatting and preserved branding. A rejected batch does not modify the catalog.

Unreal's `bValidateFormatPatterns` remains enabled. Passing mechanical validation does not
prove translation accuracy, appropriate grammar or visual usability. A literal-review hit
is a review candidate, not necessarily a player-facing defect.

Configuration asset labels, tooltips and section headers must be localizable `FText` with
stable namespace/key identities. Text can look correct in English while being excluded
from the gather entirely: editor Python string-to-`Text` conversion produces culture-invariant
text. Verify the stored fields with `TextLibrary.text_is_culture_invariant` and confirm their
identities appear in the gathered manifest. Preserve property defaults and behavior when
repairing text metadata; do not regenerate configuration objects merely to change labels.

## Wiki authoring and translation

Edit English Markdown in `wiki/`. `wiki-extract` generates `Localization/wiki/source.json`
and per-culture translation catalogs. Units include headings, paragraphs, lists, tables,
captions and alternative image text. Identical blocks retain their identities when other
blocks are inserted. Changed blocks require new review; old translator work is retained.

Each translated unit has `text`, `source_hash`, and `status` (`draft` or `reviewed`). Set
`reviewed` only after language and terminology review. Run extraction after English edits;
the builder refuses an out-of-date source manifest. A translation with a stale source hash
cannot appear even in a draft preview.

Blocks that must remain verbatim, such as a list of contributor account names, use
reviewed source hashes in the wiki policy. Changing the source invalidates that exception.
Do not use a verbatim exception to hide untranslated prose.

`wiki-build --output <staging-directory>` generates locale-prefixed pages, language navigation
and a coverage report. English URLs remain stable. Internal page links stay in the selected
language. Section links fall back to the page until translated heading anchors are mapped.
Do not edit generated pages. By default, only reviewed translations appear; missing passages
fall back to English with a visible notice. `--include-drafts` creates an explicitly marked
review preview and must never be used for publication. It is not evidence of completed
translation coverage.

`scripts/localization/preview.ps1 -Pages <generated-pages> -Output <preview>` renders an
offline review with language/direction metadata. Supply `Images` and `CameraImages` to copy
the existing illustrations. Geometry screenshots can be shared between languages. Screens
whose purpose is to explain UI text require localized captures or an explicit English-UI
caption. Never hide missing screenshots or incomplete translation behind a completion badge.

## Acceptance

Run the focused tooling tests with `python -m unittest discover -s scripts/localization -v`.
Run `scripts/localization/verify_all.py --terms <official-terms.json> --compiled --report <report.json>`
to check current mod and wiki coverage, formatting, branding and every explicitly bound
official game label. A missing reference or mismatched source is a failure, not permission
to substitute a dictionary translation. Compiled resources must contain the same text as
the current catalogs; a successful compile alone is not sufficient. Contextual prose still
requires language review.
Then verify both compiled targets and inspect representative long-text, CJK and RTL screens,
including standalone Camera. Check font coverage, shaping, clipping, numbers, control labels,
tooltips, configuration and error/confirmation flows. Reuse existing gameplay evidence;
localization changes do not require replaying paid construction tests.

Before publication, resolve all required translation gaps or explicitly agree on fallbacks,
review contextual terminology, validate generated page/image links, and distinguish
language review from automated checks. Packaging/deployment and public release remain
separate operations.
