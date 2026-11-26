import importlib.util
import tempfile
import unittest
import zipfile
from pathlib import Path

spec = importlib.util.spec_from_file_location('package', Path(__file__).resolve().parents[2] / 'tools/sdk/package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

class CandidateIntegrity(unittest.TestCase):
    def test_content_and_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'sdk'; source.mkdir()
            (source / 'a').write_text('verified')
            archive = root / 'sdk.zip'
            expected = package.archive_tree(source, archive)
            with zipfile.ZipFile(archive, 'w') as output: output.writestr('a', 'changed')
            with self.assertRaisesRegex(ValueError, 'hash mismatch'): package.verify_zip(archive, expected)
            with zipfile.ZipFile(archive, 'w') as output: output.writestr('extra', 'verified')
            with self.assertRaisesRegex(ValueError, 'inventory mismatch'): package.verify_zip(archive, expected)

    def test_unsafe_paths(self):
        for name in ('../escape', '/absolute', 'C:/escape', 'a\\b'):
            with self.subTest(name=name), self.assertRaises(ValueError): package.safe_name(name)

    def test_duplicate_members(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'duplicate.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('a', 'one')
                archive.writestr('a', 'two')
            with self.assertRaisesRegex(ValueError, 'inventory mismatch'): package.verify_zip(path, {'a': package.sha(b'two')})

if __name__ == '__main__': unittest.main()
