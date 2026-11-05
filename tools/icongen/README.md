# 图标工具

`node tools/icongen/extract.mjs "$INK_DESIGN" assets/icons` 从用户提供的 `Icon`、`IconSheet` 导出几何，保留来源哈希，不复制画板。图标清单和每个 SVG 的 SHA-256 在 `assets/icons/manifest.json`。

CMake 用 `ink-icon-raster` 转为五档尺寸的 INKI 位图（BGRA8888）。实心白图标另生成反色版本，保留内部黑色细节。清单中的图标缺失就拒绝配置。

构建时渲染器：[NanoSVG](https://github.com/memononen/nanosvg/tree/239e102ec2c691f2902e20ace2ed36ee4a35cfe6)，固定提交 `239e102ec2c691f2902e20ace2ed36ee4a35cfe6`，zlib 许可证在 `vendor/LICENSE.txt`。`nanosvg.h` SHA-256 `e34fd5d084be106cea972d19ce5d27fd96d17ba89f8d06bdceee058420c8b2b0`，`nanosvgrast.h` SHA-256 `79a9c5f4db19debf9f3a648a1589e96d92854f245a5cb4f3d823f263785234d8`。查阅和下载日期：2026-11-05。
