# ime-engine-1 执行报告

基线：`36473cc4933e6fea2f2a78a4fbec16186d3be3f4`。本地提交，不碰设备、不 push。**未达到全部验收条件：新引擎质量未过替换门槛，按任务第 5 项保留旧默认。**

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 对比 | 【实测】未过门槛，执行可切换分支 | 新 76/90 首选；旧 83/90。完整数字、110 条逐项结果见下表及 [QUALITY.md](QUALITY.md) | `python tests/ime_lexicon/google_run.py --build build`；追加 `--acceptance` 可执行默认替换硬门槛 |
| 2 组句和选词 | 【实测】通过列明的功能流程；九宫格长句质量未达标 | “我爱北京天安门”首选；选“我爱”后保留 `beijingtiananmen`，继续得“北京天安门”；整句提交后有联想；`94664` 提供“中国”，`946644862` 选“中国”后留 `2` | 同上，`composition` 检查及 `ime.google_nine_china` / `ime.google_nine_suffix` 均为 1 |
| 3 家人隔离 | 【实测】通过 | 成人在 `jiyu` 选“觊觎”，从非首选变首选；孩子不继承，切回和重启后成人仍为首选；密码不出候选、不进日志、不给 AI，两人词库哈希均不变。撤掉重开后隔离流程失败 | 同上；`python tests/ime_lexicon/google_run.py --build build --reverse` |
| 4 64 位词库 | 【实测】通过 | 模拟器与 S31 宿主机读取同一哈希的文件；宿主机另验证新增用户词的写入、重开。撤掉固定宽度读取后 SIGSEGV，未生成指标文件 | 同上反向命令；`ctest --test-dir build-s31-host -R s31-google-ime --output-on-failure` |
| 5 截图 | 【实测】通过 | 20 张实际键盘 PNG，默认/最大字号、新旧各一套，最大 46,329 B；链接见下 | 同上正向命令，生成 20 张截图；未开窗口 |
| 6 工程与资源 | 【实测】部分通过 | S31 编译通过、编译器警告 0；S31 宿主机 78/78；ASan 7/7。普通全量未全绿，原话及复跑结果见下；仍有范围外设计文档漏项 | 下列构建与测试命令 |

90 条固定用例（首选 / 前五）：

| 引擎 | 全部 90 | 全拼 73 | 简拼 16 |
| --- | --- | --- | --- |
| 旧 | 83 / 89 | 71 / 73 | 12 / 16 |
| 新 | 76 / 85 | 71 / 72 | 4 / 12 |
| 新引擎替换门槛 | 首选 ≥83 | ≥71 / 73 | ≥12 / ≥14 |

另 20 条整句（首选 / 前五）：旧全拼 3/10、3/10，旧九宫格 2/10、3/10；新全拼 10/10、10/10，新九宫格 2/10、5/10。逐条前五和消耗长度在 [QUALITY.md](QUALITY.md)，不以近似句代替目标句。

全量首次原话：`99% tests passed, 5 tests failed out of 438`，`Total Test time (real) = 2013.35 sec`；另有 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 三项跳过。五项失败为 `unit-s31-cell-nat`、`ui-scene-ImeVoice`、`ui-scene-big-ImeVoice`、`small-gaps-1`、`services-mid-capability-doc`。修正无条件分配的 96 B 内部锁后，相关 **11/11** 复跑通过（含前四项、原键盘、词库和文本服务），设计文档项仍失败。没有重跑第二轮 438 项，不能宣称全量全绿。

ASan 实际跑 `ime-lexicon`、`ime-learning`、`ime-keyboard1`、`integration-text-main`、`integration-text-nocard`、`integration-text-baddict`、`ime-google`：**7/7**；最终新增用例再单独复跑 `ime-google` 通过。S31 宿主机最后一轮 **78/78**；原生重开测试连续 4 次内存不增长，分别撤掉拼音树、词条索引、同步数组释放后都在第 1 次重开失败。

构建、复核命令（构建/ctest 均由 `taskpolicy -c utility` 启动；并行数 2）：

