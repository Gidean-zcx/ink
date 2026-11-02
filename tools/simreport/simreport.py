#!/usr/bin/env python3
"""
simreport — turn one ink-sim output directory into a self-contained HTML report:
budgets, the screens the simulated panel actually showed (one card per refresh,
with waveform and timing), action latencies, CPU per task and memory by owner.

  python3 tools/simreport/simreport.py build/test-out/reader_basic-metalio-plus -o report.html
  (run ink-sim with --trace --frames for the refresh timeline and thumbnails)

--fragment writes the page body only (for publishing as a claude.ai artifact).
"""
import argparse
import base64
import glob
import html
import io
import json
import os
import re

try:
    from PIL import Image
except ImportError:  # thumbnails are optional
    Image = None

CSS = """
:root{--paper:#f4f3ef;--sheet:#fbfaf7;--ink:#1c1d1f;--muted:#6a6c70;--rule:#d9d7d0;--accent:#2e6a86;--accent-soft:#d6e4ea;
--ok:#2f7a4f;--ok-soft:#dcefe3;--bad:#b3402a;--bad-soft:#f6ddd6;--warn:#9a6a12;--warn-soft:#f3e8cf;
--f-display:"Newsreader",Georgia,"Songti SC","Noto Serif CJK SC",serif;--f-body:"IBM Plex Sans","PingFang SC","Noto Sans CJK SC","Microsoft YaHei",system-ui,sans-serif;
--f-mono:"IBM Plex Mono",ui-monospace,"SFMono-Regular",Menlo,monospace}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--paper:#161718;--sheet:#1e1f21;--ink:#e8e6e1;--muted:#9b9da1;--rule:#34363a;--accent:#7fb4cc;--accent-soft:#233743;
--ok:#7cc79a;--ok-soft:#1f3328;--bad:#ef8f7a;--bad-soft:#3d2520;--warn:#e0b65c;--warn-soft:#3a3020;color-scheme:dark}}
:root[data-theme="dark"]{--paper:#161718;--sheet:#1e1f21;--ink:#e8e6e1;--muted:#9b9da1;--rule:#34363a;--accent:#7fb4cc;--accent-soft:#233743;
--ok:#7cc79a;--ok-soft:#1f3328;--bad:#ef8f7a;--bad-soft:#3d2520;--warn:#e0b65c;--warn-soft:#3a3020;color-scheme:dark}
body{background:var(--paper);color:var(--ink);font-family:var(--f-body);font-size:15px;line-height:1.55}
.wrap{max-width:1180px;margin:0 auto;padding-block:28px 64px;padding-left:max(16px,env(safe-area-inset-left,0px));padding-right:max(16px,env(safe-area-inset-right,0px));display:grid;gap:36px}
.wrap>*,.grid2>*{min-width:0}
h1{font-family:var(--f-display);font-weight:600;font-size:clamp(28px,4vw,40px);line-height:1.1;margin:0;text-wrap:balance}
h2{font-family:var(--f-display);font-weight:600;font-size:22px;margin:0 0 4px;text-wrap:balance}
.lede{color:var(--muted);max-width:68ch;margin:6px 0 0}
.kv{display:flex;flex-wrap:wrap;gap:8px 22px;margin-top:14px;font-size:13px;color:var(--muted)}
.kv b{color:var(--ink);font-family:var(--f-mono);font-weight:500}
.sec-note{color:var(--muted);font-size:13px;margin:0 0 14px;max-width:80ch}
.chip{display:inline-block;font-family:var(--f-mono);font-size:12px;line-height:1;padding:5px 8px;border-radius:4px;border:1px solid var(--rule);background:var(--sheet);white-space:nowrap}
.chip.ok{background:var(--ok-soft);color:var(--ok);border-color:transparent}.chip.bad{background:var(--bad-soft);color:var(--bad);border-color:transparent}
.chip.wf{background:var(--accent-soft);color:var(--accent);border-color:transparent}
.tablebox{overflow-x:auto;border:1px solid var(--rule);border-radius:6px;background:var(--sheet)}
table{border-collapse:collapse;width:100%;font-size:13px}
th,td{text-align:left;padding:8px 12px;border-bottom:1px solid var(--rule);vertical-align:top}
th{font-weight:600;color:var(--muted);font-size:12px;letter-spacing:.04em;text-transform:uppercase}
td.num{font-family:var(--f-mono);font-variant-numeric:tabular-nums;text-align:right;white-space:nowrap}
tr:last-child td{border-bottom:none}
.why{color:var(--muted)}
.strip{display:flex;gap:14px;overflow-x:auto;padding:4px 2px 14px;scroll-snap-type:x proximity}
.card{flex:0 0 auto;width:208px;scroll-snap-align:start;display:grid;gap:8px;font-size:12px}
.bezel{background:#2b2c2e;border-radius:10px;padding:9px 9px 14px}
.bezel img{display:block;width:100%;border-radius:2px;background:#fff;image-rendering:auto}
.card .t{display:flex;justify-content:space-between;align-items:center;gap:6px}
.card .meta{font-family:var(--f-mono);color:var(--muted);font-variant-numeric:tabular-nums}
.bar{position:relative;height:10px;border-radius:3px;background:var(--rule);overflow:hidden}
.bar i{position:absolute;top:0;bottom:0}
.bar .pu{background:var(--warn)}.bar .wv{background:var(--accent)}.bar .q{background:var(--muted);opacity:.5}
.legend{display:flex;flex-wrap:wrap;gap:14px;font-size:12px;color:var(--muted);margin-bottom:10px}
.legend span{display:inline-flex;align-items:center;gap:6px}.legend i{width:12px;height:8px;border-radius:2px;display:inline-block}
.grid2{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,460px),1fr));gap:28px}
.hbars{display:grid;gap:7px;font-size:13px}
.hrow{display:grid;grid-template-columns:minmax(90px,170px) 1fr 86px;gap:10px;align-items:center}
.hrow .name{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-family:var(--f-mono);font-size:12px}
.hrow .track{height:12px;background:var(--rule);border-radius:3px;overflow:hidden}
.hrow .fill{height:100%;background:var(--accent)}
.hrow .fill.int{background:var(--warn)}
.hrow .v{font-family:var(--f-mono);font-variant-numeric:tabular-nums;text-align:right;font-size:12px}
.snaps{display:flex;gap:16px;overflow-x:auto;padding-bottom:10px}
.snap{flex:0 0 auto;width:250px;display:grid;gap:6px;font-size:12px;color:var(--muted)}
.warnlist{margin:0;padding-left:18px;font-family:var(--f-mono);font-size:12px;color:var(--muted)}
.empty{color:var(--muted);font-size:13px}
:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
"""

