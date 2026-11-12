# ime-engine-1：拼音引擎换成谷歌拼音内核（整块搬 AOSP PinyinIME 的 C++ 部分），接上九宫格和每位家人的学词；对比过了才替换

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7、11 条；第 8 条不碰设备）。

## 为什么

用户 2026-11-12（D-129）：“我们系统还是要自研的 我更接受你提出的移植完整应用嫁接和封装…到我们的系统中 这里不止阅读器 还包括各类系统应用和部分基础框架（例如输入法 通知栏 设置等等）”；“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”。D-131：外壳（含键盘和输入法）先做到成熟可用。

现在的引擎（`system/services/text/pinyin.c:1-8`）是整键查约 1 MB 词库，最长前缀匹配，没有整句和联想。两份独立调研结论一致：能装进这台机器的引擎里，只有谷歌拼音内核同时有整句、联想、简拼、按人学词频，在安卓、Qt、AWTK 上用了多年（`docs/tasks/frameworks-research-1/IME.md`；主会话另一份调研的要点：12 个运行文件 7,804 行，用 ESP-IDF 的 RISC-V 编译器 `-Os` 实测代码 45,811 B、静态内存 8.5 KB，`sizeof(MatrixSearch)=12,504 B`）。

## 成熟来源（搬代码）

- AOSP 拼音输入法的 C++ 内核，Lineage 保留分支：`github.com/LineageOS/android_packages_inputmethods_PinyinIME`，提交 `a08d08cdea4319fb74b55289e1b4e1b1a5249bc2`（`frameworks-research-1/SOURCES.md` I01，哈希在 `sources.json`）。另一份调研用的是 `1702573e`（cm-11.0）：两个提交的 `jni/` 先 diff，一样就用 `a08d08cd`，不一样在报告写差别再选。只读克隆到你自己的 `work/`。
- 拷这 12 个运行文件和它们的头文件：`jni/share/` 下 `dictlist`、`dicttrie`、`lpicache`、`matrixsearch`、`mystdlib`、`ngram`、`pinyinime`、`searchutility`、`spellingtrie`、`splparser`、`userdict`、`utf16char`，`jni/include/` 对应头文件。不要建库工具、同步文件和 Java 前端。
- 词库：`res/raw/dict_pinyin.dat`（1,068,442 B，SHA-256 `6bf0bbde4e3134cce38d08524a9f4dc1af40435243c8e36b38eb68d1e14462b2`）。
- 九宫格接法只当参考：`github.com/VanillaNahida/lvgl_pinyin_ime` 提交 `1e2aa508` 的 `src/core/ime_t9.c:4-11`（数字串转成可能的拼音串，再交同一个引擎；该仓库自称 AI 生成，不搬代码）。数字到字母的表沿用我们已有的 `system/services/text/vendor/kiikoread/nine_key.h`。
- 已知的坑（`frameworks-research-1/EVIDENCE.md` “Google 宿主机探针”）：在 64 位宿主上读这个词库会 `std::bad_alloc`，因为 `spellingtrie.cpp:662-682`、`dictlist` 用 `sizeof(size_t)` 读文件字段（词库按 32 位写的）。把这些读写改成固定 32 位，让模拟器（64 位）和 S31（32 位）读同一份文件；改动逐条写进 `SOURCE.md`。

## 要做的

1. **原文入库**：上面的文件整文件拷进 `system/services/text/vendor/googlepinyin/`，旁边 `SOURCE.md`（来源提交、每个文件的 SHA-256、改了哪几行和为什么）和原许可证（Apache-2.0）。非改不可的只有：固定宽度读写、文件和锁换成我们的文件服务、内存分配走我们的 OSAL（大块放 PSRAM）、去掉安卓日志。
2. **接到文本服务**：`text_service.c` 和 `pinyin.c` 的查询换成新引擎；对外的 `text.*`、`keyboard.key`、`keyboard.pick`、`keyboard.candidates` 能力和候选结构不变（按钮和 AI 走同一个入口）。候选 0 是整句，选了一部分以后剩下的拼音接着组句（`pinyinime.h` 的选词接口）；有联想（`im_get_predicts`）就在选完词后给联想候选。
3. **每位家人一个用户词库**：用户词库文件放在当前家人的空间里（文件服务 `/me/...`），换家人时关掉旧的、打开新的；设一个比安卓小的上限（`userdict.cpp:1946-1952` 的 `set_limit`，数值你定并说明）。密码框不学、不进日志、不给 AI；孩子和大人的词互不相见。现有的按家人学词（`text.user_freq`）怎么处理：能迁就迁，不能就写进报告，旧数据不删。
4. **九宫格**：数字串经 `nine_key.h` 展开成可能的拼音串，交同一个引擎；只展开合法拼音音节的前缀，不做全组合爆炸。九宫格也要能整句（例如 `94664` 出“中国”；整句如“今天天气怎么样”的数字串首选就是这句或在前五）。
5. **词库放哪**：通过对比（见验收 1）就用新词库替换资源包里的旧词库（`assets/ime/`，`fw-size-2` 已改成压缩存放，读取时解到 PSRAM）。**没通过就不替换**：旧引擎仍是默认，新引擎做成可切换（一个按家人的设置），新词库只从存储卡读（例如 `/content/ime/dict_pinyin.dat`），不往应用分区再塞一份 1 MB 的词库。
6. 词库更新（原词库是 2009 年的，`rawdict_utf16_65105_freq.txt` 可以用自带建库工具重建）：这次**不做**，写进报告“后续可做”。

