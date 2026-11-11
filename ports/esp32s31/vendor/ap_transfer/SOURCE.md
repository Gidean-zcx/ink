# 原厂无线传书来源

## fw-size-1 解压接线（2026-11-12）

S31 不再编入下列 zlib 源文件，改为与阅读器共用既有的 miniz 3.1.2（`third_party/reader_miniz/`，原许可证不变）。原 zlib 文件和许可证保留作来源及宿主机对照，不随固件链接。`main/CMakeLists.txt` 从阅读器源表取出 `miniz.c`，只构建一次 `ink_miniz`；保留阅读器的编译宏、分配接线和符号前缀。传书通过既有 `apps/reader/vendor/shim/zlib.h` 接入。

相对本文件原先记录的适配版，`ap_transfer.cc:624` 仅将分配器赋值换成无捕获泛型 lambda：兼容 zlib 的 `uInt` 和 miniz 的 `size_t` 回调参数；仍调用同一个 `UploadZalloc`，其 PSRAM 分配和释放不变。新 SHA-256：`fad8a9b10a42a276188366eeb9f818ebd9dbb4a4fd89f998adfd9670b8945180`。上传协议、帧间字典、解压输出批次和错误处理均未改动。原厂上传实现的来源仍为下文提交 `874538e`；更换解压库由 fw-size-1 任务书明确要求。

来源：[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus)，提交 `874538e`，查阅日期 2026-11-11。路径 `main/display/screen/cloud_screen/ap/`。

整文件复制后做下列适配，MIT；二维码文件另有 Nayuki MIT 声明，保留文件头。

| 文件 | 原文 SHA-256 |
| --- | --- |
| `ap_transfer.h` | `a7d1268bc353aaeb06b9d7397f82d1c583ff9b3f3d255e93fa318a77e6d26f15` |
| `ap_transfer.cc` | `3fc762af1bffda80aac67aeacf9bee6a028ca72444cabcb607c98fa31fd26585` |
| `ap_request_worker.h` | `4e825478a4efdf91b2ff3e44e86c228621fb3c1af6e93c296eb4358a947e057e` |
| `ap_request_worker.cc` | `68564c2bbb428bc0a1534d790e68c091a97ba4767d7a9899bd24031c15f3ef69` |
| `ap_qr.h` | `4b3c609aa1c229e655d27d5c8b2e574a38b0e17802253b359bd1b1600961e52c` |
| `ap_qr.cc` | `541a2e1b99d8b4b8815727ac17c3bb823bf55250e9714b724ae4eb3e40a1ac2e` |
| `qrcodegen.c` | `014eb10d99212b15455df5657a63f84b488766958376eb5de1f2d5efc9efcbf9` |
| `qrcodegen.h` | `0c0dff3efeff6b30d4bc447b9503634a48884b01a8f535cf4f0b92693872a4e6` |
| `fileman.html` | `3bcf5a60b547e58531790a06119f69df84f98b946895898d8664ab6abc25f570` |
| `fileman.en.json` | `3cfcc4b1dd6e95f19bb4db2948bbeee039a825127cb08d74d8f3e9ba072182b9` |

## 适配

`ap_transfer.cc` 只替换依赖和安全边界，路由、WebSocket 帧协议、zlib 解压、原厂工作任务和网络参数保留：

