#!/usr/bin/env python3
"""Compatibility entry point: reconcile current manifest identities, preserving recovery history."""
from pathlib import Path
import subprocess
import sys

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    raise SystemExit(subprocess.call([sys.executable, str(root / 'scripts/localization/manage.py'), 'reconcile',
        '--report', str(root / '.local/localization-reconcile/report.json')]))
