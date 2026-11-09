# transfer-pages 执行报告

基线：`50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`。仅在本地任务分支工作，不推送。

## 复查后的修复

复查起点：`2c0c1023`；本次按主会话 `REVIEW.md` 修复两项中级问题。下文里程碑保留原执行记录，本节记录复查后的结果。URL 中的端口按 HAL 的 `uint16_t` 类型格式化：最长 78 字节（含结尾字符），沿用 80 字节缓冲；没有新增行为或接口校验。

| 编号 | 结论与改动 | 测试证据 |
| --- | --- | --- |
| TP-M1 | 【实现】新增只读 `hal_transfer_host`；模拟器通过 `getsockname` 读取真实监听 IP，宿主调用仍走 `vtk_host_call`。`transfer.status.host` 透传该地址，页面和二维码使用同一个 host＋端口；仅地址变化也刷新。取址失败返回空 host，页面与 AI 快照显示“还没连上 Wi-Fi”，撤下旧地址和二维码。提示改为画板的“用手机扫，或在电脑上打开”。 | 【实测】Werror 构建及 `transfer-pages` 专项通过；测试核对真实 HAL／状态／可见地址，并将二维码位图与状态地址编码结果逐字节比较；通过合成地址 `192.0.2.42`、HAL 失败、恢复实际地址验证更新。`work/transfer-pages/review-ctest-final.log`。 |
| TP-M2 | 【实现】仅撤掉 `transfer.start` 的 `child_ok=true`，统一由现有策略拒绝孩子开启；保留 stop、status 权限。 | 【实测】页面专项验证孩子 AI 调用被拒；原 `integration-wireless-transfer` 已通过，验证卡正常时孩子触摸和 AI 开启均被拒、监听未开、没有写入，同时保留查询和关闭能力。`work/transfer-pages/review-ctest-final.log`。 |

【实测】五项反向对照均编译、链接成功；测试客户端均以 1 退出并命中指定断言：页面地址写死、二维码编码错误、缺地址提示缺失、状态未透传 HAL 地址、恢复儿童开启权限。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/transfer_pages/reverse.py --build build --case review`。证据：`work/transfer-pages/review-reverse-final.log`、`build/transfer-reverse/summary-review.json`；源文件哈希前后相同。

端口类型收紧后另跑 `--case host`，仍编译、链接成功并命中地址硬编码断言；`work/transfer-pages/review-reverse-host-final.log`、`build/transfer-reverse/summary-host.json`。

全量首轮暴露原测试握手的时序问题：`bad-after` 回执文件在请求后 25.610 毫秒已写下，设备端却先耗尽 30 秒虚拟等待。核查 `vtk.c`，实时模式在宿主渲染落后时会追赶虚拟时间。测试在唯一的请求发布点调用既有 `vtk_set_realtime_speed`，重设当前速度的时间起点，给外部进程原有的响应窗口；不改产品代码、不增加 sleep、不调整等待上限和断言。原始失败证据：`work/transfer-pages/review-ctest.log`、`build/transfer-pages-o_6w0j_6/Transfer/`。最终结果以下述重建后的全量为准。

【实现】本次产品源码增 24 行、删 11 行；从原任务基线累计，产品源码增 241 行、删 6 行，计入 CMake 后保守合计 254 行，未超过 300 行预算。没有新任务、状态机、文件格式、配置项、缓存或队列；只增加复查明确要求的只读接口与字段，复用既有轮询和权限策略。测试替身仅覆盖只读取址，收发仍使用真实模拟器 socket。

【实测】最终普通全量 241/242、零跳过，退出 8，用时 327.58 秒；唯一失败 `capability-table-doc`，按用户补充由主会话合并时重新生成，未跳过检查。Hub 内部 pytest 为 125 passed，没有跳过。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`，设置 `PYTHONPATH="$PWD/hub"`；日志 `work/transfer-pages/review-ctest-final.log`。

