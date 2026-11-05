# backup-restore 复查

复查日期：2026-11-06；基线 `a51a804c`，受检提交 `d3c0ca98`。**不通过：1 条严重、2 条高。** 原有普通和 ASan/UBSan 全量测试均通过，补充场景复现了成员身份复用泄漏、目录遍历栈超额和正常大小文件阻塞后台能力的问题。

按 `review-prompt.md`、`codex-rules.md` 和 backlog 开头及 backup-restore 节验收。开始时旧 REVIEW.md 为 0 字节，无上一轮问题编号；优先核实了最近提交的目录成员绑定、记录校验及报告中的六组反向对照。只修改本文件，不提交。探针仅在 `build/` 生成临时测试源码、对象、可执行文件和合成数据，产品源码、正式测试源码及原构建归档不变。

## 验收核对

| 项目 | 结论 | 本轮独立证据 |
| --- | --- | --- |
| B1 清单、上传回执 | 部分通过 | 原串联场景包含集合、位置、个人文件、空文件及重启回执，区分 received/durable。2 MiB 文件使入口超时，BR-R3。 |
| B2 预览、覆盖确认、未恢复项目 | 原小样本通过，身份边界不通过 | 错误 PIN、AI 代确认、确认期间换人、预览后更新、坏哈希、越界路径和重复条目均拒绝；未安装集合有说明。复用 ID 却把旧成人快照当作新孩子资料，BR-R2。 |
| B3 备份 → 清空设备 → 恢复 | 通过，限声明的成员资料 | 实际清空并重启；两成员的编号、版本、删除、下个编号、文件和位置复原；再次重启后阅读器回第 4 页。不代表设备设置、身份及凭据已恢复。 |
| B4 中途断电、断网继续 | 现有场景通过 | 独立运行上传/恢复中断、网络中断、成员离开后回来及六组 Flash 故障；核对了各阶段断言和断电判定。 |
| B5 成员隔离 | **不通过** | 常规换人和过期 generation 场景通过；ID 复用的新孩子实际读到旧成人资料，BR-R2。 |
| G1 全量、内存检查、预算 | 原全量通过，补充预算验证不通过 | 普通 105/105（44.96 s），ASan/UBSan 105/105（83.38 s），均退出 0；三层目录起触发栈警告，BR-R1。 |
| G2 原子能力、AI 路径 | 接入通过，隔离缺口见 B5 | 五调用、两事件经统一能力门；capability-table 系列及 integration-ai-surface 通过。操作与 UI 分离，状态/清单结构化，无新页面。 |
| G3 依赖表状态 | 通过 | 只把状态格改为“部分”，原判断未改。 |
| G4 页面/视觉对照 | 不适用 | 无页面或动画实现，无需启动窗口或对照十屏。 |
| G5 服务层范围 | 通过 | 服务、启动装配、增量接口、模拟中枢、构建及测试，无范围外应用开发。 |
| V1 修复反向对照 | 六项通过 | 版本保护/日志换代/Flash 条件写/卡文件条件写/目录成员绑定撤除后分别 2/1/2/2/2 条断言失败；空闲启动变体触发既有内存预算失败。保留换代但不提交旧日志清理的对照正常。 |
| P1 差异、规则、隐私 | 检查通过 | `git diff --check a51a804c..HEAD` 退出 0；规定隐私模式 0 匹配；未改预算、删旧测试或放宽断言。既有 AI 测试仅增加新能力/事件。 |

实际执行命令，构建和全量测试均串行安排：

```sh
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
"$INK_DEPS/build-deps/pyenv/bin/python" tests/backup_restore/reverse.py --build build
```

两套 CMakeCache 均为 `INK_WERROR=ON`，内存检查另有 `INK_SANITIZE=ON`；依赖来自 `$INK_DEPS`，中枢使用已有本地虚拟环境。gui-smoke 使用既有 SDL dummy 模式，没有启动带窗口的 ink-sim-gui。反向对照证据：`build/backup-reverse-19dl4x94/`。

## 问题

### BR-R2（严重）：复用成员 ID 后，新孩子能恢复已删除成人的备份

**位置**：`system/include/ink/sys/backup_format.h:14`；`system/services/backup/backup_service.c:559`、`:906`；`simulator/environment/sim_backup.c:43`。新成员代次生成在 `system/policy/members.c:556`。

**复现命令**：下文附录，`INK_REVIEW_CASE=identity`。前置为原串联测试上传过成人资料。通过能力和人工确认创建另一位成人、切换、删除旧 `owner`，等到 ID 确实从成员表消失，再以同一 ID 创建新孩子。确认新旧 generation 不同，孩子执行预览、普通人工确认恢复及 data.get。

