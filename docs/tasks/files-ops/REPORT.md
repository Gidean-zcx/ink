# files-ops 执行报告

基线：`1a5a7355a5ac57371fb3e3eaa5af0b6e99994ce7`。工作分支：`claude/codex-files-ops`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 文件夹合法、重名、非法名 | 【实测】通过 | 真实点按、能力回执和文件状态核对 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_ops/run.py --build build` |
| 1.2 新建文本、保存、重开 | 【实测】通过 | 两次读取文件字节核对 | 同上 |
| 1.3 断电草稿 | 【实测】通过 | 独立进程重启恢复未保存的中文正文；反向对照失败 2 项 | 同上；另加 `--reverse draft` |
| 1.4 多选三个文件 | 【实测】通过 | 点按选择/取消、全选、拷贝、移动、确认后删除、部分失败显示文件名 | 同上；另加 `--reverse selection` |
| 1.5 AI 和成员隔离 | 【实测】通过 | AI 调相同能力；孩子看不到成人文件、不能写家庭内容 | 同上；`integration-ai-surface` |
| 2.1 原文件、索引、传输 | 【实测】通过 | `files-app`、`file-index-3`、`index-durable`、`integration-files`、`integration-wireless-transfer`、`transfer-pages` | 任务书全量命令 |
| 2.2 全量 | 【实测】2 项文档检查未通过，其余有通过证据 | 原话与补跑结果见下 | 任务书全量命令 |
| 2.3 内存检查 | 【实测】仅同样 2 项文档检查失败，没有额外内存异常 | ASan/UBSan 全量与最终文件测试均已跑 | 换 `build-asan` 并加 `INK_SANITIZE=ON` |
| 2.4 S31 | 【实测】编译通过，退出 0 | ELF、镜像、size 与栈报告均生成 | `INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2.5 GCC | 【实测】通过 | `gcccheck: 6 compiled, 0 with findings` | 见下 |
| 3 截图 | 【实测】已生成并看图 | 下列截图路径 | 同验收 1 |

## 没做到的

编辑页复用现有长文本输入上限：4095 UTF-8 字节，超出时提示并拒绝编辑。多选针对文件；文件夹仍用于浏览和选择目标。大量、不连续的选择受现有能力参数表容量限制；没有扩容公共参数表。

批量操作没有组合撤销；批量删除确认卡显示编号列表，全选长列表显示目录，未增加逐个文件名预览。全量的两项文档检查未通过，原因及合并时要改的文档见下。

## 和设计不一致的

新命名页、编辑页、列表工具栏按 D-117 自行补齐；删除沿用不可撤销操作的人工确认。未修改设计合同。

## 改过的已有测试

- `tests/integration/ai_surface.c`：增加 7 个新增能力的输入契约，原有能力断言不放宽；`file.write` 仍要求 `path,text`，编辑器另传草稿引用。
- `tests/index_durable/run.py`：历史代码与当前能力适配层混合链接时补入当前的成员原子写入包装函数，旧索引实现、样本数、时延断言和反向断言不变。第二轮全量中的 `index-durable` 已通过。

## 要用户定的

无。

## 顺手发现

无。

## 新增的机制

- 批量调用上下文和 `file.batch_progress` 事件：任务 3、4 要求逐项进度与结果，复用现有文件作业回调和任务展示。
- `files.drafts`：任务 2 的断电恢复要求，在已有成员数据库内存路径和草稿编号，正文仍由现有长文本草稿机制保存。
- 命名、编辑两页：任务 1、2 要求，复用键盘托管输入和长文本页。无新增 OS 线程、配置项或独立文件格式。

## 要改的文档

按总规则 §7 不修改共享的 `docs/system/capabilities.md`。已用 `python3 tools/capabilities.py --output work/files-ops/capabilities.md` 生成 468 项定义，主会话合并时重新生成正式表；因此原来的 `capability-table-doc` 报告过期。运行时目录核对已通过。`docs/system/system-services-design.md` §1.3 还需登记 `file.batch`、`file.delete_many` 和进度事件，不修改合同来迁就实现；对应的文档核对留给合并时同步。

