# ui-fix-c：AI 面板、AI 结果记录、相册三处页面照审计整改

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md` 第 11 条（只在明显冲突处改界面，用我们的主题令牌和统一组件）。

用户 2026-11-11：“我要求每个功能和页面都参考现代UI的设计审计 我非常不满意 这些都要重新搞”。审计结果在 `docs/tasks/ui-audit-2/`；本任务做其中 `TASKS.md` 的 **UX13、UX14、UX19** 三项（文件互不重叠），照各行的“具体交付和验收”和对应问题编号（A01～A07、A08～A10/A14/A15、G01～G05，详见 `PAGES-SYSTEM.md`、`PAGES-APPS.md`、`COMPONENTS.md`）。

## 要做的

1. **UX13 AI 面板可读性**（`system/ui/shell/ai_panel.c`）：首屏提示答案被截断（还有下一页），朗读和停止的文字准确，120% 字号仍保留页码和关闭；从书页叫出 AI 后回去仍在原位置。不改模型和声音服务，不新增长答案存储。
2. **UX14 AI 结果记录**（`system/ui/shell/ai_pages.c`）：每行写清对象名、结果、时间；过期或不能撤销的写原因；额度文字不写没验证过的恢复时间。拿不到真实对象名就如实标出，不改能力表来掩盖。
3. **UX19 相册浏览**（`system/apps/gallery/gallery.c`）：缩略图能看清（用现有图像缩略接口）；空态和手动重新读取；当前照片是哪张清楚；删除和“问 AI 这张图”的层级分开。不加云相册；需要新的导航能力时先写接口提案，不冒充已有。

## 范围和预算

- 只改上面三个文件、`tests/camera_gallery/camera_gallery.c` 和本任务专用的测试、截图基线、`docs/tasks/ui-fix-c/`。不改共享主题、`navigation.c`、`ai_chat.c`、`textfield.c`、`keyboard.c`、相机文件（`camera-fps-1` 在改 `camera.c`）、公共测试文件。
- 产品代码合计不超过 220 行；不新增机制、状态机、文件格式。

## 验收

1. 每项附常规和 120% 字号的前后截图（`docs/tasks/ui-fix-c/`，单张不超过 300 KB）；每项有针对性测试和撤掉修复会失败的反向对照。
2. 主工程普通全量 ctest 过（含 `ui-scale`）。
3. 报告逐项写对应的审计编号、改了什么、没改的低优先项。
