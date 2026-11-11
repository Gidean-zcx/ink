# iot_eth 来源与改动

- 来源：[CloudZao/Metalio-E-INK4-Plus@874538e](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)（v2.0.77），查阅日期 2026-11-11。只读取对象，未移动上游 HEAD。
- `uart_eth_modem.*`、iot_eth 和 UHCI 按文件头为 Apache-2.0；板级生命周期为原仓库 MIT。完整许可证随文件保留；不是 GPL。
- 帧协议、主任务、独立 TX、AT 探测、网卡和 DMA 使用原厂主体。下表同时给原文与本地哈希，**不把适配后的文件声称为逐字节原文**。

| 本地文件 | 原厂路径 | 原文 SHA-256 | 本地 SHA-256 |
| --- | --- | --- | --- |
| `CHANGELOG.md` | `components/espressif__iot_eth/CHANGELOG.md` | `ccacfaa7cebcf0dd22e7a5fbfa9c723b1c7ac1cd73898d3d63d14c009289be75` | `ccacfaa7cebcf0dd22e7a5fbfa9c723b1c7ac1cd73898d3d63d14c009289be75` |
| `CMakeLists.txt` | `components/espressif__iot_eth/CMakeLists.txt` | `545c90b919e5f2d4ba526df72d09e254049625c8bed96c34eb69788a9c4a0218` | `fe2f2b39cf4937928b5701dbf44471157ed522d2bda1509393af0afffe14e758` |
| `idf_component.yml` | `components/espressif__iot_eth/idf_component.yml` | `7241920065f25e26998e0c0d7437f9c11cf7e54997e2ccb284dd162706879b83` | `7241920065f25e26998e0c0d7437f9c11cf7e54997e2ccb284dd162706879b83` |
| `include/iot_eth.h` | `components/espressif__iot_eth/include/iot_eth.h` | `02dc0dc40f01bab2040495b6b93998b8c918c0d77d1a35690926699453d65cea` | `02dc0dc40f01bab2040495b6b93998b8c918c0d77d1a35690926699453d65cea` |
| `include/iot_eth_netif_glue.h` | `components/espressif__iot_eth/include/iot_eth_netif_glue.h` | `2ceb49686310f33811c437d910184de0f4109dcda5586a12b5c2cb893268df90` | `2ceb49686310f33811c437d910184de0f4109dcda5586a12b5c2cb893268df90` |
| `include/iot_eth_types.h` | `components/espressif__iot_eth/include/iot_eth_types.h` | `be072cb8c071703a5979602acaebc281059a8681a8df370ba6de9edad6266a6f` | `be072cb8c071703a5979602acaebc281059a8681a8df370ba6de9edad6266a6f` |
| `interface/iot_eth_interface.h` | `components/espressif__iot_eth/interface/iot_eth_interface.h` | `facd60688819bd7ab87f44dd16595bff64f9211ac3c76161f3116c94c0353456` | `facd60688819bd7ab87f44dd16595bff64f9211ac3c76161f3116c94c0353456` |
| `iot_eth.c` | `components/espressif__iot_eth/iot_eth.c` | `f9df959163f207c0d1c9d463d025fa25eb24bce4d8e99bd551dd907ea5d5cf6f` | `231ca8bbe1e2d15930c3110a98c78b45e0aeb844d30712a07c3798d682b406e9` |
| `iot_eth_netif_glue.c` | `components/espressif__iot_eth/iot_eth_netif_glue.c` | `684955d3a35520a974846fdf989b4a5ebf96cb1b375480559659ede106e63880` | `a0643dfc1c3f40179ead989d765cdd399a70a75fc7df2f2d19c97acf411afd6e` |
| `license.txt` | `components/espressif__iot_eth/license.txt` | `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30` | `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30` |

## 改动原因

- 两份 C 文件仅插入固定日志闸口，原网卡 mediator、事件、内存归还与 netif glue 未改。地址、IP、指针和命令参数不格式化到日志。
- 组件版本固定为 manifest 的 0.1.0；移除构建期 package_manager，直接定义对应三个版本常量，不拉取或升级 cmake_utilities。公开头文件、接口、版本清单、CHANGELOG 和许可证保持原文。

## 改动行清单

- `CMakeLists.txt`：原 3 → 本地 3（insert）；原 5–6 → 本地 6（replace）。

- `iot_eth.c`：原 14 → 本地 14–15（insert）。

- `iot_eth_netif_glue.c`：原 15 → 本地 15–16（insert）。
