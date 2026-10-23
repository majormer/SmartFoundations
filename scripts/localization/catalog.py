"""Lossless PO reading, identity-safe reconciliation and shared translation checks."""
import ast
from collections import Counter
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path
import re
import os
import tempfile


def atomic_text(path, text, encoding='utf-8'):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + '.', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'w', encoding=encoding, newline='\n') as output:
            output.write(text)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def read_json(path):
    raw = Path(path).read_bytes()
    return json.loads(raw.decode('utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig'))


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    atomic_text(path, json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def digest(text):
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


@dataclass
class Entry:
    context: str = ''
    source: str = ''
    translation: str = ''
    comments: list = field(default_factory=list)
    errors: list = field(default_factory=list)
    line: int = 0


def read_po(path):
    """Read multiline PO, retaining malformed entries as explicit failures, never silently skip."""
    entries = []
    current = Entry()
    active = None
    mapping = {'msgctxt': 'context', 'msgid': 'source', 'msgstr': 'translation'}
    for number, line in enumerate(Path(path).read_text(encoding='utf-8-sig').splitlines() + [''], 1):
        if not line.strip():
            if current.context or current.source or current.comments or current.errors:
                entries.append(current)
            current, active = Entry(), None
            continue
        if line.startswith('#'):
            current.comments.append(line)
            if line.startswith('#,') and 'fuzzy' in [flag.strip() for flag in line[2:].split(',')]:
                current.errors.append(f'line {number}: fuzzy translation requires review')
            continue
        match = re.match(r'^(msgctxt|msgid|msgstr)\s+(.*)$', line)
        if match:
            active = mapping[match[1]]
            value = match[2]
            current.line = current.line or number
        elif line.startswith('"') and active:
            value = line
        else:
            current.errors.append(f'line {number}: unsupported PO syntax')
            continue
        try:
            parsed = ast.literal_eval(value)
            if not isinstance(parsed, str):
                raise ValueError('not a string')
            setattr(current, active, getattr(current, active) + parsed)
        except (SyntaxError, ValueError):
            current.errors.append(f'line {number}: invalid quoted {active}')
    return entries


def write_po(path, entries):
    lines = []
    for e in entries:
        lines.extend(e.comments)
        if e.context:
            lines.append('msgctxt ' + json.dumps(e.context, ensure_ascii=False))
        lines.append('msgid ' + json.dumps(e.source, ensure_ascii=False))
        lines.append('msgstr ' + json.dumps(e.translation, ensure_ascii=False))
        lines.append('')
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    atomic_text(path, '\n'.join(lines) + '\n')


def manifest_entries(path):
    result = {}
    def walk(node, namespace=''):
        namespace = node.get('Namespace', namespace)
        for child in node.get('Children', []):
            for key in child.get('Keys', []):
                context = namespace + ',' + key['Key'] if namespace else key['Key']
                source = child['Source']['Text']
                if context in result and result[context]['source'] != source:
                    raise ValueError(f'Conflicting source for {context}')
                item = result.setdefault(context, {'source': source, 'locations': []})
                item['locations'].append(key.get('Path', ''))
        for sub in node.get('Subnamespaces', []):
            walk(sub, namespace)
    walk(read_json(path))
    return result


def archive_entries(data):
    result = []
    def walk(node, namespace=''):
        namespace = node.get('Namespace', namespace)
        for child in node.get('Children', []):
            context = namespace + ',' + child['Key'] if namespace else child['Key']
            result.append((context, child))
        for sub in node.get('Subnamespaces', []):
            walk(sub, namespace)
    walk(data)
    return result


def is_untranslated_prose(source, translation, policy):
    if source != translation:
        return False
    prose = re.sub(r'```[\s\S]*?```|`[^`\n]+`|https?://[^\s)]+|(?<=\]\()[^\n)]+', '', source)
    for token in sorted(policy['protected'], key=len, reverse=True):
        prose = prose.replace(token, '')
    prose = re.sub(r'\{[^}]*\}|</?[^>]*>', '', prose)
    return len(re.findall(r'[A-Za-z]{3,}', prose)) >= 3


def violations(source, translation, policy):
    issues = []
    if source.count('\n') != translation.count('\n'):
        issues.append('line-breaks')
    # Keep order independent, but count repeated arguments and format modifiers.
    tokens = lambda s: Counter(re.findall(r'(?<!`)\{[A-Za-z0-9_]+\}', s))
    if tokens(source) != tokens(translation):
        issues.append('format-arguments')
    modifiers = lambda s: Counter(re.findall(r'\|(?:plural|ordinal|gender|hpp)\(', s))
    if modifiers(source) != modifiers(translation):
        issues.append('format-modifiers')
    markup = lambda s: Counter(re.findall(r'</?[A-Za-z][^>]*>|</>', s))
    if markup(source) != markup(translation):
        issues.append('rich-text-tags')
    for token in policy['protected']:
        if source.count(token) != translation.count(token):
            issues.append('protected:' + token)
    return issues


def reconcile(path, manifest, culture, policy):
    old = read_po(path) if Path(path).exists() else []
    by_context = {}
    for e in old:
        if e.context:
            if e.context in by_context:
                raise ValueError(f'Duplicate PO identity: {e.context}')
            by_context[e.context] = e
    new, quarantined = [], []
    for context, item in sorted(manifest.items()):
        previous = by_context.get(context)
        source = item['source']
        translation = previous.translation if previous else ''
        errors = (previous.errors if previous else []) + (violations(source, translation, policy) if translation else [])
        if previous and (previous.source != source or errors):
            quarantined.append({'context': context, 'previous_source': previous.source,
                                'translation': translation, 'reason': errors or ['source-changed']})
            translation = ''
        if culture == policy['native']:
            translation = source
        comments = ['#: ' + loc for loc in item['locations'] if loc]
        new.append(Entry(context, source, translation, comments))
    # No history is thrown away: removed identities go into the sidecar as well.
    for context in sorted(by_context.keys() - manifest.keys()):
        e = by_context[context]
        quarantined.append({'context': context, 'previous_source': e.source,
                            'translation': e.translation, 'reason': ['obsolete']})
    if old:
        new.insert(0, Entry(source='', translation=next((e.translation for e in old if not e.context), ''),
                            comments=['# Smart! translation catalog; generated from the current source manifest.']))
    else:
        new.insert(0, Entry(translation=f'Language: {culture}\nContent-Type: text/plain; charset=UTF-8\n'))
    return new, quarantined


def sync_archive(po, archive, policy, manifest=None):
    entries = read_po(po)
    indexed = {}
    for e in entries:
        if not e.context:
            continue
        if e.context in indexed:
            raise ValueError(f'Duplicate PO identity: {e.context}')
        if e.errors:
            raise ValueError(f'Malformed PO {e.context}: {e.errors}')
        indexed[e.context] = e
    if manifest is not None:
        for context,item in manifest.items():
            if context not in indexed or indexed[context].source != item['source']:
                raise ValueError(f'PO does not match current manifest: {context}')
    data = read_json(archive)
    applied = 0
    seen = set()
    for context, child in archive_entries(data):
        seen.add(context)
        e = indexed.get(context)
        source = child['Source']['Text']
        if manifest is not None and context in manifest:
            # Gather can retain an archive's previous source for an existing key.
            # Only the current manifest may authorize replacing it.
            source = manifest[context]['source']
            child['Source']['Text'] = source
        # Clear stale archive translations too: the PO is authoritative.
        translation = ''
        if e and e.source == source and e.translation:
            errors = violations(source, e.translation, policy)
            if errors:
                raise ValueError(f'Unsafe translation {context}: {errors}')
            translation = e.translation
            applied += 1
        child.setdefault('Translation', {})['Text'] = translation
    if manifest is not None and set(manifest) - seen:
        raise ValueError('Archive missing current identities: ' + ', '.join(sorted(set(manifest) - seen)))
    atomic_text(archive, json.dumps(data, ensure_ascii=False, indent='\t') + '\n', encoding='utf-16')
    return applied
