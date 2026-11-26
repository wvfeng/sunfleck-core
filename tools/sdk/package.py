"""Seal a verified installed SDK as a local candidate with source identity and SPDX."""
import argparse
import datetime
import hashlib
import json
import re
import shutil
import subprocess
import uuid
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOTS = ('src/', 'include/', 'bindings/', 'build/', 'tools/', 'tests/', 'docs/', 'demos/', 'examples/', 'web/')

def sha(data): return hashlib.sha256(data).hexdigest()

def write(path, value): path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding='utf-8')

def build_inputs():
    paths = [ROOT / 'CMakeLists.txt']
    for directory in ('src', 'include', 'build', 'bindings/csharp', 'tools'):
        paths += [p for p in (ROOT / directory).rglob('*') if p.suffix in ('.cpp', '.c', '.h', '.hpp', '.hlsl', '.cmake', '.in', '.cs', '.csproj')
                  and not {'bin', 'obj', 'node_modules', '__pycache__'}.intersection(p.relative_to(ROOT).parts)]
    return {p.relative_to(ROOT).as_posix(): sha(p.read_bytes()) for p in sorted(set(paths)) if p.is_file()}

def inventory(root):
    result = {}
    for path in sorted(root.rglob('*')):
        if path.is_symlink(): raise ValueError(f'Symlink not accepted: {path}')
        if path.is_file(): result[path.relative_to(root).as_posix()] = sha(path.read_bytes())
    return result

def safe_name(name):
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name or not path.parts:
        raise ValueError(f'Unsafe archive path: {name}')
    return path

def verify_zip(path, expected):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        for name in names: safe_name(name)
        if len(set(names)) != len(names) or set(names) != set(expected): raise ValueError('ZIP inventory mismatch')
        for name, digest in expected.items():
            if sha(archive.read(name)) != digest: raise ValueError(f'ZIP hash mismatch: {name}')

def archive_tree(source, output):
    files = inventory(source)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in files: archive.write(source / name, name)
    verify_zip(output, files)
    return files

def verify(output):
    manifest = json.loads((output / 'candidate.json').read_text(encoding='utf-8'))
    for name, expected in manifest['files'].items():
        safe_name(name)
        if sha((output / name).read_bytes()) != expected: raise ValueError(f'Candidate hash mismatch: {name}')
    actual = set(inventory(output)) - {'candidate.json'}
    if actual != set(manifest['files']): raise ValueError('Candidate has missing or unlisted files')
    verify_zip(output / 'sdk.zip', manifest['sdkFiles'])
    verify_zip(output / 'source.zip', manifest['sourceFiles'])
    return manifest

