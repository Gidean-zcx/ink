# batch10 独立质量复查

- 日期：2026-11-10；本轮范围：`5e1e3964..56d814ba51c1d7acabe6e60b8711345de6bd66a5`。
- 方式：只读产品代码和已有测试；临时实验限于忽略的构建目录。只写本报告，不修代码、不提交、不访问设备或付费接口。
- 时间：北京时间 / 新加坡时间 08:21 开始，08:49 完成；未超过一小时。
- 证据标签：【实测】本轮执行；【实现】源码核查；【估算】模型或静态估计。未复现的疑点单列“待证实”。

本轮确认 **1 中、1 低**，未发现有证据的严重或高等级问题。普通版和 ASan/UBSan 全量均 298/298，S31 编译通过；两个独立页面实验发现全量尚未覆盖的回归。范围以指定差异为准，其中还包含 audit7 的通话和 USB 修复，一并检查。

## 已确认问题

### B10-01【中】停留在短信页时，新短信已入库却不显示，退出重进才出现

- 位置：`system/apps/sms/sms.c:205–206`（本批将订阅从 `INK_EXEC_BG` 改为 `INK_EXEC_DIRECT`）；相关路径 `:37–43`、`system/services/comm/comm_service.c:99–104`、`:441`。
- 【实现】`message.received` 的存储订阅仍在后台执行，页面订阅却立即投递 UI 重画。UI 可以先读到尚未写入的新消息列表；后台随后写完，不再触发短信页重画。其他状态栏通知不重建短信列表。
- 【实测】全新合成镜像，打开空短信页并等待稳定，经模拟 AT 模组收到 `SYNTH_SMS_ARRIVAL`；等数据库已持久化、存储和显示均空闲后，页面仍为 `total:0`、空列表。返回首页再打开短信页即为 `total:1`，包含刚才的短信。实验没有暂停线程、占住锁或修改产品代码。
- 复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit10/compile_driver.py build/audit10/sms_arrival.c`；`taskpolicy -c utility build/audit10/sms_arrival --profile simulator/profiles/metalio-plus.json --assets build/assets --content build/test-content --out build/audit10/sms-arrival --flash-image build/audit10/sms-arrival.img --sd-dir build/audit10/sms-arrival-sd --end 30s --quiet --no-random-outages --budget tests/ui_scene.budget.json`。使用未存在的镜像和目录复现；退出 1，`test.fail=1`，失败为“arrived SMS appears without leaving and reopening the list”。日志 `build/audit10/sms-arrival.log`、指标和截图在同名输出目录。
- 【实测，归因对照】仅在构建目录用基线的 `sms.c` 替换对象，其他代码、输入和断言完全相同，退出 0；到达后直接显示 `total:1`。命令和驱动保留于 `build/audit10/`，对照日志 `sms-arrival-baseline.log`。原有 `sms` 测试收完消息后重新 `open_list()`，故本轮 298 项全量通过仍漏掉此回归。
- 影响：用户停留在短信列表时看不到刚收到的消息，AI 页面快照也读不到；消息本身已持久化，本实验没有发现丢失或跨成员泄露。
- 最小修法：把短信页重画绑定到消息入库完成之后，恢复同一后台执行序列或在存储完成点发出既有 UI 更新；同时补“保持页面打开收到消息”的断言。不要仅为截图更快稳定而提前通知。

### B10-02【低】网络设置翻页后，快照仍把另一页的“忘掉这个网络”列为当前动作

- 位置：`system/ui/shell/navigation.c:1458–1463`；按钮实际创建受 `:1246–1253` 的 `block(INK_CONTROL)` 分页条件限制，快照只判断是否有已连网络。
- 【实测】通过真实能力连接合成 AP，将 `display.font_size` 设为 3，打开 `SetNetwork` 后调用 `shell.page_next`。第 2/2 页只有 SIM 卡选择和 4G 状态；遍历真实 LVGL 树找不到“忘掉这个网络”的触摸目标，快照却仍含 `忘掉这个网络：wifi.forget(...)`。截图 `build/audit10/wifi-pages-large/snap-network-second.png` 与同目录 `network-second.txt` 可直接对照。
- 复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit10/compile_driver.py build/audit10/wifi_pages.c --target sim-settings-gaps --source-base tests/settings_gaps.c`；`taskpolicy -c utility build/audit10/wifi_pages --profile simulator/profiles/metalio-plus.json --assets build/assets --content build/test-content --out build/audit10/wifi-pages-large --flash-image build/audit10/wifi-pages-large.img --sd-dir build/audit10/wifi-pages-large-sd --end 40s --quiet --no-random-outages --budget tests/ui_scene.budget.json`。使用全新镜像和目录；退出 1，`test.finished=1`、`test.fail=1`，唯一失败为第 2 页动作与实际目标不一致。日志 `build/audit10/wifi-pages-large.log`。同一实验普通字号下没有分页、退出 0，日志 `wifi-pages.log`。
- 影响：AI 可能引导用户点击当前屏幕不存在的按钮；能力本身仍可调用，确认规则也仍生效，因此定为低，不报安全绕过。
- 最小修法：新增动作沿用实际控件的分页可见条件；若快照需要保留整组动作，应明确标注动作位于第几页。补最大字号翻页的快照 / 触摸目标一致性断言。原 `settings-gaps` 的 `target()` 会自动翻页寻找按钮，不能证明当前页一致。

