# ui-list-paging：通用列表、网格和分页（应用框架 F4）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足。报告写到 docs/tasks/ui-list-paging/REPORT.md。

## 要做的

docs/system/app-framework-design.md §9 的 F4，也就是 backlog 里 app-framework-2 的“通用列表和分页组件”。预算 300 行产品代码：可见项复用；稳定的对象编号；末页和空页；返回时回到原来选中的那一项；AI 快照里有分页和截断说明。建在已合入的 F1（控件语义合同，ui-semantics）和 file-index-1（稳定文件编号）上。

## 从哪里看起

- app-framework-design.md §9 的 F4 一行，和讲列表、分页的章节；F1 的接口和 docs/tasks/ui-semantics/REPORT.md。
- 现有分页：系统壳里已有的分页（在 system/ui/ 下搜 page、paging）；墨水屏的翻页规则看 docs/product/system-ui-spec.md。
- 选一个现有的列表页面换成新组件做示范，只换一个。

## 边界

- 翻页，不做惯性滚动（墨水屏）。LVGL 只在 UI 任务里用；不新建常驻任务。
- 只做组件和一个示范页面，其它页面的迁移写进报告，不做。

## 验收

1. 1000 项的列表，内存里只有当前一页的对象（测试给出对象数）。
2. 末页、空页、只有一页时显示都对（快照测试）。
3. 进入一项再返回，回到原来那一页、原来选中的那一项（测试）。
4. AI 快照里有“第几页、共几页”和截断说明；AI 翻页和按键翻页走同一个能力（测试）。
5. 全量测试和内存检查通过；产品代码不超过 300 行。