FONTS = '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans:wght@400;600&family=Newsreader:opsz,wght@6..72,600&display=swap">'


ZH = {
    "lede": "这次场景里模拟设备做了什么：屏幕每一次物理刷新、每个操作多久变得可见、CPU 时间和内存花在哪里。时间是悲观设备档案下的虚拟设备时间，不是电脑运行时间。",
    "profile": "设备档案", "CPU model": "CPU 模型", "virtual time": "虚拟时间", "status": "状态", "budgets": "预算",
    "met": "达标", "missed": "未达标", "warnings": "警告",
    "Budgets": "预算", "The scenario passes CI only if every budget is met.": "所有预算都达标，这个场景才能通过 CI。",
    "Result": "结果", "Metric": "指标", "Value": "数值", "Limit": "限值", "Why it matters": "为什么重要",
    "What the panel showed": "屏幕实际显示了什么",
    "strip-note": "每张卡片是一次物理刷新，按时间顺序。墨水屏无论更新多大区域都要扫全屏；条形表示屏幕电源上电与波形时间。",
    "power-up": "电源上电", "waveform": "波形",
    "no user action": "无用户操作", "action→visible": "操作→可见",
    "Snapshots": "定时截图", "snap-note": "由场景脚本在固定时刻截取；淡灰色是模拟的残影。",
    "Latency": "时延", "Milliseconds of device time.": "单位：毫秒（设备时间）。",
    "CPU per task": "各任务 CPU 时间", "cpu-note": "整个场景的设备 CPU 时间；", "core": "核", "busy": "忙",
    "Internal RAM": "内部 RAM", "PSRAM": "PSRAM",
    "mem-note-int": "系统可用 {total} KiB；最低剩余 {low} KiB。各使用者峰值：",
    "mem-note-ext": "可用 {total} KiB；最低剩余 {low} KiB。各使用者峰值：",
    "Warnings": "警告", "None.": "无。", "No data.": "无数据。", "No refreshes.": "没有刷新。",
    "not recorded": "未记录（按 0 计）",
    "need-trace": "运行 ink-sim 时加 --trace --frames 才能看到每次刷新。",
}
LANG = "en"


