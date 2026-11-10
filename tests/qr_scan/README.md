# 扫码测试资料

images/ 是本项目一次性生成的合成二维码，作为 CC0-1.0 测试数据提供；不存在真人联系人、真实密码或家庭网络。PNG 不含外部图片。每张均为 640×480 灰度，最大约 25 KiB。

生成方法：在临时 Python 环境安装 qrcode==8.2（BSD-3-Clause）和 Pillow（HPND），运行 `python tests/qr_scan/generate.py`。运行测试只需 Pillow，不依赖二维码生成器，不连接网络。生成参数、倾斜 23°、模糊半径 1.2、小码 3 像素/模块及原文见脚本与 `images/payloads.json`。

运行：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/qr_scan/run.py --build build`。

测试读取提交的 PNG，在临时证据目录转换为 RGB565 大端，经 `sim_camera_set_source` 注入原有 GC2145 模型，经过真实的相机服务、quirc、能力回执与页面。测试脚本仅添加虚构的 Wi-Fi 接入点。照片、联系人、电话均使用模拟器与隔离的临时闪存/存储卡。
