# keyboard-2 执行报告

基线：`d7bc63aeee29ce9280cb15bc9c866d5e400ef4ca`；产品代码提交：`47607d4f`。**未达到全部验收通过**，未完成项保留给复核。

`PY=$INK_DEPS/build-deps/pyenv/bin/python`，`CTEST=$INK_DEPS/build-deps/pyenv/bin/ctest`。命令均加 `taskpolicy -c utility`；构建最多 `-j2`，ctest 最多 `-j2`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 页面与能力 | 部分通过 | 【实测】触摸、AI 各 14 张图，操作断言通过；第二轮草稿等待超时，整项失败。九键读音选择未做 | `$PY tests/ime_keyboard1.py --build build --pages` |
| 2 规格与缩放 | 部分通过 | 【实测】行宽、点击范围、默认／最大字号检查通过，ui-scale 0 违规；间距未随字号放大 | `$PY tools/ui_scale_check.py`；`$CTEST --test-dir build -R '^ime-keyboard1$' --output-on-failure -j2` |
| 3 刷新 | 部分通过 | 【实测】十字母不刷新键区、无整屏，切布局刷新键区；强制每键重画键区的反向版本被检出。未做其他区域的完整面积白名单 | `$PY tests/ime_keyboard1.py --build build --refresh-reverse` |
| 4 长按删除 | 通过 | 【实测】真实指针：按住连删，释放、移出停止 | 同 1 |
| 5 密码 | 通过 | 【实测】无候选、快照无正文、AI 输入被拒绝、最近符号不记密码、成员隔离；【实现】沿用不记录能力参数的日志通道 | 同 1；`system/policy/cap.c` |
| 6 截图 | 通过 | 【实测】64 张，最大 44,134 字节；六种布局改前／改后／108，均有默认及最大字号 | [截图索引](README.md)；`--baseline-pictures`、`--key-height108` |
| 7 构建与预算 | 部分通过 | 【实测】普通构建过，全量未过；最终键盘 20/21、ASan/UBSan 90/91；S31 宿主机 77/77、固件编译过、编译警告 0；增量 6,512 字节；GCC 4 文件 0 发现 | 下述命令；[validation.json](validation.json)、[firmware-size.json](firmware-size.json) |

普通全量原话：`97% tests passed, 12 tests failed out of 439`，注册 440 项；3 项 Hub 测试跳过，基线已禁用 `memo-save-busy`。全量耗时 1328.12 秒。修正定位和能力表后重点复测；第二次全量重跑中止，未计作全量通过。最终 21 项键盘测试仅 `keyboard-pages` 失败；另复测 `i18n-memo`、`sms-multipart`、`autotest-cap`，前者通过，后两者失败。

ASan/UBSan 跑了 91 项：两项键盘、三项 `integration-text-*`、两项听写、键盘语义、`ui-input-host`、`audit9-ime-layout`、全部 `ui-scene-*`。原话：`99% tests passed, 1 tests failed out of 91`；唯一失败为草稿等待，完整日志无 ASan/UBSan 内存诊断。

```sh
cmake --build build -j2
$CTEST --test-dir build --output-on-failure -j2
cmake --build build-asan -j2
$CTEST --test-dir build-asan -R '^(ime-keyboard1|keyboard-pages|integration-text-.*|integration-dictation(-handsfree)?|semantics-shell2-keyboard|ui-input-host|audit9-ime-layout|ui-scene-.*)$' --output-on-failure -j2
cmake -S ports/esp32s31/tests -B build-s31-host
cmake --build build-s31-host -j2
$CTEST --test-dir build-s31-host --output-on-failure -j2
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" bash ports/esp32s31/build.sh
$PY "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/ui/shell/keyboard.c tests/ime_keyboard1.c tests/integration/ai_surface.c tests/audit9/ime_layout.c
```

普通配置用任务书参数；ASan 只换目录并加 `-DINK_SANITIZE=ON`。S31 使用相同资源，替换基线 `keyboard.c`、`en.tsv` 编译，再恢复重编：4,270,208 → 4,276,720 字节，增量 6.36 KiB；三次退出 0，源码恢复校验通过。未提交构建产物。

