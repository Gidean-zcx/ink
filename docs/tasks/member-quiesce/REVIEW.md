# member-quiesce 第 1 轮复查

复查日期：2026-11-08。基线：`ed3d122665cf704bb77a962f1fe38d66f6febe01`；被审查提交：`3d375ba0d311437b9d5ec46648b3012b33ba7d6e`。

状态：复查进行中，尚未作验收结论。仅写本文件，不改产品、测试或共享文档，不提交。构建与测试产生的证据放在忽略的构建目录或临时目录。

已阅读任务书、总规则、复查提示及执行报告；执行报告明确承认完整验收失败。

## 已完成的独立检查

- 【实测】`taskpolicy -c utility cmake --build build -j3` 退出 0。
- 【实测】按总规则运行普通完整 CTest，退出 8，200/206 通过，耗时 100.43 秒。失败：`ui-privacy-regressions-metalio-plus`、`ui-privacy-regressions-desktop-lcd`、`device-events`、`integration-member-wake-stream`、`integration-races`、`integration-confirm`。没有跳过。
- 【实测】`integration-confirm` 完成 113 项通过、1 项失败：`tests/integration/confirm.c:246` 没有收到 `member_changed`，保留上次的 `withdrawn`。该流程未暂停线程：打开确认卡后正常换人即触发。
- 【实现】`system/policy/cap.c:942–944` 先设 `CB.sealed`；`members.c:103–105` 在恢复接收前发布成员状态，直接订阅者 `confirm.c:426–446` 此时产生的结束事件被 `cap.c:1022` 丢弃。最小修复应在中央交接边界保留状态发布所需的收尾回执，不能给确认服务另加绕过入口。
- 【实现】`tests/member_quiesce/regression.c:177–181` 要求超时后取消换人并允许原 provider 继续执行；这是实施报告披露的偏离，不等于任务书要求的“超时工作取消、如实回执、再切换”。

内存检查、反向对照及其他问题仍在核对，以下计数不是最终结论。

SEVERE=0 HIGH=0
