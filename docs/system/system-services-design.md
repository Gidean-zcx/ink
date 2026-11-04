# 系统服务详细设计：系统应用和界面依赖的底层能力

起草：2026-11-04。状态：草案，边实现边修订，结果回填 §15。依据：用户要求 R-052～R-058，[底层依赖](ui-dependencies.md)（范围和顺序），[系统架构 v1.0](system-architecture.md)，[底座设计](foundation.md) §4.8、§5.5、§7、§7.1、§8、§10，[底层系统详细设计](system-detailed-design.md) §13 的缺项。

证据边界：接口、数据格式和数字都是本文定的设计，只在模拟器里验证；时延、内存来自模拟器的悲观档；实机没有验证，只有实机能回答的列在 §14。设计里的取舍是助手的建议，新决定记为 D-65～D-72（§14），都可逆，按推荐暂定。

## 0. 这份文档管什么

| 阶段 | 内容 | 章节 |
| --- | --- | --- |
| 先定 | 审视原则和能力总表（R-055）；数据区和系统区（R-057） | §1、§2 |
| 第 1 阶段 | 结构化数据存储和文件服务；时间服务；通知和打断通道；确认卡；应用注册和生命周期 | §3～§8 |
| 第 2 阶段 | 文本输入和剪贴板；音频（焦点、媒体播放、录音、路由、蓝牙耳机模型）；Wi-Fi 管理、成员和 PIN；系统升级（OTA、卡刷，R-056）；其余缺项 | §9～§13 |

不做：界面视觉（用最简单的占位）、真实中枢、写设备。

## 1. 审视原则：划分、AI 原生、能力开放（R-055）

### 1.1 划分规则

每个新子系统都按同一个形状做，别的子系统只能经这些入口碰它：

| 部分 | 放哪 | 说明 |
| --- | --- | --- |
| 机制 | `system/services/<名字>/`，头文件 `ink/sys/<名字>.h` | 不包含 LVGL，不知道界面长什么样（测试 `layering` 检查） |
| 能力、状态、设置、事件 | 这个服务自己登记，一张静态表 | 按钮和 AI 走同一个能力；这是 AI 唯一的入口 |
| 界面占位 | `system/ui/shell/*.c`（系统壳） | 只调用服务的接口，经“呈现者”回调拿到要显示的东西；以后换成正式界面不用动服务 |
| 硬件 | `platform/include/ink/hal_*.h` + 端口 | 服务只经 HAL |

依赖只向下或经软总线。服务之间能用事件的就用事件：比如来电，通信服务发 `call.incoming`，通知服务订阅后拉起打断，通信服务不知道通知服务存在。

```mermaid
flowchart TB
  SHELL["系统壳 system/ui/shell：确认卡、打断、键盘（占位）"]
  APPS["应用与系统应用"]
  subgraph BUS["软总线：能力 · 事件 · 状态 · 设置"]
    direction LR
    NOTI["通知与打断"] --- CONF["确认卡"] --- APPM["应用管理"] --- TEXT["文本输入与剪贴板"]
    TIME["时间"] --- MEDIA["媒体与录音"] --- BT["蓝牙"] --- WIFI["Wi-Fi"] --- MEM["成员与 PIN"] --- UPD["系统升级"]
  end
  FILE["文件服务"]
  DATA["结构化数据存储"]
  STORE["存储服务（唯一碰 Flash 的任务）"]
  HAL["HAL"]
  SHELL --> BUS
  APPS --> BUS
  BUS --> FILE
  BUS --> DATA
  DATA --> STORE
  FILE --> HAL
  STORE --> HAL
  BUS --> HAL
```

### 1.2 AI 原生的七条规则

1. **同一条路径**：用户按按钮和 AI 调用走同一个能力，不写两套。C 接口是给服务和应用用的快捷方式，做的事必须和能力相同。
2. **结构化结果**：读类能力在回执里带 `result`（JSON，和 `outputSchema` 一致），不只有一句话。回执只在回调期间有效；只读调用不去重，重发就重新读一次。
3. **事件到 AI**：能力登记的事件（新短信、闹钟响、文件变了）在中枢任务里转发给中枢（JSON-RPC 通知 `device/event {event, at_s, args}`），只发开放给 AI 的；断网时留最近 8 条，连上再发。
4. **AI 看得到现在的一切**：每次提问带页面快照和系统状态快照（`state`，开放给 AI 的状态项）；有系统浮层（确认卡、打断、输入框）时页面快照就是浮层上的字；`state.list`、`settings.list` 可以直接问。
5. **渐进披露**：能力多了以后整张清单太大。`tools/list` 分页（`cursor`、`nextCursor`），`caps.domains` 给目录索引，`caps.find` 按关键词查（底座设计 §6.5）。
6. **来源和撤销**：记录都带“谁写的”（人还是 AI）；可逆的写操作回执里给 `undo_ms`，`system.undo` 按对象撤销。
7. **AI 的请求只是请求**：外部副作用、不可逆、系统级都过策略关口，停在确认卡；确认由人在设备上做，AI 不能确认。确认的结果以事件告诉 AI。

### 1.3 能力总表

“AI 可调”是经 `tools/call` 能直接用的（有副作用的按等级走确认卡）；“AI 不可调”写明原因。测试 `integration-ai-surface` 启动完整系统，用和中枢一样的路径拉工具清单，逐项核对这张表：多了、少了、缺标题、缺 Schema 都不通过；不给 AI 的能力必须在测试的例外表里写明原因。

| 子系统 | AI 可调的能力 | AI 不可调（原因） | 状态 | 设置 | 事件 | 内部机制（不开放） |
| --- | --- | --- | --- | --- | --- | --- |
| 数据 | `data.collections`、`data.list`、`data.get`、`data.put`、`data.delete`（集合自己声明开放） | 没声明开放的集合（凭据、PIN 校验值） | `storage.user_free` | — | `data.write_failed` | 日志格式、快照、压缩、存储任务 |
| 文件 | `file.list`、`file.stat`、`file.read`、`file.write`、`file.mkdir`、`file.copy`、`file.move`、`file.delete`（确认）、`file.free`；`usb.status`、`usb.mount`（系统级，确认）、`usb.unmount` | — | `storage.free`、`storage.card`、`storage.mode`、`usb.connected` | — | `file.changed`、`file.job_done`、`usb.changed` | 路径映射、临时文件、U 盘模式互斥、USB 协议 |
| 时间 | `time.now`、`time.lunar`、`alarm.set`、`alarm.list`、`alarm.cancel`、`alarm.snooze`、`alarm.dismiss`、`reminder.add`、`reminder.list`、`reminder.cancel`；`time.set`（系统级，确认） | — | `time.synced`、`time.zone`、`time.minute`、`alarm.ringing` | `time.zone`、`time.auto` | `alarm.fired`、`reminder.fired`、`time.changed` | RTC 寄存器、唤醒定时 |
| 通知 | `notify.post`、`notify.list`、`notify.read`、`notify.dismiss`、`notify.clear`、`notify.channels`、`notify.set_channel` | — | `notify.unread`、`interrupt.active` | `notify.dnd` | `notify.posted` | 打断通道本身（来电、闹钟、低电量由系统拉起，AI 不能假装来电） |
| 确认卡 | `confirm.list`、`confirm.withdraw`（撤回自己发起的） | `confirm.accept`、`confirm.reject`（只有人在设备上能按） | `confirm.pending` | — | `confirm.result` | — |
| 应用 | `app.list`、`app.open`、`app.close`、`app.cards` | — | `app.foreground`、`app.cards_rev` | — | `app.state` | 冻结、停止、状态保存的时机 |
| 文本输入 | `text.state`、`text.insert`、`text.submit`；`clipboard.write`；`clipboard.read`（设置打开才行，默认不读） | 敏感内容（`clipboard.read` 不给）；密码框内容（掩码） | `text.focused` | `ai.read_clipboard` | — | 拼音引擎、词库、候选 |
| 音频 | `media.play`、`media.pause`、`media.resume`、`media.seek`、`media.stop`、`media.status`、`audio.record`、`audio.record_stop` | — | `audio.now_playing`、`audio.route`、`audio.listening`、`audio.recording`、`media.state` | `audio.volume`、`audio.child_max`、`voice.handsfree` | `media.finished`、`audio.record_done` | 焦点、功放、I2S、模式切换 |
| 蓝牙 | `bt.scan`、`bt.list`、`bt.pair`、`bt.connect`、`bt.disconnect`、`bt.forget` | — | `bt.connected`、`bt.device`、`bt.available` | — | `bt.changed` | 芯片命令（协议未知） |
| Wi-Fi | `wifi.scan`、`wifi.list`、`wifi.connect`（系统级，确认）、`wifi.forget`（确认） | — | `wifi.state`、`wifi.ssid` | `net.prefer` | `wifi.changed` | 选网、入网步骤 |
| 成员 | `member.list`、`system.switch_member`（孩子到成人仍要 PIN）；`member.add`、`member.remove`（系统级，确认） | `member.unlock`、`member.set_pin`（PIN 只在设备上输入） | `member.current`、`member.authed` | `member.idle_return_s` | `member.changed` | PIN 校验值、盐 |
| 升级 | `update.status`、`update.check`、`update.download` | `system.upgrade`、`system.upgrade_from_card`（底座设计 §6.2：AI 不能发起升级） | `update.state`、`update.version` | `update.auto_download` | `update.available`、`update.result` | 写槽、回滚、自检 |
| 系统 | `state.list`、`settings.list`、`settings.get`、`settings.set`、`caps.domains`、`caps.find`、`system.undo`、`system.home`、`system.full_refresh`；`system.reboot`、`system.shutdown`、`system.factory_reset`（系统级，确认） | — | `power.state`、`battery.level`、`battery.charging`、`battery.low` | `power.lock_after_s` | `battery.low` | 电源轨、睡眠 |

