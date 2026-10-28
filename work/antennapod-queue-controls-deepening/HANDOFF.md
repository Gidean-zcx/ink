# M040 AntennaPod 队列深化：冻结交接

2026-10-27。只研究Android AntennaPod 3.12.3源码，固定提交 `175e5ef8fe3e125a5014d70ef0c57cf85661c71b`；3篇帮助未声明构建。不安装/运行App，无账号、设备、媒体下载或剪贴板动作。`hands_on=false`、视觉观察0。已冻结，不再运行生成器或自行重跑before检查。

## 交付和数量

- 仅改正式案例 `docs/research/interaction-library/cases/M-040-antennapod.md`。
- 新建 `docs/research/interaction-library/batches/antennapod-queue-controls-deepening-{evidence.jsonl,coverage.csv,inventory.csv}`。
- 新建独立摘记 `docs/research/interaction-library/artifacts/antennapod-queue-controls-deepening/source-notes.md`。
- 9新E：E-APQD-01～09；1新F：F-M040-13，已有标签浏览与选择恢复。其余补原F02/03/04/05/10，共18字段；D2不升级实操。
- 目录158行＝继承47帮助索引＋84设置节点＋27定点源码分支。继承项只更新H04/H44/H47三篇正文状态；没有把全部目录算作读完，也没有按分支增加F。
- 10来源＝3帮助＋7源码文件；6源码新GET，SubscriptionFragment本地复读。9次普通匿名GET全部HTTP200；无本批访问失败；不重试历史失败。

## 合并建议格式

`old-coverage-updates.json`：5项，每项含flow_id、完整before/after及changed_fields的逐字段before/after；字段数分别3/4/4/3/4。F04任务名改为“准备离线节目：自动资格与手动批量”。

`old-evidence-additional-captures.json`：只对E-APCD-13，含expected_url、before_additional_captures、append_additional_captures；原E已有SubscriptionFragment关联，追加该原件本地复读，不另立重复E。`old-evidence-field-updates.json`为空。所有旧F/E before均已与主表断言一致；本助手未改主表/旧批/全局。

`baseline.json` / `case-before.md`保存案例旧SHA和36E/12F基线；未找到以本案例作为原件的反向散列依赖（reverse_refs为空）。

## root重点复核定位

1. [DownloadLogAdapter.java](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/download/DownloadLogAdapter.java)：本地`raw/DownloadLogAdapter.java` **98–119行**先INVISIBLE再查源/媒体；缺对象只Log并return；106/116行为请求入口。125–132行检查此前列表索引的同类型/ID成功记录；未读列表排序，不能把函数名推成严格时间顺序。SHA256 `3b2ee8ab167a695edfe230246ac0d11b9825b72ad006f0aa069e0668beadb339`。
2. [EpisodeMultiSelectActionHandler.java](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/episodeslist/EpisodeMultiSelectActionHandler.java)：本地同名raw文件 **140–149行**下载资格/接口请求/计数，**152–166行**删除或取消请求/计数，**233–242行**数量提示；数值不是完成回执。SHA256 `5191c537e483a5bbfbc5925d8c5bd1bf6fec11a6b4e79bb8bcc1c6f2b2b48996`。

附：ItemFilterDialog 45–68行选择即callback、Confirm仅dismiss、Reset空集；未读调用方，不构成完整过滤任务。SubscriptionFragment 219–229/344–349/368–404行支持F13，root仍是过滤集合。该原件位于`work/antennapod-controls-deepening/src/app/src/main/java/de/danoeh/antennapod/ui/screen/subscriptions/SubscriptionFragment.java`，SHA256 `0d35a96da963bd24f39653fe879527290c302e5cc05638145407a2cc7cac643f`。7源码均与既存官方tree的Git blob核对；tree未重新获取，正式发行版本元数据沿前批。

3篇新帮助原址：
- https://antennapod.org/documentation/queue/multiple-queues
- https://antennapod.org/documentation/subscriptions/subscription-groups
- https://antennapod.org/documentation/automation/refreshing-podcasts

