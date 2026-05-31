"""
One-shot helper for the T7.2 log-category migration.

Swaps a feature's UE_LOG / wrapper-macro log calls from the catch-all LogSmartFoundations
category to a per-feature category (e.g. LogSmartHologram), then reports per-file counts so
the change can be eyeballed before building. SmartFoundations.h includes SFLogMacros.h, so no
per-file include edits are needed -- this only swaps the category identifier inside log calls.

It replaces the bare token `LogSmartFoundations` ONLY where it is the first argument of a log
macro call -- i.e. `<MACRO>(LogSmartFoundations,` -- so it catches UE_LOG and the
SF_*_DIAGNOSTIC_LOG wrappers (which forward their category arg) without touching
DECLARE_/DEFINE_LOG_CATEGORY lines or comments.

Usage:
    python scripts/migrate_log_category.py <NewCategory> <file1.cpp> [file2.cpp ...]
    python scripts/migrate_log_category.py --dry <NewCategory> <files...>   # report only

It will NOT touch a file that DECLAREs/DEFINEs LogSmartFoundations, or one that defines a
log wrapper macro whose body hardcodes the category (those need manual handling).
"""
import re
import sys

LOGCALL = re.compile(r'(\b[A-Z_]+\s*\(\s*)LogSmartFoundations(\s*,)')
GUARD = re.compile(r'(DECLARE_LOG_CATEGORY|DEFINE_LOG_CATEGORY|#define\s+\w*LOG\w*)')


def main():
    args = sys.argv[1:]
    dry = False
    if args and args[0] == "--dry":
        dry = True
        args = args[1:]
    if len(args) < 2:
        print("usage: migrate_log_category.py [--dry] <NewCategory> <files...>")
        sys.exit(2)
    new_cat = args[0]
    files = args[1:]

    total = 0
    for path in files:
        try:
            src = open(path, encoding="utf-8").read()
        except OSError as e:
            print("SKIP (cannot read): %s (%s)" % (path, e))
            continue

        # Safety: refuse files that declare/define the category or a log macro body.
        guarded = [ln for ln in src.splitlines()
                   if GUARD.search(ln) and "LogSmartFoundations" in ln]
        if guarded:
            print("SKIP (declares/defines category or macro): %s" % path)
            for ln in guarded:
                print("        %s" % ln.strip())
            continue

        new_src, n = LOGCALL.subn(r'\g<1>%s\g<2>' % new_cat, src)
        remaining = new_src.count("LogSmartFoundations")
        print("%-70s swapped=%-4d remaining=%d" % (path.split("SmartFoundations/")[-1], n, remaining))
        total += n
        if not dry and n:
            open(path, "w", encoding="utf-8", newline="").write(new_src)

    print("\n%s: %d call sites -> %s across %d file(s)"
          % ("DRY RUN" if dry else "APPLIED", total, new_cat, len(files)))


if __name__ == "__main__":
    main()
