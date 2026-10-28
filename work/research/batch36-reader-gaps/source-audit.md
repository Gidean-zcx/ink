# Batch 36：掌阅 App／掌阅硬件／墨案本地资料缺口审计

审计日期：2026-10-27。范围仅限本地已有文件；没有联网，没有修改 `docs/`。本次新增观察仅用于向 root 提供增量候选，不把商品页宣称或静态拼图升级为操作流程。

## 结论

本地确有一组“已下载、正式案例只选择性拆解”的资料：`work/extended-readers/ihw-00.jpg` 至 `ihw-33.jpg` 共 34 张 Ocean4 长续航版 2025 商品详情图。旧构建脚本只把第 16、20、26 图（1 起算）归档为独立证据；本次查看了既有 6 张接触表，并对第 14、27、31 图查看原图。可形成 3 个有限的静态增量候选，其中只有第 14 图能明显补强核心 AI 阅读界面的可见入口；仍不能形成点击后的连续操作或把 Ocean 分支升为 D2。

M-014 掌阅 App 没有发现已下载而未读的应用内截图、手册或连续界面材料。M-054 墨案的本地 HTML 确实引用了 9 个操作 GIF／图片，但媒体文件本身没有下载到工作区；正文已经拆解，不能把 HTML 中的远程 URL 算作已观察媒体。

## M-014 掌阅 App

### 当前覆盖

- 案例：`docs/research/interaction-library/cases/M-014-ireader.md`。
- 台账：`docs/research/interaction-library/batches/ireader-controls-deepening-inventory.csv`，46 行，覆盖手机功能声明、版本日志和 MatePad Paper FAQ；正式批次 coverage 文件只有表头，权威合并覆盖在 `docs/research/interaction-library/coverage.csv`。
- F-M014-01／02／03 仍为 D1 且 `visual_status=not_observed`；F-M014-04／05／06 为文字路径 D2，仍没有视觉观察。
- 本地原件：
  - `work/ireader-controls-deepening/app-store.html`、`app-store.txt`：App Store 描述和版本历史，已读并建目录；不是应用内操作手册。
  - `work/chinese-apps/ireader-huawei.html`、`ireader-huawei.txt`：MatePad Paper FAQ，正文已按 18 问和 4 个“笔迹不见”子分支拆解。
  - `work/chinese-app-depth/ireader-privacy.html`、`ireader-privacy.txt`：政策正文，已按业务层级拆解。

### 已下载未拆解候选

没有。工作区内未找到掌阅 App 的本地应用内截图、录屏、PDF 手册或帮助媒体。`app-store.html` 虽保存整页 HTML，但现有记录只支持商店文字；没有本地下载的 App Store 截图可供离线观察。

### 具体缺口与避免重复

- F-M014-01：仍缺手机端选书→开书→选词／选段→查词／划线→撤销→回原文；不要重复读商店描述或用户评论。
- F-M014-02：仍缺导入菜单、解析／损坏／重复反馈，以及原文件、阅读位置、笔记各自同步和恢复；政策用途说明不能补操作。
- F-M014-03：仍缺 AI 入口、引用定位、生成／取消／保存，以及听书暂停、定时、换声、读听锚点和中断恢复；版本日志不能补操作。
- F-M014-04／05：MatePad Paper FAQ 已全文拆解；正文图片尚未取得本地观察件，但这不是“已下载未读”候选。
- 已用或受限路线：官方 App Store文字、2024 隐私政策、华为 MatePad Paper FAQ、年报摘要；掌阅主站搜索曾受 robots 限制，手机应用内帮助正文未取得。

## M-053 掌阅硬件

### 当前覆盖和资料结构

- 案例：`docs/research/interaction-library/cases/M-053-ireader-hardware.md`。
- 没有独立的 M-053 inventory CSV；覆盖分散在：
  - `docs/research/interaction-library/batches/extended-readers-coverage.csv`：F-M053-01 至 05；
  - `docs/research/interaction-library/batches/chinese-reader-deepening-coverage.csv`：F-M053-06 至 08；
  - 合并权威表 `docs/research/interaction-library/coverage.csv`。
- 既有 8F：F01 D1、F02 D2（只因 Smart4 Pro 文字路径；Ocean 分支仍 D1）、F03 D1、F04 D2（只因 Smart4 Pro 文字路径；Ocean 分支仍 D1）、F05 D1、F06—08 D2。
- `work/extended-readers/ireader-images.json` 给出 34 张 Ocean4 商品详情图的原始顺序；对应文件为 `work/extended-readers/ihw-00.jpg` 至 `ihw-33.jpg`。`work/extended-readers/write-batch.py` 第 24—29 行明确只把索引 15、19、25（详情第 16、20、26 图）归档为 E-IHW-02／03／04。

