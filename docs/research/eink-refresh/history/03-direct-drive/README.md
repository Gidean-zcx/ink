# PQ-BN：可并行、事件驱动的物理量化器（面向 Metalio E-Ink4-Plus 直驱）

## 核心文件
- `pqbn_lut.c`
  - `pqbn_lut2_build()`：生成 64 KB 决策表。索引为 目标 4bpp | 模型状态 8 bit | 蓝噪声阈值 4 bit | 粘滞档 2 bit，每项 2 bit 驱动码
  - `pqbn_lut2_phase()`：每相全屏版本
  - `pqbn_lut2_phase_ev()`：事件驱动版本，只处理目标变了或还在推的像素（推荐）
- `pqbn_fixed.c`：同一算法的整数算术版本（不查表），用作参照
- `kernels.c` 中的 `pqbn_field()`：浮点参考实现；`pq_field()`：串行误差扩散版 PQ-ED，用作对照

## 复现
```
gcc -O2 -shared -fPIC -o libkernels.so kernels.c -lm
gcc -O2 -shared -fPIC -o libpqbn.so pqbn_fixed.c -lm
gcc -O2 -shared -fPIC -o libpqbnlut.so pqbn_lut.c -lm
gcc -O2 -shared -fPIC -o libpqfixed.so ../eink_sim/pq_fixed.c -lm   # sim.py 需要
python3 run_metalio_q.py                       # 画质：12 ms/相面板时序，满幅约 7.6 相
gcc -O2 -fno-tree-vectorize -o bench_ev bench_ev.c pqbn_lut.c -lm && ./bench_ev   # 各用例每相开销
```

## 说明
- 面板模型参数是假设值，未经真机标定。
- S31 开销按"x86 单核耗时 ×10~14（用满两颗核）"折算。这个系数用两种方式估算：一是对方 EPDiy 管线恰好能在两核上跑进 12 ms/相，二是 CoreMark 比例。真实数值需上板实测。
