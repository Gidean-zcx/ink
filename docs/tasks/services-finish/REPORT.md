# 系统服务收尾报告

2026-11-05，第 4 轮；基线 `cfc6726`，开工工作区干净。[复查报告](REVIEW.md)原文保留。R4-01 已复现并修复，同时修复同类的应用状态、媒体断点、通知及偏好、活动记录和卡上文件生命周期问题。当前复查范围没有已确认的严重、高、中遗留项；低级 AUD18-404 保留。代码提交 `7546918`、`7defb76`。以下【实测】均为本轮；继承项重跑正向，本轮变更另做反向，不把历史反向说成本次执行。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R4-01 高（承接 R3-01/R2-01 的生命周期缺口） | 已修 | 【实现】阅读位置携带打开书时的成员代次；存储在登记写者后才复制和入队。登记与删除隔离共用服务锁；有在途写者时保留编号，已入队工作仍由原队列检查阻止清理。【实测】真实阅读器的销毁、定时、校验后入队前暂停，共 9 分支、132 检查；普通换人恢复第 3 页，删除不复用不回写，复用为孩子恢复第 1 页。初始旧构建复现 offset=1488 和页码 3，反向仍触发旧阅读器断言 | V2/V3 `reader_late` |
| R3-01 高 | 已补全 | 【实测】原校验前暂停的销毁/定时六分支、88 检查原样通过；校验后的缺口由 R4-01 修复 | V2/V3 `reader_late` |
| R4-01 同类：写者与集合引用 | 已修 | 【实现】成员登记先于集合打开，成功后由数据库引用接续保护；普通存储的复制、借用、追加、删除均登记，拒绝不存在或隔离的编号。【实测】双写者、隔离后拒绝、未释放编号、释放后重试、旧代次、集合引用和队列满后的登记释放，共 34 检查；旧成员实现 4 项失败 | V2/V3 `member_writes` |
| R4-01 同类：应用状态写入和删除 | 已修 | 【实现】保存及空状态删除均携带原代次，编号失效后结束旧状态保存。【实测】合成应用经真实注册/打开与保存路径，换人、删除和孩子复用共 85 检查；旧保存会重写成人状态，旧删除会删掉孩子的新状态 | V2/V3 `late_writers` 的 `app_write/app_remove` |
| R4-01 同类：媒体 Flash/卡断点 | 已修 | 【实现】集合打开绑定播放代次，卡缓存写入从登记到结束保持成员生命周期。【实测】真实断点保存函数的两条路径共 68 检查；旧源码复用时写入成人集合，卡缓存在删除不复用和复用后均重现 | V2/V3 `late_writers` 的 `media_flash/media_card` |
| R4-01 同类：通知及渠道偏好 | 已修 | 【实现】收件箱持有者记录代次，直接/延迟投递、偏好读取及保存均使用绑定代次的集合入口。【实测】真实投递与渠道设置入口共 70 检查；旧源码向复用编号写入成人通知和偏好 | V2/V3 `late_writers` 的 `notify/notify_prefs` |
| R4-01 同类：活动待记槽 | 已修 | 【实现】延迟活动记录使用槽内原代次打开集合。【实测】真实 flush 函数与 activity.list 回读共 35 检查；旧源码将合成成人活动归入孩子空间 | V2/V3 `late_writers` 的 `activity` |
| R4-01 同类：卡上短写与打开句柄 | 已修 | 【实现】清理成员卡目录与文件修改共用原有修改锁；句柄归属取已解析物理路径，隔离成员不能再解析个人路径。【实测】在解析 /me 后、建目录前暂停真实原子写或文件打开，再经真人确认删除；6 分支共 60 检查。短写先结束再清理；未关闭的原成员句柄阻止释放编号，关闭后可重试。旧源码四个删除分支失败，含删除后文件重现、句柄被记到新成员 | V2/V3 `file_late` |
| R3-02 高 | 保留修复 | 【实测】满额 Wi-Fi 保存失败不给删除票据，正常保存/更新及旧撤销通过，23 检查 | V2 `wifi_undo` |
| R3-02 同类：闹钟/提醒版本 | 保留修复 | 【实测】稍后再响、重复提醒推进、未修改项和表内移动，21 检查 | V2 `undo_schedules` |
| R3-02 同类：撤销票据归属 | 保留修复 | 【实测】延迟提供者绑定原调用成员/代次，不能取得孩子授权，12 检查 | V2 `undo_owner` |
| R2-01 严重 | 原清理保留，生命周期补全 | 【实测】真实闹钟/提醒、成员集合与原始文件、缓存、嵌套/应用卡文件、任务和迟到写者；删除、复用及重启通过。写者间隙另见 R4-01 各行 | V2 `member_inventory removal_alarm removal reader_late late_writers file_late` |
| R2-02 高 | 保留修复 | 【实测】请求号绑定成员代次、来源、能力和参数；在途重复、迟到回执和关闭空间，23 检查 | V2 `replay` |
| R2-03 高 | 保留修复 | 【实测】记录、文件、通知、时钟和 Wi-Fi 修改后旧撤销不能覆盖新内容；未修改控制仍可撤销，37 检查 | V2 `undo_data` |
| R2-04 中 | 保留修复 | 【实测】空白卡删除未使用成员；卡/Flash/缓存占用失败仍保留编号 | V2 `empty_removal removal` |
| AUD18-106 中 | 保留修复 | 【实测】失败升级历史迁移、两个失败版本跨重启、人工重试规则 | V2 `update_history` |
| AUD18-206 中 | 保留修复 | 【实测】面板拒绝提交不能声称关机图已显示，恢复后正常关机 | V2 `offpanel`；V1 `integration-power-menu` |
| AUD18-207 中 | 保留修复 | 【实测】长选区窗口、右选择标记、拼音点击映射，7 检查 | V2 `text_window` |
| AUD18-402 中 | 保留修复 | 【实测】普通/数字输入快照带目标与版本，凭据内容掩码，16 检查 | V2 `keyboard_snapshot` |
| R1-01 高 | 保留修复 | 【实测】升级正常与有效槽提交前/后断电、提交失败四链，16 次启动 | V2 `upgrade_crash` |
| R1-02 高 | 保留修复 | 【实测】缓冲 TTS/听书交接，同成员锁屏听书 | V2 `playback`；V1 `integration-media` |
| R1-03 高 | 保留修复 | 【实测】1,600 次动作、单成员活动积压与恢复，31 检查 | V2 `activity` |
| R1-04 严重 | 保留修复 | 【实测】权限、儿童上限、确认/拒绝/换人和权限撤销，26+30 检查 | V2 `permissions policy` |
| R1-05 高 | 保留并补全 | 【实测】卡/Flash/缓存/唯一 PIN 四种故障，12 次启动；在途写者另见 R4-01 | V2 `removal member_writes file_late` |
| R1-06 中 | 保留修复 | 【实测】设置连续撤销、过时版本和过期，13 检查 | V2 `undo` |
| AUD18-101 高 | 保留修复 | 【实测】不可读文件根不报清理成功 | V2 `files` |
| AUD18-102 高 | 保留修复 | 【实测】目录和短写持卡，USB 独占不能穿透 | V2 `files` |
| AUD18-103 中 | 保留修复 | 【实测】作业先完成仍正确结束任务 | V2 `files` |
| AUD18-104 高 | 保留修复 | 【实测】状态写入失败不切槽，缺状态不标有效 | V2 `update upgrade_crash` |
| AUD18-105 高 | 保留修复 | 【实测】资源恢复事务不被新包覆盖 | V2 `update` |
| AUD18-107 中 | 保留修复 | 【实测】正常包及八类坏包 | V2 `update` |
| AUD18-108 中 | 保留修复 | 【实测】下载绑定原包身份，两条 offer 变化链 | V2 `update` |
| AUD18-109 高 | 保留修复 | 【实测】遗忘成员集合不破坏独立定时器 | V2 `storage` |
| AUD18-201 高 | 保留修复 | 【实测】拥堵时停止采集/上传 | V2 `audio voice` |
| AUD18-202 高 | 保留修复 | 【实测】OOM 后不能重做旧文本 | V2 `text` |
| AUD18-203 高 | 保留修复 | 【实测】录音结束且持久完成后关机 | V2 `shutdown` |
| AUD18-204 高 | 保留修复 | 【实测】耳机掉线不外放旧媒体 | V2 `audio` |
| AUD18-205 中 | 保留修复 | 【实测】通话结束保留闹钟焦点 | V2 `audio` |
| AUD18-208 中 | 保留修复 | 【实测】AI 不能提交隐藏字段 | V2 `text` |
| AUD18-209 中 | 保留修复 | 【实测】初始 UTF-8 截断保留完整字符 | V2 `text` |
| AUD18-401 高 | 保留修复 | 【实测】字段、内容、光标的过期会话/版本均拒绝 | V2 `bindings` |
| S6-01 高 | 保留修复 | 【实测】旧事件缓冲和空间关闭边界 | V2 `handoff replay` |
| S6-02 高 | 保留修复 | 【实测】采集、迟到帧和缓冲播放隔离 | V2 `isolation playback` |
| S4-DET 中 | 保留修复 | 【实测】栈计量两套均 0 失败，确定性测试通过 | V1；V2 `stack_meter.py` |
| S1 | 本轮范围通过 | 【实测】复查唯一高问题及同类缺口已修并做反向；原严重、高和中逐项重跑正向，见上表 | V1/V2/V3 |
| S2 | 保留修复 | 【实测】真人请求、非高风险设置、儿童权限、人工确认和撤销回归通过，未放松凭据及确认绑定 | V2 `permissions policy undo undo_data undo_owner` |
| S3 | 保留修复 | 【实测】真实能力表 actions 6 检查、AI 场景原预算通过 | V2 `actions`；V1 |
| S6 | 本轮模拟器范围通过 | 【实现】生命周期归成员服务，持久化归存储/数据/文件服务；应用和 AI 继续走共同能力、结构化回执与策略；没有 UI 直接改服务状态，没有新增用户动作。【实测】成员数据清单、删除和换人回归通过 | V2/V3 |
| S4 | 通过 | 【实测】普通 86/86（40.69 秒）、ASan/UBSan 86/86（68.47 秒）；每套 35 组回归、109 次启动、1,258 项检查通过；低电量已保存分支各 5 检查，栈计量各 0 失败 | V1/V2 |
| S5 | 通过 | 【实测】26 个相关差异文件均小于 5 MB；隐私匹配 0、空白与相对链接检查通过；预算及受保护文档未改，新增/修改 C 代码没有 libc write/read/fgets 调用 | V4 |

