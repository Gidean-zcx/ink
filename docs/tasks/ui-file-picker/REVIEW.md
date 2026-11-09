# ui-file-picker 第 1 轮复查

日期：2026-11-09。基线：c6dede1945807b2cd688e6967c7299154813827e。被查提交：3d640c32（含前面的 3 次实现提交）。

**结论：暂不通过，发现 1 项高、1 项低问题，未发现严重问题。** 正常选择、取消和成员隔离已复测；调用页在同一成员内被导航销毁时，选择器没有跟着结束。仅修改本报告，不改产品或测试源码，不提交。

## 问题

### R1-01｜高｜回到主页后遗留选择器，并回调已经销毁的调用方

- 文件：[file_picker.c 第 40～50 行](../../../system/ui/file_picker.c)、第 146～163 行。对照：system/policy/system_actions.c:27～31、system/ui/ui_task.c:488～494。
- 根因：【实现】选择会话只保存成员代次、回调及上下文，没有绑定调用页的生命周期。成员没变就执行 p->done(p->ctx, id)，即使原活动已经执行 destroy。
- 正常触发：打开选择器 → AI 调用 system.home → 回执称已回主页 → 人取消仍留在屏幕上的选择器。无需并发干扰或人为卡住线程。
- 复现命令：先执行文末 R1-01 复建命令，再运行：
  ~~~bash
  taskpolicy -c utility build/review-home \
    --profile simulator/profiles/metalio-plus.json \
    --script tests/ui_file_picker.script --budget tests/ui_scene.budget.json \
    --assets build/assets --content build/test-content \
    --out build/test-out/review-home --quiet --no-random-outages
  ~~~
- 结果：【实测】编译、链接退出 0；运行退出 1；test.finished=1、test.pass=35、test.fail=2。底层活动已是 home，原调用方的 destroy 已运行，顶层却仍是 FilePicker；之后取消又触发“callback reached a destroyed caller”。按文末命令复建重跑仍得到相同两条失败；最新证据为 build/test-out/review-home/metrics.json 中的两条 warnings，发生于虚拟时间 6.840 秒、6.852 秒。
- 影响：回执与屏幕不符；按活动生命周期释放上下文的调用方存在悬空回调风险。本次直接实测的是“销毁后仍回调”，不声称已实测崩溃或数据丢失。原测试的 CALLER 没有 destroy，ctx 指向静态 caller_page，所以 91 条断言不会暴露这一问题。
- 最小修法：在选择器内绑定调用页现有的 LV_EVENT_DELETE；调用页销毁时集中关闭浮层并丢弃回调，正常关闭时解除绑定。保留已有 BG 收尾方式，不逐个调用方打补丁，不新加生命周期框架。补一个正常 system.home 的回归即可。

### R1-02｜低｜第二行位置没有跟随字号，两个文字行框重叠

- 文件：[file_picker.c:61](../../../system/ui/file_picker.c)。第二行固定放在 INK_GAP_S + INK_LINE_LIST，但第一行的 ink_ui_line 会按当前字号调整高度；相关实现为 system/ui/theme.c:137、223～227。
- 复现：复用 tests/ui_shell_scenes.c:209～258 的现有 assert_text_fits 检查，在本任务测试第一页画面稳定后检查一次；分别通过真实设置 display.font_size=3 和切换 kid 打开选择器。只在编译器标准输入中的测试副本接入检查，产品源码不变。
- 复现命令：
  ~~~bash
  taskpolicy -c utility build/review-text \
    --profile simulator/profiles/metalio-plus.json \
    --script tests/ui_file_picker.script --budget tests/ui_scene.budget.json \
    --assets build/assets --content build/test-content \
    --out build/test-out/review-text --quiet --no-random-outages
  ~~~
