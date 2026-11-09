# transfer-pages 执行报告

基线：`50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`。仅在本地任务分支工作，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】本机模拟器通过；手机入口未完成 | 地址由状态端口构成，数字取自状态；真实 LVGL 二维码；返回和 HOME 均断开 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^transfer-pages$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 外部 HTTP 传入 12288 字节，Live 显示 4096 字节，完成显示 1 个；逐字节核对，文件应用可见 | 同上；`tests/transfer_pages/run.py --build build --visual` |
| 3 | 【实测】流程通过；占用者名单未完成 | 占卡拒绝、停用后确认交出、结束及弹出后 rescan、新文件可读；坏卡零 SD 写入，卡内容哈希不变 | 同上；`build/transfer-compare/*/snapshots.ndjson` |
| 4 | 【实测】页面部分通过；既有儿童后端权限未改 | 儿童无两行入口，直接调用两项新导航能力被拒，USB 挂载被拒；五页快照在 3072 字节内，动作能力供 AI 调用 | 同上；反向对照 `tests/transfer_pages/reverse.py --build build` |
| 5 | 【实测】未全部通过：最终普通与内存全量各 241/242 | 唯一失败为禁止本分支修改的共享能力文档过期；无 sanitizer 报错。产品源文件新增 227 行，连 CMake 保守计 233 行；截图已留存，分数未达目标 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存版换 `build-asan` |

## 没做到的

- 普通与内存全量均不是全绿，能力文档需主会话生成；未改或跳过检查。
- 后端没有手机可达地址和卡占用者名单，未改后端行为，也未添加真实手机传入或逐应用停止能力。
- 【实现】既有 `transfer.start/stop/status` 仍为 `child_ok=true`，本次只拒绝儿童进入新页面和挂载整卡。若“直接调用被拒”包括既有无线后端，尚未达到；改变它会修改后端行为。
- 未运行 Linux/gcc 或实机测试。

## 和设计不一致的

- 无线 HAL 仅监听环回地址。页面按 `transfer.status.port` 显示本机电脑地址和真实二维码，明确标“仅本机”，不伪造手机可达地址。
- USB 占用页如实写“占用的应用暂不能查看”；不能列出真实应用名，也不能承诺自动停下它们。
- 后端沿用 `/me/收件箱/`，文件界面将其显示为“传进来的”，不迁移目录。
- 页面未提供画板的无线／数据线分段切换，改由任务要求的首页两行分别进入。Live 显示实际收到的数量和字节，不虚构文件名、总量百分比或“自动准备这本书”。
- 对照图使用实际数据；字号、控件和灰度遵守现行规范，与旧画板的数值不同。逐屏分数见下表，均超过目标，未改规格追分。

## 改过的已有测试

`tests/integration/ai_surface.c` 的 AI 能力清单新增 `transfer.open`、`transfer.usb`；不删除或放宽已有断言。

本任务新增的首页截图最初误用了设备的 128 token 读取器；该页快照是合法 JSON，但超过此 token 数。现在由已有宿主 cJSON 校验完整 JSON，五个新页面原有的设备解析器和能力检查仍保留，3072 字节限制不变。

## 要用户定的

本分支没有新增产品决定。后端地址、占用者名单及儿童后端权限缺口交由主会话另拆后端任务。

## 顺手发现

- 【实现】任务书提及的 `usb.refused_files_in_use`、`usb.disk_stopped_bad` 是探针指标，并非软总线事件；页面使用 `confirm.result`、`usb.changed` 和 `storage.card`。
- 【实现】后端实际为授权等待 120 秒、接收无进展 10 秒，并非规范的 10 分钟；未修改超时。

## 新增的机制

- 五个页面和两项导航能力：任务书要求；操作复用 `transfer.*`、`usb.*`、`shell.back`。
- 页面 LVGL 定时器：任务要求显示服务进度；只在页面存在时查询既有状态，无新增线程、持久格式、配置、队列或后台重试。
- `transfer.status.received`：任务允许补的唯一只读状态字段；没有新增服务计数状态。

## 要改的文档

- 主会话合并时重新生成 `docs/system/capabilities.md`，更新 `docs/system/ui-dependencies.md` 的五页与入口状态；本分支按规则不改共享文档。

## 为什么不能更小

【实现】五页共用一个 presenter、一个状态请求入口和一套快照声明，文件应用只增加入口、收件箱名称映射和初始化接线。服务只补只读字节数。复用纯静态状态图会丢掉真实进度、关页收尾和 USB 结果，直接改服务状态会绕开能力与确认规则；未采用这些更小但不满足验收的做法。

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
