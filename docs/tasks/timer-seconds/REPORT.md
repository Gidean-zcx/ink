# timer-seconds 执行报告

基线：`6171c2bf8d59238c95f2d548663f8b2ab3c6f43a`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 真实触摸从 1 分钟逐秒选到 90 秒，显示 `1:30`；30 秒显示 `0:30`，3601 秒显示 `1:00:01`；90 秒响铃在实际启动时点的 100 ms 窗内。每秒 1 次数字区域快刷，离页/锁屏无走秒刷新，返回时间准确；秒表、分段秒数与 AI 快照一致。旧版反向运行失败 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility ctest --test-dir build -R '^(timer-seconds|clock-|sound-clock)' --output-on-failure -j2`；`python tests/timer_seconds.py --build build --reverse` |
| 2 | 【实测】定向通过；全量待最后统一运行 | `100% tests passed out of 5`；`gcccheck: 3 compiled, 0 with findings` | 同上；`python ../tools/gcccheck.py build system/apps/clock/clock.c tests/timer_seconds.c tests/sound_clock.c`（工具从工作区父目录调用） |
| 3 | 【实测】通过 | 本目录 6 张 `snap-*.png`：选择、倒计时、秒表各有常规/120%；单张 27–42 KB。已逐类查看，无重叠或越界 | `python tests/timer_seconds.py --build build` |

【实现】保留原有四个分钟预设，增加分/秒加减（仍调 `clock.timer_set(seconds)`）；原能力已有秒参数，无须改时间服务或能力表。后台截止点只在到点回调，可见计时页用 LVGL 每秒更新数字，离页销毁；秒表原本已按秒局部更新，补充 `elapsed_seconds` 快照。计时页补 `remaining_seconds`。产品代码 51 增/13 删，英文 3 行，修改合计 67 行，低于 120 行。

## 没做到的

未在真机测刷新和响铃；主工程全量、内存检查与固件统一收尾。

## 和设计不一致的

无。数字快刷的范围包含 LVGL 数字标签实际绘制外扩区（标签字体行高的 1/4）；测试取 `lv_obj_get_ext_draw_size` 与安装旋转转换后的真实范围检查，没有任意放宽像素预算。

## 改过的已有测试

`tests/sound_clock.c` 的三处旧期望按任务改动：不再断言“分钟倒计时不每秒刷新”，改断言每秒一次；一分钟后的剩余时间改看 `分:秒`；孩子默认时长从 `5 分钟` 改为 `5:00`。其余断言保留。新增 `timer_seconds.c/.py` 独立验收和手动基线反向模式，ctest 不读 Git 历史。

## 要用户定的

无。

## 顺手发现

翻译表先在配置阶段复制到构建目录；改英文表后必须重新运行 `cmake -S . -B build` 才能更新该副本。本次已重新配置，未扩大范围修改构建机制。

## 新增的机制

计时器页面 1000 ms LVGL 定时回调：任务第 3 条要求，复用已有界面任务；离页删除。无新任务、线程、状态机、文件格式或配置项。

## 要改的文档

无。能力参数原本就是秒，生成能力表内容不需改；共享文档未动。

## 上机核对清单

选 90 秒，检查到点响铃；走秒只动数字；离页和锁屏不走秒刷新，返回补对时间。常规/120% 看新控件是否易点，连续快刷的实际残影由真机复核。
