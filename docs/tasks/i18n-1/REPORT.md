# i18n-1 报告：多语言机制和语言设置（中文、英文）

（Claude 子代理完成；运行环境不让子代理写报告文件，主会话按它的回报整理保存，数字和结论照原文。）

检出 `ink-codex/i18n-1/repo`，分支 `claude/sub-i18n-1`，基线 `59faf152`。

## 结论

代码里写 `TR("中文")`，按当前语言查表，查不到显示中文原文并计数；语言设置中文 / English，切换后当前页马上重画、重启后保持，AI 和按钮都走 `settings.set`；发给中枢的 AI 请求带当前语言；首次开机的全部页面和设置首页已有英文。语言行放在“关于和更新”页（理由见做法 2）。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 切到 English 后首次开机页、设置首页是英文 | 通过 | 【实测】`tests/i18n_lang.c`：AI 切到 English 后 FirstBoot 原地重画，快照 `"title":"Hello"`；BootWifi 有 `Nearby network`；设置首页 9 个名字全是英文 | `ctest -R i18n-lang` |
| 1b 切回中文 | 通过 | 【实测】在“关于和更新”里亲手点 Language 行，马上变回 `语言 / 中文`，设置首页回到中文，快照 `语言=zh` | 同上 |
| 1c 重启后保持 | 通过 | 【实测】同一份 flash 第二次运行：`system.language==1`，设置首页仍是英文 | 同上（restart 阶段） |
| 1d 查不到回落中文并计数 | 通过 | 【实测】`ink_tr()` 返回传入的中文指针，`i18n.missing` 加 1 | 同上 |
| 1e 构建时检查译文表 | 通过 | 【实测】`tests/i18n_tables.py`：提交的表能过、生成表按 strcmp 排序；9 种坏表（占位符不一致、顺序不同、重复键、没有 Tab、两个 Tab、译文为空、缺 `@name`、不是 UTF-8、页面里的 `TR()` 键没有译文）都让检查失败；译文用到的字都在字库里 | `ctest -R i18n-tables` |
| 1f AI 请求带语言 | 通过 | 【实测】`sim_hub_last_ask()` 里有 `"lang":"en"`，面板显示英文回答；`hub/tests/contract/test_language.py`（en、zh、不带字段、坏值），hub 全部 `135 passed、2 skipped` | `hub/.venv` 里 pytest |
| 2 原有测试 | 通过 | 【实测】`100% tests passed out of 307`；中文是默认语言，现有快照没变 | 全量 |
| 2 内存检查 | 通过 | 【实测】build-asan 18 项 `100% tests passed out of 18`（i18n、settings-a、boot-flow、设置和关于页场景、factory-pages、update-pages、ai-answer-long、ai-pages、capability-*） | |
| 2 S31 | 通过 | 【实测】退出 0；`.rodata.I18N_EN` 在 flash 映射区，不占内部 RAM | |

反向对照：撤掉切换语言时的重画、AI 请求写死 `"zh"`、去掉查不到时的计数，i18n-lang 各自失败；去掉生成器的占位符检查，i18n-tables 失败；en.tsv 里加一个字库没有的 `€`，字体构建报错停下。gcccheck 8 个文件 0 处；gccscan、rscan 只有已知的 `navigation.c` `card_objects` 误报。

截图（检出的 `build/test-out/i18n-lang/`）：`switch/snap-FirstBoot-en.png`、`snap-BootWifi-en.png`、`snap-BootNoHub-en.png`、`snap-BootHello-en.png`、`snap-Settings-en.png`、`snap-SetAbout-en.png`、`snap-AiAnswer-en.png`；对照 `snap-FirstBoot-zh.png`、`snap-Settings-zh.png`；重启后 `restart/snap-Settings-en-restart.png`。

## 做法

