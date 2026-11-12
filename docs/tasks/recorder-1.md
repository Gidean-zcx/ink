# recorder-1：独立的录音机（功能清单 `recorder`，移植队列 M08）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7、11、12 条）。

用户勾的需求：“AI 键一键录音，锁屏也能录，存卡回放”。现在只有语音备忘（`system/apps/memo/`），没有独立录音机。

## 要做的

1. 先找可移植的：原厂 `$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/`、kiikoread 改版 `$INK_DEPS/../ink-hw/upstream/oss/kiikoread-xiugai/`、AgentUI（`AlayaElla/MetalioClaw4-AgentUI` 提交 172ec53 的 `external_apps/examples/voice_recorder`，在线只读）里的录音页面和流程，许可能用就照它的页面和流程做，写 `SOURCE.md`；没有能用的就照手机录音机做。
2. 录音机应用（`system/apps/recorder/`）：录音、暂停、停止保存（存到当前家人的空间，文件名带时间）、列表、回放、删除（走现有确认规则）、改名。复用现有的 Opus 编码和媒体服务接口，不新增录音线程。
3. AI 键长按一键开录（锁屏时也能录，录完回到锁屏），按现有锁屏允许的能力规则接；来电时自动停止并保存。
4. 能力 `recorder.*`（开始、停止、列表、播放、删除），人和 AI 共用；页面快照给录音状态和时长。

## 范围和预算

- 只改：新增 `system/apps/recorder/`、注册应用那一处、`CMakeLists.txt` 和 `ports/esp32s31/main/CMakeLists.txt` 里它自己的几行、锁屏 AI 键路由的那一处、`tests/integration/apps.c` 和 `ai_surface.c` 登记的几行、新增测试、`assets/i18n/en.tsv`（只加新文案）、生成的 `docs/system/capabilities.md`、`docs/tasks/recorder-1/`。不改 `system/services/audio/**`（`music-pcm` 正在改），需要的接口已有就用，没有就在报告写清。
- 胶水不超过 350 行；不新增任务、线程、文件格式。

## 验收

1. 测试：录音、保存、列表、回放、删除、改名；锁屏一键录；来电打断后文件完整；两个家人的录音互不可见；AI 和页面一致；反向对照。
2. 主工程普通全量 ctest 过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小。
3. 截图：录音中、列表，常规和 120%。
