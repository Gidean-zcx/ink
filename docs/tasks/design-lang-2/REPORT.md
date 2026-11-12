# design-lang-2 执行报告

基线：`15589e0fe20fd856c0ee93aeae01ae4e6218eddc`。本地分支：`claude/codex-design-lang-2`。仅本地提交，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 普通全量及指定 ASan | 首轮全量 10 项失败；新增回归已修并定向通过，原两项按三次上限保留；最终全量和 ASan 待跑 | `build/design-lang-2/ctest-full.log`、`full-regressions.log`、`contacts-final-3.log` | 见下方命令 |
| 2 S31 宿主机及固件 | 待执行 | — | — |
| 3 共用控件 108×108 | 【实测】共用部分 0 个不足；211 个页面自绘小目标单列 | `build/design-lang-1/after/targets.json` | `tests/design_lang_1.py --build build --check-targets` |
| 4 30 页截图及失败逐项说明 | 【实测】最终代码 60 个场景的文字及页面修复检查为 0，72 张图已覆盖；逐项说明如下 | [对比图](../design-lang-1/SCREENSHOTS.md)、`capture-final-code.log` | 见下方截图命令 |

## 没做到的

- `i18n-lang` 第三轮仍失败：重启阶段还按旧首页同时核对八个设置项，第二页的锁屏、存储、关于触发三次断言。切换阶段已经通过。本任务按总规则第 7 节“三次还不过就停”保留失败，不继续修改这个测试。
- `files-app` 第三轮仍失败：新增第二页的 `FileMoveLong-2`、`FileListLong-2` 截图名未被原有快照名称检查识别，各一次；写入和重启验证已过。达到同一三次上限，保留失败。
- 本机 Linux CI 未运行；实机触摸、灰阶和固件运行未验证。

## 和设计不一致的

- 设置首页按新行高分两页，文件大字每页三行，月历下的日程每页一条；没有恢复旧容量、缩字或缩小点按范围。
- 本次截图左侧是任务基线 `15589e0f`，右侧是本次修正；不再沿用前任务更早的左侧画面。
- 已目视查看 12 张总览，并放大核对主屏、今日、联系人、短信、拨号、通话、月历、关于和键盘；最大字号设置值仍可能按原单行合同省略。没有修改设计规范迁就实现。

## 改过的已有测试

| 测试 | 改前 → 改后及依据 |
| --- | --- |
| 阅读 continuous / rapid 脚本 | 228dp → 284dp，点击新书架第三行《测试长书》。原画面证实旧坐标打开了 18 页的《星星邮差》，所以只能翻 17 次。36/60 次、2.5 秒等预算未改。 |
| 阅读 empty 脚本 | 92dp → 150dp，点击新书架第一行；旧位置在行外。拒绝翻页和两次稳定打开指标未改。 |
| `confirm_pages`、`comm_ux2`、`transfer_pages` | 查找带空格的数字 → 严格核对 1～5 个圆点的数量和选中页，更多页核对无空格数字；仍从真实下一页按钮点按。 |
| `files_ops` | 行框 2 → 0；保留勾选标记、无额外 outline、无划线与选中数量断言，符合新语言去外框。 |
| `factory_pages` | 圆底图标 → 透明图标；黑分隔线 → 中灰 2。仍要求恰好四个图标、两个分隔线、一个真实开关。 |
| `files_app` | 大字仍四行 → 三行；缩略图按三行定位；长目录由四项同页改为两页 3+1，仍检查四个完整路径的并集。后者还有上述快照名失败，未声称通过。 |
| `i18n_lang` | 八个设置项同屏 → 精确两页的完整并集；第一页返回后检查当前语言。重启阶段遗漏的旧假设保留为失败。 |
| `audit10-b10-02` | 固定两页且忘掉网络必须在第一页 → 遍历真实页数，逐页要求可见按钮与两个 AI 快照的动作完全一致，且只有一页有此动作；最后真实点按验证本人确认。 |
| `calendar` | 日程首屏两条 → 一条、三条分三页全部读取；六行月份改从可见“下一页”翻到 31 日，新增默认、孩子和最大字号各页完整文字/触控检查、页边界及 AI 选日自动定位检查；分钟刷新上限和像素裁剪边界 62 → 主题实际状态栏 108，实测 DU 为 108×684，未允许刷新进入正文。 |
| `kid_3` | 固定减去 134 的大图标高度 → 读取紧凑图标的实际点按矩形，要求与时间说明留有主题间距；没有放宽实际相交检查。 |
| `ui_list_paging` | 空内容仍显示单页点 → 空状态可见且页码隐藏；非空单页的圆点检查保留。 |
| `ui_scale_check` 及回归 | 识别 `LV_MIN` / `LV_MAX` 的两个尺寸参数，并逐一检查；新增“任一参数 123 必须失败、两个主题值必须通过”的对照。没有允许任意数值。 |
| `design_lang_1` 截图探针 | 新增主屏圆点分离、关于无孤立侧框、完整输入框与联系人/拨号不相交、短信空页无页码检查。点按检查完整保留原始不足清单，只单列任务排除的页面自绘目标。 |
| `music/product` | 新增列表行必须在目录及翻页按钮上方的检查，防止只检查屏幕边界漏掉重叠。 |
| `settings_a` | 从已删除的左右半框判断连续行 → 按实际相邻矩形判断，并强制无孤立左右框；连续行仍必须有共用分隔线且无空隙。 |
| `small_gaps_2/phone` | 163×163 → 宽仍为 163，高按输入框剩余空间分配、且严格在 108～163；保留错误说明与全部十二个键不相交检查。 |
| `contacts` | 默认每页四条 → 三条；四条变 3+1，五条变 3+2；重启后翻到真实第二页检查修改过的联系人，数量、唯一 ID、动作、成员隔离和恢复检查保留。 |
| `calendar_events`、`time_settings` | 两条同页 → 真实翻页读取第二条；时区投影和可听/静默事件、月份标记、跨日语义断言保留。 |
| `ime_keyboard1` | 密码框去掉的顶部高度改为完整候选/编辑保留区；普通候选与两行编辑工具保持同一键盘区域，`ui_input_host` 和快照的固定区域断言没有放宽。 |
| `files_app` 的输入解析 | `%u` → `SCNu32`，匹配 `uint32_t` 的实际类型；只修 GCC 的格式类型诊断，没有改变断言或结果。 |

