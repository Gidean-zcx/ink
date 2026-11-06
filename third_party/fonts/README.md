# Outfit 数字字体

来源：[Google Fonts / Outfit](https://github.com/google/fonts/tree/main/ofl/outfit)，下载日期 2026-11-05，原文件 `Outfit[wght].ttf`，本目录仅改文件名为 `Outfit.ttf`。内容 SHA-256：`fc7287273e66929776e2ba54f144fe699080bec29f61bf649d70d871468aeade`。许可：SIL OFL 1.1，见 `Outfit-OFL.txt`。

生成器用 300、400、500 三档字重；88、176 px 只生成数字和时间、数值所需符号。24～60 px 的中文 UI 字库内也使用 Outfit 数字。生成产物不提交。

中文字体沿用 Noto Sans CJK Sans2.004（OFL），常规字体由 `INK_CJK_FONT` 指定。Linux 可装 `fonts-noto-cjk`、`fonts-noto-cjk-extra`。缺 Medium 或 Bold 时，CMake 从固定版本下载到忽略的 `build-deps/fonts/` 并检查 SHA-256；下载失败明确报错。完整来源和哈希见任务报告。
