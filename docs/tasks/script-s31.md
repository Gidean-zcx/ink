# script-s31：补完框架原型 F10–F12 的测量（MicroQuickJS 候选，以及 S31 上的体积和内存）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/script-s31/REPORT.md。

## 要做的

docs/system/app-framework-design.md 的 F10（描述加载）、F11（Lua）、F12（MicroQuickJS）是有界原型，**只测量，不选赢家、不进出厂固件**（第 21 行）。§6.5 已有 C、受限 JSON、Lua 5.4.8 三个候选在主机上的结果（tools/appfw-spike），缺两样：

1. **MicroQuickJS 候选**：照 tools/appfw-spike 里 Lua 的做法加第四个候选（同一测试页、同一控件和能力桥、同一计量）。源码按固定版本下载并核对 SHA-256（照 Lua 那样，许可证放 tools/appfw-spike/licenses/），不放进 third_party、不进产品构建。记录子集限制、执行中断、退出清理（F12 那一行的要求）。
2. **S31 上的数**：用刚合入的 ports/esp32s31，为每个候选做一个只用于测量的构建配置（不改默认固件），给出相对默认固件的 Flash、内部 DIRAM、PSRAM 静态增量（`idf.py size` 和链接图差分）。能跑的部分不需要上机，只要编译和链接数字。
3. 把结果补进 §6.5（主机表加 MicroQuickJS 一行，另加 S31 静态增量表），写明是【实测】编译数字、【估算】还是没测。

## 边界

- 不改产品代码；不往默认固件里加任何解释器。
- 不连设备、不烧录。
- 同时在做的有 perf-memory、perf-latency、factory-pages（修复中）、file-index-3（修复中），不要改它们的文件。

## 验收

1. MicroQuickJS 候选在主机上跑通 appfw-spike 的同一套检查（真实触摸、AI 调同一能力、旧版本拒绝、非法能力和参数、页面释放），并有计量结果（测试或脚本输出）。
2. 四个候选在 S31 上的静态增量表（报告和 §6.5）。
3. 默认固件不变：`ports/esp32s31/build.sh` 照样通过，尺寸不变；模拟器全量测试通过。
