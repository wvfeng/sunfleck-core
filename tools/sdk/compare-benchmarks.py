"""Compare repeated SDK CPU samples without treating a single run as a release gate."""
import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidates", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--threshold", type=float, default=15)
    args = parser.parse_args()
    if not 0 <= args.threshold <= 1000:
        parser.error("threshold must be between 0 and 1000 percent")
    baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
    comparisons = []
    for path in args.candidates:
        candidate = json.loads(path.read_text(encoding="utf-8"))
        mismatch = [key for key in ("renderer", "method", "environment") if baseline[key] != candidate[key]]
        metrics = []
        for size, values in baseline["summary"].items():
            for metric, summary in values.items():
                for statistic in ("median", "p95"):
                    old = summary[statistic]
                    new = candidate["summary"][size][metric][statistic]
                    delta = (new / old - 1) * 100 if old else None
                    metrics.append({"nodes": int(size), "metric": metric, "statistic": statistic,
                                    "baseline": old, "candidate": new, "changePercent": delta,
                                    "exceedsThreshold": delta is not None and delta > args.threshold})
        comparisons.append({"file": str(path), "binarySha256": candidate["binarySha256"],
                            "capturedUtc": candidate["capturedUtc"], "configurationMismatch": mismatch,
                            "metrics": metrics})
    report = {"baseline": str(args.baseline), "baselineBinarySha256": baseline["binarySha256"],
              "thresholdPercent": args.threshold, "comparisons": comparisons,
              "note": "Historical same-machine comparison; not an interleaved A/B experiment. "
                      "Exceedances require repeated measurement and analysis, not selective dismissal. "
                      "Configuration mismatches must be resolved before interpreting deltas."}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for comparison in comparisons:
        exceeded = sum(metric["exceedsThreshold"] for metric in comparison["metrics"])
        print(f"{comparison['file']}: {exceeded}/{len(comparison['metrics'])} statistics above {args.threshold:g}%")
        if comparison["configurationMismatch"]:
            print("  Configuration mismatch:", ", ".join(comparison["configurationMismatch"]))


if __name__ == "__main__":
    main()
