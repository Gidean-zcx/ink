# batch5 独立复查报告

- 范围：`git diff 2318b590 0ab0cec7`；检出版本 `0ab0cec7af058fa5a1b9d80466582dfbfc38367c`。
- 时间：2026-11-10 02:06～02:36（Asia/Singapore），约 30 分钟；只做本轮。
- 边界：产品代码与已有测试只读；仅写本报告和构建目录中的实验/日志；不碰设备、不提交、不对外发布。
- 证据标记：【实测】本轮执行；【实现】代码路径核查；【估算】注明假设；未验证判断单列“待证实”。

结论：本轮证实 **1 项高、2 项中**。现有普通与 ASan/UBSan 全量均通过，仍不足以接受本批：确认说明和文件变更的持久性缺口需要修复，回执入口也存在成员目标残留。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 正确性、持久化、成员隔离 | 不通过 | B5-02、B5-03；既有断电恢复、成员切换、插拔卡回归通过 | 全量 CTest；`experiments.py create/rename/receipts`（各模式单独调用） |
| 2 确认、儿童限制、秘密字段 | 不通过 | B5-01；未发现本批放开孩子访问成人、AI 代确认或秘密字段导出，相关既有回归通过 | 全量中的 `audit4-b4-01`、`audit4-b4-06`、`privacy-records`、`kid-3`；`experiments.py confirmation` |
| 3 原子能力与页面快照 | 部分符合 | 新页面/示例动作复用能力；能力清单与快照检查通过；B5-03 的入口目标依赖历史，语义不完整 | `capability-table-*`、`app-framework-*`、`factory-pages`、`privacy-records`、`kid-3`（均在全量中） |
| 4 资源与固定上限 | 现有预算通过，余量紧 | 下述内部 RAM、栈估算及容量实测；系统层未新增常驻业务任务，索引复用 BG 执行器 | 全量指标；`capacity.py`；`bash ports/esp32s31/build.sh` |
| 5 合并配合 | 有遗留 | B5-02 的后台优化破坏持久边界；B5-03 跨日志/回执入口；三处输入测试被延后，见下文 | 范围 diff；两个短按实验；普通及 ASan 全量 |
| 6 Linux/gcc 可移植性 | 部分验证 | macOS Clang 与 S31 GCC 交叉编译通过；未运行 Linux 宿主 CI，不能给出其通过结论 | 两套模拟器构建；`bash ports/esp32s31/build.sh`；全量中的 S31 端口检查 |

## 已证实问题

### B5-01【高】清空存储卡的额外删除范围没有进入人的确认说明

