# backup-restore 修复报告

2026-11-06。从复查基线 `cf258b3b` 接续，修复提交 `99896dff`；原任务基线为 `a51a804c`。[复查](REVIEW.md)列出的 1 条严重、2 条高已修复，没有另列中级问题。证据范围是 macOS/clang、模拟器、合成成员和模拟中枢；未连接设备或真实服务器。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| BR-R2 严重：成员 ID 复用 | 已修复【实现、实测】 | 成员表持久保存独立身份，创建序号在同一成员表生命周期内防止模拟 RNG 跨重启重复；请求、清单、回执、日志及中枢分区均校验。删除成人后复用 ID 创建孩子，在有本地资料和清空资料两种环境下，均拒绝旧备份预览、旧预览恢复及成人资料读取；新身份自己的备份可跨重启恢复 | `run.py` 的 reused-id、identity-reboot、malicious、wrong-receipt；`review_reverse.py` |
| BR-R1 高：递归目录栈超额 | 已修复【实现、实测】 | 目录栈、路径、条目和复制状态在 PSRAM，迭代处理目录；八层目录通过，后台栈峰值估计 4,752 B，小于原 8,192 B。宿主 `-fstack-usage`：capture_step 320 B、hash_file 112 B，无随深度增长的递归帧 | `run.py` 的 deep；`review_reverse.py`；`build/repair-stack/` |
| BR-R3 高：大文件阻塞后台能力 | 已修复【实现、实测】 | 先落盘采集意图并返回 queued；每步处理一个集合、Flash 状态或目录项，个人文件采集及卡文件哈希按 16 KiB 续接。预览、确认后校验、下载最终校验和安装前哈希也拆步；条件安装在修改锁内检查版本，无整文件锁内哈希。2/8 MiB 备份和实际覆盖恢复期间，进度查询及无密码成员解锁工作均及时完成，timeouts/late 均为 0 | `run.py` 的 large-seed、large-restore；`review_reverse.py` |
| B1 清单与上传回执 | 通过【实测】 | 集合、阅读位置、个人文件、空文件及重启回执；received 与 durable 分开；错误身份回执拒绝；旧无身份日志明确要求重新备份，并允许新备份 | `run.py` 的 seed、receipt-reboot、wrong-receipt、legacy-journal |
| B2 预览、覆盖确认、未恢复项目 | 通过【实测】 | 人工确认、家长 PIN、错误 PIN、换人及过期内容校验保留；坏哈希、错误身份、旧格式、越界路径和重复条目拒绝；未安装集合有说明 | `run.py` 的 pin、stale、member-confirm、corrupt、malicious |
| B3 备份→清空→恢复 | 通过，限重新绑定后的成员资料【实测】 | 真正清空并重启；先验证新身份不能获取旧备份，再用独立合成身份夹具模拟明确重新绑定；两成员的编号、版本、删除、下个编号、文件和位置复原，再次重启后阅读器回第 4 页 | `run.py` 的 wipe、rejoin、restore-cut/resume、verify-reboot |
| B4 断电、断网后继续 | 通过【实测】 | 采集断电后从原件重建；上传/下载按持久偏移继续；采集中换人和后台暂停；未授权预览检查点丢失后从校验过的远端清单重建；六组 Flash 故障均实际撕裂后恢复成功 | `run.py` 的 collect-*、prepare-recover、upload-*、network、restore-network、member-resume、flash-* |
| B5 成员隔离 | 通过【实测】 | 持久身份与运行时代次分别校验；新孩子无旧成人清单、回执或数据；确认期间换人拒绝，回到原成员后继续；条件恢复仍绑定原成员 | `run.py` 的 reused-id、identity-reboot、member-confirm、member-resume、api |
| G1 全量、内存检查与预算 | 通过【实测】 | 普通 105/105，72.74 s；ASan/UBSan 105/105，99.81 s；均退出 0、无跳过。两套备份专项各 51 次启动、零断言失败、零警告；普通专项内部 RAM 最低余量 41,872 B，后台栈最高 4,768 B，存储栈最高 3,472 B，预算未改 | 下方四条构建/CTest 命令 |
| G2 原子能力与 AI | 通过【实现、实测】 | 原五调用、两事件仍走统一能力、确认和 PIN 门；新增内部接口只供可信服务使用；状态和清单可由 AI 查询，无新页面 | 全量中的 capability-table 系列、integration-ai-surface |
| G3 依赖表 | 保持“部分”【实现】 | 本轮未改依赖判断；身份重绑定、真实对象存储等仍有缺口 | `git diff a51a804c..HEAD -- docs/system/ui-dependencies.md` |
| G4 页面 | 不适用【实现】 | 没有界面或动画改动；GUI smoke 使用既有 SDL dummy 模式，无窗口 | 全量中的 gui-smoke |
| G5 范围 | 通过【实现】 | 仅服务、成员策略、必要接口、模拟中枢、测试及本报告；未开发应用或改需求/决策/调研资料 | `git diff --stat cf258b3b..HEAD` |
| V1 原六组反向对照 | 通过【实测】 | 版本、日志代、Flash 条件写、卡文件条件写、目录成员绑定及空闲任务预算仍能检出撤掉保护；旧日志清理不提交的保留保护对照通过 | `python tests/backup_restore/reverse.py --build build` |
| V2 本轮反向对照 | 通过【实测】 | 九个负向场景（八个源码变体）均被断言或栈警告检出，见下表；隔离编译，正式源码和归档不变 | `python tests/backup_restore/review_reverse.py --build build --evidence build/backup-restore-phj0ft3x` |
| P1 差异、文件、链接、隐私 | 通过【实测】 | 无夹带、无大于 5 MiB 的提交文件；文件/链接及 diff 检查通过；按任务规定扫描原任务基线和本轮基线到最终 HEAD，均 0 匹配（grep 退出 1） | `git diff --check a51a804c..HEAD`；任务规则的隐私扫描命令 |

