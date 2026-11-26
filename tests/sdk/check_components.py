"""Exercise installed C/C++/C# component tooling without source-tree references.

Run from a Visual Studio developer shell after cmake --install:
  python tests/sdk/check_components.py --sdk out/components-sdk
"""
import argparse
import hashlib
import json
import re
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Debug",
                        help="Match the installed native SDK build configuration")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    sdk = args.sdk.resolve()
    stage = Path(tempfile.mkdtemp(prefix="components-consumer-", dir=root / "out"))
    source = stage / "source"
    shutil.copytree(root / "tests/fixtures/reusable", source,
                    ignore=shutil.ignore_patterns("bin", "obj", "out"))

    def run(*command, expect_failure=False):
        result = subprocess.run([str(arg) for arg in command], cwd=stage,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, encoding="utf-8", errors="replace")
        with (stage / "validation.log").open("a", encoding="utf-8") as log:
            log.write("\n$ " + " ".join(map(str, command)) + "\n" + result.stdout)
        if bool(result.returncode) != expect_failure:
            raise RuntimeError(result.stdout)
        return result.stdout

    cmake = (source / "CMakeLists.txt").read_text(encoding="utf-8")
    start = cmake.index("if(CMAKE_CURRENT_SOURCE_DIR")
    end = cmake.index("endif()", start) + len("endif()")
    cmake = cmake[:start] + "find_package(Sunfleck CONFIG REQUIRED)\ninclude(CTest)" + cmake[end:]
    cmake = cmake.replace("PRIVATE sunfleck)", "PRIVATE Sunfleck::sunfleck)")
    (source / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
    for project in source.rglob("*.csproj"):
        text = project.read_text(encoding="utf-8")
        text = text.replace('<ProjectReference Include="../../../../bindings/csharp/Sunfleck.csproj" />', "")
        text = text.replace('<Import Project="../../../../build/Sunfleck.Components.targets" />',
            '<PropertyGroup><SunfleckEnableComponents>true</SunfleckEnableComponents></PropertyGroup>\n'
            '<Import Project="$(SunfleckSdkRoot)/share/sunfleck/Sunfleck.props" />')
        project.write_text(text, encoding="utf-8")

    build = stage / "native"
    run("cmake", "-S", source, "-B", build, "-G", "Ninja",
        f"-DCMAKE_BUILD_TYPE={args.configuration}", f"-DCMAKE_PREFIX_PATH={sdk.as_posix()}")
    run("cmake", "--build", build, "--parallel", "1")
    run("ctest", "--test-dir", build, "--output-on-failure")
    before = hashlib.sha256((build / "page.flkb").read_bytes()).digest()
    child = source / "controls/UserCard.fleck"
    child.write_text(child.read_text(encoding="utf-8").replace("Independent instance notes", "Updated child notes"), encoding="utf-8")
    run("cmake", "--build", build, "--parallel", "1")
    assert hashlib.sha256((build / "page.flkb").read_bytes()).digest() != before, "Child edit did not rebuild the bundle"
    run("ctest", "--test-dir", build, "--output-on-failure")

    project = source / "csharp/Reusable.csproj"
    (source / "library/NullableReferences.cs").write_text(
        'using Sunfleck; [FleckComponent("urn:test", "NullableReferences")] '
        'public sealed class NullableReferences : UserControl { '
        '[FleckProperty] public PropertyModel? Child { get => Get<PropertyModel?>("Child"); set => Set("Child", value); } '
        '[FleckProperty] public Command? Action { get => Get<Command?>("Action"); set => Set("Action", value); } '
        '[FleckProperty(Effects = PropertyEffects.Bindable | PropertyEffects.Inherits)] '
        'public int Indent { get => Get<int>("Indent"); set => Set("Indent", value); } }', encoding="utf-8")
    run("dotnet", "build", project, f"-p:SunfleckSdkRoot={sdk.as_posix()}", "--configuration", args.configuration, "--nologo")
    manifests = list((source / "library/obj").rglob("FleckComponents.g.cs"))
    assert manifests, "Generated component manifest missing"
    declaration = re.search(r'FleckManifestAttribute\(("(?:[^"\\]|\\.)*")\)', manifests[0].read_text(encoding="utf-8-sig"))
    assert declaration, "Generated manifest attribute missing"
    manifest = json.loads(json.loads(declaration.group(1)))
    inherited = next(t for t in manifest["types"] if t["name"] == "NullableReferences")
    indent = next(p for p in inherited["properties"] if p["name"] == "Indent")
    assert indent["flags"] == 129, "Generated metadata lost Bindable/Inherits flags"
    # Class is authoritative even without a duplicate FleckComponent attribute.
    card_type = source / "library/UserCard.cs"
    card_text = card_type.read_text(encoding="utf-8")
    card_type.write_text(card_text.replace('[FleckComponent("urn:example:controls", "UserCard")]', ''), encoding="utf-8")
    run("dotnet", "build", project, f"-p:SunfleckSdkRoot={sdk.as_posix()}", "--configuration", args.configuration, "--nologo")
    # A layout-only component still has a real generated, referenceable type.
    card_type.unlink()
    run("dotnet", "build", project, f"-p:SunfleckSdkRoot={sdk.as_posix()}", "--configuration", args.configuration, "--nologo")
    generated = list((source / "library/obj").rglob("Example.UserCard.g.cs"))
    assert generated and "class UserCard : global::Sunfleck.UserControl" in generated[0].read_text(encoding="utf-8-sig")
    bad = source / "library/Invalid.cs"
    bad.write_text('using Sunfleck; [FleckComponent("urn:invalid", "Invalid")] '
                   'public abstract class Invalid : Control {}', encoding="utf-8")
    error = run("dotnet", "build", project, f"-p:SunfleckSdkRoot={sdk.as_posix()}", "--configuration", args.configuration, "--nologo", expect_failure=True)
    assert "FLK001" in error, error
    bad.write_text('using Sunfleck; [FleckComponent("urn:invalid", "Invalid")] '
                   'public sealed class Invalid : Control { [FleckProperty(Default="wrong")] '
                   'public int Value { get; set; } }', encoding="utf-8")
    error = run("dotnet", "build", project, f"-p:SunfleckSdkRoot={sdk.as_posix()}", "--configuration", args.configuration, "--nologo", expect_failure=True)
    assert "FLK001" in error and "Default value" in error, error
    bad.unlink()
    run("dotnet", "build", project, f"-p:SunfleckSdkRoot={sdk.as_posix()}", "--configuration", args.configuration, "--nologo")

    # Published consumers must execute using embedded definitions only.
    for xml in source.rglob("*.fleck"):
        xml.unlink()
    run(build / "reusable_c.exe", "--test", build / "page.flkb", build / "page.flkb.fleckmap")
    run(build / "reusable_cpp.exe", "--test")
    run("dotnet", source / f"csharp/bin/{args.configuration}/net10.0/Reusable.dll", "--test")
    print("Installed components passed: C/C++, cross-project C#, incremental child rebuild, FLK001, execution without XML.")
    print(f"Evidence: {stage}")


if __name__ == "__main__":
    main()
