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

## 共用控件反向对照

【实测】同一组断言完成后，撤去字体合并、分组去框、清除透明底、焦点边框四种变体分别退出 1，失败数为 1、24、12、2。正常主题在两种显示档案通过。临时源和结果在 `build/design-lang-1/reverse/`。

## GCC 警告核对

【实测】任务指定 `gcccheck.py`：`gcccheck: 8 compiled, 0 with findings`，退出 0。新截图驱动不在主工程编译数据库中，工具明确跳过；它由截图脚本按实际主工程 clang 选项另行编译成功。此检查不等同于 Linux CI 已运行。

## 内存与 S31

运行中，最终结果在 REPORT.md。
