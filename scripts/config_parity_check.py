# Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#
# config_parity_check.py - static drift detector for the Smart! mod config.
#
# Adding/changing a config setting requires the same key to stay in lockstep across
# FOUR C++ sync points. Drift silently causes either the empty-config-on-cook bug or
# fallback-to-defaults (see comments in Smart_ConfigStruct.h / SmartFoundationsModConfiguration.h).
# This script parses all four and reports any mismatch BEFORE you cook, so you don't
# burn a packaging cycle discovering it in-game.
#
#   Sync point A: SmartFoundationsModConfiguration.cpp  ctor   (CreateSection + SectionProperties.Add)
#   Sync point B: Smart_ConfigStruct.h  nested FSmart_*ConfigSection mirror sub-structs
#   Sync point C: Smart_ConfigStruct.h  GetActiveConfig() copy-down (Sections.X.Y -> flat)
#   Sync point D: Smart_ConfigStruct.h  flat FSmart_ConfigStruct fields
#
# Points 5/6 (the Smart_Config Blueprint + Smart_ConfigStruct BP asset) live in the editor
# and are NOT covered here - validate those with AdaMCP get_class_defaults when the editor is up.
#
# Exit code 0 = parity OK, 1 = drift found. Pure stdlib; safe to run anytime / in CI.

import os, re, sys

ROOT = os.path.join(os.path.dirname(__file__), '..', 'Source', 'SmartFoundations')
CPP = os.path.join(ROOT, 'Private', 'Config', 'SmartFoundationsModConfiguration.cpp')
HDR = os.path.join(ROOT, 'Public', 'Config', 'Smart_ConfigStruct.h')

# Flat-struct fields that are intentionally NOT menu settings (kept at flat defaults,
# never config-filled). Documented in Smart_ConfigStruct.h. Not treated as drift.
KNOWN_FLAT_ONLY = {
    'AutoConnectMode',
    'PowerPoleMk1MaxConnections', 'PowerPoleMk2MaxConnections',
    'PowerPoleMk3MaxConnections', 'PowerPoleMk4MaxConnections',
}

def read(p):
    with open(p, encoding='utf-8') as f:
        return f.read()

# ── Sync point A: constructor ─────────────────────────────────────────────────
# Map the local section variable -> section key, e.g. `UConfigPropertySection* Belt = CreateSection(TEXT("BeltAutoConnect")`
# then collect leaf keys added under it: `Belt->SectionProperties.Add(TEXT("bAutoConnectEnabled"), Create...`
def parse_ctor(txt):
    sections = {}          # section_key -> [leaf keys in declaration order]
    var_to_key = {}
    for m in re.finditer(r'(\w+)\s*=\s*CreateSection\(\s*TEXT\("(\w+)"', txt):
        var_to_key[m.group(1)] = m.group(2)
        if m.group(2) != 'Root':
            sections.setdefault(m.group(2), [])
    for m in re.finditer(r'(\w+)->SectionProperties\.Add\(\s*TEXT\("(\w+)"\)\s*,\s*Create(\w+)Property', txt):
        var, leaf = m.group(1), m.group(2)
        key = var_to_key.get(var)
        if key and key != 'Root':
            sections[key].append(leaf)
    # RootSection->SectionProperties.Add(TEXT("BeltAutoConnect"), Belt) -> registered section keys
    root_added = re.findall(r'RootSection->SectionProperties\.Add\(\s*TEXT\("(\w+)"', txt)
    return sections, root_added

# ── Sync points B/C/D: the struct header ──────────────────────────────────────
def parse_header(txt):
    # B: each nested mirror sub-struct -> its fields
    substructs = {}        # struct type name -> [field names]
    for sm in re.finditer(r'struct\s+(FSmart_\w+ConfigSection)\s*\{(.*?)\n\}', txt, re.S):
        name, body = sm.group(1), sm.group(2)
        fields = re.findall(r'UPROPERTY\([^)]*\)\s*(?:bool|int32|float)\s+(\w+)\s*\{', body)
        substructs[name] = fields
    # FSmart_ConfigStruct_Sections: section field name -> sub-struct type
    sec_map = {}
    sm = re.search(r'struct\s+FSmart_ConfigStruct_Sections\s*\{(.*?)\n\}', txt, re.S)
    if sm:
        for fm in re.finditer(r'(FSmart_\w+ConfigSection)\s+(\w+)\s*;', sm.group(1)):
            sec_map[fm.group(2)] = fm.group(1)
    # D: flat struct fields
    flat = []
    fm = re.search(r'struct\s+FSmart_ConfigStruct\s*\{(.*?)static\s+FSmart_ConfigStruct\s+GetActiveConfig', txt, re.S)
    if fm:
        flat = re.findall(r'UPROPERTY\([^)]*\)\s*(?:bool|int32|float)\s+(\w+)\s*\{', fm.group(1))
    # C: copy-down  ConfigStruct.X = Sections.Section.Y;
    copydown = {}          # section field -> [(flatField, leaf)]
    for cm in re.finditer(r'ConfigStruct\.(\w+)\s*=\s*Sections\.(\w+)\.(\w+)\s*;', txt):
        copydown.setdefault(cm.group(2), []).append((cm.group(1), cm.group(3)))
    return substructs, sec_map, flat, copydown

