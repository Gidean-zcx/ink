# thumb-wiring 执行报告

基线：`d0cacd1d4b348a8e885904026f3796a10996c646`。仅在本地任务分支提交，不推送；依赖目录和设计目录只读。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】人和 AI 按同一编号取到完整像素；改名、移动后仍可取，缓存版本保持 | T1 |
| 2 | 通过 | 【实测】同大小替换、只改修改时间、只改大小均换版本；坏图不返回旧图；单文件和后台目录删除立即清图 | T1、T2 |
| 3 | 通过 | 【实测】连续插入七张图，载荷不超过 65536 B，第五张起淘汰最早一张；原图片缓存测试保持通过 | T1 |
| 4 | 通过 | 【实测】普通与 ASan/UBSan 全量各 218/218，无跳过；能力表与 AI 工具清单检查通过。【实现】产品 C/H 100 行，低于 120 行 | T3、T4 |

## 没做到的

- 验收内没有未完成项。
- 未做 Linux 运行或实机验证；不碰设备。
- 大小和修改时间都保持原样的外部改写不在本任务判定范围，没有增加全文哈希。

## 和设计不一致的

未改变已有设计约束；只在服务设计 §1.3 的表末登记任务要求的新增能力。范围增量：现有 HAL 没有修改时间，为验收 2 在平台接口和模拟端口补了共 19 行元数据查询；服务仍只走 HAL，没有引用模拟器头文件。

## 改过的已有测试

- `tests/integration/ai_surface.c`：AI 可用清单新增 `file.thumb`，必填参数为 `id`；原断言保留。
- `tests/services_finish/run.py`：只在套件列表末尾新增本任务；原期望和预算不变。
- `tests/integration/confirm.c`：能力清单增长后，原来一次导出整表的固定 32 KiB 缓冲溢出，阻塞全量验收。改为遍历已有分页接口，缓冲大小不变；每页都断言 AI 不能看到接受/拒绝操作，遍历后仍要求查询/撤回操作存在，没有放宽期望。

## 要用户定的

暂无。

## 顺手发现

- 首轮定向检查发现服务设计登记用了带参数的代码标记，现有识别脚本只接受能力名；改为单独标记能力名后，8/8 定向测试通过。没有改检测规则。
- 首轮普通和 ASan/UBSan 全量均为 217/218，唯一失败是上述确认卡测试的整表导出假设；两套的本任务 163 条检查、四项图片测试、原文件索引测试均通过。首轮日志保留为 `work/full-build-first.log`、`work/full-build-asan-first.log`。

## 新增的机制

- `file.thumb`：任务要求的新原子能力；人和 AI 共用，复用已有媒体执行器和文件权限检查。
- HAL 修改时间查询：任务要求大小或修改时间改变即失效；现有 HAL 只有大小，补最小元数据接口。模拟器经 `vtk_host_call` 读取文件保存的时间戳，不读取宿主机当前时间。
- 未新增任务、线程、状态机、持久文件格式、配置项、重试、缓存种类或队列。缓存仍是既有四槽 FIFO；缓存变更与文件操作共用既有变更锁。

## 要改的文档

- 合并时生成 `docs/system/capabilities.md`，登记 `file.thumb`；本任务不提交生成表。
- 合并时更新 `docs/system/ui-dependencies.md`、`docs/tasks/backlog.md` 的缩略图接线状态；不修改这些共享文件。

## 里程碑与检查

- 已读取任务总规则和任务书；开工时工作树干净，HEAD 等于指定基线。
- 【实现】C/H 新增及替换共 100 行（含两处执行器与缓存串行约束的注释更新）；即使计入四行测试 CMake 接入也仅 104 行，低于 120 行。文件索引原查询体只提取锁内入口，以便编号查询、解码、缓存复制共用一次变更锁。
- 为什么不能更小：需同时连接稳定编号、权限、修改时间、PSRAM 文件读取、有界缓存、删除清理及能力回执。已排除路径作键（改名失效）、只看大小（同大小替换不失效）、全量生成新格式临时文件（增加持久机制）等更小方案。缩略图返回独立副本，避免删除释放缓存后回执读取悬空指针。

