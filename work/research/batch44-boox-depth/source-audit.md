# Batch 44：BOOX 来源字段审计

日期：2026-10-27。只读本地文件，不联网、不修改 `docs/`。本报告只比对来源字段并摘录适用条件定位，不作产品适用性结论。

## 总数与范围

- 唯一 article ID：**31**。来源为 `sources.json` 的 11 篇与 `remaining-texts.json` 的 20 篇；两组无重复。
- 正式 evidence 文件扫描：131 个；同 URL 的 E 编号按精确 URL（忽略末尾 `/`）匹配。
- 图片：7 张；全部只校验文件、附件 URL、来源 article 与 SHA，未进行视觉查看。
- 31 篇正文中未检出显式高于 V4.1.2 的固件版本号。页面 `updated_at` 晚于机型固定基线日期时，仅记录为来源元数据，不当作固件适用条件。

## 31 篇来源字段

| ID | 标题 | updated_at | 同 URL E | 条件标记 |
| --- | --- | --- | --- | --- |
| `10701146091284` | [Screensaver](https://booxsupport.zendesk.com/hc/en-us/articles/10701146091284-Screensaver) | `2026-05-25T02:20:37Z` | — | limited_models, tab_series |
| `10701108549140` | [Home Screen and Dock](https://booxsupport.zendesk.com/hc/en-us/articles/10701108549140-Home-Screen-and-Dock) | `2026-05-24T11:30:17Z` | — | tab_series |
| `10701630316436` | [Screenshot](https://booxsupport.zendesk.com/hc/en-us/articles/10701630316436-Screenshot) | `2025-06-09T08:20:02Z` | — | — |
| `25403769498004` | [Screen Recording](https://booxsupport.zendesk.com/hc/en-us/articles/25403769498004-Screen-Recording) | `2025-06-09T08:20:47Z` | — | — |
| `10701620011540` | [Scan Documents App](https://booxsupport.zendesk.com/hc/en-us/articles/10701620011540-Scan-Documents-App) | `2025-06-09T08:19:12Z` | — | tab_ultra |
| `10701172864916` | [Display Settings](https://booxsupport.zendesk.com/hc/en-us/articles/10701172864916-Display-Settings) | `2026-10-08T21:10:28Z` | — | limited_models |
| `10701109999636` | [Volume](https://booxsupport.zendesk.com/hc/en-us/articles/10701109999636-Volume) | `2026-05-18T07:57:10Z` | — | tab_series |
| `10701622160916` | [Screen Mirroring](https://booxsupport.zendesk.com/hc/en-us/articles/10701622160916-Screen-Mirroring) | `2025-06-06T06:34:02Z` | — | — |
| `10701171762452` | [Sleep and Battery](https://booxsupport.zendesk.com/hc/en-us/articles/10701171762452-Sleep-and-Battery) | `2025-06-06T06:22:52Z` | E-BXCD-14 | tab_series |
| `10701262170644` | [App Management Settings](https://booxsupport.zendesk.com/hc/en-us/articles/10701262170644-App-Management-Settings) | `2025-06-06T07:06:48Z` | E-BOOX-22, E-BXCD-07, E-PALMA-13 | limited_models |
| `25213043150996` | [Navigate Your Device](https://booxsupport.zendesk.com/hc/en-us/articles/25213043150996-Navigate-Your-Device) | `2026-05-26T20:22:12Z` | E-BOOX-05, E-BXCD-03, E-PALMA-04 | limited_models, tab_series |
| `10701173983508` | [Language and Input](https://booxsupport.zendesk.com/hc/en-us/articles/10701173983508-Language-and-Input) | `2026-05-25T11:59:24Z` | — | — |
| `10701147047188` | [WiFi and Bluetooth](https://booxsupport.zendesk.com/hc/en-us/articles/10701147047188-WiFi-and-Bluetooth) | `2026-05-23T09:00:48Z` | — | — |
| `10701137102100` | [Front Lights](https://booxsupport.zendesk.com/hc/en-us/articles/10701137102100-Front-Lights) | `2026-05-26T17:15:44Z` | — | firmware_version |
| `20259982794772` | [Customize Mouse](https://booxsupport.zendesk.com/hc/en-us/articles/20259982794772-Customize-Mouse) | `2025-06-06T06:33:06Z` | — | limited_models, tab_series |
| `10701256390548` | [Contrast and Color](https://booxsupport.zendesk.com/hc/en-us/articles/10701256390548-Contrast-and-Color) | `2026-05-27T12:52:46Z` | — | — |
| `10701308914964` | [Download and Install Apps](https://booxsupport.zendesk.com/hc/en-us/articles/10701308914964-Download-and-Install-Apps) | `2026-05-23T04:22:45Z` | E-BXCD-08 | limited_models |
| `15194669819796` | [Work Profile](https://booxsupport.zendesk.com/hc/en-us/articles/15194669819796-Work-Profile) | `2026-05-14T13:53:17Z` | — | limited_models |
| `10701348022676` | [BOOXDrop](https://booxsupport.zendesk.com/hc/en-us/articles/10701348022676-BOOXDrop) | `2026-10-19T16:36:11Z` | — | — |
| `10701363849108` | [NeoBrowser](https://booxsupport.zendesk.com/hc/en-us/articles/10701363849108-NeoBrowser) | `2026-05-27T12:30:12Z` | — | — |
| `10701356797332` | [PushRead](https://booxsupport.zendesk.com/hc/en-us/articles/10701356797332-PushRead) | `2025-06-06T07:14:15Z` | — | — |
| `10701353744916` | [Calendar Memo](https://booxsupport.zendesk.com/hc/en-us/articles/10701353744916-Calendar-Memo) | `2026-05-24T12:34:06Z` | — | — |
| `10701398359572` | [Integrated Third-Party Cloud Storage](https://booxsupport.zendesk.com/hc/en-us/articles/10701398359572-Integrated-Third-Party-Cloud-Storage) | `2026-10-16T22:08:54Z` | E-BOOX-24 | — |
| `10701398942356` | [Transfer with Your Smartphone](https://booxsupport.zendesk.com/hc/en-us/articles/10701398942356-Transfer-with-Your-Smartphone) | `2026-05-23T09:47:20Z` | — | — |
| `10701493804820` | [Enhance Content Display](https://booxsupport.zendesk.com/hc/en-us/articles/10701493804820-Enhance-Content-Display) | `2026-04-28T06:20:15Z` | — | — |
| `20261817911828` | [AI Assistant](https://booxsupport.zendesk.com/hc/en-us/articles/20261817911828-AI-Assistant) | `2026-05-14T12:12:56Z` | — | limited_models |
| `37475824326420` | [Stylus Calibration (EMR)](https://booxsupport.zendesk.com/hc/en-us/articles/37475824326420-Stylus-Calibration-EMR) | `2025-05-29T07:20:52Z` | — | — |
| `37475843783700` | [InkSpire](https://booxsupport.zendesk.com/hc/en-us/articles/37475843783700-InkSpire) | `2026-05-22T12:35:44Z` | — | tab_x_c |
| `10701627053460` | [Connection and Settings](https://booxsupport.zendesk.com/hc/en-us/articles/10701627053460-Connection-and-Settings) | `2025-06-09T08:15:50Z` | — | tab_ultra |
| `10701617416852` | [Keyboard Shortcuts](https://booxsupport.zendesk.com/hc/en-us/articles/10701617416852-Keyboard-Shortcuts) | `2025-06-09T08:17:51Z` | — | — |
| `42939290151700` | [Talk Insights](https://booxsupport.zendesk.com/hc/en-us/articles/42939290151700-Talk-Insights) | `2026-05-16T22:39:30Z` | — | — |

## 条件原文摘定位

### 10701146091284 Screensaver

- `tab_series`：In this article, you will learn how to set the screensaver and shutdown image for your BOOX Tab Series device.
- `limited_models`：Monthly Calendar Screensaver is only available in some models.

### 10701108549140 Home Screen and Dock

- `tab_series`：The BOOX Tab Series devices have a redesigned interface for you to enjoy a unique user experience like on a conventional tablet.

### 10701620011540 Scan Documents App

- `tab_ultra`：Note: The camera is only available on Tab Ultra, Tab Ultra C, and Tab Ultra C Pro.
- `tab_ultra`：Tab Ultra, Tab Ultra C, and Tab Ultra C Pro are equipped with an exclusive 16MP rear camera as the Smart Scanner for document scanning, OCR, and other amazing functions.

### 10701172864916 Display Settings

- `limited_models`：Note : Importing fonts is only available in some models.

### 10701109999636 Volume

- `tab_series`：The BOOX Tab Series devices are equipped with a speaker for audio output.

### 10701171762452 Sleep and Battery

- `tab_series`：The BOOX Tab Series devices consumes more power than typical BOOX devices due to an increased performance and efficiency.

### 10701262170644 App Management Settings

- `limited_models`：Note: This function is only available for select models.

### 25213043150996 Navigate Your Device

- `tab_series`：Smart Assistant Smart Assistant is a screen of useful shortcuts on your BOOX Tab Series device.
- `tab_series`：Multi-Finger Gestures The BOOX Tab Series devices are available with multi-finger gestures.
- `limited_models`：Auto-rotate is only available for models with G Sensor.

### 10701137102100 Front Lights

- `firmware_version`：Since Firmware V4.1, you can switch between four front light modes with a tap, or customize the front light settings manually to suit your preferences.

### 20259982794772 Customize Mouse

- `tab_series`：rience, please make sure: Update to the latest firmware Choose Fast/Ultrafast mode Set Ponter Speed to about 80% Note : Currently, only the Tab series supports customizing the mouse.
- `limited_models`：trol experience, please make sure: Update to the latest firmware Choose Fast/Ultrafast mode Set Ponter Speed to about 80% Note : Currently, only the Tab series supports customizing the mouse.

### 10701308914964 Download and Install Apps

- `limited_models`：Download from the Google Play Store Google Play is pre-installed on some models.

### 15194669819796 Work Profile

- `limited_models`：Note: This function is only available for select models.

### 20261817911828 AI Assistant

- `limited_models`：The AI Assistant is only available in some models.

### 37475843783700 InkSpire

- `tab_x_c`：InkSpire is BOOX’s active stylus with wireless charging and magnetic attachment, currently compatible with Tab X C .
- `tab_x_c`：Attach the middle section of InkSpire to the magnetic charging area on the right side of the Tab X C.

### 10701627053460 Connection and Settings

- `tab_ultra`：Please do as follows: Setting＞More Settings＞Physical Keyboard＞Trackpad Gesture＞Custom Note : Currently, only Tab Ultra C Pro supports customizing Trackpad Gestures.

以上摘录来自 `boox-articles.json` 对应 article body 的去标签连续文本，供 root 回到原 HTML 定位；没有据此判断 Note Air5 C 或 Palma 2 是否适用。

## 7 张附件字段

| 文件 | 附件 ID | 来源 article | HTTP / type | bytes | SHA-256 |
| --- | --- | --- | --- | ---: | --- |
| `home.jpg` | `10725493990676` | 10701108549140 | 200 / `image/jpeg` | 188317 | `239d7b8fc1560807591d481f7c02c56dc07e212766e003a56514d45a858b5e51` |
| `screen-record.jpg` | `25403784340116` | 25403769498004 | 200 / `image/jpeg` | 165100 | `575deec83ec0e8460aca3dbc9d6c7f948486f66b078f6f939589ca7cf6b83ed5` |
| `volume.jpg` | `25175232660116` | 10701109999636 | 200 / `image/jpeg` | 326114 | `5e16b6ee8f0fd0fd532d498bc7baf0a7413f4a52b8bcb2a5609688c8acf1f3e1` |
| `screensaver-types.jpg` | `10726276107156` | 10701146091284 | 200 / `image/jpeg` | 164851 | `fae41d5c31af195fc58fc9aebc83dffbe0c77a0c5122c73176fd5520a8447b63` |
| `screensaver-images.jpg` | `10726306716820` | 10701146091284 | 200 / `image/jpeg` | 129013 | `c03c9b8127d881c260589f821a7b23082f4e88fd753ff97e420c4cd4b47e36af` |
| `mirror-step.jpg` | `27963633162900` | 10701622160916 | 200 / `image/jpeg` | 309687 | `1b5046a0fc79e6f12a2bde2d224c41030dcff2d77b8b890279e8c11ebe8933cd` |
| `mirror-browser.jpg` | `27963693945876` | 10701622160916 | 200 / `image/jpeg` | 177188 | `42c1bc6ba4de5982ddc3a0e9a81083c654824f2cb59128517d50f4b18c275d4f` |

## 文件与命令

- 读取：`work/research/batch44-boox-depth/sources.json`、`captures.json`、`images/`、`work/research/batch44-boox-inventory/remaining-texts.json`、`work/eink-systems-research/boox-articles.json`。
- 正式 E 映射读取：`docs/research/interaction-library/evidence.jsonl` 与 `docs/research/interaction-library/batches/*evidence.jsonl`。
- 初始 `ls` 与 JSON 结构探测：exit 0；确认 sources 11、remaining 20、captures 7。
- 本地条件词检索与版本号检索：exit 0；确认 Tab Series/Tab Ultra/Tab X C/部分机型摘录，并确认无显式 `> V4.1.2` 版本号。
- `/tmp/batch44_source_audit.py`：exit 0；生成本 JSON 与 Markdown。
- 最终 JSON、31 唯一 ID、URL/updated_at、E 映射、7 图 SHA/附件来源、diff 检查见验证输出。

## 证据、未知与状态变化

- evidence 映射只说明正式 JSONL 中存在同 URL E，不说明该 E 已覆盖本文全部正文。
- 附件来源按 31 篇缓存 body 中出现的 attachment ID 映射；未打开图片。
- 未知包括具体固件构建、帮助正文的历史版本差异、未列机型是否支持。
- 状态变化仅为新增 `source-audit.json` 与 `source-audit.md`；正式文档、原缓存和图片未修改，外部状态未改变。