- 结果：【实测】编译、链接退出 0，运行退出 1；test.finished=1、test.fail=8，两个字号各四条“text on text: SYNTH_IMAGE_* and 可以选择”。【实现】最大字号第一行高 64，孩子高 58，第二行偏移仍为 53，行框分别重叠 11、5 像素。
- 证据：[最大字号](../../../build/test-out/review-text/snap-review-big.png)、[孩子字号](../../../build/test-out/review-text/snap-review-child.png)。**截图中的字形尚未互相覆盖**，问题限于行框和行距，所以定低，不升级为“无法阅读”或高风险测试缺口。
- 最小修法：第 61 行用已有 ink_ui_line_height(INK_FONT_LIST) 替换固定 INK_LINE_LIST；复用现有布局检查验证这两个字号，不新增检查框架。

## 逐条验收

| 项目 | 独立复查结果 | 证据与命令 |
| --- | --- | --- |
| 1 图片过滤、分页、返回稳定编号及 lookup | 通过现有验收 | 原版 ui-file-picker 与 ui-file-picker-frames 2/2；7 张图片按 4/3 分页，错误类型不出现，触摸与 AI 选择后 lookup 成功。T1 |
| 2 取消回到原页原位置 | 普通取消通过；生命周期边界未通过 | 同一个活动屏幕对象未重建，快照及取消前后 PNG 全像素相同；正常导航离开后有 R1-01。T1、H1 |
| 3 孩子隔离及反例 | 通过 | 孩子选自己的文件成功，不能看到或 lookup 大人私有文件；撤掉权限检查后同一原测试确实失败 2 条。T1、N1 |
| 4 全量、内存检查、250 行预算 | 行数与运行检查通过；原样检出全量有一项文档失败 | 普通与 ASan/UBSan 均 218/219，唯一失败为 capability-table-doc；201 行产品改动（新增 200、删除 1，含 9 行 CMake）。T2、T3 |

能力总表未提交由总规则第 7 节明确要求，开发报告也已披露先临时生成再跑测试。因此不将这个已披露的合并步骤认定为“虚报完成”。本轮没有改共享文档，**不把本次结果写成 219/219**。合并时仍须生成总表后验证该检查。

## 命令和证据

所有构建、测试均使用 taskpolicy -c utility；构建最多 -j3，CTest 最多 -j2，两套完整构建/测试串行。依赖和设计稿只读。设置 PYTHONPATH="$PWD/hub"、PYTHONDONTWRITEBYTECODE=1，内存检查额外设置 UBSAN_OPTIONS=halt_on_error=1。

- T1：taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ui-file-picker' --output-on-failure -j2。本轮完整测试与反例结束后均验证原版，最后一次 2/2，退出 0。
- T2：按总规则第 5 节全部字体、Python、LVGL 参数重新配置 build，额外指定 -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"；保留 INK_WERROR=ON。完整构建退出 0；ctest --test-dir build --output-on-failure -j2 退出 8，218/219，130.20 秒，零跳过。
- T3：同样参数配置 build-asan，额外开启 INK_SANITIZE=ON。配置和完整构建退出 0；完整 CTest 退出 8，218/219，287.53 秒，零跳过；唯一失败同 T2。日志：build-asan/review-configure.log、review-build.log、review-ctest.log；没有 AddressSanitizer/UBSan 错误。
- 两套原版选择器均 test.pass=91、test.fail=0、test.finished=1，内部 RAM 最低余量 47,472 字节，高于原门槛 32,768；内部及外部分配失败均为 0。分层、SDK、确定性、能力运行时表、中枢契约检查均通过。
- N1：只在编译器输入的 file_service.c 副本删除 resolve_for 的 /members/ 分支中“if (!c->adult) return INK_FS_ERR_DENIED;”，链接到未改的原验收测试。编译、链接退出 0；build/review-member-unchecked 使用与 H1 相同参数、输出改为 build/test-out/review-member-unchecked，运行退出 1；test.finished=1、test.pass=89、test.fail=2。失败断言准确落在 tests/ui_file_picker.c:80（孩子看到大人私有图）与 :122（私有编号 lookup 没有被拒绝）。未改原文件、原库、原可执行程序，随后 T1 仍通过。
- H1：R1-01 所列命令；使用真实 system.home 和 file_picker.cancel 能力，不直接调用产品的私有函数。
- 文字检查夹具第一次编译因复用函数参数 label 与原测试全局 label 重名，被 -Wshadow/-Werror 拒绝；仅将副本局部参数改名后构建通过，没有放宽警告或断言。
- git diff --check 通过；相对指定基线的补丁隐私正则扫描零命中。既有测试只追加能力和 SDK 白名单，没有删断言、放宽预算或增加掩盖错误的 sleep。新测试等待的是虚拟时间条件与已静止画面。
- 本轮证据限 macOS/clang 模拟器；没有运行 Linux/gcc 或真机，没有窗口操作、付费接口、提交、推送或 PR。

