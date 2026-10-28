# M-060 QUADERNO Gen.3C 本批交接

日期2026-10-27；root待核合并。只做匿名读取、本地提取和文档修改。设备/App/账户 state changed=false；研究文件 state changed=true。未改master、旧批、全局文档、上一HiBy批；不完成总体goal。不要在root修改/合并后重跑build.py或fetch-mobile.py。

## 交付和数量

- 修改：`docs/research/interaction-library/cases/M-060-quaderno.md`。
- 新建：`docs/research/interaction-library/batches/quaderno-controls-deepening-evidence.jsonl`（9 E：E-QCD-01～09）、`quaderno-controls-deepening-coverage.csv`（8 F：F-M060-09～16）、`quaderno-controls-deepening-inventory.csv`（275条）。
- 新建：`docs/research/interaction-library/artifacts/quaderno-controls-deepening/source-notes.md`、`gen3c-p57.png`、`gen3c-p66.png`、`gen3c-p86.png`。
- E-QCD-09是E-QUA-01派生静态页，不是第9个独立外部来源。其URL重复有derived_from；8个新Mobile URL全库无旧E重复。旧PDF未重新下载。
- 目录275=PDF原212（L1 6，包括目录本身/章；L2 55；L3 151）+设备设置44+笔键选项9+Mobile类3/文7。不是275项任务；父子项均保留，未分析条目不升级D2。
- 所有新F为D2/hands_on=false。F11/12/14仅对应局部手册静态页，其余not_observed，无D3/D4或连续视频。

## 给root的建议文件（没有改旧台账）

1. `work/quaderno-controls-deepening/old-coverage-updates.json`：F04～08五条，before/after全行+changed_fields；每行改known_branches/gaps/checked_on，共15字段。E不变，原E-QUA-01新增capture补充定位。
2. `work/quaderno-controls-deepening/old-evidence-additional-captures.json`：E-QUA-01本地原PDF复读；E-QUA-02支持目录、E-QUA-03下载页匿名HTTP200复抓；每条给before/after/append，只追加。
3. `work/quaderno-controls-deepening/old-evidence-date-corrections.json`：E-QUA-01～05的version_scope before/after；E01/E04的source_updated_on由2026-04-01建议改null，因为PDF内部只有2025-04发行月、官网只有2026-04-01公开日，不能猜PDF更新时间。共7字段。原版号与两种日期均在新scope/capture定位中保留。
4. `case-change.diff`及`formal-manifest.json`供审计；不自动合并。

## 两个优先复核来源

- 原PDF：`work/research/extension-manuals/quaderno-gen3c.pdf`，134页，SHA256 `d7248dfc9cd602139f4a5dddf03c33809b02b00d9f13601cf73f17ad1b8ac5f4`。来源 https://www.fmworld.net/digital-paper/support/download/pdf/B5FL2101.pdf 。p134正文在`work/quaderno-controls-deepening/extracted/p134.txt`：末页编号B5FL-2101-02 Z0-02，发行2025年4月；封面编号B5FL-2101-02 Z0。官网 https://www.fmworld.net/digital-paper/support/ 的当前HTTP200原件`raw/support.html`、提取`extracted/support.txt`另列Gen.3C公开日2026年4月1日。根已独立核对这两处，不能把日期修正写为换了手册。
- 手机连接 https://www.fmworld.net/digital-paper/mobileapp/helpguide/contents/QHG00001.html ，原件`raw/QHG00001.html`（Shift_JIS→cp932解码）、正文`extracted/QHG00001.txt`。建立直连与设备/手机Wi-Fi断开并写的句子存在字面矛盾；紧接下一句明确切App或停止设备AP会断开直连并恢复设备/AP连接。本批没有擅自解释成“旧网络断开”，没有互联网/蜂窝回退保证。根已复核这个疑点。

其他关键页：p35换页清空撤销/已有PDF书签；p57/59图形搜索按页/文档去重；p66单次剪贴/重启清空；p71关联替换变独立；p72删页不可恢复；p78–79热点占用窗口；p82–86扫描/Received及iX100例外；p97/123备份排除/容量/失败/改名不显示；p99–102同步冲突、删除绕回收站；p115转换失败只读/释放空间/按消息重启；p121仅2.1.00.26250FP历史问题的修复副本。根已核99–102/121，不同恢复层保持分开。

## 读取、取得与命令/退出状态

以下路径均相对`/home/workspace/reader`；shell bash。详细原件哈希/URL/状态见`captures.json`，正式文件哈希见`formal-manifest.json`，逐页提取和图片映射见`read-pages.json`/`rendered.json`。

