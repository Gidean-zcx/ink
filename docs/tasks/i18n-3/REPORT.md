# i18n-3 执行报告

基线：`e65bfc261b68e9fb063fc60da507122119ae4fc6`。分支：`claude/codex-i18n-3`。仅本地提交，未 push。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 应用文案、格式占位符 | 通过 | 【实现】十个应用接入原有 TR；固定动作在原辅助函数求译，用户数据不送入 TR；【实测】577 条译文校验通过 | `python3 tests/i18n_tables.py` |
| 1b 英文截图、排版、缺译 | 通过 | 【实测】55 个英文场景：检查省略、文字尺寸、重叠、四边越界；两个用例的 `i18n.missing` 均为 0 | `ctest --test-dir build -R '^i18n-[am]' --output-on-failure -j2` |
| 1c 中文与回切 | 通过 | 【实测】十个应用共 15 个同进程中文回切场景；每个构建保存 140 帧：中文运行 70 帧，英文运行 55 张英文帧加 15 张回切帧；原始用户文字不变 | 同上；ASan 目录同跑 |
| 2a 主机全量 | 通过 | 【实测】原话：`100% tests passed out of 349`；`Total Test time (real) = 519.21 sec`；无跳过 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 2b ASan / UBSan | 通过 | 【实测】原话：`100% tests passed out of 349`；`Total Test time (real) = 818.06 sec`；无跳过，未见内存诊断；开启 SANITIZE 并保留 WERROR | 上条命令改为 `--test-dir build-asan`，`UBSAN_OPTIONS=halt_on_error=1` |
| 2c S31 | 通过 | 【实测】`ink-s31.elf` 编译、链接退出 0；`I18N_EN` 的 4616 字节指针表在 `.flash.rodata`，地址 `0x4024cc4c` | SDK 环境与 `ports/esp32s31/build.sh` 一致；`idf.py reconfigure` 后执行 `taskpolicy -c utility cmake --build build-s31 --target ink-s31.elf -j2` |
| 2d gcccheck | 通过 | 【实测】`gcccheck: 14 compiled, 0 with findings`；测试另一分支 `1 compiled, 0 with findings` | `taskpolicy -c utility python3 ../../tools/gcccheck.py build system/apps/{calculator,camera,clock,contacts,gallery,memo,phone,remote,sms,weather}/*.c system/policy/confirm.c tests/i18n_apps.c` |
| 3 每文件数量、保留项、截图位置 | 通过 | 见下表、截图目录和边界说明 | `tests/i18n_apps.c`、`tests/i18n_apps.py` |
| B 产品代码预算 | 未达成 | 【实测】相对基线产品代码新增 330 行、删除 297 行，超过 300 行上限；不含译文、测试、报告；CMake 中 8 行测试注册也已扣除 | `git diff e65bfc261b68e9fb063fc60da507122119ae4fc6..HEAD --numstat` |

全量验证的产品提交为 `3d4b783e`。之后只扩充了回切测试和报告，产品代码未变；两个构建的追加测试各 `100% tests passed out of 2`，分别用时 20.59 / 33.03 秒；gcccheck 与三组反向对照也已复跑。

构建沿用任务规则的字体、Python 和 LVGL 路径，保留 WERROR；额外设置 `INK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`，让中枢测试实际执行。构建最多 `-j2`、测试最多 `-j2`，均以 `taskpolicy -c utility` 运行，构建与全量测试顺序执行。

S31 只构建 ELF，没有运行设备操作、生成烧录命令或上机。原厂代码没有改动。主机与 ASan 日志在 `work/i18n-3/ctest-final.log`、`ctest-asan.log`；编译日志分别为 `build-final.log`、`build-asan-final.log`、`s31-build.log`。

### 文件迁移与保留内容

下列数量为本轮新增接入的原文键，按文件去重；已有译文的新使用不重复计入。固定动作辅助函数、通话状态和历史类型的显示处也计入；文件间共享键各自计算。

| 文件（位于 system/apps） | 条数 | 故意保留的内容 |
| --- | ---: | --- |
| calculator/calculator.c | 12 | 算式、数字、运算符；能力 title |
| camera/camera.c | 12 | 照片目录、序号文件名，保持已有文件可读取；能力 title |
| camera/scan.c | 0（前置任务已迁） | 二维码原文、网址、联系人、Wi-Fi 数据；本次纳入 S31 译文源检查 |
| camera/png.c、camera/quirc_alloc.h | 0 | 编码/分配实现，没有界面文案 |
| clock/clock.c | 51 | 用户闹钟名、记录键、能力 title；静态动作 ID 保持中文原键，只有 label 翻译 |
| contacts/contacts.c | 26 | 用户姓名、号码、记录键与数据库/能力元数据 |
| gallery/gallery.c | 16 | 图片、路径、照片文件名、能力 title |
| gallery/photo.h | 0 | 接口声明，没有文案 |
| memo/memo.c | 36 | 用户正文、清单项、已保存转写文字、录音路径和能力 title |
| phone/phone.c | 36 | 号码、按键、通话状态/类型的存储值；显示时翻译，不改状态比较 |
| remote/remote.c | 27 | 用户网址、输入和服务器提供的网页内容；页面内部 ID、能力 title |
| sms/sms.c | 20 | 联系人、短信正文和存储记录；收发颜色继续比较原始状态 |
| weather/weather.c | 44 | 城市、地区、城市 ID、天气缓存原文；显示时翻译已知天气描述 |

农历：本范围没有农历月份、节气或天干地支的显示代码，保持公历日期；已迁的 calendar 不改，没有新增隐藏农历行的规则。

