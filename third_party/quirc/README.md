# quirc

来源：https://github.com/dlbeer/quirc ，固定提交 `f2e4271ae43a15d9263704e07bc86cba5697d74d`（2026-10-28），查阅日期 2026-11-10。ISC，见 LICENSE。

仅复制上游 lib/ 的 C 源码和头文件，除 decode.c 的解码临时区迁到堆外，原文件未修改；本地补丁保留 ISC 声明，解码算法及公开 API 不变。构建时强制包含本项目的 quirc_alloc.h，把所有堆分配映射到 OSAL PSRAM；不使用宿主 libc 堆。

原始 codeload tar.gz SHA-256：`b8c3d3a3a6644395f9187951aba0cba7b1503566924b38c2a26c285b4686eee7`。包含本地补丁的逐文件哈希见 SHA256SUMS。
