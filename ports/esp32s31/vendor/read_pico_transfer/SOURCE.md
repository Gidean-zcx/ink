# kiikoread 改版传书来源

查阅日期：2026-11-12。来源：kiikoread-xiugai，固定提交 `746ce316faf61ce5228eceee618eacb50a02a395`。原路径 `components/read_pico_transfer/`；整文件复制所有 C、头文件和网页。原仓库许可原样保存在 `LICENSE`；它限定 kiikoread 的许可范围，各源文件另带 Apache-2.0 声明。本任务不对许可范围重新解释，公开发布前由主会话处理（D-129）。

## 文件与哈希

| 文件 | 原文 SHA-256 | 入库 SHA-256 |
| --- | --- | --- |
| `LICENSE` | `b1a477173eada381b15f2cd522bba1a9ab546759f6534e8c9943f0cb933aeb30` | `b1a477173eada381b15f2cd522bba1a9ab546759f6534e8c9943f0cb933aeb30` |
| `LICENSE-CloudZao` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `include/read_pico_transfer.h` | `c87c197c0d0c988888e591ad42ef13299eb9ac09ed2e4207484e0b8df14ecffc` | `c87c197c0d0c988888e591ad42ef13299eb9ac09ed2e4207484e0b8df14ecffc` |
| `include/read_pico_transfer_network.h` | `9dfe436d1e63251119faea643d8c77cb5729839ace0aff9fe133a417621ad7bc` | `9dfe436d1e63251119faea643d8c77cb5729839ace0aff9fe133a417621ad7bc` |
| `ink_adapter.h` | 项目胶水 | `f823f126fb73fb91d1fecce7e94cf826ff51bbe62b9477e049101e19e062b409` |
| `ink_radio.cc` | 项目胶水 | `eb31f7b3b3dc75152787e8bf2393fb97ae2a2281c6ee1d7417b25220049aba81` |
| `ink_server.h` | 项目胶水 | `17d92eab2adf162ffb995e3a96ff7d061d190b5aa3d4fd1cee4b03fbaaf9541a` |
| `ink_server_impl.h` | 项目胶水 | `8768c699b5fc33616cab266ebb6158401305801579124074f50c01eaf9bb46ad` |
| `pack_upload.py` | 项目胶水 | `7f16b33b2bc0dff12aa69953d99fd388c0726b03f619c87f858661e40b386b69` |
| `qrcodegen.c` | `014eb10d99212b15455df5657a63f84b488766958376eb5de1f2d5efc9efcbf9` | `014eb10d99212b15455df5657a63f84b488766958376eb5de1f2d5efc9efcbf9` |
| `qrcodegen.h` | `0c0dff3efeff6b30d4bc447b9503634a48884b01a8f535cf4f0b92693872a4e6` | `0c0dff3efeff6b30d4bc447b9503634a48884b01a8f535cf4f0b92693872a4e6` |
| `read_pico_transfer.c` | `c84ae9d28f067276ebde21aeee0d9dc2642330dbfec4ecc643a8eff9ce064027` | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `transfer_credentials_store.h` | `7909a5c82255458b8562cb4b1f89947bc8cb275f7f2daedf218f102e8934f73a` | `7909a5c82255458b8562cb4b1f89947bc8cb275f7f2daedf218f102e8934f73a` |
| `transfer_http_time.c` | `076c34cc34f497c02d46246dfb75171a3e67cefca3beb0d70b15769f94dc8a14` | `076c34cc34f497c02d46246dfb75171a3e67cefca3beb0d70b15769f94dc8a14` |
| `transfer_http_time.h` | `926b31faa6b5cbebfb866fe1d87981ab6ad6ef7294aa66796ab619d22302b037` | `926b31faa6b5cbebfb866fe1d87981ab6ad6ef7294aa66796ab619d22302b037` |
| `transfer_netif.h` | `4b7553468663a667e2dcc359d08bbd755f706f9d8fa313db65c459f1ef056484` | `4b7553468663a667e2dcc359d08bbd755f706f9d8fa313db65c459f1ef056484` |
| `transfer_network_probe.h` | `fb2c27ce5096fb47394f413ad7b57781adadf934048db1c030203a37dd0f7265` | `fb2c27ce5096fb47394f413ad7b57781adadf934048db1c030203a37dd0f7265` |
| `transfer_ntp_time.c` | `d68e796752df86866660f4761a3c817addc4cfbe26672e0723b9709e2761b87a` | `d68e796752df86866660f4761a3c817addc4cfbe26672e0723b9709e2761b87a` |
| `transfer_ntp_time.h` | `2626d67fb6aff56656c6d06c2e04204bc95cb9f624feb2cb634ff0b698961672` | `2626d67fb6aff56656c6d06c2e04204bc95cb9f624feb2cb634ff0b698961672` |
| `transfer_policy.h` | `99e9fc78a3fb4c1b90e94e1a68762b4078de21a0802a55e3e9788a88c3a58124` | `99e9fc78a3fb4c1b90e94e1a68762b4078de21a0802a55e3e9788a88c3a58124` |
| `transfer_s31_radio_profile.h` | `96a3895f12e4334b15d4a2dcab8d11490f6d195c6a906ea7f74a6dce7891b398` | `96a3895f12e4334b15d4a2dcab8d11490f6d195c6a906ea7f74a6dce7891b398` |
| `transfer_signature.h` | `1bc92d9a4746076248e9fc6694b57353f1f7ed1ca54acbd2ccc57f754f6217cd` | `1bc92d9a4746076248e9fc6694b57353f1f7ed1ca54acbd2ccc57f754f6217cd` |
| `transfer_stream_writer.h` | `8a1ade18758577ddc5bdca57ae1c434b86b8dee1b40a4f5b2ee4591a4933f6c2` | `8a1ade18758577ddc5bdca57ae1c434b86b8dee1b40a4f5b2ee4591a4933f6c2` |
| `upload.html` | `fa3cc38ba8059dd894acfd05d6e806db4fae93da8fa460c7e68a5eeb65a623cd` | `33176163d909ec5a449ef63eddfbe016fcefdf5db40dafea71f95cc5812289bc` |

