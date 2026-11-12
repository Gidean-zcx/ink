# txt-gbk：阅读器能打开 GBK 编码的 TXT

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7、11、12 条）。

现状：阅读器的 TXT 只按 UTF-8 读（`apps/reader/**`，2 MiB 上限），GBK/GB2312/GB18030 的中文 TXT 打开是乱码。国内很多 TXT 电子书是 GBK。

## 要做的

1. 照成熟实现移植编码识别和 GBK→Unicode 转换，先看已固定的只读副本：
   - `$INK_DEPS/../ink-hw/upstream/oss/kiikoread-xiugai/main/book/gbk.c`、`gbk.h`、`gbk_table.h`、`book_txt.c`（先读它的 `LICENSE`，确认能用哪些文件）；
   - `$INK_DEPS/../ink-hw/upstream/oss/crossmux/lib/Txt/TxtEncoding.cpp`（MIT）。
   比较许可、表的大小和覆盖范围（至少 GBK 全集；GB18030 四字节可选），选一家整段拷进 `apps/reader/vendor/`，写 `SOURCE.md`（来源、提交、许可、哈希）。
2. 打开 TXT 时识别编码（BOM、UTF-8 合法性、GBK 统计特征）。GBK 的书在分页前转成 UTF-8，或按块转换；内存占用不能超过现有上限，2 MiB 上限照旧。续读、目录、书签、阅读设置照常。
3. 表放在只读数据里，报告写固件增加多少（应用分区只剩约 8%，尽量小）。

## 范围和预算

- 只改：`apps/reader/**`（含新 vendor 目录）、阅读测试和夹具（新增合成的 GBK TXT 夹具，文字自写或公共领域）、`docs/tasks/txt-gbk/`。不碰别的任务在改的文件。
- 胶水不超过 150 行（拷来的原文不算）；不新增常驻任务、文件格式。

## 验收

1. 测试：GBK、GB2312、带 BOM 和不带 BOM 的 UTF-8 各一本，首页和目录显示正确；GBK 书的续读和书签位置对；非法字节不崩溃（显示替换字符）；UTF-8 的书行为不变；反向对照。
2. 主工程普通全量 ctest 过（含 `reader-*`）；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小。
3. 截图：GBK 书的正文页，常规和 120%（`docs/tasks/txt-gbk/`，单张不超过 300 KB）。
