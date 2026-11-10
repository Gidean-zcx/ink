# lock-camera 任务报告

基线：`ec78378e05f2694b9fa410780ab2bcf9d8de1c99`。分支：`claude/codex-lock-camera`。状态：本次开发与验证结束；验收 2 未全过，两项共享文档校验需合并会话更新。以下证据路径相对仓库根目录，构建产物未提交。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】模拟器亮屏锁定、成员门及密码页通过；浅睡边界见下文 | lock 110、gate 138、kid 129，共 377 条断言；从已打开的旧相册锁屏进入也覆盖；两张新照落卡、本次回看、旧照/文件/其他应用拦截、真实 HOME、解锁相册、儿童归属、AI 快照均核查 | 下方定向命令 |
| 2 | 【实测】部分通过；未解决的仅两项共享文档校验 | 原有测试未改；最后修改后普通与 ASan/UBSan 均 14/14，通过 S31/GCC；全量原话见下文 | 下方全量、S31、GCC 命令 |
| 3 | 【实测】通过，逐张看过最终布局 | 下表截图；大字号布局检查原样通过 | 定向命令及 `ui-scene-big-LockPage` |

| 画面 | 截图路径 |
| --- | --- |
| 默认锁屏入口 | `build/lock-camera-t5zfqf4i/lock/snap-lock-entry.png` |
| 受限取景 | `build/lock-camera-t5zfqf4i/lock/snap-restricted.png` |
| 本次第二张的拍摄确认 | `build/lock-camera-t5zfqf4i/lock/snap-confirmation.png` |
| 成员门、密码页入口 | `build/lock-camera-t5zfqf4i/gate/snap-member-entry.png`、`build/lock-camera-t5zfqf4i/gate/snap-pin-entry.png` |

【实测】普通全量原话：`99% tests passed, 2 tests failed out of 318`，430.11 秒，退出 8。失败为 `capability-table-doc`、`integration-ai-surface`，均因共享能力文档缺新能力；日志 `build/lock-final-ctest.log`。

【实测】ASan/UBSan 全量原话：`99% tests passed, 3 tests failed out of 318`，672.48 秒，退出 8。除上面两项，还有当时的 `ui-scale` 不识别 `LV_MAX`；已改成等价主题算式，未添加豁免。最后的相册清理与保存期间唤醒修改后，两目录重新构建并补跑相关 14 项：普通 `100% tests passed out of 14`（29.17 秒）、ASan/UBSan 同为 14/14（60.04 秒），退出均为 0。构建、全量、最终回归及 LastTest 日志无 ASan/UBSan/LeakSanitizer 诊断。日志 `build-asan/lock-ctest.log`、两目录的 `lock-final-regressions.log`。

【实测】四组反向对照只编译临时变异程序，未改产品源码。下表变异程序均退出 1、finished=1；包装脚本核对指定断言后退出 0。

| 撤掉的修复 / 参数 | 通过 / 失败断言 | 证据目录 |
| --- | --- | --- |
| 公共归属 `--negative` | 124 / 5 | `build/lock-camera-0816sel5/` |
| 采集前固定模式 `--late-mode` | 128 / 1 | `build/lock-camera-dic2ujrc/` |
| 清理旧相册 `--retain-gallery` | 122 / 7 | `build/lock-camera-suga5gyo/` |
| 保存期间保持唤醒 `--sleep-capture` | 128 / 1 | `build/lock-camera-k3lifg9y/` |

日志分别为 `build/lock-negative.log`、`build/lock-late-mode.log`、`build/lock-retain-gallery.log`、`build/lock-sleep-capture.log`。

【实测】S31 完整编译、链接、镜像生成、size、stack 脚本均退出 0。镜像 1,342,576 字节，DIRAM 88,180 字节，PSRAM 静态 BSS 552,728 字节；日志 `build/lock-s31.log`，镜像 `build-s31/ink-s31.bin`。仅编译，未烧录或运行设备。

【实测】GCC：`gcccheck: 12 compiled, 0 with findings`，退出 0；日志 `build/lock-gcccheck.log`。两种宿主构建均为 `-Werror`，内存构建另有 `-fsanitize=address,undefined -fno-omit-frame-pointer`；已核对产品相机与新测试的实际编译参数。

复现命令（配置沿用总规则第 5 节；ASan 目录加 `-DINK_SANITIZE=ON`，两目录同用本地 SDL2、ImGui 和 Hub Python 缓存）：

