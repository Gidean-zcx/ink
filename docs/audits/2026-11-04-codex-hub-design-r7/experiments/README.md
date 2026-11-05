# 第七轮核查辅助材料

本目录不含中枢实现或产品行为实验。没有运行模型、浏览器、服务或设备。

- [source-verification.json](source-verification.json)：审查对象、比较基线、固定提交的相关补丁，以及三份本地文档的 SHA-256。初始 `git log -5 --oneline` 成功确认 HEAD 为 `9c050dc`；随后本地 Git 元数据读取挂起，相关进程已终止。通过 GitHub 只读接口取基线文件和 `bf35c7f`、`9c050dc` 两个固定提交的补丁，在内存逐个应用，检查每个 hunk 的旧内容、行数和新位置，最终与三份本地文档完整匹配。没有把补丁应用到工作树。
- [validate_artifacts.py](validate_artifacts.py)：只核对本轮 JSON 格式与统计、报告中的本地链接及显式行号、审查文档指纹和本目录文件大小。只写本目录的 `validation.json`，不运行产品代码。

交付检查命令：

```sh
python3 -B docs/audits/2026-11-04-codex-hub-design-r7/experiments/validate_artifacts.py
```

`validation.json` 的通过只说明审计材料检查通过，不代表中枢实现已通过测试。无法完成最终 `git status` 的限制也在其中注明；本轮写入范围依据实际工具操作记录，不声称全仓重新检查通过。
