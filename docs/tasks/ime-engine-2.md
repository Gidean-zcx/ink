# ime-engine-2：英文补全照雾凇拼音的做法做——中文模式下英文词当次候选，英文模式下按前缀补全单词；谷歌拼音和旧引擎都能用

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 11 条：引擎类的东西整块拿来集成，上面加我们的 AI 接入层）。

**D-140（总规则第 11 节）**：本任务是“照着重写”。第一步写对照表 `docs/tasks/ime-engine-2/对照表.md`（参照产品每一页、每个操作和状态，应用看 `docs/tasks/replan-sysapps/APPS.md` 的参照路径出发）并单独提交；计划和 `replan-sysapps/SOURCES.md` 点名的 C/C++ 代码能直接用的照搬。参照安卓的地方按第 11 节为这台设备改，做完逐行验收。

## 为什么

用户 2026-11-12（D-129）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”，“这里不止阅读器 还包括各类系统应用和部分基础框架（例如输入法 通知栏 设置等等）”。D-131 把键盘和输入法列为先做到成熟可用的外壳部件。交付计划 v4 第 2 节，试用版 22 写给用户的是“英文打字有单词补全”；第 3a 节这一行是：英文补全，ime-engine-2，“照雾凇的做法，英文词当次候选”，自己写，依赖 ime-engine-1。

依据：主会话的调研（`frameworks-agent.md` 1.1 节表里谷歌拼音“中英混输：无”；1.3 节“要我们加：英文单词补全，中英混输照雾凇的思路把英文词当次候选”；证据 [R3] 雾凇 `t9.schema.yaml:28-34` 挂了 `melt_eng` 英文）；`docs/tasks/frameworks-research-1/IME.md:17`（谷歌内核的英文“由 AOSP 前端另管，是模式切换”）和 `:19`（我们现在“英文显式切换，非自动混输”）。librime 本身装不进这台机器（1.1 节），所以引擎不搬，只照雾凇的**做法和词表**自己写。

现状（读过代码，行号已核对）：

- ime-engine-1 已合入（`1be7587e`）：谷歌拼音解码器放在按家人的设置 `text.google` 后面；首选 76/90 低于旧引擎 83/90，**旧引擎仍是默认**。所以英文候选必须加在文本服务这一层，两种引擎都能用，不能只挂在谷歌引擎里。
- 中文模式：组合串只收 a–z（`system/services/text/text_service.c:468` 起的 `ink_text_key`），候选全来自拼音引擎（`compute_cands`，384 行起）。打 `hello`：旧引擎最长前缀只能匹配 `he`（和），按空格得到“和”，剩下 `llo` 还在组合串里；没有任何英文候选。
- 英文模式（`TS.english`，476 行切换）：字母直接写进正文，没有组合串，也**没有任何补全**。打开谷歌引擎时还有个毛病：组合串为空就调 `py_google_predict(TS.value, …)`（399 行），英文模式下也会去要中文联想。
- 谷歌联想取错了字：399 行把**整段正文** `TS.value` 传进去（函数里没有光标信息）；`google_pinyin.cpp:146-151` 从正文**开头**取最多 `LETTERS`=40 个字交给 `im_get_predicts`，上游再取这些字的最后 `kMaxPredictSize` 个（`vendor/googlepinyin/include/dictdef.h:43、:62`：`kMaxLemmaSize`=8，所以是 **7** 个；`share/pinyinime.cpp:163-168`）。正文超过 40 个字，或者光标不在末尾时，联想依据的不是光标前的字。
- 候选结构 `ink_text_cand_t`（`system/include/ink/sys/textinput.h:39-43`）只有 `word[40]`、`consumed`、`ai`，分不出英文候选；键盘快照把每个候选写成“候选[i]=词”（`system/ui/shell/keyboard.c:216-217`），说不出哪个是英文。
- AI 接话（`assist_allowed`，87 行；`ink_text_candidates`，430 行）在组合串为空时盖过其它候选。英文模式下组合串一直为空，如果不处理，补全会被 AI 接话盖住。

## 成熟来源（照做法自己写，词表搬数据）

雾凇拼音 `github.com/iDvel/rime-ice`，提交 `927e1cf1d3eeb9bf4be85e3f684dba4dd485b6f6`（2026-11-11；只读取到你自己的 `work/`）：

