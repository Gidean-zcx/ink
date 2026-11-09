# contacts 第 1 轮复查

日期：2026-11-09。基线：`d0cacd1d4b348a8e885904026f3796a10996c646`；待审提交：`22a0fc0d7b26836c6744baec0fd5895307f32297`。

**结论：暂不通过。发现高 2 项、中 1 项。** 常规 CRUD、重启、成员隔离、分页和选择能力实测通过，但编辑页与 AI 交错修改会丢失更新，删除确认也未绑定记录版本。两条高问题均为正常前后台交互的时序，无需卡住线程或故障注入。

本轮仅修改本报告，不改产品、原测试、预算或共享文档，不提交。构建／测试产生忽略目录内的产物；额外探针从标准输入编译到临时目录，产品源码原字节保留。以下结论区分【实测】与【实现】，不代表 Linux 或实机结果。

## 验收表

| 项 | 复查结论 | 本轮独立证据 |
| --- | --- | --- |
| 1：CRUD、重启、成员隔离及反例 | 部分通过 | 【实测】原联系人测试写入 117 项、独立重启进程 21 项断言通过；成人／孩子同编号各读自己的号码。编译输入中撤掉成员绑定后，4 项断言失败。编辑覆盖及过期删除确认见 C1-01、C1-02。 |
| 2：空页、一页、多页、AI 分页快照 | 列表通过；编辑状态有缺陷 | 【实测】0／1／4／5 条、末页及成员切换的 9 份分页快照和 1 份表单快照通过；另看 17 个实际界面状态。四种输入焦点均能放进生产的 3072 字节 AI 上下文；32 字中文姓名满页通过。编辑快照在 AI 更新后仍为旧值，见 C1-01。视觉差 52.004/255，已如实说明，未达 12/255 目标。 |
| 3：选择返回编号和号码，AI／触摸同源 | 稳定记录场景通过 | 【实测】原测试实际触摸“选择这个号码”，与 AI 返回一致且来源分别为 touch／ai。仅令选择返回编号加一后，稳定编号断言失败，其他 116 项通过。 |
| 4：能力表、全量、内存、300 行预算 | 部分通过 | 【实测】新增 8 项能力；临时生成表与运行时 205 项能力、16 项设置一致。产品保守计数 249 行。普通和 ASan／UBSan 各 217/218，仅共享能力文档过期失败；没有跳过测试。 |

## 问题

### C1-01｜高｜旧编辑表单会覆盖 AI 已保存的新号码

- **位置：** `system/apps/contacts/contacts.c:39`、`:63`、`:101`、`:185`。
- **复现命令：** 运行附录脚本，查看 `stale` 场景；顺序为 `contacts.put` 新建号码 `00000000001` → `contacts.edit` 打开该编号 → AI `contacts.put` 改成 `00000000002` → 实际触摸“保存联系人”。每步均等待完成。
- **结果【实测】：** AI 更新前后编辑页的 `page_version` 都是 8，字段仍为 `00000000001`，数据库已是 `00000000002`。触摸保存后数据库回到 `00000000001`。探针退出 1，22 项通过、1 项失败、`test.finished=1`。
- **原因【实现】：** `refresh()` 只处理列表重绘和编辑记录被删除，忽略当前编辑记录被修改；表单对象修订号固定为 1，保存不带读取时的记录版本，`record()` 无条件覆盖。原测试在打开编辑页之前才修改记录（`tests/contacts/contacts.c:153`），测不到这条路径。
- **最小修法：** 复用 `ink_rec_t.ver`，让编辑表单的保存参数携带读取版本，由现有 `record()` 写入口校验；旧版本返回 `STALE`，保留草稿并提示重新载入。快照中的对象修订号使用这个记录版本。补本条交互顺序的回归断言。

### C1-02｜高｜旧删除确认仍会删除已经修改的联系人

