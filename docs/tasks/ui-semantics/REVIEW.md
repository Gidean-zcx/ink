# ui-semantics 第 1 轮复查

日期：2026-11-08。基线：`ed3d122665cf704bb77a962f1fe38d66f6febe01`；复查提交：`3ea377e60f71f99c5c8d082e9e4db52e3f044cc8`。

**结论：任务书五项指定测试均可复现；另确认 1 个中等级别的快照合同问题，建议修正后验收。严重 0，高 0。** 本轮仅写本报告，不修改产品源码、既有测试或共享文档，不提交。构建输出仍在忽略目录；探针、反向实验和对照图均在临时目录。

## 1. 问题

### US-R1-001｜中｜快照把仅限本人和不可调用的能力列为可用动作

- **位置**：`system/ui/ui_semantics.c:49`；关联 `system/policy/cap.c:62`、`:1005`、`:1092`；现有测试 `tests/ui_semantics.c:84`。
- **触发**：C 页声明一个正常的本人操作，例如绑定 `confirm.accept`。该能力确实存在，`ink_cap_available()` 返回真，但它只检查存在性和页面匹配，不检查 `INK_EXPOSE_OWNER`、当前成员的能力权限，也不排除事件。
- **结果【实测】**：在原测试页的临时副本中绑定已有的 `confirm.accept` 和 `confirm.result`，现有 AI 目录 `ink_cap_ids_json()` 正确排除二者，语义快照却将二者及其参数写入 `actions`，且没有“仅本人”或“事件不可调用”的说明。探针 33 项通过、2 项失败，退出 1；失败分别为 `semantic snapshot must omit owner-only confirm.accept` 和 `semantic snapshot must omit non-callable confirm.result`。原有 29 项断言仍全部通过。
- **影响与定级**：与应用框架设计 §5.1“快照只列当前有效的操作”不符，AI 得到的页面动作和实际能力目录不一致。实测 AI 执行这两个动作仍返回 `INK_RC_INVALID`；没有发生 AI 代替人确认、越权写入或成员隔离失效，故定为中。成员权限缺口由代码确认，本轮动态探针仅验证 owner-only 和事件。
- **复现命令**：在仓库根目录运行第 5 节整段命令，其中 `policy` 分支使用未改动的产品库及上述两个真实能力登记项。
- **最小修法**：删除此处独立的弱筛选，复用能力目录已有的集中可调用性判断（`listed()` 加事件排除），让语义导出与 `ink_cap_ids_json()` 一致；不要在每个页面复制权限规则。补一条覆盖上述绑定的快照断言。涉及能力层公共入口时由对应任务协调合入，本复查没有改 `system/policy`。

## 2. 逐条验收

| 项 | 复查结论 | 独立证据 |
| --- | --- | --- |
| 1 接口、实现、最小 C 页面 | 通过 | 头文件／实现完整；`tests/ui_semantics.c:50` 使用既有顶栏、列表行、胶囊按钮；真实触摸经过能力总线，非直接调用业务回调。 |
| 2 同页导出字段、动作、对象编号和版本 | 指定用例通过；有上述合同问题 | 页面与 AI 上下文快照相同，JSON 解析、转义、字段和动作目标均有断言；对象修订号 7 与运行时页面版本分别校验。既有测试仅覆盖普通、禁用及不存在的能力。 |
| 3 秘密字段与反向对照 | 通过 | 密码／验证码覆盖有值、空串和 NULL；不输出明文、真实长度或秘密对象引用。撤掉遮盖后 3 项失败，退出 1。 |
| 4 旧页面版本拒绝 | 通过 | 使用全局测试能力，避免由总线已有的页面绑定检查替代新检查；旧版本没有副作用，新版本成功。单独删除版本比较后 3 项失败，退出 1。 |
| 5 全量、内存及行数 | 本机通过 | 普通及 ASan／UBSan 各 206/206，无失败、无跳过；产品头文件 61 行＋实现 87 行＋全部 CMake 新增 7 行，保守计 155 行，低于 300 行。 |