## 验证记录

工作区起始干净，HEAD 与指定基线一致。未访问设备、密钥或项目外资料。

【实测】`taskpolicy -c utility python3 ../../tools/gcccheck.py build system/apps/files/files.c system/services/file/file_caps.c system/services/file/file_service.c system/services/text/text_service.c tests/files_ops/files_ops.c tests/integration/ai_surface.c`：`gcccheck: 6 compiled, 0 with findings`。

【实测】命名、保存与重开、断电恢复、三文件拷贝/移动/删除、部分冲突与成员隔离均使用真实页面和服务；不依赖目录或编号顺序。点按先等待输入驱动确认收到按下，再抬起，避免闪存写入期间丢掉测试点按。

【实测】普通全量原话：`99% tests passed, 2 tests failed out of 318`，`Total Test time (real) = 824.42 sec`。失败为 `capability-table-doc`、`services-mid-capability-doc`。默认 Python 缺中枢依赖导致 3 项跳过；改用现有的 `$INK_DEPS/hub/.venv/bin/python` 后，连同 fixture 和文件操作共补跑 5 项，`100% tests passed out of 5`（33.10 秒）。未安装或升级依赖。

【实测】内存检查：`INK_SANITIZE=ON`（ASan + UBSan）、`INK_WERROR=ON`，全量原话同为 `99% tests passed, 2 tests failed out of 318`，732.69 秒；无跳过，失败仍仅上述文档同步检查。最终源码的 `files-ops`、`files-app` 又分别在普通构建和内存构建补跑：均 `100% tests passed out of 2`，分别 15.10 秒、19.67 秒。

【实测】最终文件操作测试两进程各 123、245 个检查，合计 368，普通构建证据 `build/files-ops-gihtbdit/summary.json`，内存构建证据 `build-asan/files-ops-na0e9ulu/summary.json`。

【实测】S31 构建脚本退出 0，`ink-s31.elf`、应用镜像、`build-s31/size.txt`、`size-components.txt`、`stack.txt` 均生成；`Total image size: 1351308 bytes`。这是编译和链接证据，没有实机运行结论。

【实测】反向对照不改产品文件，只编译临时副本：禁用草稿引用恢复、禁用取消选择，均正常跑到结束并各出现恰好 2 个预期失败，退出码 1。证据为 `build/files-ops-5bcaj_qr`、`build/files-ops-lidb_f4w`，汇总 `work/files-ops/reverse-summary.json`。复跑命令：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_ops/run.py --build build --reverse draft`（或 `selection`）。

## 截图

- 新建文件夹：`build/files-ops-gihtbdit/write/snap-NewFolder.png`。
- 写文件：`build/files-ops-gihtbdit/write/snap-WriteText.png`。
- 多选：`build/files-ops-gihtbdit/recover/snap-MultiSelect.png`。
- 恢复草稿：`build/files-ops-gihtbdit/recover/snap-RecoveredDraft.png`。
- 部分失败：`build/files-ops-gihtbdit/recover/snap-PartialFailure.png`，失败文件名完整、按钮不遮挡结果。

## 为什么不能更小

需同时补三组页面行为、长文本草稿引用、显式替换写入和批量能力。只在界面直接读写会绕过能力与成员权限；把正文塞进普通参数表会受 416 字节上限影响。采用现有编辑页和草稿引用、现有原子写入、既有文件作业回调，没有另写编辑器或后台调度器。

产品代码 C/C 头文件新增 240 行、删除 25 行；连同 CMake 和翻译表，严格按新增加删除计 297 行（新增 271、删除 26），在 300 行预算内。

## 最后检查

【实测】`git diff --check` 通过；对照基线的差异统计仅包含本任务的 13 个相关文件。截图和证据路径均存在；新翻译键检查通过，`en 108 entries`。重新生成的能力表与运行时核对：`392 capabilities, 27 settings`，468 项定义一致。

【实测】按总规则 §6 扫描基线至本次改动的差异：0 个匹配。未提交构建产物、设计稿或本机路径；改动全部本地提交，未推送。