成员数据及同类入口复核（【实现】检查集合定义、原始存储调用、成员路径及延迟持有者；【实测】为对应回归，不等同于所有并发窗口或物理擦除证明）：

| 数据/持有者 | 本轮结论与证据 |
| --- | --- |
| 五类成员集合：activity.log、notify.inbox、notify.prefs、text.user_freq、media.positions | 目录/集合缓存统一清理；集合打开登记与引用接续，迟到业务写者携带原代次；文本会话持有引用；`member_inventory removal_alarm storage text late_writers` |
| 阅读进度、应用 .state、自定义集合/原始小文件 | 存储复制/借用/追加/删除统一登记；延迟阅读器和应用状态携带原代次；`reader_late member_writes member_inventory removal_alarm late_writers` |
| 成员和应用卡目录、录音及未完成文件 | 清理与修改共锁，文件句柄绑定解析时的物理成员；目录句柄、文件作业的原代次保护保留；`file_late files removal_alarm shutdown` |
| 全局可重建缓存，特别是 /cache/sys/media-pos | 原全目录清理保留，媒体延迟写入登记；`member_inventory late_writers` |
| 设备集合 time.alarms 中按 owner 的闹钟/提醒 | 原持久/运行期清理、RTC 重排和等待落盘保留；`removal_alarm undo_schedules` |
| member.table 的身份、角色、PIN、失败次数与等待状态 | removing 持久隔离，写者/引用/作业或 I/O 失败保留编号，清完并持久删除身份才释放；`removal member_writes file_late` |
| 任务、确认卡、撤销票据、去重/在途回执、事件、PIN 作业 | 原成员/代次隔离与清理保留；`permissions policy replay undo_owner removal_alarm handoff` |
| 剪贴板、输入与编辑历史、AI/听写/播放/录音、应用页面 | 原换人收尾保留，页面及媒体补存入口已补保护；`text bindings voice audio playback isolation reader_late` |
| 设备设置、Wi-Fi、蓝牙、升级历史、共享书籍、无 member 字段的通信 RAM 暂存 | 当前为设备共享；没有冒报为随成员删除的数据，也没有擅自改变归属 |