### 截图与排版

可复现截图在 `build/test-out/i18n-apps/{apps,memo}/{en,zh}/snap-*.png`，对应页面快照为同目录的 `pages.ndjson`。ASan 的同一套证据位于 `build-asan/test-out/i18n-apps/`。`*-back-zh.png` 明确是中文回切帧，即使它位于 `en/` 运行目录。

代表帧：`apps/en/snap-Phone.png`、`snap-PhoneCall.png`、`snap-PhoneKeypad.png`、`snap-ClockAlarm.png`、`snap-Weather.png`、`snap-RemoteBlocked.png`、`snap-PhoneEmergency.png`、`snap-Camera-locked-photo.png`；`memo/en/snap-MemoDetail.png`、`snap-MemoSaveFail.png`。每页其余场景由 `pages.ndjson` 列出。

- 电话：英文三个页签按文字宽度分配；删除键和收起键盘按钮加宽；中文尺寸保留。
- 天气：英文预报拆成“日期 / 天气”两行，行高随现有字体行高计算；不缩字号，中文仍是原来的一行。
- 相机：保留已有测试要求的 `Scan QR code`，只在英文下加宽扫码按钮。
- 备忘用短文案 `Edit / Delete / Checklist / Retry`；世界城市用 `UTC`。其余沿用既有手机用词。

自动排版检查只比较实际显示的文字：追加回切时，页面的“已记下”通知完全被不透明键盘遮住；测量先按现有键盘区域排除此类被遮住的页面对象，再逐个检查键盘标签，未修改生产代码来迁就测量。诊断证据在 `build/test-out/i18n-memo-diagnostic/`；代表帧已目检，汇总预览为 `work/i18n-3/final-review.png`。

反向对照：【实测】`python3 tests/i18n_apps_reverse.py --build build` 在临时对象中撤掉修复，未改源文件：计算器翻译、电话页签宽度、天气行布局分别退出 1，触发 1、3、2 条失败断言；原版本通过。证据在 `build/test-out/i18n-reverse/`，临时对象和静态库自动删除。

## 没做到的

- **330 行超过 300 行产品预算 30 行。** 没有把这项写成通过。为什么不能更小：本次同时涉及十个应用的文案和语义快照，静态页面/动作描述须在创建或快照时求译，还需修三个实际英文布局。考虑过在公共组件里翻所有文本，会误翻联系人、正文等用户数据；考虑过原地修改静态表，语言回切会丢原键，均未采用。没有用新框架或压缩格式规避行数统计。
- 共享键盘、长文底栏、列表分页说明和凭据有无值等仍有中文，属于 i18n-2；服务生成的多段短信详细发送回执属于 i18n-4。本任务没有把它们标成已翻。已保存的用户内容不随语言重写。
- 排版用默认成人字号和合成数据验证，没有穷举所有字号、成员、错误组合与超长用户数据。长网址等沿原来的省略行为，测试使用短假网址 `example.com`。
- 没有 Linux 主机或 CI 运行；本机为 macOS，用指定 gcccheck 补编译核查。没有实机验证。

## 和设计不一致的

没有改设计文档迁就实现。英文电话和天气的布局调整如上，字号没有缩小。公共确认模板展开处 `system/policy/confirm.c` 仅增加 i18n 头文件和 `TR(c->ask)`：联系人应用的 ask 是静态原键，必须在此统一求译后再替换参数；没有另造一套确认流程。能力 title 按任务书保留。

## 改过的已有测试

仅 `tests/i18n_tables.py`：排序检查先用 `ast.literal_eval` 解码生成头文件的 C 字符串，再按 UTF-8 字节排序；所有原断言和九种坏表的预期保持原样。旧的“按引号 split”把 `\n` 当作反斜杠和 n，新增天气整句模板后会误判排序。【实测】恢复旧提取方法，原排序断言失败；运行时生成表和二分查找正常。扫码原有测试没有改，恢复了其要求的 `Scan QR code`。

## 要用户定的

是否接受超出 30 行的当前实现，或交回复核者收窄改动；本次不宣称预算达标，没有改预算或共享需求。

## 顺手发现

共享确认卡也把能力 title 当作人的标题使用（`system/policy/confirm.c` 的确认视图构造处）；任务书把能力 title 列为只给 AI 看。没有扩展翻译全部能力，需后续任务复核这条边界。

## 新增的机制

产品无新增机制。复用 TR、已有能力和设置、原 UI 组件；没有新增产品任务、状态机、持久格式、缓存或配置项。页面快照与按钮仍走原能力，未改变成员隔离和凭据掩码。

仅验收目标新增如下内容，均由任务验收 1 的中英截图/快照检查要求引出：

- `i18n-app-test` 测试驱动线程：两个测试程序各一份，不进产品。
- `INK_I18N_APPS_LANG` 测试环境变量、`INK_I18N_MEMO` 测试编译宏：复用同一测试文件跑两种语言和两种夹具，不是设备设置。
- `pages.ndjson` 逐帧快照证据：仅构建目录中的测试产物，不是设备持久数据格式。

## 要改的文档

共享 backlog 由主会话记录验证结果和预算未达项；i18n-2/4 继续补共享外壳和服务文案。requirements、ideas、decisions、生成的 capabilities 均未改。

## 最终检查

【实测】差异仅含任务相关的 20 个文件，未混入构建产物、设计稿、共享文档或设备信息；`git diff --check` 通过。按总规则第 6 节的正则扫描基线到 HEAD 的差异，0 匹配（grep 退出 1）。最终提交后再次核对。