def create(evidence, output, performance=None):
    result = json.loads((evidence / 'result.json').read_text(encoding='utf-8-sig'))
    if result['status'] != 'passed' or not result.get('files'): raise ValueError('Require passed SDK evidence with installed file hashes')
    if result.get('buildInputs') != build_inputs(): raise ValueError('Build sources changed after validation')
    sdk = Path(result['sdk'])
    if inventory(sdk) != result['files']: raise ValueError('SDK changed after validation')
    if not (sdk / 'share/sunfleck/LICENSE').read_text().startswith('MIT License'): raise ValueError('MIT license is missing')
    output.mkdir(parents=True, exist_ok=False)
    write(output / 'validation.json', result)
    shutil.copyfile(evidence / 'consumers.xml', output / 'consumers.xml')
    if performance:
        baseline = json.loads(performance.read_text(encoding='utf-8'))
        if baseline.get('renderer') != 'headless-software' or len(baseline.get('runs', [])) < 6:
            raise ValueError('Invalid CPU baseline report')
        write(output / 'performance.json', baseline)
    imports = json.loads((evidence / 'pe-dependencies.json').read_text(encoding='utf-8-sig'))
    write(output / 'dependencies.json', imports)
    sdk_files = archive_tree(sdk, output / 'sdk.zip')
    names = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=ROOT).decode('utf-8').split('\0')
    sources = {}
    with zipfile.ZipFile(output / 'source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(set(names)):
            if not (name.startswith(SOURCE_ROOTS) or name in ('LICENSE', 'CMakeLists.txt', 'CMakePresets.json', 'README.md')): continue
            path = ROOT / name
            if not path.is_file(): continue
            safe_name(name)
            if path.is_symlink() or not path.resolve().is_relative_to(ROOT): raise ValueError('External source path')
            data = path.read_bytes(); sources[name] = sha(data); archive.writestr(name, data)
    verify_zip(output / 'source.zip', sources)
    source_identity = sha(json.dumps(sources, sort_keys=True).encode())
    created = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    package_id = 'SPDXRef-Sunfleck'
    version_file = sdk / 'lib/cmake/Sunfleck/SunfleckConfigVersion.cmake'
    version = re.search(r'set\(PACKAGE_VERSION "([^"]+)"\)', version_file.read_text(encoding='utf-8'))
    if not version: raise ValueError('Installed SDK version is missing')
    packages = [{'SPDXID': package_id, 'name': 'Sunfleck Windows x64 SDK', 'versionInfo': version[1],
                 'downloadLocation': 'NOASSERTION', 'filesAnalyzed': False, 'licenseDeclared': 'MIT',
                 'licenseConcluded': 'NOASSERTION', 'copyrightText': 'Copyright (c) 2026 Sunfleck contributors'}]
    files = [{'SPDXID': f'SPDXRef-File-{i}', 'fileName': './' + name,
              'checksums': [{'algorithm': 'SHA256', 'checksumValue': digest}], 'licenseConcluded': 'NOASSERTION',
              'copyrightText': 'NOASSERTION'} for i, (name, digest) in enumerate(sdk_files.items())]
    relationships = [{'spdxElementId': 'SPDXRef-DOCUMENT', 'relationshipType': 'DESCRIBES', 'relatedSpdxElement': package_id}]
    relationships += [{'spdxElementId': package_id, 'relationshipType': 'CONTAINS', 'relatedSpdxElement': file['SPDXID']} for file in files]
    for index, name in enumerate(sorted({x for entries in imports.values() for x in entries} | {'.NET 10 Runtime'})):
        ref = f'SPDXRef-External-{index}'
        packages.append({'SPDXID': ref, 'name': name, 'downloadLocation': 'NOASSERTION', 'filesAnalyzed': False,
                         'licenseDeclared': 'NOASSERTION', 'licenseConcluded': 'NOASSERTION', 'copyrightText': 'NOASSERTION',
                         'comment': 'External runtime requirement; not included in SDK. Direct PE import or .NET framework dependency; transitive OS dependencies are platform supplied.'})
        relationships.append({'spdxElementId': package_id, 'relationshipType': 'DEPENDS_ON', 'relatedSpdxElement': ref})
    write(output / 'sbom.spdx.json', {'spdxVersion': 'SPDX-2.3', 'dataLicense': 'CC0-1.0', 'SPDXID': 'SPDXRef-DOCUMENT',
          'name': output.name, 'documentNamespace': 'https://spdx.org/spdxdocs/sunfleck-' + str(uuid.uuid4()),
          'creationInfo': {'created': created, 'creators': ['Tool: Sunfleck SDK packager']},
          'documentComment': 'Scope: this installed native Windows SDK. Web sites and VSIX dependencies are excluded.',
          'packages': packages, 'files': files, 'relationships': relationships})
    write(output / 'candidate.json', {'status': 'local-validation-passed', 'createdUtc': created, 'license': 'MIT',
          'sourceIdentity': source_identity, 'sdkFiles': sdk_files, 'sourceFiles': sources, 'files': inventory(output)})
    verify(output)
    return output

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('output', type=Path)
    parser.add_argument('--evidence', type=Path)
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--snapshot', action='store_true')
    parser.add_argument('--performance', type=Path)
    args = parser.parse_args()
    if args.snapshot:
        write(args.output, build_inputs())
        return
    if args.verify: verify(args.output)
    elif args.evidence: create(args.evidence, args.output, args.performance)
    else: parser.error('Provide --evidence to create, or --verify')
    print(f'Candidate verified: {args.output}')

if __name__ == '__main__': main()