V1：两套按[总规则 §5](../codex-rules.md#5-构建和测试)完整配置，内存构建仅增加 INK_SANITIZE=ON。下列命令串行执行，均退出 0；构建最多 -j4，ctest 最多 -j3，没有同时构建或同时跑两轮全量：

```bash
cmake --build build -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
```

V2：两套均执行，第二套给两个脚本加 --build build-asan；以下均退出 0：

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash removal replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel reader_late member_inventory wifi_undo undo_schedules undo_owner late_writers member_writes file_late
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py
```

正向证据目录：普通 `build/services-finish-eexni0vh/`，内存 `build-asan/services-finish-lx2zukjq/`。每套全部 109 次启动无功能失败；保存低电量分支及栈计量另有独立目录，见日志末行。

V3：初始旧构建运行新增 reader_late 入队前暂停分支，普通换人通过；删除不复用 1 项、复用 2 项功能断言失败，证据 `build/services-r4-reader-before.log`。之后两套均执行下面命令（内存构建加 --build build-asan），脚本均退出 1（预期）：

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline cfc6726 reader_late late_writers member_writes file_late
```

【实测】每套 37 次启动均编译/链接成功、模拟器退出 0、test.finished=1；14 个分支触发合计 20 项功能断言失败，无 sanitizer 诊断。阅读器复用分支 2 项；旧应用状态写入/删除、媒体集合、卡缓存、通知、偏好、活动记录合计 8 项；旧成员实现 4 项；旧文件实现 6 项。普通换人控制均通过。

【实现】反向仅从 Git 读取旧源码到忽略的构建目录；替换阅读器、应用、媒体、通知、活动、成员及文件的对应源码，共用存储和数据底层保留新版。因此部分不复用分支已由新版底层拒绝，不把它们说成旧业务本身安全。旧成员接口适配仅保留原代次比较、不登记写者，不伪造存储结果或关闭断言。媒体/活动是实际保存函数的边界探针，合成输入；应用实际注册并打开；删除使用真实能力和真人确认接口注入，没有声称模拟屏幕点击或自然触发频率。

反向证据目录：`build/services-finish-tog1zayg/`、`build-asan/services-finish-fonn9uzv/`。全部最终日志位于 `build/`、`build-asan/`，文件名为 `services-r4-configure-final.log`、`services-r4-build-final.log`、`services-r4-ctest-final.log`、`services-r4-regressions-final.log`、`services-r4-saved-final.log`、`services-r4-stack-final.log`、`services-r4-reverse-final.log`；命令退出码和耗时在 `build/services-r4-validation.json`。

【实测】两套 PQBN AI 场景内部总 RAM 最低空闲均 32,880 字节，原门槛 32,768，余量仍为 112；内部失败分配均 0。没有新增锁、定时器或常驻任务，没有抬高预算，也没有关闭确定性比较。

V4【实测】：核对 git diff --stat cfc6726..HEAD；git diff --check cfc6726..HEAD 退出 0；按总规则原隐私正则扫描 git diff cfc6726..HEAD，匹配 0。改动文件均小于 5 MB，相对文件链接有效；没有构建产物、受保护资料、预算或设计稿夹带。闭合检查记录 `build/services-r4-closure.json`。

## 没做到的

- AUD18-404 是低级能力文档漏项，按用户范围保留；当前复查没有未修的已确认严重、高或中问题。
- 只有 macOS 模拟器实测；没有 Linux 执行环境、真机、真实中枢或窗口外观验证。修改未新增 libc write/read/fgets，不能据此冒称 Linux gcc 已通过。
- 全量中的既有 gui-smoke 使用 SDL dummy，没有启动带窗口的 ink-sim-gui。

## 和设计不一致的

- 保留既有边界：文件撤销按全卡修改序号保守失效；失败升级历史有 128 KiB 上限，失败保守拒绝；成员删除遇在途写者、引用、作业或 I/O 失败仍需重试并保留 removing 编号。新增登记沿用这一失败/重试约定。
- 撤销和运行期成员代次不承诺跨重启有效；成员持久记录格式未改。不承诺 RAM/Flash 的物理取证擦除，没有修改设计文档迁就代码。

## 改过的已有测试

- reader_late.c 仅增加校验后暂停入口，原销毁/定时六分支与全部原断言保留；reader_boundary.c 同时包装旧、新保存 API，最终仍调用真实存储。
- run.py 增加三组回归、阅读器入队前分支及旧应用/媒体源码替换；原组别、预算、断言和等待阈值保持。members_boundary.c 增加旧接口适配，供反向编译，不替代业务结果。
- 新测试用显式扣住/释放任务固定并发窗口；file_late 中有界等待让旧清理先完成，以暴露文件重现。产品未加 sleep，没有放宽或删除已有测试的期望。

## 要用户定的

无；没有采购、硬件、付费、远端写入、push、PR 或发布。

## 顺手发现

- 同类卡上短操作/句柄归属缺口已复现、修复并做正反对照，见验收表；未扩展成其他功能重构。
- 历史报告的修复与反向证据保留在 Git 历史；本次不把所有原问题重复计为新发现，也不把合成哨兵或单个时序探针说成全产品并发已证明安全。
