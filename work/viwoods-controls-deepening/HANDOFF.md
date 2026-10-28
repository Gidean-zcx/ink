# M007 Viwoods AiPaper：冻结交接

2026-10-27，工作目录 `/home/workspace/reader`。固定10.65英寸AiPaper，Android13、V3.15文档基线；实际固件未知，不借Mini/Reader/Reader C。正式文件已冻结。仅研究文档与本地资料状态改变，没有登录、安装、设备/账号操作、音视频/固件/APK下载；未改主表、旧批、全局文档，也没有运行任何旧build/merge/helper。

## 交付入口

- 修改 `docs/research/interaction-library/cases/M-007-viwoods.md`。
- 新 `docs/research/interaction-library/batches/viwoods-controls-deepening-evidence.jsonl`：14E，E-VWCD-01—14，均supersedes:null。01是首次登记的V3.12官方发布说明；02—14为同一旧PDF的13张衍生静态页，不是独立交叉验证。
- 新 `docs/research/interaction-library/batches/viwoods-controls-deepening-coverage.csv`：3新F18—20，均D2、static_manual_pages_only、hands_on=false。
- 新 `docs/research/interaction-library/batches/viwoods-controls-deepening-inventory.csv`：578行。20个一级父组（17旧章+3本轮版本/FAQ组）、108个二级页/增量条目、450个三级控件项；存在包含关系，不等于578个功能或完整任务。
- 新 `docs/research/interaction-library/artifacts/viwoods-controls-deepening/source-notes.md`、`observed-pages.json`、13张 `manual-file-XX.png`。
- `formal-manifest.json`：19个正式文件的冻结路径与SHA256；`raw-manifest.json`：71个原始/中间文件的SHA256。两个清单本身只在work。

## 合并前检查

```sh
python3 work/viwoods-controls-deepening/check.py
```

已PASS，exit 0，结果在check-results.json。检查器只读、离线；核case基线、正式/原件散列、引用/URL、新ID未合并、逐页观察映射、CSV真实LF、旧字段before与主表一致及新E派生关系。合并后before/新ID断言会失效，勿作为合并后回归检查。

定向格式检查已通过，exit 0：

```sh
git diff --check -- docs/research/interaction-library/cases/M-007-viwoods.md docs/research/interaction-library/batches/viwoods-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/viwoods-controls-deepening-coverage.csv docs/research/interaction-library/batches/viwoods-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/viwoods-controls-deepening
```

请勿重跑本轮build.py：首次执行后对措辞、F10标题及散列作了明确收尾；正式冻结清单才是本批交付。没有改旧一次性脚本。

## 旧记录增量格式

`old-coverage-updates.json` 含10条对象：`flow_id`、完整 `before` / `after`、逐字段 `changed_fields:{field:{before,after}}`。先断言before，再只应用变更字段。共51字段：

- F01、02、03、04、07、08、09、11、13：各5字段，均为evidence_ids、known_branches、gaps、visual_status、checked_on。
- F10：同上5字段，再加task，标题从“通过ViTransfer传入资料”精化为“传入资料：账号收件与局域网入口”，共6字段。
- 所有旧F深度不变；F11原生云/权限仍D1。F05/06/12/14—17没有做无意义日期更新。

`old-evidence-additional-captures.json` 含4条：E-VIW-11手册入口、E-VIW-02 V3.15、E-VIW-10 FAQ、E-VICD-01原PDF。每条含 `evidence_id`、`expected_url`、`before_additional_captures`、`append_additional_captures`。旧首次采集保留；PDF capture明确记录本轮复取相同字节及68页静态观察，不把旧E的首次visual=false静默改写。`old-evidence-field-updates.json`为[]。

baseline.json / case-before.md保存案例起始状态；扫描旧JSON/JSONL未发现把本案例路径或基线SHA当归档的反向依赖，故无需迁移旧E散列引用。

## 根优先核对的结论

