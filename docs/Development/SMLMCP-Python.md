# Inspecting the editor with SMLMCP Python

SMLMCP exposes a `smlmcp_execute_python` tool that runs Python **inside the live Unreal
editor** (the editor-side Python plugin must be enabled). It captures `stdout`/`stderr`/errors
and an optional `__result__`. This is read-mostly introspection of the running editor — useful
for understanding assets (widgets, fonts, buildables, localization) without a build.

Pair it with `smlmcp_status` first to confirm the editor API is reachable.

> The editor process is separate from the running game. It has the FactoryGame **content**
> loaded but, in this SML modding project, only the **English** game localization — see
> "Localization gotcha" below.

---

## 1. Widget Blueprint introspection (the important one)

Goal: read the real geometry of a UMG panel (positions, sizes, parents, fonts) so layout fixes
aren't guesswork.

**What does NOT work** — these properties are protected/unexposed and raise
`Failed to find property ...` or `... is protected and cannot be read`:

```python
wb.get_editor_property("widget_tree")          # on UWidgetBlueprint
wb.get_editor_property("generated_class")       # on UWidgetBlueprint
cdo.get_editor_property("widget_tree")          # on the generated-class CDO
tree.get_editor_property("root_widget")         # on UWidgetTree
tree.get_all_widgets()                          # not bound to Python
```

**What DOES work** — reach the `WidgetTree` and each named widget by **object path**. Widgets
authored in the designer are subobjects of the blueprint's `WidgetTree`:

```python
import unreal
WB = "/SmartFoundations/SmartFoundations/UI/Smart_SettingsForm_Widget.Smart_SettingsForm_Widget"

tree = unreal.load_object(None, WB + ":WidgetTree")            # note the ':' subobject separator
btn  = unreal.load_object(None, WB + ":WidgetTree.CloseButton")  # any widget by its designer name
```

From a widget you can read class, parent, slot, and (for panels) children:

```python
def dump(w):
    s = w.slot
    geo = ""
    if isinstance(s, unreal.CanvasPanelSlot):
        ld = s.get_editor_property("layout_data")
        o  = ld.get_editor_property("offsets")     # Left/Top = position, Right/Bottom = size
        a  = ld.get_editor_property("anchors")      # minimum/maximum (x,y)
        al = ld.get_editor_property("alignment")    # (x,y)
        auto = s.get_editor_property("auto_size")
        geo = " pos(%.0f,%.0f) size(%.0f,%.0f) anchMin(%.2f,%.2f) align(%.1f,%.1f) auto=%s" % (
            o.left, o.top, o.right, o.bottom, a.minimum.x, a.minimum.y, al.x, al.y, auto)
    par = w.get_parent()
    print("%s [%s] parent=%s%s" % (w.get_name(), w.get_class().get_name(),
                                   par.get_name() if par else "None", geo))

# Panels: walk children (RootWidget is protected, so start from a known panel by path)
container = unreal.load_object(None, WB + ":WidgetTree.ContentContainer")
for i in range(container.get_children_count()):
    dump(container.get_child_at(i))
```

Notes:
- A `CanvasPanelSlot`'s `layout_data.offsets`: `.left/.top` are the position, `.right/.bottom`
  are the size **when the slot is not anchor-stretched** (anchors min == max). Watch for
  oversized background borders (e.g. a 953-px translucent `Border`) that are not the visible
  panel width.
- `TextBlock`: `w.get_editor_property("text")`, `w.get_editor_property("font").size`.
- `ComboBoxString`: `w.get_font()` (the `Font` property is deprecated/construction-only; set it
  in C++ with `InitFont`). Its asset default here was engine Roboto size 8.
- `get_children_count()` / `get_child_at(i)` work on any `PanelWidget`.

---

## 2. Buildable / item display names

Read a buildable's localized display name from its class default object:

```python
cls = unreal.load_object(None, "/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk1/"
                               "Build_ConveyorBeltMk1.Build_ConveyorBeltMk1_C")
cdo = unreal.get_default_object(cls)
print(str(cdo.get_editor_property("m_display_name")))   # CSS props map mDisplayName -> m_display_name
```

## 3. Culture switching (and why it didn't help here)

```python
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(w, "CULTURE=de")
# ... read display names ...
unreal.SystemLibrary.execute_console_command(w, "CULTURE=en")   # always restore
```

**Localization gotcha:** in this SML modding project the editor ships only the English game
localization (`Content/Localization/Game` is source/en only), so switching culture still returns
English. For authoritative translated game terms, extract the shipped game's locres instead
(next section).

## 4. Running external tools (UnrealPak) via subprocess

The editor Python can shell out. This bypasses the Bash tool entirely (handy when that path is
unavailable) and runs from the editor process:

```python
import subprocess, os, tempfile
unrealpak = r"C:\Program Files\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealPak.exe"
pak = r"G:\SteamLibrary\steamapps\common\Satisfactory\FactoryGame\Content\Paks\FactoryGame-Windows.pak"
dest = os.path.join(tempfile.gettempdir(), "sf_game_loc")
p = subprocess.run([unrealpak, pak, "-Extract", dest, "-Filter=*AllStringTables*"],
                   capture_output=True, text=True, timeout=300)
print(p.returncode, p.stdout[-500:])
```

`UnrealPak <pak> -list` (filtered) shows what's inside. Satisfactory's building/item names live in
the **`AllStringTables`** localization target (all cultures), not `Game.locres` (en-US only).

## 5. Parsing a `.locres` in Python

See `scripts/loc_term_audit.py` for the full parser. Format summary (UE 5.3, version 3
`Optimized_CityHash64_UTF16`): 16-byte magic GUID, 1 version byte, int64 offset to a localized-
string array, then namespaces -> keys -> index into that array. FStrings use a signed length:
negative = UTF-16 (chars = -len), positive = UTF-8 bytes; both include the null terminator.

---

## Related scripts

- `scripts/loc_term_audit.py` — diff Smart translations vs official game terms (locres parser).
- `scripts/loc_term_fix.py` — apply official game terms to the `.po` files.
- `scripts/loc_validate.py` — per-language completeness + placeholder integrity.
- `scripts/loc_parity_check.py` — key/empty parity across languages.
