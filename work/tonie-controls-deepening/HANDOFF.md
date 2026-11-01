# M-038 Toniebox 1/2 有限批次交接

日期：2026-10-27。仅文档研究；不完成总体goal。

## 范围与变更

- 24个E-TNCD证据记录（目录/正文/官方索引/产品介绍分开），7个新任务F-M038-12至18；现case18F。
- 5个旧F：05、06、08、10、11。逐字段before/after与完整替换行在 `work/tonie-controls-deepening/old-coverage-updates.json`，仅改变 evidence_ids、known_branches、gaps、checked_on。旧编号、任务标题、D2、not_observed、hands_on=false保持。
- 四目录150条：1代63、2代65、Tonieplay14、US家庭8。目录快照不等于已查150个流程或全站完整目录；未读条目显式保留。
- 正式只修改cases/M-038-tonie.md，新增同名3批次及artifacts短摘记；未改master、旧batch、summary或产品代码。设备/账户/服务无状态改变。

## 正式文件（相对仓库根）

- `docs/research/interaction-library/cases/M-038-tonie.md`
- `docs/research/interaction-library/batches/tonie-controls-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/tonie-controls-deepening-coverage.csv`
- `docs/research/interaction-library/batches/tonie-controls-deepening-inventory.csv`
- `docs/research/interaction-library/artifacts/tonie-controls-deepening/source-notes.md`

## 关键事实和复核入口

1. 游戏中断恢复：`extracted/09.txt`正文L19–20（US）与`10.txt`L22–28（EU）。quiz不续未完轮、互动故事最后位置、Discover新轮；重启页另写起点/最近checkpoint与未通关不能全重置。这里5题是举例，不是固定倒退5题。未以统一“pause”推导秒点持久续播。
   - https://support.tonies.com/hc/en-us/articles/29036389045778-How-long-does-the-Toniebox-2-remember-your-auto-saved-status-when-you-remove-the-disc-take-a-break-or-play-something-else
   - https://support.tonies.com/hc/en-150/articles/29036401221522-Can-I-restart-a-Tonieplay-game
2. 日出闹钟：`extracted/13.txt`英国官方索引正文，How do I turn on the Sunrise Alarm及页首：3+和充电器条件；若Tonie正在播则不响，另提醒电量足够。两种供电表述并存；不推导拔电或离线也可靠响铃。
   - https://support.tonies.com/hc/en-gb/articles/29255589102866-Sleep-Light-with-Timer-Sunrise-Alarm-guide
3. 家庭与Creative：`14.txt`角色与逐公仔授权，`16.txt`异家庭内容绑定，`18.txt`UK Live关闭后手动排序；`19.txt`移交后旧Owner退出，`20.txt`删家庭永久删录音。所有细粒度权限/缓存传播未知仍列case。

## 原件与哈希

8个web工具返回JSON字符串快照 +24个逐源TXT，共32个来源文件。不是原HTML。`source-manifest.json`列全部路径/字节/SHA-256；每个正式E也有raw_work_path/raw_sha256及extracted_work_path/extracted_sha256。正式archived_artifact均null；没有图像/视频观察。关键TXT：

- `work/tonie-controls-deepening/extracted/09.txt` — `bd6df6bc49662fa73267fc75b2e1d15ba9454d7e80d2c9cefd53964d3871ec61`
- `work/tonie-controls-deepening/extracted/10.txt` — `3cb79a1067f4045d7e6906cf07d80750dbe917ee45bbef293308bd8fc809ec19`
- `work/tonie-controls-deepening/extracted/13.txt` — `54c4442b14bfa50d01814e172da301d9561b502c87668fd84f6f5ebc6e83abfa`
- `work/tonie-controls-deepening/extracted/14.txt` — `17e416c3a83ff398e83057cbe7ad66c86e1f609944a67d84cf42cb7a81610efc`
- `work/tonie-controls-deepening/extracted/16.txt` — `69a0e9f1ff80b5d14af25ad3768942bdad4cde09c042c5ddeaefcf2ad28aad7a`
- `work/tonie-controls-deepening/extracted/18.txt` — `3c601f17cabce9d62f588ff7bdfb4cfdc8be3e54dea0a0824af75d9352520551`

## 执行与退出状态

以下命令均在 `/home/workspace/reader` 执行。所调用的shell组合命令、写入/构建/检查命令均exit 0；最初前缀rg自身无匹配，随后ideas检索成功，所以该组合命令exit 0。无匹配不表示工具失败，也不表示发现已用ID。完整web返回快照为web-discovery.json与tn-pages1至7.json，工具查询而非shell退出码。

```bash
cat AGENTS.md README.md docs/requirements.md
rg --files docs/research/interaction-library work | rg 'tonie|M-038|template|interaction-library/README'
rg -n 'E-TNCD|E-TOCD' docs/research/interaction-library; rg -n 'Tonie|互动|权限|听书' docs/ideas.md | head -n 25
cat docs/research/interaction-library/cases/M-038-tonie.md docs/research/interaction-library/template.md
cat docs/research/interaction-library/README.md
cat docs/research/interaction-library/batches/README.md
python3 work/tonie-controls-deepening/build_batch.py
python3 work/tonie-controls-deepening/check_batch.py
```

`build_batch.py`是一次性初稿生成器，之后已修补新旧表连续性、F12首章/全游戏引用和F10结束状态；不要复跑覆盖当前结果。精确修改见case-change.diff。

网络例外：一次普通`urllib.request.urlopen`访问原EU睡眠URL返回HTTP403；包装脚本捕获异常后exit 0，不能当请求成功。没有得到HTML文件，停止直接HTTP路线。没有改UA、代理、认证或其他规避方式。后续官方搜索返回了不同地区UK同文索引，按索引取证登记；未声称成功取得被拒页。权限及US家庭移交web工具出现400/timeout，也保留失败，未反复访问。

最后结构检查：check-results.json，24E引用与32个来源散列均通过；新coverage7行、8个物理LF，inventory150行、151个物理LF；CR与字面量反斜杠r/n为0。5个旧F在自检时仍等于before。case18个F无重复，现有相对链接均存在。全部5个正式文件UTF-8/末尾换行/无行尾空白通过。git scoped diff为空是这些文件尚未追踪，不能据此称无修改；已另存与本批case-before的真实文本diff。

## 未决与限制

具体App构建/固件未知；官方帮助跨US/UK/Europe/Canada逐条保留。未取游戏随盒说明书和完整逐题规则/判分/奖励；控制器转/按只取得D1介绍；共同游玩不等于已证多账号或竞争判分。未播放内嵌iframe，也未看静态图，hands_on/visual始终false、最高D2。闹钟停止/重复/时区、离线睡眠、断电/跨盒游戏存档、权限撤销后缓存及重置对云数据范围仍未知。
