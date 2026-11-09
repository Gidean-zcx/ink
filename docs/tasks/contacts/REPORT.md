# contacts 执行报告

基线：`d0cacd1d4b348a8e885904026f3796a10996c646`。分支：`claude/codex-contacts`。

## 第 1 轮复查修复（2026-11-09）

- 接续 `893cddf8`，开工工作区干净；保留原开发与复查证据。本轮只处理 `REVIEW.md` 的 C1-01、C1-02、C1-03，不改共享文档或预算。
- 【实现／实测】编辑保存携带读取版本，旧表单返回 `STALE` 并保留草稿；删除确认绑定版本并显示姓名、号码。复用现有记录版本、条件删除及确认描述，不加通用机制。当前产品累计 263 行。
- 【实测】定向回归通过，原 CRUD／重启断言保留；新增旧表单触摸保存和 AI 重放、原确认后更新、同值重写、描述不符及列表页 AI 删除。最终代码临时反向编译逐项撤掉保存版本校验、删除版本校验、中文描述、内容匹配，各失败 5／7／1／3 项，均完成执行；最大姓名／号码及两种输入焦点的生产上下文也通过。最终证据在忽略目录 `work/checks-6slkbsuo/`，确认卡实际截图已打开核对。
- 【实现】`contacts.delete` 现在必传读取时的 `id、if_revision、name、number`，缺项返回 `INVALID`，不生成仅编号卡片。按钮和页面快照携带原记录的值；页面外 AI 先用 `data.get/list` 读取再提交。确认执行时再次核对当前成员记录的版本、完整姓名和号码，内容不符返回 `STALE`。`contacts.put` 的直接新增／更新仍可用，重放表单则携带读取版本。
- 【实测】最终普通完整构建退出 0，CTest 为 217/218（143.53 秒），唯一失败是禁止本任务修改的共享能力文档。ASan／UBSan 完整构建、全量测试进行中，结果另补。普通首轮曾为 216/218（190.33 秒），另一个失败是按旧必填参数检查的 AI 表面测试，同步精确清单后已通过。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】定向通过 | 真正的两次进程启动；增改删持久化，成人／孩子集合隔离，越界查改反例 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^contacts$' --output-on-failure -j2` |
| 2 | 【实测】功能通过；视觉超标 | 0、1、4、5 条及末页；9 份分页快照＋1 份表单快照；短输入的姓名／号码焦点；视觉 52.004/255 | 同上；`tests/contacts/run.py` 校验 JSON；`tools/ui_compare.py --scenes PhoneContacts --reuse` |
| 3 | 【实测】定向通过 | 实际触摸和 AI 均执行 `contacts.select`，同编号、同号码、来源分别为 touch／ai | 同上 |
| 4 | 【实测】能力、规模通过；普通全量有 1 项共享文档失败，ASan 待完成 | 仍为 8 项新增能力；当前产品 263 行，未超过 300 行。普通全量 217/218；临时能力表与运行时 205 项能力、16 项设置一致 | 下文完整构建／CTest 命令及清单核对命令 |
| C1-01（高） | 【实测】已修复 | 旧表单触摸保存与 AI 重放返回过期；已保存号码不变，草稿保留；返回重开后可正常保存 | 上述联系人 CTest 的 `stale` 场景 |
| C1-02（高） | 【实测】已修复 | 原确认后 AI 更新不再删除；同值重写也按版本拒绝；最新记录重新确认可删除 | 上述联系人 CTest 的 `delete` 场景 |
| C1-03（中） | 【实测】已修复 | 触摸／列表页 AI 删除卡显示姓名、号码和中文说明；不符描述不能执行。实际截图已人工核对 | 同上；忽略目录内的临时截图探针 |

## 本轮反向对照

只替换编译输入，不改产品文件、测试断言或预算。以下均编译／链接退出 0，模拟器退出 1、`test.finished=1`；恢复完整代码的同场景退出 0。使用原构建的 `flags.make` 和 `link.txt`，命令形式为 `taskpolicy -c utility <编译器> <原参数> -x c -c -`，再以 `INK_CONTACTS_PHASE=stale/delete` 运行所得程序；具体产物和日志在上述证据目录。

| 编译输入变化 | 场景 | 失败断言 |
| --- | --- | ---: |
| 完整换回修复前 `893cddf8` 的联系人文件 | stale／delete | 6／17 |
| 仅让 put 略过 `expected != ver` | stale | 5 |
| 仅让 delete 略过版本比较，并将条件删除版本改成 0 | delete（含同值重写） | 7 |
| 仅移除 `.ask` 中文模板 | delete | 1 |
| 仅略过姓名／号码相等检查 | delete | 3 |

【实测】完整代码的临时截图探针为 stale 44、delete 58（含一项截图断言）、最长姓名／号码 27 项断言通过，均无警告。最长输入覆盖未聚焦及姓名／号码两种焦点，真实 3072 字节 AI 上下文均成功生成。

## 没做到的

