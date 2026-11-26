"""Freeze and seal a local Web candidate without changing Git history or publishing it."""
import argparse
import datetime
import hashlib
import gzip
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path, PurePosixPath
from serve import package

ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOTS = ('src/', 'include/', 'bindings/', 'web/', 'examples/', 'tests/', 'tools/', 'build/')

def digest(data):
    return hashlib.sha256(data).hexdigest()

def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding='utf-8')

def sources():
    paths = subprocess.check_output(['git', '-c', 'core.quotepath=false', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=ROOT).decode('utf-8').split('\0')
    files = {}
    for name in sorted(set(paths)):
        if not (name.startswith(SOURCE_ROOTS) or name in ('CMakeLists.txt', 'CMakePresets.json')):
            continue
        path = ROOT / name
        if path.is_file():
            if path.is_symlink() or not path.resolve().is_relative_to(ROOT):
                raise ValueError(f'External source path: {name}')
            files[name] = digest(path.read_bytes())
    return files

def verify_assets(site):
    manifest = json.loads((site / 'asset-manifest.json').read_text(encoding='utf-8'))
    seen = set()
    for asset in manifest['assets']:
        name = asset['path']
        relative = PurePosixPath(name)
        path = (site / name).resolve()
        if relative.is_absolute() or '\\' in name or '..' in relative.parts or not path.is_relative_to(site.resolve()) or name in seen:
            raise ValueError(f'Invalid asset path: {name}')
        data = path.read_bytes()
        if len(data) != asset['bytes'] or digest(data) != asset['sha256']:
            raise ValueError(f'Asset mismatch: {name}')
        compressed = Path(str(path) + '.gz')
        if compressed.exists() and gzip.decompress(compressed.read_bytes()) != data:
            raise ValueError(f'Compressed asset mismatch: {name}')
        seen.add(name)
    actual = {p.relative_to(site).as_posix() for p in site.rglob('*') if p.is_file() and p.suffix not in ('.gz', '.br') and p.name != 'asset-manifest.json'}
    if seen != actual or not seen:
        raise ValueError('Asset manifest is incomplete')
    return manifest

def freeze(output, cpp, dotnet):
    if any(output.is_relative_to(site) or site.is_relative_to(output) for site in (cpp, dotnet)):
        raise ValueError('Candidate and input site directories must be separate')
    output.mkdir(parents=True, exist_ok=False)
    records = sources()
    write_json(output / 'source-manifest.json', records)
    (output / 'contracts').mkdir()
    for name in ('web-api-support.md', 'web-api-inventory.json', 'web-delivery.md'):
        shutil.copy2(ROOT / 'docs' / name, output / 'contracts' / name)
    with zipfile.ZipFile(output / 'source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in records:
            data = (ROOT / name).read_bytes()
            if digest(data) != records[name]:
                raise ValueError(f'Source changed while freezing: {name}')
            archive.writestr(name, data)
    for name, source in [('cpp', cpp), ('dotnet', dotnet)]:
        site = output / name
        shutil.copytree(source, site, ignore=shutil.ignore_patterns('*.gz', '*.br', 'asset-manifest.json'))
        package(site)
        verify_assets(site)
    write_json(output / 'candidate.json', {
        'version': 1, 'createdUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'gitHead': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'sourceId': digest(json.dumps(records, sort_keys=True).encode()), 'sourceFiles': len(records),
        'sourceScope': 'Tracked and untracked, non-ignored build/source/test inputs; docs and IDE packages excluded.',
        'state': 'frozen-unverified', 'manualAcceptance': 'skipped-by-user',
        'distribution': 'local-validation-only; no font or framework redistribution license inferred'})

def seal(output):
    candidate = json.loads((output / 'candidate.json').read_text(encoding='utf-8'))
    if candidate['state'] != 'frozen-unverified':
        raise ValueError('Candidate has already been sealed')
    if sources() != json.loads((output / 'source-manifest.json').read_text(encoding='utf-8')):
        raise ValueError('Source changed after freezing; create a new candidate')
    with zipfile.ZipFile(output / 'source.zip') as archive:
        records = json.loads((output / 'source-manifest.json').read_text(encoding='utf-8'))
        if set(archive.namelist()) != set(records) or any(digest(archive.read(n)) != h for n, h in records.items()):
            raise ValueError('Source archive mismatch')
    suite = json.loads((output / 'evidence/suite-result.json').read_text(encoding='utf-8'))
    expected = {(h, t, d) for h in ('cpp', 'dotnet') for t, ds in [('smoke', [1]), ('text-input', [1, 2]), ('workspace', [1, 1.25, 2]), ('window-state', [1]), ('renderer', [1]), ('deployment', [1]), ('performance', [1])] for d in ds}
    actual = {(r['host'], r['test'], r['dpr']) for r in suite['cases'] if r['result'] == 'passed'}
    if suite['result'] != 'passed' or actual != expected or len(suite['cases']) != 20:
        raise ValueError('Need the exact complete 20-case suite')
    browsers = set()
    for host, test, dpr in expected:
        result = json.loads((output / f'evidence/complete-{host}-{test}-{dpr}/result.json').read_text(encoding='utf-8'))
        if result['result'] != 'passed':
            raise ValueError('Scenario did not pass')
        browsers.add(result['browser'])
    for name, count in [('native', 25), ('csharp', 1)]:
        tree = ET.parse(output / f'{name}.xml').getroot()
        cases = list(tree.iter('testcase'))
        if len(cases) != count or any(c.find('failure') is not None or c.find('error') is not None or c.find('skipped') is not None for c in cases):
            raise ValueError(f'{name} regression did not pass {count} tests')
    resources = {}
    for name in ('cpp', 'dotnet'):
        site = output / name
        verify_assets(site)
        if name == 'dotnet' and len(list((site / '_framework').glob('dotnet.native.*.wasm'))) != 1:
            raise ValueError('The .NET site contains missing or obsolete native Wasm assets')
        assets = [p for p in site.rglob('*') if p.is_file() and p.suffix not in ('.gz', '.br') and p.name != 'asset-manifest.json']
        wire = sum(Path(str(p) + '.gz').stat().st_size if Path(str(p) + '.gz').exists() else p.stat().st_size for p in assets)
        # A regression ceiling for these samples, not a production performance SLA.
        if wire > 20 * 1024 * 1024:
            raise ValueError(f'{name} exceeds the sample 20 MiB compressed asset budget')
        resources[name] = {'compressedAssetBytes': wire, 'budgetBytes': 20 * 1024 * 1024}
        shutil.make_archive(str(output / name), 'zip', site)
    font = json.loads((output / 'font-report.json').read_text(encoding='utf-8'))
    if font['missing'] or font['gzipBytes'] > 14 * 1024 * 1024:
        raise ValueError('Font coverage/14 MiB compressed sample budget failed')
    for path in (output / 'dotnet/font.ttf', output / 'cpp/sunfleck.data'):
        if digest(path.read_bytes()) != font['sha256']:
            raise ValueError('Candidate does not contain the audited font')
    environment = json.loads((output / 'environment.json').read_text(encoding='utf-8-sig'))
    if not all(environment.get(key) for key in ('dotnetSdk', 'emscripten', 'node', 'python', 'cmake', 'nativeCompiler')):
        raise ValueError('Missing toolchain identity')
    candidate.update(state='automated-validation-passed', browsers=sorted(browsers), cases=20,
                     nativeTests=25, csharpTests=1, resources=resources,
                     sealedUtc=datetime.datetime.now(datetime.timezone.utc).isoformat())
    write_json(output / 'candidate.json', candidate)
    inventory = {p.relative_to(output).as_posix(): digest(p.read_bytes()) for p in sorted(output.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS.json'}
    write_json(output / 'SHA256SUMS.json', inventory)
    print(json.dumps(candidate, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['freeze', 'seal', 'fingerprint'])
    parser.add_argument('output', type=Path, nargs='?')
    parser.add_argument('--cpp', type=Path)
    parser.add_argument('--dotnet', type=Path)
    args = parser.parse_args()
    if args.operation == 'fingerprint':
        print(digest(json.dumps(sources(), sort_keys=True).encode()))
    elif args.output is None:
        parser.error('Output directory is required')
    elif args.operation == 'freeze':
        if not args.cpp or not args.dotnet:
            parser.error('freeze needs --cpp and --dotnet')
        freeze(args.output.resolve(), args.cpp.resolve(), args.dotnet.resolve())
    else:
        seal(args.output.resolve())
