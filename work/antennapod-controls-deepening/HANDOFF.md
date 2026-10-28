# M040 AntennaPod 交接（2026-10-27）

已完成有限批次：18个新E（E-APCD-01～18），0个新F，原12F全部保留；9行旧coverage建议。没有修改主表、旧batch或汇总；没有产品操作、代码构建/安装、账号/数据提交、APK/视频下载，也没有产品截图观察。状态仅为本案文档、正式分析附件和work原件变化。后续不要重跑生成器覆盖根代理合并后的修订。

## 正式变更

- `docs/research/interaction-library/cases/M-040-antennapod.md`
- `docs/research/interaction-library/batches/antennapod-controls-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/antennapod-controls-deepening-coverage.csv`（仅header，避免重复旧F）
- `docs/research/interaction-library/artifacts/antennapod-controls-deepening/source-notes.md`
- `docs/research/interaction-library/artifacts/antennapod-controls-deepening/functional-inventory.csv`（47官方帮助索引＋84带key的全局设置节点）

`old-coverage-updates.json`逐字段before/after及完整replacement_row：F-M040-01、02、04、05、07、08、09、11、12。涉及task/evidence_ids/known_branches/gaps/checked_on；depth始终D2，visual_status不变not_observed，hands_on不变false。未改F03、06、10旧行。建议来自旧audio-coverage.csv原值，根合并时如master已有修改须对照而非盲覆盖。

## 核心复核与原件

固定正式release **3.12.3**（2026-10-13，prerelease=false），tag commit **175e5ef8fe3e125a5014d70ef0c57cf85661c71b**，app/build.gradle声明versionCode3120395。不是安装包实测。

