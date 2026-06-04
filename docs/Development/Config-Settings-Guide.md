# Adding / Changing a Smart! Config Setting

A practical runbook for the Mods → Smart! configuration menu. Following the order below
keeps the six sync points in lockstep and avoids the two classic failure modes:

- **Empty config on cook** — a purely Blueprint-authored config tree gets stripped during
  cooking, so the shipped asset is empty and every setting falls back to its default. The
  C++ archetype (`USmartFoundationsModConfiguration`) exists specifically to prevent this:
  it builds the section tree as default sub-objects so the cooked Blueprint serializes
  against a real archetype.
- **Silent fallback to defaults** — if a leaf key drifts between the constructor, the nested
  mirror struct, and the copy-down, `FillConfigurationStruct` can't match it and that
  setting reads its zero/default value with no error.

## The six sync points

Every setting (a "leaf key", e.g. `bShowHUD`) must appear, spelled identically, in all of:

| # | File | What to add |
|---|------|-------------|
| A | `Source/SmartFoundations/Private/Config/SmartFoundationsModConfiguration.cpp` | `Section->SectionProperties.Add(TEXT("Key"), CreateXProperty(TEXT("Key"), DisplayName, Tooltip, Default));` in the right section block |
| B | `Source/SmartFoundations/Public/Config/Smart_ConfigStruct.h` → nested `FSmart_<Section>ConfigSection` | mirror field with matching type |
| C | `Smart_ConfigStruct.h` → `GetActiveConfig()` | copy-down line `ConfigStruct.Key = Sections.<Section>.Key;` |
| D | `Smart_ConfigStruct.h` → flat `FSmart_ConfigStruct` | flat field (consumers read this) |
| E | `Smart_Config` Blueprint (editor) | re-skinned `BP_ConfigProperty*` for the same key — gives the menu its widget |
| F | `Smart_ConfigStruct` Blueprint struct asset (editor) | matching key |

Points **A–D are plain C++** and are verified statically by `Scripts/config_parity_check.py`.
Points **E–F live in the editor** and are verified with AdaMCP (see below).

> Field-name rule: the leaf key name must be **identical** across A–F. Section keys
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
7. **E/F — editor**: open the editor, mirror the key in the `Smart_Config` Blueprint
   (re-skin with the matching `BP_ConfigProperty*`) and the `Smart_ConfigStruct` BP asset.
8. **Localization**: add the new `LOCTEXT` keys to the loc source and run the loc validators
   (`Scripts/loc_validate.py`, `Scripts/loc_parity_check.py`).
9. **Compile** C++ (Live Coding via AdaMCP `compile`, or a normal build).
10. **Cook once** and verify in-game (see checklist).

Removing a setting: delete it from A–F (and the localization keys) and re-run the static check.

## Verifying with AdaMCP (editor-side, points E/F)

With the editor open and the AdaMCP bridge up (`http://localhost:8080`):

- `get_class_defaults /SmartFoundations/SmartFoundations/Config/Smart_Config.Smart_Config`
  — confirms the Blueprint CDO carries the section tree.
- `validate_mod SmartFoundations` — confirms config/settings assets are present and loadable.
- The cooked asset should be ~15.6 KB with the section tree intact; a near-empty asset is the
  cook-stripping symptom above.

## One-cook verification checklist

To avoid burning multiple cooks, confirm all of this in a single in-game pass:

- [ ] Section header renders a clean single title (no empty `" (Title)"` / no duplication).
- [ ] Every property is listed under its section.
- [ ] Tooltips show on hover; dropdowns/sliders work (e.g. Belt Routing Mode, HUD Theme, HUD Scale).
- [ ] The new setting's default matches what you set in the constructor.
- [ ] Changing it takes effect (live, or after the documented reload).

## Section-header binding note (resolved)

SML's `BP_ConfigPropertySection` widget composes its title as `"{HeaderText} ({DisplayName})"`.
An empty `HeaderText` yields a blank label (it does **not** fall back to `DisplayName`). The
working configuration is **`HeaderText` = the title, `DisplayName` = empty**, which renders a
clean single title. Keep that pattern when adding new sections.
