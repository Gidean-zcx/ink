"""Collect simulator memory evidence without retaining frame/trace copies."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def run_name(path):
    # Existing runners use mkdtemp; compare their case names, not random suffixes.
    return "/".join(re.sub(r"-[a-z0-9_]{8}$", "-TEMP", part)
                    if i == 0 or part.startswith("clock-alarm-") else part
                    for i, part in enumerate(path.split("/")))


def collect(build):
    rows = {}
    for path in sorted(build.rglob("metrics.json"), key=lambda p: p.stat().st_mtime_ns):
        data = json.loads(path.read_text())
        metrics = data.get("metrics", {})
        if "mem.internal_total.min_free_bytes" not in metrics:
            continue
        name = run_name(path.relative_to(build).as_posix())
        rows[name] = {
            "profile": data.get("profile"),
            "status": data.get("status"),
            "metrics": {k: v for k, v in metrics.items()
                        if k.startswith("mem.") or ".stack_" in k},
            "png": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in sorted(path.parent.glob("*.png"))},
        }
    return dict(sorted(rows.items()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--table", type=Path)
    args = parser.parse_args()
    rows = collect(args.build)
    args.output.write_text(json.dumps(rows, ensure_ascii=False, sort_keys=True, indent=2) + "\n")
    plus = [(r["metrics"]["mem.internal_total.min_free_bytes"], p)
            for p, r in rows.items() if r["profile"] == "metalio-e-ink4-plus"]
    print(f"collected {len(rows)} runs; {len(plus)} pessimistic Plus runs")
    for value, path in sorted(plus)[:5]:
        print(f"{value:8.0f} B  {path}")
    if args.before:
        before = {run_name(k): v for k, v in json.loads(args.before.read_text()).items()}
        if before.keys() != rows.keys():
            raise SystemExit("run sets differ; compare fresh builds with identical tests")
        lines = ["# 全部运行的内存前后对照", "",
                 "【实测，模拟器】单位 B。内部 RAM 含 DMA 预留；PSRAM 峰值为 total − min_free。",
                 "测试替身、故意耗尽资源的自检和重复确定性运行均保留；行名相对于构建目录。", "",
                 "| 运行 | 档案 | 内部最低余量：前 | 后 | 增量 | PSRAM 峰值：前 | 后 | 增量 |",
                 "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
        changed_png = missing_png = png_count = 0
        for name, after in rows.items():
            previous = before[name]
            a, b = previous["metrics"], after["metrics"]
            internal = [m["mem.internal_total.min_free_bytes"] for m in (a, b)]
            psram = [m["mem.external.total_bytes"] - m["mem.external.min_free_bytes"] for m in (a, b)]
            values = [*internal, internal[1] - internal[0], *psram, psram[1] - psram[0]]
            lines.append(f"| {name.removesuffix('/metrics.json')} | {after['profile']} | " +
                         " | ".join(f"{v:.0f}" for v in values) + " |")
            png_count += len(previous["png"])
            missing_png += len(previous["png"].keys() ^ after["png"].keys())
            changed_png += sum(after["png"].get(k) != v for k, v in previous["png"].items())
        print(f"PNG compared={png_count}, changed={changed_png}, added/removed={missing_png}")
        if args.table:
            args.table.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