def T(key, default=None):
    if LANG == "zh" and key in ZH:
        return ZH[key]
    return default if default is not None else key


def esc(s):
    return html.escape(str(s), quote=True)


def thumb(path, width):
    if not os.path.exists(path):
        return None
    if Image is None:
        data = open(path, "rb").read()
    else:
        im = Image.open(path).convert("L")
        h = int(im.height * width / im.width)
        im = im.resize((width, h), Image.LANCZOS)
        buf = io.BytesIO()
        im.save(buf, "PNG", optimize=True)
        data = buf.getvalue()
    return "data:image/png;base64," + base64.b64encode(data).decode()


def load_trace(out):
    p = os.path.join(out, "trace.json")
    if not os.path.exists(p):
        return None
    t = json.load(open(p))
    tracks = {e["tid"]: e["args"]["name"] for e in t if e.get("ph") == "M" and e.get("name") == "thread_name" and e.get("pid") == 2}
    ev = []
    for e in t:
        if e.get("pid") != 2 or e.get("ph") not in ("X", "i"):
            continue
        ev.append({"track": tracks.get(e.get("tid"), "?"), "name": e["name"], "t": e["ts"] / 1000.0,
                   "dur": e.get("dur", 0) / 1000.0, "args": e.get("args", {})})
    ev.sort(key=lambda x: x["t"])
    return ev


def updates_from_trace(ev):
    ups = []
    last_pu = None
    probes = [e for e in ev if e["track"] == "probes" and e["name"] == "ui.action_to_visible_ms"]
    for e in ev:
        if e["track"] != "display":
            continue
        if e["name"] == "power-up":
            last_pu = e
        elif e["name"] in ("GC16", "GL16", "DU", "A2"):
            start, done = e["t"], e["t"] + e["dur"]
            pu = last_pu if last_pu and abs(last_pu["t"] + last_pu["dur"] - start) < 0.5 else None
            queued = pu["t"] if pu else start
            cause = None
            for p in probes:
                if abs(p["t"] - done) < 0.5:
                    cause = p["args"].get("value")
            ups.append({"id": e["args"].get("id"), "wf": e["name"], "start": start, "done": done, "queued": queued,
                        "pu": pu["dur"] if pu else 0, "area": e["args"], "visible_ms": cause})
            last_pu = None
    return ups


def frame_files(out):
    m = {}
    for f in glob.glob(os.path.join(out, "frames", "*.png")):
        mm = re.match(r"(\d+)_", os.path.basename(f))
        if mm:
            m[int(mm.group(1))] = f
    return m


def fmt(v, d=1):
    return f"{v:,.{d}f}" if isinstance(v, (int, float)) else esc(v)


def hbars(rows, unit, cls=""):
    if not rows:
        return f'<p class="empty">{T("No data.")}</p>'
    mx = max(v for _, v in rows) or 1
    out = ['<div class="hbars">']
    for name, v in rows:
        out.append(f'<div class="hrow"><span class="name" title="{esc(name)}">{esc(name)}</span>'
                   f'<span class="track"><span class="fill {cls}" style="width:{100 * v / mx:.1f}%"></span></span>'
                   f'<span class="v">{fmt(v)} {unit}</span></div>')
    out.append("</div>")
    return "".join(out)


