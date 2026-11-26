"""Fetch only the evaluation dependencies, at the revisions in Skia's DEPS."""
import argparse
import json
import re
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("skia", type=Path)
parser.add_argument("--names", nargs="+", choices=["harfbuzz", "icu", "egl-registry", "opengl-registry", "freetype", "libpng", "zlib", "d3d12allocator", "spirv-cross"],
                    default=["harfbuzz", "icu", "egl-registry", "opengl-registry"])
args = parser.parse_args()
root = args.skia.resolve(strict=True)
deps = (root / "DEPS").read_text(encoding="utf-8")
manifest = root.parent / "dependencies.json"
records = json.loads(manifest.read_text(encoding="utf-8")) if manifest.exists() else []
for name in args.names:
    relative = "third_party/externals/" + name
    match = re.search(r'["\']' + re.escape(relative) + r'["\']\s*:\s*["\']([^"\']+)@([0-9a-f]{40})["\']', deps)
    if not match:
        raise RuntimeError("Pinned dependency missing: " + name)
    url, revision = match.groups()
    destination = (root / relative).resolve()
    destination.relative_to(root)
    destination.mkdir(parents=True, exist_ok=True)
    def git(*arguments):
        return subprocess.check_output(["git", "-c", "safe.directory=" + destination.as_posix(),
                                        "-C", str(destination), *arguments], text=True).strip()
    if not (destination / ".git").exists():
        git("init")
        git("remote", "add", "origin", url)
    if git("remote", "get-url", "origin") != url:
        raise RuntimeError("Unexpected remote: " + str(destination))
    print("Fetching", name, revision, flush=True)
    git("fetch", "--depth", "1", "origin", revision)
    git("checkout", "--detach", revision)
    if git("rev-parse", "HEAD") != revision:
        raise RuntimeError("Wrong dependency revision")
    records = [record for record in records if record["name"] != name]
    records.append(dict(name=name, url=url, revision=revision))
    manifest.write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
