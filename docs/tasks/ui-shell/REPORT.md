# 界面框架与系统壳执行报告

2026-11-06，第 3 轮修复。接续基线 `332cab0`，依据 [REVIEW.md](REVIEW.md) 和[总规则](../codex-rules.md)；原任务基线 `fdff779`。分支 `claude/codex-ui-shell`，仅本地提交。前轮报告保留在 `27040e3:docs/tasks/ui-shell/REPORT.md`。

**里程碑：严重 0 项，两项高问题已修并完成反向验证；普通构建通过。小型中级修复、最终普通及 ASan/UBSan 全量尚在进行，整体验收不认定通过。**

## 验收表

证据：【实测】本轮命令；【实现】代码核查。

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| UI-R3-001（高）/ UI-R2-001 / U1 | 已修，门禁回归通过 | 【实测】带/不带花括号、两路和嵌套分支、循环、条件复合赋值、别名、预处理分支及条件表达式负例；25 文件、0 违规。旧检查器在同一自检失败 23 项，无异常 | G、N |
| UI-R3-002（高）/ U4 | 已修，构建门禁通过 | 【实测】应用/分组/Dock 表、工具结构体、条件图标、别名、转发及应用 `.icon` 声明覆盖。真实 APP_ICONS 注入缺失名称：旧门禁退出 0，新门禁退出 1；实际 CMake 检查目标退出 2，恢复后退出 0；生成名称表的编译对照 0/1 | B、G、N |
| UI-R2-002、003 | 已修，待本轮全量复跑 | 【实现】未撤销成员隔离及完整答案分页 | 待 T |
| UI-R2-004、005、006 / U2、U3、U5～U13 | 本轮待最终验证 | 较大遗留项见下方；尚未用旧数据充当本轮结果 | 待 T、V |

命令（依赖使用用户指定的 `INK_DEPS` / `INK_DESIGN`；构建及反向构建串行）：

```sh
# B：按总规则配置，构建退出 0；未运行窗口程序
cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
cmake --build build -j4
# G：4/4 通过，退出 0
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3 \
  -R '^ui-(scale|scale-regressions|icons|icons-regressions)$'
# N：退出 0，表示正反对照均符合预期；隔离 CMake 构建最多 -j1
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_r3_reverse.py
```

【实现】图标表使用从 manifest 生成的 `INK_UI_ICON` 名称引用，编译时也拒绝不存在的名称；资源检查成为 `ink_system` 的前置依赖。检查器仍是有界 C 源码检查，不声称通用 C 编译/数据流分析。日志留在忽略目录 `work/ui-shell-r3/`。

## 没做到的

本轮最终全量、内存检查、视觉复跑待完成。前轮披露的两项主页刷新预算、听取睡眠、通知声音、故障注入电源失败尚未关闭；不能把门禁通过当作整体验收通过。跨页对象保留/脏矩形/两段刷新、AI 主动作、全局字号及完整声音设置不是已完成事项。

## 和设计不一致的

键盘固定高度、分段边框、搜索图标等局部问题将继续修复并验证。其余成套布局、密码流程及 30 屏视觉仍按复查记录保留；尚未取得本轮新分数。

## 改过的已有测试

`ui_scale_check_test.py` 只增加正反例，未删除原断言或放宽尺寸集合。新增 `ui_icon_check_test.py` 和独立反向脚本 `ui_shell_r3_reverse.py`；未改预算或既有失败断言。

## 要用户定的

无新增决策。按已给合同继续可逆修复，不等待答复。

## 顺手发现

【实现】本里程碑无新增服务接口、任务、常驻缓冲或第三方依赖。图标声明仍为原有字符串资源，不改变按钮/AI 的能力总线路径。最终范围、隐私、C 返回值及链接检查待结束时记录。