## 接线与改动理由

- 编译原文件的通用同步 HTTP 分支，保留其路径校验、接收/失败清理、目录分页、文件操作、复制及下载函数。用 `INK_TRANSFER` 选择接线，原有网络/NVS/电源/系统设置/FTP 段原文保留但不编译；不是占位实现。未使用的上游辅助函数只在该文件关闭 unused-function 诊断。
- `ink_adapter.h` 把所有 FILE/DIR/容量操作接到现有策略回调，`/sdcard` 仅是虚拟前缀，真实访问由 `transfer.c` 按授权成员映射 `/me`；没有 POSIX 写卡。接收缓冲 16 KiB 在 PSRAM，文件服务直接写入，不再分配上游 8 KiB stdio 缓冲。
- 通用上传的 `.pico-upload-XXXXXX` 临时名改成目标旁的 `.part`，独占创建由文件服务完成；完成时通过文件服务移动，取消/断开/过期均清理已知临时文件。删除继续走现有文件服务。原子的流式覆盖接口缺失，因此保守拒绝覆盖，保留原文件。
- `ink_server_impl.h` 注册上游网页和文件路由，统一校验一次性授权换取的 HttpOnly/SameSite cookie；每块上传数据再验证授权。停止时 HTTPD 先退出，再释放缓冲；系统统一持有不睡眠需求。HTTPD 使用原通用分支 10 KiB 内部栈；没有上传工作队列或额外线程。
- `/wifi`、`/clock`、`/signature`、FTP 不注册。`/titles` 属于上游阅读器显示名数据库，现有阅读器未接，本次不注册；文件改名走 `/files`。媒体接口只上传普通文件，不设置字体或壁纸。
- `upload.html` 保留成熟中文操作和目录算法。隐藏未接系统控制，增加取消当前 XHR 与授权失效提示；TXT 在发送前计算目录，原书提交后上传 `.toc.json`。批量补目录仍用原函数。
- `pack_upload.py` 确定性 gzip 嵌入网页，保留版权头；文件名和压缩参数固定。
- 二维码原文 `qrcodegen.*` 从原目录平移，来源 CloudZao/Metalio-E-INK4-Plus `874538e`，Nayuki MIT 声明保留。`ink_radio.cc` 的 `FillSsid` 原文及 `StartRadio` 整函数来自同一版本的已接线原文，参数、时序和暂停/恢复钩子不变；许可见 `LICENSE-CloudZao`。当前没有合入 hotspot-1，继续沿用现有热点行为。

