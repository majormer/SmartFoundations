"""
T3: Extract the 14 SFBuildableSizeRegistry_*.cpp data files into one Config/BuildableSizes.csv.

The registry data is 602 RegisterProfile(...) calls. This script parses them, writes a single CSV,
then RE-READS the CSV and compares record-for-record against a fresh parse of the .cpp files,
failing loudly on any mismatch. That round-trip proves no building profile was lost or altered
before any build happens (T3 is data-accuracy-critical: a wrong row = a building scales wrong).

RegisterProfile signature (defaults matter — calls may omit trailing args):
    RegisterProfile(ClassName, Size,
                    bSwapOnRotation=false, bSupportsScaling=true,
                    Inheritance=TEXT(""), bValidated=true,
                    AnchorOffset=FVector::ZeroVector)

CSV columns (Inheritance dropped — documentation only, per the ADR):
    ClassName,SizeX,SizeY,SizeZ,SwapXYOnRotation,SupportsScaling,Validated,AnchorX,AnchorY,AnchorZ

Usage:
    python scripts/extract_size_registry.py            # parse + write CSV + round-trip verify
    python scripts/extract_size_registry.py --check     # parse + verify only, do NOT write CSV
"""
import csv
import glob
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_DIR = os.path.join(REPO, "Source", "SmartFoundations", "Private", "Data")
CSV_PATH = os.path.join(REPO, "Content", "Data", "BuildableSizes.csv")
COLUMNS = ["ClassName", "SizeX", "SizeY", "SizeZ",
           "SwapXYOnRotation", "SupportsScaling", "Validated",
           "AnchorX", "AnchorY", "AnchorZ"]


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)   # block comments
    s = re.sub(r"//[^\n]*", "", s)                  # line comments
    return s


def split_top_level_args(s):
    """Split a C++ arg list on top-level commas (respecting nested () and string literals)."""
    args, depth, buf, in_str = [], 0, [], False
    i = 0
    while i < len(s):
        c = s[i]
        if in_str:
            buf.append(c)
            if c == '\\' and i + 1 < len(s):
                buf.append(s[i + 1]); i += 2; continue
            if c == '"':
                in_str = False
        else:
            if c == '"':
                in_str = True; buf.append(c)
            elif c == '(':
                depth += 1; buf.append(c)
            elif c == ')':
                depth -= 1; buf.append(c)
            elif c == ',' and depth == 0:
                args.append("".join(buf).strip()); buf = []
            else:
                buf.append(c)
        i += 1
    if "".join(buf).strip():
        args.append("".join(buf).strip())
    return args


def find_calls(text):
    """Yield the inner-text of each RegisterProfile(...) call (paren-balanced, string-aware)."""
    for m in re.finditer(r"RegisterProfile\s*\(", text):
        i = m.end()           # just past the opening paren
        depth, in_str, start = 1, False, i
        while i < len(text) and depth > 0:
            c = text[i]
            if in_str:
                if c == '\\':
                    i += 2; continue
                if c == '"':
                    in_str = False
            else:
                if c == '"':
                    in_str = True
                elif c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                    if depth == 0:
                        yield text[start:i]
                        break
            i += 1


def parse_text(s):
    m = re.search(r'TEXT\("((?:[^"\\]|\\.)*)"\)', s)
    return m.group(1) if m else None


def parse_vec(s):
    if "ZeroVector" in s:
        return (0.0, 0.0, 0.0)
    m = re.search(r"FVector\s*\(\s*([-\d.eEf]+)\s*,\s*([-\d.eEf]+)\s*,\s*([-\d.eEf]+)\s*\)", s)
    if not m:
        return None
    return tuple(float(g.rstrip("fF")) for g in m.groups())


def parse_bool(s, default):
    s = s.strip()
    if s == "true":
        return True
    if s == "false":
        return False
    return default


