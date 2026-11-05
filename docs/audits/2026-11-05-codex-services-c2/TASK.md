# 审计任务说明（2026-11-05 第十七轮 C2 路，交给 Codex）

你是独立的软件架构评审方。对象是本项目自己的家用墨水屏设备的固件和它的模拟器。这一路只看**模块划分、AI 原生、能力开放和文档是否属实**，靠读代码、读文档、跑现有测试。问题编号 AUD18-4xx。对象：运行时看 `git log -1`。

先读 `AGENTS.md`（末尾的 hybrid routing 不用管）、`docs/system/system-architecture.md` §4、§5，`docs/system/system-services-design.md`（尤其 §1.3、§8、§15），`docs/audits/README.md` 开头的表。

范围：

1. **模块划分**（用户要求，R-061）：模块、组件、子系统的边界是否合理，是否符合“软总线 + 原子能力”；能力、状态、设置、事件、内部机制归类对不对；服务之间有没有该走总线而没走的路（比如升级、词库直接走 `hal_fs_*`，是否合适）；机制有没有写进界面层。这一轮新增的看仔细：`system/services/res/task.c`、`system/ui/shell/off_picture.c`、`system/policy/system_actions.c` 里恢复出厂和音量键那部分、`system/services/text/` 的编辑部分。
2. **AI 原生**：是不是从第一天就给 AI 用：按钮和 AI 是否走同一条路径；结果是否结构化；事件能否到 AI；页面和系统浮层（键盘、数字键盘、安装卡、家人选择、关机图）AI 是否看得到；AI 的行为有没有来源记录、能否撤销。
3. **能力开放**：用户能做的事、能看的数据，AI 是否都能调用、能读到；对照能力总表（设计文档 §1.3）和运行时的 `tools/list` 核对（测试 `integration-ai-surface`）；只给人用的那几个（`member.unlock`、`member.set_pin`、`system.upgrade`、`system.upgrade_from_card`、`confirm.accept`、`confirm.reject`）理由站不站得住。
4. **文档是否属实**：`docs/system/system-services-design.md` §9～§15、`docs/system/ui-dependencies.md` §4.1 和 §7，和代码、测试对得上吗。
5. **第十四轮报告的架构节**（[报告](../2026-11-04-codex-services/report.md)）：当时提的改了没有。

## 规则

- **只读审计**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在本目录**：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充的测试程序和运行说明）。单个文件不超过 3 MB。
- **边做边写，这一条很重要**：运行可能被中途打断。现在就建好 `report.md` 的骨架；每查完一块，立刻把结论写进 `report.md` 和 `findings.json`，不要攒到最后。最后再通读一遍。
- **这一路不写实验程序**：只读代码、读文档、跑现有测试（`integration-ai-surface`、`layering`、`sdk-boundary` 等）。看出问题就按【代码核查】写下来，写明文件和行号，作者会自己复现。
- 前几路都被外部的内容过滤中断过，所以这一路只谈设计和结构，不讨论怎么让哪条检查失效。
- 可以在当前目录构建和跑测试（构建目录 `build-audit/`），不需要联网。不接触设备，不读 `~/Documents/Workspace/ink-hw/` 的实机数据，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。本次不要调用任何子代理或外部模型，你自己完成。

## 构建

```bash
D=<放 build-deps 的主检出目录>
cmake -S . -B build-audit -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src
cmake --build build-audit -j 4
$D/build-deps/pyenv/bin/ctest --test-dir build-audit -j 4 --timeout 400 --output-on-failure
```

内存检查构建：同样的参数再加 `-DINK_SANITIZE=ON`，构建目录 `build-audit-asan/`。macOS 没有 `timeout` 命令。做反向对照时：本机 make 是 3.81，时间戳只到秒，把源文件改回去之后等一秒再构建。另外两路审计同时在跑，所以 `-j 4`。

## 写法

- 报告用中文；代码标识、命令、路径保持原样。每条结论标明证据类型，推断不得写成事实：**【复现】**（附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- 定级：**严重**：正常使用就会丢用户数据、设备起不来、孩子能进大人的空间、AI 能代替人做只有人能做的事；**高**：特定但现实的时序或操作下丢数据、卡死、上述隔离失效，或者测试和文档给了不实的保证；**中**：出错路径上行为不对、资源浪费；**低**：其他。重点是严重和高，不要为了数量去找。
- `report.md`：一页结论；新问题（定级、标题、证据、位置、影响、建议）；修复的核查（逐条：已修复、部分修复或未修复）；结尾写明对象提交、跑了哪些命令、哪些没查。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；`verification.json`：`[{"id","verdict","evidence","note"}]`。

没做的不算问题：正式界面（现在只有占位），中枢真正的协议，正式词库，`ports/esp32s31`，中枢协议 v0.2 要设备做的改动和确认规则 D-105（设计文档 §15.4 列了），配对、备份、无线传输，D-95 的计时。
