# Batch 35 本地机械验证

验证时间：2026-10-27（Asia/Shanghai）。范围：只读现有正式资料、`work/research/batch35-integration/before/` 快照及本地原件；未联网、未执行来源 `SKILL.md`、未运行旧 builder、未修改 `docs/`。本记录是本次唯一有意写入。

## 结论

- **通过**：最终 `check_docs.py` 退出码 0；226 个 Markdown、1669 个本地链接、65 产品/65 部分案例、899F、1517E，`errors=[]`。
- **通过**：相对 batch35 before 快照，新增集合恰为 `changes.json` 所列 11F/15E；旧 F 只有 `F-M015-26` 变化，旧 E 只有 `E-RWCD-17` 变化。
- **通过**：11 个新 F 全部为 `depth=D2`、`hands_on=false`；15 个新 E 的 `locator` 均为非空字符串。这里仅核字段存在和可定位格式，不替代 root 对定位语义的来源审核。
- **通过**：`docs/requirements.md` SHA-256 保持 `e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087`。
- **通过**：15E 引用的正式摘记、原始文件、文本文件和 3 张正式截图全部存在，逐项实算 SHA 与 E 记录一致。
- **通过**：新 Readwise 目录 14 行、ID 唯一；新小猿历史目录 12 行、ID 唯一；`inventories/README.md` 中两条相对链接均可解析到对应文件。
- **通过**：旧 Readwise 目录仍为 281 行且 ID 唯一，SHA-256 `f690b2375d7484f30b137c9386c3e1fa2a3ff583572a98ee291c0d775d1ed7a1`，与 `work/xiaoyuan-hardware-controls-deepening/baseline.json` 等既有基线相同。旧小猿硬件目录仍为 33 行且 ID 唯一，SHA-256 `882c7bd92b1d561448575d84d2d01b2e97cc8a5de54eab0964f18650f7d717a1`，与 `work/xiaoyuan-hardware-controls-deepening/manifest.json` 相同。

## 关键哈希

- Readwise 新正式摘记：`151f5c756ce365487065045a89ee8949fab179285bc34c154cfe6788cb11ca9b`；13 个 E 中登记值一致。
- 小猿历史新正式摘记：`4a1d64ffbde54b15edc987a40f4290e235f4d85d4e74a37382f660ac29e935c0`；2 个 E 中登记值一致。
- Readwise 新 14 项目录：`95c474a571734b2091aa5d45db0a21026ea18d291b6fcfb02b6dc3986326c212`。
- 小猿历史新 12 域目录：`f100f5df2a88349b38b0c491ca351a3c429ac8a174e878b7275472bdc373ecae`。
- 原件核验覆盖：Readwise `README.md`、11 份固定提交工作流文件及 `web-tool-docs.json`；小猿两份 HTML、两份文本和 3 张正式抽帧。共 35 个“记录字段→文件→实算 SHA”检查，全部一致。

## 精确命令与退出状态

1. 最终文档检查（root 通知主表重合并完成后重跑）：

```bash
python3 work/research/interaction-depth/check_docs.py
```

退出码：`0`。关键输出：`flows=899`、`evidence_records=1517`、`errors=[]`。

2. 主批差异、字段、哈希、目录及链接验证：

```bash
python3 /tmp/batch35_validate.py > /tmp/batch35-validation.json
```

退出码：`0`。脚本以 `csv.DictReader`/逐行 JSON 读取当前主表和 `work/research/batch35-integration/before/`，读取 `changes.json`，实算 SHA-256，并解析 `inventories/README.md` 的两条新目录链接。关键布尔结果均为 `true`：`flow_added_exact`、`evidence_added_exact`、`expected_old_changes`、`new_flows_D2_false_ok`、`new_evidence_locators_ok`、`requirements_ok`、`file_hash_checks_ok`、`new_inventory_refs_ok`。

3. 旧目录基线复核：

```bash
rg -n 'f690b2375d7484f30b137c9386c3e1fa2a3ff583572a98ee291c0d775d1ed7a1|882c7bd92b1d561448575d84d2d01b2e97cc8a5de54eab0964f18650f7d717a1' work -g '*.md' -g '*.json' -g '*.txt'
```

退出码：`0`。Readwise 哈希命中多个既有 baseline；小猿哈希命中既有 manifest。

4. 初次临时验证脚本运行：

```bash
python3 /tmp/batch35_validate.py > /tmp/batch35-validation.json
```

退出码：`1`。原因是临时脚本把旧小猿目录主键误设为 `item_id`，实际表头为 `inventory_id`，在读取阶段抛出 `KeyError`；未写正式资料、未产生部分产品状态变化。随后只修正 `/tmp` 脚本为读取首列表头并完整重跑，结果见第 2 项。

## 差异与不确定性

机械差异符合预期：旧记录仅 `F-M015-26`、`E-RWCD-17` 改动；11F/15E 恰为新增集合，没有发现额外旧 F/E 变化。验证期间 root 曾更新 Readwise 摘记及关联 E 哈希并重新合并主表；本记录采用 root 明确“主表合并完成（899F/1517E）”后的最终快照，未把中间态判为失败。

本检查没有判断来源文字是否足以支撑每个 `known_branches`、`gaps` 或 locator 的语义边界，也没有执行任何外部 AI 工作流或设备/App 操作；这些仍由 root 的独立来源审核承担。

## 状态变化

正式产品/研究资料状态未由本验证修改；`docs/` 未写入。新增文件仅为本验证记录 `work/research/batch35-integration/worker-validation.md`；临时生成 `/tmp/batch35_validate.py` 与 `/tmp/batch35-validation.json`。

后续说明：在上述机械验证完成后，root 告知将把 `F-M015-26` 中同一事实的措辞明确为“通过 CLI 关闭只读模式需重登”，并同步 batch/case/changes/master。该修订仍落在已确认的唯一旧 F `F-M015-26` 内；按 root 指示未重复全库检查。本报告的哈希与主表内容快照均截止该措辞修订之前，正式最终值以 root 修订及其独立校验记录为准。
