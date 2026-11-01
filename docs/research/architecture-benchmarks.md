# 架构对标：同类设备怎么建，我们借什么、避什么

整理：2026-11-01。本页是[底座专题研究](foundation/README.md)的结论汇总，详细证据、代码位置与来源见各专题报告。标注：**有据** = 代码、官方文档或可靠报道；**推断** = 本项目的分析。没有任何实机测试。

## 1. 最值得记住的五条

1. **范围决定生死（有据 + 推断）。** Mudita Pure 是唯一做到量产的 MCU 墨水屏手机：约 35 万行系统代码，更新日志 252 条修复对 28 条新增，约 3000 台后停更，下一代 Kompakt 改用 AOSP。CrossPoint Reader 用一份 SCOPE.md 明确拒绝浏览器、主动联网和 PDF，才在 ESP32-C3 上把阅读做稳。→ 本项目：薄设备、厚中枢；写“永不做”清单；通信最后做。
2. **刷新必须统一裁决（有据）。** KOReader 的 UIManager 让应用只提交“区域 + 刷新类型”，由调度器合并、升级、定期全刷；同一语义类型在不同设备映射到不同波形。Mudita 与 CrossPoint 在 MCU 上做了简化版。→ 本项目的刷新调度器照此设计（[底座设计 §4](../system/foundation.md)）。
3. **一个前台、一个渲染任务、少量服务、只用消息（有据）。** CrossPoint 把每页一个渲染任务改为全局一个，消除了生命周期类崩溃；Mudita 规定服务之间只走总线消息，但它的启动“没有失败兜底”、电源管理“只是第一次迭代”。→ 本项目：服务间禁止同步阻塞，启动可降级，低功耗恢复路径在架构期就验证。
4. **AI 要的是语义，不是像素（有据）。** Apple App Intents 把视图关联到实体，Android 用视图树与 AppFunctions，Windows 用 MCP 注册表；目标板的上游固件虽然已是 MCP server，却只开放音量、亮度、状态三个工具。→ 本项目：动作注册表同时服务按钮与 AI，每个页面提供语义快照。
5. **名字相同的动作，结果常常不同（有据，来自 65 组竞品 996 个任务）。** 保存、删除、恢复、同步、继续、完成在不同产品里含义各异。→ 本项目：对象、作用范围、阶段、回执在数据层显式化（[底座设计 §5](../system/foundation.md)）。

## 2. 逐项对标

| 领域 | 谁做得好（有据） | 谁踩了坑（有据） | 本项目怎么做（推断/建议） |
| --- | --- | --- | --- |
| 系统范围 | CrossPoint SCOPE.md；Light Phone 的“工具”模型与审核制 | Mudita Pure 范围过大，崩溃与卡死修复占满工程力量 | 固定 7 个工具；生成、转换、AI 全在中枢 |
| 刷新 | KOReader 刷新类型与合并、每 6 次 partial 升级；Metalio 原厂滚动时 DU、停下 GL16；CrossPoint 每 15 页半刷 | Mudita：空列表不必要的深刷、信号强度更新触发刷新 | 统一调度器，6 种刷新类型，状态栏节流 |
| 任务模型 | CrossPoint 单渲染任务 + Activity 栈；Mudita 总线消息 | Mudita 弹窗“逐步加进系统”、启动无兜底 | 单前台 + 常驻服务 + 类型化消息；启动可降级 |
| 排版与字体 | Metalio 主机端转 .ebook（运行峰值 RAM < 64 KB）；CrossPoint 页缓存带版本号、按页预热预光栅化 CJK 字形 | EPDiy 示例阅读器只支持拉丁字形 | 中枢转换为设备格式；设备排版并缓存；CJK 字形预光栅化 |
| 位置 | KOReader xpointer + 部分 MD5 文档 ID；CrossPoint（章节，码点偏移）与 KOReader 双向映射；Kindle 同时记“最近”与“最远” | Whispersync 记最远位置，重读时被拉回书尾 | 语义定位器 + 最近/最远并存，远端较新时提示而不静默后退 |
| 持久化 | CrossPoint tmp+rename、墓碑、.bak；Mudita 用 ext4 防断电 | CrossPoint #2275：原地截断写遇断电损坏 FAT | 用户状态在内部 Flash 的日志式存储；临时文件 + 改名 |
| 同步 | KOSync 离线队列（4 周 / 200 条）、区分较新较旧；Kobo 软删除 + 同步脏位 | Anki 多档案连同一账户互相覆盖 | 变更日志 + 中枢确认后才压缩；按成员分区 |
| 多成员 | Kindle Household：最多 4 个儿童档案，家长选书，笔记互不可见，就寝时间与阅读目标 | KOReader、CrossPoint、MuditaOS 都是单用户；PocketBook 档案不隔离文件 | 成员 ID 作为所有个人记录的分区键；儿童策略系统执行 |
| AI 接入 | Apple App Intents（实体、确认、撤销、长任务）；Android AppFunctions；xiaozhi 设备侧 MCP | 上游 Plus 只开放 3 个工具，隐藏工具调用时不检查；Rabbit 硬编码 API key | 动作注册表 + 策略闸门在设备执行；密钥只在中枢 |
| 协议 | xiaozhi：WSS + Opus 60 ms 帧 + `type:"mcp"` 信封，设备主动连出适合 4G | MCP 规范多次破坏性修订（2024-11-05 → 2026-07-28） | MCP 形状的私有协议，中枢翻译成标准 MCP |
| 生成式界面 | Google A2UI：客户端组件目录 + 扁平组件表 + 数据绑定 | MCP Apps / Apps SDK 依赖 iframe HTML，设备无法渲染；LVGL 9.5 起运行时 XML 需付费许可 | A2UI v0.9 消息 + 自定义墨水屏组件目录 |
| 语音 | xiaozhi 按键说话、abort 打断；分段时延数据（ASR 首词约 0.8 s、TTS 首音约 0.25–0.7 s） | xinnan 服务端按设备存记忆，声纹不用于授权 | 流式管线；按成员存记忆；按键说话 |
| 开发方式 | KOReader `kodev run` SDL 模拟器；Mudita pytest 经 USB 驱动整机并导出 UI 树 | Mudita 模拟器“看不到 E-Ink 刷新” | 模拟器必须显示刷新类型、区域、闪烁与残影累计（见 R-034 模拟器任务） |

## 3. 我们明确不学的

- 不在设备上做通用插件或第三方原生代码（KOReader 插件、NickelMenu 符号挂钩都依赖 Linux 运行环境）。扩展走中枢下发的声明式页面。
- 不照搬 MCP 最新规范到固件；设备协议自管版本。
- 不用声纹做授权；不用硬件按钮代替儿童/成人切换的认证。
- 不把同步当备份。

## 4. 专题报告

- [竞品库对底座的含义](foundation/library-implications.md)（65 组产品的数据对象、状态与恢复模式）
- [墨水屏产品与开源系统架构](foundation/eink-architectures.md)（Mudita、CrossPoint、KOReader、Kindle、Kobo、reMarkable、BOOX 等）
- [AI 原生设备与能力协议](foundation/ai-native-protocols.md)（xiaozhi、MCP 2026-07-28、A2UI、App Intents、AppFunctions、语音时延）
- [上游 Plus 固件架构](foundation/upstream-firmware.md)
- [ESP32-S31 平台与运行时](foundation/platform-runtime.md)