- `rime_ice.schema.yaml:16`、`:67-68`：英文 `melt_eng` 作为次翻译器挂在拼音方案上，另挂中英混合词 `cn_en`；`:170-173`、`:188-204`：权重拼音 1.2、英文 1.1、中英混合 0.5（“拼音的权重应该比英文大”），英文不造句、**不学词**（`enable_user_dict: false`）；文件 SHA-256 `ec4b0dd02ac287f53fe401f315cde648913eb60bb8cf8d9a8a613d1eb4f9698b`。
- `rime_ice.schema.yaml:113-152` 和 `lua/reduce_english_filter.lua:83-115`：一份短词名单（rug、bus、my 等），这些编码的英文词即使整串匹配，也降到第 2 位，不抢第一个候选。用 `mode: custom`，只照搬 `words` 列表。
- `lua/autocap_filter.lua:11-57`：首字母大写的输入，候选首字母跟着大写（Hel → Hello）；开头两个以上大写字母，候选全大写（HEl → HELLO）；词表里本来就有大小写的（iPhone、Beijing）和单个字母不转。
- `lua/en_spacer.lua:5-14`：前面刚上屏的是英文词，再选英文词时自动隔一个空格。`cn_en_spacer.lua` 雾凇默认没开，我们也不开（中文和英文之间不加空格）。
- 词表：`en_dicts/en.dict.yaml`（约 2.1 万词，SHA-256 `1049164da983c1c5a933d6401df9a8e5f8a378b0c67b102e0f75f0a65226ec44`）、`en_dicts/en_ext.dict.yaml`（约 2,600 条缩写和网络词，`c8648c1dc1ad691c77ef29525244d42482b2e8bcabe08355baec241f563306a2`）、`en_dicts/cn_en.txt`（约 500 个中英混合词，比如 T恤、X光，`e52ffc88598eb3d4db476a10064df3e8bbb6c23bc4c3969723f3dfab45ad4089`）。许可证 GPL-3.0。D-129 用户说过“不需要在乎开源协议”，这条比 vendor-common 第 1 条的“GPL 一律不用”晚，以它为准；许可证原文跟着输出放进资源包，`SOURCE.md` 写清楚。
- 词频：雾凇的 `en.dict.yaml` 按字母排序，没有词频（它在第 2 行注明来源是 first20hours/google-10000-english 的 `20k.txt`，而那份表是按频率排的）。所以排序用同一来源：`github.com/first20hours/google-10000-english` 提交 `d0736d492489198e4f9d650c7ab4143bc14c1e9e` 的 `20k.txt`（`4ed6e5336d7760d281f7e72df31827da880c861363e820d8c65666b0f10d9ac0`），按行号当频率名次，不在表里的词排最后。该仓库 `LICENSE.md` 写的是 LDC 许可、Norvig 的 MIT 和合理使用，照抄进 `SOURCE.md`。
- 孩子不该看到的词：同一仓库 `google-10000-english.txt`（`9c965d384526facc59260e94f8ccff1582633fa385004abe1455ed457062acbc`）减去 `google-10000-english-no-swears.txt`（`d6b3e04f1ac30be6525d41474166c0bff28486ecd8c48dcb0ab9c7c9cc05ed86`），共 106 个词，其中 96 个在雾凇词表里。

**开工第一步**：按下面第 1 项的办法下载并核对上面所有 SHA-256 和提交号；哈希或提交号对不上就停下来问，不要改数字继续。

## 要做的

