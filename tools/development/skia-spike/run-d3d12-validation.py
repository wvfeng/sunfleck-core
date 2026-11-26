"""Run the native D3D12 correctness gate and retain reproducible machine evidence."""
import argparse
import hashlib
import json
import platform
import re
import shutil
import subprocess
import sys
import winreg
from datetime import datetime, timezone
from pathlib import Path


def identity(path):
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return dict(path=str(path), bytes=path.stat().st_size, sha256=digest)


def display_drivers():
    drivers = []
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                        r"SYSTEM\CurrentControlSet\Control\Video") as video:
        for index in range(winreg.QueryInfoKey(video)[0]):
            name = winreg.EnumKey(video, index)
            try:
                with winreg.OpenKey(video, name + r"\0000") as adapter:
                    values = {}
                    for field in ("DriverDesc", "DriverVersion", "ProviderName"):
                        try:
                            values[field] = winreg.QueryValueEx(adapter, field)[0]
                        except FileNotFoundError:
                            pass
                    if values:
                        drivers.append(values)
            except FileNotFoundError:
                pass
    return drivers


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe-directory", required=True, type=Path)
    parser.add_argument("--skia", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--mode", choices=("hardware", "warp"), default="hardware")
    args = parser.parse_args()
    skia, probes, output = args.skia.resolve(), args.probe_directory.resolve(), args.output.resolve()
    # Refuse stale evidence instead of combining files from different executions.
    output.mkdir(parents=True, exist_ok=False)
    git = ["git", "-C", str(skia)]
    report = dict(started_at=datetime.now(timezone.utc).isoformat(),
                  os=platform.platform(), windows_version=platform.version(),
                  machine=platform.node(), mode=args.mode, debug_layer_required=True,
                  driver_inventory=display_drivers(),
                  skia_revision=subprocess.check_output(git + ["rev-parse", "HEAD"], text=True).strip(),
                  executables=[identity(probes / name) for name in
                               ("d3d12_probe.exe", "d3d12_capabilities.exe")])
    patch = subprocess.check_output(git + ["diff", "--ignore-space-at-eol", "--",
                                          "src/gpu/ganesh/d3d"], text=True, encoding="utf-8")
    (output / "skia-d3d12.patch").write_text(patch, encoding="utf-8")
    report["skia_d3d12_patch"] = identity(output / "skia-d3d12.patch")
    config = skia / "out/sunfleck-d3d12/args.gn"
    report["gn_args"] = config.read_text(encoding="utf-8")
    report["archives"] = [identity(path) for path in sorted(config.parent.glob("*.lib"))]
    report["probe_sources"] = [identity(Path(__file__).parent / name) for name in
                               ("d3d12.cpp", "d3d12-adapter.h", "d3d12-capabilities.cpp",
                                "scene.h", "text.cpp", "text.h", "d3d12/CMakeLists.txt",
                                "run-d3d12-validation.py")]
    dependencies = skia.parent / "dependencies.json"
    report["dependencies"] = json.loads(dependencies.read_text(encoding="utf-8"))
    report["d3d12_dependency_revisions"] = {
        name: subprocess.check_output(["git", "-C", str(skia / "third_party/externals" / name),
                                       "rev-parse", "HEAD"], text=True).strip()
        for name in ("d3d12allocator", "spirv-cross")}
    archived = output / "bin"
    archived.mkdir()
    for name in ("d3d12_probe.exe", "d3d12_capabilities.exe"):
        shutil.copy2(probes / name, archived / name)
    report["archived_executables"] = [identity(path) for path in sorted(archived.glob("*.exe"))]
    (output / "identity.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                                          encoding="utf-8")
    commands = [("capabilities", [str(probes / "d3d12_capabilities.exe")]),
                (args.mode, [str(probes / "d3d12_probe.exe"), "--" + args.mode,
                             str(output), "--require-debug-layer"])]
    report["runs"] = []
    for name, command in commands:
        log = output / (name + ".log")
        timed_out = False
        with log.open("w", encoding="utf-8") as stream:
            try:
                result = subprocess.run(command, cwd=probes, stdout=stream,
                                        stderr=subprocess.STDOUT, timeout=180, check=False)
                exit_code = result.returncode
            except subprocess.TimeoutExpired:
                timed_out, exit_code = True, 124
        report["runs"].append(dict(command=command, exit_code=exit_code,
                                   timed_out=timed_out, log=identity(log)))
    log_text = (output / (args.mode + ".log")).read_text(encoding="utf-8", errors="replace")
    report["debug_errors"] = len(re.findall(r"D3D12 validation: id=\d+ severity=[01] ", log_text))
    report["debug_warnings"] = len(re.findall(r"D3D12 validation: id=\d+ severity=2 ", log_text))
    stages = ("render-target discard/load and MSAA", "two windows, 96/144/192 DPI, resize, minimize/restore",
              "original contexts teardown", "explicit device recreation")
    report["stages"] = {stage: "PASS stage: " + stage in log_text for stage in stages}
    report["passed"] = (all(run["exit_code"] == 0 for run in report["runs"])
                        and all(report["stages"].values()) and report["debug_errors"] == 0
                        and "D3D12 debug layer enabled" in log_text
                        and "PASS: D3D12 --" + args.mode in log_text)
    report["finished_at"] = datetime.now(timezone.utc).isoformat()
    report["images"] = [identity(path) for path in sorted(output.glob("*.ppm"))]
    report["performance_gate"] = "Deferred; this is a serialized correctness probe."
    (output / "result.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                                        encoding="utf-8")
    print(json.dumps(dict(passed=report["passed"], debug_errors=report["debug_errors"],
                          debug_warnings=report["debug_warnings"], output=str(output))))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
