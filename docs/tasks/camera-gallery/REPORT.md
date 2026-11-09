# 相机、相册和看图任务报告

基线：`558c7ba8604fc321073b943716f49512a7dc3690`。仅本地开发、提交；不接设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 真实触摸与 AI 各保存一张 VGA 彩色 PNG；落卡、索引、相册及跨进程读取均核查 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/camera_gallery/run.py --build build --visual` |
| 2 | 【实测】通过 | 10 张照片两页、九宫格；硬件 NEXT 与 AI 上一页；实际 LVGL 缩略图尺寸和逐像素灰度核查 | 同上 |
| 3 | 【实测】通过 | 原尺寸 16 灰阶看图；AI/触摸删除共用 `file.delete`，人确认、取消保留、AI 自批拒绝、卡和索引同步删除 | 同上 |
| 4 | 【实测】通过 | 孩子看不到大人照片、已知编号也不能打开；孩子拍照只进自己目录。撤掉页面归属检查后相同场景退出 1、3 条断言失败 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/camera_gallery/run.py --build build --negative` |
| 5 | 【实测】通过 | 三页完整快照均适配实际 AI 的 3072 字节缓冲；真实触摸与 AI 走同能力 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/camera_gallery/run.py --build build` |
| 6 | 【实测】未全过；行数通过 | 普通与 ASan/UBSan 全量各 233 项：226 通过、同样 4 失败、3 被既有脚本标记跳过，均退出 8；无 sanitizer 错误；产品 349/400 行 | `taskpolicy -c utility cmake --build build -j2`；`UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；ASan 换 `build-asan`、配置加 `-DINK_SANITIZE=ON` |

## 没做到的

实机验证没做，任务禁止接设备。扫码结果不在本任务范围。

- 两轮全量均未全绿。按任务“共用的文件只在末尾加行”和总规则第 7 节禁止修改共享能力文档，没有改三份现有测试的内部固定清单或预期；具体失败如下，需主会话合并时核对。
- Linux/GCC 没在这台 Mac 上执行，不能将本机 clang 的 `-Werror` 结果当 Linux 通过。
- 原有三项 hub 测试被各自脚本标记跳过，本任务没有新增 skip 或隐藏跳过。
- “问这张图”接既有 AI 面板和当前页上下文；本任务没有新增图像上传或视觉推理，也没有验证模型能识别照片。Camera 快照提供取景状态和动作，未提供未保存帧的完整像素读取能力。

## 和设计不一致的

【实现】画板照片是示意图；本任务使用模拟相机的测试图案和合成图片。按现有主题保留实际字号、边距与灰色；照片按原比例完整显示，不裁成长幅。取景沿用静态 HAL，每 3 秒尝试更新，采集期间不重复提交；不承诺视频帧率。没有扫码按钮和“更多”按钮；扫码结果由后续任务实现，“更多”没有已实现的额外动作。