- **位置：** `system/apps/contacts/contacts.c:68`、`:101`、`:211`、`:230`。
- **复现命令：** 附录 `delete` 场景：打开联系人 → 实际触摸“删除联系人” → 保留确认卡 → AI `contacts.put` 修改同一编号的姓名和号码 → 人接受原确认卡。
- **结果【实测】：** AI 修改成功后，旧确认仍执行删除，记录消失。探针退出 1，24 项通过、1 项失败、`test.finished=1`。没有延迟线程、替人确认或直接篡改数据库。
- **原因【实现】：** 待确认参数只有 `id`；能力未绑定页面版本，执行时又使用无版本条件的 `ink_db_del()`。确认系统保存参数摘要无法保护未放进参数的记录版本。原测试在弹卡后立即接受（`tests/contacts/contacts.c:165`），未覆盖等待期间的正常更新。
- **最小修法：** 删除参数携带请求时的记录版本，复用已有 `ink_db_del_if_ver()`，版本不同返回 `STALE`。沿用现有确认卡及 C1-01 的记录版本，不加队列、锁或新确认机制；补“弹卡后更新再接受”的回归。

### C1-03｜中｜删除确认只显示内部编号，不能辨认目标

- **位置：** `system/apps/contacts/contacts.c:230`（缺少确认描述；参数仅有编号）。
- **复现命令：** 同附录 `delete` 场景；查看其 `snap-delete-confirm.png` 和日志 `REVIEW delete detail=id：1`。
- **结果【实测】：** 确认卡标题为“删除联系人”，内容只有 `id：1`，没有姓名或号码。截图已核对；从其他页面由 AI 发起时，用户更无法凭这张卡核对删除对象。
- **依据：** 总规则 §4 要求用户文字为中文、说人话；高风险删除确认应让人辨认实际对象。
- **最小修法：** 使用现有确认描述字段，显示从当前成员记录取得的姓名和号码，并与 C1-02 的同一记录版本绑定。无需新增页面或确认组件。

## 构建、测试与反向对照

复用已有两套配置；核对均为 `INK_WERROR=ON`，ASan 配置启用 `-fsanitize=address,undefined -fno-omit-frame-pointer`，字体、Python、LVGL 等来自用户指定的只读依赖目录。命令均以 utility 优先级执行，构建和全量测试各轮串行，最多 `-j2`。

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

| 本轮命令 | 退出码／结果 |
| --- | --- |
| 普通完整构建 | 0 |
| 普通完整 CTest | 8；217/218；215.37 秒 |
| ASan／UBSan 完整构建 | 0 |
| ASan／UBSan 完整 CTest | 8；217/218；444.07 秒 |
| 两套联系人测试 | 各 117＋21 项断言通过；完成标记为 1，警告为 0 |
| 两套内存数据 | 内部总池最低余量 47,840 字节，原门槛 32,768；内／外部内存分配失败均为 0 |
| 内存诊断 | 两套 `Testing/Temporary/LastTest.log` 未发现 ASan／LSan 错误或 UBSan `runtime error` |
| 确定性、分层／SDK、已有应用／确认／AI 表面测试 | 两套全量均通过 |

