#!/usr/bin/env python3
"""Compatibility entry point for the source-checked localization synchronizer."""
from pathlib import Path
import subprocess
import sys

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    raise SystemExit(subprocess.call([sys.executable, str(root / 'scripts/localization/manage.py'), 'sync',
        '--report', str(root / '.local/localization-sync.json')]))