## 要用户定的

外观仍需用户看图验收。两个未通过的测试需主会话按上文接续；本任务没有选择新的产品范围。

## 顺手发现

- `apps/` 中背景阅读页的目录文字链接是页面自绘，100%/120% 热区高 43/52，任务明确禁止修改该目录；与键盘、月历小格一起单列，不算共用控件通过项。
- 键盘只改底行权重、两行编辑栏与统一的顶部保留区；普通输入不因显示编辑栏而改变安全边界，没有重写键盘。合并以 `keyboard-2` 为准。`transfer_pages.c` 未改，只更新其测试的页码识别。

## 新增的机制

- 月历增加三个内存分页计数及 `calendar.grid_prev` / `calendar.grid_next` 两个原子导航能力，复用现有重画、AI 快照和能力总线；由任务第 1 条“放不下就分页”要求。没有增加服务、线程、持久格式、设置项、缓存、队列或重试。
- 扩充既有截图探针及手动反向对照脚本：任务验收 3、4 和总规则第 3 节要求；不进入固件。

## 要改的文档

主会话需重新生成 `docs/system/capabilities.md`，纳入月历两项导航能力，并同步 `docs/system/ui-dependencies.md` 的页面接入状态，以及前任务 `LAYOUT.md` / `PAGE-FOLLOWUPS.md` 中已经解决的条目；本任务不修改共享清单。

## 32 项逐项处理

下列“通过”先指各次定向实测，最终全量结果另列；两个上下文项在本次基线就已通过。