### 增量候选

1. **Ocean AI 阅读静态入口（优先）**
   - 本地原件：`work/extended-readers/ihw-13.jpg`，商品详情第 14 图（1 起算）；原 URL 顺序见 `work/extended-readers/ireader-images.json` 索引 13。
   - 可见内容：书城页面有“AI阅读”页签和“精选AI书单”；书内画面顶部可见“目录／AI浓缩／书签”三个页签，当前展示 `AI浓缩` 内容。
   - 关联 F：F-M053-04。
   - 旧覆盖：Ocean 分支仅宣传 D1，且 F04 `visual_status=not_observed`；Smart4 Pro 的 D2 来自另一型号的官方更新简介。
   - 增量候选：把 Ocean 分支改为“静态界面入口已观察”，`visual_status` 最多为 `static_reference_only`。它支持“AI阅读书单→书内 AI浓缩是可见的信息架构”，不支持点击顺序、生成过程、适用范围、引用、重试或保存，不能升 D2。

2. **阅读方向的静态三态示意**
   - 本地原件：`work/extended-readers/ihw-26.jpg`，详情第 27 图。
   - 可见内容：左手竖持、横屏阅读、右手竖持三种示意，正文称重力感应旋转。
   - 关联 F：F-M053-01。
   - 旧覆盖：F01 D1、`static_reference_only`，已知道“重力旋转”，但正式独立证据没有归档该图。
   - 增量候选：仅补“商品页明确画出三种阅读方向”。没有旋转触发、锁定、布局重排、页码锚点或返回步骤，深度不变；价值低于第 14 图。

3. **朗读／录音和输入法能力静态图**
   - 本地原件：`work/extended-readers/ihw-30.jpg`，详情第 31 图。
   - 可见内容：文字声明“AI朗读书籍直接听、带有麦克风支持录音”；下半图列拼音26键、拼音9键、笔画、五笔四种键盘布局。
   - 关联 F：F-M053-05；输入法若后续建功能目录，可作为系统输入目录线索。
   - 旧覆盖：F05 D1、`static_reference_only`，E-IHW-01 的定位本就包含详情图 31—33，案例已记录朗读／录音能力声明。
   - 增量候选：没有新增播放器或录音控件，基本属于旧证据的精确定位，不建议新建流程或升深度。

### 已读／无需重复

- `work/extended-readers/ihw-15.jpg`（详情第 16 图）已经归档为 E-IHW-02：PDF 重排前后和选项弹层。
- `ihw-19.jpg`（详情第 20 图）已经归档为 E-IHW-03：七个手势区域。
- `ihw-25.jpg`（详情第 26 图）已经归档为 E-IHW-04：两枚实体键及自定功能声明。
- `work/chinese-reader-deepening/tango2-cover.jpg`、`tango2-overview.jpg` 已观察并归档；`tango2-last.jpg` 只是售后／测试条件说明，本次查看后未发现核心阅读操作。
- 其余 Ocean 图主要是屏幕、前光、残影、外观、续航和规格宣传；本次通过 `ihw-sheet-0.png` 至 `ihw-sheet-5.png` 完成离线筛查，没有发现新的连续操作链。

### 仍缺

F01 的导入→书架→开书→返回→续读、F02 的重排进度／取消／失败／原页切换、F03 的改键设置和恢复默认、F04 的生成／引用／重试／保存、F05 的播放器／录音控制仍没有本地原件支撑。Smart4 Pro 的更新简介直接正文曾返回 412；现有仅搜索索引文字。完整机内帮助仍缺。

## M-054 墨案

### 当前覆盖

- 案例：`docs/research/interaction-library/cases/M-054-moaan.md`。
- inventory：`docs/research/interaction-library/batches/moaan-controls-deepening-inventory.csv`，82 行；F-M054-01 至 04 为分型号文字／静态图 D2，F05 至 08 为 D1。
- 已下载并已观察的图：
  - `work/moaan-controls-deepening/raw/inkpalm-plus-a.png`、`b.png`、`c.png`：Plus 三图短说明书，已经拆到章节和控件台账；
  - `plus-quick-panel.jpg`、`plus-refresh-panel.jpg`：2022 作者照片，已经拆成快捷项和刷新弹窗静态目录。

### 已下载未拆解候选

没有核心阅读媒体候选。本地保存的是 HTML 正文和两张已观察照片；历史指南页面引用的操作媒体没有下载为本地文件。

HTML 中尚未观察的远程媒体定位如下，供后续单独获取时避免重新发现；它们当前不属于“已下载材料”：