- POSIX 文件、目录、容量操作改为 `ink_adapter.h` 中的策略回调，实际由文件服务执行；网页根目录映射到授权成员 `/me`。
- 原厂 `.cz-名称.part` 映射成文件服务允许的 `名称.part`；备份名映射为 `.tmp` 后缀。文件服务会隐藏这些未完成文件。
- 每个 HTTP 请求、WebSocket 握手及后续帧校验同一授权；首次输入数字后换取 128 bit 会话 cookie。授权码不进网址查询参数，扫码通过 fragment 提交。
- 流式覆盖暂时拒绝：文件服务没有原子的流式替换接口；不搬原厂绕过成员策略的备份文件写法。
- 网络生命周期接现有 STA 暂停/恢复，并恢复原有的自动发现定时器；原厂热点 IP、信道、连接数、开放认证和 Plus 发射功率不变；系统服务统一持有电源需求。
- 原厂资源符号移到匿名命名空间之外，兼容 clang；删除原文中未使用的 `s_standby_parked` 变量。路径格式化检查完整返回长度后才使用，截断直接拒绝。
- HTTPD 不复制响应头值，会话 cookie 头使用静态存储；文件响应设为 `no-store`。
- Plus 功率上限 48 直接取自原厂 `components/esp-wifi-connect/include/wifi_station.h:16`，单位 0.25 dBm。
- `PowerPolicy` 兼容壳由系统授权期间的 `INK_NEED_AWAKE` 统一承担；原厂书库缓存壳由现有文件服务成员事件替代，不新增缓存。基线没有收件箱自动入架接口，报告中明确未实现。
- 原厂语言表属于未随仓库提供的资源，`ink_lang.h` 使用项目中文错误提示；这不是原厂译文。
- `fileman.html` 的自动分类默认关，默认收件箱；现有导入规则决定书架目录。隐藏未接入的自动分类控件。没有合入 `ui-fix-a`，保留 `ap_qr.*` 原文但不编入页面；`hal_transfer_qr` 使用原文 `qrcodegen`。

以下是相对原文的每个修改范围（原文行号 → 当前行号）。通用文件调用替换均为上述文件服务适配，新增代码为授权和 HAL 接线。

| 文件 | 原文行 | 当前行 |
| --- | --- | --- |
| `ap_transfer.cc` | 1–2 | 1–1 |
| `ap_transfer.cc` | 6–14 | 4–4 |
| `ap_transfer.cc` | 27–27 | 17–17 |
| `ap_transfer.cc` | 46–46 | 35–44 |
| `ap_transfer.cc` | 50–50 | 49–49 |
| `ap_transfer.cc` | 96–96 | 95–95 |
| `ap_transfer.cc` | 165–171 | 163–163 |
| `ap_transfer.cc` | 250–250 | 241–241 |
| `ap_transfer.cc` | 354–354 | 346–346 |
| `ap_transfer.cc` | 361–361 | 353–353 |
| `ap_transfer.cc` | 383–384 | 375–376 |
| `ap_transfer.cc` | 386–386 | 378–378 |
| `ap_transfer.cc` | 390–390 | 382–382 |
| `ap_transfer.cc` | 394–394 | 386–386 |
| `ap_transfer.cc` | 404–404 | 396–396 |
| `ap_transfer.cc` | 434–434 | 426–426 |
| `ap_transfer.cc` | 440–440 | 432–432 |
| `ap_transfer.cc` | 449–449 | 441–441 |
| `ap_transfer.cc` | 464–464 | 456–456 |
| `ap_transfer.cc` | 472–472 | 464–464 |
| `ap_transfer.cc` | 522–522 | 514–514 |
| `ap_transfer.cc` | 525–525 | 517–517 |
| `ap_transfer.cc` | 542–542 | 534–534 |
| `ap_transfer.cc` | 544–544 | 537–537 |
| `ap_transfer.cc` | 553–554 | 546–547 |
| `ap_transfer.cc` | 586–586 | 579–579 |
| `ap_transfer.cc` | 596–596 | 589–589 |
| `ap_transfer.cc` | 611–611 | 605–605 |
| `ap_transfer.cc` | 663–664 | 657–658 |
| `ap_transfer.cc` | 768–768 | 762–762 |
| `ap_transfer.cc` | 771–774 | 765–769 |
| `ap_transfer.cc` | 776–776 | 771–771 |
| `ap_transfer.cc` | 782–782 | 777–777 |
| `ap_transfer.cc` | 810–810 | 805–805 |
| `ap_transfer.cc` | 812–812 | 807–807 |
| `ap_transfer.cc` | 815–815 | 810–810 |
| `ap_transfer.cc` | 824–824 | 819–819 |
| `ap_transfer.cc` | 828–829 | 823–824 |
| `ap_transfer.cc` | 831–834 | 826–829 |
| `ap_transfer.cc` | 840–840 | 835–835 |
| `ap_transfer.cc` | 848–848 | 843–843 |
| `ap_transfer.cc` | 904–904 | 899–899 |
| `ap_transfer.cc` | 918–918 | 913–915 |
| `ap_transfer.cc` | 922–922 | 920–920 |
| `ap_transfer.cc` | 953–953 | 952–952 |
| `ap_transfer.cc` | 964–964 | 963–963 |
| `ap_transfer.cc` | 966–966 | 965–965 |
| `ap_transfer.cc` | 971–971 | 970–970 |
| `ap_transfer.cc` | 979–979 | 978–978 |
| `ap_transfer.cc` | 983–983 | 982–982 |
| `ap_transfer.cc` | 1040–1040 | 1039–1039 |
| `ap_transfer.cc` | 1042–1042 | 1041–1041 |
| `ap_transfer.cc` | 1078–1078 | 1077–1077 |
| `ap_transfer.cc` | 1080–1080 | 1079–1079 |
| `ap_transfer.cc` | 1094–1094 | 1093–1094 |
| `ap_transfer.cc` | 1103–1103 | 1104–1113 |
| `ap_transfer.cc` | 1104–1104 | 1115–1115 |
| `ap_transfer.cc` | 1161–1161 | 1173–1173 |
| `ap_transfer.cc` | 1221–1221 | 1234–1234 |
| `ap_transfer.cc` | 1235–1241 | 1248–1248 |
| `ap_transfer.cc` | 1340–1355 | 1347–1349 |
| `ap_transfer.cc` | 1372–1372 | 1366–1371 |
| `fileman.html` | 216–217 | 216–217 |
| `fileman.html` | 236–236 | 236–236 |
| `fileman.html` | 1066–1072 | 1066–1067 |
| `fileman.html` | 1076–1076 | 1071–1071 |
| `fileman.html` | 1245–1245 | 1240–1240 |

