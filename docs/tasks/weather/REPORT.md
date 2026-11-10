# weather 报告（D-125 天气：中枢取数据，设备显示）

（Claude 子代理完成，含一轮收尾；运行环境不让子代理写报告文件，主会话按它的回报整理保存。）

检出 `ink-codex/weather/repo`，分支 `claude/sub-weather`，基线 `61f55c19`。

## 验收

| 编号 | 结论 | 证据 |
| --- | --- | --- |
| 设好城市后今日屏卡片和天气页 | 通过 | 【实测】卡片 `"label":"示例市","value":"12° 小雨 15°/3°"`，图标是当前天气（雨），和天气页大卡片同一张图；天气页 12°、`小雨 · 最高 15° 最低 3°`、明天/后天/周四三行、降水概率 80% |
| 断网显示上次数据和更新时间 | 通过 | 【实测】飞行模式下点“刷新”，页面仍是上次的数据，底部“没联网，11月2日 08:00 更新” |
| 没设城市的空状态 | 通过 | 【实测】天气页“还没有设天气城市”和“选城市”；AI 调 `weather.get` 得到 INVALID 并说明 |
| AI 用 `weather.get` | 通过 | 【实测】有网时带 `"t":12`、三天预报、`"fresh":true`；断网时 `"fresh":false` 并注明“没联网，这是上次的天气” |
| 城市页真打字 | 通过 | 【实测】点输入框，键盘打 b-e-i-j-i-n-g、点候选“北京”、点“搜索”，列出候选后点第一个 |
| 第一次设城市自动上今日屏 | 通过 | 【实测】第一次设城市后卡片出现；拿掉卡片后 AI 换城市，卡片不会回来（今日屏满了时不动） |
| 重启后保留 | 通过 | 【实测】同一份闪存第二段不联网：城市还是示例市，卡片还是“12° 小雨 15°/3°”，天气页显示上次数据，`weather.get` 得 `"fresh":false` |
| 中枢单元测试 | 通过 | 【实测】`hub/tests/contract/test_weather.py`（地名查询和参数校验；30 分钟缓存、按城市分开、过期重取；出错回 -32603 且不缓存；经 WebSocket 端到端），hub 全部 `139 passed` |

天气测试 `weather main PASS ... checks 142 failures 0`、`weather restart PASS ... checks 32 failures 0`；全量 `100% tests passed out of 308`；build-asan 22 项 `100% tests passed out of 22`（查出并修了一处快照字段指向栈上数组的 stack-buffer-overflow）；S31 退出 0；gcccheck 5 个文件 0 处；图标 `88 names, 264 SVG files, 0 errors`；能力表 451。反向对照：断网保留上次数据、今日屏读卡片图标、第一次设城市放卡片、中枢缓存和错误处理，各撤一处都失败。

## 做法

- **中枢**（`hub/ink_hub/content/weather.py`）：按城市编号缓存 30 分钟（最多 64 个城市），数据源出错回 -32603 且不缓存；数据源可替换（有 `search`、`place`、`forecast` 三个异步方法、返回 Open-Meteo 形状的对象都行），真实的 `OpenMeteo` 类用 aiohttp 联网，CI 不跑；WMO 天气代码在中枢转成中文文字和 5 种图标名之一。gateway 新增 `weather/find`、`weather/get`。
- **协议**：`docs/hub/device-protocol.md` §3.16：`weather/find {"name"}` → 最多 5 个城市；`weather/get {"id"}` → 城市、更新时间、当前温度和天气、最高最低、降水、三天预报。设备只传城市编号，不传经纬度。
- **设备**（`system/apps/weather/weather.c`）：设备级集合 `weather.city`（城市和上次的回答原文，开机读回）；能力 `weather.get`（人点“刷新”总去问中枢，AI 30 分钟内用缓存）、`weather.open`、`weather.city`、`weather.find`、`weather.set_city`（只能从刚找到的结果里选，孩子不能用）、`weather.focus`（城市页输入框，键盘托管）；没联网不发请求，用缓存回答；今日卡片图标随当前天气（`ink_card_t` 加 `icon`，`app.cards` 只在有图标时输出）；第一次设城市以系统身份调 `shell.today_card` 放上今日屏；改城市的入口只在天气应用（“选城市”“换城市”）和 AI；中枢给的图标名只接受五种，其它当 cloud（原来不认识的名字会让 `ink_ui_icon` panic）。
- **图标**：新画 `cloudsun`、`rain`、`snow`（line/duo/solid），晴天、阴天用已有的 `sun`、`cloud`。
- **字库**：`tools/fontgen/ui_fonts.py` 扫描加 `system/apps`，系统应用的中等、粗体字不再从常规字库借（来自中枢的天气现象词仍借）。

设备端产品代码 312 行（预算 300，超出来自收尾轮新加的键盘托管、自动上今日屏、图标名校验、卡片图标）；`sim_hub.c` +7；中枢 `weather.py` 104 行、`server.py` +21/−1。

## Open-Meteo 条款要点（2026-11-10 查阅）

免费接口不用密钥，只许非商业使用（私人、家庭自动化算非商业）；免费档每天少于 10,000 次；数据 CC BY 4.0，必须署名（天气页写“数据来自 Open-Meteo.com”，墨水屏上点不了链接）；商用要买付费档（同样的接口加 apikey），换中枢的数据源类即可，设备不用改；没验证中文地名能否查到。

## 没做到的

打字停一下自动搜（只在按“搜索”时搜）；没联网时 `weather.find` 不重试；真实数据源没联网跑过；天气页和城市页没有画板。今日屏放什么是按家人分开存的，城市整台设备一个，所以只有设城市的那个人的今日屏会自动放上卡片。

## 改过的已有测试

- `tests/integration/ai_surface.c`：登记 6 个天气能力。
- `tests/integration/apps.c`：应用数量 17 → 18。