| 项目 | 处理 / 结论 |
| --- | --- |
| lock-faces | 选择器改两列；月历表盘上移，给待办和继续读留完整行。定向通过。 |
| ui-scene-big-Today | 为首卡完整标题分配高度，小卡按剩余空间排两行；缩小空白，未缩字。通过。 |
| ui-scene-ImeEdit | 编辑工具栏分两行，统一保留顶部空间，输入与编辑时安全区固定。通过。 |
| ui-scene-big-ImeEdit | 同上；各共用编辑按钮保留 108 高。通过。 |
| ui-scene-big-PhoneIncoming | 来电标题从实际说明文字下方开始。通过。 |
| ui-scene-child-Today | 同今日容量修正，保留孩子使用时间说明。通过。 |
| ime-keyboard1 | 底行功能键按文字重新分配权重，保留空格宽度。通过。 |
| i18n-services | 通知装饰图标和正文列重新分配宽度，英文标题/正文完整显示。通过。 |
| i18n-lang | 设置两页、英文返回按钮宽度已修；重启阶段旧断言仍失败，没完成。 |
| i18n-apps | 闹钟预设两行，短信按钮宽度含边框，天气两列，网页地址独占一行。通过。 |
| i18n-memo | Retry 按真实文字宽度及内边距、边框计算。通过。 |
| i18n-shell | 出厂按钮按完整两行计算高度；资料组和备份状态按实际文字定位。通过。 |
| i18n-shell-remaining | 长删除按钮换行；网络分段按各标题宽度分配；密码键盘避开说明和取消。通过。 |
| ui-context-regressions-metalio-plus | 基线已通过，保持。 |
| ui-context-regressions-desktop-lcd | 基线已通过，保持。 |
| scenario-reader_continuous-metalio-plus | 修正点击长书的位置，36 次和保存预算不变。通过。 |
| scenario-reader_rapid-metalio-plus | 修正点击长书的位置，60 次及延迟预算不变。通过。 |
| scenario-reader_empty-metalio-plus | 修正点击空书的位置，恢复真实翻页拒绝动作。通过。 |
| notify-manage | 通知标题列保留完整可定位标题，删除动作重新可点。通过。 |
| settings-small | 两行编辑栏让 Redo 完整可见并可点，动作仍走原能力。通过。 |
| audit10-b10-02 | 按真实分页逐页核按钮与 AI 动作，不再假定忘掉网络在第一页。通过。 |
| calendar | 月切换控件移到标题下；日期/农历使用当前行高；月格按剩余空间分页，日程每页一条。默认、孩子、最大字号及六行月份定向通过。 |
| weather | 预报两列、当前天气减少空白，更新时间与来源避开底部操作。通过。 |
| confirm-pages | 测试识别新页码圆点并从真实下一页按钮完成强制阅读。通过。 |
| files-app | 大字三行、缩略图及长路径检查分页已对齐；新增第二页快照名断言仍失败，没完成。 |
| files-ops | 勾选状态保持，新语言不再要求行外框。通过。 |
| factory-pages | 检查改为透明图标、中灰分隔线，计数保持严格。通过。 |
| ui-fix-a-factory | 分组、说明、备份状态和按钮都按完整文字测量。通过。 |
| transfer-pages | 文件首页三行留出两个传入入口；测试核对共用圆点。通过。 |
| kid-3 | 时间说明与实际紧凑图标热区核对，旧检查误用大图标尺寸。通过。 |
| comm-ux2 | 联系人列表从完整搜索框下方开始，自适应行数；测试识别新页码。通过。 |
| music-product | 目录和翻页按钮固定在可用页脚；列表三行，防止与末行重叠。定向通过。 |

## 验证命令

所有构建最多 `-j2`、ctest 最多 `-j2`，均加 `taskpolicy -c utility`。字体和 Python 使用任务指定的 `$INK_DEPS`，没有修改依赖目录。

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

ASan 同样配置换 `build-asan`，只加 `-DINK_SANITIZE=ON`。其余命令及最终原话待补。

```sh
INK_DEPS="$INK_DEPS" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" \
  tests/design_lang_1.py --build build --label after --capture --check-targets --compare docs/tasks/design-lang-1
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/design_lang_2_reverse.py \
  --build build --baseline 15589e0fe20fd856c0ee93aeae01ae4e6218eddc
```

【实测】七项反向对照均执行到 `test.finished=1` 后退出 1：今日 5 次、关于 6 次、联系人 1 次、短信 2 次、拨号 1 次、月历 60 次、空页页码 1 次。只将对应基线页面对象链接到最终探针，没有回写产品源码。结果在 `build/design-lang-2/reverse/results.json`。其余失败的修前复现在 `baseline-failures.log`。

【实测】`gcccheck.py` 检查主工程编译数据库中的 40 个改动 C 文件：`gcccheck: 40 compiled, 0 with findings`，退出 0；工厂页增加命名数组上限后另查一次为 `1 compiled, 0 with findings`。截图探针不在数据库中，明确跳过，由截图脚本按真实编译参数编译成功；此结果不等于 Linux CI 已执行。

## 规模及架构核对

产品代码 22 个文件，新增 263 行、删除 132 行，合计 395 行，小于 900。为什么不能更小：输入框总高、今日卡片容量、月历两行、英文按钮、键盘固定安全区是不同的真实布局根因；只恢复旧高度会违反 108 或大字合同。已经在共用按钮、分段、列表空页和键盘高度入口集中处理，没有新增服务或通用框架。

【实现】服务、成员隔离、密码输入与确认策略没有新增路径；按钮和 AI 保持既有原子能力、列表快照及分页动作。没有更改 `apps/`、`third_party/mogo/`、声音/通知服务、S31 U 盘或已有调研和审计资料。

## 过程记录

- 已读取任务规则、任务书、前任务验证与页面接续记录；开工时工作区干净，HEAD 与指定基线一致。
- 构建和测试使用 `taskpolicy -c utility`，构建及 ctest 均最多 `-j2`；不运行窗口模拟器、不碰设备。

【实测】月历最后一轮定向 6 项全过（`calendar-pagination-tests.log`）；已目视核对六行月份最大字号的[第一页](calendar-six-weeks-max-1.png)和[第二页](calendar-six-weeks-max-2.png)，正文及页码完整可见。
