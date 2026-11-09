# clock-alarm 第 1 轮复查

日期：2026-11-09。基线：`d0cacd1d4b348a8e885904026f3796a10996c646`。复查提交：`494ca36115e96db3ad6afe6944461ae20e4da2ea`。

## 结论

本轮发现 3 项中级、1 项低级问题，没有严重或高级问题。**不能按全部验收通过交付**：编辑、开关仍未实现；普通及 ASan／UBSan 全量均为 217/218，仅 `capability-table-doc` 失败。开发报告如实披露了这两项，未发现假报全通过。

只修改本报告，不修改产品、测试或共享文档，不提交。补充探针和反向变体通过标准输入编译成独立对象／可执行文件，正常可执行文件未替换；产物最终全部位于忽略的构建目录。命令使用 `taskpolicy -c utility`，构建和全量测试串行进行，并发不超过 2。

## 验收核对

| 项目 | 结论 | 本轮独立证据／命令 |
| --- | --- | --- |
| 1 新建、编辑、删除、开关与重启 | 部分完成 | 两种构建中的 `clock-alarm` 均通过：创建 166、重启 24 项断言；补充探针确认编辑／开关缺失及长名称保存失败。见 CA-R1-01、02 和下方命令 A、B |
| 2 重复规则显示和保存 | 已实现部分符合，显示验证不足 | 原测试覆盖每天 127、工作日 62、自选 65 及重启读取；真实列表截图显示正确。清空可见文字后测试仍通过，见 CA-R1-04、命令 C |
| 3 响铃操作与 AI 快照 | 已测场景通过 | 实际响铃、触摸再睡 10 分钟、再次响铃、AI 关掉均通过；撤掉输入快照修复后出现预期 4 项失败，见命令 D |
| 4 全量、内存、300 行预算 | 未全部通过 | 普通／ASan 各 217/218；无 Sanitizer 错误。产品 220 + boot 3 + CMake 5 = 228 行；字号检查 1 文件、0 违规。见命令 A |

## 问题

### CA-R1-01｜中｜编辑和开关没有实现，验收 1 不完整

- 位置：`system/apps/clock/clock.c:78`、`:121`、`:124`、`:181`。
- 复现命令：下方命令 B；或在真实列表点击时间或开关。
- 【实测】开关区域点击后进入 `ClockAlarm`；服务记录仍 `enabled=true`。详情只暴露 `alarm.cancel`，没有修改时间、重复、名称或启停的操作。
- 【实现】时间服务确实没有 update／enable 接口。任务边界允许把服务缺口写进报告，开发者也没有把此项写成通过，因此这是验收缺口，不判为虚报或高级缺陷。
- 最小处理：保持“部分完成”，补齐时间服务前置任务后再验收；后续在已有服务内补一个按原 ID 更新的原子入口供编辑、启停共用，再连接当前详情与列表。保留原持久格式，不用删除重建替代，也不要求本轮越过只写复查报告的边界。

### CA-R1-02｜中｜输入上限与服务不一致，保存失败没有可见提示

- 位置：`system/apps/clock/clock.c:63`、`:138`、`:195`；服务合同 `system/services/time/time_caps.c:288`。
- 复现命令：下方命令 B。操作为新建闹钟，名称填 `1234567890123456`，完成输入后点“保存闹钟”。
- 【实测】输入被完整接受；点击后页面仍为 `ClockAlarm`，闹钟数为 0，画面没有错误提示。随后直接调用同一路径 `clock.save` 返回 `invalid`，详情为 `title is longer than 15 characters`。改为 15 个汉字后返回 `done`，列表有 1 条。证据：`build/clock-review-r1/probe2.log`、`probe2/snap-save-16ascii.png`。
- 【实现】表单 `.max` 是 48 **字节**，能力 `maxLength` 是 15 **字符**；16 个汉字还会被 `name_changed` 的 48 字节目标截断到半个 UTF-8 字符。触摸回调丢弃失败回执，用户无法知道原因。
- 最小修法：在表单保存处统一按现有能力的 15 字符约束验证并显示中文提示；字节上限须预留结尾，不截断 UTF-8。复用现有输入和提示组件，不放宽服务限制。

