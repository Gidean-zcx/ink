# 第 40 批本地验收

日期：2026-10-27。范围：只读核对正式资料与 `work/research/batch40-integration/before/` 基线；未联网，未重跑 merge。除本报告外未主动修改文件。

## 验收结果

通过。

- 基线为 925F / 1578E；当前为 928F / 1585E。旧 F、旧 E 均无丢失。
- 恰新增 3F：`F-M017-10`、`F-M017-11`、`F-M017-12`；三者均为 D2、`hands_on=false`。
- 恰新增 7E：`E-MMSC-01` 至 `E-MMSC-07`。
- 当前 D4 为 18，与基线一致；没有重复 flow ID。
- `reference-points.csv` 共 26 条，`reference_id` 唯一；所引用 M/F/E 全部存在，附件全部存在。
- `evidence_basis` 只出现四种合法值：`manual_diagram_static`、`official_help_documented`、`official_static_images`、`automated_public_browser`。
- 7 条新增 E 的主附件及全部 `artifact_hashes` 均与实算 SHA-256 一致。
- `E-MMSC-02` 所列 9 幅图、`source-manifest.json`、`visual-observation.json` 均存在，实算散列与 E 中登记值完全一致。
- Khan 指定 10 项（KLC-I005/006/018/028/034/041/046/049/050/051）全部仍为 `index_only_section_fetch_blocked`，缺口文字明确正文仍未读。`E-KLCD-01.additional_captures` 保存两条本批栏目 403 访问审计；本地批次摘要为 2 次 section GET、状态 `[403,403]`、0 次 article GET，10 个结果均 `article_get_attempted=false`。
- `docs/requirements.md` 当前 SHA-256 与基线相同：`e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087`。
- 既有文档检查通过：242 个 Markdown、1915 个本地链接、65 个产品且 65 个均 partial、928F（833 D2 / 77 D1 / 18 D4）、1585E，`errors=[]`。

## 附件散列复核

| 文件 | SHA-256 | 与 E 一致 |
|---|---|---|
| harmony-01.jpg | `8e4889daf64481d8f47a5a93317472b93e1bc2551664a9db0668a236c8a64cf8` | 是 |
| harmony-02.jpg | `97feed78242407e9178e2540898a7e2447d0ce597d56246dc50f39ffd0da4b72` | 是 |
| harmony-03.jpg | `1b809a26b03c96c778fd3d4fa7cebc4a1f13ba598dbce20d3b1145385b7fe3d8` | 是 |
| harmony-04.jpg | `20dec62e7fa4f095634367ceb656f16acb20e3d528e4b975c150d9bb4377b88e` | 是 |
| harmony-05.jpg | `e128d9088ab688c011e80b591f7466fa90708d4eff7e0254f8c38066f913f336` | 是 |
| harmony-06.png | `f307be9a03239cda48a8500935e7513bfe57de287c1d347b11fad031adaaf173` | 是 |
| harmony-07.png | `f6e7205b4e0725d4a51138b5e3d1effe24ff3a845bb7e73a01b7020a32122c6e` | 是 |
| harmony-08.png | `261b7fc320186a7cf8bfef6c99cda532900dcd2a26f910d32a60d96a4a7dc23f` | 是 |
| harmony-09.jpg | `af5b00fbbf93c78e10cbb5e4bb91bee853bf0645e4feb8b32963c1cf4eb4248c` | 是 |
| source-manifest.json | `ee922b817e67733e8497ce19775f5f5b940b6a789539769212eb19500a8ada5a` | 是 |
| visual-observation.json | `bbb5ed26e3e94ee120a39c08e9f64bbaa909e16ccaf2725073ea8f98422af86f` | 是 |

## 执行命令与退出状态

1. 文件定位与基线检查：

   ```bash
   rg --files | rg '(^|/)check_docs\.py$|reference-points\.csv$|source-manifest|visual-audit|batch40' | sort | tail -150
   python3 - <<'PY'
   import json, pathlib
   p=pathlib.Path('work/research/batch40-integration/before-manifest.json')
   print('baseline',p.exists())
   if p.exists():
       d=json.load(open(p)); print('files',len(d),'req',d.get('docs/requirements.md'))
   PY
   ```

   Exit 0。确认 baseline 存在，共登记 1261 个文件，requirements 基线 SHA 正确。

2. 正式资料、批次附件与字段定位：

   ```bash
   find . -name check_docs.py -type f -print
   find docs/research/interaction-library/artifacts/maimemo-input-and-sync -maxdepth 2 -type f -printf '%p\n' | sort
   rg -n 'batch40|第 40|第40|D2|D4|evidence_basis' docs/research/interaction-library/{reference-points.csv,data-dictionary.md,verification-log.md,analysis-status.md} docs/research/interaction-library/batches docs/research/interaction-library/artifacts/maimemo-input-and-sync --glob '*.md' --glob '*.csv' --glob '*.jsonl' --glob '*.json'
   ```

   Exit 0。确认相关正式文件和检查脚本位置。

3. Python 只读验收程序：读取 baseline/current `evidence.jsonl` 与所有 `*-coverage.csv`，比较 F/E 集合；读取 catalog/reference points 检查 ID、引用、附件和枚举；逐项计算新增 E 的 `archived_artifact` / `artifact_hashes`；计算 9 图、manifest、视觉审计及 requirements SHA；读取 Khan inventory、`E-KLCD-01.additional_captures`、`summary.json`、`results.json`。调用形式：

   ```bash
   python3 - <<'PY'
   # 使用 csv/json/pathlib/hashlib/collections 执行上述集合、引用和 SHA-256 只读校验
   PY
   ```

   Exit 0。关键输出：`OLD_F_LOST 0`、`OLD_E_LOST 0`、`NEW_F 3`、`NEW_E 7`、`D4 18`、`REFS 26 UNIQUE_IDS True BASES_VALID True MISSING []`、`NEW_E_ARTIFACT_HASH_ERRORS []`、11 项附件 `HASH ... True True`、Khan 10/10 未读、`KHAN_WORK 2 [403, 403] 0 0`、`REQ ... True`。

4. 既有正式检查：

   ```bash
   python3 work/research/interaction-depth/check_docs.py
   ```

   Exit 0。完整结果：

   ```json
   {
     "markdown_files": 242,
     "local_links_checked": 1915,
     "products": 65,
     "partial_cases": 65,
     "flows": 928,
     "depths": {"D2": 833, "D1": 77, "D4": 18},
     "evidence_records": 1585,
     "distinct_source_urls": 1417,
     "static_visual_evidence": 217,
     "operated_browser_evidence": 19,
     "errors": []
   }
   ```

5. 报告写入：

   ```bash
   cat > work/research/batch40-integration/validation.md <<'EOF'
   ...本报告内容...
   EOF
   ```

   Exit 0。

## 未知与边界

- 本验收检查结构、引用、附件存在性和散列一致性，不重新判断正文是否足够支撑每个产品判断，也不把静态图片视为实际操作。
- Khan 结果只证明落盘的两次栏目请求遇到 403 challenge 并已停止；10 篇目标文章没有发起 GET，因此正文内容继续未知。
- 未联网复验来源当前状态，未重跑 merge。

## 状态变化

仅新建 `work/research/batch40-integration/validation.md`。未修改正式 docs 或任何被验收文件。