【实测】最终普通版页面场景通过 205 项检查，内部 RAM 最低余量 47736 字节，未放宽 32768 字节门槛。已检查当前 Transfer 截图，文字和二维码显示正常；与画板的 1/4 灰度差为 43.054/255，仍超过 12，布局、灰度和字号差异沿用下文的已知限制。图、并排图和分数在 `build/transfer-compare/`；命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/transfer-compare --reuse --scenes Transfer`。日志 `work/transfer-pages/review-visual.log`。

【实测】最终 Werror + ASan/UBSan 全量同为 241/242、零跳过、退出 8，用时 382.87 秒，唯一失败仍是上述文档同步检查。系统与测试编译参数均含 `-fsanitize=address,undefined` 和 `-Werror`。完整 `LastTest.log` 未检出 AddressSanitizer、LeakSanitizer 或未定义行为诊断；页面同样通过 205 项检查，内部 RAM 最低余量同为 47736 字节。构建：`taskpolicy -c utility cmake --build build-asan -j2`；全量命令仅将普通版目录换为 `build-asan`。日志 `work/transfer-pages/review-asan-build-final.log`、`work/transfer-pages/review-asan-ctest-final.log`。

【实测】本机测试依赖齐全，无缺依赖或跳过项。沿用已有 `INK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 并设置上述 `PYTHONPATH`，Hub 两版均为 125 passed；检查了 pytest、pytest_asyncio、aiohttp、PyYAML、Pillow 可导入，生成字体与内容可用。原有返回 77 的缺依赖／缺字体／缺 Hub 样本条件均未触发。没有安装或升级依赖；Linux/GCC 和实机仍未测试。

【实测】本次差异为 12 个相关文件；`git diff --check` 通过，基线到最终改动的隐私扫描 0 匹配（grep 退出 1），没有构建产物、设计稿、受限共享文档或绝对本机路径。初始未跟踪的 `REVIEW.md` 保留原样，不纳入本次提交。仅本地提交，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】本机模拟器通过；实机手机访问未验证 | 地址与二维码均由状态 host＋端口构成，数字取自状态；缺地址如实提示；返回和 HOME 均断开 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^transfer-pages$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 外部 HTTP 传入 12288 字节，Live 显示 4096 字节，完成显示 1 个；逐字节核对，文件应用可见 | 同上；`tests/transfer_pages/run.py --build build --visual` |
| 3 | 【实测】流程通过；占用者名单未完成 | 占卡拒绝、停用后确认交出、结束及弹出后 rescan、新文件可读；坏卡零 SD 写入，卡内容哈希不变 | 同上；`build/transfer-compare/*/snapshots.ndjson` |
| 4 | 【实测】通过 | 儿童无两行入口，两项导航、无线开启与 USB 挂载均被拒；五页快照在 3072 字节内，动作能力供 AI 调用 | 同上；`integration-wireless-transfer`；反向对照 `tests/transfer_pages/reverse.py --build build --case review` |
| 5 | 【实测】除用户豁免项外通过：最终普通与内存全量各 241/242 | 唯一失败是共享能力文档过期；零跳过，无 sanitizer 报错。产品源码累计增 241 行、删 6 行，连 CMake 保守计 254 行；截图已留存，分数仍未达目标 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存版换 `build-asan` |

## 没做到的

- 普通与内存全量均不是全绿，能力文档需主会话生成；未改或跳过检查。
- 模拟器真实监听地址仍是环回地址；真机端口尚未实现，真实手机访问未验证。后端仍无卡占用者名单，未添加逐应用停止能力。
- 未运行 Linux/gcc 或实机测试。

## 和设计不一致的

- 无线 HAL 在模拟器中仅监听环回地址。页面按 `transfer.status.host` 和端口显示真实地址与二维码，提示语已按画板修正；真机端口须返回实际 Wi-Fi 地址。
- USB 占用页如实写“占用的应用暂不能查看”；不能列出真实应用名，也不能承诺自动停下它们。
- 后端沿用 `/me/收件箱/`，文件界面将其显示为“传进来的”，不迁移目录。
- 页面未提供画板的无线／数据线分段切换，改由任务要求的首页两行分别进入。Live 显示实际收到的数量和字节，不虚构文件名、总量百分比或“自动准备这本书”。
- 对照图使用实际数据；字号、控件和灰度遵守现行规范，与旧画板的数值不同。逐屏分数见下表，均超过目标，未改规格追分。

## 改过的已有测试

`tests/integration/ai_surface.c` 的 AI 能力清单新增 `transfer.open`、`transfer.usb`；不删除或放宽已有断言。