### CA-R1-03｜低｜时间减号没有字形，显示为方框

- 位置：`system/apps/clock/clock.c:129`。
- 复现命令：下方命令 B；查看 `build/clock-review-r1/probe/snap-new.png`，或打开时钟，点“加闹钟”。
- 【实测】“时−”“分−”中的 U+2212 显示为方框，无法直观看出减一的含义。现有 `build/assets/fonts/ui-charset.txt` 和阅读字体 `ascii,punct,pinyin,gb2312` 字符集均不含 U+2212，而含 ASCII `-`。LVGL 的通用字形查询可以返回占位字形，不能用其布尔返回值证明有正确字形。
- 最小修法：两个按钮使用已有字库支持的 ASCII `-`，无需修改字体管线或增添资源。

### CA-R1-04｜中｜列表显示断言实际只检查快照，清空屏幕文字也会通过

- 位置：`tests/clock_alarm.c:61`、`:79`；被验证的显示代码 `system/apps/clock/clock.c:83`。
- 复现命令：下方命令 C。在独立编译对象中仅将第 83 行两次 `lv_label_set_text` 的文字改为 `""`；快照、数据、测试断言、预算保持原样。
- 【实测】变体创建进程退出 0，166 项通过；同一模拟闪存重启退出 0，24 项通过。截图 `build/clock-review-r1/blank-list/create/snap-scene.png` 中三张卡片只剩开关，时间、名称和重复文字全部消失，测试仍称显示正确。当前产品没有被改成空白；这是对测试强度的反向验证。
- 原因：`verify_list` 只在 `snapshot` 中找字符串；`ink_ui_snapshot` 与 `ink_ui_context_snapshot` 的相等性也不能证明屏幕已经画出相应文字。原测试没有通过触摸列表行来打开详情。
- 最小修法：在现有测试中核对可见列表标签的时间／名称／重复内容，并至少触摸一行进入详情；保留快照和持久记录断言。不新增截图框架。按复查提示的“检查工具覆盖缺口”上限定为中。

## 验证记录

