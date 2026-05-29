import os, re, sys

root = os.path.join(os.path.dirname(__file__), '..', 'Content', 'Localization', 'SmartFoundations')
os.chdir(root)
langs = sorted([d for d in os.listdir('.') if os.path.isdir(d)])

def parse(po):
    txt = open(po, encoding='utf-8').read()
    keys = {}
    for blk in txt.split('\n\n'):
        mc = re.search(r'msgctxt "(.*?)"', blk, re.S)
        ms = re.search(r'msgstr "((?:[^"\\]|\\.)*)"', blk)
        if mc and ms:
            keys[mc.group(1)] = ms.group(1)
    return keys

data = {}
for l in langs:
    po = os.path.join(l, 'SmartFoundations.po')
    if os.path.exists(po):
        data[l] = parse(po)

en = set(data['en'].keys())
union = set()
for l in data:
    union |= set(data[l].keys())

print("en source keys=%d  union across all=%d" % (len(en), len(union)))
print("%-8s %5s %12s %11s %12s %7s" % ('lang','keys','missingVsEn','emptyMsgstr','staleNotInEn','locres'))
for l in langs:
    if l not in data:
        continue
    k = set(data[l].keys())
    missing = len(en - k)
    empty = sum(1 for kk, v in data[l].items() if v == '' and kk != '')
    stale = len(k - en)
    locres = 'yes' if os.path.exists(os.path.join(l, 'SmartFoundations.locres')) else 'NO'
    print("%-8s %5d %12d %11d %12d %7s" % (l, len(k), missing, empty, stale, locres))

# Write detailed gap report for laggards
out = open(os.path.join(os.path.dirname(__file__), 'loc_gaps.txt'), 'w', encoding='utf-8')
for l in langs:
    if l == 'en' or l not in data:
        continue
    k = set(data[l].keys())
    missing = sorted(en - k)
    empty = sorted([kk for kk, v in data[l].items() if v == '' and kk != ''])
    if missing or empty:
        out.write("=== %s : missing=%d empty=%d ===\n" % (l, len(missing), len(empty)))
        for m in missing:
            out.write("  MISSING %s | en=%r\n" % (m, data['en'].get(m, '')))
        for e in empty:
            out.write("  EMPTY   %s | en=%r\n" % (e, data['en'].get(e, '')))
        out.write("\n")
out.close()
print("\nDetailed gaps written to scripts/loc_gaps.txt")
