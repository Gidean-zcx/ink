# 家庭中枢第 1 步执行报告

基线：`fdff779f2f2746ab10194e32a00b7dc343365e2c`。分支：`claude/codex-hub-step1`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| H1 | 进行中 | 【实现】Python 3.11.15、uv.lock 和中文 README 已加入；待全新环境复跑 | `uv sync --project hub --frozen` |
| H2、H3 | 已验证第一版 | 【实测】69 项 pytest，含乱序、超限、错误、重连、超时 | `ctest --test-dir build -R '^hub-contract$' -V` |
| H4 | 已验证第一版 | 【实测】132 条自动生成消息，生产 C 解析器全部接受，最高 110 词法单元 | `ctest --test-dir build -R '^hub-json-crosscheck$' -V` |
| H5 | 已验证第一版 | 【实测】当前产品初始化导出 131 条工具、104 个 actions，768 字节缓冲溢出；分页回查不丢定义 | `ctest --test-dir build -R '^hub-' -V` |
| H6、H7 | 已验证第一版 | 【实测】Scope、权限版本、范围反例和 SQLite 队列恢复/去重在上述 pytest 中通过 | 同 H3 |
| H8 | 进行中 | 【实测】本机脚本、向导、预检、worker 探针测试通过；没有 Docker | 同 H3 |
| H9 | 进行中 | 【实测】改动前 86 项原测试全部通过；桥已实现，待改动后全量回归 | `ctest --test-dir build --output-on-failure -j8` |
| H10 | 首次通过 | 【实测】1× 实时替身语音一轮、设备工具调用一次；待连续 5 次 | `ctest --test-dir build -R '^hub-simulator-e2e$' --output-on-failure` |
| H11～H14 | 进行中 | 测量、契约、CI 和结束检查尚在整理 | — |

## 没做到的

- 尚未做最终全量回归、ASan/UBSan、连续五次和全新环境复跑。
- 本机没有 Docker；容器构建、运行与 Linux CI 尚未执行，不能写成验证通过。

## 和设计不一致的

- 本步 `/metrics` 返回开发测量 JSON；Prometheus 格式尚未实现。账号、配对、真实语音及后续能力仍按任务书不做。

## 改过的已有测试

- 无。

## 要用户定的

- 确认 token 编码、自动动作查询的最终参数名、动作范围语义分类尚待后续阶段冻结；保守默认值见 `docs/hub/contracts-step1.md`。

## 顺手发现

- 本任务不访问设备、不调用真实厂商、不推送或创建 PR；仅本地提交。
- 【实测】新代码开发中发现并修复成员切换与设备回复同时发生的取消竞态，保留回归用例；最终反向对照待补。
