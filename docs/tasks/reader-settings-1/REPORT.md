# reader-settings-1 执行报告

基线：`cfd1a3dd5550fda64d40c23e80a7d60a0a8ee7c9`。状态：功能、阅读回归与内存检查完成；完整测试有未通过项，见 2.1。只提交到本地任务分支；未连接设备、未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 档位与重排 | 通过【实测】 | 4 种字号、行距/段距/边距各 4 档均改变正文像素；章内偏移和同一句原文保留；本轮读取阶段 397 条断言通过 | `ctest --test-dir build --output-on-failure -j2 -R '^reader-settings$'` |
| 1.2 隔离与持久化 | 通过【实测】 | 大人 72/3/3/3/15、孩子 53/2/0/0/12 各自保留；重启阶段 49 条断言通过 | 同上（运行器自动用同一闪存跑第二个进程） |
| 1.3 菜单与 AI | 通过【实测】 | 中间开关菜单，左右翻页；设置、目录、返回书架、页码和目录序号输入均走能力；TXT/EPUB 都验证；非法参数和跨家人参数不改变设置 | 同上 |
| 1.4 底栏 | 通过【实测／实现】 | 四位分别开关、全关留出正文空间；时间和模拟电量变化后正文校验和不变；标签内容相同时不更新，变化时仅失效对应标签 | 同上；`reader_footer`、`footer_changed` |
| 1.5 反向对照 | 通过【实测】 | 临时对象撤掉重排分支，模拟器退出 1，14 条断言失败；不改源码、不依赖提交历史 | `ctest --test-dir build --output-on-failure -j2 -R '^reader-settings-reverse$'` |
| 1.6 原有阅读回归 | 通过【实测】 | `100% tests passed out of 23`；含 reader-epub*、reader-continue、reader_basic、reader_continuous、reader_rapid、确定性、老化闪存/故障预算，以及 120% 壳层页面 | 见下列回归命令 |
| 2.1 普通全量 | 未全部通过【实测】 | 原话：`99% tests passed, 4 tests failed out of 411`，1250.36 秒；其中 sdk-boundary、integration-ai-surface 修后与新阅读测试共 4/4 重跑通过；最终四个门槛重跑为 2/4：只余生成表及基线已失败的上机自测构建检查；原先 3 项 Hub 跳过已用既有依赖环境补跑，含前置项 4/4 通过 | `taskpolicy -c utility ctest --test-dir build --output-on-failure -j2` |
| 2.2 内存检查 | 通过【实测】 | ASan + UBSan 的上述阅读回归集合，`100% tests passed out of 23`，76.67 秒；未声称做过 ASan 全量 | 同构建参数，加 `-DINK_SANITIZE=ON`，目录 `build-asan` |
| 2.3 S31 固件 | 通过【实测】 | 基线 bin 4,605,232，最终 4,619,088 字节，增加 **13,856**；5 MiB 应用分区剩 623,792 字节。两次均 0 条编译器告警、6 条 SDK CMake 告警，告警标题一致；没有烧录 | `INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2.4 GCC 检查 | 通过【实测】 | `gcccheck: 3 compiled, 0 with findings`；阅读器、阅读设置测试与 AI 覆盖测试 | `python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build apps/reader/reader.c tests/reader_settings/device.c tests/integration/ai_surface.c` |
| 2.5 S31 宿主机 | 通过【实测】 | `100% tests passed out of 72`，151.53 秒 | `cmake -S ports/esp32s31/tests -B build-s31-tests`，构建与 ctest 均 utility、`-j2` |
| 3 截图与上机清单 | 通过【实测】 | 本目录 10 张模拟器 clean 帧 PNG，最大 50,022 字节；菜单、设置、紧凑/大字正文各有 100%/120%，另有底栏全开/全关；120% 阶段 32 条断言通过；下附真机清单 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build -R '^reader-settings$' --output-on-failure` |

阅读回归命令（前缀均为 `taskpolicy -c utility`）：

```sh
ctest --test-dir build --output-on-failure -j2 -R '^(reader-settings.*|reader-epub.*|reader-continue|ui-scene-big-(LockPage|Ai|AiListen|AiThink|PowerOff)|scenario-reader_.*|determinism-reader)$'
```

## 没做到的

- 按任务限定未移植 SD 卡自定义字体。36/44/53/72 像素复用现有完整中文字库，经正文图片缩放，未新增字体资产；大字号的字形质量须真机核对。
- 没有设备实测；重排耗时、残影不能用模拟器通过代替。
- 普通全量没有达到全绿：`capability-table-doc` 按共享文档保护规则保留；`autotest-build-switch` 在本任务与基线均失败。没有跳过这两项或放宽断言。

## 和设计不一致的

- 原厂偏好档位、默认值和底栏位整段提取，C 兼容改动及哈希见 [SOURCE.md](../../../apps/reader/vendor/metalio_prefs/SOURCE.md)。原厂 overlay/sheet 的主入口依赖 BookSession、全局 Book_State 或原厂任务，按任务允许使用系统主题组件重现其分组、加减、四档勾选和开关，未带入 NVS/线程。
- 页码沿用既有章内页；百分比是当前载入章段的字节比例，未计算全书总页数或全书精确百分比。大于 2 MiB 的章仍按既有分段读取。

