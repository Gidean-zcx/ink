# 验证记录

## 普通构建与全量

【实测】macOS、Apple clang、`INK_WERROR=ON`，完整构建退出 0。原始日志在 `build/design-lang-1/validation/normal-ctest.log`。

```text
92% tests passed, 32 tests failed out of 423
Total Test time (real) = 1116.05 sec
```

ctest 退出 8。三个测试显示 Skipped：`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`；未运行结果不算通过。

失败项：

- `lock-faces`
- `ui-scene-big-Today`
- `ui-scene-ImeEdit`
- `ui-scene-big-ImeEdit`
- `ui-scene-big-PhoneIncoming`
- `ui-scene-child-Today`
- `ime-keyboard1`
- `i18n-services`
- `i18n-lang`
- `i18n-apps`
- `i18n-memo`
- `i18n-shell`
- `i18n-shell-remaining`
- `ui-context-regressions-metalio-plus`
- `ui-context-regressions-desktop-lcd`
- `scenario-reader_continuous-metalio-plus`
- `scenario-reader_rapid-metalio-plus`
- `scenario-reader_empty-metalio-plus`
- `notify-manage`
- `settings-small`
- `audit10-b10-02`
- `calendar`
- `weather`
- `confirm-pages`
- `files-app`
- `files-ops`
- `factory-pages`
- `ui-fix-a-factory`
- `transfer-pages`
- `kid-3`
- `comm-ux2`
- `music-product`

其中两个 `ui-context-regressions-*` 用例随后修正了设置首页翻页所用能力名，并与主题、滑杆一起复跑：`100% tests passed out of 6`，11.95 秒。其余失败未宣称解决。

三个阅读场景的原始失败分别为：连续翻页 17 < 36；快速翻页 17 < 60；空书场景缺少前后页动作及第二次稳定打开指标。未抬预算，也未对它们做全量基线归因，不能把所有失败统称为旧测试。

## 最后两处共用修正后的界面复核

【实测】修正负坐标舍入及稍后绑定图标的热区后，两种构建都重新编译，运行全部 `ui-*` 与 `settings-a`，均为 `95% tests passed, 6 tests failed out of 118`。普通 305.85 秒，ASan/UBSan 176.93 秒。失败为大字今日、孩子今日、默认/大字编辑栏、大字来电、出厂设置页面；没有修改这些页面来消除断言。

最后小修后没有再次重跑其余全量项；上面的普通、下面的 ASan 全量数字属于这次修正之前。完整 60 场景、截图和控件树则已重新采集。

## 共用控件反向对照

【实测】六种变体均执行到结束并明确失败；正常主题在两种显示档案通过。

| 撤去的修正 | 退出码 | 失败断言数 |
| --- | ---: | ---: |
| late-icon-target | 1 | 2 |
| negative-rounding | 1 | 1 |
| font-merge | 1 | 1 |
| group-frame | 1 | 24 |
| clear-background | 1 | 12 |
| field-focus | 1 | 2 |

临时源和结果在 `build/design-lang-1/reverse/`。

## GCC 警告核对

【实测】任务指定 `gcccheck.py`：`gcccheck: 8 compiled, 0 with findings`，退出 0。新截图驱动不在主工程编译数据库中，工具明确跳过；它由截图脚本按实际主工程 clang 选项另行编译成功。此检查不等同于 Linux CI 已运行。

## 内存检查

【实测】主工程 `build-asan` 配置 `INK_SANITIZE=ON`、`INK_WERROR=ON`，完成构建后运行全部 423 项：

```text
93% tests passed, 30 tests failed out of 423
Total Test time (real) = 2249.69 sec
```

ctest 退出 8，三个 hub 项 Skipped。30 个失败项等于普通全量的 32 项减去已修正的两个设置翻页用例。全量与最后 118 项界面复核的失败输出均未出现 AddressSanitizer、UndefinedBehaviorSanitizer 或 `runtime error:` 诊断；这不把失败的 ctest 写成全通过。

原始记录在 `build/design-lang-1/validation/asan-ctest.log`，最后两轮界面复核在该目录的 `final-ui.log`、`final-asan-ui.log`。

## S31

【实测】S31 宿主机独立配置并完整构建，ctest 退出 0：`100% tests passed out of 77`，128.86 秒。原始记录为 `build/design-lang-1/validation/s31-host-ctest.log`。固件编译进行中。