唯一失败 `capability-table-doc` 是提交表仍为 197 项，运行时／源码已有 205 项。开发报告已如实披露；总规则 §7 禁止本任务修改该共享文件，所以不据此指控虚报。合并会话仍需生成文档后使 G1 全绿。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output "$REVIEW_TMP/capabilities.md" --runtime build/test-out/capability-table/capability-inventory.json
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output "$REVIEW_TMP/capabilities.md" --runtime build/test-out/capability-table/capability-inventory.json --check
```

【实测】上述两条均退出 0。反向对照只改编译输入中的产品文本，原测试及预算不改：

| 只撤掉什么 | 原测试结果 |
| --- | --- |
| 按调用成员开集合，替换为固定 `owner` | 退出 1，4 项断言失败；孩子越界选／改被测出 |
| 仅 `contacts.select` 返回编号加一，保留 put／delete 返回 | 退出 1，1 项断言失败：`selection returns the stable contact id` |

两次反例均编译／链接成功且 `test.finished=1`，不是靠崩溃冒充有效失败。新增观察探针在正常 write／verify 场景通过 139／25 项（额外断言只验证截图及真实 AI 缓冲）；`focus` 26 项、`long` 18 项通过。

## 逐屏视觉检查

起始仓库没有 `out/ui-compare/`；开发者产物位于 `work/contacts-compare/`。已打开该对照图及只读原稿，本轮另从真实联系人测试导出截图，在临时目录的 `out/ui-compare/` 生成并打开 10 个列表状态的对照图，另看 7 个表单／确认状态。未修改或提交设计稿。

| 已查看状态 | 观察 |
| --- | --- |
| empty、kid-empty | 空态中文正确，没有虚构联系人；页码为 1/1，添加入口可见 |
| one、kid-reboot | 单条卡片按实际条数收缩；孩子显示自己的记录 |
| four、multi | 四行姓名／号码和头像完整，无重叠；分别为 1/1、1/2 |
| last | 末页只显示一条，卡片收缩，页码 2/2，无上一页残留项 |
| owner-return、reboot | 成人记录和分页恢复，未混入孩子记录 |
| long-list | 长姓名／号码显示省略号，不挤出行；语义快照保留完整值 |
| form | 两个短输入框和保存按钮完整；新增页面标题仍写“编辑联系人”，可辨认但不如“添加联系人”准确 |
| new-name-focus、new-number-focus | 目标字段、焦点轮廓和静态光标可见，键盘没有遮挡字段 |
| existing-editor、existing-name-focus、existing-number-focus | 姓名、号码、选择／删除／保存入口可见；键盘开启后字段仍可见 |
| delete-confirm | 面板贴底、背景淡化、按钮可见；目标文案问题见 C1-03 |

明显差异：原稿顶部是“拨号／最近／联系人”分段，当前是“联系人”标题；原稿的分隔线和“常用／家人”等标签未保留，第二行改显示号码；分页条从屏底上移，底部新增添加入口。深灰底、较大的字体／头像和控件来自现行主题及规范 §1，不能按旧画板尺寸改回。拨号／最近属于后续任务，不建议为补分段假做入口。以上布局差异已核对，未发现应另列高问题的遮挡或缺项。

【实测】复用本轮已完成刷新的 PhoneContacts 帧运行 `tools/ui_compare.py --reuse`，退出 0，仍为 **52.004/255**。工具成功只代表对照产物有效，不代表达到了 12/255。编辑页没有对应正式画板，只按规范和实际操作检查。

## 范围、规则及架构审视

- 【实测】基线后 11 个文件；产品计数为联系人 240＋启动 2＋目录去重 1＋CMake 6＝249 行。通信服务、预算、历史研究／审计、共享需求与决策未改；没有夹带拨号或短信实现。
- 【实现】数据服务保管按成员集合；首次加载和持久写入走 BG，UI 只读已加载集合；页面使用 F2／F4，LVGL 留在 UI。存储、输入、列表和业务能力的边界合理。
- 【实现／实测】AI 和按钮共用能力总线，查询复用 `data.list/get`；通用 `data.put` 不能绕过联系人校验；新操作均注册。主要缺口是已有编辑／确认目标的版本未贯通，见 C1-01、C1-02。
- 【实现】原测试调整合理：应用数 6→7 与新增应用一致；AI 表追加 8 项；确认能力清单改为遍历所有分页，保留禁止 AI 确认的断言。未发现删测试、放宽断言／预算或用 sleep 掩盖错误；截图前的 5 ms 是已有测试中等待已安排的截图事件。
- 【实测】`git diff --check` 通过；基线至 HEAD 的规定隐私扫描零匹配（`rg` 退出 1）。本轮没有碰设备、调用付费接口、对外发送、推送或提交。未运行 Linux／gcc，不把 macOS 结果扩写成跨平台通过。

## 过度设计与新增机制

未发现需要删除的新增机制。按成员 JSON 集合是任务明示要求，沿用既有数据格式；异步回执复用 BG／UI 执行器；列表复用 F4、输入复用 F2。没有新常驻任务、后台重试、队列、文件格式或配置项。无需为两条版本问题增加通用事务框架；在现有联系人写入口复用记录版本即可。200 条容量和单号码边界已在开发报告列明，不将其升级为用户已定需求。

## 附录：可重放的复查探针

在待审提交、普通构建已完成的仓库根执行。每次使用新的临时目录，避免上一次合成 Flash 影响“初始为空”的断言。脚本从现有测试和源码构造编译输入，不写任何源码文件；观察用截图、日志、模拟 Flash 和可执行文件都在 `REVIEW_TMP`。C1-01／02 对应 `stale`／`delete`，C1-03 的截图为 `delete/delete/snap-delete-confirm.png`。

```sh
export REVIEW_TMP="$(mktemp -d)"
"$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
import json, os, pathlib, re, shlex, subprocess, tempfile
root = pathlib.Path.cwd()
out = pathlib.Path(os.environ["REVIEW_TMP"])
test = (root/"tests/contacts/contacts.c").read_text()
product = (root/"system/apps/contacts/contacts.c").read_text()
flags = (root/"build/CMakeFiles/sim-contacts.dir/flags.make").read_text()
ccflags = []
for name in ("C_DEFINES","C_INCLUDES","C_FLAGS"):
    ccflags += shlex.split(re.search(rf"^{name} = (.*)$", flags, re.M).group(1))
