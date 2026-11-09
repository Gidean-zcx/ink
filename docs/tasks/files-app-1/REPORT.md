# files-app-1 执行报告

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。分支：`claude/codex-files-app-1`。状态：执行完成，有下列未达项。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】触摸改名／移动、人工删除确认、AI 同能力调用、第二进程重启保号和持久性 | T1 |
| 2 | 通过 | 【实测】真实 PNG 经 `file.thumb` 成为列表及操作页 L8 图片，逐像素核对黑色夹具 | T1 |
| 3 | 通过 | 【实测】儿童入口无别人的目录，伪造路径被拒，服务拒绝成人编号；撤掉目录可见性检查时测试失败 | T1、T4 |
| 4 | 通过 | 【实测】四页 F1／F4 快照、生产缓冲区、触摸与 AI 共用文件能力 | T1 |
| 5 | 未全过；行数通过 | 【实测】普通、ASan/UBSan 全量均 232/233，零跳过；唯一失败 `capability-table-doc`。未发现 sanitizer 错误，产品 300/350 行 | T2、T3；下文规模统计 |
| G4 | 像素目标未达 | 【实测】四页分数均超过 12/255，已查看截图并列明差异 | T5 |

## 没做到的

- 未做实机和 Linux/gcc 运行验证；不启动窗口模拟器。
- 文本“打开”是最多 128 字节的开头预览；图片显示现有缩略图，音频交给 `media.play`。现有阅读器只认书架中的 TXT，没有按任意文件编号打开 EPUB/PDF 的接口，本任务不改服务或阅读器，不声明完整格式支持。
- 目录用于浏览和选择移动目标，未提供目录自身的操作菜单。画板中的批量选择、拷贝、新建目录未实现；传输和导入页按任务书留给后续任务。

## 和设计不一致的

- 【实现】`Files` 按任务书为“家庭／我的／家人”，画板为内容分类及传输入口；未写入示例容量或文件数量。`FileActions` 为单文件改名、预览、移动和删除，画板为多选及拷贝／移动／删除。
- 【实现】F4 每页四行，使用当前主题的字号、灰度和尺寸；页码在四行列表下方，画板为五行、页码贴底。`FileMove` 对照截图为真实的空目标目录，没有画板中的示例目录及新建入口；取消和放入按钮已接能力。
- 【实测】四页已查看并产出 `tools/ui_compare.py` 对照，均未达到 12/255：`Files` **57.581**、`FileList` **48.865**、`FileActions` **27.000**、`FileMove` **26.450**。证据：`build/files-app-compare/scores.json` 及同目录拼图；没有修改主题或规范迎合旧图。

## 改过的已有测试

- `tests/integration/apps.c`：应用总数 11 → 12，因新增文件应用；不改变可见性、权限或生命周期断言。
- `tests/integration/ai_surface.c`：在清单末尾登记八项 `files.*` 页面能力及必填参数，保留完整登记要求。

## 要用户定的

- 无；未新增产品取舍或配置决定。

## 顺手发现

- 普通全量唯一失败 `capability-table-doc`：共享生成文档已过期，总规则第 7 节禁止本任务修改。生成至 `build/files-app-capabilities.md` 的预览与运行时核对通过：270 项能力、16 项设置、共 332 项定义。

## 新增的机制

- 产品无新任务、线程、执行器、持久格式、配置项、队列或后台重试。页面瞬时模型及回调归属检查服务于本任务四页，使用现有能力、编号、缩略图和 F4；合成测试任务仅进入测试程序。

## 要改的文档

- 合并主会话重新生成 `docs/system/capabilities.md`，登记八项 `files.*` 页面能力及实际源码行位。
- 合并主会话同步 `docs/system/ui-dependencies.md` 和 `docs/tasks/backlog.md`，说明四页已实现的单文件流程及本报告列明的设计差异、格式限制。共享文档、需求、想法和决策均未修改。

## 里程碑

- 开工：工作区干净，当前分支与给定基线一致；已读取任务书和总规则。
- 实现：四页、应用入口及八项页面能力已接真实服务；目录查询与图片解码留在现有执行器，LVGL 只在 UI 回调。
- 第一轮定向：176 项检查、重启 34 项通过；四个独立截图场景各 177 项通过。期间修复首次解析 `file.lookup` 时把数组当对象造成的崩溃，未放宽预算。
- 最终普通定向：193 项检查、重启 34 项通过；四个截图场景各 194 项通过，均零失败及零预算违规。另补了伪造成人编号后不能复用上次文件状态、真实 AI 面板打开时操作下层文件页的验证。
- AI 面板回归：修复前新增测试准确失败 3 项（`build/files-ai-before.log`）；页面归属判断改用框架已有的 `ink_ui_action_page()`，不改变浮层规则。修复后同一场景通过。
- 本地里程碑提交：`f4799f15` 建立报告，`dde97a1c` 实现与测试，`52c11226` 修复 AI 面板下的动作归属。最终验证和结束检查见下文；未推送或开 PR。

