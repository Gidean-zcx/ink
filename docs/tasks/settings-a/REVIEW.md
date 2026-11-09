# settings-a 第 1 轮复查

日期：2026-11-09。基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。复查版本：`13d1b46f`。

**结论：未达到完整验收；严重 0、高 0、中 4。** 三项设置的真实写入、重启恢复及儿童限制成立，但备份状态刷新和翻页后的可见文字有缺陷，分页测试存在漏检，四页布局仍明显偏离设计。普通与 ASan/UBSan 全量均为 **224/225**，唯一失败是共享能力文档过期。开发报告已披露文档和视觉缺口，没有将这两项虚报为通过。

本轮只改本报告，不改产品或测试源码，不提交。构建、替换对象及模拟器证据保留在被忽略的 `build/`、`build-asan/` 中；按要求生成并打开 `out/ui-compare/` 后，因 `out/` 未被忽略，已把本轮图片移至 `build/settings-a-review/ui-compare/`，未改忽略规则。所有【实测】均为本机 macOS/clang 无窗口模拟器，不代表真机或 Linux/gcc 已验证。

## 验收逐条核对

| 项目 | 复查结论 | 独立证据 |
| --- | --- | --- |
| 1：四页、后端值、重启 | 部分通过 | 原定向测试 134 项通过，第二进程重启 10 项通过；三项设置确实恢复。补查发现备份已经启动但页面仍显示“还没有备份”（R1-02）。报告明列的未接功能仍是空态，不算已实现。 |
| 2：每页快照、按钮与 AI 同能力 | 部分通过 | 四页都有可解析的 F1/F4 快照；写入都走 `settings.set`，隐私权限仍经人工确认。补查发现实际画面缺少快照中的文字（R1-01）；撤掉翻页执行后定向测试仍通过（R1-03）。 |
| 3：孩子看不到、改不了，反例失败 | 通过 | 自行运行 `settings_a_reverse.py`：去掉页面检查、去掉设置检查分别退出 1，各 4 条断言失败，均 `test.finished=1`，不是靠崩溃判失败。正常版本拒绝四页入口和三项成人设置。 |
| 4：全量、内存、350 行预算 | 未全部通过 | 普通和 ASan/UBSan 均 224/225、退出 8、零跳过；仅 `capability-table-doc` 失败。未见 ASan/UBSan 报错。产品新增及删除合计 234 行；将 4 行 CMake 测试接线也计入为 238 行，均低于 350。 |
| G1–G5 与设计约束 | 有缺口 | 尺寸检查、分层检查、确定性测试通过；已重新生成并逐张查看 10 屏对照图。四个新页面均超过 12/255 的视觉目标（R1-04）。G3 的共享文档按总规则第 7 节交合并主会话更新，本轮不越权修改。 |

## 问题

### R1-01｜中｜复用列表行未清除隐藏标志，翻页后正文消失

- **位置**：`system/ui/shell/settings_pages.c:117–128`；槽位复用发生在 `system/ui/ui_list.c:78–89`。
- **复现命令**：运行下方附录命令；或用已生成的 `build/settings-a-review/probe` 按附录的模拟器参数运行。人工路径：打开“语音和 AI”→下一页。
- **结果【实测】**：默认字号和特大字号下，“AI 记得的事”下方均没有“记忆页面还没接入”，但该页 JSON 的 `fields[0].value` 仍包含此句。补查分别报 `font1-SetVoice-2: value hidden` 和 `font3-SetVoice-2: value hidden`。截图：`build/settings-a-review/probe-output/snap-font1-SetVoice-2.png`；快照：同目录 `snapshots.ndjson`。
- **原因【实现】**：第一页的免按键分段控件隐藏了第二个 label；第二页复用该槽时只换文字，没有清除 `LV_OBJ_FLAG_HIDDEN`。
- **最小修法**：在 `bind_row()` 一处先恢复两个基础 label 的可见性，再按当前行类型隐藏需要隐藏的 label；增加一次第二页可见文字断言即可，不改 F4。

### R1-02｜中｜备份已开始，页面和 AI 页面快照仍说“还没有备份”

