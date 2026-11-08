# speech-cache 第 1 轮独立复查

日期：2026-11-08。任务基线：`291da6760931601e4783ebaddabc08e5bcab22eb`；受检提交：`9ef15a60`。本轮只修改本报告，不修改产品或测试源码，不提交。

**结论：通过，严重 0、高 0。** DE-R6 在指定基线不可复现，既有回归能检出关闭音频身份校验后的旧语音泄漏。符合任务书“不可复现则说明证据、不改代码”的分支；不能记作本任务新增修复。

## 逐条验收

| 编号 | 结论 | 独立证据与命令 |
| --- | --- | --- |
| 1：复现、修后通过、撤修失败 | 通过，适用不可复现分支 | 【实测】`git merge-base --is-ancestor 39b7dcf4 291da676` 退出 0；产品与测试树相对基线无差异。两套 `device-events` 的 private 回归各 33 通过、0 失败，旧语音增量 0 ms；只在临时音频副本关闭身份校验后各 32 通过、1 失败，检出 810 ms；再运行未改产品均回到 33/0、0 ms。命令见下。没有声称“指定基线失败”。 |
| 2：全量、内存检查、60 行预算 | 通过 | 【实测】普通与 ASan/UBSan 构建均退出 0；全量分别 205/205、97.43 秒和 205/205、181.23 秒，均退出 0、无跳过，无 sanitizer 错误诊断。`git diff --numstat 291da676..HEAD` 仅执行报告新增 76 行，产品代码增删 0 行。 |

## DE-R6 的实现与测试证据

- 【实现】`system/services/audio/audio_service.c:157` 按成员、epoch 和语音认证状态校验归属；`:384` 在取待播帧时校验，`:509` 撤销失效流并清除 PCM，`:547` 检查已打开的输出，`:574` 在提交 PCM 前再次检查。停止缓存播放不再只靠成员状态通知。
- 【实现】`tests/device_events/ai_boundary.c:133` 的 `test_ai_private_queued` 确认回答已结束、实时会话已关闭且仍有超过 20 帧待播；`tests/device_events/private.c:94` 起使用真实语音入口及合成中枢，在 UI 与成员通知暂停时切到孩子。`:107` 的 200 ms 用于排除已提交 DMA 的尾音，之后观察 800 ms；`:112` 要求播放增量严格为 0。未修改该断言或等待时长。
- 【实现】播放增量来自 `ports/sim/hal/hal_audio_sim.c:579` 的扬声器计数，计数在模拟 DMA 消费数据时增加（`:291`），不是仅检查服务的“已停止”标记。

| 独立运行 | 普通版 | ASan/UBSan 版 |
| --- | --- | --- |
| private 正向回归 | 33 通过、0 失败；0 ms | 33 通过、0 失败；0 ms |
| `playback_owner_matches` 临时改为恒真 | 32 通过、1 失败；810 ms | 32 通过、1 失败；810 ms |
| 反向对照后重跑未改产品 | 33 通过、0 失败；0 ms | 33 通过、0 失败；0 ms |

以上六次均 `test.finished=1`、模拟器退出 0；两次反向对照在 `metrics.json.warnings` 中都只有 `tests/device_events/private.c:112` 的目标失败。模拟器退出 0 本身不代表断言通过。

【实测】另从 `docs/tasks/device-events/REVIEW.md` 提取原始 speech 夹具，普通版 case/control 各 11 通过、0 ms；链接关闭身份校验的音频对象后仍各 11 通过、0 ms。原探针没有暂停 UI，不能单独证明修复有效。执行报告已明确披露此限制，且使用能检出缺陷的 private 回归补足，因此不新增问题编号。

## 问题

本任务范围内未发现需报告的严重、高、中、低问题。没有把人为暂停线程的探针结果外推为正常切换必然泄漏，也没有把基线已有问题重新算成本任务引入的问题。

## 总规则、设计与架构审视

