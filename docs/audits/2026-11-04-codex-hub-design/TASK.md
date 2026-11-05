# 设计审查任务说明（2026-11-04，家庭中枢设计，交给 Codex）

你是独立的审查方，做的是本项目（用户自己一家用的 AI 墨水屏设备）服务器端的**软件设计审查**。用户的原话：“让 codex 做一版设计，全面性的看看设计有没有什么问题”。

这一轮的对象是**设计文档**，还没有服务实现。设备一侧的代码和模拟中枢是真的，可以用来核对设计。

要回答的问题：

1. 照这份设计开发，能不能做出用户要的东西？哪里会做不出来、要返工、或者上线后出事？
2. 设计说的和仓库里的代码、已定的决策、用户的要求，有没有对不上的地方？
3. 有没有该考虑而没考虑的？有没有过度设计、可以砍掉的？
4. 某处你认为有明显更好的做法，就给出你的替代方案和理由，不要只说“有问题”。

**重点是严重和高**，中、低级也可以记，但不要为了数量去找。定级：

- **严重**：照此开发，主要功能做不出来，或者会丢失、泄露家庭数据。
- **高**：要大改设计或大量返工，或者明显达不到用户的要求（如时延、易装、隔离）。
- **中**：设计有缺口或矛盾，开发时能补，但要先定下来。
- **低**：措辞、引用、小的不一致。

## 1. 先读

1. `CLAUDE.md`、`AGENTS.md`（协作约定；末尾的 hybrid routing 是路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. **审查对象**：
   - `docs/hub/hub-architecture.md`（概要设计）
   - `docs/hub/device-protocol.md`（设备协议）
   - `docs/research/hub-server-survey.md`（调研和费用估算）
   - `docs/decisions/README.md` 的“第九轮”（D-73～D-92）
3. **依据**：
   - `docs/requirements.md`（用户明确的要求，R-055 是这次的题目）、`docs/ideas.md` 末尾、`docs/background.md`
   - `docs/system/system-architecture.md`（§5 软总线、§6 AI、§9 实时）、`docs/system/system-detailed-design.md`（§2～§3、§6～§9、§13）、`docs/system/foundation.md`（§5、§6、§8、§9、§10）、`docs/product/system-ui-spec.md`
   - 决策记录其余部分：`docs/decisions/README.md`
4. **设备代码和模拟中枢**：`system/services/hub/hub_link.c`、`system/services/hub/rt.c`、`system/policy/cap.c`、`system/services/ai/ai_service.c`、`system/apps/remote/remote.c`、`system/util/ink_json.c`、`platform/include/ink/hal_net.h`、`simulator/environment/sim_hub.c`、`simulator/environment/sim_remote.c`，`tests/scenarios/*.hub.json`。
5. **还没合入 main、别的会话正在做的**（用 `git show <分支>:<路径>` 读，设计引用了它们）：
   - `claude/system-services`：`docs/system/system-services-design.md`（§1 审视原则、§3 数据存储、§7 确认卡、§9 文本输入、§11 成员、§12 升级），`docs/process.md` 的“架构审视”
   - `claude/system-layer`：`docs/decisions/README.md` 的 D-65、`docs/requirements.md` 的 R-056、`docs/product/system-ui-spec.md` 的语音输入一段（输入法语音输入）

## 2. 要查什么

每一块都要看，查完就写结论，没问题也写一句。

**A 需求覆盖**：R-055 的四个问题和语音输入（R-056）是否都回答到了；相关的 G-001、G-002、R-006、R-007、R-009、R-016～R-018、R-020～R-025、R-027、R-028、R-030、R-031、R-032、R-037～R-041、R-047、R-048 有没有漏，或者被设计悄悄改掉、判成不要。用户带问号的候选有没有被当成定案。需求和助手建议有没有混在一起。

**B 和现有实现对得上吗**：

- 逐条核对 `hub-architecture.md` §1.3 和 `device-protocol.md` §1、§2 对设备行为的描述（给 `文件:行号`）。写错的算问题。
- 新加的消息（`device-protocol.md` §3）放到现有设备代码上走一遍：词法单元上限、缓冲大小、前缀订阅的数量（`ink_hub_subscribe` 上限 8、`ink_hub_on_request` 上限 4）、会话重新登记、`ai.listen` 的时序、配额、帧路由。有没有“设计说不用改设备，其实要改”，或者根本走不通的。
- 听写 `asr.stream`（§3.11、概要设计 §5.3.1）在 `rt.c` 和 AI 语音的麦克风占用上能不能成立。
- 能用模拟器验证的可以写小实验（见第 3、4 节）。

**C 模块划分和 AI 原生**（用户要求每次设计审查都看）：子系统划分合不合理，是否符合“软总线 + 原子能力”；设计和实现路径是否足够 AI 原生；该开放的能力是否都开放给了 AI 和模型，不开放的理由成立吗。概要设计 §3.3 那张表逐行看。