## 视觉复查：已打开 11 屏

本次完整测试产生的已静止截图，连同通过标记的 metrics.json，送入 tools/ui_compare.py --reuse；没有编辑截图。先在 out/ui-compare/ 生成并逐屏打开，结束后移到被忽略的 [work/ui-file-picker/review-compare](../../../work/ui-file-picker/review-compare/)，避免增加待提交产物。

重算命令：
~~~bash
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py \
  --build build --design "$INK_DESIGN" \
  --out work/ui-file-picker/review-compare --reuse \
  --scenes FilePicker Main Today Apps Control Notifications Settings SetDisplay SetSound PowerMenu Confirm
~~~

| 屏幕 | 差值 /255 | 实际看到的明显差异 |
| --- | ---: | --- |
| FilePicker | 28.414 | 设计为一个灰底分组、行间分隔线及圆形图标底；实现为四个独立描边卡片、裸图标。页码条约在屏幕 y=803，设计约 y=1109，整体上移约 300 像素。实现顶部有额外外框。标题“选一张图片”和状态“可以选择”对应本次图片任务，设计“选一本书”和书籍准备状态不能拿来要求伪造数据。 |
| Main | 54.194 | 时钟、图标和 Dock 更大，应用区位置上移；续读胶囊为真实空态，没有设计中的书名和未读角标。 |
| Today | 48.401 | 卡片更高、文字更大，未读书/无内容的空态替代设计样例；底部卡片占用更多高度。 |
| Apps | 42.961 | 搜索栏和图标更大，新增 1/2 页码；设计底部三个应用被分页到后续页。 |
| Control | 39.888 | 两条滑杆变高，“我”替代“爸爸”；网络和通知显示空态；底部用“收起”按钮替代设计的短横条。 |
| Notifications | 24.323 | 只有一条合成通知，行高度与页码位置不同；设计的回复、回拨、读未读通知和全部清除没有对应外观，底部为“全部已读”。 |
| Settings | 81.373 | 灰色更深、分组更高、没有行间细分隔线和图标白色底圆；标题右侧搜索图标缺失。 |
| SetDisplay | 47.923 | 字号预览框更矮且位置下移；“刷新屏幕”和“自动锁屏”变成独立描边按钮，缺少设计的分组标题与说明层次。 |
| SetSound | 46.001 | 滑杆和分组更大；当前页只有输出选择和勿扰，按键提示、振动、铃声未在首屏展示，底部为 1/2。 |
| PowerMenu | 27.878 | 背景淡化、面板贴底且位置较低；取消按钮为白底描边，设计中的长按电源说明未出现。 |
| Confirm | 37.809 | 背景淡化、字号更大；收件人为合成 example.com，说明为“只有你确认后才会继续”，按钮为“确认”而非“确认发送”。 |

全部高于 12/255。FilePicker 的主要差异已由开发报告披露；其余十屏的绘制代码没有在本任务改动，记录为对照观察，不算本任务新增十项缺陷。现行规范已规定全套放大 1.2 倍、静态灰 #888888，且确认/电源浮层背景淡化；这些与旧画板的差异不能通过缩字、改灰、取消淡化来追分。本次未发现新增中文错字，SYNTH_* 是明确的合成文件名。

## 过度设计与架构审视

