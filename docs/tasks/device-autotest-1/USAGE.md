# 编译与主会话上机步骤

本任务只编译、跑宿主机测试，不执行以下设备命令。

## 编译

先按总规则构建宿主机工程，生成 `build/assets`。沿用已有 `INK_DEPS`、ESP-IDF 与原厂公开代码的只读依赖位置。

```bash
INK_AUTOTEST=OFF INK_S31_BUILD="$PWD/build-s31-normal" \
  taskpolicy -c utility bash ports/esp32s31/build.sh

INK_AUTOTEST=ON INK_AUTOTEST_SCRIPT="$PWD/tools/device/autotest/camera.script" \
  INK_S31_BUILD="$PWD/build-s31-test" \
  taskpolicy -c utility bash ports/esp32s31/build.sh
```

换 `reader.script` 或 `memo.script` 后重新编译即可。脚本以字节数组嵌入，不读 TF 卡上的脚本；`OFF` 不编译解析器和设备执行器，也不加链接包装。普通宿主机构建始终只复用解析，不启动设备执行器。

`build.sh` 受 `INK_S31_BUILD` 控制，可复用一个目录开关来回切换；脚本路径变化和内容变化都会触发重新配置。默认关闭，未提供脚本的测试版配置会失败。

## 脚本前提

- 已完成首次设置、当前成人空间已解锁、界面字号为默认值。执行器等主页实际提交刷新并完成后才计时；60 秒仍未出现主页就记 `home-timeout` 并退出，不代输密码。
- 相机：打开后保持 20 秒，再回主页；不按快门。
- 阅读：先由主会话将 [reader.txt](../../../tools/device/autotest/reader.txt) 放入测试 TF 卡的 `content/books/自动测试长文.txt`。脚本按书名打开，再按 20 次“下一页”。此文件是原创合成文字（CC0），内置短示例不足 20 页，不能代替它。
- 备忘：新建草稿，通过屏幕键盘输入 `hello`，再按五次退格擦除，最后回主页。不会删除已有备忘；空草稿是否保留由现有备忘服务决定。
- 百分比坐标对应逻辑屏幕；`key home/next/back` 等是逻辑键，设备不必真的有对应实体按键。墨水屏刷新或闪存占用仍可能拖慢页面，日志里的“执行”表示输入已交给系统，真实页面结果仍须主会话核对。

## trial_run

主会话确认备份、端口和测试文件后，使用现有工具；下面的变量由主会话提供，不要写入仓库。

```bash
python tools/device/trial_run.py \
  --port "$DEVICE_PORT" --backup "$VERIFIED_BACKUP" \
  --image build-s31-test/ink-s31.bin --fonts build-s31-test/ink-fonts.bin \
  --out "$DEVICE_RUN_OUT" --seconds 120 --until 'autotest: summary'
```

默认流程会验证、运行并恢复备份中的五个分区；恢复的是备份固件，不会自动安装新编译的正常版。若主会话的目标是回到本项目正常版，随后按其现有授权安装正常镜像：

```bash
python tools/device/trial_run.py \
  --port "$DEVICE_PORT" --backup "$VERIFIED_BACKUP" \
  --image build-s31-normal/ink-s31.bin --fonts build-s31-normal/ink-fonts.bin \
  --out "$DEVICE_NORMAL_OUT" --seconds 30 --install
```

## 看日志

- `autotest: <行号> <动词> executed`：该源代码行已交给输入服务／处理器。一次 tap 或 swipe 展开的多个触摸边沿只计一步。
- `autotest: <行号> cap source=test result=<数字>`：走 `ink_cap_invoke_json`，其内部复用 `ink_cap_invoke` 的同一调用实现；0 为完成、2 为已排队，其余回执按现有枚举解释。
- `skipped/rejected`、`parse-error`：未执行；模拟器环境动词在设备上跳过，未知行也跳过。cap 参数、note/mark 正文均不回显，用行号对照原脚本。
- `autotest: 0 cap rejected source=test`：测试版全局安全包装拦截了页面或后台发起的危险能力；不包含参数或未知能力名。
- `autotest: summary executed=N skipped=N elapsed_ms=N`：脚本结束；没有此行或出现 `home-timeout` 都不能认定完成。
- 刷新仍看原有 `display refresh_us=... ok=...`，资源仍看 `health`；这些不受本任务修改。测试脚本完成不表示实机帧率已达标。

拒绝规则集中在 `ink_script_cap_allowed`：风险为破坏性本地操作、外发、系统级的全部拒绝；需密码、需手动确认、只对本人开放的也拒绝。另拦截身份／家长／配对／恢复／备份／确认、短信电话（含紧急呼叫）、文件和通用数据能力、中枢及 AI 转发；名称含删除、移除、密码、重置、付费也拒绝。设备测试版还包装全部三个能力入口和确认决定，防止点按绕过；密码或隐藏输入框、AI 键不注入。调用沿用 `INK_SRC_APP` 的现有权限约束，日志单独标 `source=test`，不冒充真实触摸或已确认来源。

## 宿主机复核

```bash
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" \
  --test-dir build --output-on-failure -j2 -R '^autotest-'
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" \
  tests/device_autotest/negative.py build
```

反向对照只在构建目录生成临时源码、对象和程序，不改仓库源码、不依赖 Git 历史。移除能力拒绝和输入转发后，新测试必须失败。