**D 安装、部署、运维**：一条命令 + 向导是否真的可行，普通用户卡在哪；国内网络；Compose 分档；资源估算（§4.3）是否靠谱；升级回滚（多个 SQLite 文件的一致快照、迁移失败、回滚丢数据的范围）；备份恢复（恢复密钥、演练、搬家后设备还能不能连）；VPS 和 NAS 的判断；证书方案（设备固定公钥 + 网页可信证书）在无域名、未备案时走不走得通；中枢没有公网入口或端口被封时用户怎么发现。

**E 账号、配对和数据隔离**：家庭 → 成员 → 设备的模型；首次配对状态机和设计稿 `FirstBoot` 的文字是否一致；中枢地址怎么来的各条路径有没有死路；认领流程能不能被别人抢先；令牌的发放、丢失、吊销；换机和解绑；一家一库 + `Scope` 的隔离是否够；管理员能看到所有家庭这件事的处理；成人之间、家长和孩子之间的可见范围是否和 D-16、D-23、D-25、D-34 一致。这部分按设计审查的思路，指出设计漏洞和改法即可。

**F 语音和实时**：时延预算算不算得过来（对照模拟器数字和调研）；可取消的流水线；断句和发送节奏与设备下行缓冲是否匹配（去代码里查实际的缓冲）；打断的处理；断线；儿童过滤带来的延迟；听写的节流和失败语义；Python 单事件循环扛不扛得住。

**G 能力逐项**：模型网关（薄路由够不够，工具调用在各家兼容接口上的差异、流式工具调用、缓存）；记忆（提取、更新、删除语义，自己做是否低估了）；内容管线（`.ebook` 转换、朗读对齐、报纸）；远程画面（真实浏览器的差异计算、墨水屏适配、页面安静的判断、输入映射）；同步（版本、去重、恢复、`sync/snapshot` 与设备 `ink_db_*` 接口）；块传输（16 KiB、拉取式、吞吐）；固件分发；任务队列；可观测。哪些工作量或难度被明显低估。

**H 费用和规格**：`hub-server-survey.md` §10.3 的算术重算一遍（用 `experiments/` 里的小脚本）；假设是否合理，有没有漏掉大头（比如工具定义和快照的 token、系统提示、提示缓存的实际命中）；D-07 的预算够不够；费用闸门的语义有没有漏洞。服务器价格按用户要求不查。

**I 开发步骤和验收**（§10）：顺序对不对，依赖有没有倒置；第 1 步的“模拟器连真中枢”可行吗（看 `simulator/` 怎么接网络，虚拟时间怎么和真实服务配合）；每步的验收是否真能说明问题；设备一侧的依赖（§13）有没有漏。

**J 决策项**：D-73～D-92 每项的方案是否真实可选，推荐是否站得住；状态对不对（花钱、不可逆的不能暂定）；有没有漏掉应该问用户的；有没有和已定决策冲突；D-86、D-89 改了底座设计的说法，理由成立吗。

**K 文档本身**：内部矛盾，估算没标估算，推断写成事实，引用错误，三份文档之间不一致。来源可以联网核对，但不要注册、购买、调用付费接口；连不上就写明没核对。

## 3. 规则

- **只读审查**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在** `docs/audits/2026-11-04-codex-hub-design/`：`report.md`、`findings.json`，需要时加 `experiments/`（小脚本和运行说明）。单个文件不超过 1 MB；构建日志不要放进来。
- **边做边写**：现在就建好 `report.md` 的骨架；每查完一块，就把结论写进 `report.md` 和 `findings.json`，不要等到最后一起写；运行可能被中途打断。最后再写“一页结论”。
- 不接触设备，不读 `~/Documents/Workspace/ink-hw/` 的实机数据，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌、密钥。
- 不要写中枢的实现代码（小实验脚本除外）。

## 4. 需要跑模拟器时

依赖已在本机下载好，用下面的路径，不要联网下载：

```bash
D=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=$D/build/_deps/imgui-src \
  -DFETCHCONTENT_SOURCE_DIR_SDL2=$D/build/_deps/sdl2-src
cmake --build build-audit -j 8
```

不是必须跑。构建目录不要放进产出。

## 5. 写法与格式

- 报告用中文，说人话；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**（文档路径和小节）、**【推断】**。
- `report.md`：
  1. 一页结论：这份设计能不能作为开发依据；有几个严重和高；最该先改的五件事。
  2. 按 A～K 逐块写结论。
  3. **问题清单**：编号 HUB-001 起，每条含定级、标题、证据、位置、影响、建议（有替代方案就写替代方案）。
  4. 结尾写明审查对象提交（`git log -1`）、看了什么、跑了什么、哪些没查（以及为什么）。
- `findings.json`：数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation"}`。

## 6. 完成后

最后的回复里给出：问题数量（按严重度）、最重要的五条、报告路径。
