# lock-camera 任务报告

基线：`ec78378e05f2694b9fa410780ab2bcf9d8de1c99`。分支：`claude/codex-lock-camera`。状态：实现、定向、S31、GCC 检查完成；最终两轮全量正在运行。以下证据路径相对仓库根目录，构建产物未提交。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】模拟器亮屏锁定、成员门及密码页通过；浅睡边界见下文 | lock 110、gate 138、kid 126，共 374 条断言；两张新照落卡、本次回看、旧照/文件/其他应用拦截、真实 HOME、解锁相册、儿童归属、AI 快照均核查 | 下方定向命令 |
| 2 | 【实测】原有定向、S31、GCC 通过；最终普通及 ASan/UBSan 全量待结束 | 原有锁屏、成员门、camera-gallery 等测试未改；13 项回归全部通过，32.16 秒；日志 `build/lock-final-ui-regressions.log` | 下方全量、S31、GCC 命令 |
| 3 | 【实测】通过，逐张看过最终布局 | 下表截图；大字号布局检查原样通过 | 定向命令及 `ui-scene-big-LockPage` |

| 画面 | 截图路径 |
| --- | --- |
| 默认锁屏入口 | `build/lock-camera-fg7z2gnh/lock/snap-lock-entry.png` |
| 受限取景 | `build/lock-camera-fg7z2gnh/lock/snap-restricted.png` |
| 本次第二张的拍摄确认 | `build/lock-camera-fg7z2gnh/lock/snap-confirmation.png` |
| 成员门、密码页入口 | `build/lock-camera-fg7z2gnh/gate/snap-member-entry.png`、`build/lock-camera-fg7z2gnh/gate/snap-pin-entry.png` |

【实测】反向对照均只编译临时变异程序，不修改产品源码：`--negative` 撤掉公共归属，变异程序 122 通过、4 失败、finished=1、退出 1；`--late-mode` 恢复到采集后才判断模式，125 通过、1 失败、finished=1、退出 1。包装脚本核对指定归属断言后均退出 0。证据分别为 `build/lock-camera-f9cp94jo/`、`build/lock-camera-d8f43hgw/`，日志 `build/lock-negative.log`、`build/lock-late-mode.log`。

【实测】S31 完整编译、链接、镜像生成、size、stack 脚本均退出 0。镜像 1,342,576 字节，DIRAM 88,180 字节，PSRAM 静态 BSS 552,728 字节；日志 `build/lock-s31.log`，镜像 `build-s31/ink-s31.bin`。仅编译，未烧录或运行设备。

【实测】GCC：`gcccheck: 12 compiled, 0 with findings`，退出 0；日志 `build/lock-gcccheck.log`。两种宿主构建均为 `-Werror`，内存构建另有 `-fsanitize=address,undefined -fno-omit-frame-pointer`；已核对产品相机与新测试的实际编译参数。

复现命令（配置沿用总规则第 5 节；ASan 目录加 `-DINK_SANITIZE=ON`，两目录同用本地 SDL2、ImGui 和 Hub Python 缓存）：

```bash
taskpolicy -c utility cmake --build build -j4
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_camera/run.py --build build
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_camera/run.py --build build --negative
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_camera/run.py --build build --late-mode
INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub/src" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
# 内存检查：同一全量命令，目录换成 build-asan，构建也使用 -j4。
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
- 最终普通全量和补验结果待补。ASan/UBSan 全量原话为 `99% tests passed, 3 tests failed out of 318`，672.48 秒，退出 8：两项共享文档，加 `ui-scale` 不识别 `LV_MAX`。后者已改为等价主题算式，独立检查 `ui-scale: 49 files, 0 violations`，退出 0；无尺寸豁免、断言变更或预算放宽。

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
- 无新任务、线程、队列、重试、配置项或通用框架；保持唤醒、执行器、解码、PNG 和文件服务均复用现有设施。

产品 C/头文件增加 141 行、删除 35 行，共改动 176 行；连同构建接线 3 行和翻译 7 行为 186 行，未超 200。超过 100 行的原因是三种入口、集中策略/输入/返回、会话回看、保存和解锁相册必须接通。看过只加按钮、打开普通相机、保存给当前成员三种更小方案，分别缺触摸路由、暴露旧相册、会存给孩子。默认锁屏继续只画提示条，未增加整屏绘制成本；原有唤醒和待机预算均通过。

## 要改的文档

合并会话需重新生成 `docs/system/capabilities.md`：新增 `camera.review`，更新 `app.open`、`camera.preview`、`camera.photo` 的锁定标志与入口限制。已独立生成 `build/lock-capabilities.md`，461 项定义校验通过。还需同步界面规范 §3.5、`docs/system/ui-dependencies.md` 的 D-121 状态和浅睡边界。本次没有修改禁止的共享文档。
