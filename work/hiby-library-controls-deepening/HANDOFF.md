# M035 HiBy R3II（2025）：批30冻结交接

2026-10-27。范围固定已有R3II（2025）/HiBy OS；动态手册不标固件，1.1_202501061153只沿用前批记录，未重新核现行发布/安装版本。仅复读4个本地官方来源；无网络请求、图像观察、连续观察、账号/安装/传输/设备动作。已冻结；不要重跑写入片段或旧批脚本。

## 数量与文件

0新E；1新F（F-M035-11本地歌词显示/返回）；4旧F（03/04/09/10）共11字段；4旧E追加复读。145目录保持原ID，28行更新正文范围，其他117行标继承。没有新增目录项来增加任务数量。

正式文件仅5个，路径和SHA在formal-manifest.json：
- docs/research/interaction-library/cases/M-035-hiby.md
- docs/research/interaction-library/batches/hiby-library-controls-deepening-evidence.jsonl（空：无新来源）
- docs/research/interaction-library/batches/hiby-library-controls-deepening-coverage.csv
- docs/research/interaction-library/batches/hiby-library-controls-deepening-inventory.csv
- docs/research/interaction-library/artifacts/hiby-library-controls-deepening/source-notes.md

old-coverage-updates.json含完整before/after及changed_fields，F03=2字段、F04=2、F09=4、F10=3。旧深度不变，F04仍D1；F09新增编码建议不改变其D2仅限Qobuz时间分支的范围。

old-evidence-additional-captures.json含expected_url、before_additional_captures、append_additional_captures，分别给E-HBD-01、E-HBCD-01、E-HBD-03、E-HBCD-02。原件仍在前批work，追加采集注明本地复读、文本行号及新独立摘记SHA；E-HBY-01与E-HBD-01同URL，仅挂后者。old-evidence-field-updates.json为空。没有修改旧批/主台账/全局。baseline保存9E/10F及案例旧内容；反向案例散列依赖扫描为空。

## 关键来源与原件定位

1. [中文型号指南](https://guide.hiby.com/docs/products/audio_player/hiby_r3ii_2025/guide)，E-HBCD-01：
   - raw：work/hiby-controls-deepening/raw/manual-zh.html；SHA256 `38b541d793b2e7b343591d47ada81e5599c6a43a17e46a0131bd6fa4b654c84c`。
   - 文本：work/hiby-controls-deepening/extracted/manual-zh.txt，669–715行：上半屏切歌词、整页右划返回、LRC/同目录同文件名/编码、功能菜单仅名称。661–664行“自动滑到播放列表”没有据上下文改译成播放页。
   - 890–901行TXT菜单/百分比定位/翻页/字号背景，仅D1局部；未取得书签/退出保存。363–404/448–460行分类入口、文件导入与扫描。
2. [中文X1600 FAQ](https://guide.hiby.com/docs/question/audio_player/x1600_r)，E-HBCD-02：
   - raw：work/hiby-controls-deepening/raw/faq-zh.html；SHA256 `d132f48295345e8d33eb3a51f2c0cada130e5a2fd7ffc1b8c56f8318dbb96031`。
   - 文本：work/hiby-controls-deepening/extracted/faq-zh.txt，元数据/排序/乱码在125–231行范围，252–259行无缝限PCM“相同采样”、DSD无效；未细化相同采样的全部参数。

英文同源对照E-HBD-01/E-HBD-03，原件manual-en.html/faq-en.html，SHA依次`d631a86d1cf1cd41e685034d1c6808efbdea05effc147aaf77452b2d4be27ede`、`10cc0e02d3059703e4189b2d9c7e7e4428e350a40358525685ff1b9c5be1555d`。这些不是独立交叉验证，FAQ为点名本机的平台共用资料。read-ranges.json给本次选读范围；raw-manifest.json列4HTML＋4既有提取文本共8项。没有PDF、PDF页码或新增图；只检索HTML的img标签，发现logo与前批产品图，不把标签检查当看图。

## 命令、检查与退出

工作目录/home/workspace/reader。没有运行旧build/check。

- `cat AGENTS.md`、`sed -n '1,45p' README.md`、`sed -n '1,60p' docs/requirements.md`；case、旧HANDOFF/source-notes及145目录读取：exit0。
- `rg --files work | rg '(hiby|Hiby|HiBy|R3II|r3ii|r3-II|X1600|x1600)'`及ideas相关检索成功；同一组末尾旧前缀查询无命中返回1，是搜索结果非访问失败。后续Python全库核E-HBLD无碰撞，本批未使用新E。
- `sed -n '190,730p' work/hiby-controls-deepening/extracted/manual-en.txt`、`sed -n '827,860p' .../manual-en.txt`、`sed -n '83,245p' .../faq-en.txt`及中文对应段：exit0。完整确切路径/行段在read-ranges.json。
- `nl -ba work/hiby-controls-deepening/extracted/manual-zh.txt | sed -n '640,716p'`、同文件876–904/1100–1160；`nl -ba work/hiby-controls-deepening/extracted/faq-zh.txt | sed -n '235,285p'`：exit0。
- 一次性Python写入本批文档、旧字段建议和manifest：exit0；没有可重跑的构建器。
- `python3 work/hiby-library-controls-deepening/precheck.py`：一次PASS，exit0。只读核旧before、4旧F/11字段、4capture及raw/摘记hash、0E/1F、145目录、LF物理换行/引用、5正式与8原件散列。提供root合并前使用；合并后不可再运行依赖before的检查。
- `git diff --check -- docs/research/interaction-library/cases/M-035-hiby.md docs/research/interaction-library/batches/hiby-library-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/hiby-library-controls-deepening-coverage.csv docs/research/interaction-library/batches/hiby-library-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/hiby-library-controls-deepening/source-notes.md`：exit0；未跟踪文件内容另由precheck核定。

前批store/apps/help-center的403已读失败记录，本轮无访问/重试。原件NUL保留；正文有过终端截断，后续按范围读取，未宣称整份所有功能已分析。没有外部状态变化。

## 冻结SHA与未知

- formal-manifest.json：`83a8cd24306fb767390e683a6c1e3f1b1d6961da700f24f7ce9603bd340b8c20`
- raw-manifest.json：`44d77130487c8cd3317ff845db97a10fb10c5f099b4889e6bcfe809b351975a2`
- case：`65514e618c2944b513e8f48eba3bfc791ddd553042bf2700cab674ec4877fda8`
- source-notes：`1c626780fcb24bbfe3392b03b2a3fce77b168844fbd8ed947fdc6757790dc671`

保留未知：扫描结束/去重/坏标签/移卡；删除对象与确认/撤销；歌词反向切换、逐句定位、返回后播放状态；TXT书签/退出保存；编码改写后结果；无缝与跨组组合；实际2025硬件装配和固件。排序规则不是用户重排，条件建议不是恢复成功。计时/蓝牙焦点没有重做；没有把未说明写成不支持。

state_changed=true，仅本案例、新正式资料及专属work；主表、旧批、全局文档、设备/账号、整体goal状态未改。冻结后停止编辑/检查。