T1：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -R '^(thumb-wiring|image-|file-index-regressions|services-mid-capability-doc|integration-ai-surface)' -j2`。退出 0，8/8；日志 `work/focused-final.log`。本任务 163 条检查，另覆盖儿童访问父母编号被拒、拔卡不返回缓存图。

T2：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/services_finish/thumb_wiring_negative.py --build build`。退出 0；分别撤掉修改时间、大小、删除清理和按编号解析路径后，同一测试四次按预期失败（退出 1），恢复后 163 条检查通过；日志 `work/negative.log`、`work/thumb-negative/`。脚本使用现有测试编译入口，最终逐字节恢复产品源码。

T3、T4 的配置与构建：按总规则 §5 的全部参数配置，保留 `INK_WERROR=ON`；另外设置 `INK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，`FETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src"`，`FETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"`，均复用已有依赖。T4 改用 `build-asan`，只增加 `INK_SANITIZE=ON`。构建均为 `taskpolicy -c utility cmake --build <目录> -j2`，退出 0；日志 `work/configure.log`、`work/build-focused.log`、`work/configure-asan.log`、`work/build-asan.log`。

全量命令：

```sh
taskpolicy -c utility env PYTHONPATH="$PWD/hub/src" PYTHONDONTWRITEBYTECODE=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility env PYTHONPATH="$PWD/hub/src" PYTHONDONTWRITEBYTECODE=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】T3、T4 分别退出 0，均为 218/218、零失败、零跳过；CTest 耗时分别为 123.13 s、319.71 s，日志 `work/full-build.log`、`work/full-build-asan.log`。四项图片测试、原索引测试、两项确定性、分层与 SDK 边界均包含其中。ASan/UBSan 未报内存错误或未定义行为；本任务两套均为 163 条检查，无警告、无分配失败。普通版媒体栈估计 7504 B，小于既有 8192 B；这是宿主保守估计，不是实机量测，未调整预算或 sanitizer 策略。GUI 冒烟沿用既有 SDL dummy 模式，没有打开窗口。

能力表处理：原表未登记本任务新增能力，直接 `tools/capabilities.py --check` 如实失败，日志 `work/catalogue-before-generation.log`。沿用 file-index 两项报告的验证方式：运行前保留原始字节，临时执行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py`，全量结束后在 `finally` 中逐字节恢复；前后 SHA-256 相同，记录在 `work/validation.json`。因此本任务没有提交生成表，合并后须统一重新生成，才能在干净检出直接通过该一致性检查。

确认卡测试的反向证据是首轮两套全量的同一断言失败；改分页后两套单项复测均退出 0，日志 `work/confirm-focused.log`、`work/confirm-asan-focused.log`，上述最终两套全量也均通过。

收尾时只澄清了两处执行器与缓存串行约束的注释，产品行为未再改变；两套再次完整构建，均退出 0，日志 `work/build-final.log`、`work/build-asan-final.log`。随后用 T1 正则额外加入 `integration-confirm`，两套定向复测均为 9/9，退出 0；日志 `work/focused-after-comments.log`、`work/focused-asan-after-comments.log`。编译参数核对包含 `-Werror`，内存检查的系统、端口和解码器均包含 `-fsanitize=address,undefined`。

【实测】`git diff --check <基线>`、改动文件存在性和 Markdown 相对链接检查均通过；已核对 `git diff --stat`，只有本任务服务/接口、必要的平台查询、测试、能力登记和报告，共 16 个文件，没有夹带构建产物或设计稿。共享文档和归档资料没有最终修改。按总规则指定正则对基线至最终内容扫描，隐私命中 0，`grep` 退出 1。全部实现、测试和本报告只作本地提交，没有推送、开 PR 或访问设备。