## 验收表

| 编号 | 范围 | 结论 | 证据 / 命令 |
| --- | --- | --- | --- |
| 1 | 正确性、断电 / 重启 / 换成员 / 拔卡 | 有回归：B10-01；其余已跑用例通过 | 【实测】普通全量 298/298，含索引旧格式、重启、USB / 拔卡边界及成员隔离；见下方命令 |
| 2 | D-105、儿童限制、秘密字段 | 已查路径未发现规则放松 | 【实现/实测】新增按钮沿用原能力；确认卡禁止 AI 自批；通话按键绑定通话代次和对方、快照掩码、事件不带按键；个人词频按成员，密码不学、AI 不能直接读取。`settings-gaps`、`audit7-b7-01/02`、`ime-learning`、儿童与确认回归均在全量中 |
| 3 | R-061、G-002、页面与 AI 一致性 | 接线成立；存在 B10-01、B10-02 | 【实现/实测】拷贝、忘记网络、清空通知均复用能力；蓝牙和指定键盘边界测试通过。短信共同陈旧，网络设置有翻页不一致 |
| 4 | RAM、栈、任务和固定容量 | 已有预算通过，余量很小 | 【实测/估算】见资源段；能力表、订阅表未满，状态订阅剩 10 格 |
| 5 | 本批任务组合与回归 | 指定专项通过，独立实验仍发现两条遗漏 | 【实测】词库过滤与简拼权重合并后 968,735 B，两构建目录逐字节相同；原 90 条首选 83/90、前五 89/90；全拼 71/73、73/73，简拼 12/16、16/16。索引成本见下表。运行时 89 个变更能力、22 个设置与 INVENTORY 逐项一致 |
| 6 | Linux gcc 与 `-Werror` | 本机与 S31 通过；Linux 原生验收未做 | 【实测】Clang `-Werror` 两套全量构建通过；S31 GCC 编译通过。另对 25 个变更 C 文件做 GCC 15.2 `-O2 -Werror` 汇编检查：12 个产品文件、12 个测试文件通过，另 1 个主机测试有交叉目标的类型差异，详见限制 |

## 本轮执行与资源证据

