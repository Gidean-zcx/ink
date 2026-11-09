# update-pages 执行报告

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。分支：`claude/codex-update-pages`。

## 第 1 轮复查修复（2026-11-09）

接续 `4733a9b1`，保留上一轮实现与验证记录。本轮处理高优先级 UP-R1-01 和小工作量中级 UP-R1-02：仅改关于页的一处入口绑定与失败页的重试选择，不扩展服务事件或重试机制。当前用户要求修复全部高优先级问题，因此覆盖原任务“共用文件只在末尾加行”对这一处绑定的限制。

【实测】两项已修，定向验证通过。普通及 ASan/UBSan 全量正在收尾，以下暂保留上一轮全量结果。

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| UP-R1-01（高） | 【实测】已修 | 真实服务先发现新版本，再点击关于页“系统更新”，返回后再次点击；26 条断言通过。快照与按钮均为 `shell.update`。只换回旧入口绑定时有 6 条断言失败，并执行到结束 | T、R |
| UP-R1-02（中） | 【实测】已修 | 断网、无升级包时，失败页文案和快照提供“重新检查”；点击后 `action.user.update.check` 增加 1；18 条断言通过。只换回旧失败页时有 4 条断言失败，并执行到结束。已有包的坏签名重新下载仍通过 | T、R |

【实测】本轮普通构建退出 0；14 阶段共 262 条断言、0 失败，内部 RAM 最少 37968 字节。日志：`build/update-pages-r1-build.log`、`build/update-pages-r1-tests.log`。反向对照脚本退出 0，三个对照子测试均退出 1：原卡片 6 条失败、旧入口 6 条失败、旧失败页 4 条失败；源码未被替换，证据：`build/update-pages-reverse/summary.json`、`build/update-pages-r1-reverse.log`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 已发现版本后从关于页触摸进入、返回重进，再验证 `flow`、`done` 的真实模拟下载、人工确认、写槽、重启、自检和完成页 | T |
| 2 | 【实测】通过本任务场景 | 23% 阻止，充电恢复；注入已发布的剩余空间不足；无包时检查失败重试、有包时坏签名重试；新版本自检失败后回滚 | T |
| 3 | 【实测】通过 | 七页快照含版本、说明和能力；AI 无安装能力；14 阶段共 262 条断言，内部 RAM 最少 37968 字节 | T |
| 4 | 【实测】部分，待本轮全量 | 上一轮普通、ASan/UBSan 全量均 232/233，无跳过；唯一失败为禁止改动的共享能力文档。产品代码保守计 224/300 行 | B、N、A |
| G2 | 【实测】通过 | 临时生成目录与运行时一致：264 项能力、16 项设置；共 326 项源码定义 | C |
| G4 | 【实测】超过目标 | 先看七张画板；七页对照为 20.421～34.514/255，未达到 12/255 | V |

## 没做到的

- `capability-table-doc` 未通过：任务规则第 7 节禁止修改生成的共享能力文档。临时生成表与运行时一致，合并主会话需统一生成文档。
- 七页画板对照均超过 12/255，分数与原因见下节。
- 上一轮普通全量退出 8，232/233、无跳过，432.12 秒；ASan/UBSan 全量退出 8，232/233、无跳过，491.57 秒。日志分别为 `build/update-pages-full-final.log`、`build-asan/update-pages-full-final.log`；两轮唯一失败均为 `capability-table-doc`。本轮结果待补。
- 空间不足测试仍是注入已发布的服务状态，未填满存储卡。检查失败重试验证了离线再次调用真实 `update.check`，未验证联网恢复后与自动检查的先后次序。

## 和设计不一致的

【实现】服务没有更新说明和“今晚再装”接口：说明如实显示暂不能查看，按钮为“以后再装”，只返回、不调度。设计的安装前密码步骤未做，现有服务只要求人工确认。服务没有发布重启后的自检阶段，安装页第四步未点亮；完成页由实际自检结果事件触发。

【实测】上一轮对照分数如下。共性差异：画板仍是旧字号和尺寸、浅灰底；实现使用当前 1.2 倍主题和 `#888888` 静态灰。进度、版本和阻止原因取实际数据，未复制设计示例；缺更新说明的卡片内容与设计不同，准备页未放无数据的更新说明卡；安装步骤的位置、圆圈和页脚有差异。本轮只改入口与重试文案，未改布局；已查看断网失败页实际 `final.png`，“重新检查”与说明未重叠、未截断。

