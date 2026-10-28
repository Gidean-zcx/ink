# Batch44 work目录存储审计

日期：2026-10-27。只读盘点；未删除任何文件，未修改正式资料。

## 结果与建议顺序

当前根分区仅约80 MiB可用，`work/`约2.06 GB。本次识别三类空间：

| 类别 | 项数 | 字节 | 建议 |
| --- | ---: | ---: | --- |
| Chromium纯缓存目录 | 8目录 | 47,389,235 | **优先可删**；只删Cache/Code Cache/GPUCache/Shared Dictionary cache，保留profile其余Cookies/Local Storage/会话状态 |
| integration `before/`中与当前正式文件逐字节相同的副本 | 3,067文件 | 40,818,534 | **可删所列单文件**；每项均有正式保留路径和相同SHA。不要整删before目录，变更前版本仍可能是唯一基线 |
| work其他位置与正式docs完全相同的副本 | 811文件 | 272,965,087 | 内容已有正式副本，但很多E记录仍写`raw_work_path`；删除会形成失效临时路径。仅在接受work原路径失效后分批删，不列为第一优先 |

前两类合计 **88,207,769 bytes**，已经超过当前紧急空间需求，并且不需要触碰浏览器二进制、依赖包、唯一原件或正式资料。

完整逐文件清单在 [candidates.json](candidates.json)：

- `before_exact_duplicates[]`：每项含`delete_path`、`bytes`、`sha256`、一个或多个`retain_paths`。
- `other_work_exact_duplicates[]`：同样逐项给出正式保留路径和相同SHA，但有raw路径失效风险。
- `browser_cache_directories[]`：精确目录与实际文件字节数。
- `totals`：全部分类统计。

## 第一优先：浏览器缓存目录

可删除的精确目录：

- `work/research/batch38-week/desktop-profile/Default/Cache`
- `work/research/batch38-week/desktop-profile/Default/Code Cache`
- `work/research/batch38-week/desktop-profile/Default/GPUCache`
- `work/research/batch38-week/desktop-profile/Default/Shared Dictionary/cache`
- `work/research/batch43-time-sample/profile/Default/Cache`
- `work/research/batch43-time-sample/profile/Default/Code Cache`
- `work/research/batch43-time-sample/profile/Default/GPUCache`
- `work/research/batch43-time-sample/profile/Default/Shared Dictionary/cache`

合计47,389,235 bytes。这些是Chromium派生缓存，没有正式“保留文件SHA”对应物；安全性来自目录语义。必须保留两个profile的其他目录，尤其Cookies、Local Storage、IndexedDB、Preferences及会话状态，因为此前恢复观察可能依赖它们。不得删除项目浏览器二进制或`work/research/tools/`。

## 第二优先：before快照中的精确重复文件

3,067个候选逐项通过以下条件：

1. 路径位于 `work/research/batch*-integration/before/`。
2. 去掉`.../before/`前缀后存在当前正式文件。
3. 文件字节数相同。
4. 双方SHA-256相同。

按批次占用：

- batch43：9,811,846 bytes
- batch42：9,322,611 bytes
- batch41：8,890,507 bytes
- batch40：6,589,442 bytes
- batch39：6,189,252 bytes
- batch38：14,876 bytes

媒体扩展名（PNG/JPG/GIF/MP4/PDF/音频）匹配数为0：这些较新快照按既有策略没有复制大量正式媒体。主要冗余是Markdown、CSV、JSON/JSONL及操作日志。最大项是batch41/42/43各自重复的`artifacts/week-reader-recovery/observed-states.json`，每份839,559 bytes，正式保留件为 `docs/research/interaction-library/artifacts/week-reader-recovery/observed-states.json`，SHA逐项相同。

只能按JSON清单删除精确匹配项，不能删除整个`before/`：未列出的文件可能是正式文件后来已修改的旧版本，仍是差异审计基线。

## 第三类：其他work与正式docs的重复副本

识别811项、272,965,087 bytes，内容层面均有正式同SHA副本。最大来源：

- `work/douyin-controls-deepening/`：79,960,590 bytes
- `work/research/`（不含本审计与tools）：40,657,150 bytes
- `work/time-controls-deepening/`：21,403,374 bytes
- `work/xiaoyuan-app-controls-deepening/`：19,716,938 bytes
- `work/iflytek-planning-reading-deepening/`：14,377,532 bytes
- `work/bcz-learning-video-preview/`：13,052,107 bytes
- `work/week-junior-controls-deepening/`：11,581,329 bytes
- `work/iflytek-video-controls-deepening/`：10,459,474 bytes

例如抖音raw中的多张8MB级指南图与正式`artifacts/douyin-controls-deepening/`逐字节相同。这批仍可能被正式E的`raw_work_path`引用；删后正式归档和结论不丢，但临时原件路径会失效。因此本报告不把它们归为紧急“无副作用”删除项。若后续确需释放更多空间，应先从JSON挑大文件，确认对应E已有`archived_artifact`/`artifact_hashes`，并接受raw路径失效后再删。

## 明确保留

- `docs/**`全部正式资料。
- work中没有正式同SHA副本的原始HTML、PDF、视频、trace、响应头和失败页。
- 浏览器profile中除明确8个缓存目录外的所有状态文件。
- `work/research/tools/`（约734MB），其中可能含浏览器二进制、PDF库和包；按任务要求不列删除候选。
- 任何node_modules、浏览器二进制、包缓存和研究工具链，本轮未分析为可删项。

## 执行命令与退出状态

1. 容量总览：

   ```bash
   df -h /home/workspace/reader
   du -x -B1 -d 2 work | sort -nr | head -n 100
   ```

   Exit 0。根分区约80MB可用，work约2.06GB，tools约734MB。

2. Python扫描所有 `batch*-integration/before/**`，按对应正式路径比较size和SHA-256：exit 0；当时输出3,107项/40,835,259 bytes。随后正式资料继续变化，本报告生成器以最终当前磁盘重算为3,067项/40,818,534 bytes，采用后者。

3. `find work`定位`Cache`、`Code Cache`、`GPUCache`、`Service Worker`、`CacheStorage`并用`du`统计：exit 0；只发现上述两个研究profile的8个缓存目录。

4. 首次写审计脚本前父目录尚未创建，shell重定向失败，命令exit 2；没有文件或删除副作用。执行`mkdir -p work/research/batch44-storage-audit`后继续。

5. `python3 work/research/batch44-storage-audit/build_audit.py`：exit 0。跳过`work/research/tools/`、node_modules和本审计目录；对docs及其余work逐文件流式算SHA，生成完整JSON。最终统计：before 3,067/40,818,534；其他精确副本811/272,965,087；缓存8/47,389,235。

6. Python按类别汇总批次和顶层目录、抽查最大候选及正式保留路径：exit 0。

7. 写入本报告并计算报告/JSON SHA：exit 0。

## 未知与状态变化

- `du`按文件系统块计费可能与逐文件`st_size`总和略有差异；报告字节使用实际文件长度，实际释放空间可能稍高。
- 未分析tools内部包缓存，因为明确要求保留浏览器二进制和包；也未判断哪些依赖未来可重装。
- 第三类精确副本虽内容可恢复，但删除后会使部分`raw_work_path`失效；是否接受由根代理决定。
- 本轮没有删除、移动或压缩任何候选。状态变化仅新增 `build_audit.py`、`candidates.json` 和本报告。
