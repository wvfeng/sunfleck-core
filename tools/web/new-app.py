"""Create an editable application consuming this checkout; never overwrite a directory."""
import argparse
import html
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def create(directory, language):
    directory = directory.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    common = ['index.html', 'workspace.fleck', 'message.txt']
    for name in common:
        shutil.copy2(ROOT / 'web' / name, directory / name)
    if language == 'cpp':
        for name in ['main.cpp', 'main.js']:
            shutil.copy2(ROOT / 'web' / name, directory / name)
        framework = ROOT.as_posix().replace('"', '\\"').replace('$', '\\$').replace(';', '\\;')
        cmake = f'''cmake_minimum_required(VERSION 3.25)
project(SunfleckApplication LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(SUNFLECK_ROOT "{framework}" CACHE PATH "Framework checkout")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(UI_BUILD_WEB OFF CACHE BOOL "" FORCE)
set(UI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(UI_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(UI_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(UI_BUILD_CSHARP OFF CACHE BOOL "" FORCE)
add_subdirectory("${{SUNFLECK_ROOT}}" framework)
include("${{SUNFLECK_ROOT}}/web/SunfleckWeb.cmake")
sunfleck_add_web_app(application SOURCE "${{CMAKE_CURRENT_SOURCE_DIR}}/main.cpp"
  FONT "${{UI_WEB_FONT}}" ASSET_DIRECTORY "${{CMAKE_CURRENT_SOURCE_DIR}}")
'''
        (directory / 'CMakeLists.txt').write_text(cmake, encoding='utf-8')
    else:
        for name in ['Program.cs', 'main.js']:
            shutil.copy2(ROOT / 'web/dotnet' / name, directory / name)
        project = (ROOT / 'web/dotnet/Sunfleck.Web.csproj').read_text(encoding='utf-8')
        project = project.replace('../../', html.escape(ROOT.as_posix() + '/', quote=True))
        for name in common:
            project = project.replace(html.escape(ROOT.as_posix() + '/web/' + name, quote=True), name)
        (directory / 'Application.csproj').write_text(project, encoding='utf-8')
    (directory / 'README.md').write_text(
        '# Sunfleck browser application\n\n'
        'Edit main.cpp / Program.cs for application behavior, workspace.fleck for the workspace, '
        'and index.html for the page shell. The copied counter/workspace are working starting points.\n\n'
        f'Framework: `{ROOT.as_posix()}`. See its docs/web-delivery.md for build and package commands. '
        'No font or redistribution license is bundled. Shared host.js/text-input.js/renderer.js are consumed from the framework.\n',
        encoding='utf-8')
    print(directory)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--language', choices=['cpp', 'dotnet'], required=True)
    args = parser.parse_args()
    create(args.directory, args.language)
