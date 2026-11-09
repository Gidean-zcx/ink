# audit4-fix 修复报告

基线：`2318b590062486809a0f13daad6c20a320f8677a`。仅处理 B4-01、B4-05、B4-06；不推送、不碰设备。报告随里程碑更新。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B4-01 | 通过 | 【实测】`audit4-b4-01`：35 条通过；撤回修复后 11 条失败，场景正常结束 | T1、N1 |
| B4-05 | 通过 | 【实测】`audit4-b4-05`：25 条通过；撤回修复后 2 条失败，场景正常结束 | T1、N1 |
| B4-06 | 通过 | 【实测】`audit4-b4-06`：29 条通过；撤回修复后 6 条失败，场景正常结束 | T1、N1 |
| 全量与内存检查 | 通过 | 【实测】普通 Werror 和 ASan／UBSan 全量均 260/260 通过，零跳过，退出 0 | V1、V2 |
| 规模与隐私 | 通过 | 【实测】产品源码新增 12、删除 4，净增 8 行；含 CMake 注册净增 16 行；隐私正则无匹配 | P1 |

## 没做到的

三项专项没有未完成项。未验证实机、真实网页或 Linux/GCC，使用 macOS 模拟器、合成中枢与合成音频。

## 和设计不一致的

无新增差异。网页编辑中短暂离线时保留输入页并显示重连提示，符合本任务要求。

## 改过的已有测试

无。新增三个审计回归测试，未删除测试、放宽断言或调整资源预算。

## 要用户定的

无新增产品决定。

## 顺手发现

无新增问题。B4-02、B4-03、B4-04 按任务边界由其他任务处理。

## 新增的机制

无。复用现有计时、能力调用和网页会话生命周期。

## 要改的文档

本次只更新本报告，无需变更共享文档。

## 修改与验证

- B4-01【实现】：在既有成员计时作业到期处，通过软总线调用 `media.pause` 和 `shell.ai_stop`。到期允许暂停、停止和停下朗读；通信规则不变。暂停标记沿用人工暂停，不随加时或焦点恢复自动继续。【实测】播放中到期 300 ms 内暂停、扬声器停播；以 `INK_SRC_TOUCH` 调用暂停／停止成功，继续播放被拒；家长以合成 PIN 加时后手动继续；长朗读到期停下；电话和短信入口仍可用。
- B4-05【实现】：只改 `navigation.c` 的 `notify_open`，转发来源取 `c->src`。【实测】AI 经通知改亮度进入 `activity.list`；触摸来源打开通知照常修改且不新增 AI 活动记录；高风险设置仍待本机确认，随后取消该请求。
- B4-06【实现】：临时断线保留原输入控件、焦点和选区；真正关闭会话时删除编辑控件，复用密码控件的清零逻辑，无落盘草稿。【实测】输入“合成文字半段”后断线 6 秒，恢复仍在输入页，文字及选区不变；提交前本地发送观察点和合成中枢均为 0 次，手动提交后仅收到一次完整文字；密码快照隐藏、AI 不能提交、超过 5 分钟后编辑和密码清除。
- 架构审视【实现】：沿用成员策略、软总线原子能力和原网页会话；没有界面绕过服务写状态，也没有增加能力目录或新的机制。

### 可复验命令与证据

按任务书配置 `build`，补充 `-DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src" -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`。`build-asan` 使用相同选项，另加 `-DINK_SANITIZE=ON`。构建最多 `-j2`，测试最多 `-j2`，两轮全量依次运行；GUI 冒烟由 CTest 使用 SDL dummy 无窗口运行。

```sh
# T1：三个新增测试。
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^audit4-b4-' --output-on-failure -j2
# N1：保留测试，分别用指定基线的对应产品源码替换被测对象。
for n in 1 5 6
do
  INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audit4_fix.py --build build --case "$n" --baseline || exit $?
done
```

T1：3/3 通过，退出 0，4.87 秒。N1：三项均输出 `BASELINE_FAIL_CONFIRMED`，包装命令退出 0；被测程序均退出 1，预算正确检测到失败断言。证据在 `build/test-out/audit4-b4-{01,05,06}{,-baseline}/`（`result.json`、`run.log`、`out/metrics.json`）。首次未修复运行记录：`build/audit4-fix/pre-fix.log`；最终专项记录：`build/audit4-fix/fixed.log`。

V1【实测】：普通版 Werror 构建退出 0，全量 **260/260 通过**，零跳过，退出 0，252.13 秒。包括既有 `kid-2`、`remote-app`、`integration-media`、通知及隐私／成员边界检查。日志：`build/audit4-fix/{configure,build,ctest}.log`。

V2【实测】：ASan／UBSan Werror 构建退出 0，全量 **260/260 通过**，零跳过，退出 0，455.42 秒。编译参数含 `-fsanitize=address,undefined -fno-omit-frame-pointer -Werror`；CTest 及详细测试日志未检出 sanitizer 错误。日志：`build-asan/audit4-fix/{configure,build,ctest}.log`、`build-asan/Testing/Temporary/LastTest.log`。

```sh
# V1：普通版全量。
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# V2：ASan／UBSan 全量，复用现有测试的成功后清理规则。
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

P1【实测】：`git diff 2318b590..HEAD --stat` 和文件清单核查只有 8 个相关文件，无构建产物或设计稿；产品源码净增 8 行。按总规则第 6 节执行提交差异隐私命令，无匹配（`grep` 退出 1）；`git diff --check` 退出 0。全部改动仅作本地提交。
