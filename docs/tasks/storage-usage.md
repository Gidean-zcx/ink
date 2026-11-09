# storage-usage：存储页的分类占用和清理

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/storage-usage/REPORT.md。

## 要做的

预算 250 行产品代码：

- 分类占用（后端）：书、音频、照片、其它各占多少，按当前成员和整台设备两种口径。用文件索引里已有的类型和大小汇总，不要每次遍历整张卡；索引还没建好时如实说“正在整理”。
- 清理：列出可以安全删掉的东西（临时文件、缩略图缓存、没传完的半截文件），显示能省多少，确认后删。确认照现有规则（能找回来的不弹卡，找不回来的弹）。
- 接到 `SetStorage`：现在“书／音频／照片”几行写着“暂不能查看分类占用”，清理那行也没接；换成真实数值和清理入口。

## 从哪里看起

- 画板 `$INK_DESIGN/png/` 里的 SetStorage；规格 docs/product/system-ui-spec.md 第 7.11 节（备份一行在最上面，空间排在后面）。
- 文件索引和类型：system/services/file/（`file.index`、`file.lookup`、`ink_fs_type_t`）；剩余空间：`file.free`、状态 `storage.free`；临时文件和缩略图缓存放在哪，看 file 服务和 media 服务的实现。
- 存储页：system/ui/shell/settings_pages.c 的 `STORAGE_ROWS`（最上面的备份行刚改成打开 `Backup`，不要动它）。

## 边界

- 只删列出来的那几类；不碰用户文件，不碰数据库和备份。
- 同时在做的有 settings-b（修复中，也改 settings_pages.c 的 SetVoice 行）、kid-2、transfer-pages、remote-app、activity-card、fw-f8；settings_pages.c 里你只改 `STORAGE_ROWS` 和它的接线；共用文件只在末尾加行。

## 验收

1. 卡上放已知大小的书、音频、照片后，分类数值和实际一致（测试）；换成员，个人口径变、设备口径不变。
2. 索引没建好时显示“正在整理”，建好后自动刷新（测试）。
3. 造临时文件、缩略图缓存、半截文件和一个普通用户文件；清理后前三类没了、用户文件还在，省下的空间和显示的一致（测试，反例：清理范围包含用户目录，测试失败）。
4. 页面上的数和 AI 读到的一致；按钮和 AI 走同一个能力；孩子看不到清理（测试）。
5. 全量测试和内存检查通过；产品代码不超过 250 行。