- **位置**：`system/ui/shell/settings_pages.c:156–163,219–222`；事件契约证据为 `system/services/backup/backup_service.c:1469–1500`。
- **复现命令**：运行附录命令。场景在存储页模拟普通断网，然后真实触摸“备份”，通过 `backup.status` 核对任务已经启动，再读取页面。
- **结果【实测】**：后端查询成功且 `phase` 已不为 `none`，`action.user.backup.now=1`，页面的 `backup.value` 仍为“还没有备份”。补查报 `backup has started but page still says no backup`；该次运行 `backup.retries=7`，不是按钮没有触发。证据：`build/settings-a-review/probe-output/metrics.json` 和最后一条 `snapshots.ndjson`。
- **原因【实现】**：页面只在打开时和收到 `backup.changed` 时查询状态；已有 `backup.now` 接受任务时没有发这个事件。其他状态通知即使重绘列表，也只使用旧的 `S.backup`。
- **最小修法**：在备份服务已成功保存启动意图的位置发出既有 `backup.changed`，让本页现成订阅重读状态；不加轮询、后台重试或另一套状态。用本次“开始备份后读页面”的断言回归。

### R1-03｜中｜删掉翻页执行，原定向测试仍然全部通过

- **位置**：`tests/settings_a.c:118–120`、`tests/settings_a.py:38–43`。
- **复现命令**：运行附录命令；其中仅将 `settings_pages.c:208` 的 `ink_ui_list_turn(...)` 替换为 `(void)0` 编译为单独对象，链接 `paging-disabled`，再运行未修改的 `tests/settings_a.py --build build --binary build/settings-a-review/paging-disabled`。
- **结果【实测】**：翻页能力成为空操作，测试仍退出 0：`write: PASS checks=134`、`restart: PASS checks=10`，零失败且正常完成。正式源文件和二进制未改。证据：`build/settings-a-review/paging-disabled.log`、`build/test-out/settings-a-paging-disabled/`。
- **原因【实现】**：测试只确认下一页调用返回 DONE，随后按上一页，再查第一页原本就有的字段；从未断言中间到了第二页。Python 只检查每份快照内部自洽。
- **最小修法**：在下一页调用后立即断言 `pagination.page=2`、正确的 `offset` 和第二页字段；再断言实体键回到第 1 页。另补 R1-01 的可见文字检查，不另建测试框架。按复查提示“检查工具自身的覆盖缺口最多算中”定级，不因测试通过而上调严重性。

### R1-04｜中｜四页统一铺大卡片，丢失设计中的分组和层次

- **位置**：`system/ui/shell/settings_pages.c:102–137,171–188`。
- **复现命令**：见下方“画面对照”命令，打开生成的 `out/ui-compare/SetVoice.png`、`SetStorage.png`、`SetAbout.png`、`SetPrivacy.png`；本轮留存版本在 `build/settings-a-review/ui-compare/`。
- **结果【实测】**：四页分数分别为 63.461、58.984、57.825、31.949，均超过目标。具体差异见 10 屏记录。除规范要求的字号、灰度变化外，语音页的小节与说明消失，关于页居中身份区变为普通两张卡，隐私段落因等高单元格产生大片空白；这些并非后端缺数据所必需。开发报告已披露，仍是未完成的视觉部分。
- **最小修法**：只调整这四页已有绘制，复用现有分组行、图标及文字组件，删掉不必要的逐字段大卡外框，恢复设计的小节和关于页身份区；沿用 F1/F4、当前主题和必要分页。不改主题迎合旧画板，不加布局框架。

## 画面对照：已逐张查看 10 屏

运行命令（退出 0；工具超过目标也返回 0，不能以退出码代替看分数）。本轮完成查看后将该输出目录移入 `build/settings-a-review/ui-compare/`，表中的 10 张对照图和 `scores.json` 都保留在那里：

```bash
INK_DESIGN="$INK_DESIGN" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py \
  --build build --out out/ui-compare \
  --scenes SetVoice SetStorage SetAbout SetPrivacy Settings SetDisplay SetSound SetNetwork Control Main
```

