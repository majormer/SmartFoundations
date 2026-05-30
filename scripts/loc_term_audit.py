"""
Game-term localization audit.

Finds Smart! translations of in-game nouns (buildings/buildables) that DON'T match
the game's official translation -- the "dictionary word instead of the game term" bug
class that DE/ES/PL players reported.

Ground truth = the shipped game's AllStringTables.locres (one per culture), which holds
every building/item/build-menu name. We extract those once with UnrealPak, parse them
here, and diff against Smart's own .po translations.

Usage:
    python scripts/loc_term_audit.py [GAME_LOC_DIR]

GAME_LOC_DIR defaults to the UnrealPak extraction created during this session:
    %TEMP%/sf_game_loc/FactoryGame/Content/Localization/AllStringTables
If that has been cleaned up, re-extract with:
    UnrealPak.exe "<...>/FactoryGame-Windows.pak" -Extract <dest> -Filter=*AllStringTables*

Output: prints a per-term, per-language table and writes scripts/loc_term_report.md.
"""
import struct, os, sys, re, tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SMART_LOC = os.path.join(REPO, "Content", "Localization", "SmartFoundations")
DEFAULT_GAME_LOC = os.path.join(tempfile.gettempdir(), "sf_game_loc",
                                "FactoryGame", "Content", "Localization", "AllStringTables")

MAGIC = bytes([0x0E,0x14,0x74,0x75,0x67,0x4A,0x03,0xFC,0x4A,0x15,0x90,0x9D,0xC3,0x37,0x7F,0x1B])

# Smart language code -> game culture code (game uses regioned variants for a couple)
LANG_MAP = {
    "ar":"ar","bg":"bg","de":"de","es":"es-ES","fa":"fa","fr":"fr","hu":"hu","it":"it",
    "ja":"ja","ko":"ko","no":"no","pl":"pl","pt-BR":"pt-BR","ru":"ru","th":"th","tr":"tr",
    "uk":"uk","vi":"vi","zh-Hans":"zh-Hans","zh-Hant":"zh-Hant",
}

# Smart .po msgctxt key -> the English game string(s) whose official translation it must match.
# We resolve the game translation by matching en-US value (handles plural/category names).
SMART_TERM_TO_GAME = {
    "SmartFoundations,Family_Belt":            ["Conveyor Belts"],
    "SmartFoundations,Family_Lift":            ["Conveyor Lifts"],
    "SmartFoundations,Family_Pipe":            ["Pipelines"],
    "SmartFoundations,Family_Pump":            ["Pipeline Pump Mk.1", "Pipeline Pump"],
    "SmartFoundations,Family_PowerPole":       ["Power Pole"],
    "SmartFoundations,Family_WallOutletSingle":["Wall Outlets", "Wall Outlet Mk.1"],
    "SmartFoundations,Family_WallOutletDouble":["Wall Outlets", "Wall Outlet Mk.1"],
    "SmartFoundations,Family_Tower":           ["Power Tower"],
    "SmartFoundations,Upgrade_CrossSplitters": ["Conveyor Splitter", "Conveyor Merger"],
    "SmartFoundations,Upgrade_CrossStorage":   ["Storage Container"],
    "SmartFoundations,Upgrade_CrossTrains":    ["Freight Platform"],
    "SmartFoundations,Config_StackableBelt_TT":["Stackable Conveyor Pole"],
}


def read_fstr(f):
    (length,) = struct.unpack('<i', f.read(4))
    if length == 0:
        return ""
    if length < 0:
        return f.read((-length) * 2).decode('utf-16-le', 'replace')[:-1]
    return f.read(length).decode('utf-8', 'replace')[:-1]


def parse_locres(path):
    with open(path, 'rb') as f:
        head = f.read(16); version = 0
        if head == MAGIC:
            version = f.read(1)[0]
        else:
            f.seek(0)
        strings = []
        if version >= 2:
            (off,) = struct.unpack('<q', f.read(8)); cur = f.tell()
            if off != -1:
                f.seek(off); (cnt,) = struct.unpack('<i', f.read(4))
                for _ in range(cnt):
                    s = read_fstr(f); struct.unpack('<i', f.read(4)); strings.append(s)
                f.seek(cur)
        if version >= 1:
            struct.unpack('<I', f.read(4))
        (ns_cnt,) = struct.unpack('<I', f.read(4)); res = {}
        for _ in range(ns_cnt):
            if version >= 1:
                struct.unpack('<I', f.read(4))
            ns = read_fstr(f); (kc,) = struct.unpack('<I', f.read(4))
            for _ in range(kc):
                if version >= 1:
                    struct.unpack('<I', f.read(4))
                key = read_fstr(f); struct.unpack('<I', f.read(4))
                if version >= 2:
                    (idx,) = struct.unpack('<i', f.read(4))
                    val = strings[idx] if 0 <= idx < len(strings) else ""
                else:
                    val = read_fstr(f)
                res[(ns, key)] = val
        return res


