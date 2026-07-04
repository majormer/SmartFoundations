#!/usr/bin/env python3
"""
Sync translations from .po files into .archive files.

UE's GenerateGatherArchive creates archive entries for new LOCTEXT keys
but does NOT import translations from .po files. This script bridges that gap
by reading .po translations and writing them directly into .archive files.

Run this AFTER the Gather step and BEFORE the Compile step.
"""

import json
import os
import re
import sys

BASE = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "Content", "Localization", "SmartFoundations"))
LANGS = ["de","es","fr","it","ja","ko","pl","pt-BR","ru","zh-Hans","zh-Hant","tr","bg","hu","no","uk","vi","ar","fa","th"]

def _po_unescape(s):
    r"""Turn .po escape sequences (\n, \t, \r, \", \\) into their literal chars.

    The .po stores an embedded quote as \" and a line break as \n; the archive
    (and the compiled .locres) must hold the real characters, so every captured
    value has to be unescaped before it is written back.
    """
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            nxt = s[i + 1]
            out.append({"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}.get(nxt, nxt))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def parse_po(path):
    """Parse .po file into dict of {key: msgstr}, with values unescaped."""
    translations = {}
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # Match msgctxt + msgid + msgstr blocks. The value captures allow escaped
    # characters ( \" \\ \n ) so a string that embeds a quote or newline -- e.g.
    # the HUD Module prompts *Module "{0}": ... and multi-line dialog text --
    # parses in full instead of truncating at the first inner quote.
    pattern = r'msgctxt\s+"((?:[^"\\]|\\.)*)"\s+msgid\s+"((?:[^"\\]|\\.)*)"\s+msgstr\s+"((?:[^"\\]|\\.)*)"'
    for match in re.finditer(pattern, content):
        ctx = _po_unescape(match.group(1))
        msgid = _po_unescape(match.group(2))
        msgstr = _po_unescape(match.group(3))

        # Extract key from context "SmartFoundations,KeyName"
        if "," in ctx:
            key = ctx.split(",", 1)[1]
        else:
            key = ctx

        if msgstr and msgstr != msgid:  # Only if actually translated (identical = source fallback)
            translations[key] = msgstr

    return translations

def update_archive(archive_path, translations):
    """Update .archive file with translations from .po."""
    with open(archive_path, "rb") as f:
        raw = f.read()
    
    # Detect encoding
    if raw[:2] == b'\xff\xfe':
        data = raw.decode("utf-16-le")
    elif raw[:2] == b'\xfe\xff':
        data = raw.decode("utf-16-be")
    else:
        # Try UTF-16 LE without BOM
        try:
            data = raw.decode("utf-16-le")
        except:
            data = raw.decode("utf-8")
    
    # Remove BOM
    if data.startswith('\ufeff'):
        data = data[1:]
    
    parsed = json.loads(data)
    updated = 0

    # Walk the WHOLE archive tree, not just one level of subnamespaces.
    # Auto-keyed asset/UDE FText strings live in the archive's ROOT Children
    # (namespace ""), which the old subnamespace-only loop never visited -- so
    # their .po translations were silently dropped and shipped as English.
    # Recurse from the root over both Children and Subnamespaces so every
    # translated key is applied wherever it sits in the tree.
    def walk(node):
        nonlocal updated
        for child in node.get("Children", []):
            key = child.get("Key", "")
            if key in translations:
                new_trans = translations[key]
                if child.get("Translation", {}).get("Text", "") != new_trans:
                    child.setdefault("Translation", {})["Text"] = new_trans
                    updated += 1
        for sub in node.get("Subnamespaces", []):
            walk(sub)

    walk(parsed)
    
    if updated > 0:
        # Write back as UTF-16 LE with BOM (matching UE's format)
        output = json.dumps(parsed, ensure_ascii=False, indent="\t")
        with open(archive_path, "wb") as f:
            f.write(b'\xff\xfe')
            f.write(output.encode("utf-16-le"))
    
    return updated

def main():
    print("=== Sync .po translations into .archive files ===\n")
    
    total = 0
    for lang in LANGS:
        po_path = os.path.join(BASE, lang, "SmartFoundations.po")
        archive_path = os.path.join(BASE, lang, "SmartFoundations.archive")
        
        if not os.path.exists(po_path):
            print(f"  {lang}: SKIP (no .po file)")
            continue
        if not os.path.exists(archive_path):
            print(f"  {lang}: SKIP (no .archive file)")
            continue
        
        translations = parse_po(po_path)
        updated = update_archive(archive_path, translations)
        total += updated
        print(f"  {lang}: {updated} translations synced ({len(translations)} in .po)")
    
    print(f"\nDone! {total} translations synced into .archive files.")
    print("Next: Run compile step to generate .locres files.")

if __name__ == "__main__":
    main()