| 屏幕 | 平均灰度差 /255 | 肉眼核对 |
| --- | ---: | --- |
| SetVoice | 63.461 | 原图为分段控件、紧凑分组、“AI”“打字”小节和说明；实现为 202 高等高卡片，缺图标、分组标题和“没锁屏时才听”的说明，后两项到了第二页。 |
| SetStorage | 58.984 | 备份图标和时间、空间分区条及分类分组未还原；变成五张状态卡。真实空闲值显示为 `4194304 KiB`，阅读负担明显高于原图的 GB 表达。缺分类服务已披露，不应填画板示例值。 |
| SetAbout | 57.825 | 居中标志、设备名和版本区被普通列表卡替代；图标、分组分隔、底部清空入口位置不同；新增自动下载开关出现白矩形嵌在灰卡中，设备详情等到了第二页。 |
| SetPrivacy | 31.949 | 文段左边距因卡内边距再次内缩，等高行在短段落后留白很大；新增剪贴板开关，保留期和儿童说明移到第二页，底部记录入口未接。未发现错字，主要是结构和内容完整性差异。 |
| Settings | 81.154 | 右上搜索、图标底圆、分隔线与原图不同，整体更高；存储和关于入口现可调用。其余视觉差异属于基线，未归责本任务。 |
| SetDisplay | 47.923 | 预览卡更矮，底部原有图标列表改为两个胶囊，“屏幕”分组标题不见。基线差异。 |
| SetSound | 46.001 | 当前输出用“当前”替代勾，提示分组缩为勿扰，铃声在第二页，留白较多。基线差异。 |
| SetNetwork | 36.303 | Wi-Fi 分组与列表被未连接说明和两个按钮替代；SIM 状态区不同。测试中的真实空态与设计示例不同，基线差异。 |
| Control | 39.888 | 未连接、无通知替代示例内容，底部用“收起”按钮替代短横线。基线差异。 |
| Main | 54.194 | 时钟与图标放大后纵向位置变化、继续读为空态、Dock 图标不同。基线差异，不建议在此任务改主屏。 |

现行规范 §1 明确设计稿未同步字号放大和 `#888888` 灰底；这两项本身不作为缺陷，也不以“必须调回旧尺寸”作为修法。另查看了默认与特大字号的分页运行证据，共生成 16 个不同页面帧，确认 R1-01。

## 构建、测试与范围检查

下列命令由本轮自行执行；两轮全量串行，构建和 CTest 均最多 `-j2`：

```bash
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/settings_a_reverse.py --build build
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
git diff --check f0b0c539f3f8a30bca03f663484287d65c76de33
git diff f0b0c539f3f8a30bca03f663484287d65c76de33..HEAD | rg -n '/Users/|/private/tmp|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}'
```

- 【实测】两个构建退出 0；普通全量 213.53 秒，ASan/UBSan 全量 234.12 秒，各 224/225，退出 8。完整日志分别为 `build/Testing/Temporary/LastTest.log`、`build-asan/Testing/Temporary/LastTest.log`。唯一失败均为 `capability-table-doc`，由新增六项导航能力导致生成表不一致；合并主会话需按总规则第 7 节更新共享文档，不能跳过这个检查。
- 【实测】正常设置测试两进程内部 RAM 最低余量 47840 字节，阈值 32768；内部和外部分配失败均为 0。反向对照两组各 130 通过、4 失败，退出 1。
- 【实测】差分格式检查通过，提交差分隐私扫描零匹配（`rg` 退出 1）。未发现删测试、放宽断言/预算、用 sleep 掩盖时序或改动其它并行任务文件；现有 AI 能力清单只追加六项，原断言保留。
- 【实现】变化限定为四页、入口、构建接线、相应测试及报告；没有设备操作、付费接口、真实通信或对外发布。未修改共享规范及调研资料。Linux/gcc 与实机未验证。

## 已披露缺口与架构审视

唤醒名字、打断开关、接话设置、分类统计、清理、设备详情、许可与记录入口仍未接入。按 G4 允许无服务时呈现空态，不把这些文字当作已完成的功能。关于页 `update.check` 的失败/无新版本回执没有呈现，原报告已披露；读取 `update.state` 不能代替消费该回执（`update_service.c:571–604`）。这些限制应保留在交接中。

模块边界基本合理：页面只经设置、状态、能力和事件访问后端，写入走既有 `settings.set`；新页面不直接写服务内部数据。AI 和触摸共用能力及权限策略，密码/权限确认没有被新页面绕过。AI 原生仍有两处缺口：可见文本与快照不一致（R1-01）、动作后页面状态不更新（R1-02）。

