# Sony NW-A306 本轮交接

完成：18 E（E-SYCD-01～18）、6新F（11～16）、6旧F逐字段建议、82项目录；case现16F。D2；hands_on=false；两张手册静态摘页已实际查看。没有修改master/旧批/汇总，没有设备或账户状态变化。没有固件、App构建或地区实机基线，限定英文Help Guide 5-042-672-11(5) / NW-A306 YY1301B1。

## 正式文件

- docs/research/interaction-library/cases/M-036-sony.md
- docs/research/interaction-library/batches/sony-controls-deepening-evidence.jsonl
- docs/research/interaction-library/batches/sony-controls-deepening-coverage.csv
- docs/research/interaction-library/batches/sony-controls-deepening-inventory.csv
- docs/research/interaction-library/artifacts/sony-controls-deepening/source-notes.md
- docs/research/interaction-library/artifacts/sony-controls-deepening/manual-p022.png
- docs/research/interaction-library/artifacts/sony-controls-deepening/manual-p111.png

## 旧行合并

work/sony-controls-deepening/old-coverage-updates.json含完整before/after和changed_fields；检查时6条before均与主coverage一致。根写入前应再次断言，勿覆盖并行更新。

- F-M036-03: evidence_ids, known_branches, checked_on
- F-M036-04: evidence_ids, known_branches, gaps, checked_on
- F-M036-07: evidence_ids, known_branches, gaps, checked_on
- F-M036-08: task, evidence_ids, known_branches, checked_on
- F-M036-09: task, evidence_ids, known_branches, gaps, checked_on
- F-M036-10: evidence_ids, known_branches, gaps, checked_on

## 关键复核材料

1. https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000729844.html
   raw/05-usb-dac.html及extracted/05-usb-dac.txt；PDF p109–110。特别p110后台后无声是may，返回Music player也是may be able to restore。不能写成实测自动恢复；USB-DAC输入可用音效与USB音频输出不等同。
2. https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733525.html
   raw/07-microsd.html及extracted/07-microsd.txt；PDF p21–22；p22静态摘页已看。移卡后deleted和disappear分列；不推定音频被删或重插必恢复。

补充关键页：播放范围p90–91；播放页时间条误差p44；传回PC p69–70；重启p122、出厂清除p123–124。逐项目录从当前HTML菜单提取82条，再对照每章真正起始标题定位（已排除Related Topic中的同名引用）；目录≠完成。

原PDF work/sony-controls-deepening/raw/guide-existing.pdf，143页，复用work/audio-research/sony.pdf，SHA256 90b7cb2c48cf9e28b2dfb9fcde541a5e9e450b4ea6c63413fb9af2497181dced. 原下载时刻本轮未恢复；不是本轮新HTTP抓取。raw-pdf-manifest.json含来源、页码和两个摘页哈希。18个新HTML均HTTP200，未发生拒绝后重试；source-manifest.json含URL/实际最终URL/bytes/SHA。当前索引和Settings另有web工具阅读，正式原件用上述HTTP HTML。没有视频下载或播放。

## 运行命令和退出状态

以下工作目录均/home/workspace/reader：

- `rg --files work docs/research/interaction-library | rg -i 'sony|walkman|audio-inventory'` 与 `rg -n 'E-SYCD|E-SNWC' docs/research/interaction-library`：前者找到原件；组合命令exit1来自后者无前缀碰撞，非访问错误。
- 读 `cat AGENTS.md docs/research/interaction-library/batches/README.md`、`sed -n '1,35p' README.md`、`sed -n '1,45p' docs/requirements.md`；读取case、template及旧audio批：exit0。
- `PYTHONPATH=work/research/tools/pymupdf python3` 内联脚本：复制既有原件、抽143页文本到extracted/manual-pages.json，exit0；另用pymupdf渲染PDF零基21/110页到正式PNG，exit0。两PNG用view_image实际查看。
- `python3 work/sony-controls-deepening/acquire.py`：18个官方普通HTML HTTP200，exit0。
- `python3 work/sony-controls-deepening/extract.py`：18正文提取，exit0。
- `python3 work/sony-controls-deepening/build.py`：生成限定文件，exit0。此为交付前快照构建器，根编辑后不可重跑。
- `python3 work/sony-controls-deepening/check.py`：exit0/PASS，18E/6新F/6旧建议/82目录，所有原件/提取/正式摘记/图片SHA匹配；CSV真实LF分别7/83行，6/82记录，无字面\n或\r；E引用、链接、F唯一性及before匹配均通过。
- `git diff --check -- docs/research/interaction-library/cases/M-036-sony.md docs/research/interaction-library/batches/sony-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/sony-controls-deepening-coverage.csv docs/research/interaction-library/batches/sony-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/sony-controls-deepening`：exit0。

## 限制

时间点书签、定时停止、逐书断点、一般音频焦点/通话打断、重插卡关联恢复、Update与Rebuild的数据范围、完整备份/恢复仍未知。第三方High-Res streaming只限官方配置说明，不能推定任意App离线授权或后台行为。官方按键秒数和建库时间仅说明书数据，非测量。
