# files-app-1：文件应用（files-app 的第一块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/files-app-1/REPORT.md。

## 要做的

预算 350 行产品代码：`Files`（入口：家庭、我的、各成员）、`FileList`（目录列表，分页）、`FileActions`（打开、改名、移动、删除）、`FileMove`（选目标目录）。用文件服务现有的能力（路径、权限、作业）、稳定文件编号（file-index-1）、缩略图（`file.thumb`）和 F4 列表；删除照现有的确认规则。传输和导入的页面是后面的任务。

## 从哪里看起

- 这些页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 文件服务的能力见 docs/system/capabilities.md 里的 `file.*`；系统文件选择器 system/ui/file_picker.c 可以参考列表的写法。

## 边界

- 不改文件服务；缺的写进报告。
- 新代码放 system/apps/files/；共用的文件只在末尾加行。同时在做的有 kid-1、ai-pages-2、confirm-pages、update-pages、calendar，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 浏览、改名、移动、删除（删除要确认）都走能力，重启后结果还在（测试）。
2. 图片显示缩略图（测试）。
3. 孩子成员看不到别人的私有目录；去掉检查，测试失败（反例）。
4. 每页有 AI 快照；按钮和 AI 走同一个能力（测试）。
5. 全量测试和内存检查通过；产品代码不超过 350 行。
