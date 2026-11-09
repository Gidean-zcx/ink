# kid-2：孩子的用时、时间到、请家长、孩子的控制中心

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/kid-2/REPORT.md。

## 要做的

预算 350 行产品代码：

- 用时策略（后端）：每个孩子一个“每天能用多久”（分钟，0 表示不限），存在成员策略里（system/policy/members.c 的成员记录，和其它成员数据一起保存），不要每个孩子新增一个设置项。孩子的空间开着、屏幕没锁时计时，本地零点清零。家长可以给今天加时间。能力：读策略（孩子自己也能读）、改每天的时长、加今天的时间；改和加都要家长密码（照 `member.add` 的 `parent_pin` 写法）。
- `KidTimeUp`：用时到了就出现。“今天的时间到了”；读到哪儿照常记好；留“打电话”（电话不限制，D-25）和“爸爸妈妈加时间”（家长输密码后加时间）。时间到以后孩子只能用电话和短信。
- `KidLimit`：孩子从别处跳到不能用的地方（通知、AI、链接）时出现：“要爸爸妈妈来”“请他们输密码”“算了”。家长输对密码，只放行这一次。孩子自己在主页上看不到不能用的东西（规格第 279 行），所以平时不会进这页。
- `KidControl`：孩子的控制中心，只有亮度、暖光、音量（不超过 `audio.child_max`）和通知（规格第 187 行）。

## 从哪里看起

- 画板 `$INK_DESIGN/png/` 里的 KidTimeUp、KidLimit、KidControl、SetKid；规格 docs/product/system-ui-spec.md 第 85、187、279 行和第 1 节的数。
- 成员和密码：system/policy/members.c（`parent_pin`、`child_ok`、`allow`）；家长输密码的页已经有了（kid-1 的 system/ui/shell/member_pages.c、member_gate.c），复用，不要再做一套密码输入。
- 现在的控制中心在 system/ui/shell/ 里（找 `shell.control`），孩子版本复用它的行和写法。

## 边界

- `SetKid` 页在并行的 settings-b 里，**不要改 settings_family.c**；你只提供能力，`SetKid` 上“每天能用多久”那一行合并时由主会话接。
- 同时在做的有 settings-b（修复中）、transfer-pages、remote-app、activity-card、fw-f8、storage-usage，不要改它们的文件；共用文件（navigation.c、main.c、CMakeLists.txt、测试清单）只在末尾加函数或行，现有函数里只加最少的接线调用。
- 不做 `KidHome`、`KidToday`、`KidApps`（下一批）。

## 验收

1. 设每天 2 分钟（测试里用模拟时钟），孩子用满后出现 `KidTimeUp`，只剩电话和短信能用；锁屏期间不计时；过零点清零（测试）。
2. 家长输对密码加时间后能接着用；输错不加；孩子自己调用改时长、加时间的能力被拒（测试，反例：去掉家长密码检查，测试失败）。
3. 孩子通过 AI 或通知跳到不能用的应用时出现 `KidLimit`，家长输密码后只放行这一次（测试）。
4. `KidControl` 只有那四样；音量调不过 `audio.child_max`（测试）；四页都有 AI 快照，按钮和 AI 走同一个能力。
5. 用时和加的时间重启后还在（测试）；全量测试和内存检查通过；产品代码不超过 350 行。
