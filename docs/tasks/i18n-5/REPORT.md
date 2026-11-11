# i18n-5 执行报告

基线：`2bbf0302ac9d7bc3c97c06d1e4393fdc59793ae7`。状态：普通全量、S31 和 GCC 已通过，ASan 进行中。开工时工作树干净；只读依赖主检出的 HEAD 与指定基线一致，无需合并。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】`i18n-literals` 为 0；临时直接写中文标签时退出 1，恢复后通过；56 帧的新覆盖及英文长回答均 `i18n.missing=0` | 下列定向及反向命令 |
| 2a 普通全量 | 通过 | 【实测】`100% tests passed out of 368`，478.76 秒，0 跳过；最终提示微调后，中英文长回答及静态检查另 `100% tests passed out of 3`，8.25 秒 | 下列全量命令；`build/verification/{ctest-final,final-note}.log` |
| 2b ASan | 进行中 | 构建和全量执行中 | 同下列命令，目录换为 `build-asan`，配置加 `-DINK_SANITIZE=ON` |
| 2c S31 | 通过 | 【实测】`Built target ink-s31.elf`，退出 0；编译器警告／错误均为 0 | 下列 S31 命令；`build-s31/verification/` |
| 2d gcccheck | 通过 | 【实测】`gcccheck: 24 compiled, 0 with findings`，退出 0，无跳过 | 下列 GCC 命令；`build/verification/gcccheck.log` |
| 2e ui-scale | 通过 | 【实测】`ui-scale: 51 files, 0 violations` | `taskpolicy -c utility python3 -B tools/ui_scale_check.py` |
| 2f 隐私 | 待最终提交后核查 | 暂无命中 | 总规则第 6 节表达式 |
| 3 | 通过 | 【实现】逐文件计数见下；【实测】英文截图经标签尺寸、重叠、越界检查 | `i18n-shell-remaining`、`i18n-apps`、`i18n-memo` |

## 实现和逐文件清单

检查递归扫描 `system/ui/`、`system/apps/`、`apps/` 的 C/H，以及长回答提示头文件；不依赖 Git 历史。连本地变量、数组和组件包装函数中的中文一起检查，并核对已标记键在各语言表中存在。白名单在 [检查源码](../../../tests/i18n_literals.py)：能力／数据／状态定义的 AI 元数据、逐函数列出的 AI 页面描述、源语言比较和解析、逐项说明的持久目录／文件名。没有整文件豁免。新源码自动纳入。

下表为新增 `TR/TR_NOOP` 标记的中文字面量处数，同一行的多个文案分别计数；包括原先显示时已翻译、这次补标记的常量。

| 文件 | 处数 |
| --- | ---: |
| `apps/home/home.c` | 2 |
| `apps/reader/reader.c` | 9 |
| `system/apps/calculator/calculator.c` | 1 |
| `system/apps/calendar/calendar.c` | 36 |
| `system/apps/camera/camera.c` | 1 |
| `system/apps/camera/scan.c` | 3 |
| `system/apps/clock/clock.c` | 8 |
| `system/apps/contacts/contacts.c` | 1 |
| `system/apps/files/files.c` | 44 |
| `system/apps/gallery/gallery.c` | 1 |
| `system/apps/memo/memo.c` | 3 |
| `system/apps/phone/phone.c` | 7 |
| `system/apps/remote/remote.c` | 10 |
| `system/apps/sms/sms.c` | 1 |
| `system/apps/weather/weather.c` | 5 |
| `system/include/ink/sys/ai.h` | 1 |
| `system/ui/file_picker.c` | 1 |
| `system/ui/shell/member_pages.h` | 1 |
| `system/ui/shell/mobile_network.c` | 1 |
| `system/ui/shell/navigation.c` | 149 |
| `system/ui/shell/settings_family.c` | 79 |
| `system/ui/theme.c` | 8 |

另改 `system/services/ai/ai_service.c` 一处截断逻辑：保留原中文提示的固定 44 字节预留，按当前译文实际长度复制和终止；保留 UTF-8 整字截断。英文提示含终止符 43 字节，检查会拒绝超过原预留的译文，避免切换语言时预留随之变化。CMake 补键检查来源、注册静态检查、英文长回答及独立截图测试。英文表新增 165 键，缩短 6 个既有键和 6 个本次新增键以适应实际宽度。月历复用“周日”等短星期键，中文仍只显示“日”等原有单字，避免把日期组件的 Day 当成 Sunday。空书架提示设置现有标签宽度以自动换行。

## 英文截图

均为当前检出的相对路径，不提交图片；同名 JSON 为页面证据，书架的证据类型见下。

