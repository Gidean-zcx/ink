# 无线传文件复查（第 1 轮，唯一一轮）

基线：`cb69b93820fccc36caf28b3d5ead8d0e3878fac5`；复查提交：`98be68b4`。日期：2026-11-08。

**结论：尚未达到完整验收。发现中等级问题 2 项，严重 0、高 0。** 现有专项通过，但正常大文件会被授权期限截断，较大请求被拒绝时客户端可能读不到错误原因。两版全量均有能力文档检查失败，另有 3 项跳过，不能写“全绿”。

本轮由复查方独立执行命令、检查差异。只写本报告，不改仓库产品源码、测试、共享文档，不提交。构建、测试及临时反向变体只产生忽略目录内的验证产物；补充探针的临时目录在退出时清理。

## 验收表

| 验收项 | 结论 | 独立证据及命令 |
| --- | --- | --- |
| 1. 外部真上传、当前成员目录、文件编号 | 64 KiB 用例通过；大文件有问题 | 普通和 ASan 全量均实际运行 `integration-wireless-transfer`：18 个客户端阶段、90 项设备检查。二进制逐字节比对、`/me/收件箱/`、索引及编号查询通过；补充命令见末节，16 MiB 失败，见 WT-R1-01 |
| 2. 中断、过期、空间不足、非法名称 | 现有用例通过；较大拒绝请求的回执不可靠 | 专项覆盖断开后清理、120 秒未使用授权过期、10 秒无进展、写前/写中满卡、`..`、绝对路径、控制字符、编码及原始 NUL；补充用例见 WT-R1-02 |
| 3. U 盘双向互斥 | 通过 | 同一专项验证：U 盘已开时授权失败且有原因；授权等待期间也拒绝开 U 盘；关闭授权后 U 盘恢复 |
| 4a. 新操作进入能力表 | 运行时通过；生成文档待合并时更新 | `transfer.start/stop/status` 已注册；`integration-ai-surface`、`capability-table-runtime`、`capability-table-policy` 均通过；`capability-table-doc` 失败 |
| 4b. 普通全量与内存检查 | 未全通过 | Werror 普通及 ASan/UBSan 构建均退出 0；两版全量各 207 项：203 通过、3 跳过、1 失败，ctest 均退出 8；没有检出 sanitizer 错误 |
| 4c. 产品代码 ≤400 行 | 通过 | `git diff --numstat cb69b938..HEAD`：传输服务 202、HAL 80、头文件 11、能力注册新增 5、CMake 新增 7，共 305 行，保守包含测试注册的 CMake 行 |

全量命令，均从仓库根执行；`INK_DEPS`、`INK_DESIGN` 使用用户给定值：

