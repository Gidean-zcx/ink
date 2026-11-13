# settings-1 执行报告

由 Claude 子代理执行（基线 `8ff500c1`，分支 `worktree-agent-a565d5a141ee36ced`，7 个提交，最后 `d313769c`，只在本地、未推送、未碰设备）。子代理写报告文件时被拦下，本文由主会话照它交回的报告原样存档。

全量 ctest 原话：`97% tests passed, 14 tests failed out of 448`（`Total Test time (real) = 751.43 sec`，memo-save-busy 禁用未跑）。14 项逐个重跑：`14% tests passed, 12 tests failed out of 14`；剩下的 12 项基线上就失败，另 2 项是并行和操作造成的，见“全量失败解释”。

## 验收表

| 编号 | 结论 | 证据 |
| --- | --- | --- |
| 1 照 AOSP 走一遍 | 【实测】通过 | 新测试 `settings-1`：默认字号 575 项检查，最大字号 186 项检查，全部通过。路径：打开设置 → 首页每行都有当前值且都来自注册表 → 手点搜索框输入“暖光” → 结果写“显示和阅读 > 暖光 · 3” → 点结果进“显示和亮度”，那一行有聚焦框 → 点滑杆第 4 格，暖光变成 40 → 返回后搜索结果和首页那一行都写 4 → 搜别名“色温”也能找到 → 关 Wi-Fi 那一行写“已关闭” → 开飞行模式，“移动网络”变灰并写“飞行模式开着”，没有动作可点，AI 去打开也被拒、说同一个原因 → 关振动，“振动强度”变灰并写“振动已关闭” → 4G 注册或掉线时“移动网络”那一行当场改字 → AI 再走一遍：搜索结果和手搜一字不差，打开同一页、同一行有框。另两种状态：机器不支持叫名字时这一项搜不到、打不开并说明原因；SIM 卡只读时“SIM 卡”搜不到，恢复后又能搜到 |
| 2 孩子 | 【实测】通过 | 孩子的首页只有音量、勿扰、亮度、暖光 4 行；孩子搜“密码”能找到“家人”，但只写“家长管理”、不显示“家里 N 个人”，点和 AI 都打不开；孩子从暖光进的显示页里没有字号和自动锁屏 |
| 3 一份声明 | 【实测】通过 | 测试逐项检查注册表全部 61 项：编号都在 settings.open 的参数说明里；能用的项按名称都能被 settings.search 搜到；settings.open 都能落到所在页，能画框的页上那一行有框。反向对照 7 项全部让测试失败 |
| 4 只刷那一行 | 【实测】通过 | AI 把暖光从 4 改成 5，本页只刷这一行：`row 563..670, bands [(585, 648), (0, 108)]`；(0,108) 是状态栏，`navigation.c` 每改一次设置都会重画它，不在本任务文件里，测试里单独列出 |
| 5 改前改后截图 | 已出 | `docs/tasks/settings-1/`：首页、搜索结果、二级页、孩子首页四组左右对照（-1 默认字号，-3 最大字号），另有 dependent、kid-search、home-after，共 14 张，每张 15–44 KB |
| 6 测试和构建 | 见右 | ASan/UBSan【实测】108/108 通过（84.73 秒，UBSAN halt_on_error=1）；S31 宿主机【实测】`100% tests passed out of 78`；S31 固件【实测】编译通过，只有弃用提示，日志有 `factory screen output layer @690a023c…`、没有“没有屏幕输出层”；gcccheck【实测】`4 compiled, 0 with findings`；ui-scale 在全量里通过 |

S31 固件注意：基线和本分支都会卡在 `system/ui/shell/today_edit.c:258`（home-1 留下的 format-truncation）；为量大小临时改成 `%.23s`，量完已还原，未提交。

## 反向对照（`tests/settings_1_reverse.py`，手动跑）