自动检查之外，每轮审计人工对照这张表和代码（[审计任务说明 K 节](../audits/codex-audit-prompt.md)）。

## 2. 数据区和系统区（R-057）

### 2.1 一张总表

先分清**谁的数据**，再定放哪、什么时候清。“系统应用”和普通应用的区别只有两点：随系统提供，卸载不掉；可以在清单里声明访问别的应用数据的能力。它们的数据放在一样的地方。

| 区 | 位置 | 内容 | 谁能写 | 系统升级 | 卸载应用 | 切换成员 | 恢复出厂 | 备份到中枢 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **系统区** | Flash 应用槽 `ota_0`、`ota_1` | 系统、驱动、内置的系统应用和应用的代码（现在是一个镜像，D-08 第一阶段） | 只有升级服务 | 换另一个槽，失败回滚 | 不相关 | 不相关 | 不动 | 不备份 |
| **系统资源区** | Flash `font_data`、`resources` | 字库、图标、提示音 | 只有升级服务 | 自检通过后才写（§12） | 不相关 | 不相关 | 不动 | 不备份 |
| **引导区** | `bootloader`、分区表、`otadata` | 启动 | 只有 `otadata` 能改（切槽） | 不动 | 不相关 | 不相关 | 不动 | 不备份 |
| **设备数据区** | `/user/sys/`（LittleFS） | 设置、成员表、已知 Wi-Fi、闹钟表、升级状态 | 系统服务 | 保留 | 不相关 | 保留 | 清空 | 除凭据外备份 |
| **成员数据区** | `/user/m/<成员>/`（LittleFS） | 这位成员的小而要紧的数据：系统服务的（通知）、系统应用和应用的（备忘、位置、状态） | 这位成员的服务和应用 | 保留 | 清掉该应用前缀的文件 | 隔离，换人看不到 | 清空 | 备份 |
| **成员文件区** | TF `/content/.m/<成员>/` | 这位成员的文件：录音、照片、导入；应用的大文件在 `apps/<应用>/` 下 | 成员自己；成人可看孩子的 | 保留 | 清掉 `apps/<应用>/` | 隔离 | 清空（要确认） | 按清单 |
| **家庭内容区** | TF `/content/`（除 `.m`、`.update`） | 书、音频：全家共用、可重新下载 | 成人；孩子只读 | 保留 | 不动 | 共用 | 保留（可选清空） | 不备份，从中枢重新下载 |
| **缓存区** | TF `/cache/sys/`、`/cache/<应用>/` | 页缓存、字形缓存，随时可丢 | 系统和对应应用 | 可清 | 清 | 不隔离（不放个人内容） | 清空 | 不备份 |
| **升级暂存区** | TF `/content/.update/`、`/content/update/` | 下载的升级包；用户拷进来的升级包 | 升级服务；用户经 U 盘 | 成功后清 | 不相关 | 不相关 | 清空 | 不备份 |

### 2.2 Flash 分区（D-65）

现在设备上是出厂分区表，没有 LittleFS 用户分区。沿用底座设计 §10.1 的建议：

| 分区 | 出厂 | 建议 | 说明 |
| --- | --- | --- | --- |
| `bootloader` + 分区表 | 0x2000、0x8000 | 不变 | 换引导程序才能开回滚（§12.5），只能经 USB 一次写好，要用户同意 |
| `nvs` | 16 KiB | 不变 | 只给 ESP-IDF 自己（射频校准、Wi-Fi 库）；我们的设置走 `/user/sys`（D-57） |
| `otadata`、`phy_init` | 8 KiB、4 KiB | 不变 | |
| `model` | 400 KiB | **去掉** | S31 上不用 |
| `ota_0`、`ota_1` | 各 5 MiB | 各 4.5 MiB | 以 `idf.py size` 为准，镜像 ≤ 4 MiB 还能各减 0.5 MiB |
| `resources` | 400 KiB | 不变 | |
| `font_data` | 5 MiB | 不变 | |
| `user`（新） | 无 | 约 1.4 MiB，LittleFS | 模拟器现在就是 1408 KiB |
| `coredump` | 64 KiB | 不变 | |

改分区表要写设备：现在只写进设计，等上机时由用户同意（§14）。系统不认识分区表细节，只经 HAL 看“有一个可写的另一槽”和 `/user`。

### 2.3 `/user` 的目录规则

LittleFS 每个目录至少占两个擦除块（8 KiB），`/user` 只有 1.4 MiB，所以**目录要少，靠文件名前缀分**（实测见 §15）：

```text
/user/sys/settings.json            设置
/user/sys/<集合>.snap              设备数据区的集合（快照）
/user/sys/<集合>.<代>.log          及其追加日志
/user/m/<成员>/<集合>.snap         成员数据区的集合
/user/m/<成员>/<集合>.<代>.log
/user/m/<成员>/<应用>.<名字>       应用的小文件（状态、阅读位置）
```

集合名和文件名都以主人开头：系统服务用自己的域（`notify.inbox`），应用用应用编号（`reader.words`）。所有路径只由 `ink/sys/layout.h` 里的函数拼，别处不写字符串；成员编号只允许 `[a-z0-9_-]`，1～15 字节。

原来的 `/user/settings.json`、`/user/apps/reader-<成员>-<书>.pos` 迁到新位置。设备上还没有用户数据，所以不做迁移代码。

### 2.4 隔离到什么程度

单片机只有一个地址空间，内置应用是同一个镜像里的 C 代码，**它们要越界，技术上挡不住**。这里的隔离是接口约束：

- 应用经 `ink_app_db_open`、`/app/...` 只拿得到自己的；成员隔离在服务里判断，不靠应用自觉。
- 以后的脚本应用（D-08 第三阶段）跑在沙箱里，只能经同样的接口，那时这才是真隔离。
- 跨应用一律走能力。`sdk-boundary` 测试检查应用不碰 HAL 和 OSAL。

### 2.5 格式和版本

- 所有磁盘格式带版本号（底座设计 §10.3）。旧固件遇到更新的格式：拒绝写，读不了就报错，不覆盖。
- 格式迁移只在新固件通过自检、确认有效之后做（§12）。

## 3. 存储服务的扩展和结构化数据存储

### 3.1 存储服务：多几种作业

存储服务仍然是唯一碰 `/user` 的任务（内部 RAM 栈，刷新时不碰 Flash）。作业从“原子替换”扩成几种，按提交次序执行，失败告诉提交者：

```c
typedef void (*ink_storage_done_fn)(void *ctx, int result); /* 存储任务里运行，要短 */
int ink_storage_write_atomic2(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
int ink_storage_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
int ink_storage_remove(const char *path, ink_storage_done_fn done, void *ctx);
int ink_storage_load(const char *path, void **out, size_t *len, const char *tag); /* 整个文件读进 PSRAM，分块，块之间让出 Flash 守卫 */
int ink_storage_list(const char *dir, ink_storage_dirent_fn fn, void *ctx);
int ink_storage_volume(hal_fs_volume_info_t *out);                                 /* /user 的总量和剩余，遍历文件系统，慢 */
int ink_storage_run(void (*fn)(void *ctx), void *ctx);                             /* 在存储任务里、Flash 守卫内运行（升级写槽用） */
```

- 目录不在就建（只建缺的一层；`/user/m`、`/user/m/<成员>`、`/user/sys`）。
- 同一路径上的次序不乱：写作业只合并进“这个路径最后一个作业”，追加和删除不合并。
- 读看得到排队的替换和删除；看不到排队的追加（数据存储自己有内存里的副本，不读自己的日志）。

### 3.2 数据存储：模型

一个**集合**是一组记录，每条记录：

| 字段 | 说明 |
| --- | --- |
| `id` | 32 位，集合内唯一，不给就自动分配 |
| `sort` | 64 位排序键（通常是时间），分页按它 |
| `ver` | 最后一次改动的序号（集合内递增） |
| `updated_s` | 最后改动的 UTC 秒（时钟未知时为 0） |
| `src` | 谁写的（人、AI、系统、远端） |
| `schema` | 这条记录内容的版本 |
| 内容 | 字节串，上限 8 KiB；声明为 JSON 的集合才能经 `data.*` 给 AI |

