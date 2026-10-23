#!/usr/bin/env python3
"""Apply current-source translations with shared validation and ambiguity rejection.

Input remains <directory>/<culture>.json, keyed by full context (preferred) or an
unambiguous bare key. A value may be a string or {source, translation}. Only empty
translations change unless --overwrite is supplied. No build or publication occurs.
"""
import argparse
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent / 'localization'))
from catalog import read_json, read_po, write_po, violations


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dir', type=Path, default=root / 'scripts/.local/trans')
    parser.add_argument('--overwrite', action='store_true')
    args = parser.parse_args()
    policy = read_json(root / 'scripts/localization/policy.json')
    prepared = []
    for culture in policy['cultures']:
        if culture == policy['native'] or not (args.dir / (culture + '.json')).exists(): continue
        inputs = read_json(args.dir / (culture + '.json'))
        path = root / 'Content/Localization/SmartFoundations' / culture / 'SmartFoundations.po'
        entries = read_po(path)
        if any(e.errors for e in entries): raise ValueError(f'Malformed catalog: {culture}; reconcile first')
        full = {e.context: e for e in entries if e.context}
        bare = {}
        for e in full.values(): bare.setdefault(e.context.split(',', 1)[-1], []).append(e)
        count = 0
        for identity, value in inputs.items():
            matches = [full[identity]] if identity in full else bare.get(identity, [])
            if len(matches) != 1: raise ValueError(f'Unknown or ambiguous translation identity: {identity}')
            e = matches[0]
            text = value.get('translation', '') if isinstance(value, dict) else value
            if isinstance(value, dict) and value.get('source') != e.source:
                raise ValueError(f'Stale translation source: {identity}')
            if not isinstance(text, str): raise ValueError(f'Translation must be text: {identity}')
            if not text or (e.translation and not args.overwrite): continue
            problems = violations(e.source, text, policy)
            if problems: raise ValueError(f'{culture}/{identity}: {problems}')
            e.translation = text
            count += 1
        prepared.append((path, entries, culture, count))
    # Reject the whole batch before changing any catalog if an entry fails validation.
    for path, entries, culture, count in prepared:
        write_po(path, entries)
        print(f'{culture}: {count} translations applied')
    print('Compile localization before packaging; language review remains separate.')


if __name__ == '__main__': main()