1. **英文词表**（新工具 `tools/imedict/build_en.py`）：照 `build_base.py` 的做法，下载上面五个文件，按 `sources.lock.json` 里的 SHA-256 核对，缓存在 `build-deps/imedict`，哈希不对就停。只收纯 ASCII、长度不超过 39 字节、不含制表符的词；编码按雾凇的拼写规则只留字母并转小写（`melt_eng.schema.yaml` 的 `derive/[^a-zA-Z0-9]//`、`derive/^.+$/\L$0/` 一类）；单个字母不收（`erase/^[a-z]$/`）；同一编码有几种写法的，保留词表原样的大小写。中英混合词里的汉字要用字体字符集过滤，和 `build_base.py` 一样保证随包字体能显示。输出放进资源包 `ime/` 目录（`tools/asset_pack.py` 本来就打包 `ime/*`，不用改它），旁边放许可证。格式你定：可以沿用 IKD1（`tools/imedict/imedict.py`、`pinyin.c` 的 `dict_ok`），也可以用更紧凑的排序词表加名次；**每个偏移都要校验**，坏文件不能让设备挂掉。另生成 `en.json` 记录词数、大小和哈希。
2. **装载**（新文件 `system/services/text/english.c`，接口写进 `text_internal.h`）：第一次需要英文候选时，在后台执行器读资源包（照 `py_want_dict`/`load_job` 的做法；压缩条目由 `hal_asset_map` 解压到 PSRAM），登记内存预算和回收器（内存紧又没人打字时交还，下次再读）。读不到或文件坏了：计数 `text.en_bad`，中文照常，英文模式下候选条照旧（不白屏、不报错卡住）。词表在固件里，不依赖卡上的文件（D-138）。
3. **中文模式：英文当次候选**（在 `compute_cands` 里，拼音引擎出完候选、按用户词频排好以后再插，两种引擎都走这一处）。组合串全是字母、至少 2 个字母时：
   - **整词**：组合串等于某个英文编码。中文第一个候选吃满整个组合串，英文放第 2 位；中文吃不满（比如旧引擎打 `hello`），英文放第 1 位，按空格就上屏英文。降低名单里的编码（`rime_ice.schema.yaml:131-152`）一律放第 2 位。
   - **补全**：组合串至少 3 个字母、是英文编码的前缀，最多 3 个，按名次排，放在吃满组合串的中文候选后面，**永远不放第 1 位**。
   - **中英混合词**（cn_en）：组合串等于编码时排在吃满组合串的中文候选后面，不放第 1 位。
   - 同一个词（不分大小写）只出现一次。九宫格不出英文（见第 8 项）。
4. **英文模式：按前缀补全**。字母照旧直接进正文（`tests/ime_keyboard1.c:246-248` 断言这一点，不改）。光标前紧挨着的连续英文字母（没有选区）至少 2 个时，候选是以它开头的词：完全相同的词排第 1，其余按名次，最多 18 个（3 页）。大小写照 `autocap_filter.lua`。选中后把光标前这段字母换成这个词，再加一个空格；紧接着打 `, . ? ! : ;` 时，把这个自动加的空格换成标点。按空格、回车只是空格和回车，**不自动改词**。`consumed` 在英文模式下表示“替换光标前几个字节”，写进 `textinput.h` 的注释。这时不出中文联想，AI 接话也让位：光标前是英文字母时先给补全，空格或标点以后才轮到 AI 接话（`ink_text_candidates` 430 行的先后顺序）。**九宫格布局下的英文模式不出补全**（和第 8 项一致，英文用全键盘）。
5. **候选标出种类**：`ink_text_cand_t` 加一个字段（例如 `uint8_t kind`：中文、英文、中英混合），键盘快照里英文候选写“英文候选[i]=hello”，让 AI 和测试分得清。`keyboard.c` 只改快照那一两行。选词照旧只走 `keyboard.pick`（按钮和 AI 同一个入口），不新增能力，也不新增设置（不加开关，默认就开）。
6. **不学英文词，不碰引擎状态**：照雾凇 `enable_user_dict: false`，选英文候选不写 `text.user_freq`，不调 `py_google_pick`，也不改 `/me/ime/google.user`。选完英文以后谷歌解码器回到干净状态：先选“我爱”、再选英文、再打拼音，不能接上前面那半截。不要靠键盘重画来清这个状态（测试里不挂键盘也要对）。中文模式下，如果光标前紧挨着英文字母或数字，选英文词时先加一个空格（`en_spacer.lua`）。
7. **顺手修谷歌联想的取字**（同属文本服务，归本任务）：`text_service.c:399` 的调用处改成只传**光标前的正文**（`TS.value` 截到 `TS.cursor`）；`py_google_predict`（`google_pinyin.cpp:146` 起）只取光标前最后 `kMaxPredictSize`（=7）个字，不再从开头取 `LETTERS`=40 个；光标前是英文字母，或者在英文模式下，不出中文联想。
8. **九宫格英文**：雾凇的 `t9.schema.yaml:28-34` 给九宫格也挂了英文，但 keyboard-2 照 Mogo 定的是“九宫格只打中文拼音，英文用全键盘”。这次不做，写进报告“要用户定”，附推荐（不做）和理由，不要自己定成“不需要”。
9. 设计文档 `docs/system/system-services-design.md` §9.2（“拼音引擎”，561 行起）末尾加一段，写英文候选的排法、词表来源和“不学词”。

## 不变的规矩