把改坏的一份编成替换文件、链接在系统库前面，不改源码，跑完核对源码没变。原话：`7/7 reverse checks fail as they must; sources unchanged`。7 项：搜索漏掉暖光；首页多画一行注册表没声明的；settings.open 参数说明漏掉暖光；值变了整页重画（刷新区变成 391..1216）；孩子看到大人的项；打开后不定位到那一行；不跟 4G 状态（掉线后那一行还是旧字）。

## 全量失败解释

- 基线上就失败的 12 项：ime-keyboard1、file-index-regressions、integration-update、scenario-standby-metalio-plus、scenario-powercut-during-save、gui-smoke、power-pages、weather、ai-history-photo、perf-latency-2、cell-status、games2；逐个重跑仍失败。把基线的 settings_body.c、settings_pages.c 链进 cell-status 模拟器，或只去掉本分支新加的 4G 监听，cell_status.c:103 都同样失败。perf-latency-2 和 scenario-standby 报 `1534.387 > max 1200.000`，与基线上一样。
- mogo-flow：并行时和 mogo-page 抢着生成同一个字体文件，单独跑通过。
- integration-files：跑全量前删了 build/test-out，里面配置时生成的 files.script 没了；重新配置后单独跑通过。
- 跑全量时另有两个代理的全量 ctest 和一个 -j3 构建同时在跑，负载约 10。没有改任何测试去迁就失败。

## 没做到的

1. 全局搜索（`system/ui/search.c`）还搜不到设置项，任务书说以后再接。
2. 孩子没有用手点进设置的入口：控制中心的齿轮只给大人（`control_center.c`，属 control-1）；孩子现在只能经 AI 进。
3. 手画的三页（显示和亮度、声音、网络）仍自己排行、自己写行名，和注册表名称是两份文字；注册表只给它们提供搜索、定位画框和变灰。留给 settings-2。
4. 搜索只是名称、别名、分组的简单包含匹配：不支持拼音首字母、不容错字，英文区分大小写，没有历史和建议，结果最多 10 条。
5. 截图是 7 个关键步骤各一张。
6. 没上机。

## 和设计不一致的

1. 路径连接符用“ > ”，字体里没有 U+203A“›”。
2. 首页 7 组 14 行，默认字号 3 页、最大字号 4 页；一组跨页时新页顶上重复写组名。
3. “锁屏和壁纸”那一行写自动锁屏时间，沿用 SB-M2。
4. 去掉首页顶上的黑色“我”卡片；家人改成“家人和孩子”组里的一行“家人 · 家里 N 个人”。
5. settings.open 按项目编号打开（编号清单从注册表生成），不按路径（路径是显示文字，随语言变）。
6. 改了 `docs/system/system-services-design.md` §1.3 系统那一行，加上 settings.search、settings.open（不加则 services-mid-capability-doc 失败）；该文件不在任务书范围。
7. 孩子现在能进“显示和亮度”“声音”两页，页上只出现设置服务本来就允许孩子改的项（亮度、暖光、音量、勿扰、按键音、振动、铃声），设置服务的孩子规则没动。
8. 刷新验收把状态栏那一条（0..108）单独列出。

## 改过的已有测试（只改定位方式和页面结构，没改业务断言）

- `tests/ui_shell_scenes.c`：首页行名换成新的 14 行；快照按 `"title"` 查；分隔线改成看同组上面有没有行；改成点“音量”进 SetSound。
- `tests/i18n_lang.c`：中英文行名换新；页数翻到最后一页为止；快照语言字段 `"language":"en"`；英文重画检查用 Mobile Network。
- `tests/integration/ai_surface.c`：给 AI 的表末尾加 `settings.search`（必填 query）、`settings.open`。
- `tests/settings_b.c`：翻页找到“家人”再点。
- `tests/ui_shell_regressions.c`：“叫名字已关闭”现在在第 3 页；回第一页改用 shell.settings。
- `tests/notify_manage.c`：改成点“音量”进声音页。
- `tests/accel_use/accel_use.c`：改用 settings.search 搜“拿起亮屏”打开；快照检查改成 id 加 `"value":"关"`。
- `tests/fw_f8/editor.c`：翻到“亮度”那一行再点进显示页。