两套原始 F1 测试均为 `test.pass=29`、`test.fail=0`、`test.finished=1`。内部 RAM 最低余量均为 47,840 字节，原阈值 32,768；内外部内存分配失败均为 0。没有提高预算。

测试证明的是 F1 接口、真实页面快照及带 `INK_SRC_AI` 的能力调用；没有宣称新增了中枢端到端的页面动作协议。现有页面迁移和输入宿主均不属于本任务，`ai_service.c` 未改。

## 3. 构建与全量测试

复用了已核对的构建配置：依赖来自 `$INK_DEPS`，两套均为 `INK_WERROR=ON`；内存配置额外含 `-fsanitize=address,undefined -fno-omit-frame-pointer`。服务器测试解释器指向 `$INK_DEPS/hub/.venv/bin/python`。按总规则第 7 节串行运行两轮，构建最多 3 路、CTest 最多 2 路。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
export UBSAN_OPTIONS=halt_on_error=1
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】两个构建退出 0；普通全量退出 0、105.53 秒；ASan／UBSan 全量退出 0、195.95 秒。包含确定性、分层和 SDK 边界检查；没有启动带窗口的模拟器。Linux／gcc、真机未测。

## 4. 设计对照：实际查看 10 屏

开工时仓库的 `out/ui-compare/` 不存在。本轮用普通全量刚生成的 `build/test-out/ui-scene-*/snap-scene.png`，通过现有 `tools/ui_compare.py --reuse` 在临时目录生成同样的三联图并逐屏打开；工具核查了 settled、finished 和无失败计数。设计来源为 `$INK_DESIGN/png`。

| 屏幕 | 1/4 灰度 MAE | 肉眼看到的明显差异 |
| --- | ---: | --- |
| Main | 54.194 | 时钟、圆形入口、Dock 放大，入口行上移；浅灰变深灰；“继续读”示例换为空状态，AI 图标改为单颗四角星。 |
| Today | 48.401 | 标题、卡片及间距变大；阅读、听书、报纸等示例内容换为空状态，多处文字换行。 |
| Apps | 42.961 | 搜索栏及应用圆钮变大、组间距增大；一屏变两页并出现“1/2”，底部一组在下一页。 |
| Control | 39.888 | 滑杆、三按钮及行间距增大；无通知时不显示设计里的两张通知卡；底部短横线改为“收起”按钮。 |
| Notifications | 25.302 | 实测仅一条报纸通知；翻页箭头改为大胶囊“上一页／下一页”；增加“全部已读”，没有稿中的“读未读通知／全部清除”底栏。 |
| Members | 24.948 | 合成数据为“我／小朋友”两人，稿为三人；头像布局及字号不同，未设 PIN 的场景没有锁角标。 |
| Pin | 23.964 | 稿中的成人头像、姓名和四个空心圆未按相同层次呈现；实测为锁图标及“输入密码”，背后头像淡显；数字键放大、上移，底行变“删除／0／确定”。 |
| ImePassword | 32.135 | “连接 Wi-Fi”变“网络密码”；小型密码框变大文本区，没有网络名称条、“显示”按钮及隐私提示；“连接”变“完成”。该场景实际使用测试输入页。 |
| Confirm | 37.809 | 实测背景淡化、面板高度随内容变化；“确认发送”变通用“确认”，说明变“只有你确认后才会继续”；预览内容为合成地址。 |
| Settings | 81.373 | 分组高度和行距增大、底色变深；未呈现稿中的顶栏搜索按钮及图标白底圆；状态文字和家人数不同。 |

这些图都超过旧对照工具的 12 阈值，不能报告“视觉完全一致”。其中整体 1.2 倍放大、灰底加深、背景淡化和 AI 图标变化已有 `docs/project.md:52`、`:57`、`:61` 的后续实机反馈记录，不能只因偏离旧稿就要求回退；数据差异也不等于渲染缺陷。未发现新的明显错字。