## 过度设计与最小修法

未发现应删除的新任务、线程、状态机、持久格式或配置项。四页共用一个有限的行表和 F1/F4，代次检查只保护异步备份回执，规模与任务相称。没有理由拆出新的通用设置框架或重构导航。建议删除的仅是 R1-04 所述多余卡片包装；其余按 R1-01 的单点恢复可见性、R1-02 的现有事件补齐和 R1-03 的直接断言处理。

## 附录：R1-01～03 的可复现补查

下方脚本从现有源码读取内容，只在内存中替换测试驱动或单个产品函数，沿用现有编译参数和库。生成单独对象及二进制，不覆盖产品源码或正式二进制。补查退出 1、448 条通过、3 条失败并正常完成；失败为两个字号下的隐藏文字及备份页面陈旧状态。禁用翻页的变体反而通过原测试。精确编译/链接/运行参数保存在 `build/settings-a-review/commands.json`。

复跑整个补查（会更新同名的生成证据）：

```bash
PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
report = Path('docs/tasks/settings-a/REVIEW.md').read_text()
code = report.split('<!-- review-probe:start -->\n```python\n', 1)[1].split('\n```', 1)[0]
exec(compile(code, '<settings-a-review>', 'exec'))
PY
```

<details>
<summary>补查脚本（仅生成测试产物）</summary>

