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
| 6 | 【实测】除用户豁免的三项共用清单外通过 | 普通与 ASan/UBSan 全量各 233 项：230 通过、3 项豁免失败、0 跳过，均退出 8；无 sanitizer 错误；产品 378/400 行 | `taskpolicy -c utility cmake --build build -j2`；`PYTHONPATH="$PWD/hub/src" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；ASan 换 `build-asan`、配置加 `-DINK_SANITIZE=ON`；详见文末 |

## 没做到的

实机验证没做，任务禁止接设备。扫码结果不在本任务范围。

- 首次交付的两轮全量均未全绿。本轮已修页面回归；普通全量只剩应用个数、AI 能力清单、能力表文档三项，按用户补充由主会话合并时对齐，没有修改它们的固定清单或预期。
- Linux/GCC 没在这台 Mac 上执行，不能将本机 clang 的 `-Werror` 结果当 Linux 通过。
- 首次交付的三项 Hub skip 本轮已查明依赖并处理，三项均已实际通过，详见“复查后的修复”。
- “问这张图”接既有 AI 面板和当前页上下文；本任务没有新增图像上传或视觉推理，也没有验证模型能识别照片。Camera 快照提供取景状态和动作，未提供未保存帧的完整像素读取能力。

## 和设计不一致的

【实现】画板照片是示意图；本任务使用模拟相机的测试图案和合成图片。按现有主题保留实际字号、边距与灰色；照片按原比例完整显示，不裁成长幅。取景沿用静态 HAL，每 3 秒尝试更新，采集期间不重复提交；不承诺视频帧率。没有扫码按钮和“更多”按钮；扫码结果由后续任务实现，“更多”没有已实现的额外动作。

【实测】`tools/ui_compare.py --reuse`：Camera **110.143**、Gallery **73.001**、Photo **81.205** /255，均超过 12。Camera 的色条与 Photo 的横幅 VGA 图和画板的长幅月亮/山景内容不同，留白明显；Gallery 的黑白像素测试图不同于画板示例，且没有虚构“今天”分组。分页沿用 F4 的位置；边距、圆角、灰、按钮大小使用当前主题，和未同步放大的画板也有差异。已逐页看过实际截图；没有为了分数换成设计稿图片或缩小规格。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/camera-gallery-compare --scenes Camera Gallery Photo --reuse`。

## 改过的已有测试

原任务无。本轮修改 `tests/ui_shell_regressions.c`：按相机能力可用性核对第一页和第二页的真实项目；先翻到“工具甲”所在页再触摸；搜索后翻到实际结果页再核对过滤。分页、可见性、触摸和 AI 一致性断言均保留，详见“复查后的修复”。新增相机断言仍在 `tests/camera_gallery/`。

## 要用户定的

保留现有 `camera.capture` 的内存采集能力；新增 `camera.photo` 是页面和 AI 共用的“拍照并存储”能力。是否将来合并两者，由主会话复核；本任务只在共用文件末尾增量接线。

## 顺手发现

首次交付的普通与 ASan/UBSan 全量有同四项失败，均由本任务注册的新页面/能力触发，不冒充基线失败。下表保留当时原因；本轮页面回归已修，其余三项按用户补充留给主会话：

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

## 复查后的修复

复查基线：`85d453aa`。按主会话 2026-11-09 的三项意见修复；下面记录取代上面对应的旧测试状态。

| 编号 | 修复与测试 |
| --- | --- |
| CG-L1 | 【实现】`ink/sys/camera.h` 正式声明 `ink_camera_read_rgb(int *, int *)`，返回调用者释放的 PSRAM RGB888；应用不再自行声明，也不传 `ink_call_t`。服务内的采集函数由公开接口和原能力复用。测试直接调用公开接口，核查 VGA 尺寸、RGB 像素及采集失败回执；恢复原私有声明的反向对照因类型冲突编译失败。 |
| CG-L2 | 【实现】保存前只枚举一次照片目录，从现有最大编号继续；保留独占创建。测试准备 300 张已入索引的照片，分别在首次启动及新进程重启后拍摄，核查新增编号 301/302、数量和独占文件打开次数不超过 2。恢复从 1 试探的反向对照被同一断言捕获。 |
| CG-L3 | 【实现】用 OSAL 阻塞互斥锁替换原让出 CPU 的自旋，采集和 RGB 复制在同一临界区；回收只尝试锁，原 busy 标志防止分配时回收自己的帧。测试覆盖锁竞争、零 yield、拍摄中不能回收、复制后可以回收、失败后解锁。恢复忙等的反向对照被同一断言捕获。 |

