# reader-continue 报告

（Claude 子代理完成；运行环境不让子代理写报告文件，主会话按它的回报整理保存，数字和结论照原文。）

检出 `ink-codex/reader-continue/repo`，分支 `claude/sub-reader-continue`，基线 `4099f390`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 读到第 N 页回主页，卡片在、进度对 | 通过 | 【实测】`tests/reader_continue.c` 的 `first_run`：读到第 5 页回主页，今日屏快照和 `app.cards` 里都有“《测试长书》第 5 页” | `ctest -R reader-continue` |
| 1b 点卡片回到第 N 页 | 通过 | 【实测】三条路都回到第 5 页（用 `reader.read_page` 的 revision 核对）：今日屏真实点按卡片、主屏那一行的 `shell.resume`、AI 调 `reader.continue` | 同上 |
| 1c 重启后同上 | 通过 | 【实测】第二个进程沿用同一个 flash 镜像和 sd 目录，卡片仍是第 5 页，两条路都回到原页 | 同上 |
| 1d 两位家人各读各的 | 通过 | 【实测】owner 第 5 页、kid 第 2 页，互不可见，重启后仍不串 | 同上 |
| 1e 删书、拔卡 | 通过 | 【实测】拔卡后今日屏显示规范的空状态“还没有开始读书”，`reader.continue` 回执 FAILED、不崩溃，插回后卡片恢复；删书后同样；`shell.resume` 退回书架 | 同上 |
| 1f AI 快照 | 通过 | 【实测】今日屏快照里有卡片的值和 `reader.continue` 动作 | 同上 |
| 2 原有测试 | 通过 | 【实测】`100% tests passed out of 302`；kid-2、kid-3、power-pages、integration-power-menu、ui-shell-scenes 和阅读场景都过，期望一处没改 | 全量 |
| 2 内存检查 | 通过 | 【实测】build-asan 里 reader-continue、kid-3、kid-2、integration-ai-surface、integration-apps、integration-power-menu、power-pages、device-events 共 8 项 `100% tests passed out of 8` | |
| 2 S31 | 通过 | 【实测】`ports/esp32s31/build.sh` 退出 0 | |
| 反向对照 | 通过 | 【实测】撤掉 `reader.c` 的改动，新测试 read 阶段失败 17 条；恢复后两个阶段都过（86 条、76 条） | |

## 做法（只改 `apps/reader/reader.c`，+92 / −4，预算 150）

- 每位家人的数据区新增小文件 `reader.last`（`page=N`、`book=书名`），打开一本书时写一次、每次保存位置时一起写，用 `ink_storage_write_member_atomic`（已删除的家人写不进去），写完 `ink_app_card_changed`。原来每本书的位置文件不变。
- 卡片：当前家人正开着书就用正在读的页；否则用内存里的 `LAST`，并先查书还在不在卡上；不在就不出卡片，今日屏显示空状态。卡片的 `member` 设成当前家人，动作改成新能力 `reader.continue`。
- 新能力 `reader.continue`（接着读上次的书）：不带参数，孩子可用，导航到那本书，进入后照原样从位置文件恢复；找不到回执 FAILED。点卡片、`shell.resume`、AI 都走这一条。
- 不卡界面：换家人后第一次要卡片时先 `ink_storage_prefetch` 预读，300 毫秒后用一次性 `lv_timer` 读出来再让主页重读卡片（最早的同步读让 device-events 因开机时序变化失败；应用框架不能用 `ink/sys/exec.h`）。

## 没做到的

- 卡片上的百分比（规范“继续读大卡：书名、页码、百分比”）：加在文字里会改变关机页标题，按“不改已有期望”撤了；要今日屏把它放成小字，属于页面任务。
- 停在今日屏时插拔卡，卡片不会自己刷新（插拔卡不改 `app.cards_rev`），要等下次进今日屏；这时点卡片回执 FAILED，不出错。
- 卡片标题仍是“在读”（没卡片时才是“继续读”），为了不动关机图上的“在读：……”。
- 刚开机 300 毫秒内卡片暂不出，这时调 `reader.continue` 会失败。
- 恢复备份或清空之后，内存里的 `LAST` 要等换家人才重读（家人编号不复用，影响小）。

## 改过的已有测试

- `tests/integration/ai_surface.c`：能力清单登记 `{"reader.continue", NULL}`（新增登记，没改已有期望）。

## 顺手发现

- 改动前卡片的动作 `reader.open_book` 必须带书名，而今日屏卡片传 `{}`、`shell.resume` 传 NULL，从卡片点进去本来就会参数不合法；这次一并修好。
- device-events 的重放检查对开机时序敏感：开机时界面任务上多一次阻塞读，中间插进来的 `file.changed` 就会让它失败。

## 要改的文档

- `docs/system/capabilities.md` 已重新生成（多 `reader.continue`）。
- `docs/system/ui-dependencies.md` 阅读那一行“继续读卡片，回到上次位置”改成已接通（百分比未做）。
- backlog：kid-3、boot-flow 记下的“回主页卡片消失”“回到上次阅读位置没做”标为已修；百分比和插拔卡不刷新记为后续。