link = shlex.split((root/"build/CMakeFiles/sim-contacts.dir/link.txt").read_text())
def compile_one(name, source, prod=product):
    source = source.replace('#include "../../system/apps/contacts/contacts.c"', prod)
    source = source.replace('#include "../../system/boot/main.c"', '#include "'+str(root/"system/boot/main.c")+'"')
    source = source.replace('#include "../integration/whole_product.h"', '#include "'+str(root/"tests/integration/whole_product.h")+'"')
    obj, exe = out/(name+".o"), out/name
    cp = subprocess.run(["taskpolicy","-c","utility",link[0],*ccflags,"-x","c","-c","-","-o",str(obj)], input=source,text=True,capture_output=True)
    if cp.returncode: raise SystemExit(cp.stderr)
    args = [str(obj) if x=="CMakeFiles/sim-contacts.dir/tests/contacts/contacts.c.o" else str(exe) if x=="sim-contacts" else x for x in link]
    cp = subprocess.run(["taskpolicy","-c","utility",*args],cwd=root/"build",capture_output=True,text=True)
    if cp.returncode: raise SystemExit(cp.stderr)
    print("compiled",name,flush=True)
    return exe
frames = r'''
static char review_frame_name[32];
static volatile bool review_captured;
static void review_capture(void *arg) { (void)arg; sim_display_snapshot(review_frame_name); review_captured = true; }
static void review_frame(const char *name) {
    quiet(); snprintf(review_frame_name, sizeof review_frame_name, "%s", name); review_captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), review_capture, NULL);
    CHECK(WAIT_FOR(review_captured, 1000), "review screenshot captured");
}
static void page(const char *label) { snprintf(case_name, sizeof case_name, "%s", label); on_ui(read_page); review_frame(label); }
static void review_input(void) {
    int n = ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL);
    sim_note("REVIEW input context bytes=%d capacity=%zu page=%s", n, sizeof g_snap, ink_ui_page_name());
    CHECK(n > 0, "review: focused contact editor fits real 3072-byte AI context");
}
'''
extra = r'''
static void review_state(void) {
    int n = ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL);
    sim_note("REVIEW page=%s snapshot=%d field-number=%s page-version=%u",
        ink_ui_page_name(), n, P.fields[1] ? ink_textfield_text(P.fields[1]) : "-", ink_ui_content_version());
    if (P.db && read_contact(P.db, first_id, contact_name, contact_number))
        sim_note("REVIEW stored-name=%s stored-number=%s", contact_name, contact_number);
}
static void review_run(void) {
    open_list();
    CHECK(put("复查联系人", "00000000001", 0), "create review fixture");
    first_id = result_id(); quiet();
    if (!strcmp(phase, "long")) {
        const char *name = "测试测试测试测试测试测试测试测试测试测试测试测试测试测试测试测试";
        for (int i=0;i<3;i++) CHECK(put(name, "+00000000000000000000001", 0), "max length fixture");
        quiet(); page("long-list"); return;
    }
    snprintf(call_args, sizeof call_args, "{\"id\":%u}", (unsigned)first_id);
    CHECK(AI("contacts.edit", call_args) && g_rc.status == INK_RC_DONE, "open review contact"); quiet();
    review_frame("existing-editor"); on_ui(review_state);
    if (!strcmp(phase, "focus")) {
        CHECK(AI("contacts.name", NULL) && g_rc.status == INK_RC_DONE, "focus existing contact name"); quiet();
        on_ui(review_input); review_frame("existing-name-focus");
        CHECK(AI("text.submit", NULL), "blur existing name"); quiet();
        CHECK(AI("contacts.number", NULL) && g_rc.status == INK_RC_DONE, "focus existing contact number"); quiet();
        on_ui(review_input); review_frame("existing-number-focus");
    } else if (!strcmp(phase, "stale")) {
        CHECK(put("复查联系人", "00000000002", first_id), "AI updates open record"); quiet();
        on_ui(review_state); review_frame("after-ai-update");
        tap_label("保存联系人"); quiet(); on_ui(review_state);
        CHECK(read_contact(P.db, first_id, contact_name, contact_number) &&
              !strcmp(contact_number, "00000000002"), "review: stale form must not silently overwrite AI saved number");
    } else if (!strcmp(phase, "delete")) {
        tap_label("删除联系人");
        ink_confirm_view_t held[1];
        CHECK(ink_confirm_list(held, 1) == 1, "review delete confirmation exists");
        sim_note("REVIEW delete detail=%s", held[0].detail);
        review_frame("delete-confirm");
        CHECK(put("已经改名的联系人", "00000000002", first_id), "AI updates while delete awaits human");
        quiet(); CHECK(ink_confirm_decide(held[0].token, true, INK_SRC_TOUCH)==0,"answer original delete card"); quiet();
        on_ui(review_state);
        CHECK(P.db && ink_db_get(P.db, first_id, NULL, NULL, 0)==INK_DB_OK,
              "review: old delete confirmation must not delete changed content");
    }
}
'''
probe = test.replace('static void page(const char *label) { snprintf(case_name, sizeof case_name, "%s", label); on_ui(read_page); }',frames)
probe = probe.replace('    on_ui(inspect_short_focus);','    on_ui(inspect_short_focus); on_ui(review_input); review_frame(index ? "new-number-focus" : "new-name-focus");')
probe = probe.replace('static void run(void *arg) {',extra+'\nstatic void run(void *arg) {')
probe = probe.replace('if (!strcmp(phase, "write")) write_phase(); else verify_phase();','if (!strcmp(phase, "write")) write_phase(); else if (!strcmp(phase, "verify")) verify_phase(); else review_run();')
exe = compile_one("probe", probe)
for name, prod in (
    ("negative-member",product.replace('ink_db_open_member(&CONTACTS, ink_cap_call_member(c))','ink_db_open_member(&CONTACTS, "owner")')),
    ("negative-selection",product.replace('ink_jw_int(&w, id); ink_jw_raw(&w, ",\\"number\\":")','ink_jw_int(&w, id + (!put && !del)); ink_jw_raw(&w, ",\\"number\\":")'))
):
    assert prod != product, name
    compile_one(name,test,prod)
