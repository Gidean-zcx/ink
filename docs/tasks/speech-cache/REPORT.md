# speech-cache 执行报告

基线：`291da6760931601e4783ebaddabc08e5bcab22eb`。范围：DE-R6，仅音频服务及其测试；产品代码预算 60 行。开始时工作区干净。

【实测＋实现】DE-R6 在当前基线不可复现；基线已含 `39b7dcf4` 的修复和对应回归。按任务书的不可复现分支交付，本次不修改产品代码或测试。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 适用任务书“不可复现”分支；无需新增修复 | 【实测】普通和内存版的已有 private 回归均 33 通过、0 失败、旧语音新增 0 ms；仅在临时音频副本关闭身份校验，两套均 32 通过、1 失败，检出 810 ms。均 `test.finished=1`、模拟器退出 0。普通版原审计语音探针 case/control 各 11 通过、0 ms。 | 两套全量 CTest 中的 `device-events`；下方负面对照脚本已逐套实际执行 |
| 2 | 通过 | 【实测】普通全量 205/205（112.74 s），ASan/UBSan 全量 205/205（214.44 s），均 0 失败、0 跳过，无 sanitizer 错误诊断。产品代码增删 0 行，未超过 60 行；差异仅本报告。按总规则 §6 扫描提交差异的隐私正则为 0 命中，`git diff --check` 通过。 | 配置采用总规则 §5，另加 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；各目录执行 `taskpolicy -c utility cmake --build <目录> -j3`、`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir <目录> --output-on-failure -j2`；内存目录 `build-asan` 加 `-DINK_SANITIZE=ON`；`git diff --stat 291da676..HEAD` |

【实现】`git merge-base --is-ancestor 39b7dcf4 291da676` 退出 0。修复已在基线内：[音频身份校验](../../../system/services/audio/audio_service.c)的 `playback_owner_matches`、`stage_next`、`staged_wanted`、`service_playback` 分别覆盖待播流、PCM 与播放出口；不只依赖成员状态通知。[既有回归](../../../tests/device_events/private.c)第 94–113 行先断言回答已结束且仍有超过 20 帧待播，再同时暂停 UI 与成员通知，等待 200 ms 后观察 800 ms 的扬声器增量。

负面对照可在完成构建与全量 CTest 后重建：下列脚本只改 `work/` 中的音频副本，将 `playback_owner_matches` 改为恒真，重链接已有 private 测试；目标失败仍是原断言。用 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python"` 执行；内存版在命令前加 `INK_REVIEW_BUILD=build-asan`。脚本退出 0 表示检出了预期失败，不表示破坏后的产品通过。

```python
import json, os, re, shlex, subprocess
from pathlib import Path
b = Path(os.getenv("INK_REVIEW_BUILD", "build")).resolve()
w = Path("work/speech-cache", "reverse-" + b.name).resolve()
w.mkdir(parents=True, exist_ok=True)
s, n = re.subn(r"(static bool playback_owner_matches\([^\n]+\) \{)\n.*?\n\}",
               r"\1\n    return true;\n}",
               Path("system/services/audio/audio_service.c").read_text(), count=1, flags=re.S)
assert n == 1
(w / "audio.c").write_text(s)
t = b / "CMakeFiles/sim-device-events-private.dir"
f = dict(line.split(" = ", 1) for line in (t / "flags.make").read_text().splitlines() if " = " in line)
link = shlex.split((t / "link.txt").read_text())
flags = sum((shlex.split(f[k]) for k in ("C_DEFINES", "C_INCLUDES", "C_FLAGS")), [])
subprocess.run([link[0], *flags, "-c", str(w / "audio.c"), "-o", str(w / "audio.o")], cwd=b, check=True)
link[link.index("-o") + 1] = str(w / "private-negative")
link.insert(next(i for i, v in enumerate(link) if v.endswith(".a")), str(w / "audio.o"))
subprocess.run(link, cwd=b, check=True)
r = subprocess.run([str(w / "private-negative"), "--profile", "simulator/profiles/metalio-plus.json",
    "--assets", str(b / "assets"), "--content", str(b / "test-content"),
    "--hub", str(b / "test-out/device-events/private-hub.json"), "--out", str(w / "out"),
    "--end", "90s", "--quiet", "--no-random-outages"], capture_output=True, text=True,
    timeout=60, env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
(w / "run.log").write_text(r.stdout + r.stderr)
m = json.loads((w / "out/metrics.json").read_text())["metrics"]
print({k: m.get(k) for k in ("test.pass", "test.fail", "test.finished", "test.private_speech_after_handoff_ms")})
assert r.returncode == 0 and m.get("test.finished") == 1 and m.get("test.fail") == 1
assert m["test.private_speech_after_handoff_ms"]["max"] > 0
```

## 没做到的

- 本任务范围内无遗留项。未新增修复和测试，原因是指定基线已修复且已有回归；采用任务书明确允许的不可复现分支。没有设备、真实厂商或 Linux gcc 的实测结论。

## 和设计不一致的

- 未发现本任务范围内的不一致。音频隔离属于服务内部机制，仍经平台 HAL 输出；没有新增用户动作、能力、页面或旁路。未改 `system/policy/`。

## 改过的已有测试

- 无。

## 要用户定的

- 无。

## 顺手发现

- 【实测】原审计语音探针没有暂停 UI，关闭音频身份校验后也未检出泄漏；因此不以它单独证明修复有效。已有 private 回归同时暂停 UI 和状态通知，负面对照能检出上述 810 ms。
- 【实测】题设构建用的 Python 缺少中枢测试依赖，导致首轮 3 项明确跳过。现有 `$INK_DEPS/hub/.venv/bin/python` 为 Python 3.11.15，四项依赖均与 `hub/pyproject.toml` 固定版本一致；通过 `INK_HUB_PYTHON` 选用，不安装或修改依赖。

## 新增的机制

- 无产品机制；本次仅验证已有实现。

## 要改的文档

- 主会话收拢时更新 `docs/tasks/backlog.md` 的 speech-cache 状态：基线已有修复，本次验证不可复现；不记为本次新增修复。其余共享文档无需因本任务改变。