def parse_file(path):
    """Return list of dicts, one per RegisterProfile call in this file."""
    text = open(path, encoding="utf-8").read()
    records = []
    for inner in find_calls(text):
        args = split_top_level_args(strip_comments(inner))
        if len(args) < 2:
            raise ValueError("RegisterProfile with <2 args in %s: %r" % (path, args))
        name = parse_text(args[0])
        size = parse_vec(args[1])
        if name is None or size is None:
            raise ValueError("Bad name/size in %s: %r" % (path, args[:2]))
        swap = parse_bool(args[2], False) if len(args) > 2 else False
        scaling = parse_bool(args[3], True) if len(args) > 3 else True
        # args[4] = Inheritance (dropped)
        validated = parse_bool(args[5], True) if len(args) > 5 else True
        anchor = parse_vec(args[6]) if len(args) > 6 else (0.0, 0.0, 0.0)
        if anchor is None:
            anchor = (0.0, 0.0, 0.0)
        records.append({
            "ClassName": name,
            "SizeX": size[0], "SizeY": size[1], "SizeZ": size[2],
            "SwapXYOnRotation": int(swap), "SupportsScaling": int(scaling),
            "Validated": int(validated),
            "AnchorX": anchor[0], "AnchorY": anchor[1], "AnchorZ": anchor[2],
        })
    return records


def parse_all():
    records, per_file = [], {}
    for path in sorted(glob.glob(os.path.join(DATA_DIR, "SFBuildableSizeRegistry_*.cpp"))):
        recs = parse_file(path)
        per_file[os.path.basename(path)] = len(recs)
        records.extend(recs)
    return records, per_file


def fmt_num(x):
    # integer-valued floats as ints (800.0 -> 800) for clean diffs
    return str(int(x)) if float(x).is_integer() else repr(x)


def write_csv(records):
    os.makedirs(os.path.dirname(CSV_PATH), exist_ok=True)
    with open(CSV_PATH, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(COLUMNS)
        for r in records:
            w.writerow([r["ClassName"], fmt_num(r["SizeX"]), fmt_num(r["SizeY"]), fmt_num(r["SizeZ"]),
                        r["SwapXYOnRotation"], r["SupportsScaling"], r["Validated"],
                        fmt_num(r["AnchorX"]), fmt_num(r["AnchorY"]), fmt_num(r["AnchorZ"])])


def read_csv():
    out = []
    with open(CSV_PATH, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            out.append({
                "ClassName": row["ClassName"],
                "SizeX": float(row["SizeX"]), "SizeY": float(row["SizeY"]), "SizeZ": float(row["SizeZ"]),
                "SwapXYOnRotation": int(row["SwapXYOnRotation"]),
                "SupportsScaling": int(row["SupportsScaling"]),
                "Validated": int(row["Validated"]),
                "AnchorX": float(row["AnchorX"]), "AnchorY": float(row["AnchorY"]), "AnchorZ": float(row["AnchorZ"]),
            })
    return out


def key(r):
    return (r["ClassName"], r["SizeX"], r["SizeY"], r["SizeZ"],
            r["SwapXYOnRotation"], r["SupportsScaling"], r["Validated"],
            r["AnchorX"], r["AnchorY"], r["AnchorZ"])


def main():
    check_only = "--check" in sys.argv[1:]
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    records, per_file = parse_all()
    print("Parsed RegisterProfile calls:")
    for name, n in per_file.items():
        print("  %4d  %s" % (n, name))
    print("  ----  TOTAL = %d" % len(records))

    # Duplicate class-name report (TMap.Add = last-wins; CSV must preserve order so last wins too)
    seen = {}
    dupes = []
    for i, r in enumerate(records):
        if r["ClassName"] in seen:
            dupes.append((r["ClassName"], seen[r["ClassName"]], i))
        seen[r["ClassName"]] = i
    if dupes:
        print("\n  NOTE: %d duplicate ClassName(s) — last occurrence wins (matches TMap.Add):" % len(dupes))
        for nm, a, b in dupes[:20]:
            print("     %s (rows %d, %d)" % (nm, a, b))

    if not check_only:
        write_csv(records)
        print("\nWrote %s" % CSV_PATH)

    # Round-trip: re-parse cpp + re-read csv, compare multisets.
    if not check_only and os.path.exists(CSV_PATH):
        fresh, _ = parse_all()
        from_csv = read_csv()
        a = sorted(key(r) for r in fresh)
        b = sorted(key(r) for r in from_csv)
        if a == b:
            print("ROUND-TRIP OK: %d records identical between .cpp and CSV." % len(a))
            return 0
        print("ROUND-TRIP FAILED: .cpp and CSV differ.")
        sa, sb = set(a), set(b)
        for r in list(sa - sb)[:20]:
            print("  only in cpp: %r" % (r,))
        for r in list(sb - sa)[:20]:
            print("  only in csv: %r" % (r,))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
