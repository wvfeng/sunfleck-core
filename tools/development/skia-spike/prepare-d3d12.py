"""Verify or apply the exact, hardware-validated Skia D3D12 source patch."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

REVISION = 'e26a2f19555c0cc7cfe5f457d6555bcbf22aaca1'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skia', required=True, type=Path)
    parser.add_argument('--apply', action='store_true', help='Apply only to a clean pinned checkout')
    args = parser.parse_args()
    root = args.skia.resolve(strict=True)
    patch = Path(__file__).with_name('d3d12-fixes.patch').resolve()
    command = ['git', '-c', 'safe.directory=' + root.as_posix(), '-C', str(root)]

    def git(*arguments):
        return subprocess.check_output(command + list(arguments))

    if git('rev-parse', 'HEAD').decode().strip() != REVISION:
        raise RuntimeError('Wrong Skia revision; refusing to prepare an unvalidated candidate')
    paths = [line[6:] for line in patch.read_text().splitlines() if line.startswith('+++ b/')]
    if len(paths) != 4 or any(not path.startswith('src/gpu/ganesh/d3d/') or '..' in Path(path).parts for path in paths):
        raise RuntimeError('Unexpected patch scope')
    # Reconstruct expected bytes independently so reverse-apply success cannot
    # accidentally accept unrelated edits in the same patched source files.
    with tempfile.TemporaryDirectory(prefix='sunfleck-d3d12-') as temporary:
        expected_root = Path(temporary)
        subprocess.run(['git', 'init', '--quiet', temporary], check=True)
        for path in paths:
            destination = expected_root / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(git('show', REVISION + ':' + path))
        subprocess.run(['git', '-C', temporary, 'apply', '--ignore-space-change', str(patch)], check=True)
        normalize = lambda data: data.replace(b'\r\n', b'\n')
        expected = {path: normalize((expected_root / path).read_bytes()) for path in paths}
    changed = set(git('diff', 'HEAD', '--name-only', '-z').decode().strip('\0').split('\0')) - {''}
    matches = all(normalize((root / path).read_bytes()) == content for path, content in expected.items())
    if not matches:
        if not args.apply:
            raise RuntimeError('Validated patch is missing or differs; use --apply on a clean pinned checkout')
        if changed:
            raise RuntimeError('Tracked changes exist; refusing to overwrite local work')
        subprocess.run(command + ['apply', '--check', '--ignore-space-change', str(patch)], check=True)
        subprocess.run(command + ['apply', '--ignore-space-change', str(patch)], check=True)
        if not all(normalize((root / path).read_bytes()) == content for path, content in expected.items()):
            raise RuntimeError('Applied source does not match validated patch')
    elif changed - set(paths):
        raise RuntimeError('Additional tracked source changes are outside the validated patch')
    print(json.dumps({'revision': REVISION, 'status': 'validated-patch-present',
                      'patchSha256': hashlib.sha256(patch.read_bytes()).hexdigest(),
                      'files': {path: hashlib.sha256(content).hexdigest() for path, content in expected.items()}}, indent=2))


if __name__ == '__main__':
    main()
