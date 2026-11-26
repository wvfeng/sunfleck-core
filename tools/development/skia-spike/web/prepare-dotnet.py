"""Create an isolated copy of the real .NET browser application under out/."""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET

p = argparse.ArgumentParser()
p.add_argument("--root", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
p.add_argument("--native", type=Path, required=True)
p.add_argument("--bridge", type=Path, required=True)
p.add_argument("--skia", type=Path, required=True)
args = p.parse_args()
root = args.root.resolve()
output = args.output.resolve()
if not output.is_relative_to(root / "out"):
    raise SystemExit("The generated project must stay under Sunfleck/out")
output.mkdir(parents=True, exist_ok=True)
source = root / "web/dotnet"
probe = root / "tools/development/skia-spike/web"
tree = ET.parse(source / "Sunfleck.Web.csproj")
project = tree.getroot()
for item in project.iter():
    include = item.get("Include")
    if include and "$" not in include:
        item.set("Include", (source / include).resolve().as_posix())
for item in project.iter("Content"):
    if item.get("Link") == "wwwroot/renderer.js":
        item.set("Include", (probe / "renderer.js").as_posix())
    if item.get("Link") == "wwwroot/main.js":
        item.set("Include", (output / "main.js").as_posix())
flags = project.find(".//EmccExtraLDFlags")
flags.text = flags.text.replace("-sUSE_FREETYPE=1", "") + " -sUSE_WEBGL2=1 -Wl,--threads=1"
project.find(".//SunfleckNativeLibrary").text = args.native.resolve().as_posix()
properties = ET.SubElement(project, "PropertyGroup")
ET.SubElement(properties, "DisableParallelEmccCompile").text = "true"
ET.SubElement(properties, "DisableParallelAot").text = "true"
group = ET.SubElement(project, "ItemGroup")
ET.SubElement(group, "Compile", Include=(source / "Program.cs").as_posix())
ET.SubElement(group, "Compile", Include=(probe / "SkiaBridge.cs").as_posix())
ET.SubElement(group, "NativeFileReference", Include=args.bridge.resolve().as_posix())
ET.SubElement(group, "NativeFileReference", Include=args.skia.resolve().as_posix())
ET.SubElement(group, "NativeFileReference", Include=(args.skia.resolve().parent / "libskcms.a").as_posix())
for library in ("libfreetype2.a", "libpng.a", "libzlib.a"):
    ET.SubElement(group, "NativeFileReference", Include=(args.skia.resolve().parent / library).as_posix())
ET.SubElement(group, "Content", Include=(root / "web/renderer.js").as_posix(), Link="wwwroot/reference-renderer.js", CopyToOutputDirectory="PreserveNewest")
tree.write(output / "SkiaBrowser.csproj", encoding="utf-8", xml_declaration=True)
main = (source / "main.js").read_text(encoding="utf-8")
needle = "  const app = exports.BrowserApp;"
assert needle in main
main = main.replace(needle, needle + """
  const skia = exports.SkiaBridge;
  globalThis.skiaEvaluationBridge = {
    fontVersion: skia.FontVersion,
    features: () => skia.Features(font),
    init: skia.Init, present: skia.Present, lost: skia.Lost,
    restore: skia.Restore, close: skia.Close,
  };
""")
(output / "main.js").write_text(main, encoding="utf-8")
print(output / "SkiaBrowser.csproj")
