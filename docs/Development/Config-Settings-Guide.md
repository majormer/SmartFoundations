# Adding / Changing a Smart! Config Setting

A practical runbook for the Mods → Smart! configuration menu. Following the order below
keeps the five active sync points in lockstep and avoids the two classic failure modes:

- **Empty config on cook** — a purely Blueprint-authored config tree gets stripped during
  cooking, so the shipped asset is empty and every setting falls back to its default. The
  C++ archetype (`USmartFoundationsModConfiguration`) exists specifically to prevent this:
  it builds the section tree as default sub-objects so the cooked Blueprint serializes
  against a real archetype.
- **Silent fallback to defaults** — if a leaf key drifts between the constructor, the nested
  mirror struct, and the copy-down, `FillConfigurationStruct` can't match it and that
  setting reads its zero/default value with no error.

## The five active sync points

Every setting (a "leaf key", e.g. `bShowHUD`) must appear, spelled identically, in all of:

| # | File | What to add |
|---|------|-------------|
| A | `Source/SmartFoundations/Private/Config/SmartFoundationsModConfiguration.cpp` | `Section->SectionProperties.Add(TEXT("Key"), CreateXProperty(TEXT("Key"), DisplayName, Tooltip, Default));` in the right section block |
| B | `Source/SmartFoundations/Public/Config/Smart_ConfigStruct.h` → nested `FSmart_<Section>ConfigSection` | mirror field with matching type |
| C | `Smart_ConfigStruct.h` → `GetActiveConfig()` | copy-down line `ConfigStruct.Key = Sections.<Section>.Key;` |
| D | `Smart_ConfigStruct.h` → flat `FSmart_ConfigStruct` | flat field (consumers read this) |
| E | `Smart_Config` Blueprint (editor) | re-skinned `BP_ConfigProperty*` for the same key — gives the menu its widget |

Points **A–D are plain C++** and are verified statically by `Scripts/config_parity_check.py`.
Point **E lives in the editor** and is verified there (see below).

The legacy `Smart_ConfigStruct` user-defined Blueprint struct is not the runtime mirror.
`GetActiveConfig()` passes the native `FSmart_ConfigStruct_Sections::StaticStruct()` to SML and
copies into native `FSmart_ConfigStruct`. The legacy asset has no package referencers or source
loads in the current tree. Do not mutate it merely to add an unused sixth sync point; if a new
consumer deliberately uses it, that consumer must explicitly own its schema synchronization.

> Field-name rule: the leaf key name must be **identical** across A–E. Section keys
> (`BeltAutoConnect`, `HUD`, …) likewise must match between `CreateSection`,
> `RootSection->SectionProperties.Add`, and the `FSmart_ConfigStruct_Sections` field name.

## Step-by-step: add one setting

1. **Pick the section** (e.g. HUD) and a leaf key + type (bool / int32 / float).
2. **A — constructor**: add the `SectionProperties.Add(TEXT("Key"), Create…Property(…))` line
   to that section's block. Add a `LOCTEXT` display name + tooltip.
3. **B — nested sub-struct**: add the field to `FSmart_<Section>ConfigSection`.
4. **D — flat struct**: add the field to `FSmart_ConfigStruct` (this is what gameplay code reads).
5. **C — copy-down**: add `ConfigStruct.Key = Sections.<Section>.Key;` in `GetActiveConfig()`.
6. **Run the static check** (no cook needed):
   ```
   python Scripts/config_parity_check.py
   ```
   Fix any DRIFT before going further.
7. **E — editor**: mirror the key in the `Smart_Config` Blueprint using the matching
   `BP_ConfigProperty*`. Back up the asset first; edit the instantiated section/property objects
   and save the loaded asset without compiling the Blueprint. Class-Defaults section edits can
   compile away the instanced tree, leaving null sections. Copy the widget type from an existing
   matching property and set `bRequiresWorldReload=false` on both the section and the new leaf.
8. **Localization**: add the new `LOCTEXT` keys to the loc source and run the loc validators
   (`Scripts/loc_validate.py`, `Scripts/loc_parity_check.py`).
9. **Compile** C++ (Live Coding, or a normal build).
10. **Cook once** and verify in-game (see checklist).

Removing a setting: delete it from A–E (and the localization keys) and re-run the static check.

## Verifying in the editor (editor-side, point E)

With the editor open:

- `get_class_defaults /SmartFoundations/SmartFoundations/Config/Smart_Config.Smart_Config`
  — confirms the Blueprint CDO carries the section tree.
- `validate_mod SmartFoundations` — confirms config/settings assets are present and loadable.
- Check every expected section and leaf is non-null, correctly named, and has the expected
  default and widget type. Compare against the pre-edit asset; size alone is not a schema check.

## One-cook verification checklist

To avoid burning multiple cooks, confirm all of this in a single in-game pass:

- [ ] Section header renders a clean single title (no empty `" (Title)"` / no duplication).
- [ ] Every property is listed under its section.
- [ ] Tooltips show on hover; dropdowns/sliders work (e.g. Belt Routing Mode, HUD Theme, HUD Scale).
- [ ] The new setting's default matches what you set in the constructor.
- [ ] Changing it takes effect at its next use without reloading the world. Reload flags remain false.

## Section-header binding note (resolved)

SML's `BP_ConfigPropertySection` widget composes its title as `"{HeaderText} ({DisplayName})"`.
An empty `HeaderText` yields a blank label (it does **not** fall back to `DisplayName`). The
working configuration is **`HeaderText` = the title, `DisplayName` = empty**, which renders a
clean single title. Keep that pattern when adding new sections.
