# E-KLCD-02/07/10 本地媒体定位说明

日期：2026-10-27。仅检查本地已保存资料，未联网，未重新请求 Zendesk 文章/栏目，未下载图片、视频或 GIF。

## 结果

三个 E 没有保存原始文章 HTML；现有原件是 web 工具返回的文本快照：

- `work/khan-library-controls-deepening/text/articles-360048828572.txt`（E-KLCD-02）
- `work/khan-library-controls-deepening/text/articles-4457349308059.txt`（E-KLCD-07）
- `work/khan-library-controls-deepening/text/articles-4403614100109.txt`（E-KLCD-10）

文本保留了文章区行号、图片链接编号和文件名，但没有保留这些编号对应的真实附件 URL。因此 `local-media.json` 共登记13个图片位置，`media_url` 全部为 `null`，并明确标为“不能定位、不得猜路径”。本地也没有与这13个文件名匹配的图片。

已有 `look-at-my-map.png` 和 `mute.png` 的 URL、文件及 SHA 可核，但它们来自其他文章/E，不属于 E-KLCD-02/07/10，已排除。另一个旧 attachment URL 曾超时，且无法从本地证据证明属于这三个目标 E，也未采用。

## 给根代理的最多6张优先项

1. E-KLCD-10，正文 L59：`5-25-21_Report_Dialog_Assignments_iPad.png`，最可能显示单课多次尝试明细。
2. E-KLCD-10，正文 L66：`Progress_article.png`，邻接低分 Main→Basic 支架说明。
3. E-KLCD-10，正文 L35：`ClassReport_Assignments.png`，Assignments 报告入口与完成/尝试范围。
4. E-KLCD-10，正文 L35：`ClassReport_AllProgress__1_.png`，All Progress 汇总与返回层级。
5. E-KLCD-07，正文 L36：`Home.png`，邻接绿色 Play 与 Kodi 求助入口说明。
6. E-KLCD-07，正文 L79：`Progress_checkmarks.png`，活动后可见进度标记。

这些只是由正文位置和文件名排序，尚未看到画面。不能据此补写控件、错答反馈、提示触发、重试结果或恢复行为。

E-KLCD-02 只有 L27 `OlloStudentView.PNG`，更像 Learning Path 入口概览；相较上面6张，与即时提示/尝试报告/返回路径的关联较弱，保留在完整 JSON 清单中但未进优先6张。

## 执行记录

- `find` 与 `rg` 定位 Khan 本地文本、manifest、图片记录及 attachment URL：exit 0。
- `cat`/Python 读取三个目标文本、`manifest.json`、`source-map.json`、`web-images-response.json`：exit 0。
- `python3 work/research/batch42-child-inventory/extract-local-media.py`：首次生成成功 exit 0，发现12处文本行、5个优先项；修改为支持同一行多个图片时一次缩进错误 exit 1，未覆盖既有 JSON；修正后 exit 0，最终13个图片项、6个优先项。
- `python3 -m json.tool work/research/batch42-child-inventory/local-media.json`：exit 0。
- `sha256sum work/research/batch42-child-inventory/local-media.json`：exit 0，SHA-256 `414a809a86eafe172753a8a268f671c16d9c315784df6b3438619aca392c20dd`。

## 状态变化与未知

只新增/更新 `work/research/batch42-child-inventory/` 下的扫描、脚本、JSON和本说明；未修改 `docs/`。真实附件 URL、图片内容和当前线上可访问状态均未知；没有联网补齐。
