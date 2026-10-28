# M039 Audible US 交接（2026-10-27）

23新E（E-AUCD-01…23）、3新F（12…14）、11旧F补证，案例共14F。所有流程D2、hands_on=false、visual_observed=false；没有看图、连续操作或实测。53行目录不是全App功能数：首屏35个不同文章、9补充帮助、9设置域，Ways to listen仍有未展开Load more。完整构建号未知，US/Android/iOS与Cloud、Kindle配对和历史Car Mode分开。

## 正式修改文件

- `docs/research/interaction-library/cases/M-039-audible.md`
- `docs/research/interaction-library/batches/audible-controls-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/audible-controls-deepening-coverage.csv`，仅F12–14
- `docs/research/interaction-library/batches/audible-controls-deepening-inventory.csv`，53行
- `docs/research/interaction-library/artifacts/audible-controls-deepening/source-notes.md`

旧覆盖更新：`work/audible-controls-deepening/old-coverage-updates.json`，逐字段before/after及replacement_row；原11行快照`old-coverage-before.json`。所有11行只改evidence_ids、known_branches、gaps、checked_on；F编号、task、depth、visual_status、hands_on、case_path不变。首轮/末轮检查11行均与before相符；本代理不写旧批/master/汇总。新F12退还交易、F13沉浸文字读听、F14纸书扫描，均与旧清下载/进度同步目标区别开。

## 优先复核的两份官方正文

1. https://help.audible.com/s/article/learn-about-immersion-reading
   E-AUCD-10，文章日期2026-10-12。原文工具摘取见`work/audible-controls-deepening/extracted/10.txt`，定位What do I need / start / customization。要求对应有声书+Kindle电子书；Audible文字从网络加载，仅部分页缓存；专业旁白，不是TTS。音频已下载不能外推全书离线读听。
2. https://help.audible.com/s/article/use-page-sync
   E-AUCD-11，2026-09-03。工具摘取`extracted/11.txt`：More > Page Sync；相机权限、活动网络；纸书→音频单向，不提供反查页码；部分英语书，排除播客、无纸书对应Originals、Kids Profile、儿童绘本。未实测匹配率、错误回退或上传保存。

两者工具返回原快照均为`work/audible-controls-deepening/web-state-snapshot.json`，SHA256 `a02f03791f0fbc9fd43bcc668dbc528ae01453c8eca51e6674c18371506a9e77`。

其它关键原文定位：

- `extracted/06.txt`，https://help.audible.com/s/article/title-is-missing?language=en_US ：Check your archive当前写Library > Lists > Hidden > Remove from list。加入Hidden入口没取得，不拿普通Add to list补写；归档、移库、删除文件和退款不合并。
- `extracted/08.txt`，https://help.audible.com/s/article/return-a-title ：US网页退还资格/步骤、信用点返还；App不能直接办理。仅公开文档，不是可退资格保证，未执行。
- `extracted/18.txt`，https://help.audible.com/s/article/app-keeps-crashing?language=en_US ：Re-install note说明下载及Finished标记需要重下/重标；不把普通重下载保进度外推成清数据也保留所有笔记。
- `extracted/21.txt`，https://help.audible.com/s/article/learn-about-standard-plan?language=en_US ：月选可下载但会员终止后的资格不同于已购；锁图标/不可播放提示与恢复条件；不能退换月选。
- `extracted/23.txt`，https://help.audible.com/s/article/manage-collections?language=en_US ：直接打开仅CSS Error；后续官方搜索正文提供普通Lists/Collections管理，证据access_mode单独标official_search_index_body；删除清单不可恢复不等于删除音频。

逐来源SHA在evidence的extracted_sha256；全部35工作资料SHA在`source-manifest.json`。23个提取TXT是从8个JSON工具快照截取；**这些不是官方原始HTML**。普通HTTP的4份HTML均HTTP200、127181bytes、同SHA `ba58cc455533a178ceb1ff07e07b376a6e59c8bf720ddb30efd2b675f1e55242`，不含文章标题/操作正文，明确是前端壳，不用于证明正文归档。`raw_work_path`本批指原始工具返回快照，`capture_state=web_tool_text_snapshot_not_raw_html`。正式archived_artifact均null；正式source-notes只存简短研究记录。

