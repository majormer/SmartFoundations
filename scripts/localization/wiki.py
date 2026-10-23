"""Generate translated wiki pages without forking the English authoring source."""
from pathlib import Path
import re
from catalog import digest, read_json, write_json, violations


def is_verbatim(source, policy):
    return digest(source) in policy.get('wiki', {}).get('verbatim_source_hashes', {})


def units(text):
    """Keep Markdown blocks intact (tables/fences/lists); blank separators round-trip."""
    return re.split(r'(\n[ \t]*\n)', text)


def extract(source, catalogs, policy):
    catalogs = Path(catalogs)
    previous = read_json(catalogs / 'source.json') if (catalogs / 'source.json').exists() else {}
    manifest = {}
    for page in sorted(Path(source).glob('*.md')):
        prior = {k: v for k, v in previous.items() if v['page'] == page.stem}
        used = set()
        next_id = max((int(k.rsplit(':', 1)[1]) for k in prior), default=-1) + 1
        for index, block in enumerate(units(page.read_text(encoding='utf-8'))):
            if block.strip():
                matching = [k for k, v in prior.items() if k not in used and v['source_hash'] == digest(block)]
                key = matching[0] if matching else None
                if key is None:
                    key = f'{page.stem}:{next_id}'
                    next_id += 1
                used.add(key)
                manifest[key] = {'page': page.stem, 'position': index, 'source': block, 'source_hash': digest(block)}
    write_json(catalogs / 'source.json', manifest)
    for culture in policy['cultures']:
        if culture == policy['native']:
            continue
        path = catalogs / f'{culture}.json'
        old = read_json(path) if path.exists() else {}
        result = {}
        for key, unit in manifest.items():
            entry = old.get(key, {'text': '', 'status': 'draft', 'source_hash': unit['source_hash']})
            # Keep translator work, including old source hash, for explicit review.
            result[key] = entry
        for key in old.keys() - result.keys():
            result[key] = dict(old[key], obsolete=True)
        write_json(path, result)
    return manifest


def protected_markdown(text):
    return (sorted(re.findall(r'!?\[[^\]]*\]\(([^)]+)\)', text)),
            sorted(re.findall(r'`[^`]+`', text)))


def build(source, catalogs, output, policy, include_drafts=False):
    source, catalogs, output = Path(source), Path(catalogs), Path(output)
    manifest = read_json(catalogs / 'source.json')
    chrome = read_json(catalogs / 'chrome.json') if (catalogs / 'chrome.json').exists() else {}
    positions = {(v['page'], v['position']): k for k, v in manifest.items()}
    pages = {p.stem: p for p in source.glob('*.md')}
    output.mkdir(parents=True, exist_ok=True)
    report = {}
    def links(text, culture):
        def rewrite(m):
            target, sep, fragment = m[2].partition('#')
            name = target.removesuffix('.md')
            if name in pages:
                # English fragment identifiers are not stable when headings translate.
                # Link the localized page until a translated anchor map is reviewed.
                return m[1] + (name if culture == 'en' else f'{culture}-{name}') + ')'
            return m[0]
        return re.sub(r'(\]\()([^)]+)\)', rewrite, text)
    for culture in policy['cultures']:
        ui = chrome.get(culture, {'languages':'Languages', 'fallback':'Translation in progress: {count} section(s) below remain in English.', 'review':'REVIEW PREVIEW — may contain unreviewed draft translations. Do not publish.', 'screenshots':'Shared screenshots show the English interface.'})
        trans = {} if culture == 'en' else read_json(catalogs / f'{culture}.json')
        accepted, missing, invalid, drafts = 0, 0, [], 0
        for name, path in pages.items():
            parts = units(path.read_text(encoding='utf-8'))
            count = 0
            for index, block in enumerate(parts):
                if not block.strip():
                    continue
                key = positions.get((name, index))
                if key not in manifest or manifest[key]['source_hash'] != digest(block):
                    raise ValueError('Wiki source changed; run wiki-extract before build: ' + key)
                entry = trans.get(key, {})
                if culture == 'en':
                    continue
                errors = violations(block, entry.get('text', ''), policy) if entry.get('text') else []
                if entry.get('text') and protected_markdown(block) != protected_markdown(entry['text']):
                    errors.append('markdown-links-or-code')
                if errors:
                    invalid.append({'key': key, 'errors': errors})
                allowed = entry.get('status') == 'reviewed' or (include_drafts and entry.get('status') == 'draft')
                if allowed and entry.get('source_hash') == digest(block) and entry.get('text') and not errors:
                    parts[index] = entry['text']
                    if entry.get('status') == 'reviewed': accepted += 1
                    else: drafts += 1
                else:
                    missing += 1
                    count += 1
            prefix = '' if culture == 'en' else culture + '-'
            selector = ' · '.join(f'[{label}]({("" if code == "en" else code + "-") + name})' for code, label in policy['cultures'].items())
            status = '> ' + ui['fallback'].format(count=count) + '\n\n' if count else ''
            if include_drafts and culture != 'en':
                status = '> ' + ui['review'] + '\n\n' + status
            if culture != 'en' and any('![' in part for part in parts):
                status += '> ' + ui['screenshots'] + '\n\n'
            # Root sidebar is the language index; per-language sidebars are regular pages.
            content = selector + '\n\n' + status + links(''.join(parts), culture)
            (output / (prefix + name + '.md')).write_text(content, encoding='utf-8', newline='\n')
        report[culture] = {'reviewed_units': accepted, 'draft_units': drafts, 'fallback_units': missing, 'invalid_units': invalid}
    navigation = '# ' + ' / '.join(chrome.get(c, {}).get('languages', 'Languages') for c in policy['cultures']) + '\n\n' + '\n'.join(f'- [{label}]({"Home" if code == "en" else code + "-Home"})' for code, label in policy['cultures'].items()) + '\n'
    (output / '_Sidebar.md').write_text(navigation, encoding='utf-8')
    (output / 'Languages.md').write_text(navigation, encoding='utf-8')
    write_json(output / 'translation-status.json', report)
    return report

