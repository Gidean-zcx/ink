# 扫码测试资料

images/ 是本项目一次性生成的合成二维码，作为 CC0-1.0 测试数据提供；不存在真人联系人、真实密码或家庭网络。PNG 不含外部图片。每张均为 640×480 灰度，最大约 25 KiB。

生成方法：在临时 Python 环境安装 qrcode==8.2（BSD-3-Clause）和 Pillow（HPND），运行 `python tests/qr_scan/generate.py`。运行测试只需 Pillow，不依赖二维码生成器，不连接网络。生成参数、倾斜 23°、模糊半径 1.2、小码 3 像素/模块及原文见脚本与 `images/payloads.json`。

运行：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/qr_scan/run.py --build build`。

测试读取提交的 PNG，在临时证据目录转换为 RGB565 大端，经 `sim_camera_set_source` 注入原有 GC2145 模型，经过真实的相机服务、quirc、能力回执与页面。测试脚本仅添加虚构的 Wi-Fi 接入点。照片、联系人、电话均使用模拟器与隔离的临时闪存/存储卡。

可选选库基准（不加入 ctest、不依赖 Git 历史）：

```sh
# 将 zxing-cpp v2.3.0 解压到 work/qr-eval/zxing-cpp-2.3.0
# https://github.com/zxing-cpp/zxing-cpp/archive/refs/tags/v2.3.0.tar.gz
# tar.gz SHA-256: 64e4139103fdbc57752698ee15b5f0b0f7af9a0331ecbdc492047e0772c417ba
python tests/qr_scan/compare.py --zxing work/qr-eval/zxing-cpp-2.3.0 --build build-qr-eval-repro
python tests/qr_scan/compare.py --zxing work/qr-eval/zxing-cpp-2.3.0 --build build-qr-s31 --toolchain tests/qr_scan/s31-toolchain.cmake
taskpolicy -c utility python tests/qr_scan/size.py --build build-s31 --zxing work/qr-eval/zxing-cpp-2.3.0 --zxing-lib build-qr-s31/zxing/core/libZXing.a
```

`size.py` 仅在 `build-s31/qr-size/` 复制归档并替换一个初始化入口作为识别探针，对比同一 S31 ELF 的无库、quirc、ZXing 三次链接；不修改产品源文件或原始归档。比较 Flash 代码、只读数据、初始化 RAM 的装载字节（不是带分区填充的烧录镜像大小）。ZXing 保留异常及 RTTI，TLS 使用 IDF 支持的 local-exec；没有在设备上运行探针。

反向对照：给 `run.py` 加 `--negative`，在临时对象中撤掉 Wi-Fi 内容脱敏，复用原测试断言，应观察到隐私测试失败；原产品文件不修改。正常 ctest 使用已提交的图片，不要求生成器、网络、历史提交或对比库。