- `build/clock-review-r1/probe2.log`：独立探针退出 0，60 项检查通过；15 字汉字名称可以保存；真实点击时间能进入详情；点击开关不会关闭闹钟；成人存在闹钟及未保存草稿时切到孩子，孩子列表为 0，新草稿名称为空。
- `build/clock-review-r1-ctest.log`：全量 218 项，无跳过，159.01 秒，退出 8。
- `build-asan/clock-review-r1-ctest.log`：全量 218 项，无跳过，286.71 秒，退出 8；唯一失败仍为共享能力目录。核对产品与场景测试编译参数都有 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`，运行设置 `UBSAN_OPTIONS=halt_on_error=1`，未发现 Sanitizer 错误。
- 两种构建的原 `clock-alarm` 均为创建 166／重启 24 项通过、失败 0、完成标记 1；内部 RAM 最低余量分别 48,192／47,840 字节，高于 32,768；内外部内存分配失败、警告、预算违规均为 0。
- 反向修复核查：`no-input-fix/create/metrics.json` 为退出 1、162 通过／4 失败／完成 1，四次失败都定位 `tests/clock_alarm.c:32`。开发报告所述快照修复有有效反向证据。
- `git diff --check <基线>` 通过；九个变更文件均与任务有关；隐私扫描无本机路径／MAC 匹配。没有发现新增常驻线程、预算放宽或删除测试。
- 已有测试的数量 6→7 和 AI 白名单追加七项符合新增应用／能力，原断言保留。测试里的虚拟时间等待用于等实际状态和采样，没有发现用 sleep 掩盖产品故障。
- `capability-table-doc` 的实际差异是新增七项 `clock.*` 能力，目录计数 197→204；共享文档受总规则第 7 节限制未更新。由合并方生成目录再跑检查即可，本轮不改它，不重复列为产品缺陷。
- 证据范围为本机 macOS 模拟器；没有运行 Linux／gcc、实机或外部服务。没有改设备、调用付费接口、使用真实身份数据或对外发布。

## 10 屏视觉检查

已逐张打开 10 屏对照图，并重新运行 `tools/ui_compare.py --reuse` 核对分数。Clock／ClockRing 使用开发留存的已完成场景图，其余来自已有场景输出；本轮全量另独立运行对应测试。对照图生成并查看于 `out/ui-compare/`，随后将本轮生成目录移到 `build/clock-review-r1/ui-compare/`，避免留下未跟踪产物。数字仅说明差异，不把新主题规定的字号、深灰和测试数据差异判为本次回归。

| 屏 | MAE / 255 | 可见差异 |
| --- | ---: | --- |
| Clock | 43.297 | 去掉任务排除的计时入口；没有“明早”和倒计时；卡片和数字更大，加入页码；开关实际不能操作，见 CA-R1-01 |
| ClockRing | 35.467 | 旧呈现器未画状态栏；图标更大，时间字号较小；测试时间／日期不同 |
| Main | 54.194 | 时钟、图标更大，位置上移／下移；灰更深；续读为真实空状态 |
| Apps | 42.977 | 分页显示；工具区时钟使用通用四格图标，设计为表盘；深灰和间距变化 |
| Control | 39.888 | 控件更高；网络、通知为空状态；底部“收起”按钮代替横线 |
| ImePinyin | 39.031 | 输入框更矮；候选数量与布局不同；大写、删除、空格使用文字按钮 |
| ImeEdit | 36.919 | 选区显示成方括号，设计为反白；没有顶部完成和编辑说明；工具栏不同 |
| Settings | 81.373 | 深灰分组整体更高；缺右上搜索；图标没有独立浅底圆；成员／备份是测试实际状态 |
| SetDisplay | 47.923 | 字号预览更矮、灰更深；刷新与锁屏改为整行胶囊，缺设计的小字说明 |
| Members | 24.948 | 测试只有两人；头像与标题位置不同，没有画设计中的三个身份及锁标记 |

另查看新建表单截图，小时／分钟的减号显示为方框，见 CA-R1-03。

## 品味和规模

本次没有发现需要新增机制才能处理的问题。列表、详情、表单、七个页面能力属于任务接线；复用 F1／F2／F4 和既有时间服务，没有新线程、持久格式、重试队列或缓存。228 行包含测试 CMake 接线，仍低于 300；产品代码没有通过大块删改或放宽预算掩盖规模。

没有必须删除的新机制。CA-R1-03 只需换两个字符；CA-R1-02 在现有表单集中处理输入边界和错误显示即可，不应改通用输入框、每个调用方或增加通用校验框架。

## 架构与 AI 原生审视

页面独立放在 `system/apps/clock/clock.c`，通过既有能力写入时间服务，没有直接修改服务内部状态；新增能力默认使用 UI 执行器，状态监听也指定 UI，未发现非 UI 线程调用 LVGL。名称走 F2，列表走 F4，语义快照走 F1；已实现的按钮和 AI 使用同一能力。编辑、启停两端都缺失，见 CA-R1-01。成员切换补充用例未发现数据或草稿串入；这不替代完整安全审计。

## 复现命令

以下均在仓库根目录运行，`INK_DEPS`、`INK_DESIGN` 使用本轮用户给定的环境值。已有 `build`／`build-asan` 的缓存路径已核对，字体、Python、LVGL 和服务器测试 Python 均指向指定依赖检出；未重新下载或改依赖。

**A：本轮全量与字号检查。** 两次 CTest 的预期退出码均为 8，不能把它们写成通过。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_scale_check.py system/apps/clock/clock.c
```

