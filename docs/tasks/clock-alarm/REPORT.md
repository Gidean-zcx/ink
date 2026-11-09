# clock-alarm 任务报告

基线：`d0cacd1d4b348a8e885904026f3796a10996c646`。工作分支：`claude/codex-clock-alarm`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】部分完成 | 真实触摸新建、能力删除、同一模拟闪存重启后保留三条；编辑、开关缺服务接口，未做 | 定向命令 |
| 2 | 【实测】通过 | 每天 127、工作日 62、自选周日／周六 65；显示、原始记录、重启读取均断言 | 定向命令 |
| 3 | 【实测】通过 | 实际响铃，触摸稍后响保存十分钟，AI 调原能力关掉；页面与 AI 的 3072 字节快照一致 | 定向命令 |
| 4 | 进行中 | Werror 构建及定向测试通过；全量、内存检查、最终规模待补 | 待补 |

## 没做到的

- 编辑和开关闹钟未做。现有 `time.h` 仅有 add、cancel、next、dismiss、snooze，`time_caps.c` 也没有 update／enable 能力；新增接口会越过任务书“只做页面和接已有能力”的边界，因此没有用删除重建冒充编辑，也没有把停用做成删除。
- 尚未运行 Linux／gcc 和实机；不操作真机，不推送或开 PR。

## 和设计不一致的

- Clock 未放任务排除的“计时”入口；下一个闹钟只显示真实时间；增加 F4 页码条。已有闹钟详情明确提示暂不支持修改和开关。画板对照分数待补；尺寸均取当前主题。

## 改过的已有测试

- 暂无。

## 要用户定的

- 暂无；任务内可逆选择按最保守方案处理。

## 顺手发现

- 新增能力后，生成的能力目录需要主会话统一更新；依任务总规则第 7 节，本任务不修改该共享文件。

## 新增的机制

- Clock 列表、新建表单及详情页：任务书要求；复用 F4、F2、F1 和时间服务，新增七个页面导航／草稿／保存能力，底层写入仍走 `alarm.set`／`alarm.cancel`。
- 无新产品任务、线程、状态机、持久格式、配置项、缓存或队列；只保留三条可见记录和当前表单，位于 PSRAM。测试驱动任务仅在测试程序内。

## 要改的文档

- 完成后由主会话更新 backlog、ui-dependencies 和生成的能力目录；本任务不改共享文档。

## 阶段证据

- 【实测】`taskpolicy -c utility cmake --build build -j2` 退出 0；开启 `INK_WERROR=ON`，没有放宽任何预算。定向测试退出 0，包含两个独立进程的保存／重启读取。
- 【实现】页面不写服务内部状态；触摸和 AI 都经注册能力，响铃继续使用既有系统打断卡；`alarm.revision` 通过既有 UI 执行器刷新页面，LVGL 只在 UI 任务调用。
- 【实测】首轮定向检查发现键盘打开时快照超出 AI 的 3072 字节缓冲；输入期间的操作归键盘，表单快照在这时不重复列出不可执行的页面按钮，完整字段及真实输入会话仍可读。十分钟起点按实际点击前后虚拟时间界定，未新增 sleep。

定向命令（`INK_DEPS` 指用户给定的依赖检出）：

```bash
taskpolicy -c utility cmake --build build --target sim-clock-alarm -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^clock-alarm$' --output-on-failure -j2
```

日志：`build/clock-build.log`、`build/clock-targeted.log`；截图、模拟闪存及两轮结果在忽略目录 `build/test-out/clock-alarm-*/`。
