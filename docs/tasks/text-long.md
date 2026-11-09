# text-long：长文本、草稿和选区动作

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足。报告写到 docs/tasks/text-long/REPORT.md。

## 要做的

backlog 原文：预算 300 行产品代码。长文本翻页（`ImeLong`）、草稿自动保存并归属页面、选区动作（问 AI、加单词）做成能力。验收：长文本的字节和字符换算有测试，草稿断电不丢，换成员不串。

再加上 docs/system/app-framework-design.md §9 的 F3：长文本和草稿的合同在这里一起做，不要两处各存一份。用户定的 D-111 ③：长文和表单进专用编辑页；短输入框是 F2（ui-input-host），已合入。

## 从哪里看起

- `ImeLong` 的画板在 `$INK_DESIGN/png/`，先看图；规范 docs/product/system-ui-spec.md。
- 文本服务 system/services/text/；F2 的短输入宿主（ui-input-host 加的文件和 docs/tasks/ui-input-host/REPORT.md）。
- 草稿落盘用数据服务（`ink_db_*`）；断电测试的写法看 tests/services_finish/ 里 shutdown、lowbattery 相关的测试。

## 边界

- 用 F2 已有的输入宿主和键盘，不另做一套键盘。
- 不做备忘应用本身（那是 memo-calendar-clock）。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 中英混排、表情、组合字符的长文本：字节位置和字符位置互换正确，翻页不切坏字符（测试含反例）。
2. 草稿自动保存：输入后断电再开机，草稿还在，属于原来的页面和成员（测试）。
3. 换了成员，看不到上一个人的草稿；去掉检查，测试失败（反例）。
4. 选区动作“问 AI”“加单词”是能力，按钮和 AI 走同一条路（测试）。
5. 全量测试和内存检查通过；产品代码不超过 300 行。
