#!/usr/bin/env python3
"""Read-only completeness gate for the current mod and wiki translation sources."""
import argparse
from collections import Counter
from pathlib import Path
import re
from catalog import read_json, write_json, read_po, manifest_entries, violations, digest, is_untranslated_prose
from wiki import protected_markdown, is_verbatim
from game_terms import read_locres


def verify_official_terms(entries, terms, bindings, culture):
    indexed = {entry.context: entry for entry in entries if entry.context}
    issues = []
    for context, identity in bindings.items():
        entry, term = indexed.get(context), terms.get(identity)
        if entry is None or term is None or culture not in term.get('translations', {}):
            issues.append([context, 'official-term-reference-missing'])
        elif entry.source != term['english']:
            issues.append([context, 'official-term-source-mismatch'])
        elif entry.translation != term['translations'][culture]:
            issues.append([context, 'official-term-mismatch'])
    return issues


def verify_compiled(entries, resource):
    issues = []
    for entry in entries:
        if not entry.context or not entry.translation: continue
        namespace, key = entry.context.split(',', 1) if ',' in entry.context else ('', entry.context)
        compiled = resource.get((namespace, key))
        if compiled is None:
            issues.append([entry.context, 'compiled-entry-missing'])
        elif compiled[0] != entry.translation:
            issues.append([entry.context, 'compiled-translation-mismatch'])
    return issues


def verify(root, target, policy):
    base = root / 'Content/Localization' / target
    manifest = manifest_entries(base / (target + '.manifest'))
    conflict_report = base / (target + '_Conflicts.txt')
    has_conflicts = conflict_report.exists() and bool(conflict_report.read_text(encoding='utf-8-sig').strip())
    result = {}
    for culture in policy['cultures']:
        if culture == policy['native']: continue
        path = base / culture / (target + '.po')
        entries = read_po(path) if path.exists() else []
        indexed = {e.context:e for e in entries if e.context}
        issues = []
        if has_conflicts:
            issues.append(['*', 'gather-source-conflicts'])
        for key,item in manifest.items():
            e = indexed.get(key)
            if e is None: issues.append([key,'missing']); continue
            if e.source != item['source']: issues.append([key,'stale-source'])
            if not e.translation.strip(): issues.append([key,'empty'])
            if e.errors: issues.append([key,e.errors])
            if e.translation:
                for error in violations(e.source,e.translation,policy): issues.append([key,error])
                if is_untranslated_prose(e.source,e.translation,policy): issues.append([key,'untranslated-prose'])
                if '__KEEP_' in e.translation: issues.append([key,'unrestored-token'])
        if len(indexed) != sum(bool(e.context) for e in entries): issues.append(['*','duplicate-identity'])
        result[culture] = {'required':len(manifest),'complete':len(manifest)-len({k for k,_ in issues if k!='*'}),'issues':issues}
    return result


def verify_wiki(root, policy):
    manifest = read_json(root / 'source.json')
    result = {}
    shape=lambda s:Counter(re.findall(r'(?m)^(#{1,6}\s|[-*]\s|\d+\.\s|\|)',s))
    for culture in policy['cultures']:
        if culture == policy['native']: continue
        data = read_json(root / (culture + '.json'))
        issues = []
        for key,item in manifest.items():
            entry = data.get(key,{})
            text = entry.get('text','')
            if not text.strip(): issues.append([key,'empty']); continue
            if entry.get('source_hash') != digest(item['source']): issues.append([key,'stale-source'])
            if entry.get('status') != 'reviewed': issues.append([key,'not-reviewed'])
            for error in violations(item['source'],text,policy): issues.append([key,error])
            if is_untranslated_prose(item['source'],text,policy) and not is_verbatim(item['source'],policy): issues.append([key,'untranslated-prose'])
            if protected_markdown(item['source']) != protected_markdown(text): issues.append([key,'markdown-links-or-code'])
            if shape(item['source']) != shape(text): issues.append([key,'markdown-structure'])
            if '__KEEP_' in text: issues.append([key,'unrestored-token'])
        result[culture] = {'required':len(manifest),'complete':len(manifest)-len({k for k,_ in issues}),'issues':issues}
    return result


def verify_wiki_interface(root, policy):
    chrome = read_json(root / 'chrome.json')
    source = chrome[policy['native']]
    result = {}
    for culture in policy['cultures']:
        if culture == policy['native']: continue
        localized = chrome.get(culture, {})
        issues = []
        for key, text in source.items():
            value = localized.get(key, '')
            if not value.strip():
                issues.append([key, 'empty'])
                continue
            issues.extend([key, error] for error in violations(text, value, policy))
            if is_untranslated_prose(text, value, policy) or (key == 'languages' and value == text):
                issues.append([key, 'untranslated-prose'])
        result[culture] = {'required': len(source), 'complete': len(source)-len({key for key,_ in issues}), 'issues': issues}
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--terms',type=Path,help='Version-matched official game terminology JSON')
    parser.add_argument('--compiled',action='store_true',help='Also require compiled resources to match every translated PO entry')
    args=parser.parse_args()
    policy=read_json(Path(__file__).with_name('policy.json'))
    result={'SmartFoundations':verify(args.root,'SmartFoundations',policy),
            'SmartCamera':verify(args.root.parent/'SmartCamera','SmartCamera',policy),
            'wiki':verify_wiki(args.root/'Localization/wiki',policy),
            'wiki_interface':verify_wiki_interface(args.root/'Localization/wiki',policy)}
    if args.compiled:
        for target, root in [('SmartFoundations',args.root),('SmartCamera',args.root.parent/'SmartCamera')]:
            for culture, status in result[target].items():
                folder=root/'Content/Localization'/target/culture
                resource=folder/(target+'.locres')
                if not resource.exists():
                    status['issues'].append(['*','compiled-resource-missing'])
                    status['complete']=0
                else:
                    status['issues'].extend(verify_compiled(read_po(folder/(target+'.po')),read_locres(resource)))
                    status['complete']=status['required']-len({key for key,_ in status['issues'] if key!='*'})
    if args.terms:
        terms=read_json(args.terms)['terms']
        bindings=read_json(Path(__file__).with_name('term-bindings.json'))
        for culture, status in result['SmartFoundations'].items():
            entries=read_po(args.root/'Content/Localization/SmartFoundations'/culture/'SmartFoundations.po')
            status['issues'].extend(verify_official_terms(entries,terms,bindings,culture))
            status['complete']=0 if any(error=='compiled-resource-missing' for _,error in status['issues']) else status['required']-len({key for key,_ in status['issues'] if key!='*'})
    write_json(args.report,result)
    count=sum(len(c['issues']) for target in result.values() for c in target.values())
    print(f'Completeness/structure findings: {count}. This is not native-language or rendered UI certification.')
    return 1 if count else 0


if __name__=='__main__':raise SystemExit(main())
