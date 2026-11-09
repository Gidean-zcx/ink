# confirm-pages 第 1 轮复查

复查日期：2026-11-09。基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`；受审提交：`38bcfda3d99456747624d2a009bef7724cdfabcc`。

结论：**尚未满足全部验收。发现 3 项中级问题，未发现严重或高级问题。** 普通与内存全量均有一项已披露的失败；队列测试对执行结果的断言不足。确认来源检查和密码验证的既有保护未被放松。

本轮只修改本文件，不修改受审产品或测试文件，不提交。运行命令产生的二进制、日志和图片留在忽略的构建目录。

## 已核对

- 已读取复查提示、任务书、总规则和开发报告；基线差异为确认卡、CMake 末尾注册、四个新增测试文件和开发报告。
- 独立复现了开发报告披露的 `capability-table-doc` 失败、三项环境依赖跳过及六张画板的视觉分数，没有将这些已披露的限制算成虚假通过。
- `out/ui-compare/` 不存在；重新生成并实际查看构建目录内 10 屏设计/产品/差分对照图，详见视觉核查。

## 验收复核

| 验收项 | 复查结论 | 独立证据 |
| --- | --- | --- |
| 1 各类出现、同意、拒绝、过期 | 指定基础路径通过；页面收尾有缺口 | 普通和 ASan 专项各 500 条断言通过。家长错误密码不执行，HOME 拒绝有效，各类真实计时过期，旧 token 不能执行。额外复现 CP-R1-01、02；过期结果页本身只关闭，不是新的待同意事务。 |
| 2 连续确认排队 | FIFO/容量通过；执行结果验证不足 | 4 项顺序与第 5 项拒绝可复现；将队列第二项的确认改为取消，137 条断言仍全通过，见 CP-R1-03。 |
| 3 AI 不能替人回答及反例 | 通过 | 原版拒绝 AI/system/app/remote/confirmed 回答；仅移除人类来源检查，ConfirmSetting 失败 16 条，退出 1。 |
| 4 每类有 AI 快照 | 六类基础快照通过；跨成员生命周期不通过 | 六类均有身份、编号、阅读和队列信息，密码不进快照；过期后换成员会留下空快照，见 CP-R1-01。 |
| 5 全量、内存、300 行预算 | 未全通过；预算通过 | 两种构建退出 0；两轮全量各 229 通过、3 跳过、1 失败；产品新增 137 行，含 CMake 注册保守计 142 行。 |

【实测】普通全量耗时 **282.01 秒**，ASan/UBSan 全量 **266.54 秒**，均为 233 项、退出 8。唯一失败是生成能力目录未同步：确认卡新增了 `confirm.pending`、`interrupt.active` 状态依赖和 `confirm.result` 事件依赖。总规则第 7 节禁止本任务修改该共享文档，仍须由合并会话同步后补过此验收。三项跳过是 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，没有改跳过条件。

【实测】核对缓存和编译参数：两种构建均 `INK_WERROR=ON`，内存构建 `INK_SANITIZE=ON` 且含 `-fsanitize=address,undefined`。本轮两份 `Testing/Temporary/LastTest.log` 未出现 ASan/UBSan 错误；六场景内部 RAM 最低余量均为 **47,840 字节**，预算未改。

【实测】仅换回基线确认卡，ConfirmWeb 失败 8 条、退出 1；并非只是新增一个永远通过的测试。以下命令由本复查直接执行，依赖路径沿用用户给定环境，构建/全量均串行且最多 `-j2`：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/cmake" --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/cmake" --build build-asan -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/confirm_pages/run.py --build build --mutant ai-source --scene ConfirmSetting
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/confirm_pages/run.py --build build --baseline 3fce2beea07d9e25a814cd24c64e8d5305ea9222 --scene ConfirmWeb
```

## 问题

### CP-R1-01｜中｜换家人后保留上一人的过期卡，当前成员的 AI 快照为空