两套按[总规则 §5](../codex-rules.md)配置；`INK_WERROR=ON`，内存检查仅另开 `INK_SANITIZE=ON`，保留既有 `INK_HUB_PYTHON=$PWD/hub/.venv/bin/python`。依赖从 `$INK_DEPS` 只读复用，未升级。所有构建和全量测试串行安排：

```sh
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
```

专项入口：[run.py](../../../tests/backup_restore/run.py)、[原反向对照](../../../tests/backup_restore/reverse.py)、[本轮反向对照](../../../tests/backup_restore/review_reverse.py)。其中 `python` 使用 `$INK_DEPS/build-deps/pyenv/bin/python`。普通与内存检查证据分别为 `build/backup-restore-phj0ft3x/`、`build-asan/backup-restore-gdrx7wwz/`；全量日志在对应目录的 `repair-ctest.log` 与 `Testing/Temporary/LastTest.log`。

本轮反向对照证据：`build/backup-review-reverse-wtn7w3iv/`。编译、运行日志与断言原文均留在对应子目录。

| 撤掉的保护 | 实际失败 |
| --- | --- |
| 将身份重新绑定到可复用 ID | reused-id 4 条断言失败，包含新孩子可取旧成人预览 |
| 身份不落盘 | 重启回执 2 条断言失败 |
| 清单不检查身份 | 错误身份清单未在预览前拒绝，1 条失败 |
| 回执不检查身份 | 错误身份回执 2 条断言失败 |
| 迭代遍历改回原递归 | 八层业务断言完成，但后台栈估计 14,864 B，触发 8,192 B 栈警告 |
| 采集改回整文件复制/复核 | 8 MiB 备份 6 条失败，3 次能力超时 |
| 文件哈希改回同步整文件计算 | 8 MiB 恢复 6 条失败，3 次能力超时 |
| 条件安装忽略文件修改版本 | api 3 条失败，包含较新同大小文件被覆盖 |
| 未授权预览不恢复丢失检查点 | prepare-recover 1 条失败 |

原反向对照证据：`build/backup-reverse-kr7i7p2x/`。六组仍得到：版本保护 2 条、日志代 1 条、Flash 条件写 2 条、卡文件条件写 2 条、目录成员绑定 2 条失败；空闲任务变体内部 RAM 最低 32,408 B，低于原 32,768 B 预算。正式全量仍满足原预算。

## 没做到的

- Linux/gcc 和 S31 实机未运行。本环境未找到 RISC-V GCC，无法做目标编译栈验证；上表栈值来自宿主模拟估计及 macOS/clang 的 `-fstack-usage`。新增代码的 read/write/fgets 类返回值逐处核对，没有用 `(void)` 忽略。
- 真实中枢的身份绑定、重新入家庭、跨设备身份恢复及旧无身份快照迁移未实现。恢复测试的 rejoin 直接导入单独保留的合成成员表，是测试夹具；没有给 AI 或用户开放绕过身份授权的导入能力。清空设备后不能仅凭同名 ID 认领旧备份。
- 原任务未覆盖的设备设置、成员身份/PIN、网络/配对凭据备份恢复、真实对象存储、历史版本选择与跨设备合并仍未实现，未取消这些需求。
- 仍需存储卡暂存；上限为 128 项、8 层目录、单个 Flash 文件/集合快照 1 MiB，模拟中枢单对象 64 MiB。超限明确失败；恢复逐项原子提交，完成后需重启加载缓存。采集中断会重采原件，上传/下载复用持久偏移。

## 和设计不一致的

- 延续原任务的不可变整份清单与 `backup/commit`、`backup/latest` 协调；本轮增加持久身份和 v2 清单/日志，所有备份 RPC 与回执都带身份。旧无身份快照不自动迁移，不给可复用 ID 继承授权。真实服务器以后必须从已认证成员绑定验证身份，不能只信请求字段。
- 设备协议 §3.9 的增量推送、对账、dataset/restore_token 换代仍未接入；设备级资料覆盖仍有缺口。未改原设计迁就实现。
- 本机 AI 操作日志保留、未安装集合明确列出、清单外文件保留、外部动作不重放，延续原范围和断言。

## 改过的已有测试

- `tests/backup_restore/device.c`：原固定文件名和 98,304 B 长度参数化，默认夹具和内容断言不变；PIN 完成断言仅增加错误输出。增加持久身份复用、重启、八层目录、2/8 MiB 实际覆盖恢复、进度/解锁并发、采集断电/换人/暂停、错误身份回执、旧日志及预览检查点恢复场景；原断言保留。
- `tests/backup_restore/run.py`：清空与恢复之间新增明确重新绑定夹具，先断言新身份不能取得旧备份；恶意清单偏移按 v2 布局调整，保留原三种攻击，另加错误身份与旧格式。新增场景均校验 test.finished、test.fail 和 warnings；未放宽预算、截止时间或已有通过条件。
- `tests/backup_restore/reverse.py`、预算与既有能力测试本轮均未改。原任务对 `ai_surface.c` 新增五调用/两事件的记录继续有效。

## 要用户定的

本轮没有阻塞项。未来的身份重新绑定授权、其他成员资料迁移和旧无身份快照认领流程仍需产品决定；当前保守拒绝自动归属，保留远端历史对象。

## 顺手发现

没有另修范围外问题，复查未列中级项。未 push、未开 PR、未碰设备、真实凭据或付费服务，未启动带窗口的模拟器。