- 位置：`system/policy/system_actions.c:280` 的 `system.factory_reset.ask`；`system/policy/confirm.c:159–177` 在已有 `ask` 时只展开模板，不另列参数。
- 【实现】本批新增 `clear_media`，但确认说明仍固定为“清空这台设备：清掉所有家人的数据，不能撤销”。AI 可以直接请求 `system.factory_reset({"clear_media":true})`，无需先打开 FactoryReset 页面；人看到的确认卡无法分辨卡上书和音频是否也会永久删除。参数虽绑定到了请求，删除范围没有完整呈现给确认者。
- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch5-audit/experiments.py confirmation`：分别请求 false、true，读取真实 `ink_confirm_list` 后取消，两次说明逐字相同；差异断言失败 1 条。没有实际执行删除。证据：`build/batch5-audit/confirmation.log`、`build/batch5-audit/confirmation/confirmation/audit.txt`。
- 最小修法：在系统确认说明中明确列出“同时清空存储卡上的书和音频”的实际选择（缺省按否展示），保留现有参数绑定和家长密码校验；补直接 AI 调用的说明对照断言。

### B5-02【中】成功文件操作后断电，旧的索引完成记录仍被当作有效，重启漏文件或丢失稳定编号

- 位置：`system/services/file/file_service.c:1454–1457`（失效仅写 RAM）、`1489–1493`（`index_save` 不再等待落盘）、`1674–1687`（重启复用完成记录）；新增文件触发点 `657`。
- 【实现】卡上文件与 Flash 中的索引、完成记录没有保持同一个持久边界。新文件落盘后只是设置 `FS.index_dirty` 并约 50 ms 后排后台任务；改名的索引更新也只入队。断电后内存状态消失，但旧完成记录仍让后台直接标 ready，且停止补建。
- 【实测】先让一张只有 `SYNTHETIC_OLD.txt`（3 B）的合成卡完成索引并落盘，再分别执行以下实验。没有卡住线程，没有改产品源码；文件 API 成功返回后 1 μs 触发模拟断电，同一 Flash/Card 开新进程，等待 ready 后再等 20 秒。
  - `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch5-audit/experiments.py create`：新增 5 B 文件确实存在；重启后 `new_id=0`、`count=1`、`file.usage.status=ready`、书籍总量仍为 3 B（应为 8 B）。证据：`build/batch5-audit/create.log`。
  - `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch5-audit/experiments.py rename`：改名后的文件确实存在；原编号 1 仍指旧路径，`ink_fs_lookup(1)=-3`（NOT_FOUND），重启后书籍总量为 0 B。证据：`build/batch5-audit/rename.log`。
- 两组断电阶段均 0 条失败，重启阶段各 1 条失败。文件字节未丢；影响是“ready”统计不实，以及依赖稳定编号的内容引用在重启后不可用。手工目录查询可能补入新编号，不等于恢复原引用。
- 最小修法：在文件变更成功回执之前，持久化完成记录失效；恢复改名/删除索引编辑的持久性边界（不要恢复后台扫描逐条同步写盘）。启动时发现未完成变更须补建，改名仍须保持原编号。将以上两种成功回执后断电加入现有恢复测试。

### B5-03【中】大人看过孩子活动后，再打开自己的回执仍展示孩子数据

- 位置：`system/ui/shell/ai_pages.c:163–172`、`222–235`。`shell.ai_log` 改写共享的 `L.member`，`shell.ai_receipts` 没有按入口重新绑定目标，`list_create` 仅在成员 epoch 变化时恢复当前成员。
- 【实测】在成人空间分别给成人、孩子写明显不同的合成记录；调用 `shell.ai_log({"member":"kid"})` → `system.home` → `shell.ai_receipts({})`。真实页面与 AI 快照标题为“AI 做事的结果”，内容却是 `SYNTHETIC_CHILD`，没有 `SYNTHETIC_OWNER`。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch5-audit/experiments.py receipts`；归属断言失败 1 条，见 `build/batch5-audit/receipts.log` 和该实验目录内 `audit.txt`。
- 影响：成人自己的操作回执被此前查看的孩子记录替代；不是孩子越权读取成人，也不是数据落入了错误集合。页面参数缺少归属，结果依赖浏览历史，破坏按钮和 AI 调用的可预测语义。
- 最小修法：仅在从当前孩子日志进入对应回执时保留该目标；独立打开回执默认当前成员。若通过可选 `member` 参数传递，复用日志入口的成人/孩子鉴权，并在页面说明目标成员。

