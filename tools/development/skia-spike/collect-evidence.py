"""Collect candidate identity and artifact costs; this is not a performance test."""
import argparse
import hashlib
import json
import platform
from datetime import datetime, timezone
import subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--root", type=Path, required=True)
p.add_argument("--skia", type=Path, required=True)
p.add_argument("--angle", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
args = p.parse_args()
root, skia = args.root.resolve(), args.skia.resolve()

def identity(path):
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return dict(path=str(path), bytes=path.stat().st_size, sha256=digest)

def git(*arguments):
    return subprocess.check_output(["git", "-c", "safe.directory="+skia.as_posix(),
                                    "-C", str(skia), *arguments], text=True).strip()

def build_report(directory):
    result = {"configuration": (directory/"args.gn").read_text(encoding="utf-8")}
    result["archives"] = [identity(path) for path in sorted(directory.glob("*.lib"))]
    result["archives"] += [identity(path) for path in sorted(directory.glob("*.a"))]
    log = directory/".ninja_log"
    if log.exists():
        actions = {}
        for line in log.read_text(encoding="utf-8").splitlines():
            parts = line.split("\t")
            if len(parts) == 5:
                actions[parts[3]] = int(parts[1])-int(parts[0])
        result["completed_outputs"] = len(actions)
        result["sum_latest_action_seconds"] = sum(actions.values())/1000
        result["timing_note"] = "Sum of latest recorded build actions; excludes failed actions, downloads, configuration and idle time. Not runtime performance."
    return result

report = dict(collected_at=datetime.now(timezone.utc).isoformat(), platform=platform.platform(), candidate=dict(remote=git("remote","get-url","origin"),
              revision=git("rev-parse","HEAD"), trackedChanges=git("status","--porcelain","--untracked-files=no")), dependencies=json.loads((skia.parent/"dependencies.json").read_text()),
              windows=build_report(skia/"out/sunfleck-gpu-text"),
              web=build_report(skia/"out/sunfleck-web"),
              angle=[identity(args.angle/name) for name in ("libEGL.dll","libGLESv2.dll")],
              angle_note="Local VS Code runtime, feasibility only; not an approved reproducible redistributable package.",
              sdk=[identity(path) for path in sorted((root/"out/skia-evaluation/sdk").rglob("*")) if path.is_file()],
              probe_sources=[identity(path) for path in sorted((root/"tools/development/skia-spike").rglob("*")) if path.is_file() and "__pycache__" not in path.parts],
              performance_gate="Deferred by user; no runtime throughput, startup, or memory acceptance claimed.")
report["native_probes"] = [identity(path) for path in sorted((root/"out/skia-evaluation/probe").glob("*.exe"))]
if (skia/"out/sunfleck-d3d12/args.gn").exists():
    report["d3d12"] = build_report(skia/"out/sunfleck-d3d12")
    report["d3d12"]["probes"] = [identity(path) for path in sorted((root/"out/skia-evaluation/d3d12").glob("*.exe"))]
    report["d3d12"]["package"] = [identity(path) for path in sorted((root/"out/skia-evaluation/d3d12-sdk").rglob("*")) if path.is_file()]
    report["d3d12"]["scope"] = "Ganesh native Direct3D 12, not Graphite/Dawn. Explicit hardware and WARP tests; consult test logs for actual acceptance."
report["web"]["scope"] = "Skia drawing and bundled FreeType/png/zlib only. The unused Web SkParagraph build was stopped after drawing archives completed; no Web paragraph compatibility claim."
report["test_logs"] = [identity(path) for path in sorted((root/"out/recovery").glob("skia-*.log"))]
report["browser_results"] = [dict(file=identity(path), result=json.loads(path.read_text(encoding="utf-8")))
                             for path in sorted((root/"out/skia-evaluation").glob("browser-*/result.json"))]
report["web_payloads"] = [identity(path) for site in (root/"out/skia-evaluation/web-cpp/web", root/"out/skia-evaluation/web-dotnet/bin/Release/net10.0/wwwroot")
                          for path in sorted(site.rglob("*")) if path.is_file() and path.suffix in (".wasm", ".data", ".dat", ".ttf", ".js", ".json")]
report["web_payload_note"] = "Uncompressed isolated application artifacts, including locally licensed font bytes. Not production incremental size or redistribution authorization."
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
print(args.output)
