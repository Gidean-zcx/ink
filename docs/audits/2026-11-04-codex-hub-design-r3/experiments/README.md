# 本轮的小实验与边界

2026-11-04，全部在本审计目录内。没有编写中枢实现，没有调用模型 API 或测试真实网站副作用。

## 运行

```sh
python3 docs/audits/2026-11-04-codex-hub-design-r3/experiments/state_checks.py
```

退出码：0。输出：

```text
security_log: local=3 remote=1, valid remote prefix; missing tail is not detected
sync_status: old device backup -> manifest after hub import; B archived without preview
fixed_rules: expired pending pairing returns to UNPAIRED; ACK(V) retains edits after V
latency: first-content-token ceilings adult=2020 ms child=1570 ms (warm, no VAD wait)
latency: conditional extra network adult audio=550 ms child audio=700 ms; not measured
```

## 证明了什么

- 【复现】安全日志：缺少尚未上传的末尾记录，不破坏已经上传部分的哈希链。加密或每条签名也不能单独证明没有这个缺口。未模拟对象存储、restic、真实加密或故障注入。
- 【复现】同步判断表：完整搬家后恢复设备旧备份，会走未知数据集 + 旧 hub_instance 的 manifest 分支，把较新记录移入历史。实验优先执行 complete=false 的否决规则，且不依赖随机碰撞。不是设备同步实现的运行结果。
- 【复现】正例：COMMITTING/BOUND 的认证失败分支已能区分；仅 ACK(V) 不会把 V 后的编辑纳入确认。只是文档规则模型，不证明新功能已经实现。
- 【复现】时延算术：固定非模型耗时与精确剩余预算。跨境增量比较额外假设模型链路增加 150 ms、大陆全部网络耗时共 100 ms；仅用来展示估算的条件性，不作为实测。

脚本只用 Python 标准库、合成标签，运行不写文件，不启动服务、不访问网络。HUB3-003 依据官方文档和授权边界推演，没有把它包装成浏览器实测。

## 产出检查

使用 Python 标准库 `json`、`re`、`pathlib` 与只读 Git 命令核对，最终退出码 0：

- `verification.json` 共 20 条，编号唯一；第二轮 10 条齐全；字段为 id/verdict/evidence/note。
- `findings.json` 仅 HUB3-001～003，字段为 TASK 要求的八项，定级仅严重/高；标题、定级、处理结论与报告相符。
- 引用位置的文件和行号存在；Markdown 相对链接可解析；每个文件小于 1 MB。
- `git diff --name-only` 为空；`git ls-files --others --exclude-standard` 的五个新增文件全在本轮目录；主审对象 HEAD 未变。
- 统计：HUB2 已解决 7、部分解决 2、改出了新问题 1；第一轮指定余项已解决 6、部分解决 4；新问题严重 1、高 2。

准备阶段有两次检查退出 1，均未动产品文件：首次生成 findings 的正则错误地跨行匹配标题，修正后生成成功；首次版本检查发现系统服务分支被其他会话推进。随后对四份依赖文件比较 `77d0101..f41d3cc`，仅 cap.c 增加一行确认策略，已核对行号并固定证据为 f41d3cc。没有以更改产品行为来让检查通过。
