# memo 任务报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。分支：`claude/codex-memo`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】定向通过 | 新建、编辑、删除、撤销、清单触摸勾选与 AI 勾选；另一进程重启核查；1,800 字节正文完整保存 | `tests/memo/run.py --build build --visual` |
| 2 | 【实测】定向通过 | 注入数据服务拒绝写入，进入真实失败页，正文和长文草稿保留；提示位于编辑区和键盘上方；触摸重试及 AI 保存都落盘 | 同上 |
| 3 | 【实测】通过；反例有效 | 成人／孩子各自存储；故意把两处成员集合打开固定到成人后，空列表及禁止删除断言失败，执行完成标记仍出现 | `tests/memo/run.py --build build --mutant member` |
| 4 | 【实测／实现】定向通过 | 六页 JSON 快照均放入实际 3,072 字节 AI 上下文；触摸与 AI 调用同一组 `memo.*` 能力；实际能力目录另待全量核对 | 定向命令、全量能力检查 |
| 5 | 待验证 | 能力注册、全量、内存与 350 行预算 | 待补 |

## 没做到的

- 全量与 ASan／UBSan 正在执行，尚不声明通过。
- 没有实机或 Linux／gcc 运行证据；不做录音页、离线待整理页。
- 上限沿用长文编辑的 4,095 字节；清单最多 32 行，最多 200 条记录（含可撤销删除的数据）。没有新增清空历史、独立清单项删除按钮或跨应用提醒／短信建议。

## 和设计不一致的

- 已查看六张原画板和实际模拟器截图。使用现有主题、F4 与长文键盘，未修改设计。画面灰度差均超过 12/255，最终分数在收尾时更新。
- 列表使用统一分页卡片；详情显示真实文字及编辑／删除／转清单。画板里的录音播放器和 AI 提取事项没有实现，本任务没有录音数据或相应建议服务。
- `MemoEdit` 按规范没有保存按钮；保留长文编辑器及其固定键盘。`MemoSaveFail` 保留编辑器并提供明确重试，未接入“腾出空间”的文件页面。

## 改过的已有测试

- `tests/integration/ai_surface.c` 只在 `FOR_AI` 表末追加十项能力及必填参数，未删除或放宽原有断言。

## 要用户定的

无。

## 顺手发现

无。

## 新增的机制

- 任务要求的成员备忘集合 `memo.notes`，JSON schema 1：正文、清单标志、勾选位、可撤销删除标志和已有草稿引用。没有新建持久文件格式。
- 任务要求的删除撤销：同一记录保留内容并标记删除，撤销恢复原编号；复用数据服务落盘。
- 无新增产品任务、线程、状态机、队列、后台重试、配置项或通用框架。

## 要改的文档

- 合并主会话重新生成 `docs/system/capabilities.md`，纳入十项 `memo.*` 能力；本任务只生成忽略目录中的预览。
- 由合并主会话更新 `docs/system/ui-dependencies.md` 和 `docs/tasks/backlog.md` 的备忘状态；按总规则第 7 节，本任务不改共享文档。

## 里程碑与规模

- 开工时工作区干净，HEAD 与给定基线一致；已先提交报告。
- 【实现】新增页面和能力均在 `system/apps/memo/`；共用 CMake、启动函数、文本头文件与文本服务只在末尾追加接线或接口。唯一服务增量 `ink_text_draft_read` 按成员和页面读取已有草稿，后台无需访问 LVGL，也无需把长正文塞进 384 字节能力参数表。
- 超过 100 行的原因：六页、十个原子能力、成员数据读写、草稿接线和快照均为任务要求。看过的更小方案：仅保存编辑框内容无法提供可靠失败回执；只传正文会撞上能力参数 384 字节限制；应用复制长文草稿格式会形成第二份合同。最终复用现有数据服务、长文服务、F1／F4 和两个既有执行器。
- 【实测】定向最新一轮写入 137、重启 22、失败处理 30 项断言通过，零告警、零预算违规；内部 RAM 总余量最低 47,840 字节，原门槛 32,768 字节未改。证据：`build/memo-ne5oew_o/`。

## 验证命令

命令均在仓库根目录执行，设置用户给定的 `INK_DEPS`、`INK_DESIGN`。构建与全量测试串行，构建及 CTest 均不超过 `-j2`，均以 `taskpolicy -c utility` 启动；不启动窗口版。

```sh
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/memo/run.py --build build --visual
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/memo-compare --scenes Memo MemoDetail MemoEdit MemoChecklist MemoUndo MemoSaveFail --reuse
```

配置按总规则第 5 节，启用 `INK_WERROR=ON`，字体、Python 与 LVGL 来自只读依赖目录。另指定现有 SDL2／ImGui 源码镜像及依赖齐全的 `INK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`，没有安装或升级依赖。

## 反向对照

- `member`：两处数据库打开故意固定到成人集合；失败 2 项，均是实际跨成员读取／删除，`test.finished=1`。最初尝试把集合改为设备级，会先被缓存接口拒绝，无法抵达成员场景；该尝试不作为隔离证据。
- `failure`：仅撤掉保存失败后进入失败页的分支；失败 5 项，覆盖真实页面、提示位置、可见重试及原内容保存，`test.finished=1`。
- `write`：待最终执行。
- 反例只在忽略的构建目录生成临时源码、对象及可执行文件，不改工作区产品代码；未删除、放宽断言或加 sleep 掩盖问题。