- `build/test-out/i18n-shell-remaining/snap-{SetMembers,FamilyAdd,FamilyAdult,SetKid,Wallpaper,Wallpaper-next,Records,AiMemory,RuntimeRecords,Control,Control-more}-en.png`。
- 同目录 `snap-{SetDisplay,SetSound,SetNetwork}-en-{1,2,3,4}.png`；后续页到末页后保持末页，未声称四张都是不同页。
- 同目录已有 AI、备份、传输、搜索和更新页；本次合计 56 帧，899 条检查，0 排版问题。另有 `build/test-out/i18n-shell/` 的 35 帧。
- 日历和文件：`build/test-out/i18n-extras/snap-{Calendar,Upcoming,Files,FileList,FileActions,FileMove}-en.png`，6 帧、180 条检查；书架 `build/test-out/i18n-bookshelf/snap-Bookshelf-en.png`，1 帧、17 条检查；空书架 `build/verification/bookshelf-empty/snap-Bookshelf-en.png`。三者缺译均为 0。书架同名 JSON 只记录控件文本，不冒充 AI 快照。
- 应用英文：`build/test-out/i18n-apps/{apps,memo}/en/snap-*.png`，覆盖时钟、电话、短信、天气、相机等原有截图用例。
- 长回答最后一页：`build/test-out/ai-answer-long-en/snap-AiLongCut.png`，快照 `AiLongCut/snapshot.json`。

## 定向及反向验证

【实测】静态检查临时在 `navigation.c` 增加一个 `lv_label_set_text` 中文字面量，退出 1 并指到该行；恢复后源码哈希一致，检查为 0。日志 `build/verification/literals{,-reverse}.log`。CTest 内另有直接标签、多行按钮、间接数组、相邻字面量、Unicode 转义及白名单不能吞掉同一行第二个标签的反向自检。