本任务新增的首页截图最初误用了设备的 128 token 读取器；该页快照是合法 JSON，但超过此 token 数。现在由已有宿主 cJSON 校验完整 JSON，五个新页面原有的设备解析器和能力检查仍保留，3072 字节限制不变。

复查按 TP-M2 修改 `tests/wireless_transfer/device.c` 原有“孩子上传成功、文件可读”的期望，改为触摸与 AI 开启均拒绝、状态无授权、文件不存在；`client.py` 的 child 场景由 HTTP 201 改为验证没有监听。原因是用户本次明确要求孩子不得开启无线传入，原期望与之冲突。保留成人真实上传、成员隔离和其余失败场景断言，新增孩子 status／stop 仍可调用的检查；不放宽预算、超时或断言。

## 要用户定的

本分支没有新增产品决定。地址只读接口及儿童开启权限已按本次复查补齐；占用者名单与真机端口仍交主会话另拆任务。

## 顺手发现

- 【实现】任务书提及的 `usb.refused_files_in_use`、`usb.disk_stopped_bad` 是探针指标，并非软总线事件；页面使用 `confirm.result`、`usb.changed` 和 `storage.card`。
- 【实现】后端实际为授权等待 120 秒、接收无进展 10 秒，并非规范的 10 分钟；未修改超时。

## 新增的机制

- 五个页面和两项导航能力：任务书要求；操作复用 `transfer.*`、`usb.*`、`shell.back`。
- 页面 LVGL 定时器：任务要求显示服务进度；只在页面存在时查询既有状态，无新增线程、持久格式、配置、队列或后台重试。
- `transfer.status.received`：原任务允许补的只读字节数；没有新增服务计数状态。
- 复查新增 `hal_transfer_host` 与 `transfer.status.host`：TP-M1 明确要求的只读接口，无新增运行机制。

## 要改的文档

- 主会话合并时重新生成 `docs/system/capabilities.md`（两项导航及 `transfer.start` 儿童权限），更新 `docs/system/ui-dependencies.md` 的五页与入口状态；本分支按规则不改共享文档。

## 为什么不能更小

【实现】五页共用一个 presenter、一个状态请求入口和一套快照声明，文件应用只增加入口、收件箱名称映射和初始化接线。服务只补只读字节数、地址与儿童开启权限。复用纯静态状态图会丢掉真实进度、关页收尾和 USB 结果，直接改服务状态会绕开能力与确认规则；未采用这些更小但不满足验收的做法。

架构审视：【实现】页面负责显示和查询，接收、存储及 USB 仍归原服务；按钮和 AI 调用同一能力，没有界面直接写服务状态。五页公开结构化快照，快照遮蔽一次性数字；既有 `transfer.status` 接口仍返回数字，未改后端行为。服务尚未开放的数据、权限差异均在本报告列明。

## 截图对照

截图、并排图和分数留在 `build/transfer-compare/`。各画面均等待显示稳定后获取；对照目标为 12/255。

| 页面 | 1/4 灰度平均差 | 主要差异 |
| --- | ---: | --- |
| Files | 58.807/255 | 保留基线的真实成员目录与分页布局，仅加任务要求的两行；画板的类型分类和容量摘要未在本任务重做 |
| Transfer | 43.046/255 | 六位真实数字和动态端口、环回地址提示；未画分段切换；现行字号与底部按钮高度 |
| TransferLive | 27.889/255 | 真实接收数量与字节替代示例书名和进度条；现行字号与图标大小 |
| TransferUsbBusy | 26.949/255 | 后端无占用者名单，说明改为真实缺失状态；现行深灰底与控件尺寸 |
| TransferUsbOn | 26.663/255 | 现行字号、图标和底部按钮尺寸、共享状态栏的真实状态 |
| TransferUsbFail | 26.806/255 | 不猜测失败原因，提示换行避免孤立标点；现行灰度与字号 |
| FileList | 28.887/255 | 仅一份合成上传文件，复用既有 F4 列表及页码布局，未重画文件应用 |

## 里程碑