1. **旧手册不是新固件说明**：当前[User Manuals](https://viwoods.com/pages/user-manuals)仍链同一[英文PDF](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/viwoods-AiPaper-User-Manual.pdf?v=1787939059)。本轮GET200，17,098,207字节，与旧原件完全相同。官网Update正常导航仍到[V3.15](https://viwoods.com/pages/system-update-3-15)，没有取得更高AiPaper版本正文；只说明本轮可得资料，不证明所有渠道不存在新版本。未抓其他语言或其他型号。
2. **文图/版本差异**：文件47/印刷46历史密码分别管理，与V3.15共用笔记密码分期；文件48/印刷47正文清缓存、截图Clear Data；文件51/印刷50正文5条AI命令、截图4条；文件25/印刷24两指说明back位于Undo/Redo节。均未统一猜译为确定当前行为。
3. **删除范围**：文件19/印刷18 Paper删除入回收站没有期限；文件29/印刷28 Learning只有删书入口；文件69/印刷68最多30天仅Memo。Memo恢复与永久删除只有文字规则，具体按钮/确认/恢复位置未见。

新F18=Paper选区编辑/撤销；F19=Quick Note模板与关联文件夹；F20=Memo删除去向。AI对话返回选区、Paper跨笔记移页的完成反馈、数据恢复、权限字段依然未知。V3.12 App Display新增独立/跟随系统开关，补旧F01，不继续把“是否有逐App选择”整体列未知；保存/重启保持仍未知。[V3.12原文](https://viwoods.com/pages/system-update-3-12)

## 实际看图与原件定位

本代理实际view_image查看68个静态页：PDF文件2–69，对应印刷1–68。新渲染61页，原来已归档的2、38、39、40、53、54、55七页本次再次查看。文件1封面与70联系页未看图；未观察官网HTML图片、视频或实际设备。所有页路径、页码和散列见正式observed-pages.json。

13个新归档页及新E（目录均为 `docs/research/interaction-library/artifacts/viwoods-controls-deepening/`）：

| E | 文件 | 印刷页 | 核心定位 |
| --- | --- | --- | --- |
| E-VWCD-02 | manual-file-08.png | 7 | WLAN Transfer传输期间勿锁屏/关闭 |
| E-VWCD-03 | manual-file-19.png | 18 | Paper多选与删除入回收站 |
| E-VWCD-04 | manual-file-22.png | 21 | 快捷入口与关联目录双向关系 |
| E-VWCD-05 | manual-file-23.png | 22 | 模板名称/选择/Done示意 |
| E-VWCD-06 | manual-file-24.png | 23 | 手写工具栏、增页/返回 |
| E-VWCD-07 | manual-file-25.png | 24 | 套索与Undo/Redo、back措辞 |
| E-VWCD-08 | manual-file-30.png | 29 | Learning选区操作和输出对象 |
| E-VWCD-09 | manual-file-31.png | 30 | Back/批注/选区/撤销相互独立 |
| E-VWCD-10 | manual-file-32.png | 31 | 汇出批注页与显示范围 |
| E-VWCD-11 | manual-file-47.png | 46 | 旧独立笔记密码/忘密边界 |
| E-VWCD-12 | manual-file-48.png | 47 | 清缓存正文/Clear Data截图 |
| E-VWCD-13 | manual-file-51.png | 50 | 命令5/4条冲突与历史会话 |
| E-VWCD-14 | manual-file-69.png | 68 | Memo回收规则 |

原PDF旧路径 `work/office-aipaper-deepening/Viwoods-AiPaper-manual.pdf`；本轮原件 `work/viwoods-controls-deepening/raw/manual-current-capture.pdf`。两者SHA256同为 `90bd3c06b4bb4425ad62aef69dd2b553ab622ead68b01a668cea4ab3b5786676`。

HTML原件及提取在 `work/viwoods-controls-deepening/raw/`；captures.json含完整URL、HTTP状态、散列：

| 原件 | SHA256 |
| --- | --- |
| manual-index.html | c1cc5a0b660d53185214ea2ca28469571fcc5cd2d5d3ecf790065ba4ff746e55 |
| update315.html | d4c419a1c4425fcc0e654e9ff6eb81a3e970322ff7c6f82ea637bf26bd9ee59d |
| update312.html | 9ae8464cc53bfa86aced8e6182db2bde66136f1317bc40d8e5630b67236e9258 |
| faq.html | e08a7dec39db5a86bd03b2cf82cfbcda2f86d3c4658dc89ee3656661f6407dd0 |

案例基线SHA256：`fe3175e8faa80172134188fe1089e7bf56bcac834f91184567ee24c8987998f3`。

案例冻结SHA256：`34d716ee72e2308a4fbc17ba8a36b562f232f670119ef433d6fa54187d0037fe`。

独立摘记SHA256：`34b46514beec33e50cc881176a9f4ba9941931ca524d37a6267c6fa0526df467`。

正式manifest SHA256：`2efe8a7c54ce14a5963e893d2b9a1ed19e176f781ddbf1e43e157bc86beb56a7`；raw manifest SHA256：`2132cd3a3a956fbfc8bf19abbd1305cf4d180bf49aa25f19b43bfd1d32086348`。

## 命令与失败记录

以下均在 `/home/workspace/reader` 执行；完整实现和结构化输出保留在本work，未运行旧脚本。

1. `cat AGENTS.md && sed -n '1,95p' README.md && sed -n '1,140p' docs/requirements.md && rg -n 'Viwoods|AiPaper|M-007|撤销|选区|恢复' docs/ideas.md`：exit 0。
2. `rg --files docs/research/interaction-library work | rg -i '(M-007|viwoods|aipaper)' && rg -n 'E-VWCD|E-VICD|viwoods-controls-deepening' docs/research/interaction-library --glob '*.jsonl' --glob '*.csv' --glob '*.md'`：exit 0；旧VICD有命中，后续基线Python独立断言E-VWCD无占用，work不存在后才创建。
3. inline Python读取当前case、主E/F并保存baseline、全量20E/17F与反向依赖扫描：exit 0。旧work的fetch-report.json和viwoods-download.json用cat读取；旧build从未执行。一次宽泛rg误匹配旧HTML脚本造成输出截断，随后仅读精确报告与正文，不以截断输出作功能证据。
4. `PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY'`：使用既有PyMuPDF读取旧PDF metadata/文本span，并渲染所选页；各次exit 0。初次fitz有deprecated FutureWarning，无失败，随后用pymupdf。文件页N使用 `d[N-1].get_pixmap(matrix=pymupdf.Matrix(1.5,1.5),alpha=False).save(...)`；未装工具。
5. `python3 work/viwoods-controls-deepening/capture.py`：exit 0，4个HTML全部HTTP200。精确URL在脚本jobs及captures.json。普通公开web open/search/click亦成功，无shell exit；只查官方手册入口/版本/FAQ，搜索AiPaper V3.16/V3.17未取得更高版本，Update导航链接到V3.15。没有重试历史403或打开mytest域名。
6. inline Python `urlopen(Request(PDF_URL,headers={'User-Agent':'Mozilla/5.0'}),timeout=30)`：exit 0，PDF HTTP200，pdf-capture.json记录相同字节断言为true。本轮未遇401/403/429、登录或challenge，也没有访问绕过。
7. 共68次本地view_image成功，具体输入路径见observed-pages.json；文件2/38/39/40/53/54/55使用正式旧摘页，其他61页使用本轮raw渲染。静态内容没有观察时间戳或动作耗时。
8. `python3 work/viwoods-controls-deepening/build.py`：仅新建本轮脚本首次运行，exit 0；之后用明确inline Python收紧来源措辞、F10标题和case覆盖缺口，更新manifest，各次exit 0。不要重跑生成器。
9. `python3 work/viwoods-controls-deepening/check.py` 与上列定向 `git diff --check`：两轮均PASS/exit 0（收尾措辞变更后重新检查）。保存最终结果时一次apply_patch因同一补丁同时Delete/Add同路径被拒，未改变文件；改用Update成功。不是研究或HTTP失败。
10. SHA256核对命令使用 `python3` / `hashlib.sha256(Path(...).read_bytes()).hexdigest()`，exit 0；raw/formal两个manifest记录全部精确路径及哈希。

## 未决点

- 不能将旧手册所有入口视为V3.15已复现菜单。模板/AI命令上限、密码迁移、多用户权限及页组织的新版路径缺实际界面证据。
- Memo30天不是所有文件回收政策；恢复位置、提醒、云端传播、已过期与满盘失败仍未知。Paper/Meeting/Learning删除和导出可编辑性分别保留。
- Learning有Back不等于AI关闭后还原选区。Daily按关联笔记名回建立关联时的页只限手册说明；移页后是否继续有效未知。
- 已读全部历史功能页，但仅建目录、补少量任务；未展开邮箱账号/发信操作、投屏任务或其他型号，未把标题与控件数量冒充全部流程完成。