## 相对原文的全部修改范围

行号按本文件入库版本；表中空范围用相邻边界表示。

| 文件 | 原文行 | 入库行 |
| --- | --- | --- |
| `read_pico_transfer.c` | 19–19 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 21–21 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 33–32 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 311–322 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 355–355 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 505–505 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 507–509 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 1276–1275 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 1280–1279 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 1440–1441 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 1750–1750 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 2552–2551 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3154–3153 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3176–3175 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3180–3179 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3181–3180 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3202–3201 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3213–3212 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3280–3279 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3281–3282 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3412–3411 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3419–3418 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3432–3431 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3535–3534 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3581–3581 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 3991–3990 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 4197–4196 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `read_pico_transfer.c` | 4225–4224 | `c83989bc3ee844380a13511a17aac1db2eb3041468cef533cf6b85e7d5803e28` |
| `upload.html` | 11–11 | 11–11 |
| `upload.html` | 44–46 | 44–46 |
| `upload.html` | 49–48 | 49–49 |
| `upload.html` | 51–51 | 52–52 |
| `upload.html` | 53–54 | 54–55 |
| `upload.html` | 56–56 | 57–57 |
| `upload.html` | 83–83 | 84–84 |
| `upload.html` | 88–87 | 89–90 |
| `upload.html` | 92–94 | 95–97 |
| `upload.html` | 148–148 | 151–151 |
| `upload.html` | 152–152 | 155–155 |
| `upload.html` | 157–157 | 160–161 |
| `upload.html` | 189–188 | 193–193 |
| `upload.html` | 192–192 | 197–197 |
| `upload.html` | 195–195 | 200–200 |
| `upload.html` | 254–253 | 259–259 |

## 主会话复核后的修改（2026-11-12 晚）

复核发现的问题和改法（只改这几处，原文其余不动）：

- `read_pico_transfer.c`：音乐、图片、字体上传目录改成家人空间里已有的位置——音乐 `/sdcard/音乐`（音乐应用读 `/me/音乐`），图片 `/sdcard/照片`，字体放 `/sdcard/收件箱`（两套实现里各一处，共 6 行）。
- `read_pico_transfer.c`：`receive_file` 打不开临时文件、以及几处“查同名文件”的 `stat` 失败时，名字不合法（`EINVAL`，文件服务每段名字须小于 64 字节）返回 400，不再一律报 507“存储空间不足”。
- `read_pico_transfer.c`：下载中途读失败（到期、换家人）时不发结束块、返回失败，浏览器不会存下截断却显示成功的文件。
- `ink_server.h`、`ink_server_impl.h`：书的根目录从 `/sdcard/books` 改为 `/sdcard/收件箱`（文件应用里显示“传进来的”），旧阅读和基线一样能看到传进来的书；新阅读接入时再把收件箱列进书架。
- `ink_server_impl.h`：浏览器带着上次会话的 cookie 打开首页时仍给输码页，并清掉旧 cookie（原来直接回 403，只能清 cookie）；被拒的请求带 `Connection: close` 并返回失败，不读未授权的请求体。
- `ink_server_impl.h`：停止前先 `shutdown` 所有客户端连接，再 `httpd_stop`，慢速或中断的客户端不再把停止拖到接收超时之后；HTTP 任务栈 10 KiB 改回旧实现的 16 KiB（深层文件夹复制会超过 10 KiB）。
- `ink_adapter.h`：文件服务的“路径不合法”映射成 `EINVAL`；只有上传路由里从 `.part` 改名才记一次“收到”，浏览器生成的 `*.toc.json` 和网页里的复制不计入。
- `system/services/file/transfer.c`：网页服务一次只收一个文件，所以独占创建 `.part` 时遇到已存在的同名 `.part`，就是断电留下的，删掉重建。

未改：`/info` 到期后网页只看到连接失败、看不到“授权已失效”的提示（网页原文的行为，影响小，留待后续）。
