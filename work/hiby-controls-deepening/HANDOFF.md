# HiBy R3II2025 本批交接（2026-10-27）

已收束。4新E（E-HBCD-01–04），1新F（F-M035-10），现10F；5旧F共20字段、4旧E additional_captures建议。145目录=35手册层级标题+71一级菜单（快捷11/播放24/蓝牙6/WiFi7/系统23）+6播放功能菜单名+33平台FAQ层级标题。标题与子设置重复不算任务数量。

正式文件仅：
- docs/research/interaction-library/cases/M-035-hiby.md
- docs/research/interaction-library/batches/hiby-controls-deepening-evidence.jsonl
- docs/research/interaction-library/batches/hiby-controls-deepening-coverage.csv
- docs/research/interaction-library/batches/hiby-controls-deepening-inventory.csv
- docs/research/interaction-library/artifacts/hiby-controls-deepening/source-notes.md
- docs/research/interaction-library/artifacts/hiby-controls-deepening/official-product-1.png
- docs/research/interaction-library/artifacts/hiby-controls-deepening/official-product-2.png

合并建议：
- work/hiby-controls-deepening/old-coverage-updates.json：F02/03/04/05/06均完整before/after/changed_fields，各改evidence_ids、known_branches、gaps、checked_on。04/05仍D1。
- work/hiby-controls-deepening/old-evidence-additional-captures.json：E-HBD-01/02/03/04完整before/after，只追加additional_captures。E-HBY-01与E-HBD-01本来同URL，本轮不重复挂采集。
- work/hiby-controls-deepening/case-change.diff：相对本批开始前案例的差异。

## 根优先复核

1. https://guide.hiby.com/docs/question/audio_player/x1600_r
   原件raw/faq-zh.html；提取extracted/faq-zh.txt第53行扫描/格式化重计，第59行任何条件不重计，第63行电脑USB/充电大图标不响应睡眠，第70行省电重启保留开关，第76行其USB例外。英文原件也保留，不能以中文明晰了USB句子便消除其他冲突。型号手册extracted/manual-zh.txt第1071–1083行是省电前提/睡眠一次性及插USB进入充电界面，范围与平台FAQ并列。

2. https://guide.hiby.com/docs/products/audio_player/hiby_r3ii_2025/guide
   中文提取第20行ES9219C、第32行蓝牙5.1；英文manual-en第20行CS43198、第43/54/78行音量加减键；spec-zh第23行旋钮、第24行ES9219C、第49行音量按键。两张真实查看的型号页插图也画旋钮，但没有2025装配可识别标记。不要选择性拼成确证配置。

3. 未闭合重点：manual-zh第714–715行仅菜单名，874–876行传歌仅入口/按提示/退出。没有当前队列编辑/删除对象/确认、传歌成功失败/落盘/恢复全链，不升级D2。新F10只为列表模式/跨目录/专辑播放范围。

同源中文指南、中文FAQ是不同URL的版本补证，不当独立交叉验证。四个新URL已全库规范化查重。English/Chinese指南共用两幅产品图，只算1个静态来源、2图，无连续观察、无实机。

## 取证和检查命令/退出

- `rg --files docs/research/interaction-library work` 配合型号/品牌筛选、cat现case/旧工作稿、Python读取主E/F：exit0；未找到旧HTML/PDF原件，只有旧case工作稿，故新建采集。
- `python3 work/hiby-controls-deepening/acquire.py work/hiby-controls-deepening/tasks1.json`：exit0，4英文HTML HTTP200。
- 同命令tasks2.json：exit0，中文型号/FAQ HTTP200；语言路径来自已取得网页的语言链接。
- 同命令tasks3.json：exit0，中文规格HTTP200；官方store/apps/help-center HTTP403，停止该路径、不下载或绕过。
- 两张型号页图片inline urllib GET：exit0、HTTP200，随后view_image两次成功；没有图像编辑。
- `python3 work/hiby-controls-deepening/parse_inventory.py`：exit0；从原HTML层级取一级列表，修正快捷菜单选择后数量11/24/6/7/23。原HTML/提取存在NUL，保留原件，文本查阅用rg -a；早期普通rg无匹配exit1后改本地文本读取，没有网络重试。
- `python3 work/hiby-controls-deepening/build.py`：exit0，仅授权文件与work。
- `python3 work/hiby-controls-deepening/check.py`：exit0 PASS；4E/1F/10caseF，5旧F及4旧E before一致，所有原件/正式档案哈希一致，零新URL/ID冲突，CSV真实LF为2/146。
- `git diff --check -- docs/research/interaction-library/cases/M-035-hiby.md docs/research/interaction-library/batches/hiby-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/hiby-controls-deepening-coverage.csv docs/research/interaction-library/batches/hiby-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/hiby-controls-deepening`：exit0。未跟踪文件主要由check.py及case-change.diff检查。
- 最后只读Python按关键词列原文行号：exit0；本交接写入exit0。

状态：只改文档/work，未改主表、旧batch或全局文档。未登录/购买/提交/传歌/删文件/升级，不执行设备状态改变。hands_on均false。没有完成总体goal的动作。build.py冻结后不要重跑覆盖根审校；check.py会写check-results.json，不用于只读请求。

## 原件SHA256

- `work/hiby-controls-deepening/raw/manual-en.html`
  `d631a86d1cf1cd41e685034d1c6808efbdea05effc147aaf77452b2d4be27ede`
- `work/hiby-controls-deepening/raw/firmware-en.html`
  `324635ed3c27a01f6ffc0353c860813c65c960be79d34fc456f1109b49e28dc4`
- `work/hiby-controls-deepening/raw/faq-en.html`
  `10cc0e02d3059703e4189b2d9c7e7e4428e350a40358525685ff1b9c5be1555d`
- `work/hiby-controls-deepening/raw/usb-en.html`
  `5d13b45e18c4abfbfb996ee245650668de889020bf2ecec65a6b80327d7d52cb`
- `work/hiby-controls-deepening/raw/manual-zh.html`
  `38b541d793b2e7b343591d47ada81e5599c6a43a17e46a0131bd6fa4b654c84c`
- `work/hiby-controls-deepening/raw/faq-zh.html`
  `d132f48295345e8d33eb3a51f2c0cada130e5a2fd7ffc1b8c56f8318dbb96031`
- `work/hiby-controls-deepening/raw/spec-zh.html`
  `f741f4fed613ad0709abf57795962b27e7729f292765fe2ad29db963f4318b2e`
- `work/hiby-controls-deepening/raw/product-1.png`
  `df1452475f2b24af6adfd187de6214f9ddde5a99c391a4119f9318cb83933b91`
- `work/hiby-controls-deepening/raw/product-2.png`
  `ce5bc1038a41f76f3c1e5438a8dba7657809e1f8995b1dadfe6f3c49fa77cbc5`
