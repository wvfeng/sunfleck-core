"""Record source identity and optional build artifacts without changing the checkout.

Usage: python tools/development/capture-baseline.py out/evidence/source.json
       --artifact out/devtools/sunfleck_native.dll
This records evidence, not a successful build/test assertion.
"""
import argparse
import datetime
import hashlib
import json
import pathlib
import subprocess


def capture(root, artifacts):
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=root).decode("utf-8")

    names = git("ls-files", "-z", "--cached", "--others", "--exclude-standard").split("\0")
    files = []
    for name in sorted(set(filter(None, names))):
        path = root / name
        files.append({"path": name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None})
    outputs = []
    for path in artifacts:
        resolved = path.resolve(strict=True)
        outputs.append({"path": str(resolved), "sha256": hashlib.sha256(resolved.read_bytes()).hexdigest()})
    return {"capturedUtc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "head": git("rev-parse", "HEAD").strip(), "status": git("status", "--porcelain=v1"),
            "files": files, "artifacts": outputs,
            "note": "Source/artifact identity only. Build and test results require separate evidence."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--artifact", type=pathlib.Path, action="append", default=[])
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[2]
    result = capture(root, args.artifact)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Recorded {len(result['files'])} source paths and {len(result['artifacts'])} artifacts: {args.output}")


if __name__ == "__main__":
    main()
