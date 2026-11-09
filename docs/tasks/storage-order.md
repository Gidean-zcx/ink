# storage-order：有人在等的落盘先走，后台写盘不丢输入、不丢录音开头

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/storage-order/REPORT.md。

## 背景

file-index-3 合入时发现的（docs/tasks/file-index-3/REVIEW.md 末节）：

- 存储任务（system/services/storage/storage_service.c）的写盘队列先来先写，每次写都要等墨水屏空闲。开机时后台建索引的几次写盘排在 hub 端到端测试夹具的确认落盘前面，主页从 3 秒内推迟到约 5.4 秒；合入时只好把 tests/hub/e2e.script 第一下点按从 3 秒改到 7 秒。
- 写内部闪存时任务会停顿（模拟器按 S31 建模），碰上短按会丢按键，碰上开始录音会丢开头的声音。file-index-3 在 tests/camera_gallery/camera_gallery.c、tests/integration/media.c 里加了“按键前先等落盘”才通过。合入时试过让索引开机 5 秒后再开始，8 项场景测试在交互时撞上写盘（丢一次点按、丢 90 ms 麦克风），已撤回。
- 后台建索引每批（32 个文件）都强制落盘一次（system/services/file/file_service.c 的 `index_background` 里的 `ink_db_flush`），1000 个文件约 40 次写盘。

## 要做的

预算：产品代码改动不超过 200 行（增删合计）。

1. **写盘顺序**：有人在等的落盘（`ink_db_wait`、确认、设置这类同步等待）排在后台写盘（LAZY 批次、后台作业的刷盘）前面。在存储服务一处做，不要在各个服务里各自处理。
2. **后台写盘避开交互**：用户刚有输入（触摸、按键，`ink_power_last_activity` 这类已有信号）的一小段时间内，以及录音、通话开始前后，不开始后台写盘；有人在等的落盘不受这条限制。时长放在一处可改的值，报告写依据。
3. **后台建索引少写盘**：整卡扫完再落盘一次（或者交给 LAZY 的批量时机），不要每批都强制落盘；“建好”标记仍然只在索引落盘之后写。数据服务单个集合的待写上限是 256 KB（system/services/data/datastore.c 的 `PEND_MAX`），1024 条索引接近这个数，接近上限时怎么办要有测试。
4. 把 tests/hub/e2e.script 第一下点按改回 `@3s`（删掉合入时加的说明），把 camera_gallery、media 两处“先等落盘再按键”去掉，这三项照原样通过。

## 边界

- 不改刷新算法（D-110）；“墨水屏空闲才写”的规则保留。
- 断电语义不变：同步等待落盘的调用返回成功时数据已经落盘。
- 同时在做的有 fw-gaps、sound-clock、phone-2、ui-fidelity，稍后还有 batch5 审计的修复；只改各自的文件，共用文件只加最少的行。

## 验收

1. 第 4 条的三项按原样通过（普通和 ASan/UBSan 两套构建）。
2. 新测试：后台建索引进行中，连续短按 10 次、触摸 10 次，一次不丢；进行中按 AI 键开始录音，`audio.mic.lost_ms` 为 0。做不到为 0 的，报告给出丢失的次数、长度和原因，不能改测试凑数。
3. 前后对比【实测，模拟器】：后台写盘排队时，同步落盘的等待时间；1000 个文件整卡建索引的写盘次数。
4. 全量测试和内存检查通过；不放宽任何预算。