前两者明确当前多队列不支持/标签不创建独立队列；刷新帮助的12小时默认仅属未标构建帮助，不覆盖既有50/25/20自动下载数量差异。URL、实际获取方式、原件SHA见fetch-log.json；完整读范围见read-ranges.json。

## 命令与结果

以下均在`/home/workspace/reader`执行，退出码0；正文读取没有运行产品源码。

- `cat AGENTS.md`；`sed -n '1,30p' README.md`；`sed -n '1,32p' docs/requirements.md`；`cat docs/research/interaction-library/cases/M-040-antennapod.md`；`cat work/antennapod-controls-deepening/HANDOFF.md`。
- `python3 work/antennapod-queue-controls-deepening/fetch_selected.py`：仅一次，9 GET/200，源码blob核对。
- `python3 work/antennapod-queue-controls-deepening/build_batch.py`：仅一次，后续定点修正文档/定位，禁止重跑。
- `nl -ba work/antennapod-queue-controls-deepening/raw/DownloadLogAdapter.java | sed -n '55,155p'`。
- `nl -ba work/antennapod-queue-controls-deepening/raw/EpisodeMultiSelectActionHandler.java | sed -n '140,252p'`。
- `nl -ba work/antennapod-queue-controls-deepening/raw/ItemFilterDialog.java | sed -n '45,122p'`。
- `nl -ba work/antennapod-queue-controls-deepening/raw/DownloadLogDetailsDialog.java | sed -n '35,170p'`。
- `nl -ba work/antennapod-controls-deepening/src/app/src/main/java/de/danoeh/antennapod/ui/screen/subscriptions/SubscriptionFragment.java | sed -n '335,434p'`，另读180–299行。
- `python3 work/antennapod-queue-controls-deepening/precheck.py`：一次PASS，9E/1F、5旧F/18字段、1旧capture、158目录、10来源/7blob、LF物理换行/散列/引用/before一致。此脚本只读，提供root合并前复核；合并后不得使用。
- `git diff --check -- docs/research/interaction-library/cases/M-040-antennapod.md docs/research/interaction-library/batches/antennapod-queue-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/antennapod-queue-controls-deepening-coverage.csv docs/research/interaction-library/batches/antennapod-queue-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/antennapod-queue-controls-deepening/source-notes.md`：0。注意这些路径在当前git仍为untracked，实际内容/结构由上述precheck核定。

原件/行号输出有过终端截断（整tree为单行），未据截断输出宣称读完；tree通过JSON定点取7个path/hash，不泛读源码。未发现需重试的网络失败。

## 冻结散列和限制

`formal-manifest.json`列5正式文件；`raw-manifest.json`列14原件/派生文本（raw12＋复用SubscriptionFragment＋tree）。

- formal manifest SHA256 `6fdbfae8fd0116d122397ef9366a22be11fae7dad70cf6a0dabab43be1775930`
- raw manifest SHA256 `ef8f4bfff2b8daf62f50471672555f6380dec46ca9fe825ca132635121061058`
- case SHA256 `cb8d9fad2248a19a55367c5a61d2094aabc263933ed4f6c5f36cf49df17c9542`
- source-notes SHA256 `76023a8b5ac6214a62711bd203fd82e176fd5e2bf5a477488dd63445de13cd11`

仍未知：多选启动手势；当前播放项排序位置；排序/清空/拖动取消及DB失败回滚；下载服务内部许可/自动退避/半文件清理/实际完成次序；过滤调用方持久化；标签创建保存/删除/同步完整链；异常终止能否保存last_tag。旧帮助URL长按与固定源码点击复制保持分列。不把按钮消失、表单关闭、批量提示或SQL下载字段当设备状态确认。

状态改变仅限本案例、上述新正式文件和专属work。没有修改总目标状态、主台账、旧批、全局需求或外部账户/设备。root已告知复核核心源码条件；本助手交接后停止编辑和检查。
