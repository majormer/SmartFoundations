"""
Smart! in-game smoke-test harness.

A lightweight safety net for the code-simplicity refactor: it exercises the live game through
the SmartMCP HTTP API and asserts the core systems still respond sanely after a change. This is
NOT a unit-test suite (the mod has no test infra and most logic needs the live engine) — it is a
fast "did I break something obvious" check to run before/after a NEEDS-CARE refactor slice.

Per the refactor charter, NEEDS-CARE slices require an in-game smoke covering: foundation grid,
auto-connect preview, extend manifold, and upgrade cost. This harness automates the observable
parts of that (player/hologram/child/power/production readbacks) so a human only has to drive the
build gun, not eyeball raw JSON.

Usage:
    1. Launch Satisfactory with Smart! + SmartMCP enabled; load a save.
    2. python scripts/smoke_test.py                 # baseline: connectivity + readback
       python scripts/smoke_test.py --watch         # poll holograms/children live while you build

Exit code 0 = all probed endpoints healthy; non-zero = a probe failed (details printed).

The API base URL can be overridden with SMARTMCP_URL (default http://localhost:5095/api).
"""
import json
import os
import sys
import time
import urllib.request
import urllib.error

BASE = os.environ.get("SMARTMCP_URL", "http://localhost:5095/api").rstrip("/")

# (path, label, optional validator(parsed_json) -> None|str-error)
PROBES = [
    ("status", "API health", None),
    ("player/status", "player position/health", None),
    ("holograms/active", "active build-gun hologram", None),
    ("holograms/children", "child holograms (grid)", None),
    ("metrics/uobjects", "UObject/hologram/widget metrics", None),
    ("power/summary", "power circuit summary", None),
    ("production/summary?limit=10", "production summary", None),
]


def get(path, timeout=5):
    url = "%s/%s" % (BASE, path)
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        raw = resp.read().decode("utf-8", "replace")
    try:
        return json.loads(raw), None
    except json.JSONDecodeError:
        return raw, None


def run_probes():
    print("Smart! smoke test -> %s\n" % BASE)
    failures = 0
    for path, label, validator in PROBES:
        try:
            data, _ = get(path)
            note = ""
            if validator:
                err = validator(data)
                if err:
                    failures += 1
                    print("  FAIL  %-32s %s" % (label, err))
                    continue
            # compact one-line preview of the payload
            if isinstance(data, dict):
                note = ", ".join(list(data.keys())[:6])
            print("  ok    %-32s {%s}" % (label, note))
        except urllib.error.URLError as e:
            failures += 1
            print("  FAIL  %-32s %s" % (label, e))
        except Exception as e:  # noqa: BLE001
            failures += 1
            print("  FAIL  %-32s %s" % (label, e))
    print()
    if failures:
        print("SMOKE FAILED: %d probe(s) failed. Is the game running with SmartMCP enabled?" % failures)
    else:
        print("SMOKE OK: all probed endpoints healthy.")
    return failures


def watch():
    """Poll the active hologram + child count so you can watch grid/extend previews live."""
    print("Watching holograms (Ctrl+C to stop). Aim the build gun and scale a grid / extend...\n")
    while True:
        try:
            active, _ = get("holograms/active")
            children, _ = get("holograms/children")
            a = active.get("class", active.get("hologram", "?")) if isinstance(active, dict) else "?"
            n = (children.get("count") if isinstance(children, dict) else None)
            if n is None and isinstance(children, dict):
                for k in ("children", "items", "holograms"):
                    if isinstance(children.get(k), list):
                        n = len(children[k]); break
            print("  active=%-40s children=%s" % (a, n))
        except Exception as e:  # noqa: BLE001
            print("  (probe error: %s)" % e)
        time.sleep(1.0)


def main():
    if "--watch" in sys.argv[1:]:
        try:
            watch()
        except KeyboardInterrupt:
            print("\nstopped.")
        return 0
    return 1 if run_probes() else 0


if __name__ == "__main__":
    sys.exit(main())
