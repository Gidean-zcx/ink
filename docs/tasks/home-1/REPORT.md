# home-1 执行报告

基线 `d8808638`；分支 `worktree-agent-aa2382614066831d9`。由 Claude 子代理执行（Codex 额度用完后改用子代理），子代理在写完反向对照脚本、正要运行时因每周额度用完中断；主会话接手：复核并保留它未提交的改动、修正桌面冒烟测试的点击位置、运行反向对照、写本报告。未接设备。

## 验收

| 编号 | 结论 | 证据 |
| --- | --- | --- |
| 1 一页主屏 | 通过【实测】 | 状态栏、一行“日期 · 天气”、今天的卡片（有内容才画）、6 个常用、全部应用；左右滑和翻页键不再翻屏。截图 `Main-1.png`（默认字号）、`Main-3.png`（最大字号）、`Today-*.png` |
| 2 6 个常用按家人 | 通过【实测】 | 默认电话、短信、阅读、问 AI、相机、时钟；长按图标菜单可换（`shell.favorite`）；孩子有自己的 6 个，只出现孩子能用的（`KidHome.png`、`KidApps*.png`） |
| 3 全部应用和查找 | 通过【实测】 | 全部应用分页（`shell.apps`、`shell.apps_next/prev`），查找框列出的和 AI 的 `shell.search` 一致（`Apps-*.png`、`AppsSearch-*.png`） |
| 4 人和 AI 同一入口 | 通过【实测】 | 新测试 `home-one-page` 与场景 `ui-scene-AppsSearch`：触摸和 AI 得到同样的条目和结果 |
| 5 大字列表 | 未做 | 预算内放不下，留待后续 |
| 反向对照 | 通过【实测】 | `tests/home_one_page_reverse.py`（手动）：恢复翻屏、画空卡片、查找结果换位置、孩子共用大人的 6 个，4 项改动各自让对应测试失败；恢复后全部通过 |

主会话在本工作区跑的相关测试：主屏、全部应用、今天、孩子、图标等 26 项全过（含修正后的 `gui-smoke`）。全量 ctest 和 S31 编译在合并门禁里跑。

## 改过的已有测试

- `tests/simulator/gui_smoke.cmake`：第二次点击从书架入口改到新主屏的“阅读”常用（窗口坐标 498,372）；子代理原改的 590,386 点到的是第三个常用“问 AI”。
- 其余随一页主屏调整的测试见提交 `a8cb50da`（翻屏相关的断言改成“不翻屏”，其余期望不变）。

## 规模

产品代码 +496 / −535（`shell_home.c`、`today_edit.c`、`navigation.c` 主屏初始化、`shell_internal.h`、`ui_shell.h`），在 500 行预算内。新能力：`shell.apps`、`shell.apps_next`、`shell.apps_prev`、`shell.favorite`，能力表已重新生成。