**打开的集合整份放在 PSRAM**（D-66）：`/user` 总共 1.4 MiB，全部放下也不多，而分页不碰 Flash，界面翻列表不用等刷新；没人打开、已经落盘的集合在内存紧张时让出来（登记为可回收缓存）。

```c
typedef struct { const char *name, *title; ink_db_scope_t scope; uint8_t schema; bool json, ai_read, ai_write;
                 uint32_t max_records; size_t max_bytes; bool drop_oldest; } ink_db_def_t;
ink_db_t *ink_db_open(const ink_db_def_t *def, const char *member); /* member NULL = 现在的成员；设备级集合忽略 */
void ink_db_close(ink_db_t *db);
int  ink_db_put(ink_db_t *db, const ink_db_write_t *w, uint32_t *out_id);
int  ink_db_del(ink_db_t *db, uint32_t id, ink_source_t src);
int  ink_db_get(ink_db_t *db, uint32_t id, ink_rec_t *out, void *buf, size_t cap);
int  ink_db_next(ink_db_t *db, ink_db_cursor_t *c, ink_rec_t *out, void *buf, size_t cap); /* 分页：游标是键，插入删除都不乱 */
int  ink_db_count(ink_db_t *db);
int  ink_db_changes(ink_db_t *db, uint32_t since_ver, ink_db_cursor_t *c, ink_rec_t *out, void *buf, size_t cap); /* 给同步用，含墓碑 */
void ink_db_ack(ink_db_t *db, uint32_t ver);                 /* 中枢确认到这里：更早的墓碑可以扔 */
uint32_t ink_db_last_ver(ink_db_t *db);
uint32_t ink_db_durable_ver(ink_db_t *db);                   /* 已经落盘到哪 */
int  ink_db_flush(ink_db_t *db);                             /* 现在就交给存储服务 */
int  ink_db_wait(ink_db_t *db, uint32_t ver, int64_t timeout_us); /* 等落盘（不在界面任务里用） */
int  ink_db_error(ink_db_t *db);                             /* 最近一次写失败的原因，0 = 没有 */
void ink_db_flush_all(void);                                 /* 锁屏、关机、低电量 */
```

任务归属：`put`、`del`、读都在调用者的任务里，只碰内存（一把锁，不等 Flash）；`open` 第一次要读文件，会等（可能等一次刷新结束），所以界面应用尽量在 `create` 之外打开，或者接受这一次等待。落盘在存储任务；压缩在后台执行队列。

### 3.3 磁盘格式

```text
<集合>.snap     快照：一次写好（临时文件 + 改名）
  头 32 字节：magic "IDB1"、format(1)、schema、记录数、last_ver、next_id、log_gen、CRC
  记录帧 × n（含墓碑）
<集合>.<代>.log 追加日志：记录帧 × n

记录帧（小端）：
  u16 sync(0xA55A) u8 op(1 写, 2 删) u8 src u32 ver u32 id i64 sort i64 updated_s u16 len u8 schema u8 rsv
  内容[len]  u32 crc32（前面全部）
```

### 3.4 写入、压缩和断电

- **写**：改内存，帧追加到这个集合的待写缓冲。`INK_DB_NOW`（默认，备忘、提醒）马上交给存储服务；`INK_DB_LAZY`（阅读、播放位置）最多攒 60 s，锁屏、切换内容、低电量时立刻写。同一时刻每个集合只有一个追加作业在途，攒下的下一批一起写（LittleFS 一次追加至少重写尾块，批量省擦除）。
- **落盘回报**：追加成功后 `durable_ver` 前进。失败（比如 `/user` 满了）：这批留着，5 s 起翻倍最多 5 分钟重试；`ink_db_error` 报原因，发一次 `data.write_failed`，通知服务据此提醒。内存里的改动不丢，但重启就没了，这一点如实告诉调用者：`put` 成功只表示“已接受”。
- **待写缓冲满**（1 MiB 总额，或这个集合的 `max_bytes`）：`put` 直接失败，不悄悄丢。
- **压缩**：日志超过 `max(8 KiB, 快照的一半)` 时排一个后台作业：在锁内把全部记录序列化到 PSRAM，`write_atomic` 写快照。**代**的规则保证任何时刻断电都不丢（D-66）：
  - 快照记着 `log_gen = G`。加载：读快照，按序回放 `G` 代和 `G+1` 代日志，序号 ≤ `last_ver` 的跳过，再顺手删 `G-1` 代。
  - 压缩时如果在用的代就是 `G`，先换到 `G+1`（之后的追加写新文件），新快照写 `log_gen = 当前代`；快照写成功之后才删旧日志；失败什么都不删。所以最多只有两代。
- **坏帧**：LittleFS 的同步是原子的，正常不会有半帧。真遇到（CRC 不对、sync 不对、长度越界）：停在这里，已读到的留下，计数 `data.log_bad`，马上压缩一次把坏尾巴丢掉，之后的追加不会接在坏帧后面。快照坏了：计数 `data.snapshot_bad`，只从日志恢复。
- **墓碑**：删除留一条只有头的记录，给同步用；每个集合最多留 256 条，最老的先扔，`ink_db_ack` 之后的可以提前扔。

### 3.5 内存和预算

每条记录在 PSRAM 占 48 字节头 + 内容 + 分配器开销，排序表每条 4 字节。整个数据存储配额 2 MiB（`ink_mem_budget("data", PSRAM, …)`）。内部 RAM：一把锁 96 字节；集合表放 PSRAM。

### 3.6 给 AI 的能力

`data.collections`（当前成员能看的集合：名字、标题、条数）、`data.list {collection, limit, cursor}`、`data.get {collection, id}`、`data.put {collection, id?, sort?, value}`（`value` 是 JSON 文本，可撤销）、`data.delete {collection, id}`（不可逆，走确认）。只对声明了 `ai_read`、`ai_write` 且内容是 JSON 的集合；成员永远是当前成员，没有办法指定别人。应用和服务也用这组能力跨应用读数据。

## 4. 文件服务

### 4.1 统一路径

应用和 AI 只看到虚拟路径，物理路径只在 `file_service.c` 和 `layout.h` 里：

| 虚拟路径 | 物理路径 | 谁能读 | 谁能写 |
| --- | --- | --- | --- |
| `/me/...` | `/content/.m/<当前成员>/...` | 自己 | 自己 |
| `/family/...` | `/content/...`（不含点开头的） | 所有人 | 成人 |
| `/members/<成员>/...` | `/content/.m/<成员>/...` | 成人（家长可查看，底座设计 §8） | 成人 |
| `/app/...` | `/content/.m/<当前成员>/apps/<调用的应用>/...` | 这个应用 | 这个应用 |
| `/` | 虚拟目录：`me`、`family`，成人多一个 `members` | | |

路径校验（在任何文件操作之前）：UTF-8 合法、不超过 128 字节、不含 `..`、`.`、空段、反斜杠、控制字符和 FAT 不允许的字符、段不以点开头；`/user` 这种物理路径一律拒绝。

### 4.2 接口

```c
int ink_fs_open(const char *path, int mode, ink_fs_file_t **out);  /* INK_FS_READ / WRITE / APPEND / CREATE / EXCL */
int64_t ink_fs_read(ink_fs_file_t *f, void *buf, size_t n);
int64_t ink_fs_write(ink_fs_file_t *f, const void *buf, size_t n);
int ink_fs_seek(ink_fs_file_t *f, int64_t off, int whence);
int64_t ink_fs_tell(ink_fs_file_t *f);
int ink_fs_sync(ink_fs_file_t *f);
int ink_fs_close(ink_fs_file_t *f);
int ink_fs_stat(const char *path, ink_fs_stat_t *out);
int ink_fs_dir_open(const char *path, ink_fs_dir_t **out);  /* 拉取式，隐藏文件不出现 */
int ink_fs_dir_next(ink_fs_dir_t *d, ink_fs_stat_t *out);
void ink_fs_dir_close(ink_fs_dir_t *d);
int ink_fs_mkdir(const char *path);                         /* 缺的上级一起建 */
int ink_fs_remove(const char *path);
int ink_fs_move(const char *from, const char *to);
int ink_fs_write_atomic(const char *path, const void *data, size_t n); /* 临时文件 + 改名 */
int ink_fs_free(uint64_t *free_bytes, uint64_t *total_bytes);
int ink_fs_exclusive(bool on);                              /* U 盘模式：有文件开着就失败；进去之后所有操作返回“忙” */
int ink_fs_copy_start(const char *from, const char *to);     /* 后台作业，§4.3 */
int ink_fs_delete_start(const char *path);
int ink_fs_job_status(int id, ink_fs_job_status_t *out);
void ink_fs_job_cancel(int id);
```

