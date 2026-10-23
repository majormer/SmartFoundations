import json
from pathlib import Path
import tempfile
import unittest
import struct
from catalog import Entry, read_po, write_po, reconcile, sync_archive, violations, is_untranslated_prose
import wiki
import game_terms
from verify_all import verify, verify_official_terms, verify_wiki_interface, verify_compiled

POLICY = {'native': 'en', 'cultures': {'en': 'English', 'de': 'Deutsch', 'ar': 'العربية'},
          'protected': ['Smart!', 'Smart Camera', 'Smart', 'HUD'], 'rtl': ['ar']}


class LocalizationTests(unittest.TestCase):
    def test_gather_conflict_cannot_pass_complete_catalog(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            base = root / 'Content/Localization/Test'
            base.mkdir(parents=True)
            (base / 'Test.manifest').write_text(json.dumps({'Namespace': 'Test', 'Children': [
                {'Source': {'Text': 'Cancelled by user'}, 'Keys': [{'Key': 'Cancelled'}]}
            ]}))
            write_po(base / 'de/Test.po', [Entry('Test,Cancelled', 'Cancelled by user', 'Vom Benutzer abgebrochen')])
            policy = dict(POLICY, cultures=['en', 'de'])
            report = base / 'Test_Conflicts.txt'
            report.write_text('Test - Cancelled\nConflicting cancellation message\n')
            self.assertIn(['*', 'gather-source-conflicts'], verify(root, 'Test', policy)['de']['issues'])
            report.write_text('')
            self.assertEqual(verify(root, 'Test', policy)['de']['issues'], [])

    def test_verbatim_credit_exception_is_bound_to_exact_source(self):
        source='- FirstHandle\n- SecondHandle\n- ThirdHandle'
        policy={'wiki':{'verbatim_source_hashes':{wiki.digest(source):'Reviewed account names'}}}
        self.assertTrue(wiki.is_verbatim(source,policy))
        self.assertFalse(wiki.is_verbatim(source+'\n- NewHandle',policy))

    def test_historical_product_brand_remains_english(self):
        policy=json.loads(Path(__file__).with_name('policy.json').read_text(encoding='utf-8'))
        self.assertIn('protected:Smart! Foundations', violations('Smart! Foundations settings', 'Smart! Fundamente-Einstellungen', policy))
        self.assertEqual(violations('Smart! Foundations settings', 'Smart! Foundations-Einstellungen', policy), [])

    def test_console_command_arguments_are_not_translated(self):
        policy = json.loads(Path(__file__).with_name('policy.json').read_text(encoding='utf-8'))
        source = 'Use sc.Camera.SaveProfile <namespace> <camera-id> save.'
        broken = 'Utilisez sc.Camera.SaveProfile <namespace> <camera-id>.'
        self.assertIn('protected:sc.Camera.SaveProfile <namespace> <camera-id> save', violations(source, broken, policy))
        self.assertEqual(violations(source, 'Utilisez sc.Camera.SaveProfile <namespace> <camera-id> save.', policy), [])

    def test_configuration_identifiers_remain_exact(self):
        policy = json.loads(Path(__file__).with_name('policy.json').read_text(encoding='utf-8'))
        source = 'Namespace: SmartFoundations, SmartCamera or SpaceElevatorCamera.'
        self.assertEqual(violations(source, 'Namensraum: SmartFoundations, SmartCamera oder SpaceElevatorCamera.', policy), [])
        self.assertIn('protected:SmartCamera', violations(source, 'Namensraum: SmartFoundations, Smart Camera oder SpaceElevatorCamera.', policy))

    def test_compiled_resource_must_match_translator_catalog(self):
        entries=[Entry('Panel,Help','Help','Hilfe'),Entry('AssetKey','Close','Schließen')]
        resource={('Panel','Help'):('Old text',123),('','AssetKey'):('Schließen',456)}
        self.assertEqual(verify_compiled(entries,resource),[['Panel,Help','compiled-translation-mismatch']])
        resource[('Panel','Help')]=('Hilfe',123)
        self.assertEqual(verify_compiled(entries,resource),[])

    def test_wiki_interface_is_part_of_translation_coverage(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            (root/'chrome.json').write_text(json.dumps({'en':{'languages':'Languages','fallback':'Missing {count} sections'},'de':{'languages':'Sprachen','fallback':'Abschnitte fehlen'}}))
            result=verify_wiki_interface(root,POLICY)
            self.assertIn(['fallback','format-arguments'],result['de']['issues'])
            self.assertEqual(result['ar']['complete'],0)

    def test_english_prose_is_not_completed_translation(self):
        self.assertTrue(is_untranslated_prose('Change the grid spacing', 'Change the grid spacing', POLICY))
        self.assertFalse(is_untranslated_prose('Smart! HUD `{Key}`', 'Smart! HUD `{Key}`', POLICY))
        self.assertFalse(is_untranslated_prose('Change the grid spacing', 'Rasterabstand ändern', POLICY))

    def test_literal_backslash_does_not_replace_line_break(self):
        self.assertIn('line-breaks', violations('Cost\n{0}', 'Kosten\\n{0}', POLICY))
        self.assertEqual(violations('Cost\n{0}', 'Kosten\n{0}', POLICY), [])

    def test_official_name_mismatch_blocks_acceptance(self):
        entries=[Entry('Smart,Name','Conveyor Lift','Ascenseur')]
        terms={'Game:Lift':{'english':'Conveyor Lift','translations':{'fr':'Convoyeur vertical'}}}
        bindings={'Smart,Name':'Game:Lift'}
        self.assertEqual(verify_official_terms(entries,terms,bindings,'fr'),[['Smart,Name','official-term-mismatch']])
        entries[0].translation='Convoyeur vertical'
        self.assertEqual(verify_official_terms(entries,terms,bindings,'fr'),[])

    def test_bare_smart_brand_is_preserved(self):
        self.assertEqual(violations('Smart controls', 'Smart-Steuerung', POLICY), [])
        self.assertIn('protected:Smart', violations('Smart controls', 'Intelligente Steuerung', POLICY))

    def test_multiline_unicode_and_malformed_quote(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'test.po'
            path.write_text('msgctxt "N,K"\nmsgid "Hello "\n"{Name}"\nmsgstr "مرحبا {Name}"\n\nmsgctxt "Broken"\nmsgid "bad \\\"\nmsgstr "ok"\n', encoding='utf-8')
            entries = read_po(path)
            self.assertEqual(entries[0].source, 'Hello {Name}')
            self.assertEqual(entries[0].translation, 'مرحبا {Name}')
            self.assertTrue(entries[1].errors)

    def test_namespace_identity_and_stale_archive_cleared(self):
        with tempfile.TemporaryDirectory() as d:
            p, a = Path(d) / 'test.po', Path(d) / 'test.archive'
            write_po(p, [Entry('A,Key', 'One', 'Eins'), Entry('B,Key', 'Old', 'Alt')])
            a.write_text(json.dumps({'Subnamespaces': [{'Namespace': 'A', 'Children': [{'Key': 'Key', 'Source': {'Text': 'One'}}]}, {'Namespace': 'B', 'Children': [{'Key': 'Key', 'Source': {'Text': 'Two'}, 'Translation': {'Text': 'wrong'}}]}]}))
            sync_archive(p, a, POLICY)
            data = json.loads(a.read_text(encoding='utf-16'))
            self.assertEqual(data['Subnamespaces'][0]['Children'][0]['Translation']['Text'], 'Eins')
            self.assertEqual(data['Subnamespaces'][1]['Children'][0]['Translation']['Text'], '')

    def test_manifest_repairs_stale_archive_source_without_losing_translation(self):
        with tempfile.TemporaryDirectory() as d:
            p,a=Path(d)/'test.po',Path(d)/'test.archive'
            write_po(p,[Entry('Panel,Help','New help','Neue Hilfe')])
            a.write_text(json.dumps({'Namespace':'Panel','Children':[{'Key':'Help','Source':{'Text':'Old help'},'Translation':{'Text':'Old translation'}}]}))
            self.assertEqual(sync_archive(p,a,POLICY,{'Panel,Help':{'source':'New help'}}),1)
            child=json.loads(a.read_text(encoding='utf-16'))['Children'][0]
            self.assertEqual(child['Source']['Text'],'New help')
            self.assertEqual(child['Translation']['Text'],'Neue Hilfe')

    def test_manifest_refuses_stale_po_without_changing_archive(self):
        with tempfile.TemporaryDirectory() as d:
            p,a=Path(d)/'test.po',Path(d)/'test.archive'
            write_po(p,[Entry('Panel,Help','Old help','Alte Hilfe')])
            a.write_text(json.dumps({'Namespace':'Panel','Children':[{'Key':'Help','Source':{'Text':'Old help'}}]}))
            before=a.read_bytes()
            with self.assertRaisesRegex(ValueError,'PO does not match current manifest'):
                sync_archive(p,a,POLICY,{'Panel,Help':{'source':'New help'}})
            self.assertEqual(a.read_bytes(),before)

    def test_reconcile_quarantines_without_losing_translation(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'test.po'
            write_po(p, [Entry('N,K', 'Old', 'Bisher')])
            entries, history = reconcile(p, {'N,K': {'source': 'New', 'locations': []}}, 'de', POLICY)
            self.assertEqual(entries[-1].translation, '')
            self.assertEqual(history[0]['translation'], 'Bisher')

    def test_format_and_brand_gates(self):
        self.assertEqual(violations('Smart! HUD {Name}: {0}', 'Smart! HUD {0}: {Name}', POLICY), [])
        self.assertIn('format-arguments', violations('{Name} {Name}', '{Name}', POLICY))
        self.assertIn('protected:HUD', violations('HUD', 'Anzeige', POLICY))
        self.assertIn('rich-text-tags', violations('<b>{0}</>', '{0}', POLICY))

    def test_fuzzy_translation_is_not_accepted(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'test.po'
            p.write_text('#, fuzzy\nmsgctxt "N,K"\nmsgid "Text"\nmsgstr "Text"\n')
            self.assertTrue(read_po(p)[0].errors)

    def test_official_locres_identity_and_unicode(self):
        def string(value):
            encoded = value.encode('utf-16-le') + b'\0\0'
            return struct.pack('<i', -len(encoded) // 2) + encoded
        header = struct.pack('<IIIIB', 0x7574140E, 0xFC034A67, 0x9D90154A, 0x1B7F37C3, 3)
        body = struct.pack('<III', 1, 1, 0) + string('Buildables_Data')
        body += struct.pack('<II', 1, 0) + string('Production/Constructor') + struct.pack('<Ii', 123, 0)
        data = header + struct.pack('<q', len(header) + 8 + len(body)) + body
        data += struct.pack('<i', 1) + string('製作機') + struct.pack('<i', 1)
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'test.locres'; p.write_bytes(data)
            self.assertEqual(game_terms.read_locres(p)[('Buildables_Data', 'Production/Constructor')], ('製作機', 123))

    def test_wiki_stale_translation_and_locale_links(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); src = root / 'wiki'; src.mkdir()
            (src / 'Home.md').write_text('# Home\n\nRead [Help](Help).', encoding='utf-8')
            (src / 'Help.md').write_text('# Help', encoding='utf-8')
            cat, out = root / 'catalogs', root / 'output'
            manifest = wiki.extract(src, cat, POLICY)
            data = json.loads((cat / 'de.json').read_text())
            data['Home:0'].update(text='# Start', status='reviewed')
            (cat / 'de.json').write_text(json.dumps(data))
            wiki.build(src, cat, out, POLICY)
            self.assertIn('# Start', (out / 'de-Home.md').read_text())
            self.assertIn('(de-Help)', (out / 'de-Home.md').read_text())
            (src / 'Home.md').write_text('# Changed\n\nRead [Help](Help).')
            with self.assertRaises(ValueError): wiki.build(src, cat, out, POLICY)
            wiki.extract(src, cat, POLICY)
            wiki.build(src, cat, out, POLICY)
            self.assertNotIn('# Start', (out / 'de-Home.md').read_text())
            self.assertIn('# Changed', (out / 'de-Home.md').read_text())

    def test_wiki_identity_survives_inserted_section(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); src = root / 'wiki'; src.mkdir()
            (src / 'Home.md').write_text('# Home\n\nOriginal block')
            before = wiki.extract(src, root / 'catalogs', POLICY)
            key = next(k for k, v in before.items() if v['source'] == 'Original block')
            (src / 'Home.md').write_text('# Home\n\nNew block\n\nOriginal block')
            after = wiki.extract(src, root / 'catalogs', POLICY)
            self.assertEqual(after[key]['source'], 'Original block')

    def test_draft_never_leaks_into_default_wiki(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); src = root / 'wiki'; src.mkdir()
            (src / 'Home.md').write_text('# Home')
            cat = root / 'catalogs'
            wiki.extract(src, cat, POLICY)
            data = json.loads((cat / 'de.json').read_text())
            data['Home:0']['text'] = '# Start'
            (cat / 'de.json').write_text(json.dumps(data))
            wiki.build(src, cat, root / 'release', POLICY)
            wiki.build(src, cat, root / 'review', POLICY, include_drafts=True)
            self.assertNotIn('# Start', (root / 'release/de-Home.md').read_text())
            self.assertIn('# Start', (root / 'review/de-Home.md').read_text())
            self.assertIn('Do not publish', (root / 'review/de-Home.md').read_text())


if __name__ == '__main__': unittest.main()