```bash
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" \
  -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

内存检查使用相同配置，目录换成 `build-asan`，加 `-DINK_SANITIZE=ON`，保留 `-DINK_WERROR=ON`；构建仍 `-j3`，全量仍 `-j2`。普通全量 104.11 秒，ASan 全量 189.81 秒。本轮日志在 `build/Testing/Temporary/LastTest.log`、`build-asan/Testing/Temporary/LastTest.log`。

唯一失败均为 `capability-table-doc`。三个跳过项目为 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，原因是既有中枢锁定 Python 环境缺失。没有安装依赖或改变跳过规则。总规则 §7 禁止本分支更新共享能力文档，开发报告也已如实披露；这是合并时的验收欠项，不另算虚报问题。文档差异同时包含基线的文件索引条目与本任务的三项传输能力。

## 问题

### WT-R1-01（中）：授权期限截断已经获准、持续有进展的上传

- 文件和行号：`system/services/file/transfer.c:127`，关联 `:102`、`:136`、`:165`、`:188`。
- 复现命令：本报告末节完整命令，观察 `large`；连接建立后立即连续发送 16 MiB 合成文件，客户端不限速、不暂停、不阻塞模拟器任务。
- 【实测】`small` 对照返回 `201 文件已收到`，落盘 65,536 字节；`large` 在约 5.96 秒宿主时间收到 `BrokenPipeError`，随后能力查询返回 `active=false`、`授权或接收已超时`，目标和 `.part` 均不存在。`--realtime=20` 下对应约 120 秒虚拟时间。
- 【实现】`T.used=true` 后仍无条件检查 `T.expires`。每步仅收 2 KiB，再等至少 20 ms；忽略写卡等开销的接收上限也只有约 100 KiB/s【估算】。较大的书籍、音频等文件即使一直有进展，也会碰到这条总期限。报告所述“授权 120 秒、接收无进展 10 秒”没有在实现中分开。
- 最小修法：只在授权未使用时检查授权期限，即此处使用 `(!T.used && osal_now_us() >= T.expires)`；已经开始的接收继续使用现有无进展超时。补一个持续上传跨过授权期限的用例，不加配置、线程或重试。
- 【实测对照】只在临时链接变体中更改上述条件，其余产品库、客户端、时序和数据不变；同一 16 MiB 文件约 9.84 秒宿主时间返回 `201 文件已收到`，落盘 16,777,216 字节。未把该实验修改写入仓库。

### WT-R1-02（中）：拒绝较大请求时，客户端拿不到真实错误原因

- 文件和行号：`ports/sim/hal/hal_transfer_sim.c:55`（排空上限在 `:57`，关闭在 `:62`），`system/services/file/transfer.c:58`；现有测试的大小边界在 `tests/wireless_transfer/client.py:77`。
- 复现命令：本报告末节完整命令，观察 `invalid-large`、`wrong-large`；分别以 `..` 文件名或错误授权码，正常发送 2 MiB 请求体。
- 【实测】两种请求均得到 `BrokenPipeError`，未取得 HTTP 错误正文；设备端结果分别是“文件名或长度不合法”和“等待上传”。没有生成目标或临时文件。这里验证的是现有测试使用的 Python HTTP 客户端，不冒充浏览器实测。
- 【实现】发送错误响应后，最多排空 64 KiB 已到达的数据就关闭 socket；较大的请求仍在发送，客户端在发送阶段失败，还没进入读取响应阶段。因此现有 64 KiB 负例通过不足以支持普通大文件的错误回执。
- 最小修法：在已有单连接接收/收尾路径中，拒绝后继续消费本次请求剩余内容，再关闭连接；沿用现有无进展超时，不在宿主回调中阻塞等待，不新增后台线程。只提高 64 KiB 常数仍然有同一问题。

两项均未观察到已有文件被覆盖、错误宣告成功、成员越权或卡死，按中等级记录，不把普通上传失败上调成数据丢失。

## 反向对照、范围及规则

- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/wireless_transfer/reverse.py --build build` 退出 0。六个临时变体分别撤掉服务注册、授权校验、超时、编码 NUL 校验、U 盘占用、响应排空，客户端全部退出 1；最后一项明确复现 `ConnectionResetError`。检查过编译/链接成功才运行客户端的路径。现有专项有实际检出能力，但没有覆盖上述较大文件边界。
- 【实现】已有测试只增加 `tests/integration/ai_surface.c:40` 的三项能力清单。未删除测试、放宽断言或预算；客户端的短 sleep 用于等待阶段文件，设备延时用于启动和触发超时，未发现用 sleep 掩盖产品竞态。
- 【实测】`git diff --check cb69b938..HEAD` 通过；`git diff cb69b938..HEAD | rg -n '/Users/|/private/tmp|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}'` 无匹配（退出 1）。未发现碰策略、界面、文本服务、稳定编号实现、共享需求和决策文件的改动，也没有新增第三方依赖。
- 本轮不碰设备、不读硬件备份、不访问远端服务、不提交或推送；运行的是 macOS clang，未把跨档案测试称为 Linux/gcc 实测。
- 本任务是传输服务，设备页面没有改动；不适用“设备界面任务看 10 屏对照图”的检查。HTTP 页只验证了真实 GET 和上传协议，没有进行浏览器视觉或交互验收，开发报告也没有宣称完成该项。

## 架构、AI 原生、品味和规模

文件服务管理授权、归属和收尾，socket 位于平台 HAL；系统层没有引用模拟器头文件，宿主 socket 调用通过 `vtk_host_call`。上传路径固定在授权者的 `/me/收件箱/`，复用已有文件权限和卡占用；专项中的成员切换、儿童无法查询大人文件、儿童文件与大人隔离均通过。

开启、关闭、查询分别是 `transfer.start/stop/status`，人和 AI 经同一个能力入口；状态返回 JSON，文件变化沿用按成员发出的 `file.changed`。没有新增绕过确认的身份/权限操作，也没有让 AI 代替人确认 U 盘模式。运行时能力覆盖通过，生成能力文档仍需合并时更新。

未发现需要另外列为过度设计的问题。单客户端 HTTP、一次性数字授权、收件临时文件、共享执行器和定时器均对应任务边界；无新常驻线程、持久格式、配置项、重试队列或框架，305 行在预算内。建议删去的是“已使用授权仍受授权总期限控制”的条件，不另做传输管理框架；错误回执在同一个收尾点修，不逐个客户端补丁。