- 【实现】没有新增常驻任务、执行器、持久文件格式、配置、重试队列或通用框架。选择会话、编号数组、结果回调均为 F9 所需，数组与列表模型在 PSRAM；复用既有 BG/UI 执行器、F4 列表和成员通知。未发现需要删除的额外机制。
- 文件身份与权限仍归文件服务；选择器通过 index/lookup 取数据，没有直接改服务内部状态。prev/next/cancel/select 四项原子能力由按钮、实体键及 AI 共用；快照来自同一 F1/F4 声明，返回编号而非路径。ui_semantics.c 的页面身份修改集中在一处，没有逐调用方补丁。
- R1-01 应删除“成员代次相同就能回调”的错误前提，接已有调用页删除事件即可；不需要增加导航状态机、后台任务或给每个应用加存活检查。R1-02 只需改一个行高表达式。
- 没有混入 contacts、clock-alarm 或 files-app 的实现；共享 CMake 在末尾追加，既有调研及共享决策文档没有改动。

## R1-01 复建命令

以下命令从已配置的 build 读取原编译/链接参数，在标准输入中给测试调用方增加 destroy 标记及两条检查。只生成忽略目录内的对象与可执行程序，不写入任何产品/测试源文件。之后运行 R1-01 的模拟器命令即可。

~~~bash
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
import re, shlex, subprocess
root = Path.cwd()
b = root / "build"
s = (root / "tests/ui_file_picker.c").read_text()
s = s.replace("static unsigned callbacks, creates;",
    "static unsigned callbacks, creates;\nstatic bool caller_destroyed;\n"
    "static void caller_destroy(void) { caller_destroyed = true; }")
s = s.replace("(void)arg; creates++;",
    "(void)arg; caller_destroyed = false; creates++;")
s = s.replace(".snapshot = caller_snapshot};",
    ".snapshot = caller_snapshot, .destroy = caller_destroy};")
s = s.replace("selected = id; callbacks++;",
    'CHECK(!caller_destroyed, "REVIEW callback reached a destroyed caller");\n'
    "selected = id; callbacks++;")
probe = '''
static void home_probe(void) {
    CHECK(!strcmp(ink_ui_page_name(), "home"), "REVIEW home capability changes the activity");
    CHECK(strcmp(ink_ui_top_name(), "FilePicker"), "REVIEW home must not leave the departed caller picker on screen");
    CHECK(caller_destroyed, "REVIEW the original caller was destroyed");
}
'''
s = s.replace("static void run(void *arg) {", probe + "static void run(void *arg) {")
needle = 'on_ui(open_picker); wait_ready(); on_ui(page_one); snap("scene");'
assert s.count(needle) == 1
s = s.replace(needle, needle + '''
    CHECK(AI("system.home", NULL) && g_rc.status == INK_RC_DONE, "REVIEW request home through the real AI capability");
    quiet(); on_ui(home_probe);
    CHECK(BY_HAND("file_picker.cancel", NULL), "REVIEW close any remaining picker");
    quiet(); ink_probe_count("test.finished"); return;
''')
flags_text = (b / "CMakeFiles/sim-ui-file-picker.dir/flags.make").read_text()
flags = []
for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS"):
    flags += shlex.split(re.search("^" + key + r" = (.*)$", flags_text, re.M)[1])
cc = re.search(r"^CMAKE_C_COMPILER:FILEPATH=(.*)$",
               (b / "CMakeCache.txt").read_text(), re.M)[1]
obj, exe = str(b / "review-home.o"), str(b / "review-home")
subprocess.run(["taskpolicy", "-c", "utility", cc, *flags, "-I" + str(root / "tests"),
                "-x", "c", "-c", "-", "-o", obj], input=s, text=True, check=True)
link = shlex.split((b / "CMakeFiles/sim-ui-file-picker.dir/link.txt").read_text())
link = [obj if x == "CMakeFiles/sim-ui-file-picker.dir/tests/ui_file_picker.c.o"
        else exe if x == "sim-ui-file-picker" else x for x in link]
subprocess.run(["taskpolicy", "-c", "utility", *link], cwd=b, check=True)
PY
~~~

SEVERE=0 HIGH=1
