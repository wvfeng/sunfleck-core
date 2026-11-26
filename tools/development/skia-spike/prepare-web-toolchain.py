"""Use response files for Windows-hosted Wasm compilation, after GN generation.

Skia's generated command otherwise exposes FreeType's <header> defines to cmd.exe
when launching emcc.bat, where they become shell redirections. No source patch or
global toolchain modification is needed.
"""
import argparse
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("build", type=Path)
args = p.parse_args()
toolchain = args.build.resolve(strict=True) / "toolchain.ninja"
lines = toolchain.read_text(encoding="utf-8").splitlines()
result = []
rule = None
changed = 0
for line in lines:
    if line.startswith("rule "):
        rule = line[5:]
    if rule in ("cc", "cxx") and line.startswith("  command = ") and " -MD -MF " in line:
        command, flags = line.split(" -MD -MF ", 1)
        result += [command + " @${out}.rsp", "  rspfile = ${out}.rsp", "  rspfile_content = -MD -MF " + flags]
        changed += 1
    else:
        result.append(line)
if changed not in (0, 2):
    raise SystemExit("Unexpected generated toolchain; refusing a partial modification")
if not changed and "  rspfile_content = -MD -MF " not in "\n".join(lines):
    raise SystemExit("Expected Wasm compiler rules were not found")
toolchain.write_text("\n".join(result)+"\n",encoding="utf-8")
print(f"Prepared {changed} response-file compiler rules: {toolchain}")