上述页面与场景源码相对给定基线均未改，新语义接口仅被新增测试引用，故这些差异不计为 F1 新增缺陷。需在后续对应页面验收时处理的具体定位：通知布局 `system/ui/shell/navigation.c:1111`，PIN 页 `system/ui/shell/member_gate.c:89` 及 `system/ui/shell/pinpad.c`，密码测试替身 `tests/ui_shell_scenes.c:52`，确认按钮用词 `system/ui/shell/confirm_card.c:218`。本轮不扩范围修页面，也不改稿迁就实现。

复现对照图（先完成普通全量；`REVIEW_TMP` 使用第 5 节创建的临时目录）：

```bash
export REVIEW_TMP
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
import os, pathlib, subprocess, sys
root = pathlib.Path.cwd()
out = pathlib.Path(os.environ["REVIEW_TMP"]) / "out/ui-compare"
out.mkdir(parents=True, exist_ok=True)
scenes = "Main Today Apps Control Notifications Members Pin ImePassword Confirm Settings".split()
for scene in scenes:
    link = out / scene
    if not link.exists():
        link.symlink_to(root / "build/test-out" / ("ui-scene-" + scene), target_is_directory=True)
subprocess.run([sys.executable, "-B", "tools/ui_compare.py", "--reuse",
                "--out", str(out), "--scenes", *scenes], check=True)
PY
```

## 5. 独立反向实验及问题复现

以下只将变体从标准输入编译到临时目录，复用正常构建的链接参数，不修改仓库源码、测试或断言。三个变体依次运行；每个非零模拟器退出码均来自预期的断言／预算失败。

| 变体 | 实测结果 |
| --- | --- |
| `mask`：秘密值直接写出 | 编译／链接退出 0；运行退出 1；26 通过、3 失败、finished=1。失败为密码／空验证码、秘密长度变化、已填验证码／空密码。 |
| `version`：只删页面版本比较 | 编译／链接退出 0；运行退出 1；27 通过、3 失败、finished=1。旧版本误执行，随后两项副作用次数断言也失败。 |
| `policy`：产品库不改，增加两个现有能力绑定 | 编译／链接退出 0；运行退出 1；33 通过、2 失败、finished=1。仅两项快照过滤断言失败，总线拒绝执行的两项断言通过。 |