- 任务：TF 卡写不关 CPU 缓存，可以在调用者的任务里做（阻塞几毫秒到几百毫秒，所以大块读写放后台，不放界面任务）。现有的 `ink_file_*` 只读接口保留给阅读器。
- 错误码：路径不对、没权限、不存在、已存在、没插卡、U 盘模式中、空间满、读写错。没插卡时 `storage.card` 变假，界面据此提示。
- 变化通知：写、建、删、移都发事件 `file.changed {path, kind}`，走软总线，应用和 AI 用同一个订阅；退出 U 盘模式发一次 `kind: rescan`。
- 同时最多开 8 个文件（表在 PSRAM）；进 U 盘模式前要全关（媒体播放、录音会被通知先停）。
- `storage.free` 等状态：开机、卡插拔、写操作之后（最多一分钟一次）在后台刷新，`f_getfree` 在大卡上慢，不放界面任务。

模拟器：加 TF 卡的物理动作（脚本 `sd remove|insert|full`），HAL 没卡时返回失败。真机的卡检测有没有引脚**不知道**（§14），所以系统不依赖插拔中断：操作失败时和解锁时查一次。

### 4.3 拷贝、移动、删除

- `ink_fs_move`：同一张卡上就是改名，马上完成，可撤销。目标已存在就失败，不覆盖。
- `ink_fs_copy_start(from, to)`、`ink_fs_delete_start(path)`：目录连里面的内容一起，单个大文件也可能要几分钟（卡的写速度约 1 MB/s），所以是**后台作业**：每步 64 KiB 或一个目录项，能取消（`ink_fs_job_cancel`），能查进度（`ink_fs_job_status`：已处理的字节和文件数、总量、状态），结束发事件 `file.job_done {id, ok, why}`。同一时刻一个，别的回“忙”。
- 拷贝先写 `<目标>.part`，写完才改名，断电留下的 `.part` 不算文件（列表不显示；开机不去翻卡，下次拷到同一个目标时覆盖，整个目录删除时一起删；U 盘模式下电脑能看到）；空间不够提前拒绝（先算总量）；中途失败或取消，已经完整拷完的留着，正在拷的删掉，并报告。
- 删除目录：先删里面的，再删目录；中途失败报告还剩什么。删除不可撤销（这一轮没有回收站），AI 发起要确认卡；人在设备上按的，确认由界面做。

### 4.4 U 盘模式（R-058，D-72）

```mermaid
stateDiagram-v2
  [*] --> 普通: 开机
  普通 --> 连着电脑: 插上数据线（usb.changed，发一条通知）
  连着电脑 --> 普通: 拔掉
  连着电脑 --> 卡给电脑: usb.mount（人按，或 AI 发起经确认卡；成人）
  卡给电脑 --> 普通: usb.unmount，或电脑弹出，或拔线
```

- **开放什么**：只有 TF 卡。U 盘是块设备，电脑看到整个卡的 FAT，单个目录挡不住，所以卡上的 `.m/`（各成员的私人文件）也看得到。系统区、`/user`（设置、PIN 校验值、Wi-Fi 密码、个人数据）在内部 Flash，**永远不开放**。卡上不放凭据（底座设计 §8.1）。
- **谁能开**：系统级能力：成人才行，孩子被拒绝；成人解过 PIN；AI 发起走确认卡。
- **进入**：先发事件让持有文件的停下（媒体暂停并存位置，录音收尾），`ink_fs_exclusive(true)` 要求没有打开的文件和后台作业，否则回“忙”并说明谁占着；然后 `hal_usb_msc_start()`。这期间文件服务所有操作回“U 盘模式中”，`storage.mode` = `usb`。持有“别自动锁屏、别睡”。
- **退出**：`hal_usb_msc_stop()`，重新挂载，查卡是否还好；`storage.mode` = `normal`，发 `file.changed {kind: rescan}`，刷新 `storage.free`，查一下有没有新的升级包（§12.4）。电脑弹出和拔线一样处理；拔线没弹出可能留下脏的 FAT：挂载失败就 `storage.card` 变假并发通知，不自动格式化。
- **HAL**：`hal_usb.h`：`hal_usb_host_connected()`、回调、`hal_usb_msc_start/stop`。真机是 TinyUSB MSC（上游配置启用，**本项目没有实测**）。S31 的 USB 口平时是串口/JTAG，切 TinyUSB 对下载和串口日志的影响待测（§14）。
- **模拟器**：脚本 `usb plug|unplug|eject|copy-in <host 文件> <卡上路径>`：模拟“电脑”往卡上拷文件；U 盘模式中系统访问卡的 HAL 调用全部失败，能查出没停干净的地方。

### 4.5 给 AI 的能力

`file.list {path, cursor, limit}`、`file.stat`、`file.read {path, offset, max_bytes}`（只读文本，最多 2 KiB，不是合法 UTF-8 就说明是二进制）、`file.write {path, text, append}`（已有文件不覆盖，除非 `append`）、`file.mkdir`、`file.copy`、`file.move`（可撤销）、`file.delete`（不可逆，走确认）、`file.free`、`usb.status`、`usb.mount`、`usb.unmount`。跑在后台执行队列（卡慢，不占界面）。大文件的拷贝回执“已排队”，对象是作业，结束有事件。

## 5. 时间服务

### 5.1 时钟

- 开机读 RTC。有效：墙钟 = RTC，`time.synced` 假（还没和中枢对过）。无效（电池没了）：墙钟未知，`ink_time_valid()` 假，依赖时钟的闹钟等到知道时间。
- 校时：中枢在 `hello` 的回答里带 `now_ms`（不多一个来回），连接建立时取一次，用往返时间的一半补偿。差超过 2 s 才跳，写回 RTC。真机还可以加 SNTP，同一个入口。
- 手动设时间：能力 `time.set`，系统级；孩子不能改（底座设计 §7.1）。设置 `time.auto` 关掉就不听中枢的。
- 时长、超时一律用单调时钟，不受改时间影响。墙钟跳了：闹钟重新排。

### 5.2 时区

设置 `time.zone`：一张小表（`Asia/Shanghai` 默认、`UTC`、`Asia/Tokyo`、`Asia/Hong_Kong`、`Asia/Singapore`、`Europe/London`、`Europe/Paris`、`America/New_York`、`America/Los_Angeles`），带标准偏移和夏令时规则（美国、欧盟两种）。记录存 UTC，显示和闹钟按本地。夏令时开始那一小时不存在的本地时间：往后推到存在的时刻；结束时重复的一小时：取第一次，不响两次。

```c
int64_t ink_time_now_s(void); bool ink_time_valid(void);
void ink_time_to_local(int64_t utc_s, ink_tm_t *out);
int64_t ink_time_from_local(const ink_tm_t *tm);
```

### 5.3 农历

公历和农历互转，1900～2100，一张紧凑的年表（每年一个数，放 Flash）。接口：`ink_lunar_from_solar`、`ink_lunar_to_solar`、月名、日名（正月、初一、闰四月）、生肖、干支。年表和苹果系统的中国历法逐日比对过（方法和结果见 §15）。节气先不做（要天文计算或另一张表，§13）。

### 5.4 闹钟和提醒

存在设备级集合 `time.alarms`，每条带主人（成员）：闹钟不分谁在用都要响。

| 类型 | 时间怎么定 | 重复 |
| --- | --- | --- |
| 闹钟 | 本地时分（换时区仍是这个时分） | 只响一次，或按星期几 |
| 提醒 | UTC 时刻（另存当时的时区）或本地日期时分 | 不重复、每天、每周、每月、每年、**农历每年**（生日） |

排程：一张内存表，算出每项下次响的时刻，取最早的，设：

1. 一个单调时钟的定时（共用服务定时器，§6.5），
2. **电源服务的唤醒时刻** `ink_power_wake_at()`：锁屏睡眠的每一片不超过它，到点没处理完不再睡（最多等 5 s），
3. **RTC 闹钟**（分钟精度，向下取整）：RTC 中断能唤醒就早一点醒，然后再睡到点；中断线不可用（出厂日志里 P16 一直低，`power.rtc_wake_unavailable`）时靠第 2 条，一样准时。

到点：闹钟经打断通道（§6.3）响铃，`alarm.ringing`，等 `alarm.dismiss` 或 `alarm.snooze`（默认 9 分钟），响 2 分钟没人管就自己停、记一条“错过”通知；提醒发高优先级通知。迟到的处理：晚 60 s 内照常响；更晚的（关机了、时钟跳了）不补响，发“错过”通知（底座设计 §5.7“不补跑”）。重复的闹钟算下一次。

低电量也要响。关机后能不能靠 RTC 开机**不知道**（§14）。

## 6. 通知和系统打断通道

### 6.1 通知