【实测】原定向复测 `100% tests passed out of 3`（32.98 秒）；扩大截图覆盖后的最终定向 `100% tests passed out of 4`（7.17 秒），包含系统页组、日历文件页组、书架和英文长回答。长回答的隔离反向对象只撤掉提示翻译，编译和链接退出 0，英文测试退出 1，指定断言 `the last page ends with the note` 失败；未修改产品源码。命令和日志在 `build/i18n-5-reverse/`。

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build \
  -R '^(i18n-shell-remaining|i18n-extras|i18n-bookshelf|i18n-shell|ai-answer-long-en)$' --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/i18n_5_reverse.py --build build
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/i18n_literals.py
```

## 没做到的

Linux 原生 CI 未运行；本机为 macOS，另做任务要求的 GCC 检查。ASan 结果待补；24 个改动 C 编译项均已核对带 `-fsanitize=address,undefined`。独立页组验证不等于长串预览无异常，见“顺手发现”。

## 和设计不一致的

无设计偏离。固定文案经现有翻译表；用户姓名、正文、文件名、能力 ID、状态比较键和持久路径保持其语义。按钮与 AI 仍经原能力路径，未新增权限或跨成员访问。

## 改过的已有测试

没有删除基线断言、放宽预算或增加 sleep。

- `i18n_shell_remaining.c` 新增家人、控制中心和三组设置截图，另以独立模式运行日历／文件和书架；扫描时不重复统计滑块用 `LV_OBJ_FLAG_USER_1` 标注的白色裁剪副本，底层完整标签仍检查尺寸和重叠。AiHome 的既有英文期望从 Conversations 改为 Chats，与缩短后的译文一致，使长回答的三位数页码有足够空间。
- `i18n_shell.c` 的存储截图先等待实际统计／备份记录加载完成，再沿用原有截图断言和时限；首轮在异步加载重画时出现 `frame presented` 失败，复测通过。
- `ai_answer_long.c` 默认中文期望保留语义：页码改为按固定字段 ID 及 `=` 后数字读取，原来只识别中文前缀；截断提示、未截断检查和面板溢出说明改为当前语言完整译文。新增英文运行、面板标签不省略、提示译文符合固定预留和缺译为 0 的断言；宿主环境只在 `vtk_host_call` 读取。
- `sdk_boundary.cmake` 把公开翻译接口 `i18n` 加入应用框架名单，头文件名称匹配允许数字；仍须逐项在名单内。反向检查确认未知的 `not_allowed1.h` 和 OSAL 头文件均退出 1，`i18n.h` 退出 0。证据 `build/verification/sdk-reverse.log`。

## 要用户定的

暂无。

## 顺手发现

- 【实现／实测】旧书架未提供 AI 页面快照，快照接口返回 0。按本任务范围只迁移文案，独立书架用例检验控件文本、布局和截图；没有把它标成 AI 快照已完成。
- 【实测】把日历、文件和原系统页面连续放在同一轮约 62 帧预览时，约 110 秒虚拟时间在 `undo-line-106.inki` 加载处 panic；磁盘上的资源存在。将 `extra_pages()` 放在主 `run` 的 `shell.tasks` 打开之前可恢复当时的组合顺序。原因未定位，也未与基线对照，不能认定是既有缺陷或翻译回归。现按独立页组运行，保留每页断言；未声称该组合异常被修复。复现记录 `build/verification/focused-final{,-2}.log`。

## 新增的机制

无新增产品任务、线程、状态机、文件格式、配置项、缓存、队列或重试。静态检查和英文长回答用例由任务书验收 1、2 要求；截图扩展复用现有测试任务。仅有测试运行模式 `INK_I18N_ANSWER_EN`、`INK_I18N_BOOKSHELF`、`INK_I18N_EXTRAS`，用于任务书要求的独立英文截图和反向验证，不是产品配置。

## 要改的文档

主会话合并后更新任务总表的 G7 英文覆盖状态，并记录书架 AI 快照和组合预览异常；共享需求、决策、能力表和 UI 依赖文档未改。

## 规模与收尾

产品代码当前 `+292/-261`，净增 31 行（译文、测试和文档另算），低于净增 300 行预算。超过 100 行的原因是逐处标记 22 个源码文件中的固定文案；更小的做法已采用常量 `TR_NOOP` 加原显示点查表。没有选择在通用标签组件里翻译任意输入，以免把用户内容当键、重复翻译译文或改变状态判断。

首轮普通全量原话：`99% tests passed, 1 tests failed out of 366`（1081.62 秒）；失败为 SDK 名单，另 3 项中枢依赖跳过。名单已修复，中枢检查已用只读依赖里的锁定环境单独通过；清空测试输出目录后恢复 CMake 生成的 `files.script`，最终普通全量已通过（见验收表）。ASan 和最终隐私扫描结果待补。


## 全量和端口复现

宿主机用总规则的配置参数，额外指定 `-DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"` 和 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，仅复用只读依赖。普通和 ASan 均开启 Werror；并行度固定为 2。清空 `test-out` 后须重新执行 CMake 配置，恢复生成的文件服务脚本。

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# ASan: 按总规则同参数配置 build-asan，额外加 -DINK_SANITIZE=ON，再构建和全量测试。
# GCC: 文件参数为下方列出的全部改动 C 文件。
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B "$INK_DEPS/../ink-codex/tools/gcccheck.py" build \
  apps/{home/home,reader/reader}.c \
  system/apps/{calculator/calculator,calendar/calendar,camera/camera,camera/scan,clock/clock,contacts/contacts,files/files,gallery/gallery,memo/memo,phone/phone,remote/remote,sms/sms,weather/weather}.c \
  system/services/ai/ai_service.c system/ui/{file_picker,theme,shell/mobile_network,shell/navigation,shell/settings_family}.c \
  tests/{ai_answer_long/ai_answer_long,i18n_shell,i18n_shell_remaining}.c
```

S31 只构建 ELF，未打包烧录镜像。复用 SDK 和只读依赖中的 LVGL、原厂显示代码和固定版本 Opus 压缩包，加载 SDK 环境；临时目录和组件缓存均指向本检出的 `build-s31/`。复制端口默认配置到构建目录，浅睡保持默认关闭。SDK 有 Kconfig 弃用提示，未改变 SDK 或硬件参数。

```sh
b="$PWD/build-s31"
mkdir -p "$b/tmp" "$b/cache"
export TMPDIR="$b/tmp" XDG_CACHE_HOME="$b/cache" PYTHONDONTWRITEBYTECODE=1
export IDF_COMPONENT_MANAGER=0 IDF_COMPONENT_CACHE_PATH="$b/cache/components" CCACHE_DISABLE=1
export IDF_PATH="$HOME/esp/esp-idf" IDF_TOOLS_PATH="$HOME/esp/.espressif"
unset IDF_DEACTIVATE_FILE_PATH
. "$IDF_PATH/export.sh"
cp ports/esp32s31/sdkconfig.defaults "$b/sdkconfig"
cp "$INK_DEPS/build-s31/esp-opus-1.0.5.zip" "$b/"
export INK_LVGL_SOURCE_DIR="$INK_DEPS/build/_deps/lvgl-src"
export INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"
cd ports/esp32s31
taskpolicy -c utility idf.py --preview -B "$b" -DIDF_TARGET=esp32s31 -DSDKCONFIG="$b/sdkconfig" -DINK_S31_LIGHT_SLEEP=OFF reconfigure
taskpolicy -c utility cmake --build "$b" --target ink-s31.elf -j2
```