【实测】`tools/ui_compare.py --reuse`：Camera **110.143**、Gallery **73.001**、Photo **81.205** /255，均超过 12。Camera 的色条与 Photo 的横幅 VGA 图和画板的长幅月亮/山景内容不同，留白明显；Gallery 的黑白像素测试图不同于画板示例，且没有虚构“今天”分组。分页沿用 F4 的位置；边距、圆角、灰、按钮大小使用当前主题，和未同步放大的画板也有差异。已逐页看过实际截图；没有为了分数换成设计稿图片或缩小规格。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/camera-gallery-compare --scenes Camera Gallery Photo --reuse`。

## 改过的已有测试

无。所有新增断言都在 `tests/camera_gallery/`。

## 要用户定的

保留现有 `camera.capture` 的内存采集能力；新增 `camera.photo` 是页面和 AI 共用的“拍照并存储”能力。是否将来合并两者，由主会话复核；本任务只在共用文件末尾增量接线。

## 顺手发现

普通与 ASan/UBSan 全量的同四项失败均由本任务注册的新页面/能力触发，不冒充基线失败：

| 检查 | 证据及需要主会话处理的地方 |
| --- | --- |
| `capability-table-doc` | 基线单独核查为 324 定义通过；当前目录应重新生成，按任务禁止修改。新目录单独生成到 `build/camera-capabilities.md` 并校验。 |
| `integration-ai-surface` | `tests/integration/ai_surface.c:204` 的固定表缺 5 个能力，另有 `:175` 把未知 `photo.open` 当无参数，共 6 条断言失败。需在 FOR_AI 表末加入五项，`photo.open` 必需 `id`。 |
| `integration-apps` | `tests/integration/apps.c:146` 固定预期 11 个应用，实际 13；增加的恰是 camera、gallery。需按最终合并应用数更新。 |
| `ui-snapshot-regressions-metalio-plus` | `tests/ui_shell_regressions.c:480`、`:482`、`:483`、`:486`、`:490` 共 5 条：相机/相册成为已注册应用后，“工具甲”从首屏移到后页，旧首屏点击及搜索起始页预期失效。需更新真实页面顺序的测试流程；没有放宽分页或可见性断言。 |

## 新增的机制

【实现】任务要求的低频取景：一个仅随 Camera 页面存在的 LVGL 定时器；工作复用 MEDIA 执行器。任务要求的照片文件：PNG，复用仓库 stb_image_write v1.16（MIT / Unlicense），无新依赖、持久配置、常驻任务、缓存或后台重试。

## 要改的文档

主会话需更新生成的 `docs/system/capabilities.md`（新增五个能力）及 `docs/system/ui-dependencies.md` 对应状态。本任务按总规则第 7 节不改共享文档。`third_party/stb/README.md` 中“只用于模拟器截图”的说明已不覆盖本任务新增的可移植编码入口，需主会话统一更新。

## 里程碑

- 【实现】已完成三个页面、F4 九宫格、`file.thumb` 接线、16 灰阶显示、成员过滤、删除确认及启动注册；共享产品文件只在末尾增量增加。
- 【实测】普通 `-Werror` 完整构建退出 0；PNG 编码对象的 macOS `__TEXT` 为 11,384 字节（`size build/CMakeFiles/ink_camera_png.dir/system/apps/camera/png.c.o`），不是设备固件大小。全部编码分配走 PSRAM。
- 【实测】定向场景内部 RAM 最低余量 47,840 字节，MEDIA 标记 PSRAM 峰值 2,923,976 字节，相机保留帧峰值 614,416 字节，PSRAM 最低余量 9,786,472 字节；内部与外部失败分配均为 0（含分配器开销、基于模拟器，非实机）。
- stb_image_write.h SHA-256：`cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05`，复用既有文件、许可证未改。
- 第三方来源：`https://github.com/nothings/stb`，仓库固定提交 `d14d46f6b44456eb88beac58cba833a4135d4c89`，头文件末尾保留 MIT / Unlicense 双许可。
- 【实测】首轮定向测试发现两处测试夹具错误：索引顺序不等于目录名字顺序；Plus 档案没有 BACK 实体键（模拟器明确忽略）。已按真实索引计算预期灰度，返回改为触摸真实顶栏按钮；未改已有测试或产品输入行为。
- 【实现】删除按钮直接绑定 `file.delete` 及实际文件参数，与快照里的 AI 动作完全相同；删掉最初的转发包装，避免把等待确认的结果变成仅表示排队的回执。
- 【实测】定向四轮均退出 0：Camera、Gallery、Photo 各 156 项断言，restart 22 项；证据 `build/camera-gallery-a2zbg0vx/`，命令输出 `build/camera-targeted.log`。Python/Pillow 另外验证卡上文件确为 640×480 RGB PNG，保留彩色信息。
- 【实测】产品代码按最保守算法计 349 行：新目录 317 行（含头文件、编码包装）、服务增量 19 行、boot 2 行、CMake 11 行（含测试接线），未超过 400。第三方源码是已存在的 stb 文件，没有新增第三方行。
- 为什么不能更小：三个页面的 F1/F4 声明、异步文件与图片处理、像素所有权、成员核对、存储及能力接线共同构成完整链路。看过只展示 `camera.capture` 的内存帧或只画图标的更小方案，分别缺落卡和真实缩略图；没有采用。
- 【实测】反向对照的编译、链接退出 0，变异程序退出 1，151 条通过、3 条失败、finished=1；脚本验证期望失败后退出 0。证据 `build/camera-gallery-wjx60d8o/`；正常产品源码、可执行文件没有改。普通全量日志 `build/camera-ctest.log`，307.12 秒，新增 camera-gallery 通过。
- 【实测】ASan/UBSan 配置及完整构建退出 0；全量 651.14 秒，camera-gallery 通过，无 AddressSanitizer、UndefinedBehaviorSanitizer 或 runtime error 报错。确认测试编译参数含 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`；日志 `build-asan/camera-build.log`、`build-asan/camera-ctest.log`。两轮 layering、determinism、determinism-reader 均通过。
- 配置沿用任务书字体/Python/LVGL 参数；额外只读复用 `$INK_DEPS/build/_deps/` 下已有 SDL2、ImGui 源码，普通与 ASan 配置一致，未下载或升级依赖。所有构建/测试均为 utility，最大并行 2，无窗口。
- 【实测】最终仅为三处单行 `if` 补上花括号，按任务的 GCC 书写规则消除歧义，没有改变行为或断言。两种构建的定向目标再次编译成功，各 156 + 22 条断言通过；日志 `build/camera-final-targeted.log`、`build-asan/camera-final-targeted.log`，证据 `build/camera-gallery-c_1tixiz/`、`build-asan/camera-gallery-8nvqhatk/`。
- 【实测】`git diff --check` 退出 0；基线到提交的 `git diff --stat` 仅 11 个相关文件。任务书第 6 节隐私扫描无匹配（grep 退出 1），未提交设计稿、构建产物或禁止保存的信息。所有代码和报告只作本地提交，未推送。