```c
typedef enum { INK_NOTIFY_LOW = 0, INK_NOTIFY_NORMAL, INK_NOTIFY_HIGH } ink_notify_prio_t;
typedef struct { const char *channel, *title, *body, *group, *tag, *action, *action_args; ink_notify_prio_t prio;
                 const char *member; ink_source_t src; } ink_notify_t;
int  ink_notify_post(const ink_notify_t *n, uint32_t *out_id); /* 任何任务，不等 Flash；同 tag 替换旧的 */
int  ink_notify_cancel(uint32_t id);
int  ink_notify_cancel_tag(const char *tag);
int  ink_notify_mark_read(uint32_t id);
int  ink_notify_next(ink_db_cursor_t *c, ink_notify_view_t *out);
int  ink_notify_channel_define(const ink_notify_channel_t *ch);
```

- 存在成员集合 `notify.inbox`（JSON，最多 200 条，满了扔最老的），按成员隔离；给不在用的成员的通知进他们自己的收件箱，不打扰现在的人。
- 渠道：`system`、`message`、`call`、`alarm`、`reminder`、`download`、`ai`，每个有默认优先级；用户可以按渠道关（`notify.set_channel`，存在成员集合）。
- 分组：同 `group` 的归成一组，列表能折叠，计数按组。
- 勿扰 `notify.dnd`：关、开。开着时普通和高优先级都只进收件箱，没声音没振动。
- 墨水屏规则：通知不弹横幅。`notify.unread` 变了只改状态栏小图标（界面框架做）；高优先级且不在勿扰时加一声提示音和一次振动。

### 6.2 打断通道：不经过前台应用

来电、闹钟、低电量这类事不能等前台应用，也不能被一次慢刷新或网络请求挡住（底座设计 §7）。所以：

```c
/* ink/sys/interrupt.h：只给系统用，应用只有通知（sdk-boundary 测试拦着） */
typedef struct { const char *label, *cap; char args[64]; } ink_int_action_t;
typedef struct { int id; ink_int_kind_t kind; uint32_t ref; bool ringing; char title[48], body[96];
                 ink_int_action_t actions[3]; int nactions; uint64_t raised_us; } ink_interrupt_t;
int  ink_interrupt_raise(const ink_interrupt_t *it);              /* 任何任务（包括定时器任务），不阻塞；返回编号，满了（4 个）返回 -1 */
void ink_interrupt_end(int id);
void ink_interrupt_update(int id, const char *body, const ink_int_action_t *a, int n, bool ringing); /* 响铃 -> 通话中 */
int  ink_interrupt_action(int id, int index, uint8_t src);        /* 执行按钮对应的能力 */
void ink_interrupt_set_presenter(const ink_int_presenter_t *p);   /* 系统壳登记 */
```

`raise` 在调用者的任务里直接做三件事，不排队等界面任务：

1. **电源**：亮屏（锁屏时临时解开，页面被全屏浮层盖住），持有“别睡”；结束后恢复锁屏。
2. **声音**：按类型拿音频焦点响铃（§10.1），在音频任务里，界面卡住也响。
3. **界面**：把“显示”投递到界面任务（节点是打断表自己的，不会丢）。这时前台应用的 `on_key`、`on_swipe` 不再收到输入：系统浮层（`ink_ui_overlay_add`，画在 `lv_layer_sys`）先收，全屏控件挡住触摸。浮层出现或消失时，手指已经按着的那次触摸整个作废，否则它接下来一动就按到新出现的按钮，抬手就成了一次点击（比如正好落在“挂断”上）。AI 看到的页面内容变成浮层上的字，只在原页面有效的能力这时不可用。

电源键（`ink_input_set_power_key_hook`）：来电时静音，只静音不挂断；闹钟时稍后再响；低电量时关掉；没有打断时照常锁屏。

锁着时被打断：屏亮，设备“底下还锁着”，最后一个打断结束后重新锁上。打断期间有人（能力、竞争）锁屏：电源服务的订阅里立刻解开，并记下结束后要锁（`interrupt.lock_undone`）。

事件在哪个任务里处理：来电和闹钟在发出事件的任务里直接处理（通信任务、定时器任务），前面不排队；定时器任务栈小，所以闹钟这条路径不发通知、不占大栈。其余（短信、错过、提醒、低电量、写入失败、USB）走后台执行器：电源任务和存储任务的栈是内部 RAM，很小，不能在上面跑这些。

优先级：来电 > 闹钟 > 低电量。高的来了，低的留在表里，等高的结束再显示。AI 面板、确认卡都让路。

### 6.3 三种打断的来源

| 类型 | 谁拉起 | 按钮 | 结束 |
| --- | --- | --- | --- |
| 来电 | 通知服务订阅 `call.incoming` | 接听（`call.answer`）、挂断（`call.hangup`） | `call.ended`；没接就发“未接来电”通知。接听后切成“通话中”浮层 |
| 闹钟 | 时间服务 | 关闭（`alarm.dismiss`）、稍后（`alarm.snooze`） | 按钮，或响够 2 分钟 |
| 低电量 | 电源服务 | 知道了（`interrupt.ack`） | 按钮，或开始充电 |

勿扰（D-68）：闹钟照响；来电不响铃，但全屏显示；低电量照常。

短信：通知服务订阅 `message.received`，发普通通知（渠道 `message`）。

### 6.4 低电量

电源服务在状态变化和锁屏每一片醒来时读电量（不另开定时器，不打断睡眠），亮屏时每分钟读一次。事件 `battery.low`（参数 `level`：1 低，2 很低，3 马上关机，0 充上电了）：≤ 15% 发通知；≤ 5% 打断；≤ 2% 且没在充电：`ink_db_flush_all`、等写完（最多 5 秒）、关机（底座设计 §7.4“先保存状态再关机”）。每一档只说一次，开始充电复位；启动完成后才开始判断（`ink_power_battery_policy_start`），这样开机时已经低电的情况也有人听。低电量时不压缩、不下载升级。

### 6.5 共用的锁和定时器

内部 RAM 很紧（最紧的场景只比 32 KiB 底线多 736 字节，§15），每把锁、每个定时器都占 96 字节。所以小服务共用：

- `ink_svc_lock()`：文件、时间、通知、确认卡、应用、剪贴板、Wi-Fi、成员的表共用一把叶子锁：持锁时只改自己的表，不调别的服务，不等任何东西。
- `ink_svc_at(node, t_us, fn, ctx)`：共用一个 `osal_timer`，节点由调用者持有（和执行队列一样，不会满）；回调在定时器任务里，不能阻塞。

## 7. 系统确认卡

策略关口要确认时，不再只回“待确认”：

```c
typedef struct { uint32_t token; char cap[48], title[48], detail[160]; uint8_t src; uint64_t expires_us; bool parent; } ink_confirm_view_t;
ink_rc_t ink_confirm_hold(const ink_cap_t *c, const ink_args_t *args, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc); /* 关口，或自己才知道要确认的处理函数 */
int  ink_confirm_list(ink_confirm_view_t *out, int max);
int  ink_confirm_decide(uint32_t token, bool accept, ink_source_t src); /* 只收触摸和按键 */
int  ink_confirm_withdraw(uint32_t token, ink_source_t src);
void ink_confirm_set_presenter(const ink_confirm_presenter_t *p);
```

1. 关口发现要确认：先查参数（不合法的当场回“无效”，不弹卡），再把调用（能力、参数、来源、内容版本、成员）存进待确认表（最多 4 条，放 PSRAM），回执“待确认”，`object` 是 `confirm:<token>`，`detail` 说明在等什么。满了回“被拒绝：确认队列满”。参数里带调用者缓冲区的（`BLOB`）不能等，直接拒绝。重发同一个请求号拿到同一个回执（现有的去重）。
2. 呈现者显示卡片（系统占位）：能力标题、一句话（能力的 `.ask`，比如 `"发短信给 {to}：{text}"`；没写就是参数列表；密码类参数显示 `***`，给 AI 看的列表也一样）、确认、取消。外框由系统画，页面无法模仿（底座设计 §6.5）。设备锁着时等解锁再显示；有打断时让路，打断结束回来。
3. 用户确认：用来源 `INK_SRC_CONFIRMED` 重新走一遍调用：关口再查一次（成员还是不是那个人、儿童限制、页面版本、页面还在不在，看的是卡片底下的页面），只是不再要确认。结果发事件 `confirm.result {token, cap, decision, status, detail, say, object}`，事件经 AI 代理转给中枢（§1.2 第 3 条）。
4. 取消、过期（60 s）、换成员、AI 撤回（`confirm.withdraw`）：调用作废，同样发 `confirm.result`。主页键、返回键等于取消。
5. 只有 `INK_SRC_TOUCH`、`INK_SRC_KEY` 能确认。`confirm.accept`、`confirm.reject` 标 `INK_EXPOSE_OWNER`：不开放给 AI、远端和应用，也不在工具清单里；C 接口也查来源。同一个 token 只能决定一次。撤回只有发起的来源或人能做。
6. 要家长确认的（`INK_CONFIRM_PARENT`，比如恢复出厂）：卡片上要输成人 PIN（§11.3）。

