# ui-fix-g 专用验证

复用主工程 `sim-qr-scan` 的编译、链接选项，逐个替换本任务六个页面文件；不改 CMake、公共测试或产品工作树。正向模式不读 Git 历史。

```sh
PY="$INK_DEPS/build-deps/pyenv/bin/python"
BASE=943e8d9f44a3ecfb6cbafa6778388c6ef92920bf
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$PY" -B tests/ui_fix_g/run.py --build build --screens
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$PY" -B tests/ui_fix_g/run.py --build build --before "$BASE" --screens
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$PY" -B tests/ui_fix_g/run.py --build build --reverse "$BASE"
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$PY" -B tests/ui_fix_g/run.py --build build-asan
```

可用 `--case scan|menu|battery|off|interrupt|airplane` 单独运行。常规和120%各跑一次；孩子沿用系统固定110%的规则。电池五种状态各用独立模拟器，避免构造不真实的关机后再次显示充电页。使用既有 `tests/ui_scene.budget.json`，不放宽预算。

扫码覆盖网址真实按钮进入网页应用、完整复制、中文长文按完整 UTF-8 字节拼回、AI翻页、Wi-Fi凭据脱敏、失败重扫、孩子限制；只使用模拟器网络和 `example.com`，不连接真实网页、Wi-Fi或拨号。扫码渲染输入是合成解码结果，真实二维码解码和相机会话另由现有 `qr-scan`、`camera-gallery` 回归验证。

电源菜单测试间距、分组、底边、实际记录与导出回执。电池页核对保存中、存好、没存好、充电和静态状态。关机图用合成书签测量完整换行。来电/闹钟和飞行卡使用合成服务回执检查呈现，打断和唤醒的真实服务路径由现有全量测试验证。

`--before` 仅跳过新增整改断言，保留原路径与截图；`--reverse` 对基线文件运行整改断言，每个文件至少一档必须失败并完整结束。历史模式只供手动对照，不注册进 ctest，不要求 CI 检出历史。

证据在 `build/ui-fix-g/{after,before,reverse}/`，包括摘要、日志、快照与图片。提交的 `clean-*` 灰阶 PNG 是同一模拟帧的干净布局；包含残影模型的 `snap-*` 原图保留在构建目录。未启动窗口模拟器，不代表真机显示或触摸验收。

全量中枢测试若因默认Python缺依赖返回77，可以用现有的只读锁定环境补跑。`PYTHONPATH`指定本工作区源码；不安装依赖，服务只绑定本机回环，使用假模型。本次普通和内存构建的三项均通过。

```sh
HUB_TEST_PY="$INK_DEPS/hub/.venv/bin/python"
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 UBSAN_OPTIONS=halt_on_error=1
for task_build in build build-asan; do
  taskpolicy -c utility "$HUB_TEST_PY" tests/hub/run_contract.py --caps "$task_build/test-out/hub/caps.json" --samples "$task_build/test-out/hub/messages.jsonl"
  taskpolicy -c utility "$PY" tests/hub/check_samples.py "$task_build/hub-json-check" "$task_build/test-out/hub/messages.jsonl"
  taskpolicy -c utility "$HUB_TEST_PY" tests/hub/e2e.py --sim "$PWD/$task_build/sim-hub-e2e" --parser "$PWD/$task_build/hub-json-check" --assets "$PWD/$task_build/assets" --content "$PWD/$task_build/test-content" --out "$PWD/$task_build/test-out/hub-e2e"
done
```
