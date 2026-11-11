# reader-settings-1 执行报告

基线：`cfd1a3dd5550fda64d40c23e80a7d60a0a8ee7c9`。状态：功能与阅读回归完成，完整验证进行中。只提交到本地任务分支；未连接设备、未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 档位与重排 | 通过【实测】 | 4 种字号、行距/段距/边距各 4 档均改变正文像素；章内偏移和同一句原文保留；本轮读取阶段 397 条断言通过 | `ctest --test-dir build --output-on-failure -j2 -R '^reader-settings$'` |
| 1.2 隔离与持久化 | 通过【实测】 | 大人 72/3/3/3/15、孩子 53/2/0/0/12 各自保留；重启阶段 49 条断言通过 | 同上（运行器自动用同一闪存跑第二个进程） |
| 1.3 菜单与 AI | 通过【实测】 | 中间开关菜单，左右翻页；设置、目录、返回书架、页码和目录序号输入均走能力；TXT/EPUB 都验证；非法参数和跨家人参数不改变设置 | 同上 |
| 1.4 底栏 | 通过【实测／实现】 | 四位分别开关、全关留出正文空间；时间和模拟电量变化后正文校验和不变；标签内容相同时不更新，变化时仅失效对应标签 | 同上；`reader_footer`、`footer_changed` |
| 1.5 反向对照 | 通过【实测】 | 临时对象撤掉重排分支，模拟器退出 1，14 条断言失败；不改源码、不依赖提交历史 | `ctest --test-dir build --output-on-failure -j2 -R '^reader-settings-reverse$'` |
| 1.6 原有阅读回归 | 通过【实测】 | `100% tests passed out of 23`；含 reader-epub*、reader-continue、reader_basic、reader_continuous、reader_rapid、确定性、老化闪存/故障预算，以及 120% 壳层页面 | 见下列回归命令 |
| 2.1 普通全量 | 进行中 | 待写最终原话 | `taskpolicy -c utility ctest --test-dir build --output-on-failure -j2` |
| 2.2 内存检查 | 待运行 | ASan + UBSan，全量 | 同构建参数，加 `-DINK_SANITIZE=ON`，目录 `build-asan` |
| 2.3 S31 固件 | 待运行最终版 | 基线编译完成，bin 4,605,232 字节；待记录最终增量及警告 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2.4 GCC 检查 | 通过【实测】 | `gcccheck: 2 compiled, 0 with findings`；产品阅读器及新增 C 测试 | `python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build apps/reader/reader.c tests/reader_settings/device.c` |
| 3 截图与上机清单 | 通过【实测】 | 本目录 10 张 PNG，单张均小于 300 KB；菜单、设置、紧凑/大字正文各有 100%/120%，另有底栏全开/全关；120% 阶段 29 条断言通过；下附真机清单 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build -R '^reader-settings$' --output-on-failure` |

阅读回归命令（前缀均为 `taskpolicy -c utility`）：

```sh
ctest --test-dir build --output-on-failure -j2 -R '^(reader-settings.*|reader-epub.*|reader-continue|ui-scene-big-(LockPage|Ai|AiListen|AiThink|PowerOff)|scenario-reader_.*|determinism-reader)$'
```

## 没做到的

- 按任务限定未移植 SD 卡自定义字体。36/44/53/72 像素复用现有完整中文字库，经正文图片缩放，未新增字体资产；大字号的字形质量须真机核对。
- 没有设备实测；重排耗时、残影不能用模拟器通过代替。
- 全量、内存及最终 S31 结果待完成。

## 和设计不一致的

- 原厂偏好档位、默认值和底栏位整段提取，C 兼容改动及哈希见 [SOURCE.md](../../../apps/reader/vendor/metalio_prefs/SOURCE.md)。原厂 overlay/sheet 函数均直接依赖 BookSession、全局 Book_State 或原厂任务，按任务允许使用系统主题组件重现其分组、加减、四档勾选和开关，未带入 NVS/线程。
- 页码沿用既有章内页；百分比是当前载入章段的字节比例，未计算全书总页数或全书精确百分比。大于 2 MiB 的章仍按既有分段读取。

## 改过的已有测试

无。没有删、跳过、放宽原有断言或预算；全部新增测试位于 `tests/reader_settings/`。

## 要用户定的

暂无阻塞决定。

## 顺手发现

- 120% 底栏固定行高导致重叠和越界，已统一改为实际字体行高，相关原有 5 个大字号场景通过。
- 其余范围外问题只随最终完整测试记录，不修改其他任务的文件。

## 新增的机制

- `reader.font_size`：按成员的阅读字号设置，任务第 1、3 条要求。
- `reader.line_spacing`：按成员的行距档位，任务第 1、3 条要求。
- `reader.paragraph_spacing`：按成员的段距档位，任务第 1、3 条要求。
- `reader.margins`：按成员的边距档位，任务第 1、3 条要求。
- `reader.footer_mask`：按成员的底栏位图，任务第 1、3、5 条要求。
- 菜单及设置浮层、`get_layout/set_layout/menu` 能力：任务第 2～4 条要求。复用原分页表、预渲染、设置服务和状态事件；没有新增常驻任务、定时循环、文件格式、后台重试或缓存机制。

## 为什么不能更小

需要把原厂档位、成员设置、重排定位、浮层、数字输入、底栏和 AI 串到现有阅读器。更小的替代是只加字号按钮或直接搬原厂 BookSession；前者缺任务功能，后者会引入 NVS、任务和另一套分页。实际只接入现有组件与服务，新增产品代码 290 行（阅读器 264、CMake 7、翻译 19）；原文提取头的 C 兼容和保护共 11 行也算入，则共 301 行，低于 500 行预算。删除 37 行，不用净行数抵扣新增量。

## 要改的文档

由合并会话统一生成 `docs/system/capabilities.md`：新增 `reader.get_layout`、`reader.set_layout`、`reader.menu`，说明字号单位为像素、其余三个档位为 0～3、底栏为 0～15 的位图；`menu.view` 可取 menu/settings/page/chapter/shelf。没有改共享文档。

## 上机核对清单

1. 同一本真实 EPUB 停在章内中段，连续修改字号、行距、段距和边距：原句留在当前页开头，前后翻页没有漏字；记录重排至稳定耗时、内部 RAM/PSRAM 余量、残影。
2. 常规和 120% 系统字号检查菜单/设置控件、底栏边界；阅读字号独立；36/72 像素字形清晰度可接受。
3. 中央开关菜单，左右翻页；TXT 与 EPUB 的目录、数字跳页/跳章、返回书架均能到达；输入无效序号留在输入处。
4. 时间/电量仅刷底栏对应区域，正文不刷；四项分别开关和整条隐藏正确；长期阅读按系统 D-127 累计刷新策略执行。
5. 两位家人设不同字号和底栏，切换、回书架及重启后各自恢复，孩子能改自己的偏好。
