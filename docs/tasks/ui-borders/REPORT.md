# ui-borders 执行报告

基线：`858213ff01e10ddbd362c04ac5adf699eedd1478`。仅本地编译、宿主机测试和截图，不操作设备、不对外发布。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1-宿主机 | 通过【实测】 | `100% tests passed out of 365`；0 跳过，571.03 秒；`work/ui-borders/ctest-final.log` | 下方 N |
| 1-ASan | 通过【实测】 | `100% tests passed out of 365`；0 跳过，1296.42 秒；`work/ui-borders/ctest-asan.log` | 下方 A |
| 1-ui-scale | 通过【实测】 | `ui-scale: 51 files, 0 violations` | `python tools/ui_scale_check.py` |
| 1-S31 | 通过【实测】 | 构建退出 0；IDF 镜像统计 3,268,846 字节；`work/ui-borders/build-s31.log`、`build-s31/size.txt` | 下方 S |
| 1-gcccheck | 通过【实测】 | `gcccheck: 14 compiled, 0 with findings`；`work/ui-borders/gcccheck.log` | 下方 G |
| 1-隐私 | 通过【实测】 | 基线至提交的增量扫描 0 匹配（grep 退出 1）；`git diff --check` 通过，变更清单均属本任务 | 下方 P |
| 2 | 通过【实测】 | 仅恢复主题灰底：退出 1、4 条断言失败、`test.finished=1`；全部 9 组反向对照成立，见 `build/ui-fidelity/recheck-reverse/results.json` | 下方 R |
| 3 | 通过【实测／实现】 | 下方 29 个文件清单；353 张截图、6 组对比；两份规范已更新，文件及链接检查通过 | 下方截图命令及 4 个 `--visual` 命令 |

## 没做到的

- 没有连接设备或验证真屏观感；仅按任务要求做宿主机验证与交叉编译。
- 未运行 Linux 原生环境；用任务指定的 gcccheck 补充检查 GCC 警告。

## 和设计不一致的

- 已检查：静态卡片内的列表行和 Dock 图标共用外框；连续设置行用白色半块接直侧边、内部用黑线；贴底面板沿用原有延伸与裁切。确认卡的内容预览与面板留有独立间隔，没有重叠边线。
- 外部只读设计稿仍为旧灰底；本次按任务修改界面规范，不修改或提交设计导出。

## 改过的已有测试

- `ui_shell_regressions.c`：组外框为白底黑线、嵌套行无重复框，分隔线由白改黑；新增已有外框／黑底卡片的内容坐标断言，保留点击与尺寸检查。
- `ui_shell_scenes.c`：设置页改验黑分隔线，新增静态表面白底／单框巡检；页面、语义和操作断言保留。
- `settings_a.c`：分组预期改为 STATIC 种类、白底和细框，分隔线改黑。失败消息的 8 个坐标显式转为 `int`，适配 RISC-V GCC 的 `int32_t`；不改断言或期望值。
- `factory_pages.c`：按表面种类定位分组、统计黑分隔线；装饰图标和透明开关的断言保留。
- `files_ops.c`：多选改为标题下划线，行始终保留一圈细框，取消选择移除下划线。
- `ui_theme_reverse.py`：更新表面种类判断的定位字符串，增加恢复灰底的反向对照。撤回设置分隔线时保留空操作，避免单行 `if` 意外包含后面的图标语句。

## 要用户定的

- 文件多选暂定用标题下划线表示选中：常驻外框后，旧的有／无边框标记会失效；下划线保留原布局。最终样式交用户复核，本次按可逆方案继续完成。

## 顺手发现

- 【实现】模拟器资源映射上限为 64（`ports/sim/hal/hal_assets_sim.c:13,68`）。同一进程遍历所有页面会触发上限；本任务截图按页启动，不修改资源机制。
- 【实现】阅读书架活动没有快照回调（`apps/reader/reader.c:194`）。只列为发现，不扩大本任务到语义补齐。
- 【实测】最大字号的 SetVoice 开关紧贴分组右边，见 `build/test-out/ui-scene-big-SetVoice/snap-scene.png`；按用户要求保留间距，留待后续讨论。

## 新增的机制

- 表面种类用已有 LVGL 对象 flag 保存，由任务第 2 项要求；产品无新增显式内存分配、任务、缓存、队列、状态机、文件格式或配置项。
- 测试专用 `ui-borders` 任务和 `INK_BORDER_CASE` 选择变量用于任务第 4 项逐页截图，不进入产品。

## 要改的文档

- 本任务直接修改允许范围内的界面规范；不修改共享需求、决策和生成文档。
- 主会话可按新版规范同步外部设计画布与导出；本任务未写入只读目录。

## 执行记录

- 【实测】起始工作区干净，HEAD 与指定基线一致。
- 【实测】六个基线场景均通过并保留原截图；第一轮全量 364 项仅 `ui-scale` 的三处表达式解析失败，等价改写后为 0 违规；3 项中枢测试因默认 Python 缺依赖跳过，最终全量改用已有中枢环境补跑。
- 【实测】39 个应用／设置页面的 `ui-borders` 截图巡检通过；相关设置、主题、文件多选、恢复设置测试通过。截图发现的连续分组断边已修正。
- 【实测】最终产品源码通过正常版和 ASan／UBSan 版各 365 项全量测试。其后只修正测试失败消息的参数类型，两版 `settings-a` 补跑均通过；GCC 修正前后的证据为 `gcccheck-before.log`、`gcccheck.log`。
- 【实测】相机／相册、日历、文件和备忘的 `--visual` 补检全部通过，见 `work/ui-borders/visual.log`。已查看全部收集截图的缩略图及六组前后对比；Control 原图逐像素相同。

