# Yoto 有限批交接（2026-10-27）

只修改M037案例；新增三个同名批次和一个source-notes。不得再运行build.py覆盖根代理合并结果。未改master、旧batch、README、需求或全局汇总；无账号/设备/内容状态改变。

## 数量

14个新E（E-YTCD-01–14，先全库规范化URL及编号查重，零重复）；4个新F（16–19），现19F。6个旧F共23字段建议；7个旧E仅additional_captures追加建议。123目录=7个一级分类+87文章标题+29App页签/设置。文章分组23内容/MYO、30使用、7新用户、19故障、8Club。目录不表示所有文章正文被分析；订单和第四代分类不展开。

## 正式文件

- docs/research/interaction-library/cases/M-037-yoto.md
- docs/research/interaction-library/batches/yoto-controls-deepening-evidence.jsonl
- docs/research/interaction-library/batches/yoto-controls-deepening-coverage.csv
- docs/research/interaction-library/batches/yoto-controls-deepening-inventory.csv
- docs/research/interaction-library/artifacts/yoto-controls-deepening/source-notes.md

## 合并建议文件

work/yoto-controls-deepening/old-coverage-updates.json：每行完整before/after/changed_fields。
work/yoto-controls-deepening/old-evidence-additional-captures.json：旧E完整before/after；仅追加additional_captures，不覆盖旧的索引证据取得方式。
work/yoto-controls-deepening/case-change.diff：相对本批开始前的实际差异（可用于未跟踪文件审阅）。

旧F字段：
- F-M037-01：4项，evidence_ids / known_branches / gaps / checked_on。
- F-M037-02：3项，known_branches / gaps / checked_on。
- F-M037-03：4项，evidence_ids / known_branches / gaps / checked_on。
- F-M037-05：4项，evidence_ids / known_branches / gaps / checked_on。
- F-M037-07：4项，evidence_ids / known_branches / gaps / checked_on。
- F-M037-11：4项，evidence_ids / known_branches / gaps / checked_on。

## 采集与重要边界

6个正常公开HTML GET：App总览、5分类，均HTTP200。HTML中的script#initial-data data-json内含公开article.body；只解析该页面已有数据，无API调用。新文章E的url是官方文章标准链接，capture_url是真正取得正文的分类页，raw_work_path/raw_sha256指分类HTML，extracted_work_path是该文章记录；不要误写成每篇都直接打开全文。分类数据虽附带其他全文，本轮只逐项读标题、选定文章读正文。

重要来源：
1. https://support.yotoplay.com/en_gb/how-to-set-an-alarm-on-your-yoto-BJIxYtimMl
   取自Using Your Yoto分类，hash=BJIxYtimMl。步骤1–18：App>=2.46、OS>=2.10.7，设置时开机联网、响铃时开机；正文未说明离线/关机唤醒。录音/响铃预听均未执行。
2. https://support.yotoplay.com/en_gb/how-to-factory-reset-your-yoto-player-or-mini-ByE1FKsXzl
   取自Fixing a Problem分类，hash=ByE1FKsXzl。清本机下载、重新设置、家庭绑定仍在，另需解绑；三键/红提示/三秒/右键路径，不是10秒硬重启。
3. https://support.yotoplay.com/en_gb/how-much-content-can-my-yoto-store-for-offline-listening-SJNFKiQGe
   当前型号表将Mini2024与2021–23合并一行列16/32GB且有序列分支，不能据此确认2024本身存在两种装配；型号单列/本机仍缺。longest-time未播放淘汰与旧E-YOT-01 least-played不一致；不能替换成确定LRU或LFU实现。
4. https://support.yotoplay.com/en_gb/safe-sharing-with-friends-and-family-S1vytKsmMg
   原作者所有权、收件人接受、作者更新传播与链接可转发；没有据此证明离线即时撤权。

历史App3.34/3.35 beta与OS2.23.2/特定批次2.23.3仅保留记录；当前实际构建未知。新正文仍混有第四代段落，明确排除绿键20标题与第四代夜灯自动亮度。Player3屏幕自动亮度和夜灯不等同；Mini无夜灯。

## 命令与退出状态

- 初始cat/sed/rg读取AGENTS、README、requirements、M037及旧批/work：exit 0。
- `python3 work/yoto-controls-deepening/acquire.py work/yoto-controls-deepening/tasks1.json`：exit 0，6 HTML HTTP200（App总览复用本轮第一次成功GET）。
- 用inline Python/HTMLParser提取公开HTML initial-data与选定article.body：exit 0；没有安装工具。
- 两张闹钟图片urllib GET：脚本exit 0，但各HTTP403，未保存、未重试。随后一次view_image因本地文件不存在返回工具错误，没有图像观察。失败URL见image-manifest.json。
- web打开帮助根/en_gb返回404，未重试；检索结果保存search-1/2/3.json。
- `python3 work/yoto-controls-deepening/build.py`：exit 0。
- `python3 work/yoto-controls-deepening/check.py`：exit 0 PASS；new_E14/new_F4/case_F19、旧6F和旧7E的before匹配、散列一致、URL/ID零重复、CSV物理LF分别5/124、hands_on/visual均false。
- `git diff --check -- docs/research/interaction-library/cases/M-037-yoto.md docs/research/interaction-library/batches/yoto-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/yoto-controls-deepening-coverage.csv docs/research/interaction-library/batches/yoto-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/yoto-controls-deepening`：exit 0，无输出；未跟踪文件主要由check.py及case-change.diff核对。
- 最终写本交接/差异并检查正式文本无行末空白：exit 0。

check.py会写check-results.json，不要在要求只读时运行。build.py已冻结提示；根代理修改后不要重跑。没有完成总体goal的操作。

## 原件SHA256

- `work/yoto-controls-deepening/raw/app-overview.html`
  `f2d2a15aa4390f1a468f8974bed083f73f5702552a7460306a20e4979ab5658d`
- `work/yoto-controls-deepening/raw/category-cards-content-make-your-own.html`
  `c5f2e000ac1aa6df5b1afb3867ffdd7c6e74e22d8d749af0d0b065c21ddd3dfc`
- `work/yoto-controls-deepening/raw/category-using-your-yoto.html`
  `52cc235dd8f8d5a769d11e164c730ea93958d8fa7a3aed7f70a433e33d330727`
- `work/yoto-controls-deepening/raw/category-yoto-club.html`
  `7f2b4a41fd811f55e546b7771f39372f4125534d4ca886b77d0eead9a9ac1257`
- `work/yoto-controls-deepening/raw/category-new-to-yoto.html`
  `6189ec32a48622bf0acdf2f6d6c9aa9e490d8d9cf88d58dddd57343a2722cb2b`
- `work/yoto-controls-deepening/raw/category-fixing-a-problem-safety.html`
  `1bd9833e216068f5fa09cc4a1e9a8b63dd6dc29a21fd9d95d0882ceddd82c3b9`

关键原文定位：`extracted/article-SJNFKiQGe.txt`第25–26行是Mini两代合列，第38/40行是序列条件，第46行是最长未播放淘汰；`extracted/article-rkmbFFj7fg.txt`第45行是least-played淘汰。最后`rg -n -C 4`只读核对exit 0。