```sh
cmake --build build -j2
ctest --test-dir build --output-on-failure -j2
ctest --test-dir build -R '^(ime-|integration-text|ui-scene-(big-)?ImeVoice|small-gaps-1|unit-s31-cell-nat)' --output-on-failure -j2
cmake --build build-asan -j2
ctest --test-dir build-asan -R '^(ime-|integration-text)' --output-on-failure -j2
bash ports/esp32s31/build.sh
cmake -S ports/esp32s31/tests -B build-s31-host
cmake --build build-s31-host -j2
ctest --test-dir build-s31-host --output-on-failure -j2
python3 tests/ime_lexicon/google_host_reverse.py --build build-s31-host
python3 tests/ime_lexicon/google_size.py
```

普通/ASan 配置使用任务总规则的字体、Python、LVGL 路径；ASan 仅另加 `-DINK_SANITIZE=ON`。`gcccheck.py build system/services/text/pinyin.c system/services/text/text_service.c tests/ime_lexicon/google_device.c` 最后结果：`gcccheck: 3 compiled, 0 with findings`。C++ 胶水另由 S31 GCC 编译；上游原文使用 `-w`，未清理其风格。

【实测】相同 S31 配置重链接对照：text **+45,876 B**，data **+24 B**，BSS **+29,552 B**；代码小于 80 KiB。对照只恢复基线的 `pinyin.c` / `text_service.c`，由链接回收未引用新引擎；译文和生成资源保持当前版本，这不是整份基线重新构建。压缩应用资源包前后均 **767,212 B**，净增 **0 B**；新词库没有进应用包。固件只编译，未烧录。

【实测】引擎标记的 PSRAM 动态分配峰值 **2,188,080 B**（含载入文件、搜索结构和用户词库）；S31 的桥接静态区 21,508 B、预测区 8,000 B。【估算】按宿主动态峰值加目标静态区约 **2.22 MB**；目标 32 位结构和 HAL 开销不同，须上机测总峰值。

【实测】本机逐键调用文本服务直到取得候选，含这组输入和两种字号，不含墨水屏物理刷新；共享机器负载会影响墙钟：

| 引擎 | 样本数 | p50 | p95 | 最大 |
| --- | --- | --- | --- | --- |
| 旧 | 853 | 0.185 ms | 0.562 ms | 2.692 ms |
| 新 | 887 | 0.259 ms | 8.269 ms | 23.821 ms |

【估算】若宿主按 3.2 GHz、当前 S31 构建的 320 MHz 计算，再假设 1～3 倍访存差异，新引擎 p95 约 83～248 ms。这是粗略比例，不能当设备时延实测。

截图：

| 输入 | 旧默认字号 | 新默认字号 | 旧最大字号 | 新最大字号 |
| --- | --- | --- | --- | --- |
| `nihao` | [旧](ime-old-normal-nihao.png) | [新](ime-google-normal-nihao.png) | [旧大](ime-old-large-nihao.png) | [新大](ime-google-large-nihao.png) |
| `zhongguo` | [旧](ime-old-normal-zhongguo.png) | [新](ime-google-normal-zhongguo.png) | [旧大](ime-old-large-zhongguo.png) | [新大](ime-google-large-zhongguo.png) |
| `jintiantianqizenmeyang` | [旧](ime-old-normal-jintiantianqizenmeyang.png) | [新](ime-google-normal-jintiantianqizenmeyang.png) | [旧大](ime-old-large-jintiantianqizenmeyang.png) | [新大](ime-google-large-jintiantianqizenmeyang.png) |
| `xx` | [旧](ime-old-normal-xx.png) | [新](ime-google-normal-xx.png) | [旧大](ime-old-large-xx.png) | [新大](ime-google-large-xx.png) |
| `94664` | [旧](ime-old-normal-94664.png) | [新](ime-google-normal-94664.png) | [旧大](ime-old-large-94664.png) | [新大](ime-google-large-94664.png) |

## 没做到的

