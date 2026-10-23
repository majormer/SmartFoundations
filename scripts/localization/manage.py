#!/usr/bin/env python3
"""Local-first localization tooling. No command publishes, launches a game, or installs mods."""
import argparse
from pathlib import Path
import re
from catalog import read_json, write_json, read_po, write_po, manifest_entries, reconcile, sync_archive, violations, digest
import game_terms
import wiki

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent


def audit(root, target, policy, terms=None):
    base = root / 'Content/Localization' / target
    manifest = manifest_entries(base / (target + '.manifest'))
    report = {'target': target, 'manifest_entries': len(manifest), 'source_inventory': manifest,
              'cultures': {}, 'literal_review': []}
    bindings = read_json(HERE / 'term-bindings.json') if target == 'SmartFoundations' else {}
    for culture in policy['cultures']:
        path = base / culture / (target + '.po')
        entries = read_po(path) if path.exists() else []
        index, findings = {}, []
        for e in entries:
            if not e.context:
                continue
            if e.context in index:
                findings.append({'context': e.context, 'issues': ['duplicate-identity']})
            index[e.context] = e
            issues = list(e.errors)
            if e.context not in manifest:
                issues.append('obsolete')
            elif e.source != manifest[e.context]['source']:
                issues.append('stale-source')
            if e.translation:
                issues += violations(e.source, e.translation, policy)
                if terms and culture != 'en':
                    for identity, term in terms['terms'].items():
                        if not identity.startswith(('Buildables_Data:', 'Architecture_Data:')) and bindings.get(e.context) != identity:
                            continue
                        if e.source.strip(': ') == term['english'] and culture in term['translations']:
                            if e.translation.strip(': ') != term['translations'][culture]:
                                issues.append('official-label:' + identity)
            if issues:
                findings.append({'context': e.context, 'line': e.line, 'issues': issues})
        report['cultures'][culture] = {
            'catalog_entries': len(index), 'missing_identities': sorted(manifest.keys() - index.keys()),
            'untranslated': sorted(k for k in manifest if k not in index or not index[k].translation),
            'findings': findings}
    for path in (root / 'Source').rglob('*'):
        if path.suffix not in ('.cpp', '.h'):
            continue
        for number, line in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
            if re.search(r'FText::FromString\(\s*(?:TEXT\()?\s*"[A-Za-z]', line) or re.search(r'AddOption\(TEXT\("[A-Za-z]', line):
                glyph = bool(re.search(r'(?:FromString\(TEXT|AddOption\(TEXT)\("(?:X|Y|Z|X\+Y|x)"', line))
                classification = 'test-fixture' if path.stem.endswith('Tests') else ('language-neutral-symbol' if glyph else 'needs-context-review')
                report['literal_review'].append({'file': str(path.relative_to(root)), 'line': number, 'text': line.strip(), 'status': classification})
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--policy', type=Path, default=HERE / 'policy.json')
    sub = p.add_subparsers(dest='command', required=True)
    for command in ('audit', 'reconcile', 'sync'):
        s = sub.add_parser(command)
        s.add_argument('--root', type=Path, default=ROOT)
        s.add_argument('--target', default='SmartFoundations')
        s.add_argument('--report', type=Path, required=True)
        if command == 'audit':
            s.add_argument('--terms', type=Path)
    for command in ('wiki-extract', 'wiki-build'):
        s = sub.add_parser(command)
        s.add_argument('--source', type=Path, default=ROOT / 'wiki')
        s.add_argument('--catalogs', type=Path, default=ROOT / 'Localization/wiki')
        if command == 'wiki-build':
            s.add_argument('--output', type=Path, required=True)
            s.add_argument('--include-drafts', action='store_true')
    s = sub.add_parser('game-terms')
    s.add_argument('--resources', type=Path, required=True)
    s.add_argument('--tables', type=Path, required=True)
    s.add_argument('--output', type=Path, required=True)
    s.add_argument('--game-version', required=True)
    s = sub.add_parser('reuse')
    s.add_argument('--root', type=Path, default=ROOT)
    s.add_argument('--target', default='SmartFoundations')
    s.add_argument('--donor-root', type=Path, default=ROOT)
    s.add_argument('--donor-target', default='SmartFoundations')
    s.add_argument('--terms', type=Path, required=True)
    s.add_argument('--report', type=Path, required=True)
    s = sub.add_parser('export-review')
    s.add_argument('--root', type=Path, default=ROOT)
    s.add_argument('--target', default='SmartFoundations')
    s.add_argument('--output', type=Path, required=True)
    s = sub.add_parser('import-review')
    s.add_argument('--root', type=Path, default=ROOT)
    s.add_argument('--target', default='SmartFoundations')
    s.add_argument('--input', type=Path, required=True)
    s.add_argument('--report', type=Path, required=True)
    a = p.parse_args()
    policy = read_json(a.policy)
    if a.command == 'audit':
        result = audit(a.root, a.target, policy, read_json(a.terms) if a.terms else None)
        write_json(a.report, result)
        print(f"{a.target}: {result['manifest_entries']} current identities; audit written to {a.report}")
    elif a.command == 'reconcile':
        base = a.root / 'Content/Localization' / a.target
        manifest = manifest_entries(base / (a.target + '.manifest'))
        report = {}
        for culture in policy['cultures']:
            path = base / culture / (a.target + '.po')
            entries, history = reconcile(path, manifest, culture, policy)
            # Append-only recovery history; a subsequent run must not erase previous quarantines.
            recovery = a.report.parent / 'translation-history' / a.target / (culture + '.json')
            prior = read_json(recovery) if recovery.exists() else []
            for entry in history:
                if entry not in prior:
                    prior.append(entry)
            write_json(recovery, prior)
            write_po(path, entries)
            report[culture] = {'entries': len(manifest), 'quarantined': len(history)}
        write_json(a.report, report)
        print('Catalogs reconciled; unsafe/stale translations retained in recovery history.')
    elif a.command == 'sync':
        base = a.root / 'Content/Localization' / a.target
        report = {}
        manifest = manifest_entries(base / (a.target + '.manifest'))
        for culture in policy['cultures']:
            report[culture] = sync_archive(base / culture / (a.target + '.po'), base / culture / (a.target + '.archive'), policy, manifest)
        write_json(a.report, report)
        print('Source-matched, validated translations synchronized.')
    elif a.command == 'wiki-extract':
        print(f'{len(wiki.extract(a.source, a.catalogs, policy))} wiki units extracted.')
    elif a.command == 'wiki-build':
        r = wiki.build(a.source, a.catalogs, a.output, policy, a.include_drafts)
        print(f'{len(r)} languages generated; see translation-status.json for fallback and review status.')
    elif a.command == 'export-review':
        base = a.root / 'Content/Localization' / a.target
        manifest = manifest_entries(base / (a.target + '.manifest'))
        for culture in policy['cultures']:
            if culture == 'en': continue
            entries = {e.context: e for e in read_po(base / culture / (a.target + '.po')) if e.context}
            bundle = {'target': a.target, 'culture': culture, 'entries': {}}
            for context, item in manifest.items():
                e = entries.get(context)
                bundle['entries'][context] = dict(item, source_hash=digest(item['source']),
                    translation=e.translation if e else '', status='needs-review')
            write_json(a.output / (culture + '.json'), bundle)
        print('Contextual review bundles exported; no entries marked reviewed automatically.')
    elif a.command == 'import-review':
        bundle = read_json(a.input)
        if bundle['target'] != a.target or bundle['culture'] not in policy['cultures'] or bundle['culture'] == 'en':
            raise ValueError('Review bundle target/culture mismatch')
        base = a.root / 'Content/Localization' / a.target
        manifest = manifest_entries(base / (a.target + '.manifest'))
        path = base / bundle['culture'] / (a.target + '.po')
        entries = read_po(path)
        indexed = {e.context: e for e in entries if e.context}
        changes = []
        for context, reviewed in bundle['entries'].items():
            if reviewed.get('status') != 'reviewed': continue
            if context not in manifest or context not in indexed:
                raise ValueError('Unknown review identity: ' + context)
            source = manifest[context]['source']
            if reviewed.get('source_hash') != digest(source) or reviewed.get('source') != source:
                raise ValueError('Stale reviewed source: ' + context)
            text = reviewed.get('translation', '')
            errors = violations(source, text, policy)
            if not text or errors:
                raise ValueError(f'Invalid reviewed translation {context}: {errors}')
            if indexed[context].translation != text:
                changes.append({'context': context, 'old': indexed[context].translation, 'new': text})
                indexed[context].translation = text
        # Validate the entire import before writing any catalog changes.
        write_po(path, entries)
        write_json(a.report, changes)
        print(f'Imported {len(changes)} reviewed translations; compile before packaging.')
    elif a.command == 'reuse':
        terms = read_json(a.terms)
        bindings = read_json(HERE / 'term-bindings.json') if a.target == 'SmartFoundations' else {}
        result = {}
        for culture in policy['cultures']:
            if culture == 'en':
                continue
            source = a.donor_root / 'Content/Localization' / a.donor_target / culture / (a.donor_target + '.po')
            by_source = {}
            for entry in read_po(source):
                if entry.translation and not entry.errors and not violations(entry.source, entry.translation, policy):
                    by_source.setdefault(entry.source, set()).add(entry.translation)
            path = a.root / 'Content/Localization' / a.target / culture / (a.target + '.po')
            entries = read_po(path)
            changed = []
            for entry in entries:
                if not entry.context:
                    continue
                term = terms['terms'].get(bindings.get(entry.context, ''), {})
                official = term.get('translations', {}).get(culture)
                if official and term['english'] == entry.source and official != entry.translation:
                    changed.append({'context': entry.context, 'old': entry.translation, 'new': official, 'origin': bindings[entry.context]})
                    entry.translation = official
                elif not entry.translation and len(by_source.get(entry.source, [])) == 1:
                    entry.translation = next(iter(by_source[entry.source]))
                    changed.append({'context': entry.context, 'new': entry.translation, 'origin': 'existing-source-identical-translation'})
            write_po(path, entries)
            result[culture] = changed
        write_json(a.report, result)
        print('Applied identity-bound game terms and unambiguous source-identical prior translations.')
    else:
        result = game_terms.extract(a.resources, a.tables, a.output, policy, a.game_version)
        print(f"{len(result['terms'])} official term identities extracted.")


if __name__ == '__main__':
    main()
