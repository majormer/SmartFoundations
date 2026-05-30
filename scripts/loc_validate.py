"""
Validate localization .po files before enabling a language.

Checks, per language:
  - completeness (translated / empty / missing vs the en source key set)
  - format-placeholder integrity: the set of {0}, {1}, ... tokens in each
    translation must match its English source (a mismatch fails UE's compile
    when bValidateFormatPatterns=true)
  - script sanity: flags translations that contain NO character in the
    language's expected Unicode block (possible mojibake / untranslated-but-nonempty)

Usage: python scripts/loc_validate.py [lang ...]   (default: ar fa th)
"""
import os, re, sys

BASE = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "Content", "Localization", "SmartFoundations"))

# Unicode ranges that a real translation in each script should contain at least one of.
SCRIPT_RANGES = {
    "ar": [(0x0600, 0x06FF), (0x0750, 0x077F), (0xFB50, 0xFDFF), (0xFE70, 0xFEFF)],  # Arabic
    "fa": [(0x0600, 0x06FF), (0x0750, 0x077F), (0xFB50, 0xFDFF), (0xFE70, 0xFEFF)],  # Persian (Arabic script)
    "th": [(0x0E00, 0x0E7F)],                                                         # Thai
}

PLACEHOLDER = re.compile(r'\{[^}]*\}')


def parse_po(path):
    txt = open(path, encoding="utf-8").read()
    out = {}
    for blk in txt.split("\n\n"):
        mc = re.search(r'msgctxt "(.*?)"', blk, re.S)
        mi = re.search(r'msgid "((?:[^"\\]|\\.)*)"', blk)
        ms = re.search(r'msgstr "((?:[^"\\]|\\.)*)"', blk)
        if mc and mi and ms:
            out[mc.group(1)] = (mi.group(1), ms.group(1))
    return out


def has_script(s, ranges):
    for ch in s:
        cp = ord(ch)
        for lo, hi in ranges:
            if lo <= cp <= hi:
                return True
    return False


def main():
    try: sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception: pass
    langs = sys.argv[1:] or ["ar", "fa", "th"]
    en = parse_po(os.path.join(BASE, "en", "SmartFoundations.po"))
    en_keys = set(en)

    for lang in langs:
        po = os.path.join(BASE, lang, "SmartFoundations.po")
        if not os.path.exists(po):
            print("\n### %s: NO .po" % lang); continue
        d = parse_po(po)
        keys = set(d)
        translated = [k for k, (mi, ms) in d.items() if ms.strip() and k != ""]
        empty = [k for k, (mi, ms) in d.items() if not ms.strip() and k != ""]
        missing = sorted(en_keys - keys)

        ranges = SCRIPT_RANGES.get(lang, [])
        bad_ph = []   # placeholder mismatches (compile-breaking)
        no_script = []  # nonempty translation with no native script char
        for k, (mi, ms) in d.items():
            if not ms.strip() or k == "":
                continue
            if set(PLACEHOLDER.findall(mi)) != set(PLACEHOLDER.findall(ms)):
                bad_ph.append((k, mi, ms))
            if ranges and not has_script(ms, ranges) and re.search(r'[A-Za-z]{3,}', ms):
                no_script.append((k, ms))

        print("\n### %s" % lang)
        print("  keys=%d  translated=%d  empty=%d  missing_vs_en=%d" %
              (len(keys), len(translated), len(empty), len(missing)))
        print("  placeholder mismatches (COMPILE-BREAKING): %d" % len(bad_ph))
        for k, mi, ms in bad_ph:
            print("     %s\n        en : %r\n        loc: %r" % (k, mi, ms))
        print("  nonempty-but-looks-untranslated (latin, no native script): %d" % len(no_script))
        for k, ms in no_script[:20]:
            print("     %s -> %r" % (k, ms))


if __name__ == "__main__":
    main()
