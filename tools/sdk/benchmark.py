"""Repeat the installed-SDK CPU benchmark and retain raw samples, never a GPU claim."""
import argparse
import datetime
import hashlib
import json
import math
import os
import platform
import statistics
import subprocess
import time
from pathlib import Path

def stats(values):
    ordered = sorted(values)
    return {'count': len(values), 'median': statistics.median(values),
            'p95': ordered[math.ceil(len(values) * .95) - 1], 'min': ordered[0], 'max': ordered[-1]}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--runs', type=int, default=5)
    args = parser.parse_args()
    if args.runs < 3: parser.error('Use at least three independent processes per scenario')
    runs = []
    for nodes in (100, 1000):
        for index in range(args.runs):
            start = time.perf_counter()
            completed = subprocess.run([str(args.executable.resolve()), str(nodes)], capture_output=True, text=True, check=True, timeout=120)
            record = json.loads(completed.stdout)
            record.update(run=index, processWallMs=(time.perf_counter() - start) * 1000)
            runs.append(record)
    summary = {}
    for nodes in (100, 1000):
        selected = [run for run in runs if run['nodes'] == nodes]
        summary[str(nodes)] = {key: stats([x for run in selected for x in run[key]])
                              for key in ('commandNotifyFrameMs', 'refreshFrameMs', 'idlePollMs')}
        summary[str(nodes)].update({key: stats([run[key] for run in selected])
                                   for key in ('startupMs', 'processWallMs', 'privateLoadedBytes', 'privateAfterBytes')})
    result = {'capturedUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'renderer': 'headless-software',
              'environment': {'platform': platform.platform(), 'processor': os.environ.get('PROCESSOR_IDENTIFIER', platform.processor()),
                              'python': platform.python_version(), 'logicalCpus': os.cpu_count()},
              'binarySha256': hashlib.sha256(args.executable.read_bytes()).hexdigest(),
              'method': 'Release, 800x600, 5 warmup + 50 samples per process; no GPU/presentation/physical-input timing; OS cache not cleared',
              'summary': summary, 'runs': runs}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))

if __name__ == '__main__': main()
