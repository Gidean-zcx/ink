# 电话任务报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。仅限模拟器 AT 模型；实现已接入，正在验证。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | AT 拨出、接通、挂断；接听与拒接各记一条；新进程读回大人 5 条、孩子 1 条 | T1 |
| 2 | 【实测】通过 | 真实联系人编辑页点“选择这个号码”，`contacts.select` 返回后填入拨号盘，再点拨出 | T1 |
| 3 | 【实测】通过，含反例 | 去掉读取时的成员绑定，孩子记录数断言失败；恢复后通过 | T2 |
| 4 | 【实测】通过 | 三页快照能放进 3072 字节 AI 上下文；按钮与 AI 同用 `call.dial` / `call.hangup`；AI 拨号先待确认，AI 不能自己确认 | T1 |
| 5 | 待验证 | 能力注册、全量测试、内存检查、350 行预算 | 待补 |

T1：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(phone-calls|contacts)$' --output-on-failure -j2`，2/2 通过。电话测试五阶段共 251 个断言。

T2：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/phone_calls/reverse.py build`。反例把唯一一处 `ink_db_open_cached_member(&HISTORY, P.member)` 改为固定读大人；3 个隔离断言失败。脚本恢复源码、重新构建后全五阶段通过。证据：`build/phone-reverse-summary.log`、`build/phone-reverse.log`、`build/phone-restored.log`。

## 没做到的

- 全量、内存和反向对照尚未完成。
- 静音、通话键盘、耳机和录音没有可用服务能力；页面如实说明，未伪造操作。
- 本机仅验证 macOS；没有可用 Linux 执行环境，不声称 Linux 已实测。

## 和设计不一致的

- 电话入口从联系人列表改为拨号盘；联系人仍由原页面和 `contacts.select` 提供。
- 为接通现有入口，联系人文件除末尾增补接口外，还修改了应用入口并在选择成功处发事件。这两处不能仅靠末尾注册完成，未加入通用拦截或替换机制。
- 【实测】`tools/ui_compare.py --reuse`：Phone **32.474/255**、PhoneRecents **52.382/255**、PhoneCall **25.851/255**，均超过 12，未宣称达标。输入框为 F2 控件，数字键按现行主题 163；列表为 F4 四行，页码在列表下方；通话页显示实际号码及缺失能力的文字，没有画板的联系人头像、姓名与四个可用按钮。现行主题的白底、`#888888`、1.2 倍尺寸也与旧画板不同。

## 改过的已有测试

- `tests/contacts/contacts.c`：原入口断言改为电话入口能打开，再调用 `contacts.open` 进入联系人；原联系人 CRUD、选择、隔离和确认断言保留。
- `tests/integration/ai_surface.c`：在 AI 预期清单追加 8 个新操作，核查 `phone.open` 的可选号码参数、`phone.key` 的必需参数 `key`，并追加 3 个事件注册断言。原断言不变；首轮漏登导致 10 个断言失败，补登后实测通过。

## 要用户定的

- 暂无；按任务书既有确认规则实施。

## 顺手发现

- 【实现】既有 `call.dial` 只限制号码长度，没有校验字符；本任务未改通信参数验证策略。
- 【实测】默认 Python 缺少中枢测试依赖，首轮有 3 项跳过；改选依赖检出中现成的中枢 Python 后，`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 全部通过，未安装或升级依赖。

## 新增的机制

- `phone.calls` 成员数据集合：任务书要求保存通话记录；复用数据服务现有 JSON 日志格式及默认 256 KiB 限额，不自动删除历史。
- `call.dialed`、`call.connected`、`contacts.selected` 事件：让电话页面接收到已有能力的实际结果，接通事件来自模拟 AT 的 `+CLCC`。
- 无新增常驻任务、线程、后台重试、队列、缓存、配置项或独立文件格式。

## 要改的文档

- 按总规则第 7 节，主会话合并后运行 `python tools/capabilities.py` 更新共享能力表；本分支只在 `build/phone-capabilities.md` 生成核查副本。源码与运行时校验通过：224 个能力、16 项设置，共 285 个定义。
- `docs/system/ui-dependencies.md`：Phone / PhoneRecents / PhoneCall 可标为模拟器已接线；通话附加控件、姓名头像和画板分数差距仍需保留。共享文件未修改。

## 规模与边界

- 当前产品改动 **274 行新增、2 行替换删除，共 276 行**（含 CMake 接线及空行），低于 350 行。超过 100 行的原因是三页、F1/F2/F4 接入、事件归属与落盘缺一不可；更小的纯占位页不能满足验收，没有加入通用框架。
- 服务增量仅为：通信服务发布拨出/接通事件、模拟蜂窝 HAL 识别外呼接通的 `+CLCC`、联系人选择成功事件与页面入口。未改模组固件或配置路径。
- 【实现】页面只经能力总线执行通信，记录只经成员数据接口；BG 订阅负责落盘，UI 订阅负责页面。无模拟器头文件进入产品代码。