## 额外原文依赖

`pack_ap_fileman.py` 来自同一原厂提交的 `scripts/pack_ap_fileman.py`；`cloudzao-logo.png` 来自同一 ap 目录。原厂声明依赖 `espressif/zlib: "*"` 和 `espressif/cjson: "*"`，本端口将源码固定为 [zlib 1.3.1](https://github.com/madler/zlib/tree/v1.3.1)（zlib 许可证），整文件复制并保留许可证；[cJSON 1.7.19](https://github.com/DaveGamble/cJSON/tree/v1.7.19)（MIT）直接复用仓库已有 `third_party/cjson/`，哈希与该版本原文相同，不再复制第二份。只编入解压所需的 zlib 源文件。没有升级仓库既有依赖。

| 文件 | 本次入库 SHA-256 |
| --- | --- |
| `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `ap_qr.cc` | `541a2e1b99d8b4b8815727ac17c3bb823bf55250e9714b724ae4eb3e40a1ac2e` |
| `ap_qr.h` | `4b3c609aa1c229e655d27d5c8b2e574a38b0e17802253b359bd1b1600961e52c` |
| `ap_request_worker.cc` | `68564c2bbb428bc0a1534d790e68c091a97ba4767d7a9899bd24031c15f3ef69` |
| `ap_request_worker.h` | `4e825478a4efdf91b2ff3e44e86c228621fb3c1af6e93c296eb4358a947e057e` |
| `ap_transfer.cc` | `c9a6e6a47af652f61a41db0c00affd8f55b673c1a9b5d796de5850fb039009f4` |
| `ap_transfer.h` | `a7d1268bc353aaeb06b9d7397f82d1c583ff9b3f3d255e93fa318a77e6d26f15` |
| `cloudzao-logo.png` | `d5d56e1927256523dd7f29468c980dcec90b8288ea94c26e40f7a139f1eb1d12` |
| `fileman.en.json` | `3cfcc4b1dd6e95f19bb4db2948bbeee039a825127cb08d74d8f3e9ba072182b9` |
| `fileman.html` | `b4d2bbfce317d6a2645b0a0740ea696146bf83b114398d97e63b7d110a70a4e5` |
| `ink_adapter.h` | `ac862ec9d5b807df6b62f25f620fa03a6abf075890725db14de4969b5541ec7a` |
| `ink_lang.h` | `cc8420e89158ee29701d001bc6de59243a3a4a90966634596100cc7ee91384b6` |
| `pack_ap_fileman.py` | `d8b1479d1b9e87c46adff183d5af55c3f6854f6135bf7fdbe9572bc74c77eb2e` |
| `qrcodegen.c` | `014eb10d99212b15455df5657a63f84b488766958376eb5de1f2d5efc9efcbf9` |
| `qrcodegen.h` | `0c0dff3efeff6b30d4bc447b9503634a48884b01a8f535cf4f0b92693872a4e6` |
| `zlib/LICENSE` | `845efc77857d485d91fb3e0b884aaa929368c717ae8186b66fe1ed2495753243` |
| `zlib/adler32.c` | `9cd1443a24ff2a3053961695bd432035c58347386a420d3388232376ebabe211` |
| `zlib/crc32.c` | `8fd16f0a7714d51c89c2eb37eb98ec15e8a4dc57ba343e7b7398b19144039fda` |
| `zlib/crc32.h` | `9a2223575183ac2ee8a247f20bf3ac066e8bd0140369556bdbdffc777435749e` |
| `zlib/gzguts.h` | `716fa648aca1bb06c219d7b97ad4846d8479206143bc39557bfd8283f5783e04` |
| `zlib/inffast.c` | `e6ef64ce5dc0a4cd5c7ad08ceeb2b2a698b8447f6bd156057caeb2edab68c0cb` |
| `zlib/inffast.h` | `05cc5dc9ff1da7b8b52a4bd8bda0d8a5c236a2f39efe84b941516ea13857e6c5` |
| `zlib/inffixed.h` | `237ba710f090e432b62ebf963bee8b302867e9691406b2d3f8ee89ee7bfef9b0` |
| `zlib/inflate.c` | `34c998ce0037c0537c04b03b276f680b945f9b2c9d1e01b287605bd6879f7fd2` |
| `zlib/inflate.h` | `e8d4a51b07694bf48cb91979c19974cf6a5ab0b8a09d26ec0d14df349230673e` |
| `zlib/inftrees.c` | `5d4f335221d2dc76f17abd2577d92c2d7baf68fa6d7f23373b360830493d1563` |
| `zlib/inftrees.h` | `0a0fcaf2ae2fae57426bdc06637270e9bba974f35202cadbdba479d946e6409d` |
| `zlib/zconf.h` | `f5134250a67d57459234b63858f0d9d3ef8dcc48e9e1028d3f4fdcf6eae677ae` |
| `zlib/zlib.h` | `8a5579af72ea4f427ff00a4150f0ccb3fc5c1e4379f726e101133b1ab9fc600c` |
| `zlib/zutil.c` | `8ced40d8c88588811edd2bdb35b7439983d5e1f8e9e32b8a3b244731f3c317b7` |
| `zlib/zutil.h` | `dddb2dc7a1dc339ecf2c8e089b366f08bb731c0839c7110240d17ce731bb4fea` |

复用的既有依赖（不计新增代码）：

| 文件 | SHA-256 |
| --- | --- |
| `third_party/cjson/cJSON.c` | `298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d` |
| `third_party/cjson/cJSON.h` | `25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee` |
| `third_party/cjson/LICENSE` | `a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c` |
| `third_party/cjson/UPSTREAM_TAG` | `4a858f64d3acabc18e23202bee96da2a60fde2388a6d93f04f21aecb4ef0276c` |