**B：重跑本轮保留的补充探针。** 该二进制用 `sim-clock-alarm` 的原编译／链接参数构建，替换测试驱动函数，产品库完全不变。驱动依次执行 16 字符名称保存失败、改成 15 汉字保存成功、触摸时间／开关、带成人草稿切到孩子。退出 0 表示这些观察被复现，不表示产品缺陷已修好。探针源未写入仓库；删除构建产物后，可按 CA-R1-01～03 的逐步操作复现。

```bash
taskpolicy -c utility build/clock-review-r1/sim-probe2 \
  --profile simulator/profiles/metalio-plus.json \
  --assets build/assets --content build/test-content \
  --out "$(mktemp -d "$PWD/build/clock-review-r1/replay-XXXXXX")" \
  --end 120s --no-random-outages --budget tests/ui_scene.budget.json
```

**C／D：重新生成并运行反向变体，不改任何源文件。** 以下默认 C（清空列表标签）；把第一行的 `blank-list` 换成 `no-input-fix` 即 D（撤掉快照修复）。C 应打印创建／重启退出 0、通过 166／24；D 应打印退出 1、通过 162、失败 4。此代码对应本轮已执行的变换，独立输出目录避免读取旧模拟闪存。

```bash
INK_REVIEW_VARIANT=blank-list taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
import json, os, shlex, subprocess, tempfile
r = Path.cwd()
b = r / 'build'
variant = os.environ['INK_REVIEW_VARIANT']
w = Path(tempfile.mkdtemp(prefix='clock-review-repro-', dir=b))
flags = {}
for line in (b / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
source = (r / 'system/apps/clock/clock.c').read_text()
if variant == 'blank-list':
    old = 'lv_label_set_text(lv_obj_get_child(slot, 1), time); lv_label_set_text(lv_obj_get_child(slot, 2), note);'
    new = 'lv_label_set_text(lv_obj_get_child(slot, 1), ""); lv_label_set_text(lv_obj_get_child(slot, 2), "");'
else:
    assert variant == 'no-input-fix'
    old = 'if (C.name && ink_text_focused()) EDIT.action_count = 0;'
    new = 'if (C.name && ink_text_focused()) EDIT.action_count = actions;'
assert source.count(old) == 1
link = shlex.split((b / 'CMakeFiles/sim-clock-alarm.dir/link.txt').read_text())
obj, exe = w / 'clock.o', w / 'sim-clock-alarm'
subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                '-x', 'c', '-c', '-', '-o', str(obj)], input=source.replace(old, new),
               text=True, cwd=b, check=True)
link.insert(next(i for i, value in enumerate(link) if value.endswith('.a')), str(obj))
link[link.index('-o') + 1] = str(exe)
subprocess.run(link, cwd=b, check=True)
(w / 'sd').mkdir()
for phase in (('create', 'restart') if variant == 'blank-list' else ('create',)):
    out = w / phase
    result = subprocess.run([str(exe), '--profile', str(r / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(b / 'assets'), '--content', str(b / 'test-content'), '--out', str(out),
        '--end', '120s', '--flash-image', str(w / 'user.img'), '--sd-dir', str(w / 'sd'),
        '--quiet', '--no-random-outages', '--budget', str(r / 'tests/ui_scene.budget.json')],
        env={**os.environ, 'INK_CLOCK_PHASE': phase}, capture_output=True, timeout=120)
    (w / (phase + '.log')).write_bytes(result.stdout + result.stderr)
    j = json.loads((out / 'metrics.json').read_text())
    print(variant, phase, 'exit=', result.returncode,
          {k: v for k, v in j['metrics'].items() if k.startswith('test.')}, out)
    print('\n'.join(j['warnings']))
PY
```

10 屏对照重算命令（图已移入构建目录，`--reuse` 不启动额外模拟器）：

```bash
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py \
  --build build --design "$INK_DESIGN" --out build/clock-review-r1/ui-compare --reuse \
  --scenes Clock ClockRing Main Apps Control ImePinyin ImeEdit Settings SetDisplay Members
```

SEVERE=0 HIGH=0
