# camera-gallery：相机、相册和看图

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/camera-gallery/REPORT.md。

## 要做的

预算 400 行产品代码（第三方库不算，但报告里写清楚）：

- `Camera`：取景、拍照；照片存成文件放到卡上，并进媒体索引（现在的 `camera.capture` 只在内存里留一张 RGB565，拍好了不等于存好了）。
- `Gallery`：相册，缩略图网格，可分页（F4 的网格，缩略图用 `file.thumb`）。
- `Photo`：看一张照片，抖动到屏幕的 16 级灰；删除照现有的确认规则。

规范第 12 节里的相机取景这次做；扫码结果是后面的任务。

## 从哪里看起

- 这些页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 现有的相机能力（在 docs/system/capabilities.md 里搜 camera）；文件服务写文件；媒体索引 `ink_fs_index`（file-index-1）；解码、抖动、缩略图在 system/services/media/。
- 存成 PNG 时，可以把 third_party/stb 里的 `stb_image_write` 用到设备上（现在只给模拟器截图用），报告里写代码大小和内存。

## 边界

- 不改相机驱动和 HAL；缺的写进报告。
- 新代码放 system/apps/camera/、system/apps/gallery/；共用的文件只在末尾加行。同时在做的有 settings-b、backup-pages、search，还有上一批在收尾的六个任务，不要改它们的文件。
- 大缓冲放 PSRAM；不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 拍照后文件在卡上、媒体索引里有、相册里看得到（测试）。
2. 相册分页、缩略图正确（测试）。
3. 看图和删除（测试）。
4. 孩子成员的照片按成员隔离；去掉检查，测试失败（反例）。
5. 每页有 AI 快照；按钮和 AI 走同一个能力（测试）。
6. 全量测试和内存检查通过；产品代码不超过 400 行。
