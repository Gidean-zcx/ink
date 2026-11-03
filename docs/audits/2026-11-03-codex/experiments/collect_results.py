#!/usr/bin/env python3
"""Keep compact, portable evidence from bulk outputs left in build-audit/."""
import json
from pathlib import Path
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
selected = ["audio.mic.captured_ms", "audio.speaker.played_ms", "power.sleep_ms",
            "power.locked_sleep_share", "power.avg_ma.locked", "hub.binary_bytes",
            "display.power_failures", "display.unpowered_waveforms",
            "mem.internal_total.min_free_bytes", "mem.internal_total.largest_free_min_bytes",
            "reader.page_turns", "reader.saves", "ui.action_to_visible_ms.TEXT",
            "input.key_delivery_ms"]
cases = {}
for parent in ("audit-scenarios", "audit-probes", "test-out"):
    for path in sorted((ROOT / "build-audit" / parent).glob("*/metrics.json")):
        data = json.loads(path.read_text())
        cases[parent + "/" + path.parent.name] = dict(
            status=data.get("status"), virtual_time_s=data.get("virtual_time_s"),
            budget_violations=data.get("budget_violations"), srs_gaps=data.get("srs_gaps"),
            warnings=data.get("warnings"),
            metrics={k: data["metrics"][k] for k in selected if k in data["metrics"]})
(HERE / "evidence-summary.json").write_text(json.dumps(cases, indent=2, ensure_ascii=False) + "\n")
for name in ("ai_held_dismiss", "ai_held_control"):
    print(name, cases["audit-scenarios/" + name])