## 改过的已有测试

`tests/integration/ai_surface.c` 的阅读器 `FOR_AI` 表新增 `get_layout`、`set_layout`、`menu` 三个登记，参数预期分别为无参数、全可选、全可选。未删除或放宽断言、预算；其余测试新增于 `tests/reader_settings/`。

## 要用户定的

生成的 `docs/system/capabilities.md` 在任务范围列为可改文件，但[总规则第 7 条](../codex-rules.md)明确说“这些共享文档不改，合并时由主会话统一改”；本轮采用这条保护规则，保留生成表校验失败，所需登记列于“要改的文档”。

## 顺手发现

- 120% 底栏固定行高导致重叠和越界，已统一改为实际字体行高，相关原有 5 个大字号场景通过。
- 【实测】`autotest-build-switch` 对基线归档同样失败：自测 CMake 只创建 fake_main/fake_system，原有 `ports/esp32s31/main/CMakeLists.txt:32` 调用阅读器构建却需要 `__idf_ink_apps`；另伴随沙箱禁止 ps 的报错。未改该测试或端口。手动复现：`python work/baseline/tests/device_autotest/build_switch.py build`，退出 1；基线归档由 `git archive` 原样取得，不是 ctest 对历史的依赖。

## 新增的机制

- `reader.font_size`：按成员的阅读字号设置，任务第 1、3 条要求。
- `reader.line_spacing`：按成员的行距档位，任务第 1、3 条要求。
- `reader.paragraph_spacing`：按成员的段距档位，任务第 1、3 条要求。
- `reader.margins`：按成员的边距档位，任务第 1、3 条要求。
- `reader.footer_mask`：按成员的底栏位图，任务第 1、3、5 条要求。
- 菜单及设置浮层、`get_layout/set_layout/menu` 能力：任务第 2～4 条要求。复用原分页表、预渲染、设置服务和状态事件；没有新增常驻任务、定时循环、文件格式、后台重试或缓存机制。

## 为什么不能更小

需要把原厂档位、成员设置、重排定位、浮层、数字输入、底栏和 AI 串到现有阅读器。更小的替代是只加字号按钮或直接搬原厂 BookSession；前者缺任务功能，后者会引入 NVS、任务和另一套分页。实际只接入现有组件与服务，新增产品代码 292 行（阅读器 265、CMake 8、翻译 19）；原文提取头的 C 兼容和保护共 11 行也算入，则共 303 行，低于 500 行预算。删除 37 行，不用净行数抵扣新增量。

## 要改的文档

由合并会话统一生成 `docs/system/capabilities.md`：新增 `reader.get_layout`、`reader.set_layout`、`reader.menu` 和上述 5 项阅读设置，说明字号单位为像素、其余三个档位为 0～3、底栏为 0～15 的位图；`menu.view` 可取 menu/settings/page/chapter/shelf。没有改共享文档。

## 最终核查

- 【实测】`git diff --check`、报告相对链接、原文头哈希、10 张截图大小、任务路径范围检查通过；没有构建产物或设计稿入库。基线至最终提交的隐私正则检查无命中（退出 1）。
- 【实测】构建最多 `-j2`，ctest 最多 `-j2`，均使用 utility；仅普通全量运行既有 SDL dummy 的无窗口 GUI smoke，没有启动窗口版或连接设备。没有新增依赖、付费接口或对外写入。
- 初轮全量因 120% 底栏问题中止；第二次全量进程退出 143，未当成通过；表中 411 项为之后完整跑完的一轮。修复项、Hub 补跑与最终门槛结果分别保留在本地忽略目录 `work/logs/`。

截图：[菜单 100%](menu-100.png) / [120%](menu-120.png)，[设置 100%](settings-100.png) / [120%](settings-120.png)，[紧凑正文 100%](compact-100.png) / [120%](compact-120.png)，[大字正文 100%](large-100.png) / [120%](large-120.png)，[底栏全开](footer-all-100.png) / [全关](footer-hidden-100.png)。clean 帧不代表真实墨水屏残影。

## 上机核对清单

1. 同一本真实 EPUB 停在章内中段，连续修改字号、行距、段距和边距：原句留在当前页开头，前后翻页没有漏字；记录重排至稳定耗时、内部 RAM/PSRAM 余量、残影。
2. 常规和 120% 系统字号检查菜单/设置控件、底栏边界；阅读字号独立；36/72 像素字形清晰度可接受。
3. 中央开关菜单，左右翻页；TXT 与 EPUB 的目录、数字跳页/跳章、返回书架均能到达；输入无效序号留在输入处。
4. 时间/电量仅刷底栏对应区域，正文不刷；四项分别开关和整条隐藏正确；长期阅读按系统 D-127 累计刷新策略执行。
5. 两位家人设不同字号和底栏，切换、回书架及重启后各自恢复，孩子能改自己的偏好。