- 文件：`system/ui/shell/confirm_card.c:265`、`:326`、`:349`；通知路径见 `system/policy/confirm.c:511`、`:537`。
- 【实测】设好大人密码，等网页确认真实过期；按电源锁屏，经过 121 秒虚拟时间后唤醒，在成员页触摸选择孩子。所有前置断言通过，随后 `top=ConfirmExpired`，孩子快照为空，两条断言失败，进程退出 1。没有卡线程或伪造事件。
- 复现命令：运行文末复现脚本的 `expired` 场景。它只替换测试入口，链接当前产品库；证据为 `build/test-out/confirm-pages-review-probe/expired/metrics.json` 和 `snap-expired-after-child.png`。
- 结果：`hidden()` 只在 `show(NULL)` 或收到结果时检查；队列已经为空后，确认服务在成员/电源变化时不会再调用 presenter。`ink_ui_snapshot()` 的既有归属检查挡住了旧内容，所以不是孩子读取大人内容；但错误浮层持续截获输入，孩子必须额外关闭才能恢复页面及 AI 上下文。
- 最小修法：在确认卡这一处接入已有状态变化通知，归属失效、锁屏或被打断时复用 `hidden()` 和 `close_card()` 收起过期卡。不新增任务、轮询或队列，不改确认规则。

### CP-R1-02｜中｜家长密码页缺少画板中的触摸取消入口

- 文件：`system/ui/shell/confirm_card.c:94`；复用控件 `system/ui/shell/pinpad.c:36`；测试 `tests/confirm_pages/pages.c:180`。
- 【实测/实现】进入 `ConfirmParent` 密码页，底行只有“删除 / 0 / 确定”，画板左下的“取消”消失；键盘捕获整个页面的触摸，左缘返回也不能取消。HOME 可以取消，已有拒绝测试只测了这个键。原密码控件已有此限制，本任务补做家长页面后仍未补齐画板交互，不将其描述成新引入的策略漏洞。
- 复现命令：运行文末复现脚本的 `parent` 场景；进入密码页后左缘返回，事务仍等待，退出 1；随后 HOME 拒绝成功。亦可直接查看 `build/test-out/confirm-pages-current/ConfirmParent.png`。
- 最小修法：在 `parent_page()` 保留一个可见、可触摸的取消控件，复用现有拒绝路径；保持密码长度、删除键及验证逻辑不变。

### CP-R1-03｜中｜队列测试不验证同意、拒绝及实际执行对象

- 文件：`tests/confirm_pages/pages.c:135`、`:137`、`:23`；对应报告 `docs/tasks/confirm-pages/REPORT.md:10`。
- 【实测】仅在内存中的测试副本把 `if (i == 1) { read_all(tokens[i]); press("确认"); }` 改为按“取消”，当前产品不变：StressConfirm **137 条断言仍全通过、退出 0**，实际累计 `confirm.accepted=1`、`confirm.rejected=5`，队列四项全被拒绝。唯一一次同意来自前面的单事务测试。
- 复现命令：运行文末脚本的 `queue-reject` 场景，证据为 `build/test-out/confirm-pages-review-probe/queue-reject/metrics.json`。
- 结果：队列部分只检查队首 token、数量和回执 token；没有检查 decision/status、执行次数或执行参数。不能据此声称“第二项同意执行，其余拒绝”，也测不到同意了错误对象。此处属于检查覆盖缺口，按复查提示上限定为中，不据此宣称当前产品已经错执行。
- 最小修法：在现有夹具记录执行收到的 `n`，逐项核对回执 decision，最后断言只有 `n=11` 执行一次；不用新增测试框架。

## 视觉核查（10 屏）

【实测】逐张打开六张任务对照图，以及三张共用确认卡和主页对照图。前六张位于 `build/test-out/confirm-pages-current/`，后四张位于 `build/test-out/confirm-pages-review-visual/`。均对照 `$INK_DESIGN/png/` 原图。

| 页面 | 1/4 灰度差 /255 | 实际可见的差异与判断 |
| --- | ---: | --- |
| ConfirmWeb | 39.942 | 灰色内容块更高，域名和订单金额同用正文大小，域名换行；原稿域名是次要文字。支付按钮清楚，底部没有裁切。 |
| ConfirmParent | 31.066 | 数字键大幅增大、面板上移；旧稿四个空心密码位改为输入提示及黑点。“取消”缺失见 CP-R1-02。 |
| ConfirmCall | 35.202 | 原稿的来源行、联系人说明合为号码正文；实际服务只给号码，没有伪造联系人。标题、取消和拨打按钮可辨认。 |
| ConfirmSetting | 35.631 | 原稿的设置名称、前后值、家人身份变为通用标题和“把设置 test.confirm_page 改成 1”。仍有技术编号/数值暴露给用户，不能视为原稿的信息层次已做完；报告已列服务数据缺口。 |
| ConfirmExpired | 36.402 | 原稿“关闭 / 再来一次”变为一个黑色“关闭”；文字改为请重新发起。服务无重发接口，开发报告已明确没做，未把空按钮当实现。 |
| StressConfirm | 43.802 | 原稿收件人副标题与长短信变为合成说明；灰块外单独放页码，3 页而非原图 2 页。确认前必须翻完；截图并未展示多事务队列状态。 |
| Confirm | 37.809 | 通用“确认”替代“确认发送”；收件人与正文合排，没有原稿副标题层次；沿用基线通用卡与测试夹具。 |
| ConfirmDelete | 40.930 | 仍用 AI 星形图标，原稿是垃圾桶；文件信息和不可恢复提示合入正文。属于基线已有差异。 |
| ConfirmSecret | 42.954 | 标题现在换行；验证码、域名及解释合入灰块，仍用通用“确认”；原稿分层展示并写“确认填写”。 |
| Main | 54.194 | 字号、图标和灰块更大，空书架取代示例阅读进度；用于核对浮层底图，不是本次改动。 |