- 【实测】`INK_WERROR=ON` 普通构建成功，`taskpolicy -c utility cmake --build build -j2` 退出 0；`INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` 退出 0，298/298，220.64 秒。配置、构建、测试日志为 `build/audit10/{configure,build,ctest}.log`。
- 【实测】`build-asan` 保留相同配置并加 `INK_SANITIZE=ON`，构建退出 0；上述测试命令换为 `--test-dir build-asan`，加 `UBSAN_OPTIONS=halt_on_error=1`，退出 0，298/298，410.62 秒，无跳过。日志 `build-asan/audit10/{configure,build,ctest}.log`。
- 【实测，模拟器预算】普通全量各指标文件中内部 RAM 最低剩余为 `member-wake-stream` 的 33,200 B，距 32 KiB 门槛仅 432 B；`media` 为 33,424 B。不是实机堆测量。
- 【估算，来自本轮模拟器的栈计量】产品任务最紧的几项：`esp_timer` 3552/3584 B（余 32 B），`main` 4000/4096 B（余 96 B），`power` 2720/3072 B（余 352 B），`exec.media` 7632/8192 B（余 560 B），`ui` 9664/10240 B（余 576 B）。数据、汇总程序在 `build/audit10/resources.{json,py}`，原始证据在 `build/test-out/` 等全量输出目录。这些紧余量在 batch9 已有，不重复计本批缺陷。
- 【实现】UI 分条由 96 行改为 128 行，新增 PSRAM 21,888 B；本批没有新增常驻任务。JSON 解析器仍为 128 token；新增 `files.move(copy)` 只有一个布尔参数，未增加大 JSON 入口。
- 【实测】只在构建目录给实际启动加只读计数：能力 368/512、事件订阅 33/64、状态 48/64、状态订阅 54/64、设置 22/48、设置订阅 7/64、数据库定义 20/32。`build/audit10/capacity.log`；命令 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit10/compile_driver.py build/audit10/capacity.c --target sim-capability-inventory --source-base tests/capability_table/export.c --extra-source system/policy/state.c:build/audit10/capacity_state.c`，再运行该目录的 `capacity` 模拟器。这里是启动注册占用，不冒充所有运行路径的峰值。
- 【实测】`taskpolicy -c utility bash ports/esp32s31/build.sh` 退出 0；未运行任何刷写或设备命令。链接器统计 DIRAM 83,042 B、外部 BSS 507,784 B；这些静态值不包含所有运行期分配。日志 `build/audit10/s31-build.log`，链接与静态栈证据 `build-s31/{size,stack}.txt`。静态栈报告明确不含间接回调等，不能当作完整上界。
- 【实测】隐私扫描指定基线差异，仅命中 `tests/audit9/bt_ai.c` 的规则指定合成地址 `00:11:22:33:44:55`；无本机路径命中。结果 `build/audit10/privacy-scan.json`。两套测试日志无跳过，ASan 日志未检出 AddressSanitizer / UBSan 运行时错误。
- 【实测】结束时 HEAD 仍为指定终点，产品和测试源码无工作区差异；本轮新增的非忽略文件只有本报告，`TASK.md` 是开始时已有的未跟踪文件。`git diff --check`、报告格式与本机路径检查通过；未提交或推送。

### 索引成本复测

【实测，模拟器】沿用既有工具，固定 1000 个合成文件和 seed 1，基线对照只替换文件服务对象，其余代码一致；每次使用工具重建的独立镜像。命令为 `INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_cost/run.py --build build --cases prepare probe control`；基线再加 `--baseline 5e1e3964`。两次均退出 0。

| 项目 | 基线文件服务 | 本批完成版 |
| --- | ---: | ---: |
| 首次追加 | 252,000 B / 4614.355 ms | 92,000 B / 1602.096 ms |
| 压缩重写 | 252,032 B / 4426.503 ms | 92,032 B / 1659.689 ms |
| 与压缩竞争的前台探针等待 | 4449.647 ms | 1665.952 ms |
| 减去各自空载对照 16.742 ms 的额外等待 | 4432.905 ms | 1649.210 ms |

日志 `build/audit10/index-cost{,-baseline}.log`；原始事件、指标、汇总在 `build/test-out/index-cost{,-baseline}/`。这个收益不代表实机耗时；本轮没有重跑工具的一小时组合负载。

## 没做到的

- 未做实机或实网业务验证；本轮禁止接触设备。没有真实电话、短信或付费 AI 调用。
- 未运行 Ubuntu 原生 GCC 构建 / 测试；本机无该运行环境。辅助命令 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit10/gcc_check.py` 使用 RISC-V GCC，退出 1：唯一诊断为 `tests/phone_calls/phone_calls.c:222` 的 `%u` 对该工具链的 `uint32_t`（`unsigned long`）不匹配，改用显式 `(unsigned)` 或对应格式宏可消除。该文件只用于主机测试，不进 S31 产品；这里不把交叉目标的类型别名差异认定为 Linux CI 失败。完整逐文件命令、退出码见 `build/audit10/gcc-results.json`。
- 断电、拔卡、切成员以已有专项和本轮代码检查为界，未穷举所有时序。输入法仍沿用最多 60 秒的延迟写入，突然断电可丢未落盘偏好；`ime-learning` 的重启验证在显式落盘后进行，不代表零丢失承诺。
- 临时驱动、图片、镜像和日志只留在忽略的构建目录，不纳入产品或测试源码。

