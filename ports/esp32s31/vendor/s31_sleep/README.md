# S31 原厂浅睡补丁

来源：[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/power_policy/s31_sleep)，提交 `690a023c66862af103a3187464373ee7c5efcd14`，核查日期 2026-11-10。许可证为同仓库根目录的 MIT，见 [LICENSE](LICENSE)。下表为原始文件哈希；头文件和 CMake 原样复制。

| 文件 | SHA-256 |
| --- | --- |
| s31_sleep.cc | `19c95fe1dae79878c54290baf86b8d35b6e651342627a4a363805c9f60cfc721` |
| s31_sleep.h | `1152739f5877be85ae420554f8448e80def89ccd32357075710a7539a60e581e` |
| s31_sleep_sdk.cmake | `90d2190a5ac0cfae52b6f95353d90afeea978c79bf733d1fe6d83ffe6d74c927` |
| LICENSE | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |

接入适配在目录外：`s31_sleep_bridge.cc` 提供 C 入口，`s31_sleep_sdk_check.cmake` 限定 ESP-IDF 6.1.0 开发版本（本次 SDK 提交 `fff9895c82d744c7237be8847347bdd1b07c6643`）及 PMU 文件哈希。SDK 版本、文件内容或锚点变化即停止构建，不能凭出现一个函数名就跳过审核。SDK 安装目录只读，兼容副本由原厂 CMake 写入构建目录。

`s31_sleep.cc` 仅作两处编译适配：将 RTC volatile 字段的连续赋值拆开（GNU C++26 的 volatile 弃用检查在警告当错误时拒绝该写法）；诊断改用 IDF 当前的多源唤醒位图接口，消除旧单源接口的弃用警告。底层寄存器和时钟次序不变，没有关闭警告。适配后 SHA-256 为 `515df763f512cb531ebf77a1c2280e6bea05181c010cbbd5ff6fac76841eb01d`。