系统级设置（`settings.set`）走同样的路：设置表回“待确认”时，处理函数调用 `ink_confirm_hold`，回执带 token。应用改系统级设置也要确认。

## 8. 应用注册和生命周期

### 8.1 应用目录

```c
typedef struct {
    const char *id, *title, *icon;          /* "reader"，"阅读"，图标编号（视觉另定） */
    const struct ink_activity *entry;       /* 入口页面 */
    bool system_app, child_ok;
    bool keep;                              /* 内存紧张也不停（主页） */
    uint32_t background;                    /* 声明的后台类型：INK_BG_PLAYBACK / DOWNLOAD / SYNC */
    const char *const *needs;               /* 必需的能力（NULL 结尾），缺了不显示 */
    const char *const *permissions;         /* 这个应用可以用的权限 */
    const ink_app_ops_t *ops;               /* 生命周期回调，都在界面任务 */
    int (*card)(int index, ink_card_t *out);/* 服务卡片 */
} ink_app_def_t;
int  ink_app_register(const ink_app_def_t *def);                     /* 启动时，界面开始之前 */
int  ink_app_count(void); const ink_app_def_t *ink_app_at(int i);
bool ink_app_visible(const ink_app_def_t *def);                      /* 这位成员能不能看到，这台设备有没有它要的能力 */
ink_app_state_t ink_app_state_of(const char *id);
int  ink_app_open(const char *id, void *from, void *arg);            /* from：被点的控件，先给反馈；已经开着就把它的页面提上来 */
int  ink_app_close(const char *id);
int  ink_app_background(const char *id, uint32_t kind, bool on);     /* 只收声明过的类型 */
ink_db_t *ink_app_db_open(const char *app, const ink_db_def_t *def); /* 自己的集合：名字要以“应用编号.”开头 */
```

前台跟着界面的页面栈走：不管是按钮、AI 还是按键把页面放到最上面，都是同一套转换，在页面建出来之前做完（`ink_ui_set_top_hook`）。一个应用翻到自己的下一页不算变化；上面是不属于任何应用的系统页面时，没有前台应用。占位的主页按目录列按钮，配额（`psram_quota`）留到有第三方应用时再做。

`ink_activity_t` 多一个 `app` 字段（属于哪个应用）。权限检查（详细设计 §2.4 第 4 条）真正生效：来源是应用时，能力要求的权限必须在当前前台应用的清单里。

### 8.2 生命周期

```mermaid
stateDiagram-v2
  [*] --> 已停止
  已停止 --> 前台: 打开（on_start、restore_state、on_foreground）
  前台 --> 后台: 别的应用盖上来或锁屏（on_background）
  后台 --> 前台: 回来（on_foreground）
  后台 --> 冻结: 没有后台任务，10 s 后；或内存紧张（save_state、on_freeze）
  冻结 --> 前台: 回来（on_resume、on_foreground）
  后台 --> 已停止: 内存危急（save_state、on_stop）
  冻结 --> 已停止: 内存危急，或换成员（on_stop）
```

- 应用没有线程，“冻结”的含义是：它答应不再做事（停定时器、放掉缓存）；执行队列的工作由它自己收住。
- 状态保存：`save_state(buf, cap)` 最多 1 KiB，写到成员数据区 `/user/m/<成员>/<应用>.state`；停止后再打开时 `restore_state`，用户感觉不到。冻结时已经存过，从冻结到停止不再存。
- 内存压力接 `ink_mem_on_pressure`：紧张（LOW）冻结所有后台的（播放除外）；危急（CRITICAL）从最久没用的开始停，播放的最后停，主页和前台不停，停到不再危急为止。
- 换成员：全部保存、停止，回主页，文件和数据都换成新成员的。

### 8.3 服务卡片

```c
typedef struct { char id[16], title[32], text[96], action[40]; int64_t value; bool has_value; uint32_t rev; } ink_card_t;
```

应用提供 `card(index, out)`：便宜、不依赖应用是否开着。`ink_app_card_changed(id)` 让状态 `app.cards_rev` 加一，主页据此重读。**主页和 AI 读同一份**：能力 `app.cards`。

## 9. 文本输入和剪贴板

### 9.1 输入会话

```c
typedef struct { ink_text_type_t type; ink_text_enter_t enter; const char *initial, *hint; size_t max; bool ai_visible; } ink_text_cfg_t;
int  ink_text_focus(const ink_text_cfg_t *cfg, const ink_text_client_t *client, void *ctx); /* 界面任务 */
void ink_text_blur(int session);
/* 键盘（或测试）喂进来 */
void ink_text_key(char c); void ink_text_backspace(void); void ink_text_pick(int index); void ink_text_enter(void); void ink_text_commit_utf8(const char *s);
/* 读 */
const char *ink_text_value(void), *ink_text_composing(void); int ink_text_candidates(int page, ink_text_cand_t *out, int max);
```

- 一个时刻一个会话。类型：普通、数字、电话、密码（不进候选、不学词、对 AI 掩码、不能复制）。
- 流程：字母进组合串 → 查候选 → 选中提交一部分，剩下的继续组合。回车键的动作由类型决定（完成、发送、搜索、下一个）。
- 文本框控件 `ink_textfield_create()`（`system/ui/textfield.c`，LVGL 占位）：焦点、组合串、光标、长按菜单（复制、粘贴、问 AI）；键盘面板 `system/ui/shell/keyboard.c` 是系统浮层，候选条用快刷（底座设计 §4.8）。
- 按键完整排队，不因为刷新忙丢键。

### 9.2 拼音引擎

- 全拼（首版）。词库是一个文件 `/content/.system/ime/base.ikd`：头 + 按拼音排序的键表 + 词表（词、频率），第一次开键盘时读进 PSRAM（目标 ≤ 1 MiB），登记为可回收；没卡时退到内置的很小的单字表。
- 查询：取组合串，从最长的前缀往短找完全匹配的键，同长度按“用户频率 + 词库频率”排。选中一个候选消耗对应的字母。不做拼音切分：数据本身决定（`xian` 既是“先”也是“西安”）。
- 用户词频：成员集合 `text.user_freq`，选中时 `INK_DB_LAZY` 更新；密码框不学。
- 词库来源（D-70）：工具 `tools/imedict` 从制表文本生成；仓库里放一份测试和模拟器够用的小词表，正式词库的来源和许可等决定。
- AI 联想（R-027）：默认关，接口先留（`ink_text_suggest_hook`），走软总线，不在这一轮做。

### 9.3 剪贴板

```c
int ink_clip_set(const char *text, bool sensitive, ink_source_t src);
int ink_clip_get(char *out, size_t cap, ink_source_t src, bool *sensitive);
void ink_clip_clear(void);
```

一条文本（最多 4 KiB，PSRAM），不落盘；换成员清空；敏感内容 60 s 后自动清。AI 读要过策略：设置 `ai.read_clipboard` 默认关，关着就“被拒绝”；敏感内容永远不给 AI。

### 9.4 给 AI 的能力

`text.state`（有没有焦点、类型、内容；密码掩码）、`text.insert {text}`（往焦点框里写，等于替用户打字）、`text.submit`（等于按回车）；`clipboard.read`、`clipboard.write`。这样“帮我把这段填到搜索框”不用模拟按键。

## 10. 音频

### 10.1 五档焦点和续播（底座设计 §7）

| 档 | 谁 | 被更高的打断时 | 高的结束后 |
| --- | --- | --- | --- |
| 1 通话 | 来电、去电 | — | — |
| 2 闹钟、提醒 | 响铃 | 不被打断（通话除外：先静音，通话结束后如果还在响的时间内继续） | 用户处理后结束 |
| 3 AI 回答、录音 | AI 朗读、`audio.record` | 朗读：停，不续（文字还在）；录音：停并存好，`audio.record_done` 带原因 | 不自动恢复 |
| 4 媒体 | 听书、播放 | 暂停，记位置、记原因 | 用户没手动暂停、中断不超过 30 分钟：自动续播，回退 3 s |
| 5 提示音 | 按键音、通知音 | 通话、闹钟时丢弃；其他时候插在两帧之间 | — |

```c
typedef void (*ink_focus_fn)(void *ctx, ink_focus_event_t ev, ink_audio_prio_t by); /* 丢了、暂时丢了、又拿到 */
int  ink_audio_focus_acquire(ink_audio_prio_t prio, ink_focus_fn fn, void *ctx, ink_exec_t exec);
void ink_audio_focus_release(int token);
```

焦点是内部机制，不是能力。现有的 `ink_audio_stream_open` 不变，内部多一条：同档替换，高的盖低的；被盖的持有者收到回调。响铃：`ink_audio_ring(on)`，音频任务里循环生成，不依赖文件。

### 10.2 媒体播放（`ink/sys/media.h`）

格式（D-67）：**Ogg Opus（RFC 7845），16 kHz 单声道，60 ms 一帧**。标准格式，用户拷到电脑上能播，中枢也容易转；设备上没有 MP3 解码，别的格式由中枢转换。容器解析和写入都自己写（很小）；模拟器里包的是替身 Opus 帧。

