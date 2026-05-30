"""
Corrects Smart! family-label translations to the game's official building terms.

Scope (safe wholesale replacements only -- pure noun labels sourced from clean,
Mk-less game strings): the Family_* keys. For each target language we read the
official term from the shipped game's AllStringTables.locres and overwrite the
Smart .po msgstr ONLY when the game has a non-empty translation that differs from
what Smart currently has (so we never replace a localized term with English fallback,
and never touch already-correct cells).

Wall-outlet single/double keep Smart's own "(Single)/(Double)" qualifier (already
localized) and only swap the noun to the game's "Wall Outlets" category term.

NOT handled here (need translator/phrase-level care): Family_Pump (only Mk-suffixed
game source), Upgrade_CrossSplitters/Storage/Trains and Config_StackableBelt_TT
(noun embedded in a translated phrase).

Usage:  python scripts/loc_term_fix.py GAME_LOC_DIR [--apply]
Without --apply it's a dry run (prints the diff it would make).
"""
import struct, os, sys, re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SMART_LOC = os.path.join(REPO, "Content", "Localization", "SmartFoundations")
MAGIC = bytes([0x0E,0x14,0x74,0x75,0x67,0x4A,0x03,0xFC,0x4A,0x15,0x90,0x9D,0xC3,0x37,0x7F,0x1B])

LANG_MAP = {
    "ar":"ar","bg":"bg","de":"de","es":"es-ES","fa":"fa","fr":"fr","hu":"hu","it":"it",
    "ja":"ja","ko":"ko","no":"no","pl":"pl","pt-BR":"pt-BR","ru":"ru","th":"th","tr":"tr",
    "uk":"uk","vi":"vi","zh-Hans":"zh-Hans","zh-Hant":"zh-Hant",
}

# Authoritative game source per concept (namespace, key) -- all Mk-less.
SRC = {
    "ConveyorBelts": ("Player_UI", "BuildMenu/Tabs/Logistics/ConveyorBelts"),
    "ConveyorLifts": ("Player_UI", "BuildMenu/Tabs/Logistics/ConveyorLifts"),
    "Pipelines":     ("Player_UI", "BuildMenu/Tabs/Logistics/Pipelines"),
    "PowerTower":    ("Buildables_Data", "Power/PowerTower"),
    "PowerPole":     ("Buildables_UI", "Power/Pole/WindowTitle"),
    "WallOutlets":   ("Player_UI", "BuildMenu/Tabs/Power/WallOutlets"),
}

# Smart .po key -> (concept, mode)   mode: "whole" or "walloutlet" (keep parenthetical)
FIXES = {
    "SmartFoundations,Family_Belt":             ("ConveyorBelts", "whole"),
    "SmartFoundations,Family_Lift":             ("ConveyorLifts", "whole"),
    "SmartFoundations,Family_Pipe":             ("Pipelines",     "whole"),
    "SmartFoundations,Family_Tower":            ("PowerTower",     "whole"),
    "SmartFoundations,Family_PowerPole":        ("PowerPole",      "whole"),
    "SmartFoundations,Family_WallOutletSingle": ("WallOutlets",    "walloutlet"),
    "SmartFoundations,Family_WallOutletDouble": ("WallOutlets",    "walloutlet"),
}


def read_fstr(f):
    (length,) = struct.unpack('<i', f.read(4))
    if length == 0: return ""
    if length < 0: return f.read((-length)*2).decode('utf-16-le','replace')[:-1]
    return f.read(length).decode('utf-8','replace')[:-1]


def parse_locres(path):
    with open(path,'rb') as f:
        head=f.read(16); version=0
        if head==MAGIC: version=f.read(1)[0]
        else: f.seek(0)
        strings=[]
        if version>=2:
            (off,)=struct.unpack('<q',f.read(8)); cur=f.tell()
            if off!=-1:
                f.seek(off);(cnt,)=struct.unpack('<i',f.read(4))
                for _ in range(cnt):
                    s=read_fstr(f); struct.unpack('<i',f.read(4)); strings.append(s)
                f.seek(cur)
        if version>=1: struct.unpack('<I',f.read(4))
        (ns_cnt,)=struct.unpack('<I',f.read(4)); res={}
        for _ in range(ns_cnt):
            if version>=1: struct.unpack('<I',f.read(4))
            ns=read_fstr(f);(kc,)=struct.unpack('<I',f.read(4))
            for _ in range(kc):
                if version>=1: struct.unpack('<I',f.read(4))
                key=read_fstr(f); struct.unpack('<I',f.read(4))
                if version>=2:
                    (idx,)=struct.unpack('<i',f.read(4)); val=strings[idx] if 0<=idx<len(strings) else ""
                else: val=read_fstr(f)
                res[(ns,key)]=val
        return res


def po_get_msgstr(text, ctx):
    # returns (value, is_present)
    for blk in text.split("\n\n"):
        if re.search(r'^msgctxt "%s"$' % re.escape(ctx), blk, re.M):
            m = re.search(r'^msgstr "((?:[^"\\]|\\.)*)"$', blk, re.M)
            if m:
                return m.group(1)
    return None


def po_set_msgstr(text, ctx, new_val):
    esc = new_val.replace("\\", "\\\\").replace('"', '\\"')
    blocks = text.split("\n\n")
    for i, blk in enumerate(blocks):
        if re.search(r'^msgctxt "%s"$' % re.escape(ctx), blk, re.M):
            blocks[i] = re.sub(r'^msgstr "(?:[^"\\]|\\.)*"$',
                               'msgstr "%s"' % esc, blk, count=1, flags=re.M)
            return "\n\n".join(blocks)
    return text


def main():
    if len(sys.argv) < 2:
        print("usage: loc_term_fix.py GAME_LOC_DIR [--apply]"); sys.exit(2)
    game_loc = sys.argv[1]
    apply = "--apply" in sys.argv[2:]
    try: sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception: pass

    def gpath(c): return os.path.join(game_loc, c, "AllStringTables.locres")
    game = {}
    for smart_lang, culture in LANG_MAP.items():
        p = gpath(culture)
        if os.path.exists(p):
            game[smart_lang] = parse_locres(p)

    total = 0
    for smart_lang in sorted(LANG_MAP):
        po_path = os.path.join(SMART_LOC, smart_lang, "SmartFoundations.po")
        gl = game.get(smart_lang)
        if not gl or not os.path.exists(po_path):
            continue
        text = open(po_path, encoding="utf-8").read()
        changes = []
        for ctx, (concept, mode) in FIXES.items():
            official = gl.get(SRC[concept], "").strip()
            if not official:
                continue  # game has no translation here -> keep Smart's (don't force English)
            cur = po_get_msgstr(text, ctx)
            if cur is None:
                continue
            if mode == "walloutlet":
                # Keep Smart's own (Single)/(Double) qualifier; support ASCII "(..)" and
                # full-width CJK "（..）", preserving the original separator/spacing.
                paren = re.search(r'(\s*[\(（].*[\)）])\s*$', cur)
                new = official + (paren.group(1) if paren else "")
            else:
                new = official
            if new != cur:
                changes.append((ctx, cur, new))
                text = po_set_msgstr(text, ctx, new)
        if changes:
            print("\n[%s] %d change(s):" % (smart_lang, len(changes)))
            for ctx, old, new in changes:
                print("   %-42s %r -> %r" % (ctx.split(",")[1], old, new))
            total += len(changes)
            if apply:
                open(po_path, "w", encoding="utf-8", newline="\n").write(text)
    print("\n%s: %d total change(s) across languages." % ("APPLIED" if apply else "DRY RUN", total))


if __name__ == "__main__":
    main()
