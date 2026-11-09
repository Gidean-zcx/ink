# contacts：联系人（phone-sms 的第一块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/contacts/REPORT.md。

## 要做的

backlog 的 phone-sms 一节拆成几块，这是第一块，预算 300 行产品代码（查缺口报告 B13）：

- 联系人存进数据服务，按成员，能增删改查。
- `PhoneContacts` 页面用通用列表（F4，`ink_ui_list`），姓名和号码用短输入框（F2）。
- “选联系人和号码”做成能力，给以后的拨号和短信用。
- AI 能查、能加联系人，和按钮走同一个能力。

## 从哪里看起

- `PhoneContacts` 的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题（system/ui/theme.c）和比例为准。
- 系统应用的写法：system/apps/ 下已有的应用（例如 remote）和应用注册 system/ui/app_registry.c；F1、F2、F4 的报告在 docs/tasks/ui-semantics、ui-input-host、ui-list-paging。
- 数据服务 `ink_db`（按成员的集合）。通信服务 system/services/comm/ 只读，不改。

## 边界

- 只做联系人。拨号、通话记录、短信是后面的任务。
- 号码只做基本校验（数字、`+`、长度），不做运营商相关的东西。
- 孩子成员能不能改联系人，按现有的成员权限（`ink_member_allows`）。
- 页面放在自己的文件里；共用的列表（应用目录、CMake、能力注册）只在末尾加行。同时在做的还有 clock-alarm、ui-file-picker。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 增删改查，重启后还在；按成员隔离，换了成员看不到别人的（测试，含反例）。
2. `PhoneContacts` 空页、一页、多页都显示对，AI 快照里有分页（快照测试）。
3. 选联系人的能力返回联系人编号和选中的号码；AI 和触摸走同一个能力（测试）。
4. 新操作进能力表；全量测试和内存检查通过；产品代码不超过 300 行。
