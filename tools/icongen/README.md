# 图标工具

`node tools/icongen/extract.mjs "$INK_DESIGN" assets/icons` 从用户提供的 `Icon`、`IconSheet` 导出几何，保留来源哈希，不复制画板。图标清单和每个 SVG 的 SHA-256 在 `assets/icons/manifest.json`。

CMake 用 `ink-icon-raster` 转为五档尺寸的 INKI：只存墨量（16 级）并压缩，第一次用时解压到 PSRAM。白色图标用同一份。

`ai` 图标是后来单独画的（D-109，2026-11-10 用户选定的编织立方体），不是设计稿导出的：重新导出后要把它改回来。它的线宽和断口是按 34、43 px 下断口至少留 2 px 浅色调的（line 2.4、solid 3.2），和其它图标的 2.8、4 不同。

天气的 `cloudsun`（多云）、`rain`、`snow` 也是后来画的（D-125，2026-11-10），照 `cloud`、`sun` 的线宽和三种画法，没有设计稿：重新导出后要保留。

相册的 `rotate`（顺时针转 90 度）也是后来画的（gallery-3，2026-11-13）：半径 13 的开口圆弧加箭头，照 `undo` 的线宽（line、duo 2.8，solid 4）和三种画法，没有设计稿：重新导出后要保留。

构建时渲染器：[NanoSVG](https://github.com/memononen/nanosvg/tree/239e102ec2c691f2902e20ace2ed36ee4a35cfe6)，固定提交 `239e102ec2c691f2902e20ace2ed36ee4a35cfe6`，zlib 许可证在 `vendor/LICENSE.txt`。`nanosvg.h` SHA-256 `e34fd5d084be106cea972d19ce5d27fd96d17ba89f8d06bdceee058420c8b2b0`，`nanosvgrast.h` SHA-256 `79a9c5f4db19debf9f3a648a1589e96d92854f245a5cb4f3d823f263785234d8`。查阅和下载日期：2026-11-05。
