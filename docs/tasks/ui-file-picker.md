# ui-file-picker：系统文件选择器（应用框架 F9）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足。报告写到 docs/tasks/ui-file-picker/REPORT.md。

## 要做的

docs/system/app-framework-design.md §9 的 F9，预算 250 行产品代码：`FilePicker`——按类型列出当前成员能读的文件，分页（用 F4），选中后返回文件编号（file-index-1 的稳定编号），不裸传路径；取消就回到原来的页面和位置；AI 快照和动作走同一个能力。

## 从哪里看起

- app-framework-design.md §9 的 F9 一行，以及讲文件选择的章节；画板 `$INK_DESIGN/png/` 里如果有 `FilePicker`，先看图。
- 通用列表 F4（`ink_ui_list`，docs/tasks/ui-list-paging/REPORT.md）；文件编号 `ink_fs_index`、`ink_fs_lookup` 和 `file.index` 能力（docs/tasks/file-index-1/REPORT.md）。
- 选文件是一个系统浮层，见 docs/system/ui-dependencies.md 的“系统浮层”一行。

## 边界

- 只做选择器和一个测试里的调用方；不做文件应用（files-app）。
- 代码放在自己的文件里；共用的列表（CMake、能力注册）只在末尾加行。同时在做的还有 contacts、clock-alarm。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 选图片：只列图片，可分页，选中返回编号，调用方用 `ink_fs_lookup` 能找到（测试）。
2. 取消：回到原来的页面和位置（测试）。
3. 孩子成员看不到别人的私有目录；去掉检查，测试失败（反例）。
4. 全量测试和内存检查通过；产品代码不超过 250 行。