```c
int  ink_media_play(const char *path, uint32_t start_ms, const ink_media_opts_t *o);
void ink_media_pause(void); void ink_media_resume(void); int ink_media_seek(uint32_t ms); void ink_media_stop(void);
int  ink_media_status(ink_media_status_t *out); /* 路径、位置、总长、状态、暂停原因 */
```

- 一个播放会话，属于“成员 × 内容”，切换成员时暂停并保存。
- 任务：媒体执行队列按需读文件、往音频服务推帧，保持约 2 s 提前量；不占界面任务。卡被拔、读错：会话结束，事件带原因。
- 跳转：用 Ogg 页的颗粒位置二分查找（只读几页）。
- **记位置**：暂停、停止、跳转、换内容、低电量时写成员集合 `media.positions`（Flash）；播放中每 30 s 写 TF 卡上的 `/cache/sys/media-pos`（写 Flash 会打断声音，TF 卡不会，底座设计 §5.5.1）；下次打开取两者较新的。
- 后台：会话属于系统服务，应用切走、锁屏都继续（`INK_NEED_AUDIO`）。声明了 `INK_BG_PLAYBACK` 的应用保持后台状态。
- 断网无关；AI 朗读仍走实时会话。

### 10.3 录成文件

`audio.record {path?, max_seconds?}`：默认存 `/me/recordings/<时间>.opus`，回执的对象是文件路径。和 AI 共用一个麦克风：`ink_audio_capture_begin` 只有一个主人，录音时按 AI 键提示“正在录音”，免按键的唤醒词不触发；反过来 AI 这一轮进行时录音回“忙”。来电打断录音（焦点）。帧先进 PSRAM 环，媒体执行队列写文件，1 s 一页，5 s 同步一次；空间不够或到时长自动停并报原因。断电：Ogg 页自带边界，最多丢最后几秒，文件仍可播放（没有结束页也按文件尾结束）。

### 10.4 路由和蓝牙耳机

**不知道的（§14）**：BH1098G 没有手册；官方只写“模式 2 经蓝牙耳机通话”；扫描、配对、连接的串口协议没有拿到；音质未知；回采声道在模式 2 下有没有未知。所以：

- HAL 只定接口（`hal_bt.h`、`hal_audio_route_*`），**不假设协议**。真机端口在协议弄清之前 `hal_bt_available()` 返回假，系统照常用外放。
- 模拟器的耳机是行为模型，在 HAL 这一层（不编 AT 命令）：脚本 `headset add/on/off/range`；连上后声音算到耳机上（指标 `audio.bt.played_ms`），功放关，麦克风来自耳机且没有回采；切模式只调用已知的 `bh1098g_set_mode`（700 ms，官方代码），不动 P14；通话中不切。
- 系统：蓝牙服务（扫描、配对、连接、忘记，已配对的存设备集合 `bt.devices`），连上自动切到耳机（`audio.route` = `bt_headset`），断开退回外放（`speaker`）；耳机断开时**媒体暂停**（不让声音突然外放，D-68 里一起记）。
- 儿童音量上限 `audio.child_max` 在所有路由上都生效（底座设计 §7.4，Sony 的反例）。

采样率仍固定 16 kHz（RK-12）；S31 自带蓝牙不在这一轮（D-63）。

## 11. Wi-Fi、成员和 PIN

### 11.1 Wi-Fi 管理

HAL 加：

```c
int  hal_wifi_scan(hal_wifi_ap_t *out, int max, uint32_t timeout_ms);  /* 任务，阻塞几秒 */
int  hal_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms); /* 0、鉴权失败、找不到、超时 */
void hal_wifi_set_known(const hal_wifi_known_t *list, int n);          /* 自动选网时平台从这些里连信号好的 */
```

- 服务（`system/services/net/wifi.c`）：`wifi.scan`、`wifi.list`、`wifi.connect`、`wifi.forget`；状态 `wifi.state`（idle、scanning、joining、connected、failed）、`wifi.ssid`。
- 已知网络存设备集合 `net.wifi`（内容不开放给 AI，不进任何快照和日志，密码只进不出）。凭据放 `/user/sys`（D-57 的理由），设备丢失时能被读出，属于底座设计 §8.1 已知的风险。
- 连接新网络是系统级（AI 发起走确认卡，卡片只显示网络名）。
- 选网逻辑（D-58）不变：`hal_net_available(WIFI)` = 已知网络有一个在范围内。
- 模拟器：脚本 `ap add <名字> <信号> [密码]`、`ap del <名字>`；现有的 `wifi on|off|broken` 保留（等于一个已知网络）。

### 11.2 成员表

设备集合 `member.table`（成员编号、名字、角色、盐、PIN 校验值、失败次数、下次可试时间、白名单）。内置的两位成员（`owner`、`kid`）是第一次开机的默认值，以后由中枢同步。能力 `member.list`、`member.add`、`member.remove`（系统级）。删除成员：清它的成员数据区和成员文件区（不可逆，家长确认）。

### 11.3 PIN

- PBKDF2-HMAC-SHA256，每台设备独立的盐（底座设计 §8.1），迭代次数让 S31 上大约 100 ms（D-69，待实机校准），在后台执行队列算，不占界面任务。
- 校验值存表里，明文不落盘、不进日志、不进参数 JSON；`member.unlock`、`member.set_pin` 不开放给 AI 和远端。
- 输错递增等待：3 次之后 30 s，再错翻倍，最多 15 分钟；次数落盘，重启不清零。忘了 PIN 由中枢重置（以后）。

### 11.4 成人空间闲置退回（D-16）

`member.authed`：成人解过 PIN 为真。成人闲置 `member.idle_return_s`（默认 120 s）或进入锁屏：`authed` 变假，应用保存、停止，回到成员选择（占位）。`authed` 假时策略关口只放行标了 `while_locked` 的能力（`member.*`、`alarm.dismiss`、`alarm.snooze`、`call.answer`、`call.hangup`），AI 一律拒绝。孩子没有 PIN，不受影响。没设过 PIN 的成人也不受影响（第一次开机）。

切换成员时：剪贴板清空、AI 这一轮结束、媒体暂停存位置、应用全停、通知换收件箱、输入法换用户词频、确认卡作废。

## 12. 系统升级（R-056）

### 12.1 总流程

```mermaid
flowchart TB
  A["来源：中枢（OTA）或 TF 卡（卡刷）"] --> B["升级包放到 TF 卡"]
  B --> C["验签名、产品、版本；逐段算哈希"]
  C -->|不过| X["拒绝，文件留着，说明原因"]
  C --> D["用户确认（人，不是 AI）；电量 ≥ 30% 或在充电"]
  D --> E["写另一个应用槽（存储任务，Flash 守卫，避开刷新）"]
  E --> F["设下次从新槽启动，重启"]
  F --> G["新系统自检"]
  G -->|通过| H["确认有效；写字库和资源（如果包里有）；做数据格式迁移；清升级包"]
  G -->|没通过或中途重启| I["引导程序回滚到旧槽；旧系统报告失败"]
```

OTA 和卡刷只有第一步不同，后面完全一样：同一种包、同一个校验、同一个安装流程。

### 12.2 升级包 `.inkfw`（D-69）

```text
头（小端，4 KiB 以内）：
  magic "INKFW1"、format、product（"metalio-e-ink4-plus"）、version（字符串）、version_code（递增整数）、
  min_from_code（能从多老的版本升）、flags（允许降级）、段表 × n {kind: app|font|resources, offset, size, sha256}
  签名：ECDSA P-256 over 头（除签名外）；key_id
段数据
```

- 公钥编进固件；私钥离线保存（底座设计 §8.1），中枢只分发。签名只防“有人塞别的固件”，不依赖安全启动和 eFuse（不烧，硬性规则）。
- HAL `hal_crypto_verify_fw(hash, sig, key_id)`：真机用 mbedTLS；**模拟器是替身**（带密钥的 SHA-256），只验证系统逻辑：坏包、改过的包、错的产品、降级都被拒绝。SHA-256 本身是真的。
- 版本：`version_code` 不能比现在小，除非包里带“允许降级”（也在签名范围内）。

### 12.3 下载（OTA）

`update.check`：向中枢要最新版本（`update/check` → `{version, version_code, size, sha256}`）；有新版本发 `update.available` 和一条通知。`update.download`：后台作业，分块从中枢取（`update/fetch {offset, len}`，二进制帧，后台优先级，实时会话时让路），写到 TF 卡 `/content/.update/<版本>.inkfw.part`，能续传；完了校验、改名。没卡不能 OTA（Flash 放不下暂存，也不想边下边写）。电量低、有实时会话时暂停。设置 `update.auto_download` 默认关（流量要钱）。

### 12.4 卡刷

