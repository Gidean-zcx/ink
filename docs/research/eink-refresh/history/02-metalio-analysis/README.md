# Metalio E-Ink4-Plus 刷新分析

- `wf_dc.py <waveforms 目录>`：波形表直流平衡审计。把仓库里的 LUT 表当数据解析，并按其源码逻辑在 Python 中重建运行时表（complete_du_build / short_du_build / my_waveform_trim / gl16_white_tick），不执行仓库代码。
- `bench_metalio.c`：1216×684 每相像素开销。EPDiy 式差分查表路径（我按其算法重写）与积分器（逐像素版、256 项查表版）对比。`gcc -O2 -fno-tree-vectorize` 更接近 S31 上的标量 C 情况。
- `sim_metalio.py`：12 ms/相面板时序下，现架构（全局锁 DU20 / DU8）与异步积分器对比：点按延迟、滚动跟踪 PSNR、回到初始画面后的残余直流。依赖 ../eink_sim 的面板模型。
- `make_clip.py`：生成 metalio_scroll_compare.mp4。
