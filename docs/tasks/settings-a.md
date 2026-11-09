# settings-a：设置里的声音、存储、关于、隐私（settings-all 的第一块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/settings-a/REPORT.md。

## 要做的

预算 350 行产品代码：`SetVoice`、`SetStorage`、`SetAbout`、`SetPrivacy` 四页，接上后端。蓝牙“当音箱用”“翻页器”、锁屏相机入口是收起来等用户定的，接口保留，不要删（这一块不碰蓝牙）。

## 从哪里看起

- 这四页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 现有的设置页面（在 system/ui/shell/ 下搜 Settings、SetDisplay、PAGE_NETWORK）；设置项的定义 `ink_settings_define`；docs/system/capabilities.md 里的 `settings.*` 能力；存储服务的状态（剩余空间、卡、U 盘）。
- 页面用 F1 控件语义、F4 通用列表。

## 边界

- 只做这四页，其它设置页是后面的任务。
- 设置都走已有的 settings 能力，不另开一条写法。
- 新页面放新文件；navigation.c 只加注册的几行。同时在做的有 phone-calls、sms、memo、power-pages，以及在修的 clock-alarm、ai-chat，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 四页都能打开，显示的值来自后端，改了重启后还在（测试）。
2. 每页有 AI 快照；按钮和 AI 走同一个能力（测试）。
3. 孩子成员看不到、改不了不该改的设置；去掉检查，测试失败（反例）。
4. 全量测试和内存检查通过；产品代码不超过 350 行。
