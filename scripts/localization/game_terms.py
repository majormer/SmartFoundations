"""Read versioned official locres dictionaries; never infer translations from English."""
import csv
import io
import hashlib
from pathlib import Path
import struct
from catalog import digest, write_json


def read_locres(path):
    data = Path(path).read_bytes()
    stream = io.BytesIO(data)
    def number(fmt):
        size = struct.calcsize(fmt)
        value = stream.read(size)
        if len(value) != size:
            raise ValueError('Truncated locres')
        return struct.unpack(fmt, value)[0]
    def string():
        count = number('<i')
        if abs(count) > len(data):
            raise ValueError('Invalid locres string length')
        raw = stream.read(abs(count) * (2 if count < 0 else 1))
        if count == 0:
            return ''
        return raw.decode('utf-16-le' if count < 0 else 'utf-8').rstrip('\0')
    magic = struct.pack('<IIII', 0x7574140E, 0xFC034A67, 0x9D90154A, 0x1B7F37C3)
    if stream.read(16) != magic:
        raise ValueError('Unsupported legacy locres')
    version = number('<B')
    if version not in (1, 2, 3):
        raise ValueError(f'Unsupported locres version {version}')
    offset = number('<q')
    position = stream.tell()
    stream.seek(offset)
    strings = []
    for _ in range(number('<i')):
        strings.append(string())
        if version >= 2:
            number('<i') # reference count
    stream.seek(position)
    if version >= 2:
        number('<I') # entry count
    result = {}
    for _ in range(number('<I')):
        if version >= 2:
            number('<I') # TextKey serialized hash (32-bit in this engine)
        namespace = string()
        for _ in range(number('<I')):
            if version >= 2:
                number('<I')
            key = string()
            source_hash = number('<I')
            index = number('<i')
            if not 0 <= index < len(strings):
                raise ValueError('Invalid locres string index')
            result[(namespace, key)] = (strings[index], source_hash)
    return result


def extract(resource_root, csv_root, output, policy, game_version):
    if not Path(resource_root).is_dir() or not Path(csv_root).is_dir():
        raise ValueError('Official resource and string-table directories must exist')
    tables = {}
    for path in Path(csv_root).glob('*.csv'):
        with path.open(encoding='utf-8-sig', newline='') as f:
            for row in csv.DictReader(f):
                if row.get('Key') and row.get('SourceString') and not row['Key'].endswith('/Description'):
                    tables[(path.stem, row['Key'])] = row['SourceString']
    result = {'game_version': game_version, 'cultures': {}, 'terms': {}}
    for culture in policy['cultures']:
        game_culture = policy.get('game_culture_aliases', {}).get(culture, culture)
        path = Path(resource_root) / game_culture / 'AllStringTables.locres'
        if not path.exists():
            result['cultures'][culture] = {'status': 'missing-resource'}
            continue
        localized = read_locres(path)
        result['cultures'][culture] = {'status': 'extracted', 'resource_sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'game_culture': game_culture}
        for (namespace, key), english in tables.items():
            # Match actual namespace/key pairs only. No fuzzy English-string matching.
            value = localized.get((namespace, key))
            if value:
                term = result['terms'].setdefault(namespace + ':' + key, {'english': english, 'translations': {}})
                term['translations'][culture] = value[0]
    if not result['terms']:
        raise ValueError('No official namespace/key matches; refusing an empty glossary')
    for term in result['terms'].values():
        if term['translations'].get('en'):
            term['english'] = term['translations']['en']
    write_json(output, result)
    return result