## 范围和预算

- 只改：`system/services/text/`（`pinyin.c`、`text_service.c`、`text_internal.h`、新的胶水文件）、新增 `system/services/text/vendor/googlepinyin/`、`assets/ime/` 里的词库文件和许可证、`tools/imedict/`（只在需要转换或校验词库时）、`tools/asset_pack.py` 里词库那几行、输入法测试（`tests/ime_lexicon/`，新引擎和九宫格的测试放新文件）、CMake 里新目录的那几行、`docs/tasks/ime-engine-1/`。
- 不碰：`system/ui/shell/keyboard.c` 和键盘界面、键盘测试 `tests/ime_keyboard1.*`、`tests/keyboard_matrix.h`（都归 `keyboard-2`，同时在改）、`ui_theme.h`、`textfield.c`。键盘测试因为候选顺序变了而失败的，逐条写进报告（失败项、原因、建议的新期望），由主会话合并时处理；键盘要配合的地方也写进报告。
- 共享文件：新设置或能力进 `tests/integration/ai_surface.c` 的表、`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交、新文案的英文加进 `assets/i18n/en.tsv`，`ai_surface.c` 和 `en.tsv` 只在末尾追加本任务的行；合并冲突由主会话处理。
- 胶水不超过 500 行（拷来的原文不算）；对原文的改动不超过 150 行，超了先停下写原因。
- 固件：代码增量不超过 80 KB；词库替换后资源包净增（压缩后）不超过 60 KB。超了停下写清差多少，不删功能、不换小词库来凑。
- 开工前提：`fw-size-2` 已合入（它改了资源包和 `asset_pack.py`）。

## 验收

1. **对比（照安卓拼音的用法走一遍）**：现有 90 条用例（`tests/ime_lexicon/cases.tsv`，断言在 `ime-lexicon`）新引擎首选不少于 83/90；全拼首选不少于 71/73、前五 73/73；简拼首选不少于 12/16、前五不少于 14/16（现有硬断言，一条不放宽）。另加 20 条整句用例（日常短句，全拼 10 条、九宫格 10 条），列出新旧引擎各自的首选和前五结果。两组数字和逐条差异写进报告。
2. **组句和选词**：输入 `woaibeijingtiananmen` 首选“我爱北京天安门”；先选“我爱”后剩下的拼音继续组句；选完出联想候选。`946644862` 选“中国”后剩 `2`（`ime-reuse-1` 已有的断言照过）。
3. **家人隔离**：大人 A 选了一个生僻词组后再打同样拼音它排前面；换成孩子 B 打同样拼音看不到 A 学的；换回 A 还在；重启后还在。密码框打字后用户词库文件不变。反向对照：去掉换家人时的重开，隔离测试必须失败。
4. **64 位读词库**：模拟器和 S31 宿主机测试都能打开同一份 `dict_pinyin.dat`；反向对照：改回 `sizeof(size_t)` 读法，模拟器测试必须失败。
5. **改前改后对照图**：同一组输入（`nihao`、`zhongguo`、`jintiantianqizenmeyang`、`xx`、九宫格 `94664`）在键盘候选条上的截图，旧引擎一套、新引擎一套，默认和最大文字大小各一套，放 `docs/tasks/ime-engine-1/`（单张不超过 300 KB）。
6. 主工程普通全量 ctest 过；ASan 跑输入法和文本服务相关测试过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写：S31 代码增量、资源包增量、PSRAM 峰值（词库加用户词库加搜索结构）、宿主机上每按一键到出候选的耗时（S31 上的估算标【估算】，并写上机核对清单）。