1. [正式服务配置](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/common.gradle#L15)：common.gradle15–16开启Media3/禁旧服务；配合playback/service/src/main/AndroidManifest.xml20–79（包括旧QuickSettingsTileService disabled）与PlaybackServiceStarter44–60。原件 `work/antennapod-controls-deepening/src/common.gradle`，SHA256 `681cda45eb6bc6ab8d34a46f6c20552809c242799cdc7d77287a1b9f89c1ba0c`。
2. [计时扣减](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/playback/service/src/main/java/de/danoeh/antennapod/playback/service/internal/ClockSleepTimer.java#L34)：34–54由PlaybackPositionEvent触发，扣System.currentTimeMillis两次事件到达差，**不是event.position差**；>10秒间隔跳过。原件同层 `src/playback/service/src/main/java/de/danoeh/antennapod/playback/service/internal/ClockSleepTimer.java`，SHA256 `6f3380a396662acb2b745f2fbc20a735c5ed00c79ba9c5e21d9359711d1a2421`。调用方Media3PlaybackService390–428、504–535按状态开停观察、每秒duration>0才发事件；884–937到期pause/disable/extend。其SHA `54dfb5aed8744d3d627f9a53b0871e70cc935473038e31947ee717f330c36f8a`。

另两项重要范围：DatabaseExporter45–105只导出SQLite，UserPreferences148–152另为SharedPreferences；importDatabase警告/确认/错误见ImportExportPreferencesFragment189–278。官网all data不能推成全部app设置。取消订阅/归档/恢复见RemoveFeedDialog59–84、119–178，DBWriter118–249/862–884；本地文件夹退订跳过底层原文件删除，单独删本地媒体则会调用DocumentFile.delete。

全部50个成功原件逐一URL、raw_work_path、SHA256在 `source-manifest.json`。release.json SHA `ed458f804d5cc2dd21bd4c046dc064df28214c7e0820ebd037c7204604a029fc`；tag-ref.json SHA `4ba68ad3cd04ead52f0d52c92056453a6f08f0e0d65e9847eb9e1097d814a948`。正式evidence的archived_artifact均为library内长期附件，不填work；原件另设raw_work_path/raw_sha256。

## 命令、返回值与访问记录

工作目录均 `/home/workspace/reader`。读了README.md、docs/requirements.md、AGENTS.md、template.md、当前case、旧audio-evidence/coverage，以及docs/ideas.md有关后台/播客段落。关键实际命令如下；所有网络目标和结果逐条在fetch-log.jsonl（首个release.json直接urllib获取补在source-manifest）。

```sh
rg --files docs/research/interaction-library/cases work | rg -i 'antennapod|antenna|M-040'
rg -n 'E-APCD' docs/research/interaction-library
python3 work/antennapod-controls-deepening/fetch.py 'tag-ref.json=https://api.github.com/repos/AntennaPod/AntennaPod/git/ref/tags/3.12.3' 'documentation-index.json=https://antennapod.org/assets/datasets/documentation_index.json'
python3 work/antennapod-controls-deepening/fetch.py 'tree.json=https://api.github.com/repos/AntennaPod/AntennaPod/git/trees/175e5ef8fe3e125a5014d70ef0c57cf85661c71b?recursive=1' 'release-3.12.0.json=https://api.github.com/repos/AntennaPod/AntennaPod/releases/tags/3.12.0'
python3 work/antennapod-controls-deepening/fetch-targets.py
python3 work/antennapod-controls-deepening/read_ranges.py ExoPlayerUtils.java:47-100 ClockSleepTimer.java:26-155
python3 work/antennapod-controls-deepening/read_ranges.py DatabaseExporter.java:25-125 DBWriter.java:118-199,212-250,862-887 FeedMenuHandler.java:35-100
python3 work/antennapod-controls-deepening/build_records.py
python3 work/antennapod-controls-deepening/update_case.py
python3 work/antennapod-controls-deepening/check_records.py
git diff --check -- docs/research/interaction-library/cases/M-040-antennapod.md docs/research/interaction-library/batches/antennapod-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/antennapod-controls-deepening-coverage.csv docs/research/interaction-library/artifacts/antennapod-controls-deepening
git status --short -- docs/research/interaction-library/cases/M-040-antennapod.md docs/research/interaction-library/batches/antennapod-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/antennapod-controls-deepening-coverage.csv docs/research/interaction-library/artifacts/antennapod-controls-deepening
```

上述除查重rg（exit1，表示当时未用前缀）均exit0。后续定点获取也通过fetch.py；首次猜测的AudioPlayerDetailsFragment.java、ui/preferences/.../PlaybackPreferencesFragment.java返回404，使两个inline subprocess批次exit1，未重试这些错误地址；依据已取得tree改读CoverFragment.java和app/.../PlaybackPreferencesFragment.java后HTTP200。没有401/403/429或规避拒绝。fetch-log共49个HTTP200、2个404；另首个release APIHTTP200，因此成功原件50个。web工具打开release API返回Internal Error，而普通公开HTTP API一次成功；官网帮助、GitHub release页面已由web打开读取。

校验脚本exit0：18E编号唯一、12F保留、9旧行before匹配、E引用可解析、正式附件与50原件散列相符、局部链接存在、0hands-on/0visual。检查结果在check-results.json。git diff --check exit0但这些docs当前为untracked，不能把该命令当内容审校；实质校验由自检脚本完成。

## 尚未解决

- 官方当前帮助不绑定此tag；自动下载默认网页50/XML25/Java缺值回退20，未判定真实初始化值。
- 只核定47条帮助索引及11个全局设置XML，未完成每播客动态设置和全部播放器、转录、统计、Cast/车载功能目录。
- Media3库/Android焦点具体策略、电话/导航/耳机重连、系统通知布局和后台生存未实测。
- 计时短暂停/缓冲/系统时间跳变/未知duration的结果、传感器异常未闭合；当前计时持久恢复不作保证。
- 删除文件失败可只日志；表单错误也dismiss；DB替换失败回滚、自动备份实际周期及多端冲突未证明。
