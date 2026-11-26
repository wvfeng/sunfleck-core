"""Create one language SDK and its separate editor tools from installed artifacts."""
import argparse
import json
import shutil
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--language', choices=['c', 'cpp', 'csharp'], required=True)
parser.add_argument('--sdk', type=Path, required=True)
parser.add_argument('--tools', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
a = parser.parse_args()
# Refuse to merge with an older SDK: callers review/replace the explicit directory.
a.output.mkdir(parents=True, exist_ok=False)
sdk = a.output / 'sdk'

def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir(): shutil.copytree(source, target)
    else: shutil.copy2(source, target)

copy(a.sdk/'share/sunfleck/LICENSE', sdk/'LICENSE')
if a.language in ('c', 'cpp'):
    for file in ['bin/fleckc.exe', 'bin/sunfleck_native.dll', 'lib/sunfleck.lib', 'lib/sunfleck_native.lib', 'lib/cmake']:
        copy(a.sdk/file, sdk/file)
    headers = ['ui.h', 'elements.h', 'component_attributes.h'] if a.language == 'c' else [p.name for p in (a.sdk/'include/sunfleck').iterdir() if p.suffix == '.hpp'] + ['ui.h']
    for file in headers: copy(a.sdk/'include/sunfleck'/file, sdk/'include/sunfleck'/file)
    source = 'include/sunfleck/elements.' + ('h' if a.language == 'c' else 'hpp')
else:
    for file in ['bin/sunfleck_native.dll', 'lib/net10.0/Sunfleck.dll', 'share/sunfleck/Sunfleck.props', 'share/sunfleck/Sunfleck.Components.targets']:
        copy(a.sdk/file, sdk/file)
    for file in (a.sdk/'lib/sunfleck/components').iterdir():
        if file.suffix != '.pdb' and not file.name.endswith('.runtimeconfig.dev.json'):
            copy(file, sdk/'lib/sunfleck/components'/file.name)
    source = 'types/Elements.cs'
    for file in (a.sdk/'share/sunfleck/types').glob('*.cs'):
        copy(file, sdk/'types'/file.name)
    copy(a.sdk/'share/sunfleck/types/Sunfleck.Navigation.csproj', sdk/'types/Sunfleck.Navigation.csproj')
manifest = json.loads((a.sdk/'share/sunfleck/types'/ (a.language+'.json')).read_text())
manifest['source'] = source
for attribute in manifest['componentAttributes']:
    attribute['source'] = {'c': 'include/sunfleck/component_attributes.h', 'cpp': 'include/sunfleck/components.hpp', 'csharp': 'types/Components.cs'}[a.language]
(sdk/'fleck.types.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
# The language service is shared .NET tooling, not a language SDK or an app dependency.
for file in ['Sunfleck.Development.dll', 'Sunfleck.Development.deps.json', 'Sunfleck.Development.runtimeconfig.json', 'Sunfleck.dll', 'Sunfleck.ComponentGenerator.dll', 'Microsoft.CodeAnalysis.dll', 'Microsoft.CodeAnalysis.CSharp.dll', 'sunfleck_native.dll']:
    copy(a.tools/file, a.output/'.tools/fleck'/file)
copy(a.sdk/'share/sunfleck/LICENSE', a.output/'.tools/fleck/LICENSE')
(a.output/'fleck.project.json').write_text(json.dumps(dict(version=1, language=a.language, sdk='sdk'), indent=2)+'\n')
print(a.output)
