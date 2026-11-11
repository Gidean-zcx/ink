# 阅读偏好来源

核查日期：2026-11-12。来源：CloudZao/Metalio-E-INK4-Plus，固定提交 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`，MIT（旁边 LICENSE）。

`presets.h` 整段提取 `main/display/screen/book_screen/reader/book_reader_prefs.h` 第一个 enum（档位数、底栏位和默认位）；整段提取同目录 `book_reader_prefs.cc` 的四个默认值声明、`BookMarginPreset`、`kSpacingTable`、`kParaGapTable`、`kMarginTable`。数值、默认值和注释原样。只将 `static constexpr` 换成 C 的 `static const`（7 处），表类型补 `struct`（1 处），添加许可引用和头文件保护。未带入 NVS、FreeRTOS 和原厂产品策略。

界面参照原厂 `reader/book_reader_overlay.cc:93–141` 的显隐和关闭保留正文，以及 `settings/book_settings_sheet.cc:664–751` 的字号加减、行距、段距、边距和底栏分组。它们直接访问 Book_State、BookSession 和 LayoutWorker，无法整函数编译到本系统；用现有 LVGL 主题组件实现同组交互，保留控件原地更新，不照搬 NVS 或线程。

| 文件 | SHA-256 |
| --- | --- |
| 上游 book_reader_prefs.h | `b0bb15124ccd8f085b57a45ab00fc26be4083ad71be0bac81ef9fd3e005f9c7b` |
| 上游 book_reader_prefs.cc | `86182767a60e69c1f6ee9690e88ab3fe18c94eea6082ef83e0419c89787dc462` |
| presets.h | `daab6b9093f99394c1d9d07a21d107fe3ea15d301aa3c7fc26f0f5d774d812aa` |
| LICENSE（原文） | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
