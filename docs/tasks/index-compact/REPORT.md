# index-compact 执行报告

基线：`5e1e3964b5aafae3587b805c2edcf13922cccd44`。工作分支 `claude/codex-index-compact`，开工工作区干净。实现、计量、定向测试与反向对照完成，全量、内存检查与 S31 编译待完成。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过【实测，模拟器】 | 下表及 [before.csv](before.csv)、[after.csv](after.csv)；重新从基线源码编译的汇总、逐次 CSV 与开工测量完全一致 | 下文计量命令 |
| 2 | 定向通过【实测】，待全量复核 | schema 1/2 的 6 个种子、升级、重启阶段通过，编号均为 `[2, 1, 3]`；重启目录读取、索引写盘均为 0；155 B 中文路径持久化回读；换回基线后 6 阶段均预期失败 | `ctest --test-dir build -R '^(index-compact|index-durable|file-index-3)$' --output-on-failure -j2`；下文反向命令 |
| 3 | 待测：全量、内存检查及 S31 编译 | 待记录 | 待记录 |

## 计量与实现

同一工具、`metalio-plus`、种子 1、`tail` Flash 时序，关闭随机断电。每组为 1000 个合成文件首次建索引；耗时均为虚拟时间，单位 ms。`prepare` 测首次追加和压缩；`probe` 在压缩已被存储任务取走后提交另一集合的同步写盘；`control` 是空卡同内容写盘。没有改存储服务、数据帧、队列、调度或测量断言。

| 测量项【实测，模拟器】 | 基线 | schema 3 |
| --- | ---: | ---: |
| 首次追加字节 | 252000 | 92000 |
| 首次追加耗时 | 4614.425 | 1602.159 |
| 压缩字节（含快照头） | 252032 | 92032 |
| 压缩耗时 | 4426.503 | 1659.688 |
| 竞争探针的压缩耗时 | 4428.007 | 1660.259 |
| 等待任务排队与索引写盘重叠 | 4427.878 | 1660.130 |
| 等待任务全部排队时间 | 4428.110 | 1660.379 |
| 同步等待总耗时 | 4442.595 | 1683.199 |
| 空卡对照与索引重叠 | 0 | 0 |

字节量减少约 63.5%；竞争时索引直接造成的排队减少约 62.5%，仍约 1.66 秒，不能宣称停顿消失或等于真机结果。

【实现】产品代码只改 `file_service.c`，55 行增加、19 行删除，共 74 行；另有 4 行 CMake 注册测试。磁盘 payload 为卡身份 16 B、大小 8 B、修改时间 8 B、UTF-8 路径及结尾零。读入口统一解码，写入口统一编码，共用路径长度/终止符/内容区前缀校验，另检查 schema 与精确长度。元数据通过 `memcpy` 读写，避免未对齐访问。schema 1/2 均读取固定 216 B；原编号交给既有扫描更新，schema 2 的改名匹配继续使用元数据。完成记录的固定 payload 和原有落盘流程保留，版本随索引升为 3，使旧完成标记触发既有后台补建。

155 B 是前次报告给出的映射后路径上界；本次用 155 B UTF-8 路径直接验证同一编解码与真实持久化，payload 恰为 188 B，进程重启后经扫描和单条读取均能取回，不把它称为长度上限为 128 B 的公开虚拟路径。

计量和反向复现（均先按总规则 §5 配置、构建；依赖只读，最多并行 2）：

```bash
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_cost/run.py --build build --baseline 5e1e3964b5aafae3587b805c2edcf13922cccd44 --cases prepare probe control
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_cost/run.py --build build --cases prepare probe control
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/run.py --build build
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/run.py --build build --baseline 5e1e3964b5aafae3587b805c2edcf13922cccd44
```

【实测】定向测试 3/3，退出 0。反向对照只在构建目录替换文件服务，基线 6 阶段失败数为 `1, 2, 2, 1, 2, 2`，仅变长布局及升级后 schema/长度断言失败；工具确认这些预期失败后退出 0。ctest 新测试直接构造旧 payload，不依赖 Git 历史。原始日志在 `work/index-compact/`，保留的重启镜像和指标在 `build/test-out/index-compact/`。

## 没做到的

- 全量、内存检查与 S31 尚在验证，不宣称已通过。未连接设备或运行 Linux/远端 CI。

## 和设计不一致的

- 无本次新增差异。【实现：架构审视】这是文件服务内部记录编码，用户和 AI 仍走原有文件能力及成员鉴权；没有页面、能力、软总线边界变化。

## 改过的已有测试

- `tests/index_durable/device.c`：v1 升级后的期望由 schema 2、216 B 改为 schema 3、`32 + 路径字节数 + 1`，契合本任务要求；没有放宽断言或预算。
- `tests/file_index_3/boundary.c`：路径观测助手兼容 schema 3 的路径偏移；保留对 schema 1/2 基线的读取，不改已有测试期望。
- `tests/index_cost/run.py`：仅增加手动 `--baseline` 对照入口，原计量及断言不变。

## 要用户定的

- 暂无。

## 顺手发现

- 暂无。

## 新增的机制

- schema 3 变长 payload：任务书第 1 项明确要求。沿用既有任务、完成记录与后台补建；无新增任务、状态机、配置项或迁移流程。

## 要改的文档

- 主会话可在 `docs/system/system-services-design.md` 同步 schema 3 布局、schema 1/2 随后台补建升级及本次模拟器计量；共享文档与历史调研均未改。
