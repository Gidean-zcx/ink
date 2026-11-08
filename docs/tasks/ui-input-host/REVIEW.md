# ui-input-host 独立复查（第 1 轮／唯一一轮）

复查日期：2026-11-08～09（新加坡时间）。基线：`3e503e74daa9c8003bcd788073fe00e74c0def04`；被查提交：`c714f609`。

**结论：暂不通过。严重 0、高 1、中 1。** 固定键盘、位置恢复、成员拒绝和现有全量均可复现；但新增 URL 输入沿用的键盘缺少常用网址字符，现有测试直接注入键码掩盖了这个问题。另有短字段末尾及光标被裁切的问题。

本轮独立运行命令、读源码并查看截图。仓库仅写本 `REVIEW.md`，未改产品、测试或开发报告，未提交；构建／测试及临时探针产生的文件不纳入提交。没有访问设备、调用外部付费服务或推送。

## 发现

### R1｜高｜网址测试绕过实际键盘，用户无法键入测试所用的网址

- **位置**：[tests/ui_input_host.c:195](../../../tests/ui_input_host.c#L195)～198；[system/services/text/text_service.c:344](../../../system/services/text/text_service.c#L344)；依赖的按键表在 [system/ui/shell/keyboard.c:300](../../../system/ui/shell/keyboard.c#L300)～307。
- **触发**：在 URL 短框中用屏幕键盘输入 `example.com/path?x=1`。切到“符号”后没有 `=`；`_`、`%`、`#` 也都没有，无法输入常见路径、转义或锚点。不是竞态，也不需要异常数据。
- **复现命令**：运行下文“独立探针复现”的命令；它在原测试切到符号键盘后查找实际 LVGL 按键，并保存 `physical-keys-cursor-out/clean-url-symbols.png`。原验收命令 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ui-input-host$' --output-on-failure -j2` 仍会通过。
- **结果【实测】**：原 F2 用例 184 项通过；补查 4 个字符的按键全部失败，截图确认确实不存在。原测试逐字调用 `BY_HAND("keyboard.key", ...)`，能传入屏幕上不存在的键码，不能证明人能打出该网址。AI `text.insert` 可以写入，人与 AI 的实际可达输入范围因此不同。
- **范围／定级**：缺键源自既有键盘，但本次新增 URL 类型及网址验收首次依赖它；属于未闭合的接入问题。高的依据是正常网址输入受阻且当前测试测不到，不把全部旧键盘外观问题计入本任务。
- **最小修法**：在现有符号行补齐网址需要的字符，保持固定键盘矩形；把网址用例改为查找可见键并真实点击，不再注入不存在的键码。无需另建键盘、布局机制或输入会话。

### R2｜中｜40 字符的短网址会裁掉末尾和光标

- **位置**：[system/ui/textfield.c:63](../../../system/ui/textfield.c#L63)～78、[system/ui/textfield.c:331](../../../system/ui/textfield.c#L331)～332；新宿主复用该渲染方式。
- **触发**：给同一 URL 短框填入 `https://example.com/abcdefghijklmnop?x=1`（40 个 ASCII 字符，远小于测试字段的 128 字节上限），光标在末尾。
- **复现命令**：同下文探针；查看 `physical-keys-cursor-out/clean-url-long-cursor.png`。探针通过原文本服务写入，读取真实标签的光标位置及字段可见范围。
- **结果【实测】**：光标局部 x=606，标签起点 x=79，合计屏幕 x=685；字段内容右界只有 x=604。截图只显示中间的 `ample.com/abcdefghijklmnop…`，末尾 `?x=1` 和光标都被切掉，模型中的完整值仍在。字段外轮廓可见，不能据此认为插入位置也可见。
- **根因／范围【实现】**：`compose_line()` 固定保留光标前 30 个字符，未按标签的实际像素宽度计算；随后 `LV_LABEL_LONG_CLIP` 截断。计数裁窗是既有问题，本次加出的末尾 `|` 仍受它影响。没有证据表明发生数据丢失，按中记录，不冒称成员隔离或严重问题。
- **最小修法**：在 `compose_line()` 这一处按现有字体测量结果和标签宽度调整窗口起点，并为光标留宽；补一个普通长度网址的可见性断言。不要给各页面分别加补丁或引入滚动／分页框架。

## 逐项验收

| 项目 | 本轮结论 | 独立证据 |
| --- | --- | --- |
| 1 固定键盘、字段不被遮挡 | 给定测试页面通过 | 搜索／URL 的 JSON 矩形与 LVGL 一致；符号切换保持矩形。5 个原截图状态已人工查看；网址实际按键缺口见 R1。 |
| 2 焦点可见、完成回原位 | 基础场景通过；保留 R2 | 空框和短样例有轮廓及光标，完成／HOME 后字段、旁边标题坐标恢复；更长但仍属短字段的光标被裁。 |
| 3 人与 AI 的字段值、事件一致 | 部分通过，不能接受完整结论 | “合成查询”的人工粘贴与 AI 插入，值及 change/submit 事件记录相同；网址手工键入被 R1 阻断，原测试未覆盖实际按键可达性。 |
| 4 换人拒绝旧会话，去检查则失败 | 通过 | 原用例拒绝旧 `text.insert`／`text.submit`，字段及事件不变。仅撤成员检查后旧 insert 及无副作用断言失败，`test.fail=2`、`test.finished=1`。 |
| 5 全量、内存、250 行预算 | 本机通过 | 普通与 ASan／UBSan 各 207/207、零跳过；保守产品改动 153 行。未运行 Linux/gcc 或真机。 |

### 构建与全量命令

检查过两套 `CMakeCache.txt`：均 `INK_WERROR=ON`，ASan 目录另有 `INK_SANITIZE=ON`；依赖路径符合给定的 `$INK_DEPS`，复用既有配置。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】四条均退出 0。普通全量 96.83 秒；ASan／UBSan 全量 188.94 秒。包含确定性、分层、SDK 边界、F1 与 F2。两套 F2 均 `test.pass=184`、`test.fail=0`、`test.finished=1`、无警告，内部 RAM 总池最低余量 47,840 字节，原门槛 32,768 未变。完整测试日志为各目录的 `Testing/Temporary/LastTest.log`。

### 反向对照

【实测】只在内存中删除 `text_caps.c:19`～22 的成员检查，将源码从标准输入编译为临时对象，先于 `libink_system.a` 链接；原测试对象不变。编译／链接退出 0，模拟器退出 1（本次直接运行二进制，不是 CTest 的退出 8）。失败恰为 `tests/ui_input_host.c:161` 的旧成员 insert 被接受，以及 `:165` 的字段／事件有副作用。第二次 submit 此时仍会被变化后的 revision 拒绝，不能把这次实验说成“submit 单独也做了反例”；共用的成员检查本身确实被测试检出。

## 界面逐屏核查

原 `out/ui-compare/` 不存在。本轮用已重跑的 `build/test-out/ui-scene-*` 作为输入，给临时目录中同名场景建链接，运行：

```bash
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py \
  --build "$PWD/build" --design "$INK_DESIGN" --out "$INK_REVIEW" --reuse \
  --scenes Main Apps ImePinyin ImeCandidates ImeEdit ImePassword ImeVoice SetDisplay SetSound SetNetwork
```

【实测】工具退出 0；**逐一打开了以上 10 张设计／产品／差分三联图**，另看了 F2 的搜索前、搜索聚焦、搜索恢复、URL 聚焦、URL 恢复及两张探针图。以下是观察，不将旧页面占位内容误报为 F2 新增缺陷。主入口 `docs/project.md` 已说明设计稿尚未同步 1.2 倍放大、灰色加深和分页；这些不能仅因差分高就判回归。

| 屏幕 | 与设计稿明显不同之处 | 本轮归属 |
| --- | --- | --- |
| Main | 时钟更大、入口网格上移、灰胶囊更深；续读为真实空态；AI 图标不同 | 主题／已有页面，未由本次改动引入 |
| Apps | 图标与间距增大，一屏清单变分页，第一屏没有网页等最后一排 | 已有缩放及分页 |
| ImePinyin | 输入区更矮，键盘上沿更高；“大写／删除”用文字，少中英切换及独立标点键，完成无黑底主次区分 | 输入页仍是既有测试样式；本次改了区域高度定义，其余按键样式未改 |
| ImeCandidates | 稿中 5×3 候选变成少量候选和“更多／键盘”；没有稿中的顶部完成及底部页码条 | 旧候选及合成词库呈现；不在本次实现 |
| ImeEdit | 选区用方括号，没有黑底反白；工具条为“剪切”而非“全选”，缺编辑提示 | 旧字段渲染，未改这些语义 |
| ImePassword | “连接 Wi-Fi”变“网络密码”；缺网络名称、“显示”及隐私说明；多行大框代替单行密码框 | 旧测试页，本任务明确不做秘密宿主 |
| ImeVoice | “回复 妈妈”变“写备忘”；听写面板没有稿中的波形及临时稿边框；两底按钮顺序、主次不同 | 旧测试／听写面板；不声称已还原业务页 |
| SetDisplay | 刷新、自动锁屏变独立描边按钮，缺图标及副文案；预览区高度不同 | 已有页面 |
| SetSound | 当前项由勾改“当前”；部分设置分页，灰卡分组不同 | 已有页面及分页 |
| SetNetwork | 真实未连接状态代替示例网络列表；无设计稿中的 Wi-Fi 开关行，扫描用大按钮 | 已有页面及不同服务状态 |

F2 两个测试框的轮廓、原位恢复和键盘避让可见；它们不是 `Search`／`RemoteAddress` 业务页面的还原，任务也只要求测试页面。对照这两个设计稿，键盘仍有话筒靠左、三键底行、文字大写／删除、完成无黑底等旧差异。R1、R2 列的是可操作性问题，不是拿这些旧外观差异凑数。

## 规则、架构与过度设计

- **范围【实测】**：相对基线 12 个文件；产品新增 137、删除 10，连 CMake 6 行也计入共 153，低于 250。F1 实现／接口／测试、`system/policy`、文件及传输服务均无改动。没有改已有断言或资源门槛；新测试的启动／截图虚拟时间等待未替代状态断言，成员反例未靠卡住线程。
- **第 1、4 节【实现／实测】**：本次 diff 未引入硬件访问、密钥、付费调用、第三方资源或模拟器反向依赖；公开接口标明在 UI 任务使用，新增字段内存仍走外部池。分层与确定性检查通过。基线至 HEAD 的规定隐私扫描零匹配；`git diff --check` 退出 0。
- **软总线与 AI 原生【实现】**：布局／几何快照归 UI，编辑与成员校验复用文本服务，普通点击和 AI 聚焦共用 F1 声明能力，搜索粘贴／AI 插入结果同源。秘密字段被短宿主拒绝。没有新增另一套会话或服务数据写入口；R1 表明“有能力”还不能等同于人能通过界面完成。
- **过度设计结论【实现】**：未发现需要单列缺陷的新任务、状态机、格式、缓存、队列或配置。成员 epoch、原坐标及语义绑定对应 F2 要求；URL 类型避免拼音组合，也有明确任务来源。规模说明合理。本轮不建议删除必要成员检查，也不建议为两个问题扩展框架；最小改动就是修既有符号行和字段裁窗。
- **边界**：Linux/gcc、实机、长文／表单、既有业务页迁移未验收；开发报告已明示。10 屏差分高不代表这些业务页面由本任务实现，也不据此追加严重／高问题。

## 独立探针复现

本轮产物保留在 `$TMPDIR/ink-f2-review-6t2pai4y/`：`negative-member.log`、`negative-member-out/metrics.json`、`physical-keys-cursor.log`、`physical-keys-cursor-out/metrics.json`、对应 PNG 及 10 张三联图。若临时目录已清理，可在完成上述普通构建后用以下命令重建两个探针；源码仅在内存中改写，产物只写新临时目录。

```bash
export INK_REVIEW="$(mktemp -d "${TMPDIR:-/tmp}/ink-f2-review.XXXXXX")"
"$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
import json, os, shlex, subprocess
root = Path.cwd(); build = root / "build"; out = Path(os.environ["INK_REVIEW"])
flags = dict(line.split(" = ", 1) for line in
             (build / "CMakeFiles/sim-ui-input-host.dir/flags.make").read_text().splitlines()
             if " = " in line)
link = shlex.split((build / "CMakeFiles/sim-ui-input-host.dir/link.txt").read_text())
def check(cmd, **kw):
    result = subprocess.run(["taskpolicy", "-c", "utility", *cmd], **kw)
    assert result.returncode == 0, result.returncode

def probe(name, source, include, test=False):
    obj = out / (name + ".o"); exe = out / name
    opts = sum((shlex.split(flags[k]) for k in ("C_DEFINES", "C_INCLUDES", "C_FLAGS")), [])
    check([link[0], *opts, "-I" + str(root / include), "-x", "c", "-c", "-", "-o", str(obj)],
          input=source, text=True)
    cmd = link.copy(); cmd[cmd.index("-o") + 1] = str(exe)
    if test:
        cmd[cmd.index("CMakeFiles/sim-ui-input-host.dir/tests/ui_input_host.c.o")] = str(obj)
    else:
        cmd.insert(cmd.index("libink_port_sim.a"), str(obj))
    check(cmd, cwd=build)
    cmd = ["taskpolicy", "-c", "utility", str(exe),
           "--profile", str(root / "simulator/profiles/metalio-plus.json"),
           "--script", str(root / "tests/ui_input_host.script"),
           "--budget", str(root / "tests/ui_scene.budget.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--out", str(out / (name + "-out")), "--quiet", "--no-random-outages"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    (out / (name + ".log")).write_text(result.stdout + result.stderr)
    data = json.loads((out / (name + "-out/metrics.json")).read_text())
    print(name, "exit", result.returncode, "warnings", data["warnings"])
    print({k:v for k,v in data["metrics"].items() if k.startswith("test.")})

source = (root / "system/services/text/text_caps.c").read_text()
guard = ('    if (!ink_text_session_current()) {\n'
         '        fail(c, INK_RC_STALE, "输入已换人，请重新读取");\n'
         '        return false;\n    }\n')
assert source.count(guard) == 1
probe("negative-member", source.replace(guard, ""), "system/services/text")

source = (root / "tests/ui_input_host.c").read_text()
extra = r"""
static void verify_url_symbols(void) {
    CHECK(find_label(lv_layer_sys(), "=") != NULL, "URL keyboard missing equals");
    CHECK(find_label(lv_layer_sys(), "_") != NULL, "URL keyboard missing underscore");
    CHECK(find_label(lv_layer_sys(), "%") != NULL, "URL keyboard missing percent");
    CHECK(find_label(lv_layer_sys(), "#") != NULL, "URL keyboard missing hash");
}
static void verify_cursor(void) {
    ink_text_select(0, 128);
    ink_text_commit_utf8("https://example.com/abcdefghijklmnop?x=1");
    lv_obj_update_layout(field);
    lv_obj_t *label = lv_obj_get_child(field, 0);
    const char *line = lv_label_get_text(label);
    lv_point_t pt; lv_label_get_letter_pos(label, (uint32_t)strlen(line) - 1, &pt);
    lv_area_t content, area;
    lv_obj_get_content_coords(field, &content); lv_obj_get_coords(label, &area);
    CHECK(area.x1 + pt.x >= content.x1 && area.x1 + pt.x < content.x2,
          "cursor x=%d outside visible field %d..%d", area.x1 + pt.x, content.x1, content.x2);
}
"""
source = source.replace("static void run(void *arg) {", extra + "\nstatic void run(void *arg) {")
source = source.replace('"switch to symbols");\n    on_ui(inspect_focus);',
                       '"switch to symbols");\n    on_ui(verify_url_symbols); capture("url-symbols"); on_ui(inspect_focus);')
source = source.replace('on_ui(ai_focus); on_ui(inspect_focus); on_ui(handoff);',
                       'on_ui(ai_focus); on_ui(verify_cursor); capture("url-long-cursor"); on_ui(inspect_focus); on_ui(handoff);')
probe("physical-keys-cursor", source, "tests", test=True)
PY
```

预期：编译／链接均成功；两个探针的模拟器退出码均为 1。成员反例 2 项失败；按键／光标探针 5 项失败、`test.finished=1`。这两个非零结果是复查证据，不冒充原全量测试失败。

SEVERE=0 HIGH=1
