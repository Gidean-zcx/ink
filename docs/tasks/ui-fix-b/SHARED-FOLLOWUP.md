# 合并时的共享文件收尾

本任务范围禁止修改共享能力文档，且没有授权修改共享翻译表。补丁只供主会话合并时处理，本任务未将它应用到工作区。

- 能力文档增加设置页读取 `backup.auto` 的依赖；以主会话最后运行 `python3 tools/capabilities.py` 为准。
- 给新增的 10 条中文补英文词条，并将产品代码中的对应字面量标记为 `TR`／`TR_NOOP`。补丁同时包含词条草案和产品代码标记，应用后需重建字库及翻译表。
- 公共 `small-gaps-2-picker` 仍须只数宽度为 `INK_PRIMARY` 的加减键；新增的五个宽输入入口走同一能力，不能一起数成加减键。补丁保留原有 10 键数量、图标和双方几何断言，未将上限改成 15。
- 收尾前 `capability-table-doc`、`i18n-literals`、`small-gaps-2-picker` 仍会失败，不能称全量通过。

详见 [补丁](SHARED-FOLLOWUP.patch)。