- 视觉差未达到 12/255，差异见下一节。
- 生成的共享能力文档没有更新，因此 `capability-table-doc` 不通过；总规则第 7 节明确留给合并主会话处理，未跳过此测试。
- 未运行 Linux／gcc 或实机；本报告运行证据只来自 macOS／clang 模拟器，没有碰设备、推送或开 PR。

## 和设计不一致的

- 【实测】PhoneContacts 对照分数 **52.004/255，未达到 12/255**。产物在忽略目录 `work/contacts-compare/`，已经人工看图。原稿灰底很浅、字号和头像较小，按合同采用现行主题的第 8 级灰及放大后的规格。原稿顶部分段包含后续拨号／最近页面，本次只做联系人页头；新增底部添加入口，分页条在其上方；列表第二行显示真实号码，未实现原图的常用／家人标签。测试姓名和页数也与原图不同，没有为了分数改规格或写死示例。
- 姓名和号码的编辑页原稿尚缺（规范第 12 节已列缺口），本次直接复用短输入框和既有主题。每条联系人保存一个号码，选中能力返回该记录编号和这个号码。

## 改过的已有测试

- `tests/integration/apps.c` 的精确应用数由 6 改为 7：新增真实的 `phone` 系统应用，原有 2 个产品应用＋4 个夹具应用现在是 3＋4；未放宽为范围或删除断言。
- `tests/integration/ai_surface.c` 的 AI 可用表末尾追加本次 8 项能力及必填参数；保留逐项清单、标题、参数、权限和运行时检查。
- `tests/integration/confirm.c` 原一次取完整清单的 32 KiB 测试缓冲已装不下新增能力。改用现有分页接口、4 KiB PSRAM 缓冲遍历，仍要求所有页不含确认／取消能力，同时确实找到查询／撤回能力；没有提高预算或缩小检查范围。
- 本轮只向 `tests/contacts/contacts.c` 和 `run.py` 追加两个独立回归阶段；原 CRUD／重启及分页期望不改。新增阶段各用独立合成 Flash，检查真实确认结果，未加 sleep、跳过或提高预算。
- 本轮将 `tests/integration/ai_surface.c` 的 `contacts.delete` 精确必填清单从 `id` 改成 `id,if_revision,name,number`，与安全绑定后的接口一致；仍逐项检查且数量必须完全相等，没有放宽断言。

## 要用户定的

- 集合暂设每成员最多 200 条，姓名最多 32 个字符且 UTF-8 不超过 96 字节，号码最多 24 字符；任务未给容量数字，取了可逆的小容量。是否扩大集合上限、同一联系人是否需要多个号码，留给用户定；当前按任务的姓名＋号码完成一条一号。

## 顺手发现

- 【实测】应用目录原来把所有已注册应用也排进“工具”；注册 `phone` 后会与固定的“记和联系”入口重复，并改变已有两页快照。本次只增加一条 `phone` 排除语句；原来的目录测试不改期望，修复后两个硬件档案的快照测试通过。
- 【实测】普通全量首轮为 213/218，5 项失败：上述目录快照 2 项、AI 能力枚举 1 项、旧完整清单缓冲 1 项、生成的共享能力文档 1 项。前四项修正后定向 5/5 通过；共享文档按总规则第 7 节不改。

## 新增的机制

- 本轮无新任务、线程、状态机、文件格式、配置项或公共 C 接口；只扩展原能力参数，复用已有记录版本、BG 执行器、条件删除和确认卡描述。所有业务修改限于联系人原文件。
- 任务书要求的按成员 `phone.contacts` 集合，记录为现有数据服务的 JSON（姓名、一个号码）；没有自定义磁盘格式。
- 页面加载、写入和回执复用现有 BG／UI 执行器；没有新增常驻任务、线程、重试、缓存或配置项。

## 要改的文档

- 合并时更新 `docs/system/ui-dependencies.md` 的联系人状态，重新生成 `docs/system/capabilities.md`；本任务不修改这些共享文件。

## 执行记录

- 开工：工作区干净，HEAD 与指定基线一致，尚无此前任务报告或任务提交。
- 已读取任务规则及 G1–G5。按第 7 节：命令使用 utility 优先级，构建和测试最多各 `-j2`，不 push，不开窗口，不碰设备。
- 【实测】普通全量首次构建退出 0；联系人定向测试通过，写入阶段 115 项、重启阶段 21 项断言，零失败。两阶段内部 RAM 最低余量均为 47,840 字节，分配失败均为 0。新测试复用原 `ui_scene.budget.json`，未改预算。
- 【实测】补充短输入快照焦点后，定向测试为写入 117 项、重启 21 项断言，10 份快照均通过；没有新增产品机制。
- 【实现】首次打开集合经现有 `data.list` 在 BG 加载；UI 只拿已加载集合，不等待 Flash。写能力绑定调用成员，成功回执等数据服务确认写入。AI 查询走 `data.list/get`，集合禁止通用写入，增改都走 `contacts.put` 校验。
- 【实测】另行生成 `work/contacts/capabilities.md` 并与模拟器完整注册表比较，205 项能力、16 项设置一致；没有修改共享的 `docs/system/capabilities.md`。

