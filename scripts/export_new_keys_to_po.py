#!/usr/bin/env python3
"""
Append newly-gathered LOCTEXT keys into each culture's .po so translators see them.

UE's Gather writes new keys into the .archive (English source) but NEVER into the
.po (the translator-facing master). sync_po_to_archive.py only goes .po -> .archive,
so brand-new keys would stay invisible to translators forever. This script bridges
that gap in the OTHER direction, additively:

  - reads the gathered native (en) .archive for every key + its English source text,
  - APPENDS any key missing from a culture's .po:
        en  -> msgstr = the English source (native reference)
        other -> msgstr = ""  (untranslated; translators fill it in)
  - existing .po entries are left completely untouched (purely additive).

Run AFTER the Gather step (so the en .archive holds the new keys). No recompile is
needed afterwards: the Gather already put the new keys into every .locres as English;
this only unlocks translation. Once a translator fills a msgstr, the normal
gather -> sync_po_to_archive -> compile pipeline ships it.
"""

import json
import os
import re

BASE = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "Content", "Localization", "SmartFoundations"))
NATIVE = "en"
# Native first so its English msgstr is written; the rest get empty msgstr.
LANGS = ["en", "de", "es", "fr", "it", "ja", "ko", "pl", "pt-BR", "ru", "tr",
         "zh-Hans", "zh-Hant", "bg", "hu", "no", "uk", "vi", "ar", "fa", "th"]


def read_archive(path):
    with open(path, "rb") as f:
        raw = f.read()
    if raw[:2] == b"\xff\xfe":
        data = raw.decode("utf-16-le")
    elif raw[:2] == b"\xfe\xff":
        data = raw.decode("utf-16-be")
    else:
        try:
            data = raw.decode("utf-16-le")
        except Exception:
            data = raw.decode("utf-8")
    if data.startswith("﻿"):
        data = data[1:]
    return json.loads(data)


def collect_keys(parsed):
    """Recursively collect {key: (namespace, source_text)} from an archive tree."""
    out = {}

    def walk(node, ns):
        cur = node.get("Namespace", ns)
        for child in node.get("Children", []):
            key = child.get("Key", "")
            src = child.get("Source", {}).get("Text", "")
            if key:
                out[key] = (cur, src)
        for sub in node.get("Subnamespaces", []):
            walk(sub, cur)

    walk(parsed, "")
    return out


def parse_po_keys(path):
    keys = set()
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    for m in re.finditer(r'msgctxt\s+"([^"]+)"', content):
        ctx = m.group(1)
        keys.add(ctx.split(",", 1)[1] if "," in ctx else ctx)
    return keys


def po_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def main():
    en_archive = os.path.join(BASE, NATIVE, "SmartFoundations.archive")
    if not os.path.exists(en_archive):
        print(f"ERROR: native archive not found: {en_archive}")
        print("Run the Gather step first (compile_localization.ps1).")
        return 1

    all_keys = collect_keys(read_archive(en_archive))  # {key: (ns, english_source)}
    print(f"=== Append missing gathered keys into .po ({len(all_keys)} keys in en.archive) ===\n")

    total = 0
    for lang in LANGS:
        po_path = os.path.join(BASE, lang, "SmartFoundations.po")
        if not os.path.exists(po_path):
            print(f"  {lang}: SKIP (no .po)")
            continue
        existing = parse_po_keys(po_path)
        missing = [(k, ns, src) for k, (ns, src) in all_keys.items() if k not in existing]
        if not missing:
            print(f"  {lang}: up to date")
            continue
        missing.sort()
        with open(po_path, "a", encoding="utf-8") as f:
            for key, ns, src in missing:
                ctx = f"{ns},{key}" if ns else key
                f.write("\n")
                f.write(f'msgctxt "{po_escape(ctx)}"\n')
                f.write(f'msgid "{po_escape(src)}"\n')
                f.write(f'msgstr "{po_escape(src)}"\n' if lang == NATIVE else 'msgstr ""\n')
        total += len(missing)
        print(f"  {lang}: appended {len(missing)} keys")

    print(f"\nDone! {total} key-entries appended across cultures (existing entries untouched).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