def main():
    if not (os.path.exists(CPP) and os.path.exists(HDR)):
        print("ERROR: could not find config sources at %s / %s" % (CPP, HDR))
        return 2

    ctor_sections, root_added = parse_ctor(read(CPP))
    substructs, sec_map, flat, copydown = parse_header(read(HDR))

    flat_set = set(flat)
    problems = []

    # Section-level parity: CreateSection keys vs RootSection.Add vs FSmart_ConfigStruct_Sections fields
    ctor_keys = set(ctor_sections)
    root_set = set(root_added)
    secmap_set = set(sec_map)
    if not (ctor_keys == root_set == secmap_set):
        problems.append("SECTION SET MISMATCH:")
        problems.append("  CreateSection:        %s" % sorted(ctor_keys))
        problems.append("  RootSection.Add:      %s" % sorted(root_set))
        problems.append("  Sections struct:      %s" % sorted(secmap_set))

    print("%-20s %5s %5s %5s %5s  %s" % ('section', 'ctor', 'nest', 'copy', 'flat', 'status'))
    print("-" * 70)
    for key in sorted(ctor_keys | root_set | secmap_set):
        a = ctor_sections.get(key, [])
        sub_type = sec_map.get(key)
        b = substructs.get(sub_type, []) if sub_type else []
        c_pairs = copydown.get(key, [])
        c = [leaf for (_flat, leaf) in c_pairs]
        a_s, b_s, c_s = set(a), set(b), set(c)

        ok = (a_s == b_s == c_s)
        # every leaf must also have a flat field + a copy-down LHS into that flat field
        flat_ok = True
        for (flat_field, leaf) in c_pairs:
            if flat_field not in flat_set:
                flat_ok = False
                problems.append("  [%s] copy-down targets flat field '%s' that does not exist" % (key, flat_field))
        for leaf in a_s:
            if leaf not in c_s:
                continue  # caught by ok mismatch below
        status = 'OK' if (ok and flat_ok) else 'DRIFT'
        print("%-20s %5d %5d %5d %5d  %s" % (key, len(a), len(b), len(c), len(flat_set), status))

        if not ok:
            problems.append("LEAF MISMATCH in section '%s':" % key)
            problems.append("  ctor(A)      : %s" % sorted(a_s))
            problems.append("  nested(B) %-12s: %s" % (sub_type or '?', sorted(b_s)))
            problems.append("  copydown(C)  : %s" % sorted(c_s))
            if a_s - b_s: problems.append("    in ctor, missing from nested struct : %s" % sorted(a_s - b_s))
            if b_s - a_s: problems.append("    in nested struct, missing from ctor : %s" % sorted(b_s - a_s))
            if a_s - c_s: problems.append("    in ctor, missing from copy-down     : %s" % sorted(a_s - c_s))
            if c_s - a_s: problems.append("    in copy-down, missing from ctor     : %s" % sorted(c_s - a_s))

    # Flat fields that are neither a known exclusion nor covered by any copy-down LHS
    covered = {fl for pairs in copydown.values() for (fl, _leaf) in pairs}
    orphan_flat = flat_set - covered - KNOWN_FLAT_ONLY
    if orphan_flat:
        problems.append("FLAT FIELDS not copy-filled and not in KNOWN_FLAT_ONLY (typo, or forgot copy-down?):")
        problems.append("  %s" % sorted(orphan_flat))

    print()
    if problems:
        print("DRIFT DETECTED (%d issue group(s)) - fix before cooking:" % len(problems))
        for p in problems:
            print(p)
        return 1
    print("Config parity OK: sync points A/B/C/D agree across all %d sections." % len(ctor_keys))
    print("NOTE: editor sync points (Smart_Config Blueprint + Smart_ConfigStruct BP asset) are not")
    print("      checked here - verify with AdaMCP get_class_defaults when the editor is running.")
    return 0

if __name__ == '__main__':
    sys.exit(main())