```bash
export REVIEW_TMP="$(mktemp -d)"
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
import json, os, pathlib, re, shlex, subprocess
root = pathlib.Path.cwd()
build = root / "build"
tmp = pathlib.Path(os.environ["REVIEW_TMP"])
def flags(target):
    text = (build / "CMakeFiles" / (target + ".dir/flags.make")).read_text()
    return sum((shlex.split(re.search(r"^" + key + r" = (.*)$", text, re.M).group(1))
                for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS")), [])
link = shlex.split((build / "CMakeFiles/sim-ui-semantics.dir/link.txt").read_text())
test_obj = "CMakeFiles/sim-ui-semantics.dir/tests/ui_semantics.c.o"
source = (root / "system/ui/ui_semantics.c").read_text()
test = (root / "tests/ui_semantics.c").read_text()
probe = r'''
static void review_policy(void) {
    ink_ui_action_spec_t forbidden[] = {
        {.id="owner", .label="本人确认", .capability="confirm.accept",
         .args_json="{\"token\":42}", .enabled=true},
        {.id="event", .label="事件", .capability="confirm.result", .enabled=true}
    };
    ink_ui_page_spec_t page=model;
    page.actions=forbidden; page.action_count=2;
    char *directory=osal_malloc(16384, OSAL_MEM_EXTERNAL);
    CHECK(directory && ink_cap_ids_json(directory,16384)>0 &&
          !strstr(directory,"\"confirm.accept\"") && !strstr(directory,"\"confirm.result\""),
          "existing AI directory omits owner-only actions and events");
    CHECK(ink_ui_semantics_snapshot(&page,previous,sizeof previous)>0,
          "probe snapshot succeeds");
    CHECK(!strstr(previous,"\"confirm.accept\""),
          "semantic snapshot must omit owner-only confirm.accept");
    CHECK(!strstr(previous,"\"confirm.result\""),
          "semantic snapshot must omit non-callable confirm.result");
    ink_receipt_t receipt;
    ink_ui_action_target_t t={ink_ui_content_version(),model.object};
    CHECK(ink_ui_semantics_action(&page,"owner",&t,INK_SRC_AI,&receipt)==INK_RC_INVALID,
          "execution gate still refuses AI confirmation");
    CHECK(ink_ui_semantics_action(&page,"event",&t,INK_SRC_AI,&receipt)==INK_RC_INVALID,
          "execution gate still refuses an event invocation");
    osal_free(directory);
}
'''
variants = [
    ("mask", source.replace(
        'f->secret ? (f->value && f->value[0] ? "有值" : "无值") : (f->value ? f->value : "")',
        '(f->value ? f->value : "")'), None),
    ("version", source.replace(' || target->page_version != rc->page_gen', ''), None),
    ("policy", None, test.replace('static void inspect(void) {',
        probe + '\nstatic void inspect(void) {\n    review_policy();'))
]
for name, semantic, test_source in variants:
    obj, binary = tmp / (name + ".o"), tmp / (name + "-sim")
    target = "ink_system" if semantic is not None else "sim-ui-semantics"
    content = semantic if semantic is not None else test_source
    subprocess.run([link[0], *flags(target), "-I" + str(root / "tests"),
                    "-x", "c", "-c", "-", "-o", str(obj)], input=content, text=True, check=True)
    command = link.copy()
    command[command.index("-o") + 1] = str(binary)
    if semantic is not None:
        command.insert(command.index("-o"), str(obj))
    else:
        command[command.index(test_obj)] = str(obj)
    subprocess.run(command, cwd=build, check=True)
    out = tmp / name
    result = subprocess.run([
        str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
        "--script", str(root / "tests/ui_semantics.script"),
        "--budget", str(root / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"),
        "--out", str(out), "--quiet", "--no-random-outages"
    ], text=True, capture_output=True)
    (tmp / (name + ".log")).write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text())
    print(name, "exit=", result.returncode,
          {key: data["metrics"].get(key, 0) for key in ("test.pass", "test.fail", "test.finished")})
    print(data.get("warnings", []))
PY
```

## 6. 品味、规模及总规则

【实现】没有另造绘图库、持久格式、状态机、配置项或常驻产品任务。148 行 C 头文件与实现只承担声明、JSON 导出及执行前校验，复用主题、页面版本和能力总线；150 行测试与任务规模相称。超过 100 行的理由和更小方案已在开发报告说明，未见为预算压缩必要语义的证据。

**建议删除的内容仅为 US-R1-001 的独立弱筛选；没有理由再增加通用渲染器、权限缓存或逐页面补丁。** 声明归页面、业务仍由能力提供方执行，分层总体合理。对象参数自身的业务修订校验仍归能力提供方，头文件已明确，不应把桥接检查说成业务事务校验。

【实测／代码检查】相对基线只有 7 个任务文件；不含其他并行任务、AI 服务或共享需求文档。已有测试仅调整 SDK 头文件允许表。新测试的 `delay_ms(1800)` 用于启动准备，随后还有实际就绪等待和真实触摸；没有修改产品时序、放宽断言或预算来让测试通过。

运行 `git diff --check` 无错误；按总规则扫描基线至 HEAD 的本机路径与 MAC 模式零匹配（`rg` 退出 1）。复查结束时 `git diff --exit-code -- CMakeLists.txt system tests` 退出 0，工作区仅新增本报告。没有触碰设备、真实密钥、付费接口、提交、推送或发布。

交互式决策板本次读取不可达；本轮按任务书已写明的 2026-11-08 五项 A 决定复查，未改共享决策记录。

SEVERE=0 HIGH=0