- 已核对任务规则，工作区干净，HEAD 与指定基线一致；已定位六张相关画板。
- 【实测】Werror 构建、界面尺寸检查通过；专项测试通过，日志 `work/transfer-pages/focused-3.log`。
- 专项首轮发现同步状态回调被随后写回的 pending 标记覆盖，已在页面请求入口一处修复；第二轮只剩 USB 测试夹具写错模拟卡目录，修正为既有 `content/` 路径后通过，未改断言。
- 【实现】唯一服务增量为 `transfer.status.received` 只读字节数，复用现有文件位置和已完成文件大小；无新增传输计数器或后台任务。
- 【实测】六屏各通过 159 项检查，内部 RAM 最低余量 47736 字节；`work/transfer-pages/visual-2.log`。
- 【实测】五个反向变体均编译链接成功，客户端均以 1 退出并命中对应断言：同步回调、关页断开、完成数量、儿童导航权限、坏卡页面；`work/transfer-pages/reverse-verified.log`、`build/transfer-reverse/summary.json`。源码哈希前后相同。
- 早期“隐藏接收字节”的反向变体先触发了后端接收超时，未得到目标断言，未计为通过；最终改为撤掉完成数量显示，验证命中目标断言，不改产品超时或加等待。
- 重复截图运行发现 USB 收尾中 `lent=false` 早于卡状态刷新；改为等既有 `usb.changed` 事件再判断，后续六轮全部通过。
- 【实测】普通全量 241/242，通过的包含确定性、现有文件／传输服务和新页面；无跳过，CTest 退出 8，用时 349.25 秒。唯一失败 `capability-table-doc`，候选表已生成至 `build/transfer-capabilities.md`，仅新增两项导航及相应关联；未改共享表。日志 `work/transfer-pages/ctest.log`、`build/Testing/Temporary/LastTest.log`。
- 【实测】第一轮 ASan/UBSan 全量同为 241/242、零跳过、退出 8，用时 689.60 秒，唯一失败仍为共享能力文档；`work/transfer-pages/asan-ctest.log`。
- 【实测】最后核对首页发现新增入口的 101 高度被按 125 排列，会压住分页条。只修正入口两处高度引用，专项 170 项检查通过，新增真实控件边界断言与首页截图；`work/transfer-pages/entry-focused-2.log`、`build/transfer-compare/Files/`。
- 【实测】最终六项反向对照全部命中预期断言，新增“恢复错误入口高度会遮住分页条”；源码哈希不变。`work/transfer-pages/reverse-final-all.log`、`build/transfer-reverse/summary.json`。
- 【实测】最终代码 Werror 普通全量 241/242、零跳过、退出 8，用时 194.71 秒；Werror + ASan/UBSan 全量同为 241/242、零跳过、退出 8，用时 324.77 秒。唯一失败均是 `capability-table-doc`；完整内存日志没有 AddressSanitizer、LeakSanitizer 或未定义行为报错。日志 `work/transfer-pages/ctest-final.log`、`work/transfer-pages/asan-ctest-final.log`。
- 【实测】最终两版的新页面专项各 167 项检查通过；内部 RAM 最低余量均为 47736 字节，未放宽 32768 字节门槛。结束、弹出后的重整、坏卡无写入、成员入口和五页快照均覆盖。

## 复现与结束检查

配置沿用总规则 §5 的字体与 Python 参数，额外指定已有只读依赖：`-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`、`-DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src"`、`-DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"`。ASan 仅换成 `build-asan` 并加 `-DINK_SANITIZE=ON`，保留 Werror。构建 `taskpolicy -c utility cmake --build build -j2`，测试前 `export PYTHONPATH="$PWD/hub"`；内存版本替换构建目录。

```bash
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/transfer_pages/run.py --build build --visual
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/transfer_pages/reverse.py --build build
INK_TRANSFER_SCENE=Files taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/transfer_pages/run.py --build build
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/transfer-compare --reuse --scenes Files Transfer TransferLive TransferUsbBusy TransferUsbOn TransferUsbFail FileList
```

`INK_DESIGN` 由任务环境提供；截图不提交。全量、反向对照均执行完成，构建与测试最多并行 2，均使用 utility 优先级。

【实测】基线至最终代码提交按总规则原命令做隐私扫描，0 匹配，grep 退出 1；`git diff --check` 通过。差异共 9 个任务相关文件，无构建产物、设计稿或受限共享文档。只做本地提交，未推送、未开 PR、未碰设备。
