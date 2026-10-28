# 第四十七期：AI动作的数据传递与结果

2026-10-27。继续回应“研究是否停留在功能宣传”的问题：**当前已有操作文档、静态画面和少量公开网页实操，但没有逐一完成所有产品、所有功能的交互拆解。** 全部65组仍部分覆盖，原生设备、取消/失败与恢复是主要缺口。后续设计引用须按具体结论选择证据，不能用来源数量或某处有实操给整个产品背书。

本轮沿Apple快捷指令原目录补28篇官方27指南正文，重建6个任务并扩展6个既有任务。没有执行快捷指令、调用示例API或观察设备界面；因此全部保持文档级依据。原95个主题当前49个已读、46个仍目录层；19目录分组不当功能。

| 从功能名继续追问 | 本轮得到的具体区别 | 对本项目的研究价值 |
| --- | --- | --- |
| “支持选择”到底选什么 | 列表选内容对象；菜单选执行路径；字典可显示键而传对应值 | AI和用户需要知道选中的是书、题目，还是下一步操作 |
| “支持流程编排”怎样传结果 | If传所走路径最后产生的输出；循环汇总每轮末尾输出 | 增加中间步骤可能改变结果，运行次数不代表独立资料条数 |
| “可以运行时询问”暂停在哪里 | 到具体参数步骤才暂停，Done后继续，前序动作已运行 | 取消时可能已有部分产物，不能只把整链标成功/失败 |
| “能够操作当前网页”需要什么 | 活动Safari网页上下文、许可、每段脚本输入及完成回调 | 看见页面、取得对象、执行动作、交回结果各需证据 |
| “跨App执行完成”如何辨别 | 成功带文本结果、取消无输出、错误带描述 | AI还需核对业务结果；没有输出不代表没有副作用 |
| “保存了变量”能否下次继续 | 普通链复位；跨运行存储是另有发布声明的动作，完整流程仍未知 | 临时上下文、持久记忆与用户数据备份不能混为一谈 |

上述区别来自[列表操作](https://support.apple.com/guide/shortcuts/use-list-actions-apd4dcacc115/10.0/ios/27)、[条件输出](https://support.apple.com/guide/shortcuts/use-if-actions-apd83dcd1b51/10.0/ios/27)、[运行时参数](https://support.apple.com/guide/shortcuts/use-the-ask-each-time-variable-apd8b28e2166/10.0/ios/27)、[网页脚本](https://support.apple.com/guide/shortcuts/intro-to-the-run-javascript-on-webpage-action-apd218e2187d/10.0/ios/27)及[回调规则](https://support.apple.com/guide/shortcuts/use-x-callback-url-apdcd7f20a6f/10.0/ios/27)；实际取消、超时、重试和重启结果尚未验证。竞品条件也不代表Plus已具备同等实现。

后续使用时，从[按问题查依据](../research/interaction-library/design-reference-guide.md)进入，找到[Shortcuts案例](../research/interaction-library/cases/M-043-shortcuts.md)的任务，再回查[来源摘记与获取审计](../research/interaction-library/artifacts/shortcuts-data-flow/source-notes.md)。新增RP-M043-01～10分别记录前提、动作、文档所述结果、保存情况和未知分支；可按“空输入、选择对象、循环部分失败、跨App取消”等问题检索，不必按时间读完所有简报。

仍保持两项边界：未研究不写成不支持；竞品启发不自动成为我们的需求。没有开始全系统设计。
