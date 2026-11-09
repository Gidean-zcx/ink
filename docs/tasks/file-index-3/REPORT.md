# file-index-3 执行报告

基线：`2228df7de99de9fea15fddc22868a0de1caae770`。验收 1～4 通过，验收 5 未通过：普通全量 255/258，ASan/UBSan 全量 253/258。五项时序回归均有基线对照；未达到全部验收条件。仅本地分支提交。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 不调用 file.index 自动索引共享及全部成员；实际存储页从“正在整理”变成数值；个人 17 字节、全机 1031 字节 | T1 |
| 2 | 【实测】通过 | 每批最多 32 项；1000 文件专项后台持锁探针最大 12.147 ms，含用户操作的全部编辑段最大 155.087 ms；写入最长 66.646 ms，结束时索引尚未建完 | T1 |
| 3 | 【实测】通过 | 1000 个共享文件：原算法写盘 1008 次，新后台 41 次，减少 24.6 倍；含索引及完成记录的日志、快照；测试上限 100 次；数据库迭代 500501→2038 | T1、T2 |
| 4 | 【实测】通过 | 真正重启进程后目录读取、索引写入均为 0，编号不变；拔卡后无进度，插回续建保留编号；换卡拒绝旧编号，两张完成卡往返均不补扫 | T1 |
| 5 | 【实测】未通过；新增行数在预算内 | 普通全量失败 3 项，ASan/UBSan 失败 5 项，无跳过、无 sanitizer 错误；产品 C 新增 213 行、删除 43 行，测试 CMake 另 4 行 | T3、T4、P |

## 没做到的

- 两套均失败：`confirm-durable`，token 5 预期 `member_changed`，实际 `unknown`。
- 两套均失败：`hub-simulator-e2e`，本地假中枢 `tool_calls=0`，预期 1。
- 两套均失败：`integration-races`，66 秒附近等待 BG 忙碌信号的断言失败。
- 仅 ASan/UBSan 失败：`scenario-reader_basic-metalio-plus-pqbn`，首屏反馈 p95 为 468.057 ms，超过原预算 460 ms。
- 仅 ASan/UBSan 失败：`scenario-ai_page_version-metalio-plus`，完成回执 5 次超过上限 4，缺少预期的 stale 回执。
- 【实测】这五项在对应构建的基线文件服务下均通过，属于本轮引入的回归。经初版分批、实时会话暂停、已完成索引轮询减负三轮后，同类时序问题仍未消除；依总规则第 7 节停止追加修补，未改这些测试或预算。确切根因尚未完全定位。
- Linux/gcc、实机未执行。卡标识匹配但完成记录的版本失配，只有代码检查，未单独做版本变更测试。

## 和设计不一致的

- 原有 512 条上限装不下验收要求的 1000 文件，接口容量改为 1024；文件记录及数据库格式不变，原超限和释放一个位置后恢复的断言保留。
- 【实现】旧固件没有持久完成记录，升级后需要补扫一次，并按已有路径复用编号；之后的重启零扫描已测试，旧固件升级场景未单独实测。

## 改过的已有测试

- `file_index.c`：容量夹具扣除后台已索引的条目，仍检查全机超限、编号不复用和删除后恢复；报错使用当前容量并输出实际错误码。
- `file_index_boundary.c`：增加读取实际记录数的测试函数。
- `storage_usage.c`：开机后的预期改为自动建好后的数值；新增成员后接受当前后台状态，原“必须等待手动扫描”的前提已改变。原分类、缓存、权限、页面自动更新断言保留；新专项严格验证未完成时的“正在整理”。
- 其余已有测试、预算均未改动。

## 要用户定的

- 无新增产品选项；本轮未达到全部验收条件。

## 顺手发现

- 无其他范围外修改；回归已列在“没做到的”。

## 新增的机制

- BG 扫描游标和既有服务计时器上的 1 秒卡检测：任务书要求开机、插卡后台建索引；没有新增线程。
- 扫描期 PSRAM 查重表：任务书要求消除逐文件遍历；扫描结束释放。
- `file.index_ready` 完成记录：任务书要求按卡标识和索引版本复用；按卡记录成员集合、版本，沿用数据库及 `index_rec_t` 格式，没有新文件格式。
- 后台与显式扫描的互斥、游标失效标记：防止两种入口交叉插入同一路径；查重准备、落盘等待在编辑锁外。
- 复用已有实时会话暂停、存储请求检查及同一计时器让出批次；没有新队列、配置项或重试任务。

## 要改的文档

- 主会话更新共享进度、1024 条容量、后台完成记录、organizing 状态值及探针。本任务未改共享文档。

## 命令与证据

- T1：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(file-index-3|file-index-regressions|storage-usage)$'`。三项在两套最终全量中均通过；`build/review-focused.log`、两套 `test-out/file-index-3/summary.json`。两套专项内部总余量最低 43,120 字节，内部、外部堆分配失败均为 0。
- T2：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/file_index_3/run.py --build build --baseline 2228df7de99de9fea15fddc22868a0de1caae770`，退出 0；`build/index-baseline.log`。只替换基线文件服务编译输入，双方统一使用 1024 条容量宏（原 512 条无法容纳该夹具）。旧算法用显式查询启动，新实现自动启动；同样 1000 个共享文件，新实现还扫描两位成员的文件。
- 【实测】反向对照中，基线自动索引验收确实失败，索引条目为 0；旧算法单次持锁 122.472 秒。产品源码没有临时替换或撤改。
- T3：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`：255/258，无跳过，305.36 秒，退出 8；`build/ctest-full.log`。
- T4：同 T3，目录改为 `build-asan`：253/258，无跳过，471.60 秒，退出 8；`build-asan/ctest-full.log`。完整 CTest 日志及 `Testing/Temporary/LastTest.log` 未发现 AddressSanitizer、LeakSanitizer 或 UBSan 错误。
- 【实测】两套 `INK_WERROR=ON` 构建通过，内存构建另加 `INK_SANITIZE=ON`；两套均使用依赖目录现有中枢 Python 环境，没有安装或升级依赖。构建和测试均为 utility，最多 `-j2`，未运行有窗口的模拟器。
- 五项回归的基线复现：`build/confirm-baseline.log`（core 158 条通过）、`build/index-compat-baseline/summary.json`、`build-asan/index-compat-baseline/summary.json`（各自退出均为 0）。仅替换文件服务编译输入，其他库、断言和预算相同；脚本、日志、二进制留在忽略的构建目录。
- P：【实测】基线至提交的补丁按总规则指定隐私正则检查为零匹配，grep 退出 1；`git diff --check` 通过。变更只包含文件服务、容量声明、测试登记、回归测试和本报告，没有构建产物或并行任务文件。

【实现】超过 100 行的原因：后台续步、换卡退出、分批落盘、开机复用均为验收要求。较小的“每批遍历整库”方案仍出现长持锁，因此使用锁外一次读取及扫描期查重；没有搭建通用框架。

架构审视：【实现】文件服务继续经 OSAL/HAL、数据存储及既有 BG 执行器工作；UI/AI 共用 file.index、file.usage、file.index_state 和 file.changed。完成事件按成员发布通用根路径，内部扫描不会把其他成员的私有路径放入事件。