- `work/moaan-controls-deepening/raw/moaan-transfer-2020.html`：方法一 GIF、方法二 JPG、方法三 GIF；关联 F-M054-06。
- `raw/root-moaan-87.html`：蓝牙传输 GIF；关联 F-M054-06。
- `raw/moaan-85.html`：重排 GIF；关联 F-M054-05。
- `raw/moaan-88.html`：移动 GIF；关联 F-M054-07，且正好对应正文“目标文件夹外面粘贴”的歧义。
- `raw/moaan-83.html`：多笔记 GIF；`raw/moaan-84.html`：笔记名称 GIF；`raw/root-moaan-82.html`：导出 PDF GIF；关联 F-M054-08。

这些媒体即使后续取得，也只能先补 2020 年未标型号的历史界面，不能归属 inkPalm5、Plus、W7 或 W8，不能自动把目标型号分支升 D2。

### 具体缺口与避免重复

- F-M054-05：固定机型开书、查词、返回、待机续读仍缺；短说明书没有阅读软件菜单。
- F-M054-06：文件到设备、进入书架、扫描／重名／拒绝加入后的状态仍缺；USB `books` 与 FTP `book` 的正文冲突已记录。
- F-M054-07：移动 GIF 尚未本地取得；正文歧义已保留，不能自行改写目标目录。
- F-M054-08：创建笔画→保存→改名→导出→返回原稿及失败恢复仍缺；历史 GIF 的型号也未知。
- Maitube 电子说明书目标旧 403、ManualsPlus 本地下载旧 403、W8 原文旧超时；本次没有重试。

## 本次精确命令与退出状态

工作目录均为 `/home/workspace/reader`。

1. `pwd && rg --files | rg '(^|/)(AGENTS\\.md|README\\.md|docs/requirements\\.md|M-014|M-053|M-054|ireader|掌阅|moaan|墨案|coverage|inventory)' | sed -n '1,240p'`：exit 0。定位项目约束、3 个案例和相关批次文件。
2. Python `Path.stat()` 统计案例／台账行数，随后 `rg --files work | rg -i '(ireader|moaan|reader|掌阅|墨案)'`：exit 0。定位全部本地工作记录。
3. `cat AGENTS.md`，并用 `sed` 读取三个案例及两个 coverage 文件：exit 0；输出较长发生显示截断，随后按文件分项读取，不把截断当证据缺失。
4. Python `csv.DictReader` 读取 iReader 和墨案 inventory 全字段：exit 0；终端显示较长发生截断，关键行随后通过产品／状态筛选复核。
5. `sed` 读取 `ireader-images.json`、`write-batch.py`、两轮 HANDOFF 和旧研究稿：exit 0；确认 Ocean 34 图中只有 3 图被独立归档。
6. `view_image` 读取 `work/extended-readers/ihw-sheet-0.png` 至 `ihw-sheet-5.png`：6 次均成功；离线筛查 34 张商品详情图。
7. `view_image` 读取 `ihw-13.jpg`、`ihw-26.jpg`、`ihw-30.jpg` 和 `work/chinese-reader-deepening/tango2-last.jpg`：4 次均成功；确认上述静态候选和 Tango 尾图无核心操作增量。
8. Python `csv.DictReader` 筛选 `coverage.csv`、`chinese-reader-deepening-coverage.csv`、`extended-readers-coverage.csv` 的 M-014／053／054，并逐行读取两份 evidence JSONL：exit 0。确认旧深度、视觉状态和证据归档。
9. Python `HTMLParser` 扫描 `work/moaan-controls-deepening/raw/*.html` 的本地媒体引用：exit 0。得到 9 个指南正文媒体引用；`find work -type f` 对 PDF／EPUB／GIF／WEBP 的相关文件筛选 exit 0，确认这些墨案 GIF 没有下载到工作区。
10. `rg -l 'M-053' docs/research/interaction-library/batches --glob '*.csv' --glob '*.jsonl'` 及后续筛选：exit 0。确认 M-053 没有独立 inventory，覆盖分布在两个旧批次。

## 状态变化、未知与边界

- 状态变化：新增本审计文件；查看了已有 6 张 Ocean 接触表和 4 张本地原图。没有修改 `docs/`，没有下载或覆盖任何原始资料。
- 未知：无法从本地状态证明 34 张 Ocean 图此前是否由人逐张看过；可以证明旧构建脚本只独立归档 3 张，正式案例未记录第 14 图的 AI 界面控件。App Store HTML 是否含可远程获取的全部截图不在本次离线审计范围。
- 证据边界：商品详情静态图只支持“画面上可见什么”；不支持动作已执行、状态变化、性能、错误处理或版本普遍适用。墨案 HTML 中的 URL 不等于媒体已下载或已观察。
