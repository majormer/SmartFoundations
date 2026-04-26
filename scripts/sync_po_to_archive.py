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
LANGS = ["de","es","fr","it","ja","ko","pl","pt-BR","ru","zh-Hans","zh-Hant","tr","bg","hu","no","uk","vi"]

def parse_po(path):
    """Parse .po file into dict of {key: msgstr}."""
    translations = {}
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    
    # Match msgctxt + msgid + msgstr blocks
    pattern = r'msgctxt\s+"([^"]+)"\s+msgid\s+"([^"]*?)"\s+msgstr\s+"([^"]*?)"'
    for match in re.finditer(pattern, content):
        ctx = match.group(1)
        msgid = match.group(2)
        msgstr = match.group(3)
        
        # Extract key from context "SmartFoundations,KeyName"
        if "," in ctx:
            key = ctx.split(",", 1)[1]
        else:
            key = ctx
        
        if msgstr and msgstr != msgid:  # Only if actually translated
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
    
    for sub in parsed.get("Subnamespaces", []):
        for child in sub.get("Children", []):
            key = child.get("Key", "")
            if key in translations:
                current_trans = child.get("Translation", {}).get("Text", "")
                new_trans = translations[key]
                src = child.get("Source", {}).get("Text", "")
                
                # Update if translation is currently the source text (untranslated)
                # or if it's empty
                if current_trans == src or current_trans == "" or current_trans != new_trans:
                    if current_trans != new_trans:
                        child["Translation"]["Text"] = new_trans
                        updated += 1
    
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