**结果【实测】**：23 条前置和流程断言通过、恢复完成，仅两条隔离断言失败。孩子的 `data.get {collection:test.backup,id:41}` 返回原成人的 `{"text":"测试大人资料"}`。从有本地成人数据及已清空的设备副本各做一次，均复现。证据：`build/review-backup-reused-local-6lu9ealf/out/metrics.json`、`build/review-backup-reused-id-clean-4buir_8a/out/metrics.json`。没有绕过数据能力门或用管理员身份读取；确认文案仍声称恢复的是“这位成员”的资料。

**原因与影响【实现】**：清单、请求和模拟中枢都只按可复用的成员字符串分区。当前 generation 只能挡住旧本地调用，不能证明云端快照属于新身份。删除成人、创建孩子不等于授权转移原成人备份，B5 失效。这里是任务指定模拟环境中已发生的成人资料泄漏，不声称真实服务器已部署。

**修法**：备份绑定不会随本地 ID 复用而复用、可跨重启验证的成员身份；请求、清单、回执和恢复授权一并校验，模拟中枢同步验证。新身份默认不得取旧身份的备份；确需迁移另走明确授权。不能把每次启动重建的本地 generation 当作持久身份。补删除 → ID 复用 → 孩子预览/恢复/读取的正反向测试。

### BR-R1（高）：支持的嵌套目录超出后台任务栈预算

**位置**：`system/services/backup/backup_service.c:286`、`:302`；栈配置 `system/services/res/exec.c:17`。

**复现命令**：附录 `INK_REVIEW_CASE=depth`，默认三层；用 `INK_REVIEW_DEPTH=8` 验证声明的最大深度。只把原测试的 `photos/test.bin` 改为嵌套目录，仍用原文件服务创建、写入和备份。

**结果【实测】**：深度 1/2/3/4 时，后台栈峰值估计为 5,840/7,056/8,272/9,488 B；八层 14,352 B，配置均为 8,192 B。第三层起警告；八层两个成员仍收到备份回执，36 条业务断言通过，但原 run.py 因警告退出 1。证据：`build/review-backup-depths-v8zyvgjj/`、`build/backup-restore-7nsgioxz/roundtrip/seed/metrics.json`。同参数加 `-fstack-usage` 编译原源码，walk_personal 每层宿主栈帧 1,216 B，见 `build/review-backup-stack-p28b7rc5/backup.c.su`。

**影响与边界**：三层和八层都在声明范围内；原测试只有一层，不能据其通过认定支持范围内的栈安全。这是模拟器保守宿主估计及本机栈帧，不冒充 S31 实机溢出测量。

**修法**：用有界迭代遍历，将目录栈、路径和复制状态放 PSRAM；补支持深度的预算测试及目标编译栈验证。不要提高现有预算或忽略警告。

### BR-R3（高）：整份文件采集占住共享后台执行器，入口和进度查询超时

**位置**：`system/services/backup/backup_service.c:258`、`:327`、`:888`、`:948`；`system/services/res/exec.c:38`；默认能力期限 `system/policy/cap.c:517`。

**复现命令**：附录 `INK_REVIEW_CASE=size`，默认 2 MiB；`INK_REVIEW_BYTES=8388608` 验证 8 MiB。只改原个人文件夹具长度，未改档案、网络、超时或断言。

**结果【实测】**：2 MiB 时，两位成员的 backup.now 都先返回 `timed out, it may still finish`，原“开始备份应 queued”断言各失败一次；之后仍完成两份备份，`cap.timeouts=2`、`cap.late=2`。8 MiB 时 `cap.timeouts=10`，除两次入口外，轮询 backup.status 也超时。证据：`build/review-backup-size-9fk840ca/2097152/out/metrics.json`、`8388608/out/metrics.json`。

**原因与影响【实现】**：cap_backup 返回 queued 前同步遍历、复制全部个人文件并重新整文件计算哈希，一直占用唯一 exec.bg；这些能力未声明 QoS，默认期限 3 秒。状态查询、成员 PIN、重启也依赖该执行器。个人卡文件不受 1 MiB 小文件限制，照片/录音等正常规模即可触发；资料越多，其他后台操作排队越久。96 KiB 原夹具测不到这条实际卡顿路径；没有把有限超时说成实测永久死锁。

**修法**：先持久登记采集任务、及时返回 queued，把遍历、复制及哈希改为可续接的有界步骤；每步检查成员和后台暂停条件，给状态、PIN 等工作执行机会。仅延长 backup.now 超时不能解决共享队列阻塞。补数 MiB 文件及并发查询进度的测试。

## 已修复项、差异与边界