- `rg --files work docs/research/interaction-library | rg -i '(quaderno|B5FL2101|M-060)' && cat docs/research/interaction-library/cases/M-060-quaderno.md` — exit0；定位旧PDF/文本/旧图/案例。
- `pwd && cat AGENTS.md && sed -n '1,180p' README.md && sed -n '1,180p' docs/requirements.md && sed -n '1,180p' docs/research/interaction-library/template.md` — exit0。
- `cat docs/research/interaction-library/cases/M-060-quaderno.md && head -n 3 docs/research/interaction-library/batches/hiby-controls-deepening-inventory.csv && head -n 1 docs/research/interaction-library/batches/hiby-controls-deepening-evidence.jsonl && cat work/hiby-controls-deepening/old-coverage-updates.json` — exit0；仅看旧批schema，没有执行或修改旧构建器/旧批。
- Python标准库脚本过滤master M060并核E-QCD前缀/新Mobile URL — exit0；初始前缀无碰撞；快照为`case-before.md`、`coverage-before.json`、`evidence-before.json`。
- `PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY' …` — exit0；对既有PDF `fitz.open`/`get_toc`、逐页`get_text(sort=True)`，导出`outline.json`（212条）、`extracted/p001.txt`～`p134.txt`。最初fitz导入仅有弃用警告；后面改用pymupdf，无安装。
- 分组`python3 - <<'PY' … Path(.../pNNN.txt).read_text() …` — exit0；核读目录与相关页，具体页集在`read-pages.json`。部分一次输出截断，后续关键页分组重读；不以成功提取134页宣称通读134页。
- `rg -n 'しおり|ブックマーク|目次|取り消|戻す' work/quaderno-controls-deepening/extracted/p0{3,4,5,6,7}*.txt` — exit0；定位书签与撤销。
- 首次HTML抓取inline脚本导入`bs4`失败 — **exit1，ModuleNotFoundError**；发生在网络读前，没有安装依赖。改用Python标准库HTMLParser。
- 第二次inline脚本HTTP200取得mobile-index/download；mobile-index按UTF-8解码失败，被异常处理捕获，因此shell exit0；其原件已保存。随后从同一原件按声明Shift_JIS用cp932解码，**没有重新请求/绕过**。下载页UTF-8成功。
- `python3 work/quaderno-controls-deepening/fetch-mobile.py` — exit0；QHG00001～00007各HTTP200，解析/散列成功；索引从本地已存原件解析。无401/403/429。
- support页另用一次`urllib.request.urlopen`匿名GET保存原件并HTMLParser提取 — exit0，HTTP200。全批10个HTML原件均200（Mobile首页+7正文+download+support）。网页/图片中的App、固件、视频链接没有执行下载。
- `PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY' …` — exit0；只渲染原PDF页57、66、86，矩阵1.6，图片SHA在rendered.json。随后tool `view_image`三次实际查看成功，观察仅三张静态页；不是设备操作。
- `python3 work/quaderno-controls-deepening/build.py` — exit0；只生成本批正式文件及work建议。M060案例用单独here-doc写入，exit0。随后修正旧F06建议措辞的定点Python编辑exit0，未重跑任何旧批构建器。
- **`python3 work/quaderno-controls-deepening/check.py` — exit0 / PASS**：9 E、8 F、16案例F；5旧F15字段；3旧E追加的before及5旧E日期修正before一致；原件/提取/正式图及摘记SHA一致；引用存在；新URL无非派生重复；新coverage8行/9个LF、inventory275行/276个LF、无字面量反斜杠r反斜杠n；局部链接存在；hands_on=false、无D3/D4。

## 未决边界

- 当前安装固件/App构建未知；Mobile帮助未标版本，还提旧PC App名。官网适用条件并不能证明所有步骤在当前手机OS保持相同；iOS单文件限制只按该帮助口径。
- p87/110–111更新后续只写遵循屏幕提示；无完整安装进度/错误/回滚链。F15专门记录转换只读恢复分支，不宣称安装流程已闭合。
- 新任务仍无实机成功率/耗时/重启恢复测试；手机错误认证/拒权/满盘/重名/后台中断，扫描卡纸/取消/部分文件、剪切后崩溃和跨端关联保留均未知。
- 文档正文明确的无恢复仅限笔记删页；不能推广到全部删除。RESET也有数据丢失风险，未把它写为安全恢复。
- 未扩查其他型号/新版本/商业评测，没有购买/发送/安装/固件执行；所有限制保持有来源的事实或明确未知。
