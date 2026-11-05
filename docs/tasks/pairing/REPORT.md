# pairing 执行报告

基线：`103382b1`。仅完成 backlog 的 pairing 服务任务；实现提交至 `b6acd510`。所有身份、凭据和资料均为合成夹具，没有接触设备或真实家庭服务器。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| P1 配对状态机、配网与令牌隔离 | 通过（模拟器） | 【实测】五个持久状态、独立保存 Wi-Fi/地址/身份、固定公钥、同公钥换地址、待生效令牌过期及已绑定认证失败 | T2、T4 |
| P2 热点、小网页、存储卡配对文件 | 通过（模型） | 【实测】随机密码热点、本地表单提交、启动自动导入、一次性文件删除；配对完成但 Wi-Fi 未保存时重启可续接 | T2 |
| P3 掉线、确认丢失、重启恢复 | 通过 | 【实测】begin/邀请/accept/complete/hello 的重连和重启、正常回包与显式丢失、4 类拒写；18 次恢复启动及 8 次拒写启动 | T2、T3 |
| P4 吊销和数据处理 | 通过 | 【实测】移除后停止重连，重启保留私有 Flash/卡资料；人重新加入后回未配对；仅布尔 wipe:true 清空并恢复启动 | T2、T3 |
| G1 全量、内存、预算 | 通过 | 【实测】普通 95/95、43.84 秒；ASan/UBSan 95/95、78.63 秒；均无跳过，预算未改 | T4、T5 |
| G2 原子能力与成员隔离 | 通过 | 【实现/实测】8 个调用能力、1 个事件及脱敏状态；网页走 pairing.configure / wifi.connect，绑定成员和代次；认证前普通调用、回执和媒体均不外发 | T2、T4 |
| G3 依赖状态 | 通过 | 【核查】只改“配网和家庭绑定”的状态格 | T6 |
| G4 页面验收 | 不适用 | 服务任务；BootClaim/BootConfirm 等页面属 boot-flow，本次提供状态和能力，不启动窗口 | — |
| G5 修改范围 | 通过 | 【核查】仅服务、策略、HAL 模型、启动装配、测试及报告；未改其他任务与需求文档 | T6 |
| 隐私、文件与链接 | 通过 | 指定路径/MAC 模式 0 命中，空白检查退出 0；23 个变更文件均小于 5 MB，报告链接无缺失 | T6 |

实现见[配对服务](../../../system/services/hub/pairing.c)、[中枢连接](../../../system/services/hub/hub_link.c)。热点网页只在模拟器内部建模，没有宿主 HTTP 监听。原有 proto:1 开发场景保留；配置家庭服务器后走 proto:2。长期令牌只在内部 Flash，公开快照不含令牌、一次性码或 Wi-Fi 密码。

T1：按总规则 §5 配置。两个目录均 `INK_WERROR=ON`、`INK_GUI=OFF`，字体、Python、LVGL 使用 `$INK_DEPS`；内存目录另设 `INK_SANITIZE=ON`。`INK_HUB_PYTHON` 指向 `work/pairing-hub-venv/bin/python`，用 uv 0.9.5 按 `hub/uv.lock --frozen` 安装；没有升级锁文件。

```sh
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
```

- T2：`ctest --test-dir build -R '^pairing-' --output-on-failure -j2`，2/2；[恢复矩阵](../../../tests/pairing/run.py)和[拒写矩阵](../../../tests/pairing/storage.py)。
- T3：`python tests/pairing/reverse.py --build build`，10/10。忽略目录中逐项撤掉保护，原测试均失败；异常清空变体触发了不该发生的重启。覆盖确认门槛、两处落盘、吊销状态、回包发送完成、wipe 类型、空字符、普通回执隔离、未确认交易恢复及转义后的握手长度。生产源码未撤回。
- T4/T5：上面两轮全量命令，顺序执行；日志在 `work/pairing-validation/full.log`、`asan-build.log`、`asan-full.log`。
- T6：`git diff --stat 103382b1..HEAD`、`git diff --check 103382b1..HEAD`、总规则指定路径/MAC 模式扫描和本地链接核对。额外用同一编译器对 pairing、hub_link、wifi 加 `-Wsign-compare -Werror=unused-result -fsyntax-only`，均退出 0；新增代码没有 POSIX read/write/fgets 调用，HAL 读写检查返回值。

## 没做到的

- 实机 HAL/TLS/NVS、手机真正弹出网页、真实家庭服务器及 Linux/gcc 没有实测；不把模拟器结果外推为这些验证。开机页面没有越过本节范围开发。

## 和设计不一致的

- 设计写 NVS；本模拟器复用已有内部 Flash 原子存储，分别保存 `pair.config` 和 `pair.identity`。真机 NVS 映射仍需设备端口实现，长期令牌没有写入存储卡。

## 改过的已有测试

- 仅 `tests/integration/ai_surface.c` 新增 pairing.status、7 项仅供人的操作、pairing.state 和 pairing.changed 的期望。没有删改旧期望、预算、跳过规则或超时。

## 要用户定的

- 服务器对接时需冻结配对文件/链接的具体编码。当前卡模型暂用 JSON：`url`、`pin`，可选 `console`、`preauth`、`ssid`、`password`；公钥接受小写 SHA-256 十六进制或 sha256/base64。控制台地址默认同主机 443，密码只交设备。本次没有把这些暂定字段升级为产品需求。

## 顺手发现

- 原状态监听名额紧张，本任务没有新增监听名额或预算。卡文件在启动时自动读取，也可显式调用 pairing.import。
- 最紧 AI 场景的模拟内部 RAM 最低余量为 32880 字节，原预算是 32768 字节。配对服务复用共享锁，落盘信号量用完释放，没有新常驻任务。
