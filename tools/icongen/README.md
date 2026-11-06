# 图标工具

`node tools/icongen/extract.mjs "$INK_DESIGN" assets/icons` 从用户提供的 `Icon`、`IconSheet` 导出几何，保留来源哈希，不复制画板。图标清单和每个 SVG 的 SHA-256 在 `assets/icons/manifest.json`。

CMake 用 `ink-icon-raster` 转为五档尺寸的 INKI：只存墨量（16 级）并压缩，第一次用时解压到 PSRAM。白色图标用同一份。

`ai` 图标是后来单独画的（D-109），不是设计稿导出的：重新导出后要把它改回来。

构建时渲染器：[NanoSVG](https://github.com/memononen/nanosvg/tree/239e102ec2c691f2902e20ace2ed36ee4a35cfe6)，固定提交 `239e102ec2c691f2902e20ace2ed36ee4a35cfe6`，zlib 许可证在 `vendor/LICENSE.txt`。`nanosvg.h` SHA-256 `e34fd5d084be106cea972d19ce5d27fd96d17ba89f8d06bdceee058420c8b2b0`，`nanosvgrast.h` SHA-256 `79a9c5f4db19debf9f3a648a1589e96d92854f245a5cb4f3d823f263785234d8`。查阅和下载日期：2026-11-05。