注：临时实验二进制未带预算文件，模拟器退出码为 0，但 `test.fail` 与 warnings 明确记录失败；脚本退出 0 仅表示实验执行完，不表示被审行为通过。构建命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch5-audit/experiments.py compile`（退出 0，复用正常构建的编译参数和静态库，只替换测试驱动对象）。

重复运行时设置 `INK_AUDIT_OUT=build/batch5-recheck`，并使用新的目录名；脚本拒绝覆盖既有证据。例如：`INK_AUDIT_OUT=build/batch5-recheck taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch5-audit/experiments.py create`。

## 本轮验证记录

- 【实测】普通构建成功；全量 CTest **268/268 通过、0 跳过**，退出 0，耗时 198.43 秒。日志：`build/batch5-audit/build.log`、`build/batch5-audit/ctest.log`。
- 【实测】ASan/UBSan 构建成功；全量 CTest **268/268 通过、0 跳过**，退出 0，耗时 458.65 秒。日志：`build-asan/batch5-audit/{build,ctest}.log`。核对产品编译参数含 `-Werror -fsanitize=address,undefined`；CTest 和 `LastTest.log` 的 sanitizer 错误扫描无匹配，见 `build/batch5-audit/sanitizer-scan.log`。
- 构建使用任务指定字体、Python 与 LVGL；额外配置 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，测试设置 `PYTHONPATH="$PWD/hub"`，使用当前检出的中枢代码。未安装或修改依赖。
- 全量命令：`INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。
- ASan 全量命令同上，将 `build` 换成 `build-asan`；配置额外加 `-DINK_SANITIZE=ON`。构建、测试均以 utility 优先级运行，并行度不超过 2，没有同时运行两轮构建或全量测试。
- 【实测】临时能力清单驱动读取真实注册表：能力 **353/512**、事件订阅 **32/64**、设置 **17/48**、数据定义 **20/32**。前三张表未接近满额；数据集合的同时打开上限 32 不等同于定义数，本轮未遍历其最大并发组合。JSON 解析器固定 128 token，既有协议交叉检查通过，未证明任意大响应都能在设备端重解析。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch5-audit/capacity.py`，退出 0，见 `capacity.log`。
- 【实测】索引千文件夹具：最大持锁 78,016 μs、夹缝中的用户写入 9,492 μs；完成后 10 秒零后台运行检查通过。证据：`build/test-out/file-index-3/summary.json`。固定索引上限为 1,024 条，此夹具共 1,003 条，已到约 98%；不能把千文件验收解释成任意整卡容量。
- 【实测】`git diff --check 2318b590 0ab0cec7` 通过；对同一差异按任务要求扫描本机路径和 MAC 格式，0 匹配，见 `build/batch5-audit/privacy-diff-scan.log`。构建输出保留在忽略目录，不写进报告正文或版本历史。
- 【实测，交叉编译】使用任务给定的 `INK_DEPS`，`taskpolicy -c utility bash ports/esp32s31/build.sh` 退出 0，使用已安装的 ESP-IDF v6.1 / RISC-V GCC 15.2.0；本轮日志未检出编译警告或错误。当前默认固件 Flash 段 1,158,836 B、DIRAM 静态 82,966 B、PSRAM 静态 495,472 B，`.bin` 为 1,222,720 B；这些不含运行时堆峰值，也不是与本批前版本的受控差分。证据：`build/batch5-audit/s31-build.log`、`build-s31/{size,stack}.txt`。
- 【实测＋实现】S31 `.su/.ci` 已知直接调用链：storage 736 B、hub 3,088 B、ui 4,640 B；回调和库函数未闭合，不能用这些小计解除上面的栈风险。端口沿用 IDF 的部分 warning 例外，不能把交叉编译替代 Linux 原生 `-Werror` 检查。
- 【实测，视觉抽查】检查 FactoryReset、Records、KidToday 的模拟器截图，并对照前两屏设计图；未见明显遮挡。布局抽查不是逐像素验收，完整设计对稿未重跑。

## 待证实

- 【实测，模拟器】普通全量产物中的最低内部 RAM 总余量为 `media` 的 33,424 B，相对 UI/主要场景采用的 32 KiB 参考下限只剩 656 B（`media.budget.json` 自身没有此内存断言）；`ai_tools-metalio-plus` 为 37,376 B，距该场景新加的 35 KiB 下限剩 1,536 B。计量使用 `mem.internal_total.min_free_bytes`，没有把可向 DMA 池借用的普通内部池单独余量当成总余量。证据：`build/batch5-audit/resources.json`、对应 `build/test-out/*/metrics.json`。这是含测试驱动的模拟器结果，不能直接当实机余量。
- 【实测，宿主栈估算】`esp_timer` 为 3,552/3,584 B，`main` 为 4,000/4,096 B，`exec.media` 为 7,552/8,192 B，`ui` 为 9,664/10,240 B。没有把自检故意超限当产品缺陷；这些路径需要目标板高水位验证，ASan 或单函数 `.su` 不能证明整条任务调用链安全。本轮不据此调栈或定级。
- **S31 超时换算的时间起点假设**：`ports/esp32s31/osal.c:74–81` 将 `esp_timer_get_time()` 和 FreeRTOS tick 直接交给 `osal_ticks.h:12–18` 相减。已安装 SDK 中，两者用不同计数器：`$IDF_PATH/components/esp_hw_support/include/esp_private/systimer.h:30–31`；前者在早期初始化启动，后者在 `components/freertos/port_systick.c:88–89` 归零。换算依赖它们同起点、同相位，但端口没有校准。【实测，合成起点差】`taskpolicy -c utility cc -Wall -Wextra -Werror build/batch5-audit/ticks.c -o build/batch5-audit/ticks` 成功；执行该二进制退出 1，100 Hz 下请求 1,001 μs，在对齐样本中返回 1 tick，加入 50 ms 起点差后返回 6 tick，见 `ticks.log`。**50 ms 是实验输入，不是实机测量**；本轮不能确认目标启动后的实际相位差与延迟幅度，故不定级。下一项决定性验证是只读采样目标板两时钟及有限等待耗时；若起点差存在，应在 OSAL 的统一换算处解决，保留“不提前超时”和回绕检查。

## 没做到的

- 没有运行 Linux 宿主 GCC/远端 CI，没有运行任何设备、烧录、真实存储卡或真实服务；本轮的断电是模拟器断电。
- 十个任务的产品差异和报告均已核查；实际运行的是当前检出的全部 268 项测试、S31 默认固件构建及上述一次性实验，没有重跑十个任务所有历史基线、反向变体、性能采样或设计像素对比。
- `script-s31` 四候选原型及其 S31 增量测量不属于这 268 项 CTest，本轮未重跑。MicroQuickJS 的上游 UBSan 诊断已由原任务报告披露；本文“ASan/UBSan 全量通过”仅指默认产品测试，不能解读为四个原型均通过。已核查候选只由显式注入构建，未进入默认固件。

## 和设计不一致的

- 【实现】`privacy-records` 验收 1 要求真实对话次数；当前后端仍没有此查询，Records 如实显示“暂时读不到次数”。活动件数与导航已测过，但不能把这条整体验收写成完成。原任务报告已披露，未作为新缺陷重复计数。
- 【实现】`port-skeleton` 的 HAL 是明确的无硬件桩；`script-s31` 的解释器只是测量候选，没有进入默认产品。编译通过不表示屏幕、存储、网络或解释器已经在目标硬件运行。

## 改过的已有测试

本轮无。以下是本批已有的改动，不能与本轮修改混淆：

- `tests/hub/e2e.script:2–4`：首次点按由 3 秒改到 7 秒。
- `tests/integration/media.c:506–510`、`tests/camera_gallery/camera_gallery.c:233–235`：在原短按前增加落盘/索引完成等待。`file-index-3/REVIEW.md` 已披露后台写盘的输入风险；这削弱了与后台写盘交叠的覆盖。
- 【实测，反证】本轮分别在构建目录副本中去掉音量落盘等待，以及相册索引/落盘等待，保留原 80 ms 短按和断言，两个普通构建实验均退出 0、`test.fail=0`。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch5-audit/short_key.py` 和 `camera_key.py`（同目录）；证据 `short-key.log`、`camera-key.log`。因此未把“当前必然丢键”列为已证实问题，输入风险仍待专项验证。
- `tests/scenarios/ai_tools.budget.json` 新增 35 KiB 内部 RAM 下限，是收紧检查；未发现本批抬高此资源预算来放行。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

无；本轮仅复查。

## 要改的文档

- 修复后将 B5-01～03 和回归证据同步到对应任务报告；共享需求与决策文件本轮未改。
- `docs/tasks/file-index-3/REPORT.md` 仍写“原五项回归的测试/脚本均未改动”，但最终合入版本修改了中枢脚本；应按同目录 `REVIEW.md` 的最终变更补齐说明，避免只看 REPORT 时误以为时序覆盖完整保留。

SEVERE=0 HIGH=1 MEDIUM=2 LOW=0