用户经 U 盘模式把 `.inkfw` 放到 TF 卡的 `content/update/` 目录（系统里就是 `/family/update/`）。系统在开机、退出 U 盘模式、卡插上时看一眼；发现比现在新的包：发通知“发现系统更新”，点开确认后安装。能力 `system.upgrade_from_card`。这条路不需要网，所以中枢坏了、Wi-Fi 配不上也能救。

### 12.5 安装、自检和回滚

- HAL `hal_ota.h`：`hal_ota_info`、`hal_ota_begin`、`hal_ota_write`、`hal_ota_end`、`hal_ota_set_boot`、`hal_ota_mark_valid`、`hal_ota_rollback`；`hal_power_reboot`。真机对应 `esp_ota_*`。
- 写槽在存储任务里，每次一块（擦 4 KiB + 写），每块都有 Flash 守卫，块之间让路给刷新和读（整个槽约 1100 块，每块 60～90 ms 停顿，约 1～2 分钟，界面明显变慢，所以安装时显示进度、不让用别的）。通话、录音、播放中不开始；开始后不被打断，来电照样响但等写完这一块。
- 断电：新槽写一半没有关系，旧槽没动；开机看到 `/user/sys/update.json` 说“在写”就清状态，包还在卡上，重新来。
- 新系统第一次启动（`pending_verify`）：`ink_update_boot_check()`：所有服务起来了、屏刷过一次、`/user` 能读写；通过后 `hal_ota_mark_valid`。自检之前不做数据格式迁移（底座设计 §10.3）。崩溃或 60 s 内没通过：重启，引导程序回滚。旧系统启动后看到“上次在试新版本”，发通知说明失败，并记下这个版本不再自动提示。
- **回滚要新引导程序**：出厂的没开回滚（底座设计 §10.1），第一次装我们的系统要经 USB 写引导程序、分区表和系统，要用户同意（§14）。在那之前的真机上这一节不能完全生效。
- 字库和资源没有 A/B：新系统确认有效之后再写；写的时候断电，开机发现资源标记没完成：用内置最小字库兜底，接着写。

### 12.6 升级和数据

系统升级不碰 `/user` 和 TF 卡（§2.1）。格式变了由新系统迁移，迁移前留旧格式副本到下一次同步（底座设计 §10.3）。恢复出厂（`system.factory_reset`，家长确认）清 `/user`、`/content/.m`、`/content/.update`、`/cache`；不动系统区、资源区；家庭内容区默认保留。

### 12.7 给 AI 的能力

`update.status`、`update.check`、`update.download` 给 AI（它可以提醒“有新版本”“我帮你下好了”）；`system.upgrade`、`system.upgrade_from_card` 不给（现有规则：名字带 upgrade 的 AI 一律拒绝，底座设计 §6.2）。

## 13. 其余缺项

详细设计 §13 里和这些相关的：

| 缺项 | 处理 |
| --- | --- |
| `storage.free` | §4 |
| `time.zone`、闹钟提醒 | §5 |
| `audio.record` | §10.3 |
| `system.reboot` | HAL 加 `hal_power_reboot`，能力系统级 |
| “一直在听”没有专门测试 | 加测试 |
| 作业“离线不跑、联网后跑”没有专门测试 | 加测试 |
| 生命周期回调和状态保存 | §8 |
| 权限检查 | §8.1 |
| 给 AI 的上下文带系统状态；事件转发（主动决策的第一步） | §1.2 第 3、4 条 |
| 工具清单只有 6 KiB 缓冲，能力多了放不下 | 分页（§1.2 第 5 条） |
| `MAX_CAPS` 96、事件订阅 16、状态 48、设置 32、`on_online` 2 | 都要加大，放 PSRAM |
| `SDK 头文件自成一体` | 这轮不做，界面框架会动这些头文件，记在 §15 |

明确不做的（以后）：媒体索引、节气、成员级设置（现在设置表是设备级的，音量等都全家一个）、AI 联想、S31 自带蓝牙、配对（首次开机）、无线传输。

## 14. 决定、实机问题

### 14.1 决定（都可逆，按推荐暂定）

| 编号 | 决定 | 方案 | 推荐 |
| --- | --- | --- | --- |
| D-65 | Flash 分区和 `/user` 目录（R-057） | **A** 去掉 `model`，两个 4.5 MiB 应用槽，约 1.4 MiB 给 LittleFS，第一次经 USB 写；`/user` 目录少、靠前缀分；**B** 不改分区表，只拿 `model` 的 400 KiB 当 `/user`（不用写表，但太小）；**C** 每个应用一个目录（清楚，但每个目录至少 8 KiB） | A |
| D-66 | 结构化数据放哪、怎么写 | **A** 打开的集合整份放 PSRAM，追加日志加快照，两代日志；**B** 只放索引，内容按需读（省内存，但界面翻列表要等 Flash）；**C** SQLite（底座设计已否） | A |
| D-67 | 本地音频文件格式 | **A** Ogg Opus（标准、电脑能播，16 kHz 单声道）；**B** 自定义帧格式（更简单，但别的软件播不了）；**C** 加 MP3 解码（要新解码器和内存） | A |
| D-68 | 勿扰和耳机断开 | **A** 勿扰时闹钟照响、来电不响但全屏；耳机断开时媒体暂停；**B** 勿扰时全部静音（闹钟不响有风险）；**C** 耳机断开继续外放 | A |
| D-69 | 升级包和 PIN 的密码学 | **A** 包头签名 ECDSA P-256，PIN 用 PBKDF2-HMAC-SHA256；**B** 只靠 TLS 下载不签名（卡刷没有保护）；**C** 开安全启动（不可逆，硬性规则不做） | A |
| D-70 | 拼音词库来源 | **A** 开源词表（MIT 或类似）生成，放 TF 卡，由中枢或卡刷下发；**B** 只内置常用字（打字慢）；**C** 全放中枢（断网不能打字） | A，具体词表要核许可 |
| D-71 | 升级包放哪 | **A** 先完整下载到 TF 卡，校验后再写槽；**B** 边下边写槽（不需要卡，但网断了要重来、写之前不能验） | A |
| D-72 | U 盘模式开放什么 | **A** 整张 TF 卡（上游也是这样，简单；电脑能看到所有成员的卡上文件，所以要成人确认，孩子不能开），系统区和 `/user` 永远不开放；**B** 卡上分两个分区，只开放公共的（要重新分区）；**C** MTP（能按文件开放，但 ESP-IDF 上没有现成的） | A |

### 14.2 只有实机能回答的问题（不写设备，交给用户决定）

| 问题 | 影响 | 怎么测 |
| --- | --- | --- |
| 第一次装系统：写引导程序、分区表、系统 | 回滚和 `/user` 都依赖它 | 要整片备份和用户同意（已有备份） |
| P16 一直低（RTC 中断线） | RTC 唤醒能不能用；不能用也靠定时器准时，只是不省电 | 测量固件读 P16、设 RTC 闹钟 |
| 关机后 RTC 闹钟能不能开机 | 关机闹钟 | 同上，加实际关机 |
| TF 卡有没有卡检测引脚、拔卡时 HAL 怎么报错 | 卡状态的及时性 | 看原理图或实测 |
| U 盘模式：S31 的 USB 口平时是串口/JTAG，切 TinyUSB MSC 后下载和串口日志怎样；出厂固件的做法 | R-058，上机调试方式 | 用出厂固件试一次（电脑挂载可能写卡，要用户同意） |
| 电脑和充电器怎么区分（USB 数据线插上的判断） | 什么时候提示 U 盘模式 | 同上 |
| FAT 上改名覆盖已有文件 | `ink_fs_write_atomic` 在真机上要先删后改（ESP-IDF 的 FATFS 不允许覆盖），有一小段窗口 | 端口实现时处理，实测 |
| BH1098G 模式 2：扫描、配对、连接的协议，音质，回采 | 蓝牙耳机（D-63） | 底层依赖 §6 |
| 通话声音怎么走 | 通话焦点只占位，不出声 | RK-07、SP-06、SP-07，打电话要花钱，用户选 |
| `lfs_fs_size` 和 `f_getfree` 的实际耗时 | `storage.free` 刷新频率 | 测量固件 |
| PBKDF2 迭代次数 | PIN 解锁时长 | 测量固件 |
| 升级时写槽对音频、触摸的影响 | 安装体验 | 用户同意后试一次 |

## 15. 验证记录

每项完成后回填：提交、测试、指标、与本文不一致的地方。

### 15.1 预算

基线（`claude/system-layer` 1fbf510，模拟器悲观档）：内部 + DMA 最低余量，AI 场景 PQ-BN 档 33504 字节（底线 32768），EPDiy 档 36168 字节。这轮新增的内部 RAM 目标 ≤ 300 字节（两把锁、一个定时器）。

| 项 | 状态 | 提交 | 测试与指标 |
| --- | --- | --- | --- |
| （实现后回填） | | | |

## 修订记录

- v0.1（2026-11-04）：初稿，含用户补充的 R-055～R-058。