- 最近提交的原成员目录绑定生效；撤掉后两条断言失败。记录校验、版本条件、Flash/卡文件条件替换和日志换代证据成立，不能推及成员 ID 复用。
- 没发现删旧断言、抬预算或加 sleep 掩盖时序。等待用于异步完成、重试或故障注入；本次补充场景保持原配置和断言。
- 报告已明确披露设备级设置/身份/凭据、真实对象存储、增量对账和 dataset/restore_token 未实现。任务允许 sim_hub，这些缺口未另算“假实现通过”；仍不能称完整设备同步/恢复。
- 本机 macOS/clang 验证；新增宿主读写和 fgets 已逐处核对返回值，没有 Linux/gcc 或真机通过证据。未连接设备、真实服务器、付费服务，未 push/开 PR。

## 附录：独立补充探针

从原测试生成隔离副本、链接原产品归档；identity 先跑原完整串联测试取得合成快照。模拟器退出 0 不代表断言通过：脚本核对 test.fail、test.finished、warnings，受检提交三个用例均应退出 1。

【实测】本报告内的脚本已逐个重跑，均得到预期失败，且都完成测试流程：identity 为 23 通过/2 失败（`build/review-backup-probe-44kbdrzz/`）；depth 为 36 通过/0 失败但栈警告（`build/review-backup-probe-4lvcnyxs/`）；size 为 34 通过/2 失败（`build/review-backup-probe-etcx4kwp/`）。结束时 `git diff HEAD` 为空，工作区仅本 REVIEW.md 为未跟踪文件；本报告隐私扫描 0 匹配。

分别运行三次，把第一行 identity 改为 depth、size：

```sh
INK_REVIEW_CASE=identity "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
report = Path("docs/tasks/backup-restore/REVIEW.md").read_text()
code = report.split("<!-- REVIEW_PROBE_BEGIN -->\n```python\n", 1)[1].split("\n```", 1)[0]
exec(compile(code, "backup-review-probe", "exec"))
PY
```

