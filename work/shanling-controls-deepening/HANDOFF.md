# M-063 山灵 M1 Plus 本批交接（冻结待合并）

日期2026-10-27。本批完成；不再扩展来源。不重跑本批build.py/update-case.py，更未重跑旧QUADERNO/HiBy/hifi脚本。

## 结果与合并范围

- **3新E、0新F，案例仍10F。** E-SLCD-01官网Eddict下载页；02官方Eddict手册；03来自02的静态观察，故仅2个独立新外部来源。
- **3旧F、12字段**：F-M063-01改evidence_ids/known_branches/gaps/checked_on（4）；F08加visual_status（共5）；F10改known_branches/gaps/checked_on（3）。before/after及逐字段差异在`old-coverage-updates.json`。F07格式化仍D1，未增加未证实任务。
- **3旧E additional_captures**：E-SHCD-01已有M1指南本地复读；02中国M1 V1.8页复抓；03HiFi Oasis评测正文复抓。见`old-evidence-additional-captures.json`。没有改旧记录。
- **114目录行**：M1官方指南22章节域+11部件标签+5FAQ；型号评测39项（8主屏/15播放/16系统）；共用App21章节域+12本地设置+下载页4范围项。层级、来源、设备/App作用对象分开，条目不等于完成任务。

正式变更：

1. `docs/research/interaction-library/cases/M-063-shanling.md`
2. `docs/research/interaction-library/batches/shanling-controls-deepening-evidence.jsonl`
3. `docs/research/interaction-library/batches/shanling-controls-deepening-coverage.csv`（**仅表头0数据行**，因为无新F；旧F在建议文件）
4. `docs/research/interaction-library/batches/shanling-controls-deepening-inventory.csv`
5. `docs/research/interaction-library/artifacts/shanling-controls-deepening/source-notes.md`
6. `docs/research/interaction-library/artifacts/shanling-controls-deepening/eddict-v12-p12.png`
7. `docs/research/interaction-library/artifacts/shanling-controls-deepening/eddict-v12-p15.png`

所有正式文件SHA/字节数见`formal-manifest.json`，case差异见`case-change.diff`。只改上述文件及自己的work；未改master、旧批、汇总/全局文档、其它案例。研究文件state changed=true；设备/App/账户state changed=false。没有安装、登录、购买、执行固件、发送内容或下载音视频/APK。

## 重要依据与根复核定位

1. **Eddict官方手册V1.2 p15**：播放设置只影响App本地播放，对连接的山灵设备无影响。手机定时关闭/记忆播放不能闭合M1 Plus计时/断点。
   - 官网https://shanling.com/download/76 的说明书链接指向 https://shanling.com/uploads/image/EddictPlayerAPP.pdf 。
   - 原件`work/shanling-controls-deepening/raw/EddictPlayerAPP.pdf`，16页。
   - SHA256 **a1baaac55b5b5fa1f58b0c8f2bf927e97fc4534a7dccadb886e8d7ba60cba8ff**。
   - p2说明非单型号/功能可能不同；p16是文档V1.2。正文未标发布日期或绑定App构建。中文p9列12个播放设置；英文p15明确作用对象。不能用下载页当前Android2.3.7套作PDF截图版本。
2. **同手册p6/12 SyncLink范围**：中文p6有设备控制→SyncLink、控制端搜索、连接成功提示，iOS只控制端；英文p12建议必要时Music Scan，并写双方BT/Wi-Fi开启。
   - 后一要求与旧M1型号评测的无线互斥报告存在范围冲突，未照搬为M1必做步骤。
   - 官网下载页最新版本标题与其未列M1的兼容名单并存；M1产品介绍原E-SHAN-04又明确支持Eddict/SyncLink。名单缺项不能判为不支持。
   - 同手册p5中文电脑/手机Wi-Fi传歌步骤重复；英文p12提供电脑浏览器访问App地址。没有用它补M1设备自己的接收地址/落盘/错误链。

原M1指南：`work/hifi-controls-deepening/M1_Plus-V1.0.pdf`，24页；SHA256 **9e6810edca745df4ea7e0b0dc825ddc7d796a45006f62552f9250b65b3bec9f6**。本次本地复读，不重抓旧URL；章节定位修成英文USB13–14/更新14–15（原简略页码不完整）。V1.0是文档修订号，不是固件。

当前App官网原件：`raw/eddict-download.html`及`extracted/eddict-download.txt`；中国M1页`raw/m1plus-v18.html`；商业评测`raw/review.html`。三份HTML与PDF的URL、HTTP状态、原件/提取SHA集中在`captures.json`。

## 实际查看的图/读取范围

