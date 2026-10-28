# Batch44 本地文档验收

日期：2026-10-27。范围：按要求运行一次幂等 merge 和正式检查，并只读核对 Batch44 新资料；未联网，未扩大产品研究。

## 结果

通过。结构检查通过不代表BOOX行为已经在设备上验证。

- `merge_batches.py` exit 0：65产品/65案例，938F（841 D2、77 D1、20 D4），1661E、1493个不同URL、226条静态视觉E。
- `check_docs.py` exit 0：255 Markdown、2091本地链接，聚合数量同上，21条浏览器操作E，`errors=[]`。
- 相对 `before-manifest.json`：正式文件新增13、修改23、删除0。完整路径见 `worker-validation.json.formal_diff`。
- `docs/requirements.md` SHA-256仍为 `e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087`。
- `reference-points.csv`共68条、ID唯一、15字段均非空，全部M/F/E引用和artifact存在。Batch44新增12条为 `RP-M004-01～10` 与 `RP-M010-01～02`。
- `boox-system-content-deepening-evidence.jsonl`恰25E，全部`hands_on=false`。
- `boox-system-content-deepening-coverage.csv`恰7F，全部D2、`hands_on=false`。
- `source-captures.json`含31条source，31个`article_id`唯一。
- 新E的`artifact_hashes`合计关联7个互异静态图文件；7图均存在，SHA-256全部匹配。
- 根README保留第44期主题入口及“全部研究简报”入口，没有直接保留第43期链接；`docs/briefs/README.md`包含第43期，并恰有001～044共44个唯一简报链接。

## 执行命令与退出状态

1. 合并与检查：

   ```bash
   python3 work/research/interaction-depth/merge_batches.py
   python3 work/research/interaction-depth/check_docs.py
   ```

   两者均exit 0。stdout分别保存为 `merge-output.json`、`check-output.json`，stderr文件为空。merge只重写生成索引，检查随后证明master与批次一致。

2. `find`定位新BOOX批次、artifact和Batch44 integration文件；`head`读取RP表头；`ls`读取简报目录：exit 0。

3. Python读取 `source-captures.json`、新E/F、RP、README、brief索引及基线manifest，计算正式文件SHA差异和附件散列：

   - 首次脚本尝试读取不存在的 `before/.../reference-points.csv`，exit 1；Batch44基线只提供manifest，不含before文件树，未产生正式修改。
   - 改用已明确的新RP稳定集合 `RP-M004-01～10`、`RP-M010-01～02` 后运行；第一次把brief索引中的两个导航链接也计入，得到46并exit 1。
   - 将统计限定为001～044编号简报后最终exit 0，`status=PASS`。

4. 写入 `worker-validation.json/md` 并计算散列：exit 0。

## 证据文件

- 基线：`work/research/batch44-integration/before-manifest.json`
- 新批：`docs/research/interaction-library/batches/boox-system-content-deepening-evidence.jsonl`、`boox-system-content-deepening-coverage.csv`
- 来源清单：`docs/research/interaction-library/artifacts/boox-system-content-deepening/source-captures.json`
- RP：`docs/research/interaction-library/reference-points.csv`
- 简报入口：根`README.md`、`docs/briefs/README.md`
- 机器结果：`work/research/batch44-integration/worker-validation.json`

## 未知与状态变化

- 本次只验证数量、字段、引用、文件存在和散列；没有重读31篇正文或重新查看7张图的语义。
- `source-captures.json`的唯一性检查按`article_id`；不判断上游文章内容是否后来变化。
- merge按任务明确执行并写回已有生成索引；除此之外未编辑正式资料。状态变化限于生成的merge/check输出、验证脚本及worker报告。