```bash
taskpolicy -c utility cmake --build build -j4
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_camera/run.py --build build
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_camera/run.py --build build --negative
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_camera/run.py --build build --late-mode
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_camera/run.py --build build --retain-gallery
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_camera/run.py --build build --sleep-capture
INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub/src" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
taskpolicy -c utility cmake --build build-asan -j4
INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub/src" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build \
  system/apps/camera/camera.c system/apps/gallery/gallery.c system/policy/cap.c \
  system/policy/system_actions.c system/services/input/input_service.c system/ui/app_registry.c \
  system/ui/lock_mark.c system/ui/ui_task.c system/ui/shell/member_gate.c \
  system/ui/shell/member_pages.c system/ui/shell/navigation.c tests/lock_camera/lock_camera.c
```

## 没做到的

- 【实现】当前 HAL 不把触摸作为浅睡唤醒源，模拟器明确丢弃睡眠中的触摸。因此未实现“睡着后直接点相机”；须先按唤醒键。本次不宣称已验证实机触摸唤醒。
- 【实测】采集中的真实 HOME 短按曾未送达输入驱动。拍完后的 HOME 用真实按键通过；采集中退出另用同一 `system.home` 能力核对照片归属、回锁屏和延迟回执。未修改输入驱动。
- 没有运行 Linux 主机全量或实机测试；本机为 macOS，另跑了提供的 GCC 检查及 S31 交叉编译。
- 两项共享能力文档校验未通过；任务明令不改该生成文档，留给合并会话。最终独立尺寸检查为 `ui-scale: 49 files, 0 violations`。

## 和设计不一致的

【实现】规范 §3.5 仍写“相机没有入口、锁屏不响应触摸”，与任务 D-121 不一致；本次按任务实现入口，没有改规范迁就实现。受限相机沿用 Camera 组件与低频取景，左下角只回看本次，点画面恢复取景。默认入口放在原提示条内，保留电源键、AI 键和“正在听”的完整提示，并按文字行高容纳大字号。

## 改过的已有测试

无。只新增 `tests/lock_camera/` 和 CTest 接线。`docs/system/app-framework.md` 仅更新两个源码行号链接，正文未改。

## 要用户定的

无。任务已授权选择独立公共目录 `/family/锁屏拍的照片`，不属于任何家人，解锁后的各成员相册均可见。写公共目录沿用现有大人权限，只写此目录，不读取大人个人相册；无需新增“最后一位大人”状态。归属在采集开始前固定，退出页面也不会转存给孩子。

## 顺手发现

【实测】负向断言里的长中文按字节截断可能让指标 JSON 含非法 UTF-8；新驱动读取时替换非法字节以展示错误，仍严格核对退出码、finished、失败计数和指定断言，没有放宽功能判断。

## 新增的机制

- 受限浮层及本次照片编号表：任务第 2、3 条要求；退出即清空，不写新持久格式。
- `camera.review` 原子能力：任务第 5 条要求；入口、取景、拍照仍走 `app.open`、`camera.preview`、`camera.photo`。
- 保存任务和受限页面分别复用唤醒引用；退出画面后，未完的保存仍能收尾。进入时复用页面栈清理旧相册，避免旧列表与后台刷新影响本次会话。
- 无新任务、线程、队列、重试、产品配置项或通用框架；执行器、解码、PNG 和文件服务均复用。反向开关仅在测试工具中，用于总规则要求的撤修复对照。

产品 C/头文件增加 145 行、删除 37 行，共改动 182 行；连同构建接线 3 行和翻译 7 行为 192 行，未超 200。超过 100 行的原因是三种入口、集中策略/输入/返回、会话回看、保存和解锁相册必须接通。看过只加按钮、打开普通相机、保存给当前成员三种更小方案，分别缺触摸路由、暴露旧相册、会存给孩子。默认锁屏继续只画提示条，未增加整屏绘制成本；原有唤醒和待机预算均通过。

## 要改的文档

合并会话需重新生成 `docs/system/capabilities.md`：新增 `camera.review`，更新 `app.open`、`camera.preview`、`camera.photo` 的锁定标志与入口限制。已独立生成 `build/lock-capabilities.md`，461 项定义校验通过。还需同步界面规范 §3.5、`docs/system/ui-dependencies.md` 的 D-121 状态和浅睡边界。本次没有修改禁止的共享文档。

【实测】按总规则的隐私模式扫描基线至本地提交的差异，无匹配（扫描退出 1）；未发现本机路径或硬件号。差异检查无空白错误，文件范围仅本任务代码、测试、翻译、报告及两个文档源码行号链接，无构建产物、设计稿或禁止的共享文档。