- 密码框不出候选、不学、不进日志、不给 AI；数字、电话、网址框不出英文候选。日志里不写候选词，只写计数。
- 孩子（当前家人是孩子）连整词都不出那 106 个词；大人只有完整拼出时才出，不作补全。
- 家人隔离不受影响（英文不学，本来就没有按人的数据）。
- 刷新：英文模式每按一个键，只刷组合区和候选条（D-127），不整屏刷新、不重画键盘。
- 文字大小：不加新控件；候选条跟系统“文字大小”一起缩放，长英文词在最大字号下照 keyboard-2 的规则处理（放不下就翻页，不缩字）。
- 旧引擎仍是默认。所有验收在 `text.google` 关和开两种情况下都要跑。
- 测试不许放宽、跳过或改时限。只有本任务故意改变的行为（比如中文模式打 `hello` 按空格现在上屏 hello）才改已有测试，而且要一样严格；改过的每条测试都写进报告。

## 范围和预算

- 只改：`system/services/text/`（`text_service.c`、`text_internal.h`、`google_pinyin.cpp` 里联想那一处、新的 `english.c`）；`system/include/ink/sys/textinput.h`（候选结构加字段、注释）；`system/ui/shell/keyboard.c` 只改快照里候选标签那一两行；`tools/imedict/`（新的 `build_en.py`、`sources.lock.json` 加五条、`README.md` 加一节、`licenses/` 加许可证）；主工程 CMake 和 `ports/esp32s31` 组件里生成词表、编译新文件的那几行；输入法测试（新文件放 `tests/ime_lexicon/english*`，单元测试用的小词表放 `tests/fixtures/ime/`）；`docs/system/system-services-design.md` §9.2；`docs/tasks/ime-engine-2/`。
- 不碰：`system/services/text/vendor/googlepinyin/` 的上游原文；`vendor/kiikoread/`；键盘界面其余部分和键盘测试的业务断言（归 `keyboard-2`）；`ui_theme.h`、`theme.c`、`textfield.c`、`ui_list.c`；设置注册表（不加设置）；`tools/asset_pack.py`（`ime/*` 已经会打包，真要改就只改词库那几行，并写进报告）。
- 共享文件：新文案（比如快照里的“英文候选”）的英文追加到 `assets/i18n/en.tsv` 末尾；如果能力目录有变化，用 `python3 tools/capabilities.py` 重新生成 `docs/system/capabilities.md` 后提交。合并冲突由主会话处理。同时在跑的 `ai-panel-1`、`shell-sweep-1`、`settings-d137` 若也要改 `text_service.c` 的 87 行（assist_allowed）、384–460 行或 `keyboard.c` 快照区，主会话合并时先合它们再合本任务。
- 产品代码不超过 **520 行**（C，含 `textinput.h` 和 `keyboard.c` 的改动；原稿 300 行装不下装载、校验、回收、大小写、过滤、光标前替换和状态清理）；生成工具（Python）另计，不超过 250 行；测试不计。超了先停下写原因。
- 固件：代码增量不超过 **16 KB**；资源包净增（压缩后）不超过 **120 KB**；装载后 PSRAM 不超过 **350 KB**。超了停下写清差多少，不删词凑数（只能按第 1 项的规则收词）。
- 开工前提：`keyboard-2` 已合入（它改测试和键盘快照，候选条截图和刷新统计办法也要用新键盘；目前仓库里还没有它的合并提交），基于那之后的 `claude/system-layer`。没合入就不要开工。

## 验收