## 产品改动与灰底清单

【实现】产品源码新增 48 行、删除 27 行，净增 21 行；加上 CMake 测试注册 4 行，保守计净增 25 行，未超过 200 行。6 处直接设灰色的调用接入同一个主题函数。未修改尺寸、间距、圆角和字号 token；新增边框通过 padding 补偿保留内容坐标，已有边框的短信卡片不再补偿。

【实现】样式只在 UI 层，未修改服务状态或能力路由；按钮和 AI 继续使用原能力。分隔线与后绑定圆按钮均按表面种类判断，不依赖填充颜色。

以下为基线搜索的 29 个文件、59 行（含颜色函数和判断，行号均为基线）。未逐页重写绘制逻辑：

| 原灰底位置（基线行号） | 改法 |
| --- | --- |
| `system/apps/camera/camera.c`：123 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/apps/contacts/contacts.c`：182 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/apps/files/files.c`：171, 318 | 统一表面样式；保留原黑底条件 |
| `system/apps/gallery/gallery.c`：133, 147 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/apps/memo/memo.c`：312, 319, 320 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/apps/phone/phone.c`：338, 396 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/apps/remote/remote.c`：729, 749 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/apps/sms/sms.c`：143 | 统一表面样式；保留原黑底条件 |
| `system/apps/weather/weather.c`：212, 262 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/file_picker.c`：163 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/lock_mark.c`：121 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/search.c`：195, 205 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/backup_pages.c`：92, 103, 130 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/bluetooth_page.c`：98, 112 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/boot_flow.c`：148 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/confirm_card.c`：350 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/factory_pages.c`：88 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/interrupt_card.c`：111 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/kid_pages.c`：56 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/member_gate.c`：94 | 统一表面样式；保留原黑底条件 |
| `system/ui/shell/member_pages.c`：141, 145 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/navigation.c`：392, 526, 675, 793, 805, 978, 1163, 1255, 1512, 2092, 2144 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/power_pages.c`：81 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/settings_family.c`：70, 119 | 统一表面样式；保留原黑底条件 |
| `system/ui/shell/settings_pages.c`：202, 207, 211 | 统一表面样式；保留原黑底条件 |
| `system/ui/shell/transfer_pages.c`：103 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/shell/update_pages.c`：97, 125, 146 | 经主题自动改为白底细边框；嵌套静态子项共用外框 |
| `system/ui/textfield.c`：387 | 统一表面样式；保留原黑底条件 |
| `system/ui/theme.c`：135, 216, 357, 361 | 统一表面样式；保留原黑底条件 |

## 截图与规范

- 图片索引：`build/ui-borders-evidence/index.md`；353 张截图、23 张缩略图，含不同字号、成员、语言及重复场景。来源均有已完成且无失败的测试指标。
- 全部已注册应用 14 个、设置入口 25 个：`build/test-out/ui-borders/00`～`38`；主页三屏、浮层、成员门和孩子页：`build/test-out/ui-scene-*`。其余应用内部流程按索引查找。
- 六组前后对比：`build/ui-borders-evidence/comparisons/{Main,Apps,Control,SetDisplay,Confirm,Members}.png`；原图分别在同目录的 `before/`、`after/`。Control 是原黑白控件的保持对照。
- `system-ui-spec.md` 第 1 节及待验证事项改为白底细框、分组外框与内部分隔、贴边裁切、保留间距；`legibility.md` 保留旧灰度选择的历史并追加本次规则。

## 复核命令

配置沿用任务书字体、Python 和 LVGL 参数；正常版与 ASan 版都加 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，通过 `PYTHONPATH="$PWD/hub/src"` 使用本检出的中枢代码。ASan 版只增加 `-DINK_SANITIZE=ON`，SDL2、ImGui 复用正常构建的相同源码。

```bash
# N：完整宿主机验证
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# A：ASan + UBSan，先按上面的配置说明生成 build-asan
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# S：只编译，不接设备
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" INK_S31_LIGHT_SLEEP=OFF taskpolicy -c utility bash ports/esp32s31/build.sh
# G：全部改过的 C 文件，包含测试
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build $(git diff 858213ff01e10ddbd362c04ac5adf699eedd1478 --name-only -- '*.c')
# R：在构建目录副本上撤回修复，不修改产品文件
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_theme_reverse.py --build build
# 补充应用内部页面截图；每条分别执行 --build build --visual
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/camera_gallery/run.py --build build --visual
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar/run.py --build build --visual
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_app/run.py --build build --visual
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/memo/run.py --build build --visual
# 截图汇集；before 是开工时从基线二进制保留的六张原图
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_borders_evidence.py --build build --before work/ui-borders/before
```

P：执行[总规则第 6 节](../codex-rules.md)的基线至 HEAD 隐私扫描，并检查 `git diff --check` 与相对基线的 `git diff --stat`。日志保留在 `work/ui-borders/`，均未提交。