1. **机制**：`system/include/ink/sys/i18n.h`、`system/text/i18n.c`。`TR(s)` 当场翻译，`TR_NOOP(s)` 给数组里之后再翻译的字符串做标记；`ink_tr()` 带 `format_arg(1)`，`snprintf(buf, n, TR("正在连接 %s"), x)` 仍有 gcc 格式检查；当前语言每次读设置 `system.language`。译文表 `assets/i18n/en.tsv`（一行“中文<TAB>英文”，`@name` 行写语言自称）。`tools/i18n/i18n_gen.py` 构建时检查格式、重复键、UTF-8、占位符种类和顺序，核对迁过的页面里每个 `TR("…")` 都有译文，生成按 strcmp 排序的 const 表（运行时二分查找）；加一种语言只加一个 tsv，设置选项和表自动多一项；主构建和 S31 都生成。`tools/fontgen/ui_fonts.py` 把译文里的字加进字库，缺字构建时报错。
2. **语言设置**：`system.language`（中文 / English，只有大人能改，随设置文件落盘）；改了以后走 `navigation.c` 原有的设置监听调 `ink_ui_redraw()` 重画当前页（和改字号同一条路）。语言行放在“关于和更新”页、“系统更新”下面（globe 图标，点一下换下一种语言）：设置首页和“显示和亮度”页在默认字号下都已排满一页，加一行会把整组挤到第二页、改变现有的中文设置首页；“关于和更新”相当于手机的“系统”，本来就放语言、更新和恢复出厂，而且自带翻页。设置首页快照加 `语言=zh/en`。
3. **AI**：`ai.ask`、`ai.listen` 参数加 `"lang"`；`sim_hub` 脚本可写 `chunks_<lang>`；`hub/` 的 FakeModel 按语言回答、`flow.py` 检查 `lang`（中文默认）；协议文档补字段。
4. **迁移的页面**：`boot_flow.c` 11 个页面的标题、按钮、提示和快照字段名；设置首页的页名、9 个行名、状态值和“家里 N 个人”；`settings_pages.c` 的行名统一走 `TR()`（表里目前只有“语言”，其余回落中文）。
5. **排版**：测试检查每一屏所有标签没有被截断、单行放得下、换行高度够、互不重叠、不出屏；英文按手机系统的常用说法写短（Display、Lock Screen、Offline、While in use / Always on / Wake word off、Bookshelf）。字体用现有字库（Noto 自带拉丁字母，数字用 Outfit）。

产品代码约 340 行（`system/` +164，CMake、S31 CMake、sim_hub、hub、tools 其余），预算 350，不含译文表和测试。

## 没做到的

- 日期时间的显示格式没改（留给后续）。
- 已显示出来的提示语（`B.message`）切换语言后不重新翻译（开机流程里没有切换入口，实际碰不到）。
- 中枢自己出错时回的中文提示没迁。
- 其余页面没迁（SetAbout 本身、AI 面板、状态栏等），英文下这些页面中英混排。
- 语言行是“点一下换下一种”，语言多了要改成选择列表。

## 以后迁其余页面

把字符串包成 `TR("…")`（数组里用 `TR_NOOP` 标记），把文件加进 CMake 和 S31 的 `INK_I18N_SOURCES`，往 en.tsv 加条目，构建会报出所有缺译文的键；每块用 i18n-lang 的标签检查跑一遍英文截图。条目估计（去重后含中文的字符串字面量）：system/ui 约 807、system/apps 约 332、apps 约 18、services 约 479、policy 约 74；界面上要翻的约 1100～1300 条，分 4～6 批。能力标题（约 440 条，只给 AI 看）要不要翻另外定。

## 改过的已有测试

- `tests/settings_a.py`：SetAbout 的行编号列表加 `'system.language'`（这个列表写死了页面上所有行）。

## 新增的机制

`TR()`、`TR_NOOP()`、`ink_tr()`、`ink_lang_code()`、`ink_lang_name()`、`ink_i18n_init()`；设置 `system.language`；构建时生成译文表和检查；协议字段 `lang`；sim_hub 的 `chunks_<lang>`；FakeModel 的 `texts` 参数。

## 要改的文档

决策记录记下 i18n 的机制和语言行的位置；`docs/product/system-ui-spec.md` 补英文用语约定和语言行；backlog：i18n-1 完成、i18n-2 起分批迁移；`docs/system/capabilities.md` 已重新生成；`docs/hub/device-protocol.md` 已补 `lang`。