- 【实测】相对基线只有 `docs/tasks/speech-cache/REPORT.md`，未夹带 `system/policy/` 或其他产品改动；未改测试、断言、预算或依赖，未增加 sleep。提交差异按总规则隐私正则扫描为 0 命中，`git diff --check 291da676..HEAD` 退出 0。
- 【实现】音频归属检查属于音频服务内部机制，符合系统架构“四类东西”的划分。输出仍走平台 HAL；没有新增跨层引用或绕过成员策略的路径。分层、SDK 边界、确定性及预算测试均包含在本次通过的全量中。
- 【实现】本任务没有新增用户动作、页面或数据接口，无需新增原子能力或 AI 页面快照；现有按钮、AI 与软总线的调用边界没有变化。无需把缓存内部操作直接开放给 AI。
- 本任务不是界面任务，不适用十屏设计稿对照；未使用设计稿目录。全量中的 `gui-smoke` 使用 SDL dummy，未启动可见窗口。

## 过度设计

本任务产品代码 0 行，没有新增任务、状态机、文件格式、配置项、缓存、队列或抽象层。已有问题已修复时停止追加产品改动，符合“小问题小改”。**建议删除项：无。** 不建议为重复验证另建测试框架或逐个调用方补检查。

## 执行命令与证据边界

复用了已配置的构建目录，核对两套缓存均为 `INK_WERROR=ON`，内存版另为 `INK_SANITIZE=ON`；字体、Python、LVGL 使用题设 `INK_DEPS`。`INK_HUB_PYTHON` 指向 `$INK_DEPS/hub/.venv/bin/python`，实测 Python 3.11.15，四项依赖版本与 `hub/pyproject.toml` 一致，没有安装或修改依赖。

以下构建与全量按序执行，没有重叠运行；构建最多 3 路、CTest 最多 2 路：

```sh
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
git merge-base --is-ancestor 39b7dcf4 291da676
git diff --quiet 291da676 -- system tests CMakeLists.txt platform ports simulator
git diff --numstat 291da676..HEAD
git diff --check 291da676..HEAD
```

反向对照复现命令如下。本轮逐套执行了相同操作：运行执行报告中的脚本，仅把输出目录改到新建的临时目录；不替换仓库源码或库文件。脚本退出 0 表示检出预期缺陷。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
import json, os, tempfile
from pathlib import Path
code = Path('docs/tasks/speech-cache/REPORT.md').read_text().split('```python\n', 1)[1].split('\n```', 1)[0]
old = 'w = Path("work/speech-cache", "reverse-" + b.name).resolve()'
assert code.count(old) == 1
for build in ('build', 'build-asan'):
    os.environ['INK_REVIEW_BUILD'] = build
    folder = Path(tempfile.mkdtemp(prefix='ink-speech-review-'))
    ns = {}
    exec(code.replace(old, 'w = Path(' + repr(str(folder)) + ')'), ns)
    data = json.loads((folder / 'out/metrics.json').read_text())
    warnings = data.get('warnings', [])
    assert len(warnings) == 1
    assert 'tests/device_events/private.c:112' in warnings[0]
    assert 'finished adult speech cannot play before member publication' in warnings[0]
    print(build, 'expected private.c:112 failure confirmed')
PY
```

全量日志在各构建目录的 `Testing/Temporary/LastTest.log`，正向 private 指标在 `test-out/device-events/private/metrics.json`；两套日志各记录 205 个通过、0 个失败、0 个跳过，未见 AddressSanitizer 或 UBSan 错误。临时夹具、目标文件、二进制及日志只留在系统临时目录，构建与测试产物在既有忽略目录；仓库待交付文件只有本报告。

本结论限于指定 DE-R6 场景与当前 macOS 模拟器。未执行 Linux gcc、真机或真实语音厂商测试；未触碰设备、读取硬件备份、调用付费接口或进行远端写入。

SEVERE=0 HIGH=0
