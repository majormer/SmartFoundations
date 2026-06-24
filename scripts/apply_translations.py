#!/usr/bin/env python3
"""
Apply per-key translations from a <dir>/<lang>.json into each culture .po.

Reads a flat {KEY: "translation"} JSON per culture (KEY may be the bare key or the
full "SmartFoundations,KEY" msgctxt; both are matched). By default ("fill" mode) it
only writes entries whose msgstr is currently empty. With --overwrite it also replaces
EXISTING translations for keys present in the JSON (used for context-correction passes
that fix already-translated strings). Keys absent from the JSON are never touched.

A placeholder-integrity warning is printed when a translation's {N} placeholders differ
from the English source's (the compile step's bValidateFormatPatterns is the hard gate).

Usage:
  python apply_translations.py                          # fill empties from .local/trans
  python apply_translations.py --dir .local/corrections --overwrite   # overwrite from corrections
"""

import argparse
import json
import os
import re

ROOT = os.path.dirname(__file__)
BASE = os.path.normpath(os.path.join(ROOT, "..", "Content", "Localization", "SmartFoundations"))
DEFAULT_DIR = os.path.join(ROOT, ".local", "trans")
LANGS = ["de", "es", "fr", "it", "ja", "ko", "pl", "pt-BR", "ru", "tr",
         "zh-Hans", "zh-Hant", "bg", "hu", "no", "uk", "vi", "ar", "fa", "th"]

PH = re.compile(r"\{\d+\}")
MSGSTR = re.compile(r'^msgstr "(.*)"\s*$')


def po_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def apply_lang(lang, trans_dir, overwrite):
    tpath = os.path.join(trans_dir, lang + ".json")
    popath = os.path.join(BASE, lang, "SmartFoundations.po")
    if not os.path.exists(tpath):
        return (lang, "NO JSON", 0, 0, 0, [])
    if not os.path.exists(popath):
        return (lang, "NO PO", 0, 0, 0, [])
    with open(tpath, "r", encoding="utf-8") as f:
        try:
            trans = json.load(f)
        except Exception as e:
            return (lang, f"BAD JSON: {e}", 0, 0, 0, [])
    if not isinstance(trans, dict):
        return (lang, "JSON not an object", 0, 0, 0, [])

    with open(popath, "r", encoding="utf-8") as f:
        lines = f.read().split("\n")

    out = []
    cur_ctx = cur_key = cur_msgid = None
    filled = overwritten = still_empty = 0
    warnings = []
    for line in lines:
        m_ctx = re.match(r'msgctxt\s+"([^"]+)"', line)
        if m_ctx:
            cur_ctx = m_ctx.group(1)
            cur_key = cur_ctx.split(",", 1)[1] if "," in cur_ctx else cur_ctx
            cur_msgid = None
            out.append(line)
            continue
        m_id = re.match(r'msgid\s+"(.*)"\s*$', line)
        if m_id and cur_key is not None:
            cur_msgid = m_id.group(1)
            out.append(line)
            continue
        m_str = MSGSTR.match(line)
        if m_str is not None and cur_key is not None:
            current = m_str.group(1)
            t = trans.get(cur_ctx)
            if t is None:
                t = trans.get(cur_key)
            do_set = (t is not None and t != "" and (overwrite or current == ""))
            if do_set:
                src_ph = sorted(PH.findall(cur_msgid or ""))
                tr_ph = sorted(PH.findall(t))
                if src_ph != tr_ph:
                    warnings.append(f"{cur_key}: placeholders src={src_ph} != trans={tr_ph}")
                out.append(f'msgstr "{po_escape(t)}"')
                if current == "":
                    filled += 1
                elif current != t:
                    overwritten += 1
            else:
                out.append(line)
                if current == "" and t is None:
                    still_empty += 1
            cur_ctx = cur_key = cur_msgid = None
            continue
        out.append(line)

    with open(popath, "w", encoding="utf-8") as f:
        f.write("\n".join(out))
    return (lang, "ok", filled, overwritten, still_empty, warnings)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=DEFAULT_DIR, help="directory of <lang>.json translation files")
    ap.add_argument("--overwrite", action="store_true", help="replace existing translations for keys in the JSON")
    args = ap.parse_args()
    tdir = args.dir if os.path.isabs(args.dir) else os.path.join(ROOT, args.dir)

    print(f"=== Apply translations from {tdir} (overwrite={args.overwrite}) ===\n")
    tot_fill = tot_over = tot_warn = 0
    for lang in LANGS:
        lang_, status, filled, overwritten, still_empty, warnings = apply_lang(lang, tdir, args.overwrite)
        tot_fill += filled
        tot_over += overwritten
        tot_warn += len(warnings)
        msg = f"  {lang_}: {status} | filled {filled}, overwritten {overwritten}, still-empty {still_empty}"
        if warnings:
            msg += f", {len(warnings)} placeholder WARNING(s)"
        print(msg)
        for w in warnings[:8]:
            print(f"      ! {w}")
    print(f"\nDone! {tot_fill} filled, {tot_over} overwritten, {tot_warn} placeholder warnings.")
    print("Next: sync_po_to_archive.py + compile (compile_localization.ps1), then re-cook to ship.")


if __name__ == "__main__":
    main()
