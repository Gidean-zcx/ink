# 固定来源与代码证据

查阅日期：2026-11-12。以下都是本次实际读过的文件；【实现】只证明对应版本的代码行为，不代表目标硬件已经验证。文件完整 SHA-256、大小及行号见 [sources.json](sources.json)。负向结论仅限已检索的这些路径。

## I01 AOSP 拼音（Lineage 保留分支）

[LineageOS/android_packages_inputmethods_PinyinIME](https://github.com/LineageOS/android_packages_inputmethods_PinyinIME/tree/a08d08cdea4319fb74b55289e1b4e1b1a5249bc2) · `a08d08cdea4319fb74b55289e1b4e1b1a5249bc2`。

- [jni/include/pinyinime.h:28–204](https://github.com/LineageOS/android_packages_inputmethods_PinyinIME/blob/a08d08cdea4319fb74b55289e1b4e1b1a5249bc2/jni/include/pinyinime.h#L28-L204)：打开词库、增量搜索、整句候选、选择、预测和首字母开关。
- [jni/share/matrixsearch.cpp:665–850](https://github.com/LineageOS/android_packages_inputmethods_PinyinIME/blob/a08d08cdea4319fb74b55289e1b4e1b1a5249bc2/jni/share/matrixsearch.cpp#L665-L850)：更新词频与组词学习。
- [jni/share/spellingtrie.cpp:662–682](https://github.com/LineageOS/android_packages_inputmethods_PinyinIME/blob/a08d08cdea4319fb74b55289e1b4e1b1a5249bc2/jni/share/spellingtrie.cpp#L662-L682)：按 size_t 读取词库并分配缓冲。
- [src/com/android/inputmethod/pinyin/InputModeSwitcher.java:37–155](https://github.com/LineageOS/android_packages_inputmethods_PinyinIME/blob/a08d08cdea4319fb74b55289e1b4e1b1a5249bc2/src/com/android/inputmethod/pinyin/InputModeSwitcher.java#L37-L155)：中文、英文、数字、符号输入模式。
- [src/com/android/inputmethod/pinyin/CandidatesContainer.java:75–205](https://github.com/LineageOS/android_packages_inputmethods_PinyinIME/blob/a08d08cdea4319fb74b55289e1b4e1b1a5249bc2/src/com/android/inputmethod/pinyin/CandidatesContainer.java#L75-L205)：候选容器和翻页。
- [src/com/android/inputmethod/pinyin/EnglishInputProcessor.java:25–95](https://github.com/LineageOS/android_packages_inputmethods_PinyinIME/blob/a08d08cdea4319fb74b55289e1b4e1b1a5249bc2/src/com/android/inputmethod/pinyin/EnglishInputProcessor.java#L25-L95)：英文由前端单独处理，不等于中文引擎混输。

## I02 Qt Virtual Keyboard 的 Google 拼音派生版本

[qt/qtvirtualkeyboard](https://github.com/qt/qtvirtualkeyboard/tree/705290a76114fc5b39102e838eed82d83abef33e) · `705290a76114fc5b39102e838eed82d83abef33e`。

- [src/plugins/pinyin/3rdparty/pinyin/include/pinyinime.h:27–216](https://github.com/qt/qtvirtualkeyboard/blob/705290a76114fc5b39102e838eed82d83abef33e/src/plugins/pinyin/3rdparty/pinyin/include/pinyinime.h#L27-L216)：解码 API 与预测、用户词典开关。
- [src/plugins/pinyin/3rdparty/pinyin/share/dicttrie.cpp:170–217](https://github.com/qt/qtvirtualkeyboard/blob/705290a76114fc5b39102e838eed82d83abef33e/src/plugins/pinyin/3rdparty/pinyin/share/dicttrie.cpp#L170-L217)：词典读入及内存分配。
- [src/plugins/pinyin/3rdparty/pinyin/share/dicttrie.cpp:243–270](https://github.com/qt/qtvirtualkeyboard/blob/705290a76114fc5b39102e838eed82d83abef33e/src/plugins/pinyin/3rdparty/pinyin/share/dicttrie.cpp#L243-L270)：QFile 依赖，不能把 Qt 版当纯标准库代码。
- [src/plugins/pinyin/3rdparty/pinyin/CMakeLists.txt:14–68](https://github.com/qt/qtvirtualkeyboard/blob/705290a76114fc5b39102e838eed82d83abef33e/src/plugins/pinyin/3rdparty/pinyin/CMakeLists.txt#L14-L68)：源码清单、Qt::Core 和 Threads 依赖。
- [src/plugins/pinyin/pinyininputmethod.cpp:60–155](https://github.com/qt/qtvirtualkeyboard/blob/705290a76114fc5b39102e838eed82d83abef33e/src/plugins/pinyin/pinyininputmethod.cpp#L60-L155)：Qt 输入方式适配，不是商业 Google 输入法。

## I03 librime

[rime/librime](https://github.com/rime/librime/tree/bb56876890ba60f446fa2640c244b1794b965eda) · `bb56876890ba60f446fa2640c244b1794b965eda`。

- [CMakeLists.txt:60–157](https://github.com/rime/librime/blob/bb56876890ba60f446fa2640c244b1794b965eda/CMakeLists.txt#L60-L157)：Boost、线程、yaml-cpp、LevelDB、Marisa、OpenCC 依赖。
- [src/rime/gear/script_translator.cc:214–246](https://github.com/rime/librime/blob/bb56876890ba60f446fa2640c244b1794b965eda/src/rime/gear/script_translator.cc#L214-L246)：用户词典与拼音翻译。
- [src/rime/gear/script_translator.cc:351–398](https://github.com/rime/librime/blob/bb56876890ba60f446fa2640c244b1794b965eda/src/rime/gear/script_translator.cc#L351-L398)：提交后的学习。
- [src/rime/gear/script_translator.cc:705–753](https://github.com/rime/librime/blob/bb56876890ba60f446fa2640c244b1794b965eda/src/rime/gear/script_translator.cc#L705-L753)：词图与整句组合。

## I04 Rime 明月拼音数据和方案

[rime/rime-luna-pinyin](https://github.com/rime/rime-luna-pinyin/tree/56b934b099dfbeab842320f13aa8b461a6ab3e42) · `56b934b099dfbeab842320f13aa8b461a6ab3e42`。

- [luna_pinyin.schema.yaml:41–91](https://github.com/rime/rime-luna-pinyin/blob/56b934b099dfbeab842320f13aa8b461a6ab3e42/luna_pinyin.schema.yaml#L41-L91)：输入流水线、字母表、简拼补丁；custom_phrase 的 enable_sentence=false 不代表主引擎禁用整句。
- [luna_pinyin.dict.yaml:1–36](https://github.com/rime/rime-luna-pinyin/blob/56b934b099dfbeab842320f13aa8b461a6ab3e42/luna_pinyin.dict.yaml#L1-L36)：词典来源、版本与 use_preset_vocabulary；本文件不是完整运行内存。

## I05 libpinyin

[libpinyin/libpinyin](https://github.com/libpinyin/libpinyin/tree/85905e59cb19d0343cae84d831027aee113420b8) · `85905e59cb19d0343cae84d831027aee113420b8`。

- [src/pinyin.h:369–411](https://github.com/libpinyin/libpinyin/blob/85905e59cb19d0343cae84d831027aee113420b8/src/pinyin.h#L369-L411)：整句和上下文预测。
- [src/pinyin.h:610–618](https://github.com/libpinyin/libpinyin/blob/85905e59cb19d0343cae84d831027aee113420b8/src/pinyin.h#L610-L618)：训练接口。
- [src/storage/pinyin_custom2.h:20–65](https://github.com/libpinyin/libpinyin/blob/85905e59cb19d0343cae84d831027aee113420b8/src/storage/pinyin_custom2.h#L20-L65)：不完整拼音等配置。
- [configure.ac:69–118](https://github.com/libpinyin/libpinyin/blob/85905e59cb19d0343cae84d831027aee113420b8/configure.ac#L69-L118)：GLib 与 Berkeley DB / Kyoto Cabinet。
- [data/CMakeLists.txt:1–58](https://github.com/libpinyin/libpinyin/blob/85905e59cb19d0343cae84d831027aee113420b8/data/CMakeLists.txt#L1-L58)：模型和 bigram 数据单独下载生成。

## I06 SunPinyin

[sunpinyin/sunpinyin](https://github.com/sunpinyin/sunpinyin/tree/e4be7e64e4a168a684b26679b7e076118a2c97f7) · `e4be7e64e4a168a684b26679b7e076118a2c97f7`。

- [README.md:1–9](https://github.com/sunpinyin/sunpinyin/blob/e4be7e64e4a168a684b26679b7e076118a2c97f7/README.md#L1-L9)：二元、三元统计语言模型，上游说明。
- [src/ime-core/imi_context.h:207–221](https://github.com/sunpinyin/sunpinyin/blob/e4be7e64e4a168a684b26679b7e076118a2c97f7/src/ime-core/imi_context.h#L207-L221)：历史学习权重。
- [src/ime-core/imi_context.h:284–325](https://github.com/sunpinyin/sunpinyin/blob/e4be7e64e4a168a684b26679b7e076118a2c97f7/src/ime-core/imi_context.h#L284-L325)：整句和候选接口。
- [src/pinyin/syllable.h:98–146](https://github.com/sunpinyin/sunpinyin/blob/e4be7e64e4a168a684b26679b7e076118a2c97f7/src/pinyin/syllable.h#L98-L146)：声母和不完整音节处理。
- [src/ime-core/userdict.cpp:34–70](https://github.com/sunpinyin/sunpinyin/blob/e4be7e64e4a168a684b26679b7e076118a2c97f7/src/ime-core/userdict.cpp#L34-L70)：SQLite 用户词库。
- [SConstruct:399–432](https://github.com/sunpinyin/sunpinyin/blob/e4be7e64e4a168a684b26679b7e076118a2c97f7/SConstruct#L399-L432)：mmap 与 SQLite 等构建探测。

## I07 fcitx5-android 前端和引擎归属

[fcitx5-android/fcitx5-android](https://github.com/fcitx5-android/fcitx5-android/tree/eb5b50216e70484a009933d9519db76005f1faf3) · `eb5b50216e70484a009933d9519db76005f1faf3`。

- [README.md:30–72](https://github.com/fcitx5-android/fcitx5-android/blob/eb5b50216e70484a009933d9519db76005f1faf3/README.md#L30-L72)：内置中文引擎、Rime 插件、候选展开和截图入口。
- [.gitmodules:1–37](https://github.com/fcitx5-android/fcitx5-android/blob/eb5b50216e70484a009933d9519db76005f1faf3/.gitmodules#L1-L37)：libime、chinese-addons 与 Rime 的独立来源。
- [app/src/main/java/org/fcitx/fcitx5/android/input/keyboard/TextKeyboard.kt:33–82](https://github.com/fcitx5-android/fcitx5-android/blob/eb5b50216e70484a009933d9519db76005f1faf3/app/src/main/java/org/fcitx/fcitx5/android/input/keyboard/TextKeyboard.kt#L33-L82)：QWERTY、语言和数字符号切换布局。
- [app/src/main/java/org/fcitx/fcitx5/android/input/keyboard/KeyboardWindow.kt:47–88](https://github.com/fcitx5-android/fcitx5-android/blob/eb5b50216e70484a009933d9519db76005f1faf3/app/src/main/java/org/fcitx/fcitx5/android/input/keyboard/KeyboardWindow.kt#L47-L88)：键盘与符号面板切换。
- [app/src/main/java/org/fcitx/fcitx5/android/input/keyboard/NumberKeyboard.kt:25–66](https://github.com/fcitx5-android/fcitx5-android/blob/eb5b50216e70484a009933d9519db76005f1faf3/app/src/main/java/org/fcitx/fcitx5/android/input/keyboard/NumberKeyboard.kt#L25-L66)：数字与符号入口。

## I08 libime（Fcitx 拼音核心）

[fcitx/libime](https://github.com/fcitx/libime/tree/53df6a248793f9a00c1769e658630de313f3e419) · `53df6a248793f9a00c1769e658630de313f3e419`。

- [src/libime/pinyin/pinyincontext.h:75–143](https://github.com/fcitx/libime/blob/53df6a248793f9a00c1769e658630de313f3e419/src/libime/pinyin/pinyincontext.h#L75-L143)：候选、选词与学习接口。
- [src/libime/pinyin/pinyincontext.cpp:1089–1139](https://github.com/fcitx/libime/blob/53df6a248793f9a00c1769e658630de313f3e419/src/libime/pinyin/pinyincontext.cpp#L1089-L1139)：用户词与历史学习。
- [src/libime/pinyin/pinyinprediction.h:20–72](https://github.com/fcitx/libime/blob/53df6a248793f9a00c1769e658630de313f3e419/src/libime/pinyin/pinyinprediction.h#L20-L72)：拼音联想接口。
- [src/libime/pinyin/pinyinencoder.h:20–59](https://github.com/fcitx/libime/blob/53df6a248793f9a00c1769e658630de313f3e419/src/libime/pinyin/pinyinencoder.h#L20-L59)：拼写与部分音节匹配配置。
- [data/CMakeLists.txt:1–51](https://github.com/fcitx/libime/blob/53df6a248793f9a00c1769e658630de313f3e419/data/CMakeLists.txt#L1-L51)：语言模型与词典包的 URL、版本和哈希。
- [CMakeLists.txt:33–47](https://github.com/fcitx/libime/blob/53df6a248793f9a00c1769e658630de313f3e419/CMakeLists.txt#L33-L47)：Fcitx5Utils 与 Boost 依赖。

## I09 Trime 同文输入法

[osfans/trime](https://github.com/osfans/trime/tree/76a9e763b148d747f6245f61d65e02595082f080) · `76a9e763b148d747f6245f61d65e02595082f080`。

- [app/src/main/assets/shared/trime.yaml:22–45](https://github.com/osfans/trime/blob/76a9e763b148d747f6245f61d65e02595082f080/app/src/main/assets/shared/trime.yaml#L22-L45)：键盘固定高度和边距。
- [app/src/main/assets/shared/trime.yaml:686–710](https://github.com/osfans/trime/blob/76a9e763b148d747f6245f61d65e02595082f080/app/src/main/assets/shared/trime.yaml#L686-L710)：分类符号与历史面板。
- [app/src/main/assets/shared/trime.yaml:920–929](https://github.com/osfans/trime/blob/76a9e763b148d747f6245f61d65e02595082f080/app/src/main/assets/shared/trime.yaml#L920-L929)：数字、符号和键盘切换。
- [app/src/main/assets/shared/trime.yaml:975–1035](https://github.com/osfans/trime/blob/76a9e763b148d747f6245f61d65e02595082f080/app/src/main/assets/shared/trime.yaml#L975-L1035)：默认键盘布局。
- [app/src/main/java/com/osfans/trime/ime/composition/CandidatesView.kt:40–145](https://github.com/osfans/trime/blob/76a9e763b148d747f6245f61d65e02595082f080/app/src/main/java/com/osfans/trime/ime/composition/CandidatesView.kt#L40-L145)：候选视图；Android 实现需翻译。
- [doc/Keyboard.md:7–58](https://github.com/osfans/trime/blob/76a9e763b148d747f6245f61d65e02595082f080/doc/Keyboard.md#L7-L58)：符号固定按键条及横屏布局。

## I10 mogo-e4p 输入法

[chopper1026/mogo-e4p](https://github.com/chopper1026/mogo-e4p/tree/864f2f9dc75980a67d2d6b67b17365020b7a50ce) · `864f2f9dc75980a67d2d6b67b17365020b7a50ce`。

- [main/ui/ui_ime.c:14–34](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/ui/ui_ime.c#L14-L34)：有界工作区和 PSRAM。
- [main/ui/ui_ime.c:47–63](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/ui/ui_ime.c#L47-L63)：九键数字映射。
- [main/ui/ui_ime.c:94–158](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/ui/ui_ime.c#L94-L158)：词组接续和动态规划组词。
- [main/ui/ui_ime.h:8–29](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/ui/ui_ime.h#L8-L29)：48 原始字符、24 候选及 API。
- [main/ui/ui_ime_phrases.h:1–25](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/ui/ui_ime_phrases.h#L1-L25)：静态常用短语表。
- [main/ui/ui_keyboard.c:1–82](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/ui/ui_keyboard.c#L1-L82)：键盘模式与墨水屏刷新说明。
- [main/ui/ui_keyboard.c:180–238](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/ui/ui_keyboard.c#L180-L238)：候选与键盘绘制。
- [main/ui/ui_keyboard.c:420–448](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/ui/ui_keyboard.c#L420-L448)：区域更新处理。

## I11 kiikoread 输入法

[wegooo-cell/Metalio-S31-kiikoread](https://github.com/wegooo-cell/Metalio-S31-kiikoread/tree/83942eb45d13064a47aca7ff04c2faf8fd51f98a) · `83942eb45d13064a47aca7ff04c2faf8fd51f98a`。

- [main/ui/ui_ime.c:14–34](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_ime.c#L14-L34)：有界工作区和 PSRAM。
- [main/ui/ui_ime.c:47–63](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_ime.c#L47-L63)：九键数字映射。
- [main/ui/ui_ime.c:94–158](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_ime.c#L94-L158)：词组接续和动态规划组词。
- [main/ui/ui_ime.h:8–29](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_ime.h#L8-L29)：48 原始字符、24 候选及 API。
- [main/ui/ui_ime_phrases.h:1–25](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_ime_phrases.h#L1-L25)：静态常用短语表。
- [main/ui/ui_keyboard.c:1–82](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_keyboard.c#L1-L82)：键盘模式与墨水屏刷新说明。
- [main/ui/ui_keyboard.c:180–238](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_keyboard.c#L180-L238)：候选与键盘绘制。
- [main/ui/ui_keyboard.c:420–448](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_keyboard.c#L420-L448)：区域更新处理。

## I12 固件口袋 Plus 源码包

[Changkai9888/CloudZao-Projects](https://github.com/Changkai9888/CloudZao-Projects/tree/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8) · `90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8`。

- [main/ime/notepad_ime.cc:9–50](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/firmware-pocket/firmware-pocket-plus-1.0.7-plus.16-source.zip)：最多六字母，精确单音节查表，逐个单字候选。（ZIP 内相对路径，网页不能直接定位包内行；先解压文本源码再用 `nl -ba`。）
- [main/ime/pocket_keyboard.cc:121–179](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/firmware-pocket/firmware-pocket-plus-1.0.7-plus.16-source.zip)：五候选、翻页、中文英文切换。（ZIP 内相对路径，网页不能直接定位包内行；先解压文本源码再用 `nl -ba`。）
- [main/ime/pocket_keyboard.cc:245–285](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/firmware-pocket/firmware-pocket-plus-1.0.7-plus.16-source.zip)：LVGL 键盘布局。（ZIP 内相对路径，网页不能直接定位包内行；先解压文本源码再用 `nl -ba`。）
- [sources/pinyin-generation.json:1–23](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/firmware-pocket/firmware-pocket-plus-1.0.7-plus.16-source.zip)：Rime 数据固定提交、哈希和裁剪统计。（ZIP 内相对路径，网页不能直接定位包内行；先解压文本源码再用 `nl -ba`。）
- [scripts/generate_pinyin.py:30–79](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/firmware-pocket/firmware-pocket-plus-1.0.7-plus.16-source.zip)：只保留单字的读音和排序数据。（ZIP 内相对路径，网页不能直接定位包内行；先解压文本源码再用 `nl -ba`。）

## I13 ink 基线现状

[Gidean-zcx/ink](https://github.com/Gidean-zcx/ink/tree/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a) · `6171c2bf8d59238c95f2d548663f8b2ab3c6f43a`。

- [system/services/text/pinyin.c:1–26](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/text/pinyin.c#L1-L26)：查表、词库上限，非自动音节切分。
- [system/services/text/pinyin.c:173–238](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/text/pinyin.c#L173-L238)：全拼与九键最长前缀匹配。
- [system/services/text/text_service.c:30–32](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/text/text_service.c#L30-L32)：按成员隔离的词频集合。
- [system/services/text/text_service.c:355–415](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/text/text_service.c#L355-L415)：已有候选重排学习，不是整句语言模型。
- [system/ui/shell/keyboard.c:198–214](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/ui/shell/keyboard.c#L198-L214)：AI 快照及按钮能力。
- [system/ui/shell/keyboard.c:264–303](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/ui/shell/keyboard.c#L264-L303)：九键、全键、符号布局。
- [system/ui/shell/keyboard.c:649–660](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/ui/shell/keyboard.c#L649-L660)：键盘能力注册。
- [tools/imedict/README.md:22–54](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/tools/imedict/README.md#L22-L54)：1 MiB、全拼和首字母键、学习边界。

## H01 Zinnia 识别核心

[taku910/zinnia](https://github.com/taku910/zinnia/tree/581faa8f6f15e4a7b21964be3a5ec36265c80e5b) · `581faa8f6f15e4a7b21964be3a5ec36265c80e5b`。

- [index.html:18–58](https://github.com/taku910/zinnia/blob/581faa8f6f15e4a7b21964be3a5ec36265c80e5b/index.html#L18-L58)：SVM 识别及模型独立安装，上游说明。
- [zinnia/recognizer.cpp:73–119](https://github.com/taku910/zinnia/blob/581faa8f6f15e4a7b21964be3a5ec36265c80e5b/zinnia/recognizer.cpp#L73-L119)：C++ 分类、文件映射和缓冲区打开接口。
- [zinnia/mmap.h:154–180](https://github.com/taku910/zinnia/blob/581faa8f6f15e4a7b21964be3a5ec36265c80e5b/zinnia/mmap.h#L154-L180)：POSIX mmap 路径，MCU 需另接存储。

## H02 Tegaki 模型与 Wagomu

[tegaki/tegaki](https://github.com/tegaki/tegaki/tree/7a74e442c4130cccc226a7e7c2b683ac94c0cccb) · `7a74e442c4130cccc226a7e7c2b683ac94c0cccb`。

- [tegaki-models/tegaki-zinnia-simplified-chinese/handwriting-zh_CN.meta:1–3](https://github.com/tegaki/tegaki/blob/7a74e442c4130cccc226a7e7c2b683ac94c0cccb/tegaki-models/tegaki-zinnia-simplified-chinese/handwriting-zh_CN.meta#L1-L3)：官方简体中文模型标识。
- [tegaki-models/README.in:21–49](https://github.com/tegaki/tegaki/blob/7a74e442c4130cccc226a7e7c2b683ac94c0cccb/tegaki-models/README.in#L21-L49)：模型与元数据独立，仓库并未提供已测 MCU 运行占用。
- [tegaki-engines/tegaki-wagomu/README:1–22](https://github.com/tegaki/tegaki/blob/7a74e442c4130cccc226a7e7c2b683ac94c0cccb/tegaki-engines/tegaki-wagomu/README#L1-L22)：DTW 识别及旧 Python 包装依赖。
- [tegaki-engines/tegaki-wagomu/wagomu.cpp:277–313](https://github.com/tegaki/tegaki/blob/7a74e442c4130cccc226a7e7c2b683ac94c0cccb/tegaki-engines/tegaki-wagomu/wagomu.cpp#L277-L313)：DTW 计算及缓冲。

## N01 AOSP SystemUI / Lineage 衍生实现

[LineageOS/android_frameworks_base](https://github.com/LineageOS/android_frameworks_base/tree/22de88b7b8eb979c816669c7963dee66ae4953cd) · `22de88b7b8eb979c816669c7963dee66ae4953cd`。

- [packages/SystemUI/src/com/android/systemui/statusbar/notification/collection/ShadeListBuilder.java:94–146](https://github.com/LineageOS/android_frameworks_base/blob/22de88b7b8eb979c816669c7963dee66ae4953cd/packages/SystemUI/src/com/android/systemui/statusbar/notification/collection/ShadeListBuilder.java#L94-L146)：过滤、分组、排序的列表构建。
- [packages/SystemUI/src/com/android/systemui/statusbar/notification/row/NotificationContentView.java:1480–1535](https://github.com/LineageOS/android_frameworks_base/blob/22de88b7b8eb979c816669c7963dee66ae4953cd/packages/SystemUI/src/com/android/systemui/statusbar/notification/row/NotificationContentView.java#L1480-L1535)：直接回复视图的实际绑定和可见性回调。
- [services/core/java/com/android/server/notification/ZenModeFiltering.java:185–267](https://github.com/LineageOS/android_frameworks_base/blob/22de88b7b8eb979c816669c7963dee66ae4953cd/services/core/java/com/android/server/notification/ZenModeFiltering.java#L185-L267)：闹钟、对话、来电、重复来电与消息的勿扰例外。
- [packages/SystemUI/src/com/android/systemui/qs/tiles/InternetTileNewImpl.kt:91–118](https://github.com/LineageOS/android_frameworks_base/blob/22de88b7b8eb979c816669c7963dee66ae4953cd/packages/SystemUI/src/com/android/systemui/qs/tiles/InternetTileNewImpl.kt#L91-L118)：点击与长按设置入口。
- [packages/SystemUI/src/com/android/systemui/qs/tiles/ModesTile.kt:103–142](https://github.com/LineageOS/android_frameworks_base/blob/22de88b7b8eb979c816669c7963dee66ae4953cd/packages/SystemUI/src/com/android/systemui/qs/tiles/ModesTile.kt#L103-L142)：模式切换和次级动作。

## N02 PebbleOS 通知和设置

[coredevices/PebbleOS](https://github.com/coredevices/PebbleOS/tree/aa0c27e8330d010e9202f9a706a56002db5861ee) · `aa0c27e8330d010e9202f9a706a56002db5861ee`。

- [pebble/popups/notifications/notification_window.c:241–280](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/popups/notifications/notification_window.c#L241-L280)：清除全部通知及完成回调。
- [pebble/services/timeline/timeline_actions.c:1143–1168](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/services/timeline/timeline_actions.c#L1143-L1168)：回复菜单入口。
- [pebble/services/notifications/do_not_disturb.c:148–185](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/services/notifications/do_not_disturb.c#L148-L185)：跨日勿扰时间段。
- [pebble/apps/system/settings/quiet_time.c:56–110](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/apps/system/settings/quiet_time.c#L56-L110)：手动、智能、计划、来电例外与背光。
- [pebble/apps/system/settings/menu.c:23–50](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/apps/system/settings/menu.c#L23-L50)：设置模块目录和入栈。
- [pebble/apps/system/settings/option_menu.c:26–80](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/apps/system/settings/option_menu.c#L26-L80)：选项行和选择回调。

## N03 MuditaOS Pure 的通知和设置

[mudita/MuditaOS](https://github.com/mudita/MuditaOS/tree/2cd0e4721405c0bf263f311f27d38aadae6ac89f) · `2cd0e4721405c0bf263f311f27d38aadae6ac89f`。

- [README.md:1–15](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/README.md#L1-L15)：E Ink / FreeRTOS / Pure 产品身份。
- [module-gui/gui/widgets/StatusBar.cpp:30–105](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-gui/gui/widgets/StatusBar.cpp#L30-L105)：状态栏控件与手机模式。
- [module-apps/apps-common/notifications/NotificationsModel.cpp:43–94](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/apps-common/notifications/NotificationsModel.cpp#L43-L94)：通知列表、清除与可见性。
- [module-apps/application-settings/windows/SettingsMainWindow.cpp:14–39](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/application-settings/windows/SettingsMainWindow.cpp#L14-L39)：网络、显示、手机模式、安全、系统等分类。
- [module-apps/application-settings/windows/phone-modes/PhoneModesWindow.cpp:22–80](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/application-settings/windows/phone-modes/PhoneModesWindow.cpp#L22-L80)：当前模式与勿扰、离线入口。
- [module-apps/application-settings/windows/phone-modes/DoNotDisturbWindow.cpp:43–71](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/application-settings/windows/phone-modes/DoNotDisturbWindow.cpp#L43-L71)：锁屏通知、收藏联系人来电例外。
- [module-apps/application-settings/windows/display-keypad/DisplayLightWindow.cpp:9–49](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/application-settings/windows/display-keypad/DisplayLightWindow.cpp#L9-L49)：前光当前值和调节。
- [module-apps/application-settings/windows/network/SimCardsWindow.cpp:20–120](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/application-settings/windows/network/SimCardsWindow.cpp#L20-L120)：SIM 页面。
- [module-apps/application-settings/windows/network/ApnSettingsWindow.cpp:20–103](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/application-settings/windows/network/ApnSettingsWindow.cpp#L20-L103)：APN 页面。
- [module-apps/application-settings/windows/apps/SoundSelectWindow.cpp:22–65](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/application-settings/windows/apps/SoundSelectWindow.cpp#L22-L65)：铃声选择页和声音模型。

## N04 InfiniTime

[InfiniTimeOrg/InfiniTime](https://github.com/InfiniTimeOrg/InfiniTime/tree/9ad16304f5fd526b781a760cf1313813d232645e) · `9ad16304f5fd526b781a760cf1313813d232645e`。

- [src/components/ble/NotificationManager.h:31–84](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/components/ble/NotificationManager.h#L31-L84)：100 字节消息、最多 5 条通知。
- [src/displayapp/screens/Notifications.cpp:320–367](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/displayapp/screens/Notifications.cpp#L320-L367)：来电动作和静音。
- [src/displayapp/screens/settings/QuickSettings.cpp:54–118](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/displayapp/screens/settings/QuickSettings.cpp#L54-L118)：四个快捷按钮及刷新任务。
- [src/displayapp/screens/settings/QuickSettings.cpp:136–166](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/displayapp/screens/settings/QuickSettings.cpp#L136-L166)：亮度、通知与睡眠状态切换。
- [src/displayapp/screens/settings/Settings.cpp:20–42](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/displayapp/screens/settings/Settings.cpp#L20-L42)：手表设置页面结构。

## N05 ESP-Brookesia v0.4.2 LVGL 壳

[espressif/esp-brookesia](https://github.com/espressif/esp-brookesia/tree/2c0c83fad95b81c81ee560ba8af684ff32a402f4) · `2c0c83fad95b81c81ee560ba8af684ff32a402f4`。

- [src/widgets/status_bar/esp_brookesia_status_bar.cpp:18–57](https://github.com/espressif/esp-brookesia/blob/2c0c83fad95b81c81ee560ba8af684ff32a402f4/src/widgets/status_bar/esp_brookesia_status_bar.cpp#L18-L57)：Wi-Fi、电池状态栏。
- [src/widgets/navigation_bar/esp_brookesia_navigation_bar.cpp:18–90](https://github.com/espressif/esp-brookesia/blob/2c0c83fad95b81c81ee560ba8af684ff32a402f4/src/widgets/navigation_bar/esp_brookesia_navigation_bar.cpp#L18-L90)：返回、主页、最近任务导航条。
- [src/widgets/recents_screen/esp_brookesia_recents_screen_snapshot.cpp:37–104](https://github.com/espressif/esp-brookesia/blob/2c0c83fad95b81c81ee560ba8af684ff32a402f4/src/widgets/recents_screen/esp_brookesia_recents_screen_snapshot.cpp#L37-L104)：最近任务截图对象及图像分配。

## N06 ESP-Brookesia v0.8 master

[espressif/esp-brookesia](https://github.com/espressif/esp-brookesia/tree/65d66c810ad706c9e2f82fe9177b11816acee546) · `65d66c810ad706c9e2f82fe9177b11816acee546`。

- [README.md:37–49](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/README.md#L37-L49)：当前与历史版本、S31 的 IDF 要求。
- [app/brookesia_app_settings/src/settings_app.cpp:61–90](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/app/brookesia_app_settings/src/settings_app.cpp#L61-L90)：JSON 资源入口；每个 Wi-Fi 行约 25 KB 是上游注释。
- [app/brookesia_app_settings/package/res/screens/settings_home.json:29–82](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/app/brookesia_app_settings/package/res/screens/settings_home.json#L29-L82)：行标题、当前值、事件动作。
- [app/brookesia_app_settings/package/res/templates/search_card.json:25–60](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/app/brookesia_app_settings/package/res/templates/search_card.json#L25-L60)：只有搜索占位文案，不是可验证搜索引擎。
- [app/brookesia_app_settings/package/res/screens/sound.json:48–80](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/app/brookesia_app_settings/package/res/screens/sound.json#L48-L80)：声音页面和音量动作。

## N07 ZSWatch 通知

[ZSWatch/ZSWatch](https://github.com/ZSWatch/ZSWatch/tree/821c340366a0bc55b536f7feb5d45ca3836c44c3) · `821c340366a0bc55b536f7feb5d45ca3836c44c3`。

- [app/src/managers/zsw_notification_manager.h:25–107](https://github.com/ZSWatch/ZSWatch/blob/821c340366a0bc55b536f7feb5d45ca3836c44c3/app/src/managers/zsw_notification_manager.h#L25-L107)：通知管理器和字段上限。
- [app/src/ui/notification/zsw_popup_notification.c:40–135](https://github.com/ZSWatch/ZSWatch/blob/821c340366a0bc55b536f7feb5d45ca3836c44c3/app/src/ui/notification/zsw_popup_notification.c#L40-L135)：通知弹层。
- [app/src/ui/notification/zsw_notification_reply.c:39–102](https://github.com/ZSWatch/ZSWatch/blob/821c340366a0bc55b536f7feb5d45ca3836c44c3/app/src/ui/notification/zsw_notification_reply.c#L39-L102)：录音、转写、草稿、发送和失败状态；依赖手机链路。

## N08 LilyPhone

[dparnell/lilyphone1](https://github.com/dparnell/lilyphone1/tree/de10fc3882292b9be1cfcd14632726590fdef262) · `de10fc3882292b9be1cfcd14632726590fdef262`。

- [src/ui/ui_phone1.cpp:448–465](https://github.com/dparnell/lilyphone1/blob/de10fc3882292b9be1cfcd14632726590fdef262/src/ui/ui_phone1.cpp#L448-L465)：从顶端下拉快捷设置。
- [src/ui/ui_phone1.cpp:4706–4751](https://github.com/dparnell/lilyphone1/blob/de10fc3882292b9be1cfcd14632726590fdef262/src/ui/ui_phone1.cpp#L4706-L4751)：快捷开关与当前值行。
- [src/main.cpp:39–54](https://github.com/dparnell/lilyphone1/blob/de10fc3882292b9be1cfcd14632726590fdef262/src/main.cpp#L39-L54)：拖动刷新节流和定期全刷参数，不能照搬 Plus。

## S01 Lineage / AOSP 设置

[LineageOS/android_packages_apps_Settings](https://github.com/LineageOS/android_packages_apps_Settings/tree/f82184028165ea108335a9745428c26fea3cec47) · `f82184028165ea108335a9745428c26fea3cec47`。

- [res/xml/top_level_settings.xml:30–94](https://github.com/LineageOS/android_packages_apps_Settings/blob/f82184028165ea108335a9745428c26fea3cec47/res/xml/top_level_settings.xml#L30-L94)：分类层级、当前值和控制器。
- [src/com/android/settings/search/BaseSearchIndexProvider.java:52–110](https://github.com/LineageOS/android_packages_apps_Settings/blob/f82184028165ea108335a9745428c26fea3cec47/src/com/android/settings/search/BaseSearchIndexProvider.java#L52-L110)：可索引 XML 和原始数据。
- [src/com/android/settings/search/SearchFeatureProvider.java:82–140](https://github.com/LineageOS/android_packages_apps_Settings/blob/f82184028165ea108335a9745428c26fea3cec47/src/com/android/settings/search/SearchFeatureProvider.java#L82-L140)：搜索界面委托给外部 Activity。
- [src/com/android/settings/dashboard/DashboardFragment.java:462–481](https://github.com/LineageOS/android_packages_apps_Settings/blob/f82184028165ea108335a9745428c26fea3cec47/src/com/android/settings/dashboard/DashboardFragment.java#L462-L481)：控制器更新当前状态。
- [src/com/android/settings/notification/RingVolumePreferenceController.java:30–96](https://github.com/LineageOS/android_packages_apps_Settings/blob/f82184028165ea108335a9745428c26fea3cec47/src/com/android/settings/notification/RingVolumePreferenceController.java#L30-L96)：响铃音量分类。
- [src/com/android/settings/security/ChangeSimPinPreferenceController.java:30–90](https://github.com/LineageOS/android_packages_apps_Settings/blob/f82184028165ea108335a9745428c26fea3cec47/src/com/android/settings/security/ChangeSimPinPreferenceController.java#L30-L90)：SIM PIN 与设备锁不同。

## C01 ink 接入合同与现有服务

[Gidean-zcx/ink](https://github.com/Gidean-zcx/ink/tree/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a) · `6171c2bf8d59238c95f2d548663f8b2ab3c6f43a`。

- [system/ui/shell/navigation.c:980–1008](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/ui/shell/navigation.c#L980-L1008)：控制中心 AI 快照。
- [system/ui/shell/navigation.c:1011–1135](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/ui/shell/navigation.c#L1011-L1135)：快捷设置、成员分流与通知摘要。
- [system/services/notify/notify_caps.c:149–192](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/notify/notify_caps.c#L149-L192)：通知原子能力与事件。
- [system/services/display/light.c:74–87](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/display/light.c#L74-L87)：亮度和暖光设置。
- [system/policy/settings.c:414–434](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/policy/settings.c#L414-L434)：统一读写设置能力。
- [system/policy/members.c:1034–1054](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/policy/members.c#L1034-L1054)：家人能力与只能本人输入的 PIN。
- [system/ui/shell/settings_pages.c:181–191](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/ui/shell/settings_pages.c#L181-L191)：设置行和能力绑定。
- [docs/process.md:46–58](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/docs/process.md#L46-L58)：软总线、原子能力与 AI 原生审视。