print(json.dumps({"review_tmp":str(out)}),flush=True)

import json, os, pathlib, subprocess
root=pathlib.Path.cwd()
tmp=pathlib.Path(os.environ["REVIEW_TMP"])
base=[str(tmp/"probe"),"--profile",str(root/"simulator/profiles/metalio-plus.json"),"--script",str(root/"tests/contacts/contacts.script"),"--budget",str(root/"tests/ui_scene.budget.json"),"--assets",str(root/"build/assets"),"--content",str(root/"build/test-content"),"--no-random-outages"]
for phase in ("write","verify","stale","focus","delete","long"):
    saved=tmp/("visual" if phase in ("write","verify") else phase)
    saved.mkdir(exist_ok=True)
    cp=subprocess.run(["taskpolicy","-c","utility",*base,"--out",str(saved/phase),"--flash-image",str(saved/"user.img"),"--sd-dir",str(saved/"sd")],env=dict(os.environ,INK_CONTACTS_PHASE=phase),capture_output=True,text=True)
    (saved/(phase+".log")).write_text(cp.stdout+cp.stderr)
    m=json.loads((saved/phase/"metrics.json").read_text())
    print(json.dumps({"phase":phase,"exit":cp.returncode,"pass":m["metrics"].get("test.pass"),"fail":m["metrics"].get("test.fail"),"finished":m["metrics"].get("test.finished"),"warnings":m.get("warnings")},ensure_ascii=False))
    for line in cp.stderr.splitlines():
        if "REVIEW" in line or "CHECK FAILED" in line: print(line)
for name in ("negative-member","negative-selection"):
    cp=subprocess.run(["taskpolicy","-c","utility",os.sys.executable,str(root/"tests/contacts/run.py"),str(tmp/name),str(root),str(root/"build/assets"),str(root/"build/test-content"),str(tmp/(name+"-out"))],capture_output=True,text=True)
    print(name,"exit",cp.returncode)
    m=json.loads((tmp/(name+"-out")/"write/metrics.json").read_text())
    print(json.dumps({"pass":m["metrics"].get("test.pass"),"fail":m["metrics"].get("test.fail"),"finished":m["metrics"].get("test.finished"),"warnings":m.get("warnings")},ensure_ascii=False))

PY
```

SEVERE=0 HIGH=2
