# 布局来源

查阅：2026-11-11。CrossMux 固定提交 `a6fd569f7552db4ea1a6550ced7bc06568ae0e61`，独立布局文件适用 MIT（见 LICENSE）。没有搬入其固件、wolfSSL、待机循环、随机数字引擎或第二份历法表。

| 上游文件 | SHA-256 | 本次使用 |
| --- | --- | --- |
| `src/activities/apps/standby/SloppyClockFace.cpp` | `06ce350b4ab3b662e5b0172dd45890ec5841bd54593b44075e62fc8acb766546` | 58–67 行的小时／分钟上下排列；用现有 LVGL 时钟字体，未移植摇动换字形。日期、农历、电量按任务书补上。 |
| `src/activities/apps/standby/ChineseCalendarFace.cpp` | `bf7564e19c7bebb6ae38a6b878a880e6b5f960cfb1f4e60bbbe15dee5d0dc32c` | 177–244 行的公历、农历、干支、生肖层次；307–380 行按日期变化重画的条件。上游实际是单日黄历，月格按任务书补上。宜忌不搬。 |

本地 `faces.c` 是以主题令牌重现上述信息排列的 C/LVGL 适配；未复制上游函数原文。历法统一调用 `ink_lunar_*`。上游节气为近似公式，本地时间服务没有节气接口，本次保留缺口，未把近似算法称为已验证历法。