## 和设计不一致的

- B10-01 不满足新消息到达后页面反映真实状态的要求；B10-02 不满足本轮“页面快照和屏幕一致”的要求。
- 【实现/目视】核对 `$INK_DESIGN/png/{SetNetwork,FileActions,Notifications}.png` 及界面规范：拷贝和清空通知的入口有画板依据，忘记网络的位置有任务书明确授权。没有据此宣称原有页面与画板全面一致，也没有把既有多选等缺口算成本批新增问题。

## 改过的已有测试

本轮没有改任何已有测试。对本批修改的已有测试逐项核查如下：

- `backup_restore/device.c`：导出家庭绑定前增加持久化等待、最多 50 次导出重试；仍要求成功导出和同步写入，没有改变备份验收结果。等待对应导出的持久化前置条件。
- `file_index_3/boundary.c`、`index_durable/device.c`：观测器兼容 schema 3 偏移；旧记录由开机强制升级改为正常索引查询后再验证升级，仍断言原编号、精确长度和 schema 3，符合最终延迟升级要求。
- `ime_lexicon/run.py`：保留原 90 条及原阈值，新增全拼 / 简拼分组阈值；没有用换题掩盖质量下降。
- `phone_calls/phone_calls.c`：原“快照含按键明文”改为“掩码和有值”，契合秘密字段修复；新增旧通话确认卡失效、事件和活动日志隐私断言。
- `integration/ai_surface.c`：`files.move` 从无参数改为接受可选对象，匹配 `copy` 参数；`index_cost/run.py` 新增手动历史对照入口，未把 Git 历史依赖加入默认 CTest。
- 指定差异中没有删除既有用例、抬高预算或降低已有数值阈值。B10-01、B10-02 表明仍需补相应行为覆盖。

## 新增的机制

本轮无。对被审计批次的核查：

- `file.index` 的 schema 3 变长载荷由 index-compact 明确要求；仍走既有数据库帧、存储队列和扫描，不新增迁移任务。完成记录独立接受版本 1/2，不能再写成“随索引升级至 3”。
- `text.user_freq` schema 2 以拼音和词共同定键，复用 512 条 / 2048 B 载荷限额及既有淘汰；简拼先验、每键 32 个候选和字体交集在电脑生成阶段处理，设备查询引擎不变。
- 文件拷贝复用目标目录页；通知和 Wi-Fi 复用原能力、确认规则。电源通知未交付状态纳入 UI 忙判断，没有新增常驻任务或配置项。

## 要用户定的

本轮两条问题均不需要新的产品选择。`settings-gaps/INVENTORY.md` 已列出的时间 / 时区、通知渠道、语音备忘和安全设置等入口缺口仍待主会话处理；本轮不把“待定”改成“不需要”，不替用户决定。

## 要改的文档

- 主会话同步 `docs/system/system-services-design.md` 的索引载荷、完成标记独立版本、输入法学习限额与隐私边界；同步资源说明的 128 行条带和 PSRAM 增量。
- `docs/tasks/index-compact/REVIEW.md` 已将短信订阅调整描述为修复，需结合 B10-01 更新当前验收结论；本轮按只写报告要求保留历史文件不动。
- backlog / UI 依赖状态应保留尚未设计的设置入口，并登记本轮两条问题。共享文档未改。

## 顺手发现 / 待证实

- RAM 和栈的紧余量属于 batch9 已记录的既有风险，见资源段，不重复评级。
- 未发现需要单独评级的其他已证实问题；不将未穷举路径写成“全部安全”。

SEVERE=0 HIGH=0 MEDIUM=1 LOW=1