<!-- REVIEW_PROBE_BEGIN -->
```python
from pathlib import Path
import json, os, shlex, shutil, subprocess, sys, tempfile

root = Path.cwd()
build = root / "build"
case = os.environ.get("INK_REVIEW_CASE", "identity")
assert case in ("identity", "depth", "size")
work = Path(tempfile.mkdtemp(prefix="review-backup-probe-", dir=build))
source = (root / "tests/backup_restore/device.c").read_text()
if case == "depth":
    depth = int(os.environ.get("INK_REVIEW_DEPTH", "3"))
    assert 1 <= depth <= 8
    tree = "/".join("d" + str(i + 1) for i in range(depth))
    source = source.replace("photos/test.bin", tree + "/test.bin")
    source = source.replace('"/me/photos"', '"/me/' + tree + '"')
elif case == "size":
    length = int(os.environ.get("INK_REVIEW_BYTES", "2097152"))
    assert 0 < length <= 8388608
    source = source.replace("98304", str(length))
else:
    helper = r'''static bool review_member_exists(const char *id) {
    for (int i=0;;i++) { const ink_member_t *m=ink_member_at(i); if (!m) return false; if (!strcmp(m->id,id)) return true; }
}
'''
    branch = r'''    if (!strcmp(phase,"reused-id")) {
        uint32_t old_generation=ink_member_generation("owner");
        unsigned token=0;
        CHECK(BY_HAND("member.add","{\"id\":\"reviewadult\",\"name\":\"测试监护人\",\"role\":\"adult\"}") && g_rc.status==INK_RC_PENDING_CONFIRMATION,"add separate adult goes through human gate");
        CHECK(sscanf(g_rc.object,"confirm:%u",&token)==1 && ink_confirm_decide(token,true,INK_SRC_TOUCH)==0,"human approves new adult");
        CHECK(WAIT_FOR(ink_member_generation("reviewadult")>0,10000),"separate adult exists");
        CHECK(BY_HAND("system.switch_member","{\"id\":\"reviewadult\"}") && g_rc.status==INK_RC_DONE,"switch separate adult");
        CHECK(BY_HAND("member.remove","{\"id\":\"owner\"}") && g_rc.status==INK_RC_PENDING_CONFIRMATION,"remove old adult goes through human gate");
        CHECK(sscanf(g_rc.object,"confirm:%u",&token)==1 && ink_confirm_decide(token,true,INK_SRC_TOUCH)==0,"human removes old adult");
        CHECK(WAIT_FOR(!review_member_exists("owner"),10000),"old adult fully removed before id reuse");
        CHECK(BY_HAND("member.add","{\"id\":\"owner\",\"name\":\"测试新孩子\",\"role\":\"child\"}") && g_rc.status==INK_RC_PENDING_CONFIRMATION,"reuse deleted id for a child");
        CHECK(sscanf(g_rc.object,"confirm:%u",&token)==1 && ink_confirm_decide(token,true,INK_SRC_TOUCH)==0,"human approves new child identity only");
        CHECK(AI("member.list",NULL) && g_rc.status==INK_RC_DONE,"new child creation has finished");
        CHECK(BY_HAND("system.switch_member","{\"id\":\"owner\"}") && g_rc.status==INK_RC_DONE && ink_member_current()->role==INK_ROLE_CHILD,"now using replacement child identity");
        CHECK(ink_member_generation("owner")!=old_generation,"new child has a different local incarnation");
        CHECK(AI("restore.preview",NULL),"new child submits preview request");
        CHECK(wait_phase("\"phase\":\"preview\"",60),"new child receives a preview");
        char preview_token[33]="";
        ink_json_t *j=osal_malloc(sizeof *j,OSAL_MEM_EXTERNAL);
        CHECK(j && ink_json_parse(j,g_result,strlen(g_result))==0 && ink_json_str(j,ink_json_find(j,0,"preview"),preview_token,33)==32,"preview token belongs to child call");
        osal_free(j);
        unsigned confirm=request_apply(preview_token);
        CHECK(ink_confirm_decide(confirm,true,INK_SRC_TOUCH)==0,"ordinary touch confirms child's own restore");
        CHECK(wait_phase("\"phase\":\"done\"",180),"old adult snapshot installs as new child (%s)",g_result);
        CHECK(AI("data.get","{\"collection\":\"test.backup\",\"id\":41}") && g_rc.status==INK_RC_DONE,"child reads restored data through capability gate");
        CHECK(!strstr(g_result,"测试大人资料"),"child AI must not see old adult data: %s",g_result);
        ink_db_t *db=ink_db_open(&NOTES,NULL); char value[128]="";
        int found=db ? ink_db_get(db,41,NULL,value,sizeof value) : -1;
        CHECK(found!=0 || !strstr(value,"测试大人资料"),"new child must not receive removed adult data: role=%d record=%s",(int)ink_member_current()->role,value);
        ink_db_close(db);
    } else if (!strcmp(phase,"api")) {'''
    source = source.replace("static void run(void *arg) {", helper + "static void run(void *arg) {")
    anchor = '    if (!strcmp(phase,"api")) {'
    assert source.count(anchor) == 1
    source = source.replace(anchor, branch)

flags = {}
for line in (build / "CMakeFiles/sim-backup-restore.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / "CMakeFiles/sim-backup-restore.dir/link.txt").read_text())
obj, binary = work / "device.c.o", work / "sim-review"
(work / "device.c").write_text(source)
commands = [[link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
             "-I", str(root / "tests/backup_restore"), "-c", str(work / "device.c"), "-o", str(obj)]]
link[link.index("-o") + 1] = str(binary)
link = [str(obj) if x.endswith("/tests/backup_restore/device.c.o") else x for x in link]
commands.append(link)
for i, command in enumerate(commands):
    result = subprocess.run(command, cwd=build, capture_output=True, text=True, timeout=90)
    (work / ("build-" + str(i) + ".log")).write_text(result.stdout + result.stderr)
    result.check_returncode()

peer = work / "hub"
phase = "seed"
if case == "identity":
    result = subprocess.run([sys.executable, str(root / "tests/backup_restore/run.py"),
                             "--build", str(build)], capture_output=True, text=True, timeout=240)
    (work / "baseline.log").write_text(result.stdout + result.stderr)
    result.check_returncode()
    seed = root / result.stdout.rsplit("Evidence: ", 1)[1].strip()
    shutil.copy2(seed / "roundtrip/user.img", work / "user.img")
    shutil.copytree(seed / "roundtrip/sd", work / "sd")
    peer, phase = seed / "hub", "reused-id"
else:
    peer.mkdir()
command = [str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--out", str(work / "out"), "--end", "500s", "--quiet", "--no-random-outages",
           "--flash-image", str(work / "user.img"), "--sd-dir", str(work / "sd")]
result = subprocess.run(command, capture_output=True, text=True, timeout=90,
                        env={**os.environ, "INK_BACKUP_PHASE": phase, "INK_SIM_BACKUP_DIR": str(peer)})
(work / "run.log").write_text(result.stdout + result.stderr)
data = json.loads((work / "out/metrics.json").read_text())
metrics = data["metrics"]
print("Evidence:", work.relative_to(root))
print({k: metrics.get(k, 0) for k in ("test.pass", "test.fail", "test.finished", "backup.saved",
       "backup.restored", "cap.timeouts", "cap.late", "task.exec.bg.stack_used_est_bytes")})
print(json.dumps(data.get("warnings", []), ensure_ascii=False))
sys.exit(0 if result.returncode == 0 and metrics.get("test.finished") == 1 and
         metrics.get("test.fail", 0) == 0 and not data.get("warnings", []) else 1)
```
<!-- REVIEW_PROBE_END -->

SEVERE=1 HIGH=2
