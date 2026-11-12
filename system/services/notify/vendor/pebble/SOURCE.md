# PebbleOS 通知逻辑来源

来源：https://github.com/coredevices/PebbleOS ，固定提交 `aa0c27e8330d010e9202f9a706a56002db5861ee`。查阅：2026-11-12。根许可证 Apache-2.0；三个文件保留 Google LLC 的 SPDX 文件头。

| 上游路径 | SHA-256 | 修改 |
| --- | --- | --- |
| `LICENSE` | `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30` | 无，逐字节保留 |
| `pebble/services/notifications/do_not_disturb.c` | `fa14e67fbdcc3a2f8e38edbb5b8ccc3846402a3137e399a6c3bc81de47ff10d0` | 无，逐字节保留 |
| `pebble/services/notifications/alerts_preferences.c` | `f8f4c41551d8ea56af6465f811406e2751d2c3d1d1c10f3d6cff56ff13f52e60` | 无，逐字节保留 |
| `pebble/services/timeline/timeline_actions.c` | `c1fc0ccc91fb14c7e04463d9d1e6f971111c606698fe96a710f84591b685189f` | 无，逐字节保留 |

原文件整份入库；仅接入可独立复用的完整函数，存储、调度和动作派发使用 ink 现有服务，不链接 Pebble 的 UI、手机传输或第二个通知库。提取和适配记录如下。

## 接入边界

- `schedule.inc`：从原文 `do_not_disturb.c:149–176` 提取 `prv_next_days`、`prv_minutes`、`prv_is_in_slot_period` 三个完整函数，无修改；SHA-256 `bb2fe71fd8dff14e534df288cae2bcbdbf01d6e4af03a40285ca809dda1e6e55`。实际编译进 `notify_model.c`，类型、星期掩码与分钟常量由薄适配提供。原文三个 `.c` 仅归档，不单独编译。
- 三种来源规则见 `do_not_disturb.c:242–265`；手动关闭不覆盖仍在生效的时段，遵守本任务的“任一来源生效”与沿用开关语义。
- `alerts_preferences.c:34–55,733–740` 的例外偏好接到成员 `notify.prefs`，过滤采用任务规定的 AOSP 规则；不接 Pebble 喇叭和背光硬件。
- 五条默认回复依照 `timeline_actions.c:794–827` 翻译，菜单与手机传输不编译，动作交给 ink 能力。
- `notify.prefs` 记录 1 保留渠道开关，记录 2 存勿扰和回复；复用 JSON 记录，不另建通知库、线程、队列或定时器。
- 定时复用 `time.minute` 和 `time.changed`，提醒变更看 `alarm.revision`；忙时复用 `ink_alarm_next` 和 `ink_alarm_occurrence`。