def parse_po(path):
    txt = open(path, encoding='utf-8').read()
    out = {}
    for blk in txt.split('\n\n'):
        mc = re.search(r'msgctxt "(.*?)"', blk, re.S)
        ms = re.search(r'msgstr "((?:[^"\\]|\\.)*)"', blk)
        if mc and ms:
            out[mc.group(1)] = ms.group(1)
    return out


def norm(s):
    return re.sub(r'[^\w]', '', s.casefold())


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # console may be cp1252
    except Exception:
        pass
    game_loc = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_GAME_LOC
    if not os.path.isdir(game_loc):
        print("Game loc dir not found: %s\nRe-extract AllStringTables with UnrealPak first." % game_loc)
        sys.exit(2)

    # Parse game en-US, build English-value -> set of translations per language
    def gpath(culture):
        return os.path.join(game_loc, culture, "AllStringTables.locres")

    en_game = parse_locres(gpath("en-US"))
    # English value -> list of (ns,key)
    en_index = {}
    for k, v in en_game.items():
        en_index.setdefault(v.strip(), []).append(k)

    # Parse all target languages' game locres
    game_by_lang = {}
    for smart_lang, culture in LANG_MAP.items():
        p = gpath(culture)
        if os.path.exists(p):
            game_by_lang[smart_lang] = parse_locres(p)

    # For each game English term, official translation per lang = translation of its first key
    def official(term, smart_lang):
        keys = en_index.get(term, [])
        gl = game_by_lang.get(smart_lang)
        if not gl:
            return None
        for k in keys:
            if k in gl and gl[k].strip():
                return gl[k]
        return None

    report = ["# Game-term localization audit",
              "",
              "Flags Smart translations whose rendering of an in-game noun may not match the",
              "game's official term. `MATCH` = Smart's translation contains the game's official",
              "word(s); `CHECK` = it does not (likely the confusing-term bug). Always eyeball both.",
              ""]
    summary = []
    smart_en = parse_po(os.path.join(SMART_LOC, "en", "SmartFoundations.po"))

    for smart_key, game_terms in SMART_TERM_TO_GAME.items():
        en_src = smart_en.get(smart_key, "")
        report.append("\n## `%s`  (en: %r)" % (smart_key, en_src))
        report.append("game term(s): %s\n" % ", ".join(game_terms))
        report.append("| lang | Smart translation | game official | flag |")
        report.append("|---|---|---|---|")
        for smart_lang in sorted(LANG_MAP):
            po = os.path.join(SMART_LOC, smart_lang, "SmartFoundations.po")
            if not os.path.exists(po):
                continue
            smart_tr = parse_po(po).get(smart_key, "")
            offs = [official(t, smart_lang) for t in game_terms]
            offs = [o for o in offs if o]
            off_disp = " / ".join(dict.fromkeys(offs)) if offs else "(none)"
            # heuristic match: any official word-token present in smart translation
            flag = "CHECK"
            if not smart_tr:
                flag = "empty"
            elif offs:
                ns = norm(smart_tr)
                hit = False
                for o in offs:
                    # strip "Mk.x" and split into tokens
                    core = re.sub(r'\bMk\.?\s*\d+\b', '', o, flags=re.I)
                    toks = [norm(t) for t in re.split(r'[\s/]+', core) if len(t) >= 3]
                    if toks and all(t in ns for t in toks):
                        hit = True; break
                    if toks and any(t in ns for t in toks):
                        hit = "partial"
                flag = "MATCH" if hit is True else ("partial" if hit == "partial" else "CHECK")
            else:
                flag = "no-gt"
            if flag in ("CHECK", "partial"):
                summary.append((smart_key, smart_lang, smart_tr, off_disp, flag))
            report.append("| %s | %s | %s | %s |" % (smart_lang, smart_tr or "(empty)", off_disp, flag))

    report.append("\n\n## Likely mismatches (CHECK / partial)\n")
    report.append("| key | lang | Smart | game official | flag |")
    report.append("|---|---|---|---|---|")
    for s in summary:
        report.append("| %s | %s | %s | %s | %s |" % s)

    out_path = os.path.join(REPO, "scripts", "loc_term_report.md")
    open(out_path, "w", encoding="utf-8").write("\n".join(report))
    print("Wrote %s" % out_path)
    print("Languages with game ground truth: %s" % sorted(game_by_lang))
    print("Total flagged (CHECK/partial): %d" % len(summary))
    # Print a compact preview
    for s in summary[:40]:
        print("  [%s] %s: Smart=%r  game=%r  (%s)" % (s[4], s[1], s[2], s[3], s[0]))


if __name__ == "__main__":
    main()