def build(out, title):
    met = json.load(open(os.path.join(out, "metrics.json")))
    m = met["metrics"]
    ev = load_trace(out)
    budgets = met.get("budgets", [])
    nfail = sum(1 for b in budgets if not b.get("ok"))
    parts = []
    name = title or os.path.basename(os.path.normpath(out))
    lede = T("lede", "What the simulated device did in this scenario: every refresh the panel performed, how long each user "
             "action took to become visible, and where CPU time and memory went. Times are virtual device time from the "
             "pessimistic profile, not host time.")
    parts.append(f'<header><h1>{esc(name)}</h1><p class="lede">{esc(lede)}</p>'
                 f'<div class="kv"><span>{T("profile")} <b>{esc(met.get("profile"))}</b></span>'
                 f'<span>{T("CPU model")} <b>{esc(met.get("cpu_mode"))}</b></span>'
                 f'<span>{T("virtual time")} <b>{fmt(met.get("virtual_time_s", 0), 2)} s</b></span>'
                 f'<span>{T("status")} <b>{esc(met.get("status"))}</b></span>'
                 f'<span>{T("budgets")} <b>{len(budgets) - nfail}/{len(budgets)} {T("met")}</b></span>'
                 f'<span>{T("warnings")} <b>{len(met.get("warnings", []))}</b></span></div></header>')

    # budgets
    if budgets:
        rows = []
        for b in budgets:
            lim = (f'≤ {fmt(b["max"])}' if "max" in b else "") + (f' ≥ {fmt(b["min"])}' if "min" in b else "")
            val = fmt(b["value"]) if "value" in b else "—"
            chip = f'<span class="chip ok">{T("met")}</span>' if b.get("ok") else f'<span class="chip bad">{T("missed")}</span>'
            why = b.get("why") or (T("not recorded") if "value" not in b else b.get("detail", ""))
            rows.append(f'<tr><td>{chip}</td><td><code>{esc(b["metric"])}</code></td><td class="num">{val}</td>'
                        f'<td class="num">{lim.strip()}</td><td class="why">{esc(why)}</td></tr>')
        parts.append(f'<section><h2>{T("Budgets")}</h2><p class="sec-note">{T("The scenario passes CI only if every budget is met.")}</p>'
                     f'<div class="tablebox"><table><thead><tr><th>{T("Result")}</th><th>{T("Metric")}</th><th>{T("Value")}</th><th>{T("Limit")}</th><th>{T("Why it matters")}</th></tr></thead><tbody>'
                     + "".join(rows) + "</tbody></table></div></section>")

    # refresh timeline
    if ev:
        ups = updates_from_trace(ev)
        frames = frame_files(out)
        maxlen = max([u["done"] - u["queued"] for u in ups] + [1])
        cards = []
        for u in ups:
            img = thumb(frames[u["id"]], 190) if u["id"] in frames else None
            a = u["area"]
            span = u["done"] - u["queued"]
            pu_w = 100 * u["pu"] / maxlen
            wv_w = 100 * (u["done"] - u["start"]) / maxlen
            vis = f'{T("action→visible")} {fmt(u["visible_ms"], 0)} ms' if u["visible_ms"] is not None else T("no user action")
            cards.append(
                '<div class="card">'
                + (f'<div class="bezel"><img alt="panel after refresh {esc(u["id"])}" src="{img}"></div>' if img else "")
                + f'<div class="t"><span class="chip wf">{esc(u["wf"])}</span><span class="meta">#{esc(u["id"])} · {fmt(u["start"] / 1000, 2)} s</span></div>'
                + f'<div class="bar" title="{T("power-up")} {fmt(u["pu"], 0)} ms, {T("waveform")} {fmt(u["done"] - u["start"], 0)} ms">'
                + f'<i class="pu" style="left:0;width:{pu_w:.1f}%"></i><i class="wv" style="left:{pu_w:.1f}%;width:{wv_w:.1f}%"></i></div>'
                + f'<div class="meta">{fmt(span, 0)} ms · {esc(a.get("w"))}×{esc(a.get("h"))} px</div><div class="meta">{esc(vis)}</div></div>')
        note = T("strip-note", "One card per physical refresh, in order. An e-ink update scans the whole panel whatever its size; "
                 "the bar shows PMIC power-up and waveform time.")
        parts.append(f'<section><h2>{T("What the panel showed")}</h2><p class="sec-note">{esc(note)}</p>'
                     f'<div class="legend"><span><i style="background:var(--warn)"></i>{T("power-up")}</span><span><i style="background:var(--accent)"></i>{T("waveform")}</span></div>'
                     f'<div class="strip">{"".join(cards) if cards else "<p class=empty>" + T("No refreshes.") + "</p>"}</div></section>')
    else:
        parts.append(f'<section><h2>{T("What the panel showed")}</h2><p class="empty">{T("need-trace", "Run ink-sim with --trace --frames to see every refresh.")}</p></section>')

    snaps = sorted(glob.glob(os.path.join(out, "snap-*.png")))
    if snaps:
        cells = []
        for s in snaps:
            img = thumb(s, 250)
            cells.append(f'<figure class="snap" style="margin:0"><div class="bezel"><img alt="{esc(os.path.basename(s))}" src="{img}"></div>'
                         f'<figcaption>{esc(os.path.basename(s)[5:-4])}</figcaption></figure>')
        parts.append(f'<section><h2>{T("Snapshots")}</h2><p class="sec-note">{T("snap-note", "Taken by the scenario script at fixed times; faint grey is modelled ghosting.")}</p>'
                     f'<div class="snaps">{"".join(cells)}</div></section>')

    # latency + cpu
    lat = []
    for k, v in sorted(m.items()):
        if isinstance(v, dict) and v.get("count") and (k.startswith("ui.") or k.startswith("reader.") or k.startswith("display.update_ms")):
            lat.append(f'<tr><td><code>{esc(k)}</code></td><td class="num">{int(v["count"])}</td><td class="num">{fmt(v["p50"])}</td>'
                       f'<td class="num">{fmt(v["p95"])}</td><td class="num">{fmt(v["max"])}</td></tr>')
    cpu = sorted(((k[5:-7], v) for k, v in m.items() if k.startswith("task.") and k.endswith(".cpu_ms") and v > 0.05), key=lambda x: -x[1])
    parts.append(f'<section class="grid2"><div><h2>{T("Latency")}</h2><p class="sec-note">{T("Milliseconds of device time.")}</p><div class="tablebox"><table><thead><tr>'
                 f'<th>{T("Metric")}</th><th>n</th><th>p50</th><th>p95</th><th>max</th></tr></thead><tbody>' + "".join(lat)
                 + f'</tbody></table></div></div><div><h2>{T("CPU per task")}</h2><p class="sec-note">{T("cpu-note", "Device CPU time over the whole run; ")}'
                 + " · ".join(f'{T("core")} {c} {T("busy")} {fmt(m.get(f"cpu.core{c}.busy_pct", 0))}%' for c in range(4) if f"cpu.core{c}.busy_pct" in m)
                 + f'</p>{hbars(cpu, "ms")}</div></section>')

    # memory
    def tags(pool):
        rows = [(k[8:-len(f".{pool}.peak_bytes")], v / 1024) for k, v in m.items()
                if k.startswith("mem.tag.") and k.endswith(f".{pool}.peak_bytes")]
        return sorted(rows, key=lambda x: -x[1])[:14]
    mi = (m.get("mem.internal.total_bytes", 0), m.get("mem.internal.min_free_bytes", 0))
    me = (m.get("mem.external.total_bytes", 0), m.get("mem.external.min_free_bytes", 0))
    ni = T("mem-note-int", "{total} KiB available to the system; lowest free {low} KiB. Peak per owner:").format(total=fmt(mi[0] / 1024), low=fmt(mi[1] / 1024))
    ne = T("mem-note-ext", "{total} KiB available; lowest free {low} KiB. Peak per owner:").format(total=fmt(me[0] / 1024), low=fmt(me[1] / 1024))
    parts.append(f'<section class="grid2"><div><h2>{T("Internal RAM")}</h2><p class="sec-note">{ni}</p>'
                 f'{hbars(tags("internal"), "KiB", "int")}</div><div><h2>{T("PSRAM")}</h2><p class="sec-note">{ne}</p>'
                 f'{hbars(tags("external"), "KiB")}</div></section>')

    warns = met.get("warnings", [])
    parts.append(f'<section><h2>{T("Warnings")}</h2>' + (
        '<ul class="warnlist">' + "".join(f"<li>{esc(w)}</li>" for w in warns) + "</ul>" if warns else f'<p class="empty">{T("None.")}</p>') + "</section>")
    return name, '<div class="wrap">' + "".join(parts) + "</div>"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    ap.add_argument("-o", "--output", default=None)
    ap.add_argument("--title", default=None)
    ap.add_argument("--fragment", action="store_true", help="page body only (artifact publishing)")
    ap.add_argument("--lang", default="en", choices=["en", "zh"])
    a = ap.parse_args()
    global LANG
    LANG = a.lang
    name, body = build(a.out_dir, a.title)
    head = f'<title>{esc(name)}</title>\n<meta name="claude:viewport-fit-sides" content="cover">\n{FONTS}\n<style>{CSS}</style>\n'
    page = head + body if a.fragment else (
        f'<!doctype html><html lang="{"zh-CN" if LANG == "zh" else "en"}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">'
        + head + "</head><body>" + body + "</body></html>")
    outp = a.output or os.path.join(a.out_dir, "report.html")
    open(outp, "w", encoding="utf-8").write(page)
    print(f"simreport: {outp} ({len(page) / 1024:.0f} KiB)")


if __name__ == "__main__":
    main()