- 新引擎首选、全拼前五、简拼均未过替换门槛；九宫格“今天天气怎么样”不在前五。未替换旧引擎或缩小词库凑数。
- 普通全量没有全通过：服务设计文档缺 `text.google`；文件范围不允许修改该文档。未上设备测时延和 PSRAM。
- 旧 `text.user_freq` 继续按成员参与新候选排序，旧数据不删；其历史记录只有哈希和计数，无法反向迁成完整谷歌词条。本次未强行转换。
- 2009 年词库更新本次不做，后续可用原建库工具重建。候选结构仍为 40 字节；更长整句会被截断，超出本次 20 句范围，需另行解决，不能据本报告认定任意长句可用。

## 和设计不一致的

- 用户词库 `/me/ime/google.user` 上限 512 条、词条区 16 KiB、回收 20%，比上游小；64 KiB 的文件适配上限覆盖词条、索引、频率。上限由任务授权选定，依据是延续旧词频集合的 512 条规模并限制卡文件缓冲。
- 大字号长句候选在现有候选条里拥挤，截图也保留模拟屏的残影；按分工未改键盘。交 `keyboard-2` 检查长整句候选的布局和刷新。

## 改过的已有测试

- `tests/integration/ai_surface.c` 的设置表末尾追加 `text.google`；原 90 条用例、原质量断言和键盘测试不动，没有放宽预算。
- 新增整句用例曾把“一起”误写成 `yiqu`，最终统一纠正为 `yiqi`，目标“我们一起吃饭”不变，新旧引擎一起重跑。报告仅使用纠正后的结果。

## 要用户定的

无；质量不合格时保留旧默认是任务书已定的分支。可经现有 `settings.set` 按成员开启 `text.google`，卡上需有 `ime/dict_pinyin.dat`。

## 顺手发现

- 首轮 `unit-s31-cell-nat` 在 `at_ok` 中止；复跑及最后 S31 宿主机全套均通过，未改蜂窝代码。
- 任务书所说原有硬断言与基线脚本不完全相同：基线总首选阈值为 75、简拼首选为 10。本任务仍按任务书更高的 83/12 门槛判断替换，原断言不动。
- 重开会暴露上游三处漏释放；均在各自所有者处补释放，见 [SOURCE.md](../../../system/services/text/vendor/googlepinyin/SOURCE.md) 和三项反向对照。

## 新增的机制

- 谷歌解码器、每个数字边界最多 8 条合法音节路径、从同一解码器取预测：任务第 1、2、4 项要求。
- 按成员的 `text.google` 设置与 `/me/ime/google.user`：任务第 3、5 项要求；用户文件格式沿用上游。
- 文件流适配的临时缓冲、OSAL 分配和锁：任务第 1 项要求；复用已有后台加载执行器，没有新增线程、任务、后台重试或持久格式。

胶水共 **382 行**（含本目录 CMake 和现有三个文本文件的改动），另有主/端口构建注册 8 行及译文 1 行；上游改 **127 个行位置**，原文件整体搬入不计入胶水预算。为什么不能更小：直接 stdio/new 的探针不能满足资源统计、文件准入和成员隔离；仅转换一个音节不能组九宫格整句。未用影响其它 C++ 模块的全局分配器替换。

## 要改的文档

服务设计文档 §1.3 的设置表需追加按成员的 `text.google`（布尔、默认关闭、孩子可修改、现有 `settings.set` 入口）。这是 `services-mid-capability-doc` 唯一缺项；已生成能力目录并补 AI 表，但没有越界编辑服务设计文档。

## 上机核对清单

- 卡上装同哈希词库，分别测全拼/九宫格逐键 p50、p95、最大值及卡写入延迟，核对实际 320 MHz 配置。
- 记录全机 PSRAM 峰值；连续切换成人/孩子 20 次后常驻占用应稳定；确认两人词库、重启恢复和密码不变。
- 实屏查看默认/最大字号候选条、长句截断及残影；复核 `94664`、整句九宫格和部分选择，不把宿主机截图当实机通过。

隐私检查：最终提交差异按任务规定表达式扫描，0 匹配（grep 退出码 1）；文件链接、词库哈希及所有入库原文哈希校验通过。未加入设计稿、构建产物或真实设备标识。