尺寸、灰度和背景淡化依规范第 1 节及当前主题，不能为了接近旧画板而缩回 1.2 倍之前的数值或浅灰。10 屏未见新增明显错字；服务信息不足、通用正文层次及上述取消入口是与纯尺寸差异不同的问题。

复现对照图：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --reuse --design "$INK_DESIGN" --out build/test-out/confirm-pages-current --scenes ConfirmWeb ConfirmParent ConfirmCall ConfirmSetting ConfirmExpired StressConfirm
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --design "$INK_DESIGN" --out build/test-out/confirm-pages-review-visual --scenes Confirm ConfirmDelete ConfirmSecret Main
```

另尝试过 `Pin`，该对照命令使用的短场景未产出 `test.finished`，工具明确退出 1；未把它计入上述 10 屏，也未据此判产品失败。全量中的正常 Pin 测试通过。

## 品味、规模和架构审视

【实现】产品文件新增 137 行、删除 18 行；连同 CMake 末尾 5 行保守计 142 行，未超 300 行。新增过期显示状态和结果订阅服务于指定的 `ConfirmExpired`，没有新队列、持久格式、常驻任务、重试或框架。暂未发现应整套删除的新机制。

【实现】本轮没有为减少规模而必须删除的机制。`parent_page()` 按子节点序号改密码控件较脆弱，当前顺序与现有控件一致，不另报假想故障，也不要求为此重构。

【实现】策略、排队、参数/内容版本绑定、落盘仍由确认服务负责；新增 LVGL 操作在 UI 执行器。用户确认/密码刻意不向 AI 开放，属于要求内的例外；普通确认继续复用服务接口，过期关闭复用既有 `confirm.page`。主要边界问题是 CP-R1-01：页面新增的存续期长于服务队列，却没接齐失效通知。

## 规则和证据边界

- 【实测/实现】基线差异只涉及任务允许的文件，CMake 只在末尾追加；没有改现有测试、预算或跳过条件，未见用新增 sleep 掩盖产品时序问题。复查探针的 121 秒是验证两分钟锁屏期限所需的虚拟时间，不阻塞宿主线程。
- 【实测】`git diff --check` 通过；受审提交差异按总规则隐私表达式检查零匹配。未接触设备、真实凭据、付费接口或远端写入。
- 无大人密码时家长确认会退为普通确认，是基线 `confirm.c:317` 的规则；开发报告已明确披露，本轮未改，不重复计为本次引入的缺陷。
- 本机 macOS clang 结果不等于 Linux 或实机验证；这两项未执行。构建目录证据可能被后续测试覆盖，下面的探针可以从当前源码重建。

## 附：三项问题的可重建复现

在仓库根目录执行下面命令。脚本从本报告读取下一段 Python，只在 `build/test-out/confirm-pages-review-probe/` 生成对象、程序和测试证据；不改产品源文件或受审测试。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
report = Path('docs/tasks/confirm-pages/REVIEW.md').read_text()
script = report.split('<!-- repro-script -->', 1)[1].split('```python\n', 1)[1].split('\n```', 1)[0]
exec(compile(script, 'confirm-pages-review', 'exec'))
PY
```