- 【实测】原有页面回归在 Plus 和 desktop-lcd 均通过：相机/相册注册后“工具甲”移到第二页，测试按真实能力可用性翻页，再核查可见项、触摸启动和 AI 搜索；没有修改产品导航或放宽分页断言。原三项 Hub skip 的原因是通用字体 Python 缺 `pytest`、`pytest_asyncio`、`aiohttp`、`yaml`，CMake 改选已有配置 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，并以 `PYTHONPATH="$PWD/hub/src"` 指向本工作树。该环境版本为 pytest 8.4.2、pytest-asyncio 1.2.0、aiohttp 3.12.15、PyYAML 6.0.3；三项及能力导出前置检查全部通过（6/6，含两项页面回归），日志 `build/camera-review-prior-failures.log`。
- 【实现】没有新增任务、定时器、配置、队列或文件格式；互斥锁替换原帧自旋锁，仍复用 MEDIA 执行器。共享能力/应用清单及文档按用户补充留给主会话；不改共享资料。
- 【实测】300 张夹具首次建立媒体索引时触发既有 15 秒能力超时；改为先准备“已入索引”的现有相册再测拍照，没有调整超时或索引实现。首次建索引逐项落盘慢是既有文件索引问题，本次未扩展修复。
- 【实测】最终定向测试 238 条断言全部通过：Camera 156、restart 22、service 22、many 19、many-restart 19。两次大相册拍摄的 `test.camera.file_opens` 均为 1；证据 `build/camera-gallery-n9d_adv_/`。命令：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/camera_gallery/run.py --build build`。
- 【实测】同脚本的 `--revert api` 恢复私有签名，编译退出 1；`--revert names` 恢复从 1 试探，实际打开 301 次、18 通过/1 失败；`--revert wait` 恢复忙等，21 通过/1 失败，均命中对应断言。`--negative` 的成员隔离反例仍为 151 通过/3 失败。包装脚本均在确认预期失败后退出 0，未改工作树源码；日志 `build/camera-review-final-controls.log`，反例证据依次为 `build/camera-gallery-9jew8vd_/`、`build/camera-gallery-jjdv45vm/`、`build/camera-gallery-cpfhxogk/`、`build/camera-gallery-6_tfzd0_/`。
- 【实测】普通 `-Werror` 完整构建退出 0；全量 233 项，230 通过、3 失败、0 跳过，204.11 秒，退出 8。只剩用户明确豁免的 `capability-table-doc`、`integration-ai-surface`、`integration-apps`；没有排除或跳过它们。命令：`taskpolicy -c utility cmake --build build -j2`；`PYTHONPATH="$PWD/hub/src" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。日志 `build/camera-review-build.log`、`build/camera-review-ctest.log`。
- 【实测】产品代码按基线至本轮的新增行数保守计算为 **378/400 行**（新应用目录 323、服务 38、公开头文件 4、boot 2、CMake 11；删除行不抵扣）。本轮产品文件新增 39 行、删除 23 行；没有新增第三方代码。`git diff --check` 通过；任务书隐私扫描无匹配（退出 1）。
- 【实测】ASan/UBSan 的配置、完整构建退出 0；全量 233 项，同样 230 通过、3 项豁免失败、0 跳过，416.12 秒，退出 8。命令与普通构建、全量相同，构建目录换 `build-asan`，配置加 `-DINK_SANITIZE=ON`；已核对服务及相机测试编译参数均有 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`。完整构建日志、CTest 输出和 `Testing/Temporary/LastTest.log` 均无 AddressSanitizer、UndefinedBehaviorSanitizer、LeakSanitizer 或 runtime error 诊断。两轮的 camera-gallery、三项 Hub 测试、两档页面回归、layering、determinism、determinism-reader 全部通过；日志 `build-asan/camera-review-build.log`、`build-asan/camera-review-ctest.log`。
- 【实测】最后仅将 `--revert api` 的编译诊断匹配改为兼容 GCC/Clang 引号的关键词匹配，随后该反向对照和 Python 语法检查均退出 0；不影响默认全量路径或产品。最终复查涉及 7 个相关文件，原有 `REVIEW.md` 保持未改、未加入提交；没有构建产物或设计稿。
