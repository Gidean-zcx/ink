# 开发机测量

测量入口为 `tests/hub/e2e.py`（从仓库根目录计）：和真实中枢同一个 Python 主进程，通过独立计时器采样事件循环延迟；`resource.getrusage(RUSAGE_SELF)` 取主进程峰值 RSS，不混入模拟器子进程。

`measurement.json` 含采样数、p95、最大值、峰值 RSS、完成轮数和工具数。数字随开发机负载变化，写入设计时标【实测，开发机】，不作为日本服务器容量或真实语音时延的承诺。