## 准确命令与退出状态

- `cat AGENTS.md README.md docs/requirements.md`：exit0；读取项目阶段/范围。
- `rg --files docs/research/interaction-library work | rg '(audible|M-039|audio.*evidence|audio.*coverage|template.md)'`：exit0；定位case及audio旧批，无既有Audible原件。
- `cat docs/research/interaction-library/cases/M-039-audible.md && rg -n 'E-AUCD' docs/research/interaction-library`：cat成功、rg无匹配，组合exit1；表示新前缀无碰撞，不是读取失败。
- `python3 work/audible-controls-deepening/fetch.py 'player-index.html=https://help.audible.com/s/player?language=en_US' 'listening-index.html=https://help.audible.com/s/listening?language=en_US' 'listen.html=https://help.audible.com/s/article/listen-in-the-app' 'library.html=https://help.audible.com/s/article/manage-your-library'`：exit0，四份壳HTTP200。未继续批量抓壳，未借缓存URL绕过访问。
- 正文通过`tools.web__run`公开open/click/search，结果逐字保存8个`web-*-snapshot.json`。快照包含确切来源URL、open/click定位/行号及文章内容；无shell退出码。正文/索引能取到不代表页面实际UI可操作。搜索曾返回其它地区结果，只将本批所列US官方URL用于事实。
- `python3 work/audible-controls-deepening/build_batch.py`：exit0；生成23E、3新F、11旧建议、53目录、35原件/派生件manifest。**不要在根审校/合并后重跑覆盖。**
- `python3 work/audible-controls-deepening/write_case.py`：exit0；写授权case和正式短摘记。
- `python3 work/audible-controls-deepening/check_batch.py`：exit0。核前缀碰撞、14F全集、34新旧E引用、11旧行before、US主机、35工作资料哈希、相对链接、D2/nohands/noimage、CSV物理换行。
- `git diff --check -- docs/research/interaction-library/cases/M-039-audible.md docs/research/interaction-library/batches/audible-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/audible-controls-deepening-coverage.csv docs/research/interaction-library/batches/audible-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/audible-controls-deepening`：exit0。目标未跟踪，不能代替内容检查。
- `git status --short -- docs/research/interaction-library/cases/M-039-audible.md docs/research/interaction-library/batches/audible-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/audible-controls-deepening-coverage.csv docs/research/interaction-library/batches/audible-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/audible-controls-deepening`：exit0，以上目标??。

CSV生成显式`lineterminator='\n'`。终检coverage 4个物理LF/3数据行、inventory54个LF/53数据行；CRLF与字面量`\\r\\n`均0。详`check-results.json`。

获取失败/边界：最初猜测Listening topic路径`https://help.audible.com/s/topic/0TO4T000000lh1uWAA/listening`工具Internal Error，随后使用搜索得到的公开/s/listening正式入口；不是对拒绝页面重试。Manage Lists直接打开CSS Error但同URL搜索可得官方正文；View player settings链接Internal Error，停止，不填完整定制手势。未见401/403/429、无安装、无helper、无音频/APK/视频下载。

## 不确定性与状态改动

保留构建/手势细节、下载取消/续传、离线授权检查周期、来电/耳机打断后续播、离线并发同步胜出、片段与书签联动删除、隐藏加入入口、笔记/片段重装保留、沉浸文字缓存范围、Page Sync误匹配/图像处理等缺口。不因文档存在而填D3/D4。

仅本地研究文件状态改变。用户账号、书库、交易、设备和服务状态均未改变；不登录、不买、不退、不提交、不播放。旧批/master由根代理负责。

根预审后定点收紧F13：E-AUCD-07的L81–83明确写两模式之间切换，E-AUCD-10仅列Listen→Read & Listen启动。案例现区分切换能力和反向后状态保持，后者未知。未改新F编号或coverage字段；原件及哈希不变。再次独立运行check_batch.py与git diff --check均exit0。