产品 C／头文件新增 **382 行**、删除 58 行；计入 CMake、翻译、设备脚本共新增 **402 行**，低于 600。为什么不能更小：分类、分页、最近符号、长按弹层和六个能力需界面与参数；旧符号键表、候选换行列表不能完成这些操作。复用现有 LVGL 键盘、按钮、设置、文本服务，保留听写和编辑。

## 没做到的

- 九键读音列表与筛选未实现：公开文本接口仅有编码及候选，未越过服务调用内部引擎。屏幕隐藏编码，AI 快照保留底层组合串。
- 新页面测试依次增加刷新等待、指针收发确认、草稿及存储等待，第二轮等待仍超时；按三次止损规则保留失败。两轮未另做结果逐字比较；电话步骤用合成 `INK_TEXT_PHONE` 会话，未打开真实电话应用。
- 全量未全绿，最终保留 `keyboard-pages`、`sms-multipart`、`autotest-cap` 失败；未把后两项认定为基线问题。
- 刷新未做完整面积白名单；400 ms 整理节奏只有【实现】证据，没有独立时间断言。
- 上滑未做；分类仅中文、数学、英文，未实现单位、特殊、序号等分类。未移植表情、剪贴板历史、手写面板及其运行时。
- 多指取消长按未专门实现或实测；只验证释放、移出。

## 和设计不一致的

- 候选固定三列两行、长词换行；上游按文字宽度安排网格。数量和顺序由文本服务决定。
- 字与键高随字号变化，间距沿用原令牌；多字符省略号／破折号归为单字符。展开箭头复用 `next` 图标旋转，避免字体缺少下三角。
- 读音及未移植面板见上；来源、行号、哈希见 [SOURCE.md](SOURCE.md)。

## 改过的已有测试

- `ime_keyboard1.c` 适配符号面板定位、数字底行偏移；业务断言保留。截图期望 20 → 22，补两个字号的候选展开。
- `keyboard_matrix.h` 将“更多／字母”定位到箭头／26，经真实面板、分类、分页能力找原符号与数字键；`audit9/ime_layout.c` 仅映射键帽，业务断言未改。
- `ai_surface.c` 在终止项前追加六能力及必填参数。首次误放终止项后，已修正并复测通过。
- 手动基线模式新增展开图，仅该新增图不检查已隐藏字母键；原图检查保留。此模式只取图，不作修复通过证据；普通 ctest 不读历史。

## 要用户定的

默认保持 96。108 的两个字号截图已齐，最大字号进一步压缩页面空间，见截图索引。

## 顺手发现

- 图标检查器对通用局部指针别名递归膨胀；新增变量用专用名字后恢复，未改检查器。
- 备忘初始保存期间第一下点按曾未提交；等待存储后输入步骤通过，第二轮稳定等待仍超时。未扩改备忘或文本框。
- 【实现】普通中文框数字页仍受文本服务标点规则影响，小数点可能转句号；未专测。固定数字／电话字段的布局切换受输入类型优先级限制。

## 新增的机制

- 六个键盘能力：任务第 4 项要求，按钮与 AI 共用。
- `keyboard.recent0/1` 两项成员整数设置：任务第 5 项要求，各存四个标识，复用既有格式。
- 候选／符号页与弹层状态：任务第 1 项要求。
- 一个暂停式 LVGL 计时器：Mogo 规则要求，候选变三次后停手 400 ms 整理词栏，切布局、结束输入即暂停。
- 原生事件前后抑制不变键帽的失效请求：任务第 3 项要求。新增任务／线程、持久文件格式、通用框架均为 0。

## 要改的文档

- `docs/system/capabilities.md` 已按本任务例外重新生成。
- 主会话汇总共享 backlog、UI 依赖：读音接口、字号间距、数字页标点、未过测试；本任务未改这些共享文件及需求、决策。
- 按任务书第 6 节隐私表达式检查提交差异，提交前后均为 0 命中。文件、链接、图片大小、修改范围检查通过。

## 上机核对清单

未连接设备、烧录、开启串口。主会话核对：连打按键到出字时间、实际刷新范围与停手整理；长按备选及取消；删除释放／移出／多指停止；`94664` 选“中国”和连续翻页；两个字号及 96／108 的遮挡。读音接口补齐后另验读音筛选。