T1：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_app/run.py --build build --visual`。最终证据：`build/files-focused.log`、`build/files-app-gv_8cqus/`。

## 反向对照

【实测】T4：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_app/run.py --build build --mutant <变体>`。七项均编译、链接成功，模拟器及运行器退出 1，`test.finished=1`；没有用编译错误或崩溃冒充反例。替代对象只写入忽略目录，产品源码哈希前后相同。汇总：`build/files-reverses.json`；日志：`build/files-reverse-<变体>.log`。

| 变体 | 撤掉的实现 | 失败断言数 |
| --- | --- | ---: |
| child | 成人才显示其他家人目录的检查 | 1 |
| browse | 伪造目录导航的权限检查 | 1 |
| thumb | 将缩略图像素交给 LVGL | 3 |
| rename | 改名按钮实际调用 `file.move` | 4 |
| paging | 实际执行 F4 翻页 | 14 |
| preview | 将实际文本呈现在可见预览 | 2 |
| ai-panel | AI 面板下使用框架动作页面判断 | 3 |

## 构建与验证

两套构建按总规则 §5 配置，额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`、`-DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src"`、`-DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"`，未下载或升级依赖。均 `INK_WERROR=ON`；内存检查另启用 `INK_SANITIZE=ON`，实际编译参数包含 `-fsanitize=address,undefined`。构建和全量检查串行，均为 `taskpolicy -c utility`、`-j2`；窗口测试使用既有 SDL dummy 模式，没有打开可见窗口。

- 【实测】普通、ASan/UBSan 构建均退出 0：`taskpolicy -c utility cmake --build <目录> -j2`；日志为两套构建目录中的 `files-build.log`。
- 【实测】内存定向通过 193＋34 项：`INK_KEEP_EVIDENCE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_app/run.py --build build-asan`；证据：`build-asan/files-focused.log`、`build-asan/files-app-rmlr_msj/`。
- 【实测】两套文件场景零告警、零预算违规、零分配失败，内部 RAM 最低余量均为写入 47,728、重启 47,840 字节，高于 32,768；没有抬高预算。
- 【实测】T2 普通全量 **232/233、零跳过、155.91 秒、退出 8**；T3 ASan/UBSan 全量 **232/233、零跳过、280.42 秒、退出 8**，均只失败 `capability-table-doc`，无 sanitizer 诊断。日志：`build/files-ctest-final.log`、`build-asan/files-ctest.log`；状态：`build/files-validation-exits.txt`。命令如下，未排除失败项。

```sh
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

T5：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/files-app-compare --scenes Files FileList FileActions FileMove --reuse`。工具退出 0 表示对照成功产出，不能解释为像素目标通过。

T6：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/files-app-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json`。生成预览与真实运行时一致，未改共享文档。

## 规模与架构审视

- 【实现】产品共 300 行：新应用 291 行、启动末尾接线 2 行、CMake 末尾 7 行（连同测试接线也计入），低于 350。共享启动文件只在初始化末尾追加，不修改并行任务文件或文件服务。
- 为什么不能更小：四页要同时处理目录、稳定文件编号、异步能力返回、确认删除、短输入和缩略图。只用 `file.list` 缺编号，只用 `file.index` 缺目录，现有文件选择器只返回所选文件；因此组合两种查询并复用 F4。没有另建文件服务、目录索引或通用框架。
- 【实现】权限、持久化、作业和确认留在文件服务与策略层；页面只调用软总线。八项新增能力只负责页面导航／预览／输入，改名和移动直接调用 `file.move`，删除直接调用 `file.delete`。AI 可读取四页及同一能力；四行显示像素缓冲和列表模型在 PSRAM，无新增常驻任务。

## 结束检查

- 【实测】`git diff --check` 通过；核对基线至提交的 `git diff --stat`，仅九个本任务文件。没有修改文件服务、并行任务产品文件、共享文档、历史调研或审计资料。
- 【实测】按总规则第 6 节原正则扫描基线至 HEAD 的完整差异，命中 0，`grep` 退出 1；最终报告另作同样检查。记录：`build/files-privacy-status.txt`、`build/files-privacy-hits.txt`。
- 仅本地提交产品、测试和中文报告；构建产物、截图、原始设计稿、日志、反例和生成文档预览均未提交。未碰设备、读取硬件目录或调用付费接口。
