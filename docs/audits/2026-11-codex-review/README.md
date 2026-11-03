# 2026-11 Codex 审计复核：复现材料

结论见 [审计记录](../README.md)。这里只放复现用的脚本和小程序，都在模拟器或主机上运行，不接触设备。

先按 [background.md §11](../../background.md) 构建，下面的命令都在仓库根目录执行：

```bash
SIM="./build/ink-sim --assets build/assets --content build/test-content --quiet --no-random-outages"
R=docs/audits/2026-11-codex-review
```

| 条目 | 命令 | 看什么 |
| --- | --- | --- |
| AUD-002 连续翻页积压 | `$SIM --profile profiles/metalio-plus.json --script $R/rapid_4hz.script --out out/rapid`；对照组换 `slow_15s.script` | `ui.action_to_first_change_ms.p95`、`ui.action_to_visible_ms.TEXT.p95` |
| AUD-004 连续阅读不落盘 | `$SIM --profile profiles/metalio-plus.json --script $R/continuous_save.script --flash-image out/cont.img --out out/cont-save`，再用 `continuous_resume.script` 和同一个镜像跑一次 | 第一次没有 `storage.write_ms`；第二次 `reader.restored_page` |
| AUD-007 页面代次 | `$SIM --profile profiles/metalio-plus.json --script $R/gen_race.script --hub $R/gen_race.hub.json --out out/gen` | `tools.calls.done` 与 `reader.page_turns`（AI 的“下一页”在手动翻页之后仍被执行） |
| AUD-013 三个触点 | `$SIM --profile profiles/metalio-plus.json --script $R/three_contacts.script --out out/3c` | `input.touch_delivery_ms.max`：修复后手指要等一个盖板键松开 |
| AUD-011 PQ 相时钟 | `cc -O1 -std=c11 -Idrivers/epd -o out/pqbn_clock $R/pqbn_clock.c drivers/epd/pqbn_v4.c && out/pqbn_clock 20 3` | 被内核误判为“刚动过”的静止像素比例；参数是每次驱动的相数和“驱动 + 长静止”的轮数 |
| AUD-010 JSON 与 UTF-8 | 见下 | 非整数、溢出被拒绝；非法 UTF-8 解成 U+FFFD |
| AUD-016 size 脚本 | `bash $R/size_probe.sh tools/s31-size/run.sh` | 一个子工程失败时脚本的退出码 |

AUD-010 的小程序要 LVGL 头文件（`text.h` 经 `font.h` 引入）：

```bash
L=build/_deps/lvgl-src
cc -std=c11 -Iplatform/include -Isystem/include -Ithird_party/jsmn -I$L -Isystem/ui -DLV_CONF_INCLUDE_SIMPLE \
   -o out/json_probe $R/json_probe.c $R/json_probe_stub.c system/core/ink_json.c system/text/linebreak.c && out/json_probe
```

**AUD-012 的两个变异**：在仓库副本里改一处、重新构建、跑原场景和预算。

- 关闭 PQ 相时钟：`ports/sim/hal_display_sim.c` 的 `pq_advance_idle()` 开头加 `return;`，跑 `reader_basic` + `reader_basic.pqbn.budget.json`（PQ-BN 档案）。
- 盖板键退回一个触点：`ports/sim/hal_input_sim.c` 里盖板键按下的条件 `points_reported() < g_profile.touch_points` 改成 `< 1`，跑 `keys_overlap`。

复核时（修复前）两个变异都通过了全部预算；补上 `display.pqbn.idle_advance_phases` 与 `input.key_delivery_ms.max` 两条门槛后都会失败。

**另外三个复现已经变成 CI 场景**：`tests/scenarios/reader_continuous`（AUD-004）、`ai_dismiss`（AUD-006）、`reader_empty`（AUD-009，内容目录 `tests/content-bad`）。在修复前的代码上，它们分别是“没有落盘”、“关闭后仍播放 10740 ms”、SIGSEGV 退出。
