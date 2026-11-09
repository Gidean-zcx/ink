# remote-app：网页的几种状态，以及翻页、长按、打字

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/remote-app/REPORT.md。

## 要做的

预算 350 行设备端产品代码，中枢端另计、尽量少：

- `RemoteLoading`：打开网页后第一屏最多等 1.5 秒，等的时候显示这页。
- `RemoteOffline`：连接断了；5 分钟内回得来就自动接上，回不来如实说。
- `RemoteAddress`：输网址（用系统键盘），去网页。
- `RemoteType`：网页上要打字时，用系统键盘打，**打完才发给网页**。
- `RemoteBlocked`：中枢说这个网站不让开时显示（孩子默认不开网页，D-94 待选，先照“孩子看不到网页”做）。
- 补输入：现在设备只有点按。按协议补上翻页（物理键）、长按、提交文字，设备端（system/apps/remote/remote.c）和中枢端（hub/）一起改，配中枢测试。

## 从哪里看起

- 画板 `$INK_DESIGN/png/` 里的 Remote、RemoteLoading、RemoteOffline、RemoteAddress、RemoteType、RemoteBlocked；规格 docs/product/system-ui-spec.md 第 7.10 节。
- 现有的远程画面：system/apps/remote/remote.c 开头的注释写清了上下行格式（区域帧、额度、点按）；中枢设计 docs/hub/hub-architecture.md 的 §5.7 和 §13 表里“远程画面补翻页、长按、提交文字”一行讲还缺的输入。
- 键盘和输入框：system/services/text/ 与 `ink_textfield_*`（system/include/ink/sys/textfield.h）。

## 边界

- 不改区域帧和额度的格式；新输入在现有“上行输入”里加类型，旧的点按不变。
- 打的字在打完前不离开设备；密码类输入框照系统的密码规则（AI 看不到）。
- 同时在做的有 settings-b（修复中）、kid-2、transfer-pages、activity-card、fw-f8、storage-usage，不要改它们的文件；共用文件只在末尾加行。

## 验收

1. 模拟中枢慢于 1.5 秒时先显示 `RemoteLoading`，画面到了再换；断线显示 `RemoteOffline`，5 分钟内恢复自动接上（测试，用模拟时钟）。
2. `RemoteAddress` 输入网址后中枢收到打开请求；`RemoteType` 打字过程中中枢收不到任何字，按完成后一次收到（测试，反例：边打边发，测试失败）。
3. 翻页键、长按都发到中枢，中枢测试能收到并处理（设备端测试和中枢 pytest 各一）。
4. 被拦的网站显示 `RemoteBlocked`；孩子看不到网页入口，直接调用被拒（测试）；各页有 AI 快照。
5. 全量测试（含中枢测试）和内存检查通过；设备端产品代码不超过 350 行。
