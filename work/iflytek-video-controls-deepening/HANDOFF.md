# 第二十七批冻结交接

2026-10-27。本批已合并和检查，整体目标仍active。65组全部部分覆盖；没有产品实施。

## 根任务和结果

M027四条官方帮助视频抽帧，4新E（IFVC01–04）、3新D2（F16–18）、2旧F（01/14）11字段、1旧E（ILD17）追加。F01从D1补局部D2，保留同题错误链缺口。27必要摘帧+1目录图；原70项台账只更新4项，其余30功能视频/24问题视频未观看。源视频未保存MP4，没有听音/设备操作。完整方法/工具失败见正式source-notes与verification-log。

M003：5新E/7新D2、2旧F7字段、1旧E采集、5页图；65目录更新快照26项复读。M006：0新E/5新D2、3旧F10字段、2旧E采集、5页图；140目录快照29项复读。两个助手已冻结；不要再运行其合并前check或build。助手HANDOFF及manifest各留原目录。

合计15新D2、9E，7旧F28字段和4旧E追加。总表846F=753D2+78D1+15D4，1391E/1223不同URL，185静态视觉E/18公开网页操作E。没有新增D4。正式新图38幅不是38次产品操作。

## 已执行，禁止重跑

- `build_batch.py`：一次，exit0。构建根来源摘记、视频元数据/PNG/网络元数据、4E/3F及旧行建议，回写M027案例和四目录行。
- `merge_reviewed.py`：一次，exit0，保存逐字段before/after及旧批备份；`applied-reviewed-updates.json`是合并审计。
- 内联摘要更新：一次，exit0，`summaries-updated.json`作为标记；简报27、README、比较、目录、缺口、analysis-status、ideas及guide实例已更新。
- `formal-manifest.json`：根36项正式文件散列，单独记录；不含全局摘要避免循环。

`work/research/interaction-depth/merge_batches.py`由root运行，exit0；它从所有正式批重建总表和案例索引。不要运行旧build_indexes.py或旧批构建脚本。

## 可重复的合并后检查

`python3 work/iflytek-video-controls-deepening/verify_batch.py`：已exit0/PASS，核80冻结文件、所有合并字段/备份、68已链接CSV/6783行与需求SHA。结果存verification-result.json。

`python3 work/research/interaction-depth/check_docs.py`：初检192Markdown/1320本地链接/errors空；追加日志后的最终输出另存final-doc-check.json。`git diff --check`是辅助，未跟踪文件由文档检查覆盖。

需求SHA保持e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087。无账户、设备、发送、安装或固件变更；浏览器85580正常结束exit0。

## 下轮不要重复

当前简报是docs/briefs/027-learning-states-and-evidence-reuse.md；稳定入口是design-reference-guide.md，最新规模见case-index.md。root已经复核M003/M006重要原文并看全部10页图；M006助手对rootM027文字边界做只读一致性检查，无问题。不要再以同样截图和手册段落增加重复F/E。

仍缺：讯飞同题错答/提示/重试/交卷及定时回执、断点恢复；PocketBook档案隔离和Scribble保存歧义；Supernote模板/层删除恢复、日历冲突、当前构建表现。它们只是部分产品缺口，不能缩减整体65组研究目标或宣告所有功能完成。
