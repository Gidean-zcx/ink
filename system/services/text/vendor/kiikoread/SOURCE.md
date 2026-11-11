# kiikoread 九键函数

来源：kiikoread，固定提交 `83942eb`，`main/ui/ui_ime.c:48–51,61–66`。查阅日期：2026-11-11。

仅原文复制完整的 `digit`、`prefix` 两个静态函数和 Apache-2.0 文件头；没有移入个人学习许可的代码、原词库、缓存或分配器。头文件补上标准头以供本工程包含，函数体未改。IKD1 的键不保证 NUL 结尾，由调用处拷贝当前待比较的前缀并补 NUL。

| 文件 | SHA-256 |
| --- | --- |
| `上游 main/ui/ui_ime.c` | `f54f815481be5e761069a1c79704107f67ce28201884ae43aa7b150cd9a598c3` |
| `nine_key.h` | `5daf3ec9e8050036c3d7ad03ada8695d65c3d7f12ced51010a2db326520f1c1c` |
| `LICENSE` | `c2ede378097fb1fcdcd128c47880c918663f475eba53e1e2dc59d95b537c8809` |