## 要用户定的

1. 要不要给孩子一个用手点进设置的入口（要改 `control_center.c`）。
2. 首页默认要翻 3 页，要不要砍掉几行让两页放得下。

## 顺手发现

1. 每改一项设置 `navigation.c` 都会重画状态栏，和 D-127 不符（属 control-1）。
2. S31 固件在基线上就编不过（today_edit.c:258）。
3. 清掉 build/test-out 后必须重新配置，否则 integration-files 失败；建议写进门禁步骤。
4. mogo-flow 和 mogo-page 并行时抢同一个字体文件，会偶发失败。
5. `tests/settings_b_reverse.py` 里 SB-M2-settings、SB-M2-voice 两项要改的字符串已不存在，现在改了也照样通过；未动该文件。
6. accel-use 里充电时的保持唤醒和“静止才锁屏”有时序竞争，首页多重画一次就会暴露；本任务改成只用一次 settings.search 打开来避开，未改断言。
7. 状态监听表最多 80 个，满了会静默失败；代码里登记处约 76 处（不全在开机时），本任务加 1 个。

## 新增的机制

- 设置注册表：`ITEMS[]` 共 61 项（首页 14、列表页 33、手画页 15；备份同时在首页和列表页），加 `ink_sr_*` 接口；`ink_sr_target` 记住 settings.open 要定位的行，只在建页时用。
- 首页记住本页画出的行和值标签（PSRAM），值变了只改那个标签。
- 2 个新能力 settings.search、settings.open（导航类，孩子能用）；1 个状态监听 `cell.`（给“移动网络”那一行）。没有新设置、新服务、新线程、新存储格式。

## 注册表写法（给 settings-2）

在 `system/ui/shell/settings_registry.c` 的 `ITEMS[]` 加一行：`.id`、`.title = TR_NOOP("名称")`、`.aliases = TR_NOOP("别名，空格分开")`、`.icon`、`.group`（NET/SOUND/SHOW/FAMILY/AI/STORE/SYS）、`.page`（所在页名）。`.setting` 走 settings.get/set，开关自动写“开/关”、选项写选项名、数字写数字；没有设置的行用 `.text` 写固定字（"" 不写值）或 `SAY(name, 表达式)` 写摘要。首页的行默认走 settings.open，直接调别的能力写 `.cap`/`.args`；所在页不按页名打开时写 `.open = "shell.xxx"`。`.state` 返回 `INK_SR_*` 并把原因写进 why；孩子的“家长管理”由 `.adult = true` 自动给。`.where`：H 首页的行（按表序），L 列表页的行（页要在 settings_pages.c 的 PAGES 里登记），0 手画页上的行（只进搜索和定位）。手画页上把那一块包进 `if (at("编号", 高度)) {…}`，只给大人的块再加 `for_me("编号")`。新文字的英文追加到 en.tsv 末尾，重新配置才生效。加完跑 settings-1 和 ui-scale（`one_declaration` 会逐项核对能否搜到、打开、画框）。

## 规模

- 产品代码 +599 −214（600 行内）：settings_body.c +301 −92，settings_pages.c +64 −122，settings_registry.c +192，settings_registry.h +42。
- 测试 +432 −29（新 settings-1 测试 382 行）；CMake +7 −2；en.tsv 末尾 65 行；capabilities.md 已重新生成。
- 固件 +12,400 字节（20 KB 内）：ink-s31.bin 4,451,040 → 4,463,440（.text +5,726，.rodata +6,672，PSRAM .bss +2,368，内部 RAM +8）。
- 隐私检查（`git diff 8ff500c1..HEAD` 里查本机路径和 MAC）：无输出。
