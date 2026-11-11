# ime-reuse-1 执行报告

基线：`fa68b45098b745152ea8fb9a30c7a8727d659305`。分支：`claude/codex-ime-reuse-1`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 控件、键表、九宫格与隔离 | 【实测】专项与反向对照通过 | 专项、成员重启及 18 张帧图通过；移除九键匹配后失败 3 条，另外三项既有反向对照仍失败 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ime_keyboard1.py --build build` |
| 2 全量、字号、S31 | 执行中 | 普通构建成功；全量 ctest 运行中；字号静态检查无违规 | `taskpolicy -c utility cmake --build build -j2` |
| 3 前后截图与性能 | 【实测】已采集 | 24 张 PNG，单张最大 29708 字节；性能见下表 | `tests/ime_keyboard1.py --build build`；[截图清单](screenshots.json) |

## 没做到的

普通全量未通过：`99% tests passed, 5 tests failed out of 391`，383 通过、5 失败、3 跳过。4 项是下述翻译／目录同步；`integration-dictation` 的 AI 隐藏输入场景有 3 条失败（等待进入监听、人可听写、回到键盘），独立新输出目录也复现。依三次止损规则保留失败，不改听写引擎。内存检查见最终记录。

## 和设计不一致的

【实现】LVGL 按各行剩余宽度分配像素：同一行等宽键最多差 1 px，跨行字母最多差 2 px；C12 的同一基本宽度有这个取整差异。数字区和九键均采用三行 163 高的数字区，另一个同事件桥的 LVGL 键盘承载普通高度的功能键（数字两行、九键一行）；其余字体、边框和间距相同。

## 改过的已有测试

键盘点击定位由子按钮／label 改为 native matrix 的命中框，原功能断言不变：`integration/textinput.c`、`integration/dictation.c`、`weather/weather.c`、`ui_input_host.c`、`ui_shell_scenes.c`、`ui_shell_regressions.c`、`semantics_shell2/keyboard.c`。英文界面采集器两份增加读取 matrix 的可见键名。`ime_keyboard1` 保留原功能检查，补充几何、九键、学习、隔离与重启；截图数从 6 增至 18，场景结束由 35 秒增至 150 秒以容纳新增截图；未加 sleep。图标断言改成检查 matrix 的主题图标绘制及保留的可访问键名。`ui_input_host` 的坐标诊断显式转成 int，适配 GCC 的 int32_t 类型；未改条件。

## 要用户定的

暂无。只实施任务书已授权的可逆改动。

## 顺手发现

【实测】听写日志显示存在 UI／点按时序的排查方向；尚未证明根因，不能归因给语音服务。复查时应同时记录点击前顶层页面、命中的控件和能力回执。没有为它加 sleep 或后台重试。

## 新增的机制

新增设置 `text.nine_key`（成员级布尔值）：任务第 4 条要求；沿用现有设置文件。其余没有新增任务、状态机、文件格式、缓存或索引。

## 要改的文档

`docs/system/capabilities.md`：新增成员设置 `text.nine_key` 后需要由主会话运行 `python3 tools/capabilities.py` 生成；本任务按总规则不修改共享目录。

`assets/i18n/en.tsv` 及对应标记：需补“九宫格拼音”“全键”“九键”三项，并把源码这三处标为 TR_NOOP/TR。英文键帽宜用短标签（例如 26 / 9）以保持大字号尺寸。任务允许列表未包含翻译表，所以保留可用的中文原名并报告失败，未放宽 i18n 检查。

## 上机核对清单

由主会话验证真机打字、全拼与九宫格切换、按键到出字的时间；本任务不接触设备。

## 第一里程碑

【实现】按键区已换成 `lv_keyboard` 自定义键表，移除默认 textarea 回调；所有按键和选词仍经过 `keyboard.key` / `keyboard.pick`，没有增加能力或 AI 编辑通道。主题末尾增加键盘令牌块，键高随系统文字行高变化。语音键放候选条最右侧，保留已有听写入口的位置和可见候选空间，宽 91、高随普通功能键一起变化；与删除键的整数取整差异待最终量表核对。

【实现】拷贝 kiikoread `83942eb` 的两个完整函数，保留 Apache-2.0 声明和原许可证；无词库转换、缓存或新索引。九键只读遍历 IKD1 的原键表，以消耗长度、原词频排序，再沿用成员学习。`94664` 是 `zhong`；词组未输完时允许补全“中国”，`946644862` 选“中国”保留 `2`。

【实测】宿主机每种查询 100 次，单位微秒（全拼与九键分别输入各自组合串的前 1～6 位；不代表真机性能）：

| 位数 | 全拼均值 | 全拼最大 | 九键均值 | 九键最大 |
| --- | --- | --- | --- | --- |
| 1 | 0.170 | 2.000 | 569.700 | 786.000 |
| 2 | 0.210 | 1.000 | 333.940 | 664.000 |
| 3 | 0.290 | 1.000 | 310.640 | 562.000 |
| 4 | 0.380 | 1.000 | 284.770 | 541.000 |
| 5 | 0.600 | 1.000 | 312.690 | 481.000 |
| 6 | 0.850 | 2.000 | 341.710 | 425.000 |

【实现】既有全链路输入测试要求按键到可见小于 800 ms；没有独立的全拼查询耗时上限。九键扫描比二分全拼慢，宿主机本次最大值不足 1 ms；不能用它保证 S31 的时延。

## 第二里程碑

【实测】S31 宿主机：`100% tests passed out of 51`。命令：`taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host`，构建 `-j2`，ctest `-j2`。GCC：`gcccheck: 12 compiled, 0 with findings`（本任务改过的 3 个产品 C 文件及 9 个测试 C 文件）。四项反向对照均退出 1，且命中对应失败断言，见 [validation.json](validation.json)。

【实测】胶水累计 +234 / -109，净增 **125 行**，不含拷入的 18 行上游头文件、测试和文档；上限 350。删除手工键帽／排键 73 行；keyboard.c 总删除 107 行。超过 100 行的原因：需同时桥接六种键表、维持候选／语音／密码语义，并提供只读九键查询与成员设置。更小的两种方案已核查：直接开 lv_ime_pinyin 会丢 IKD1 词组和学习；保留手排按键不符合本任务。没有采用两者。

### 截图

基线键盘来自 `fa68b450`，所有图均由无窗口模拟器采集，未经设计稿替换。常规为字号 1（100%），大字号为字号 3（120%）。基线没有九键，“九键前”展示当时实际的全键盘，不代表已有九键。

| 键表 | 常规前 | 常规后 | 大字号前 | 大字号后 |
| --- | --- | --- | --- | --- |
| 拼音 | [before/normal](before-pinyin-normal.png) | [after/normal](after-pinyin-normal.png) | [before/large](before-pinyin-large.png) | [after/large](after-pinyin-large.png) |
| 英文 | [before/normal](before-english-normal.png) | [after/normal](after-english-normal.png) | [before/large](before-english-large.png) | [after/large](after-english-large.png) |
| 符号 | [before/normal](before-symbols-normal.png) | [after/normal](after-symbols-normal.png) | [before/large](before-symbols-large.png) | [after/large](after-symbols-large.png) |
| 数字 | [before/normal](before-number-normal.png) | [after/normal](after-number-normal.png) | [before/large](before-number-large.png) | [after/large](after-number-large.png) |
| 九键 | [before/normal](before-nine-normal.png) | [after/normal](after-nine-normal.png) | [before/large](before-nine-large.png) | [after/large](after-nine-large.png) |
| 密码 | [before/normal](before-password-normal.png) | [after/normal](after-password-normal.png) | [before/large](before-password-large.png) | [after/large](after-password-large.png) |