## 原开发阶段反向对照（历史）

逐个临时撤掉下面的产品行为，测试和预算不动；每次恢复原字节。3 次构建均退出 0，CTest 均退出 8，`test.finished=1`。证据在忽略目录 `work/contacts/negative/`。

| 撤掉的行为 | 命令（均加 utility 优先级） | 失败断言 |
| --- | --- | ---: |
| 能力按调用成员开集合，临时改成固定 owner | 构建 `sim-contacts -j2`；`ctest --test-dir build -R '^contacts$' --output-on-failure -j2` | 4 |
| 选择返回原始编号，临时返回编号加一 | 同上 | 1 |
| 目录排除重复的 phone 入口 | 构建 `sim-ui-regressions -j2`；`ctest --test-dir build -R '^ui-snapshot-regressions-metalio-plus$' --output-on-failure -j2` | 1 |

## 规模与架构审视

- 【实测】当前基线累计产品代码 263 行新增、0 行删除，含全部 6 行 CMake 接线：联系人文件 254，启动注册 2，目录去重 1，CMake 6。原开发为 249 行；本轮净增 14 行。测试和文档不计，未超过 300 行。
- 为什么不能更小：本任务同时要求持久 CRUD、成员边界、可触摸的分页页面、两个短输入框、AI 快照和号码选择。已复用 `data.list/get`，未另造查询／分页服务；更小的通用 `data.put/delete` 方案会绕过号码校验，单纯显示文本则没有短输入、同源动作和分页快照。
- 【实现】存储仍由数据服务负责；页面及原子能力集中在自己的 `system/apps/contacts/contacts.c`。新能力均进总线和现有 `ink_member_allows` 闸门，按钮不改数据服务内部状态；删除使用已有人工确认卡；通信服务未改。
- 【实现】联系人唯一写入口在 BG 执行器串行处理，通用 `data.put/delete` 无权写该集合；版本比较集中在该入口。触摸和 AI 使用相同校验，表单对象引用改成实际联系人编号与读取版本，符合 R-061；未改数据服务或增加锁、事务框架。

## 复现命令

配置按总规则第 5 节全部参数，额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，使中枢测试使用已有依赖；ASan 配置只增加 `-DINK_SANITIZE=ON`，SDL2、ImGui 仍是已有固定版本，源码复用 `$INK_DEPS/build/_deps/`，未安装／升级依赖。所有命令在仓库根运行。

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/contacts/capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out work/contacts-compare --scenes PhoneContacts --reuse
```

最后一条对照命令的 `work/contacts-compare/PhoneContacts/` 需先复制 `build/test-out/contacts/write/` 下的 `snap-scene.png` 和 `metrics.json`；截图来自已完成刷新、联系人测试通过的真实页面。

## 原开发阶段最终验证（历史）

- 【实测】两套完整构建均退出 0。最终普通全量 **217/218**，204.30 秒；ASan／UBSan 全量 **217/218**，335.84 秒；两轮 CTest 均退出 8，唯一失败 `capability-table-doc`，没有跳过测试。`build-asan/Testing/Temporary/LastTest.log` 中没有 AddressSanitizer、LeakSanitizer 错误或 UBSan 的 `runtime error`。
- 【实测】普通和 ASan 的联系人测试各通过写入 117 项、重启 21 项断言；内部 RAM 最低余量各 47,840 字节（原门槛 32,768），内外部内存分配失败均为 0。确定性测试在两套全量中均通过，预算未改。
- 【实测】`tools/capabilities.py --output work/contacts/capabilities.md --runtime … --check` 在普通和 ASan 注册表均通过，205 项能力、16 项设置；待合并会话生成共享文档的命令是 `python3 tools/capabilities.py`。
- 【实测】差异仅 11 个相关文件；通信服务、设计稿、共享文档及历史资料未改。`git diff --check` 退出 0。按规定对基线至 HEAD 及包含最终报告的差异执行隐私扫描，零匹配（搜索退出 1）；未提交设计稿或构建产物。
- 本地提交：`92db0381`（开工报告）、`c771b6fe`（联系人实现与验收）、`cadf2ce8`（目录接线和输入快照）；最终报告另作本地提交，未推送。

## 本轮最终验证

- 【实测】普通完整构建退出 0；完整 CTest 217/218，退出 8，143.53 秒；唯一失败 `capability-table-doc`。没有跳过测试。ASan／UBSan 结果待补。
- 【实测】普通联系人四阶段通过 117／21／44／57 项断言，全部 `test.finished=1`、零警告；各阶段内部总池最低余量 47,840 字节，原门槛 32,768，内外部内存分配失败均为 0。
- 【实测】临时能力表生成和 `--check` 均退出 0，源码与运行时 205 项能力、16 项设置一致。命令为 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/contacts-review-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json`，第二次追加 `--check`；未改共享能力文档。
- 差异、隐私扫描和最终提交核对待补。
