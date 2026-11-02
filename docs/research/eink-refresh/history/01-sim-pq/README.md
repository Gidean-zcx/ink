# 墨水屏快刷驱动：积分器驱动 + 物理量化器（PQ）

仿真 + 参考实现，对比对象是 PaperBoy（Wenting Zhang）`msg.c` 里的实际驱动逻辑。

## 文件
- `pq_fixed.c`：可直接移植的整数实现
  - `int_field()`：积分器驱动（PaperBoy 驱动的零成本替换）
  - `pq_field_q8_fast()`：物理量化器，闭式解版本（推荐）
  - `pq_field_q8()`：同一算法的循环参考版（用于核对）
  - `pq_field_q8_bl()` / `pq_field_q8_wf()`：无分支版和行波前版，属于提速实验，没有带来收益
- `kernels.c`：仿真用的浮点版本（FS、滞回 ED、计数器、积分器、PQ 浮点版）
- `sim.py`：面板物理模型、方法封装、指标
- `scenes.py`：三个测试场景（自然视频 / 阅读滚动 / Game Boy 类游戏）
- `run_exp.py`：跑对比，`python3 run_exp.py V|X|gb [dc]`
- `bench.c`：计算开销基准，`gcc -O2 -o bench bench.c pq_fixed.c -lm && ./bench`
- `make_video.py`：生成并排对比视频
- `res_*.json`：最终结果数据

## 构建
```
gcc -O2 -shared -fPIC -o libkernels.so kernels.c -lm
gcc -O2 -shared -fPIC -o libpqfixed.so pq_fixed.c -lm
python3 -c "from scenes import *; import numpy as np; np.save('V.npy', video_scene()); np.save('X.npy', text_scene()); np.save('G.npy', gb_scene())"
python3 run_exp.py V
```

## 面板模型（全部是假设，需要真机标定）
- 满幅转换约 3.8 场（PaperBoy 驱动 4 场）
- 像素迁移率 ±4%（每个像素固定，控制器不知道）
- 首场粘滞：静止越久，第一场走得越少，长时间静止后约少 30%；每个像素的粘滞强度 ±30%
- 每场 3% 随机噪声
- `dc` 配置额外加入"直流残留场"：直流不平衡会削弱反向驱动，并让静止像素缓慢漂移。这一项只用于定性展示鬼影机理，参数是拍脑袋定的。

## 核对
- PaperBoy 驱动的移植与计数器模型逐像素一致：820 万个像素·场，0 处不同
- 打包积分器与参考实现一致：0 处不同
- 闭式 PQ 与循环 PQ 的驱动决策 99.91% 一致（其余是整数舍入的平局）；画质相同
- 整数 PQ 与浮点 PQ 的画质差在 0.1 dB 以内（运动段）