- 本次仅实际view_image查看**Eddict PDF页12、15**，印刷页与PDF文件页一致。图像文件名v12表示本批文档V1.2，正文中已明确版本。
- p12左下SyncLink示例已连接SCD3.3，列表含EH3/H5/M9 Plus等，**不是M1 Plus**；上方为App Wi-Fi Transfer示例。局域网示例地址不作为可执行地址。
- p15可见App Timed off/Resume等设置及“不影响连接设备”的正文；下方USB权限示例是UA7，不是M1 DAC实测。
- 原M1指南5/6图是上轮观察，本批没有重复计为新观察。HiFi Oasis正文已复读，未看其图片/视频，不下载远程视频。
- M1全部24页文本与Eddict全部16页文本均已读取；M1的PDF书签只有“页1…页24”，故目录按实际章节/标签/FAQ登记，未拿24个页标签充数。
- 仍无连续观察、实机或App操作；所有hands_on=false。F08视觉状态仅静态伴随App范围说明，不表示M1界面已观察。

## 精确执行命令与状态

工作目录`/home/workspace/reader`，shell bash。

- `rg --files work docs/research/interaction-library | rg -i '(shanling|m1.?plus|hifi-controls)'` — exit0。
- `cat docs/research/interaction-library/cases/M-063-shanling.md`及Python标准库过滤master M063旧E/F、检索`E-SLCD-`与新URL占用 — exit0；前缀无占用。旧行快照为`evidence-before.json`、`coverage-before.json`、`case-before.md`。
- `python3 work/shanling-controls-deepening/fetch.py` — shell **exit0**，但逐源HTTP状态分别：Eddict下载200、中国M1下载200、HiFi Oasis200；英文`https://en.shanling.com/article/101376`、`https://en.shanling.com/article/101309`、`https://en.shanling.com/download/118`均**403并停止该路线**。403不是正文取得成功，没有换域或接口重试；本轮不为这些页新建E。
- Web搜索先前返回过上述英文页索引信息，标作发现路径，不伪造成功GET原件，不用旧机型名单补M1流程；主要事实由已取得PDF/已有M1证据支持。
- `PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY' …` 本地打开旧M1 PDF，逐页`get_text(sort=True)`保存`extracted/m1plus-p01.txt`～p24并读取 — exit0；本次未安装工具。
- 一次Python标准库`urllib.request.urlopen('https://shanling.com/uploads/image/EddictPlayerAPP.pdf',timeout=30)`正常GET — exit0/HTTP200，5733267字节；只下载说明书，不是App。
- `PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY' …` 打开EddictPDF，逐页提取`extracted/eddict-p01.txt`～p16和`eddict-pages.json` — exit0；按矩阵1.7渲染12/15页并记录`rendered.json` — exit0。
- `view_image`查看上述两个绝对PNG路径 — 两次成功，非shell退出状态；没有加载产品界面。
- `cat work/shanling-controls-deepening/extracted/eddict-download.txt`、Python列说明书链接，以及`rg -n -A100 -B8 'Memory playback|System Setting|Idle|Playlists|Playlist stores|Wireless|Network|MTouch OS' work/shanling-controls-deepening/extracted/review.txt` — exit0；商业评测版本/来源身份保留。
- `python3 work/shanling-controls-deepening/build.py` — exit0，仅生成当前批和work建议。
- `python3 work/shanling-controls-deepening/update-case.py` — exit0，仅修改M063案例。
- **合并前可重跑：`python3 work/shanling-controls-deepening/check.py` — exit0/PASS。** 检查3新E无碰撞、URL无非派生重复、原件/提取/图片/摘记SHA、引用和局部链接、3旧F12字段的before/diff、3旧E追加before、10原F完整保留、F07仍D1、coverage0行/1个LF、inventory114行/115个LF、无字面量反斜杠r反斜杠n。合并后before断言本应失配，勿将其当产品回归。

## 未解决事项

- M1实际固件、Eddict实际Android/iOS构建、地域服务权限未知；不把共用手册绑定当前M1。
- 断点持久时点/每文件还是全局、重启和耗尽恢复、计时操作重置/结束动作仍缺型号证据。没有为菜单名称新增F。
- 歌单/文件删除的对象、批量确认、撤销、失效项重联、导出文件名和重导冲突未知。评测支持导出/顺序说明，不等于可恢复备份或完整队列。
- SyncLink拒权、搜索为空、连接失败重试、退出/后台、Wi-Fi与BT冲突及网络服务离线均未闭合；App本地设置不用于填设备缺口。
- 没有推广评测性能/稳定性/无线失败为当前保证；当前高级M1型号手册仍未取得。
