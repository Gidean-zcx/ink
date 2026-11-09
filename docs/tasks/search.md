# search：全局搜索（应用框架 F7）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/search/REPORT.md。

## 要做的

预算 300 行产品代码：`Search`、`SearchEmpty`。按当前成员搜应用、设置、联系人、备忘、文件这几类；结果分页、标出类型；点开一个结果再返回，回到结果原来的位置。依据 docs/system/app-framework-design.md §9 的 F7：游标、查询版本、成员和回到原位置；其它范围留在各应用里。

## 从哪里看起

- 这两页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 现有主页搜索（system/ui/shell/navigation.c 的 `search_entered`）；各应用的数据：联系人 system/apps/contacts/、备忘 system/apps/memo/、文件索引 `ink_fs_index`。
- 列表用 F4（`ink_ui_list`），输入框用 F2。

## 边界

- 只读搜索，不改各应用的数据和存储格式。
- 新代码放新文件（例如 system/ui/search.c）；navigation.c 只改入口的几行。同时在做的有 settings-b、backup-pages、camera-gallery，还有上一批在收尾的六个任务，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 输入关键字，结果按类型列出，可分页；点开再返回，回到原位置（测试）。
2. 换了成员看不到别人的结果；去掉检查，测试失败（反例）。
3. 没有结果时显示 `SearchEmpty`（测试）。
4. AI 能用同一个搜索能力（测试）。
5. 全量测试和内存检查通过；产品代码不超过 300 行。
