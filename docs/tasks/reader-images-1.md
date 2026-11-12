# reader-images-1：EPUB 插图和漫画页（功能清单 `epub` 插图、`comic`，移植队列 R05）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7、9～12 条）。

现状：阅读器用原厂 v2.0.77 的 EPUB 内核读文字（`apps/reader/vendor/`），书里的插图不显示，漫画类 EPUB 看不了。

## 要做的

1. 照原厂的图片流移植：`$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/main/reader/image_stream.cc`、`epub_document.cc` 里的单图页检测（以及它们用到的解码，先看仓库里已有的 PNG/JPEG 解码能不能接）。整段拷进 `apps/reader/vendor/metalio_images/`，写 `SOURCE.md`。
2. EPUB 里的插图按原厂做法缩放到页面、转灰度（L8，用现有抖动）；只有一张图的页面按“单图页”整页显示（漫画模式），翻页照常。
3. 大图准入：解码前按尺寸估算内存，超限给占位和说明，不崩溃；离开页面释放；透明 PNG 按白底合成。
4. 续读、目录、书签、阅读设置照常；插图不参与文字分页的锚点计算错乱（书签和续读位置不变）。

## 范围和预算

- 只改：`apps/reader/**`（含新 vendor 目录）、阅读测试和夹具（自制的小 EPUB 夹具，图片自绘）、`docs/tasks/reader-images-1/`。不碰别的任务在改的文件。
- 胶水不超过 300 行（拷来的原文不算）；不新增常驻任务、缓存格式。

## 验收

1. 测试：带插图的 EPUB 显示图片（截图像素检查）；单图页整页显示；超大图给占位不崩溃；透明 PNG；书签和续读位置和改前一致；反向对照。
2. 主工程普通全量 ctest 过（含 `reader-*`）；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小、峰值 PSRAM 和翻到插图页的耗时（模拟器）。
3. 截图：插图页、漫画单图页，常规和 120%。
