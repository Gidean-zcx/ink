# 家庭中枢第 1 步执行报告

基线：`fdff779f2f2746ab10194e32a00b7dc343365e2c`。分支：`claude/codex-hub-step1`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| H1 | 通过 | 【实测】独立新环境按锁文件安装，CLI 就绪事件及 healthz/setup 检查、84 项测试通过 | `uv sync --project hub --frozen`；`python tests/hub/check_fresh.py --out work/fresh-cli-check` |
| H2、H3 | 通过 | 【实测】84 项 pytest，含乱序、超限、错误、重连、超时 | `ctest --test-dir build -R '^hub-contract$' -V` |
| H4 | 已验证第一版 | 【实测】132 条自动生成消息，生产 C 解析器全部接受，最高 110 词法单元 | `ctest --test-dir build -R '^hub-json-crosscheck$' -V` |
| H5 | 已验证第一版 | 【实测】当前产品初始化导出 131 条工具、104 个 actions，768 字节缓冲溢出；分页回查不丢定义 | `ctest --test-dir build -R '^hub-' -V` |
| H6、H7 | 已验证第一版 | 【实测】Scope、权限版本、范围反例和 SQLite 队列恢复/去重在上述 pytest 中通过 | 同 H3 |
| H8 | 本机通过，Docker 未实跑 | 【实测】脚本、向导、预检、worker 探针通过；Compose 校验已放入 Linux CI，本次没有触发 | 同 H3；`sh -n hub/deploy/install.sh` |
| H9 | 通过 | 【实测】普通与 ASan/UBSan 各 91 项通过，含原 86 项与确定性；旧代次/关闭后回调检查通过 | `ctest --test-dir build --output-on-failure -j8`；同命令换 `build-asan` |
| H10 | 通过；正在最终复跑 | 【实测】连续 5 次 1× 实时替身语音一轮、设备工具调用一次；缺环境退出 77，旧样本会清掉 | `ctest --test-dir build -R '^hub-simulator-e2e$' --repeat until-fail:5 -V` |
| H11～H14 | 进行中 | 待最终数字入 §4.3、报告逐项展开与结束扫描 | — |

## 没做到的

- 本机没有 Docker；容器构建、运行与 Linux CI 尚未执行，不能写成验证通过。

## 和设计不一致的

- 本步 `/metrics` 返回开发测量 JSON；Prometheus 格式尚未实现。账号、配对、真实语音及后续能力仍按任务书不做。

## 改过的已有测试

- 无。

## 要用户定的

- 确认 token 编码、自动动作查询的最终参数名、动作范围语义分类尚待后续阶段冻结；保守默认值见 `docs/hub/contracts-step1.md`。

## 顺手发现

- 本任务不访问设备、不调用真实厂商、不推送或创建 PR；仅本地提交。
- 【实测】取消竞态、权限版本下的迟到输出、关闭其他会话、资源探针受限、作业执行中权限变化的保护，撤掉对应保护后定向测试均失败；已恢复，最终测试通过。反向日志在忽略的 `work/reverse-*.log`。