## 补充复现命令

从仓库根执行下面命令。它只在 `build/` 临时目录编译测试驱动，链接当前未修改的产品库；复用原测试启动及能力调用助手，仅把驱动改为单次上传后查询状态，延长测试驱动等外部完成的上限。没有延长产品超时，没有人为阻塞或降低发送速度。每个用例使用独立模拟器及合成数据，结束清理。

```bash
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
import http.client, json, shlex, subprocess, tempfile, time
from pathlib import Path
root = Path.cwd()
build = root / "build"
target = build / "CMakeFiles/sim-wireless-transfer.dir"
flags = {}
for line in (target / "flags.make").read_text().splitlines():
    if " = " in line:
        k, v = line.split(" = ", 1)
        flags[k] = shlex.split(v)
template = shlex.split((target / "link.txt").read_text())
source = (root / "tests/wireless_transfer/device.c").read_text()
a, b = source.index("static void run(void *arg)"), source.index("\nvoid ink_main(void)")
source = source[:a] + '''
static void run(void *arg) {
    (void)arg; delay_ms(4000);
    start_transfer(); exchange("review");
    BY_HAND("transfer.status", NULL);
    sequence++; case_name = "result"; vtk_host_call(announce, NULL);
    ink_probe_count("test.finished");
}
''' + source[b:]
source = source.replace("osal_now_us() + 60000000", "osal_now_us() + 300000000")
def wait_stage(out, sequence, proc):
    end = time.monotonic() + 25
    while time.monotonic() < end:
        if proc.poll() is not None:
            raise RuntimeError("simulator exited: " + (out / "sim.log").read_text()[-2000:])
        try:
            s = json.loads((out / "session.json").read_text())
            if s["sequence"] == sequence:
                return s
        except (FileNotFoundError, json.JSONDecodeError):
            pass
        time.sleep(0.005)
    raise TimeoutError("no session")

with tempfile.TemporaryDirectory(prefix="review-wireless-", dir=build) as td:
    td = Path(td)
    obj, binary = td / "probe.o", td / "probe"
    cc = [template[0], *flags["C_DEFINES"], *flags["C_INCLUDES"],
          "-I" + str(root / "tests/wireless_transfer"), *flags["C_FLAGS"],
          "-x", "c", "-c", "-", "-o", str(obj)]
    subprocess.run(cc, input=source, text=True, cwd=build, check=True)
    link = list(template)
    idx = next(i for i, s in enumerate(link) if s.endswith("/tests/wireless_transfer/device.c.o"))
    link[idx] = str(obj)
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    cases = [("small", 65536, "review.bin", None),
             ("large", 16*1024*1024, "review.bin", None),
             ("invalid-large", 2*1024*1024, "..", None),
             ("wrong-large", 2*1024*1024, "review.bin", "000000")]
    for label, size, name, code in cases:
        out = td / label
        out.mkdir()
        script = out / "network.script"
        script.write_text("@0 net wifi\n@350s end\n")
        cmd = [str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--out", str(out), "--script", str(script), "--realtime=20", "--quiet", "--no-random-outages"]
        with (out / "sim.log").open("w") as log:
            proc = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            conn = None
            try:
                s = wait_stage(out, 1, proc)["session"]
                conn = http.client.HTTPConnection("127.0.0.1", s["port"], timeout=20)
                t = time.monotonic()
                try:
                    conn.request("POST", "/upload", b"X" * size,
                                 {"X-Ink-Code": code or s["code"], "X-Ink-Name": name})
                    reply = conn.getresponse()
                    outcome = str(reply.status) + " " + reply.read().decode()
                except (OSError, http.client.HTTPException) as exc:
                    outcome = type(exc).__name__ + " " + str(exc)
                elapsed = time.monotonic() - t
                conn.close()
                (out / "ack-1").touch()
                result = wait_stage(out, 2, proc)["session"]
                files = {str(p.relative_to(out / "sd")): p.stat().st_size
                         for p in (out / "sd").rglob("*") if p.is_file() and ("review.bin" in p.name)}
                print(json.dumps({"case": label, "bytes": size, "client": outcome,
                                  "wall_seconds": round(elapsed, 3), "active": result["active"],
                                  "result": result["result"], "files": files}, ensure_ascii=False), flush=True)
            finally:
                if conn:
                    conn.close()
                if proc.poll() is None:
                    proc.terminate()
                proc.wait(timeout=10)
PY
```

SEVERE=0 HIGH=0
