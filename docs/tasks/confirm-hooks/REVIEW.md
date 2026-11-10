# confirm-hooks 主会话复查（2026-11-10）

结论：成立，直接合入；合到最新主线后全量 298 项通过，真 gcc 检查无发现。

能力定义（`ink_cap_t`）新增可选钩子 `prepare_confirm(args, rc)`：确认请求进入等待之前调用，能力在这里绑定上下文（加只给自己核对用的私有字段）、补充说明里 `{占位}` 的内容；返回非 DONE 就不进入等待，回执写明原因。`call.dtmf` 的通话代次绑定挪回通信服务（静态函数），`system.factory_reset` 的清卡说明改成说明模板里的 `{_media}`，由它自己的钩子填。`system/policy/confirm.c` 里不再出现具体能力名，也不再引用通信服务的头文件；docs/system/app-framework.md 写了用法。
