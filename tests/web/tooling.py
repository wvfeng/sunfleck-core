"""Integrity checks for packaging; no browser/environment expansion."""
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/web'))
import release
from serve import package

class Packaging(unittest.TestCase):
    def test_tampered_original_and_compressed_assets(self):
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp)
            (site / 'main.js').write_text('console.log(1)', encoding='utf-8')
            package(site)
            release.verify_assets(site)
            (site / 'main.js').write_text('console.log(2)', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Asset mismatch'):
                release.verify_assets(site)
            package(site)
            import gzip
            (site / 'main.js.gz').write_bytes(gzip.compress(b'old content'))
            with self.assertRaisesRegex(ValueError, 'Compressed asset mismatch'):
                release.verify_assets(site)

    def test_incomplete_and_escaping_manifest(self):
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp)
            (site / 'main.js').write_text('hello', encoding='utf-8')
            package(site)
            manifest = json.loads((site / 'asset-manifest.json').read_text())
            (site / 'extra.txt').write_text('not listed', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'incomplete'):
                release.verify_assets(site)
            manifest['assets'][0]['path'] = '../outside.js'
            (site / 'asset-manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'Invalid asset path'):
                release.verify_assets(site)

    def test_scaffold_refuses_overwrite(self):
        spec = importlib.util.spec_from_file_location('new_app', ROOT / 'tools/web/new-app.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp) / 'app'
            module.create(site, 'cpp')
            (site / 'main.cpp').write_text('user work', encoding='utf-8')
            with self.assertRaises(FileExistsError):
                module.create(site, 'cpp')
            self.assertEqual((site / 'main.cpp').read_text(), 'user work')

if __name__ == '__main__':
    unittest.main()
