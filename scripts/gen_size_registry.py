"""
T3: Generate SFBuildableSizeRegistry_Data.cpp from Content/Data/BuildableSizes.csv.

The CSV (Content/Data/BuildableSizes.csv) is the canonical, human-editable source of building
size profiles. This script regenerates the single compiled data file that feeds the registry, so
edits are made in ONE plain-text file and applied by re-running this script + rebuilding.

This replaces the 14 hand-maintained SFBuildableSizeRegistry_*.cpp files: same RegisterProfile
calls, one generated file, one editable CSV. Compiled-in (not runtime-loaded), so it always
packages and carries zero parse/staging risk.

Usage:  python scripts/gen_size_registry.py
"""
import csv
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV_PATH = os.path.join(REPO, "Content", "Data", "BuildableSizes.csv")
OUT_PATH = os.path.join(REPO, "Source", "SmartFoundations", "Private", "Data",
                        "SFBuildableSizeRegistry_Data.cpp")


def cpp_float(s):
    """Render a CSV numeric cell as a C++ float literal (800 -> 800.0f)."""
    f = float(s)
    return (str(int(f)) if f.is_integer() else repr(f)) + ".0f" if f.is_integer() else repr(f) + "f"


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    with open(CSV_PATH, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    lines = []
    lines.append("// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.")
    lines.append("")
    lines.append("// SFBuildableSizeRegistry_Data.cpp")
    lines.append("//")
    lines.append("// GENERATED FILE - DO NOT EDIT BY HAND.")
    lines.append("// Source of truth: Content/Data/BuildableSizes.csv")
    lines.append("// Regenerate with: python scripts/gen_size_registry.py")
    lines.append("//")
    lines.append("// Replaces the former 14 SFBuildableSizeRegistry_*.cpp files. The CSV is the single")
    lines.append("// human-editable size table; this file is compiled in (no runtime/packaging risk).")
    lines.append("")
    lines.append('#include "SFBuildableSizeRegistry.h"')
    lines.append('#include "Logging/SFLogMacros.h"')
    lines.append("")
    lines.append("extern FString CurrentSourceFile;")
    lines.append("")
    lines.append("void USFBuildableSizeRegistry::RegisterDefaultProfiles()")
    lines.append("{")
    lines.append('\tCurrentSourceFile = TEXT("BuildableSizes.csv");')
    lines.append('\tSF_LOG_ADAPTER(Normal, TEXT("📂 Registering buildable size profiles from generated table (%d rows)"), %d);'
                 % (len(rows), len(rows)))
    lines.append("")

    for r in rows:
        name = esc(r["ClassName"].strip())
        sx, sy, sz = cpp_float(r["SizeX"]), cpp_float(r["SizeY"]), cpp_float(r["SizeZ"])
        swap = "true" if r["SwapXYOnRotation"].strip() == "1" else "false"
        scaling = "true" if r["SupportsScaling"].strip() == "1" else "false"
        validated = "true" if r["Validated"].strip() == "1" else "false"
        ax, ay, az = cpp_float(r["AnchorX"]), cpp_float(r["AnchorY"]), cpp_float(r["AnchorZ"])
        anchor_zero = (float(r["AnchorX"]) == 0 and float(r["AnchorY"]) == 0 and float(r["AnchorZ"]) == 0)

        # RegisterProfile(ClassName, Size, bSwapOnRotation, bSupportsScaling, Inheritance, bValidated, AnchorOffset)
        if anchor_zero:
            lines.append('\tRegisterProfile(TEXT("%s"), FVector(%s, %s, %s), %s, %s, TEXT(""), %s);'
                         % (name, sx, sy, sz, swap, scaling, validated))
        else:
            lines.append('\tRegisterProfile(TEXT("%s"), FVector(%s, %s, %s), %s, %s, TEXT(""), %s, FVector(%s, %s, %s));'
                         % (name, sx, sy, sz, swap, scaling, validated, ax, ay, az))

    lines.append("}")
    lines.append("")

    out = "\n".join(lines)
    with open(OUT_PATH, "w", encoding="utf-8", newline="\n") as f:
        f.write(out)

    print("Wrote %s" % OUT_PATH)
    print("  %d RegisterProfile rows from %s" % (len(rows), os.path.basename(CSV_PATH)))


if __name__ == "__main__":
    main()