| 页面 | 1/4 灰度平均差（/255） |
| --- | ---: |
| Update | 34.514 |
| UpdateReady | 29.362 |
| UpdateBlocked | 24.358 |
| UpdateFail | 24.804 |
| UpdateInstall | 20.421 |
| UpdateDone | 24.476 |
| BootRecovery | 24.983 |

## 改过的已有测试

1. `tests/integration/update.c`：旧安装文案改为 `UpdateInstall` 和“正在安装，不要关机”；保留无按钮断言，新增 AI 动作为空断言。
2. `tests/integration/ai_surface.c`：严格白名单追加 `shell.update`、`shell.update_close`，仍要求每个注册能力恰好出现在一张表中。未删除测试、跳过测试、放宽预算或断言。
3. 本轮 `tests/update_pages.c`、`.py`：触摸查找同时覆盖活动屏与浮层；新增 `entry`、`check-retry`，共 44 条断言，未修改旧场景期望。离线场景使用独立脚本，包为工具生成的合成包。
4. 本轮 `tests/update_pages_reverse.py`：保留原卡片反向对照，再分别链接复查版本的入口文件、升级页面文件；要求各自指定的回归断言失败且测试执行到结束，不修改产品源码。

## 要用户定的

暂无。

## 顺手发现

暂无新增。上一轮关于页入口的问题已按 UP-R1-01 修复。

## 新增的机制

常驻任务、服务状态机、持久文件格式、配置项：均未新增。

本轮复查修复未新增能力或服务接口。入口、按钮和 AI 快照继续走同一条能力总线；页面只读取现有版本信息，未改服务校验、签名或回滚逻辑。

- `shell.update`、`shell.update_close`：任务 G2 要求的页面导航能力。
- `ink_update_page_info`：G5 允许的小型只读接口，读取目标版本、包大小和后台下载任务编号；服务只在文件末尾追加 12 行。

## 要改的文档

任务规则禁止改共享文档；主会话需更新能力表中的 `shell.update`、`shell.update_close`，以及 `ui-dependencies.md` 的升级页面状态。

## 为什么不能更小

七页共用一个呈现函数、现有主题、浮层和控件语义；删除旧安装占位浮层。更小的仅更换安装卡方案无法覆盖阻止、失败重试和重启结果。服务只追加读取版本、包大小和下载任务编号的接口，未改校验、签名或回滚逻辑。

【实现】本轮产品代码只新增或替换 5 行：关于页绑定 1 行，页面选择及文案 4 行。相对任务基线新增产品 C 218 行（页面 202、启动桥 3、服务接口 12、入口绑定 1），另加 CMake 6 行；保守计 224/300 行，删除旧卡片 60 行不抵扣预算。

## 复现命令

以下命令均从仓库根目录执行，依赖路径使用用户给定环境变量。

```sh
# B：普通构建，全部依赖沿用指定目录；中枢使用现成锁定环境。
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
taskpolicy -c utility cmake --build build -j2
# N：全量。A 使用相同配置，目录换成 build-asan 并加 -DINK_SANITIZE=ON，再构建、跑全量。
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# T：14 阶段，保留本地证据。R：原卡片及两项复查问题的反向对照。
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/update_pages.py --build build
# R
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/update_pages_reverse.py --build build
# V：已验证的七张实际面板截图。
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/test-out/update-pages --reuse --scenes Update UpdateReady UpdateBlocked UpdateFail UpdateInstall UpdateDone BootRecovery
# C：只向忽略的工作目录生成能力表，并校验真实运行时清单。
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/update-pages/capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
```

## 范围与收尾检查

【实测】仅 macOS 模拟器；升级签名使用既有模拟替身。未连接设备、打开窗口版模拟器、调用付费接口、推送或开 PR；Linux 未实测。

【实测】提交差异扫描无本机路径或硬件标识匹配（扫描退出 1 表示无匹配）；`git diff --check` 退出 0。差异只含本任务产品、测试和报告，未改共享文档。