<!-- repro-script -->
```python
from pathlib import Path
import json, os, shlex, subprocess
root = Path.cwd()
build = root / 'build'
area = build / 'test-out/confirm-pages-review-probe'
area.mkdir(parents=True, exist_ok=True)
original = (root / 'tests/confirm_pages/pages.c').read_text()
probe = r'''
static void review_frame(void *arg) { sim_display_snapshot(arg); }
static void review_capture(const char *name) {
    view(); vtk_host_call(save_snapshot, NULL);
    vtk_event_at(vtk_now() + SIM_MS(1), review_frame, (void *)name); delay_ms(5);
}
static void run(void *arg) {
    (void)arg; delay_ms(2000); test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    CHECK(ink_cap_register(FIXTURES, 2) == 0 && ink_settings_define(&SETTING, 1) == 0, "fixtures");
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "results");
    CHECK(ink_member_set_pin("owner", NULL, "1234", pin_told, NULL) == 0 && WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK, "owner PIN");
    if (!strcmp(scene, "ConfirmExpired")) {
        ink_hub_demand();
        CHECK(WAIT_FOR(ink_hub_online() && ink_hub_member_matches("owner", ink_hub_member_epoch(), ink_hub_connection()), 30000), "hub");
        uint32_t token = ask(0);
        CHECK(WAIT_FOR(result_token == token, 62000) && !strcmp(decision, "expired"), "deadline");
        review_capture("expired-before-lock");
        key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 5000), "lock");
        delay_ms(121000); key(HAL_KEY_POWER); view();
        CHECK(!strcmp(g_top, "member_gate"), "wake member gate: %s", g_top);
        press("小"); view();
        CHECK(!strcmp(ink_member_current()->id, "kid"), "touch switched to child");
        CHECK(strcmp(g_top, "ConfirmExpired"), "old expired card must close for child: top=%s", g_top);
        CHECK(g_snap[0], "child snapshot is empty");
        review_capture("expired-after-child");
    } else {
        uint32_t token = ask(0); read_all(token); press("输入密码");
        review_capture("parent");
        hw(HW_PRESS, 1, 600); delay_ms(100);
        hw(HW_PRESS, 260, 600); delay_ms(100);
        hw(HW_RELEASE, 260, 600); quiet();
        CHECK(!waiting(token), "left-edge return should reject parent confirmation");
        key(HAL_KEY_HOME); quiet();
        CHECK(result_token == token && !strcmp(decision, "rejected"), "HOME rejects");
    }
    ink_probe_count("test.finished");
}
'''
start = original.index('static void run(void *arg) {')
end = original.index('void ink_main(void)', start)
flags = {}
for line in (build / 'CMakeFiles/sim-confirm-pages.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
base_link = shlex.split((build / 'CMakeFiles/sim-confirm-pages.dir/link.txt').read_text())
for tag, scene in [('expired', 'ConfirmExpired'), ('parent', 'ConfirmParent'), ('queue-reject', 'StressConfirm')]:
    source = original[:start] + probe + original[end:]
    if tag == 'queue-reject':
        old = 'if (i == 1) { read_all(tokens[i]); press("确认"); }'
        assert original.count(old) == 1
        source = original.replace(old, 'if (i == 1) { read_all(tokens[i]); press("取消"); }')
    obj, binary = area / (tag + '.o'), area / ('sim-' + tag)
    subprocess.run([base_link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I' + str(root / 'tests/confirm_pages'), '-x', 'c', '-c', '-', '-o', str(obj)],
                   input=source, text=True, cwd=build, check=True)
    link = list(base_link)
    link[next(i for i, part in enumerate(link) if part.endswith('/pages.c.o'))] = str(obj)
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    out = area / tag
    result = subprocess.run([str(binary), '--profile', 'simulator/profiles/metalio-plus.json',
        '--script', 'tests/confirm_pages/pages.script', '--budget', 'tests/ui_scene.budget.json',
        '--hub', 'tests/ui_shell.hub.json', '--assets', 'build/assets', '--content', 'build/test-content',
        '--out', str(out), '--quiet', '--no-random-outages'],
        env={**os.environ, 'INK_CONFIRM_SCENE': scene}, capture_output=True, text=True, timeout=90)
    (out / 'stdout.log').write_text(result.stdout)
    (out / 'stderr.log').write_text(result.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    m = data['metrics']
    print(tag, 'exit=', result.returncode, 'pass=', m.get('test.pass', 0), 'fail=', m.get('test.fail', 0),
          'accepted=', m.get('confirm.accepted', 0), 'rejected=', m.get('confirm.rejected', 0))
    print('\n'.join(data.get('warnings', [])))
```

SEVERE=0 HIGH=0