1. **照雾凇的用法走一遍**（模拟器脚本，每一步截图并断言，旧引擎和谷歌引擎各一遍）：备忘里中文模式打 `women` → 第 1 个是“我们”，第 2 个是英文候选 women；打 `rug` → 中文第 1，rug 第 2（降低名单）；旧引擎打 `hello` → hello 第 1，按空格上屏 hello，组合串清空；接着打 `world` 选英文 → 正文是“hello world”（自动隔一个空格）；打 `hel` → 补全里有 help、hello，并且都不在第 1 位。切到英文模式，打 `Hel` → 候选里有 Hello；选中后正文末尾是“Hello ”；再打 `,` → “Hello,”；打 `tomo` → 候选第 1 页有 tomorrow；打 `HEl` → 出 HELLO。同一条路径用能力调用（`keyboard.key`、`keyboard.pick`，来源 AI）再走一遍，每一步正文和候选和点按一样；快照里英文候选标着“英文候选”。
2. **中文不退步**：英文候选打开（默认）时，现有 90 条用例（`tests/ime_lexicon/cases.tsv`）在两种引擎下，每条的首选都和没有英文时一样，**唯一例外是一份有意变化名单**：整串恰好是英文编码、而该引擎的中文吃不满因此英文按第 3 项排第 1 的用例；名单由本任务先跑出来，逐条列出并说明理由，名单之外不许有任何首选变化。前五命中数不减少（旧引擎首选 83、全拼 71/73、前五 73/73、简拼 12/16 这些硬断言一条不放宽；名单里的条目按“中文候选仍在前五”计）；ime-engine-1 的 20 条整句结果不变。逐条差异写进报告（`ai`、`you`、`women`、`beijing`、`shanghai`、`zsh` 这几条同时是英文词，要单独列出来）。
3. **英文用例**：新建 `tests/ime_lexicon/english_cases.tsv`，30 条，分五类：中文模式整词 10 条（目标在前 2 位，10/10）、中文模式补全 5 条和英文模式补全 5 条（目标在第 1 页，至少 9/10）、大小写 5 条（5/5）、降低名单 5 条（都在第 2 位，5/5）。两种引擎的前五结果都列进报告。
4. **不该出的**：密码框不出候选、快照里没有正文；九宫格（含九宫格布局下的英文模式）不出英文；孩子打那 106 个词里的任意一个都不出，大人前缀补全也不出；选英文候选后，`text.user_freq` 和 `/me/ime/google.user` 的哈希都不变；日志里搜不到候选词。
5. **引擎状态**：不挂键盘的单元测试，谷歌引擎下打 `woaihello`，先选“我爱”，再选英文 hello，再打 `nihao`，第 1 个候选是“你好”，不是接着前面那半截组出来的词。
6. **联想取字**：谷歌引擎下正文 60 个字、光标放在第 20 个字后面，联想依据的是光标前最后 7 个字（写一组能分出前后的例子）；英文模式下没有中文联想。反向对照：改回传整段正文、从开头取 40 个字，这条必须失败。
7. **反向对照**（每条撤掉以后对应的测试必须失败）：撤掉“中文吃满就放第 2 位”，`women` 失败；撤掉降低名单，`rug` 失败；撤掉孩子过滤，第 4 条失败；撤掉 en_spacer，`hello world` 失败。
8. **刷新**：英文模式按 10 个字母，用模拟器显示统计断言刷新面积只覆盖组合区和候选条，没有整屏刷新（沿用 keyboard-2 的统计办法）。
9. **缺词表**：用不带 `ime/` 的资源（`tests/ime_lexicon` 已有的无词库配置）启动，中文照常，英文模式没有补全，不崩、不白屏，`text.en_bad` 或“未装载”有计数。
10. **改前改后截图**：候选条上的 `women`、`hello`、`hel`（中文模式），`Hel`、`tomo`（英文模式），默认和最大文字大小各一套，旧引擎为主，`women`、`hello` 另出谷歌引擎默认字号一套；放 `docs/tasks/ime-engine-2/`，单张不超过 300 KB，先看干净的整页图。
11. 主工程普通全量 ctest 过（含 `ui-scale`，先清掉旧的 test-out 再跑）；ASan 跑 `ime-*`、`integration-text-*` 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告；`gcccheck` 查新改的 C 文件。报告写：S31 代码增量、资源包增量、PSRAM 峰值、词表装载耗时、宿主机上每按一键到出候选的 p50/p95（英文开和关对比；S31 上的估算标【估算】）、产品代码行数、改过的已有测试，以及上机核对清单（真机打 `women`、`hello`、英文模式补全的出字时间，最大字号下的长英文词）。

## 顺手核对（不在预算内，属实就写进报告）

- `pinyin.c:127`、`:255`：资源包里的旧词库（`ime/base.ikd`，当前构建 968,735 字节，约 0.95 MB，已核实）按“映射在闪存里，不占内存、没什么可交还”处理；但 fw-size-2 以后压缩条目是由 `hal_asset_map` 解压到 PSRAM 的（`ports/esp32s31/hal_assets_s31.c:29-46`），大约 0.95 MB 可能永远不交还。核对属实就写进报告，由主会话决定要不要另开修正。
- 同样核对谷歌解码器词库（`assets/ime/dict_pinyin.dat`，1,068,442 字节，超过 `pinyin.c:26` 的 `DICT_MAX` 1 MiB）实际走哪条装载路径、压缩后会不会也常驻 PSRAM、有没有回收器，一并写进报告。