<!-- review-probe:start -->
```python
import os, re, shlex, subprocess, json
from pathlib import Path
root = Path.cwd()
build = root / "build"
area = build / "settings-a-review"
area.mkdir(exist_ok=True)
env = dict(os.environ, INK_KEEP_EVIDENCE="1", PYTHONDONTWRITEBYTECODE="1")
commands = []
def execute(args, **kw):
    commands.append([str(x) for x in args])
    p = subprocess.run(args, cwd=build, capture_output=True, text=True, env=env, **kw)
    if p.returncode:
        print(p.stdout[-2000:] + p.stderr[-4000:])
    return p
def compile_and_link(name, source, target, replace_test=False):
    flags = (build / ("CMakeFiles/" + target + ".dir/flags.make")).read_text()
    opts = []
    for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS"):
        opts += shlex.split(re.search("^" + key + r" = (.*)$", flags, re.M)[1])
    obj = area / (name + ".o")
    link = shlex.split((build / "CMakeFiles/sim-settings-a.dir/link.txt").read_text())
    inc = root / ("tests" if replace_test else "system/ui/shell")
    p = execute([link[0], *opts, "-iquote", str(inc), "-x", "c", "-", "-c", "-o", str(obj)], input=source, timeout=60)
    assert p.returncode == 0
    link[link.index("-o") + 1] = str(area / name)
    if replace_test:
        index = next(i for i, v in enumerate(link) if v.endswith("/tests/settings_a.c.o"))
        link[index] = str(obj)
    else:
        index = next(i for i, v in enumerate(link) if v.endswith("libink_system.a"))
        link.insert(index, str(obj))
    assert execute(link, timeout=60).returncode == 0
    return area / name
source = (root / "tests/settings_a.c").read_text()
helpers = r'''
static char frame_name[100];
static volatile bool captured;
static int review_page, review_font;
static void capture_frame(void *arg) { sim_display_snapshot(arg); captured = true; }
static void review_view(void) {
    read_snapshot();
    const char *pagination = strstr(snapshot, "\"pagination\":");
    CHECK(pagination && sscanf(pagination, "\"pagination\":{\"page\":%d", &review_page) == 1, "page number is readable");
    snprintf(frame_name, sizeof frame_name, "font%d-%s-%d", review_font, ink_ui_page_name(), review_page);
    const char *p = strstr(snapshot, "\"fields\":[");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    char text[384];
    if (p && j) for (p += strlen("\"fields\":["); *p == '{'; ) {
        const char *end = strstr(p, "}}");
        CHECK(end && !ink_json_parse(j, p, (size_t)(end - p) + 2), "field parses");
        if (!end) break;
        ink_json_str(j, ink_json_find(j, 0, "label"), text, sizeof text);
        wanted = text; find_label();
        CHECK(x >= 0 && y >= 0, "%s: title hidden: %s", frame_name, text);
        if (!ink_json_eq(j, ink_json_find(j, 0, "role"), "setting")) {
            ink_json_str(j, ink_json_find(j, 0, "value"), text, sizeof text);
            wanted = text; find_label();
            CHECK(!text[0] || (x >= 0 && y >= 0), "%s: value hidden: %s", frame_name, text);
        }
        p = end + 2;
        if (*p == ',') p++;
    }
    osal_free(j);
}
static void review_snapshot(void) {
    quiet(); on_ui(review_view);
    captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture_frame, frame_name);
    CHECK(WAIT_FOR(captured, 1000), "capture current page");
}
static void outage_event(void *arg) { (void)arg; sim_net_outage(SIM_MS(60000)); }
'''
body = r'''static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home(); quiet();
    for (review_font = 1; review_font <= 3; review_font += 2) {
        char args[80];
        snprintf(args, sizeof args, "{\"id\":\"display.font_size\",\"value\":%d}", review_font);
        CHECK(AI("settings.set", args) && g_rc.status == INK_RC_DONE, "set real font size");
        quiet();
        for (int i = 0; i < 4; i++) {
            CHECK(AI(CAPS[i], NULL) && !strcmp(ink_ui_page_name(), PAGES[i]), "open page");
            review_snapshot();
            for (int k = 0; k < 3 && strstr(snapshot, "\"id\":\"next\""); k++) {
                CHECK(AI("shell.settings_next", NULL), "next page");
                review_snapshot();
            }
            for (int k = 0; k < 3 && strstr(snapshot, "\"id\":\"prev\""); k++) {
                CHECK(AI("shell.settings_prev", NULL), "previous page");
                review_snapshot();
            }
            CHECK(BY_HAND("system.home", NULL), "close page"); quiet();
        }
    }
    CHECK(AI("settings.set", "{\"id\":\"display.font_size\",\"value\":1}"), "normal font");
    CHECK(AI("shell.storage", NULL), "open storage"); quiet();
    vtk_event_at(vtk_now() + SIM_MS(1), outage_event, NULL);
    CHECK(WAIT_FOR(!ink_hub_online(), 10000), "ordinary network outage");
    press("备份");
    CHECK(AI("backup.status", NULL) && g_rc.status == INK_RC_DONE &&
          !strstr(g_result, "\"phase\":\"none\""), "backup really started");
    on_ui(read_snapshot);
    CHECK(strstr(snapshot, "备份或恢复正在进行"), "backup has started but page still says no backup");
    ink_probe_count("test.finished");
}
'''
start = source.index("static void run(void *arg) {")
end = source.index("void ink_main(void)", start)
probe = compile_and_link("probe", source[:start] + helpers + body + source[end:], "sim-settings-a", True)
out = area / "probe-output"
p = execute([str(probe), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
    "--script", str(root / "tests/settings_a.script"), "--budget", str(root / "tests/ui_scene.budget.json"),
    "--assets", str(build / "assets"), "--content", str(build / "test-content"),
    "--out", str(out), "--no-random-outages", "--quiet"], timeout=90)
(area / "probe.log").write_text(p.stdout + p.stderr)
m = json.loads((out / "metrics.json").read_text())
print("probe exit", p.returncode, "pass", m["metrics"].get("test.pass"), "fail", m["metrics"].get("test.fail"), "finished", m["metrics"].get("test.finished"))
product = (root / "system/ui/shell/settings_pages.c").read_text()
needle = 'else ink_ui_list_turn(S.list, !strcmp(c->cap->id, "shell.settings_next") ? 1 : -1);'
assert product.count(needle) == 1
mutant = compile_and_link("paging-disabled", product.replace(needle, "else (void)0;"), "ink_system")
p = execute([os.sys.executable, str(root / "tests/settings_a.py"), "--build", str(build), "--binary", str(mutant)], timeout=180)
(area / "paging-disabled.log").write_text(p.stdout + p.stderr)
print("paging-disabled existing-test exit", p.returncode, p.stdout.strip())
(area / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
```

</details>

SEVERE=0 HIGH=0
