# M065 ak-directory-deepening 交接（2026-10-27）

状态：本有界批次完成并冻结正式文件；不完成总体goal。只改本case/本批/本artifact和work；未写主表、旧batch或全局文档。没有账号、设备、音频文件、应用或固件状态变化。无新helper。

## 交付与合并

- 3新E：E-AKDD-01/02为两篇首次取得的共用官方FAQ；03为既有E-AKCD-01同原件的五页派生静态图，不是第二来源。
- 2新F：F-M065-17 EQ、F-M065-18 ReplayGain；均D2、hands_on=false。旧16F保留编号，总18F。
- 202目录：36印刷目录+30设置标题+23明列设置子项+54编号界面项+26操作菜单项+24型号关联FAQ+9下载索引。层级有交叉；不是202独立功能或已完成操作。
- `old-coverage-updates.json`：4旧F、15字段；完整before/after和changed_fields。仅供根合并，不直接执行写主表。
- `old-evidence-additional-captures.json`：5旧E各append一项，本地PDF1+公开页面4。before可能为空，以当前字段读取作断言。
- 新证据的新URL已对全库JSONL查重；PDF同URL仅explicit derived_from的静态项。旧URL不重复新E。

合并前在仓库根执行（本次两轮均exit 0）：

```bash
python3 work/ak-directory-deepening/check.py
```

另执行 `git diff --check -- docs/research/interaction-library/cases/M-065-astell-kern.md docs/research/interaction-library/batches/ak-directory-deepening-evidence.jsonl docs/research/interaction-library/batches/ak-directory-deepening-coverage.csv docs/research/interaction-library/batches/ak-directory-deepening-inventory.csv docs/research/interaction-library/artifacts/ak-directory-deepening`，exit 0。检查器不联网、不写正式文件；合并后旧before断言应失败，不应为通过而重写before。**不要重跑build.py**：末尾另做了措辞收紧且父代理可能开始编辑。

CSV物理检查：coverage实际LF=3、DictReader=2；inventory实际LF=203、DictReader=202；两者无CR和字面量反斜线r反斜线n。引用、原件/摘页/正式文件散列、绝对本地链接及旧F/E before均PASS。

## 优先复核两处

1. 既有官方PDF印刷40/41/62/63（文件41/42/63/64）。40是重命名/整歌单删除；39成员移除才明确不删存储文件。41校准完成标记与启用开关分开。62正文SAVE但配图Rename EQ / CHANGE，不能消除图文矛盾。52的EQ Sync只给配置共享路径，生成触发/覆盖/跨固件规则未给。
2. [ReplayGain共用FAQ](https://www.iriver.jp/support/faq_1104.php)，正文Line Out时不工作；以及[封面FAQ](https://www.iriver.jp/support/faq_965.php)的AK70及第二代以后范围/旧数据库界面。两者被[SR35型号索引](https://www.iriver.jp/support/product_235.php)链接；关联不能证明旧截图或所有配置适用V1.22。本批没有查看这些FAQ图片。

## 原件与实际看图

官方PDF沿用本地原件，不重下：
`work/ak-controls-deepening/SR35_instruction_book_ENG.pdf`
SHA256 `2fc0dd11a7b6babfcc11028e2f1223c16320287f47adb18bb72ae10d97155182`
来源 https://cdn.astellnkern.com/bbs/download/202305/20230503165340_8b122109-9c25-4ef8-8ecc-69290747288f.pdf
73文件页，文件页=印刷页+1。2023索引/元数据沿旧采集，不当V1.22手册。提取保存manual-pages.json，本批目标正文读取8–11、13–26、35–41、49–63、66–67；目录1实际看图。余章仅目录或旧研究，不声称所有正文重新精读。

实际工具view_image只新看5页：印刷1/40/41/62/63；文件2/41/42/63/64。图是PDF印刷示意，不是触发动作、录屏或连续观察。旧24/30/48/59/60图不重复观察。

本批普通匿名GET六页面HTTP200；无401/403/429，无登录/软件固件或媒体下载。

| 原件 | URL | SHA256 |
| --- | --- | --- |
| work/ak-directory-deepening/raw/sr35-support.html | https://www.iriver.jp/support/product_235.php | `4d1e2e1174b32c40f671835cdc4f2437d082bddde1f847b7fa9807c3883f8a32` |
| work/ak-directory-deepening/raw/replaygain.html | https://www.iriver.jp/support/faq_1104.php | `93f7cd4546e5747aac70c09342fec9184bb598d2f47cfd001b0fef458a4c99c5` |
| work/ak-directory-deepening/raw/album-art.html | https://www.iriver.jp/support/faq_965.php | `3b42dc8677755292d4780d0b28668ab14364feaf592884bc771fcc756384ff12` |
| work/ak-directory-deepening/raw/art-size.html | https://www.iriver.jp/support/faq_1035.php | `90bb74086a63e37b33ff800c6f722f783d0dfc040f0bdb54ad6b4c19e0d7358c` |
| work/ak-directory-deepening/raw/m3u.html | https://www.iriver.jp/support/faq_1033.php | `7dbaa5bf1589006fb1c1223cb608faf358539c14eeb7ff92b602f4170a266857` |
| work/ak-directory-deepening/raw/v122.html | https://www.iriver.jp/support/file_1596.php | `1fdff34d5fe3aecc796398c77ae78d62a1750061934c338bd60fe7fd21d24f8d` |

## 正式文件清单/散列

- `docs/research/interaction-library/cases/M-065-astell-kern.md` — `9b469f925bdd2c7b31ab2ce599db6e5c1256977a8ede16aff219af41227959a0`
- `docs/research/interaction-library/batches/ak-directory-deepening-evidence.jsonl` — `b1e75cf35e98f2fb85e7c435c4d0d64f87c43503309788256b1e465dd96e950b`
- `docs/research/interaction-library/batches/ak-directory-deepening-coverage.csv` — `d72638e3e3cee825d7a6e7e2affeedd050bb76f4d7e0c22db96b146a2bbe47f5`
- `docs/research/interaction-library/batches/ak-directory-deepening-inventory.csv` — `32a97997a6a40350e3dcf14ab5c5e75fe34a863f0373a82d64363e34ecf4c87a`
- `docs/research/interaction-library/artifacts/ak-directory-deepening/source-notes.md` — `208461d068269f64ab8668855e1fb1c2103a552b7d7f3bd48330aca1dc6ee99c`
- `docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-1.png` — `c2f40511e8a87d7f94515fdef5b72417f0fc89f8d005bc4c9ddb2396ddb4a9ec`
- `docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-40.png` — `f2d2cf71ea72a61fdca427255740c8882c82f4608dbdf43e21af00f47caed8c1`
- `docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-41.png` — `1780dfad8317273d43b76103c6c51c660e352230761d8112708b83571ebb4e13`
- `docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-62.png` — `eb34b2b5715c557702c8f02fea5ca3e8d438fb950c7cab8fab1f73c88fb37ddd`
- `docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-63.png` — `7606a026d3979915bf669bca2d2e6dacadde65d2fea19ce853a73a2f4c48713b`

## 旧F逐字段范围

- F-M065-01：evidence_ids, known_branches, gaps, checked_on（4字段）。
- F-M065-02：evidence_ids, known_branches, gaps, visual_status, checked_on（5字段）。
- F-M065-09：known_branches, gaps, checked_on（3字段）。
- F-M065-16：known_branches, gaps, checked_on（3字段）。

旧E append：E-AK-12（支持索引）、E-AKCD-11（ReplayGain）、E-AK-10（M3U）、E-AK-04（日区V1.22）、E-AKCD-01（原PDF本地复读）。所有append含原件/提取哈希、真实读取方式、短摘记引用；未改旧E其他语义字段。

## 命令及退出状态

- `cat AGENTS.md && rg --files work docs/research/interaction-library | rg '(ak-controls|M-065|astell)'`：exit 0。
- `sed -n '1,130p' README.md && sed -n '1,180p' docs/requirements.md && sed -n '1,160p' docs/research/interaction-library/template.md`：exit 0。
- 读取case与旧E/F、全库E-AKDD前缀与两新FAQ URL查重的Python heredoc：exit 0；查重初始均无碰撞。
- `mkdir -p work/ak-directory-deepening/raw && PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY'`：使用pymupdf打开旧PDF、提取73页、SHA256、旧case/E/F快照；exit 0。原始结果保存在manual-pages.json与三个before快照，不重新抓PDF。
- `sed -n '1,260p' docs/research/interaction-library/cases/M-065-astell-kern.md && sed -n '1,100p' docs/research/interaction-library/artifacts/ak-controls-deepening/source-notes.md && rg -n 'M-065|Astell|ReplayGain' docs/ideas.md docs/research/interaction-library/README.md`：exit 2，旧source-notes不存在；后续独立rg成功，未补造旧文件。
- 首次HTML归档Python尝试import bs4失败：exit 1（ModuleNotFoundError），发生于请求前；未安装工具，改用标准库HTMLParser，成功归档六页exit 0。精确URL和响应码见captures.json。
- web工具open型号索引/ReplayGain；click索引11/25/21/34分别封面/大小/M3U/V1.22，均返回正文。web没有shell exit；未调用图片、账户、安装或下载链接。
- `PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY'` 渲染：`d=pymupdf.open('work/ak-controls-deepening/SR35_instruction_book_ENG.pdf')`；遍历`[1,40,41,62,63]`，`d[n].get_pixmap(matrix=pymupdf.Matrix(1.5,1.5)).save(w/f'sr35-printed-{n}.png')`；exit 0。随后view_image五次成功。
- `python3 work/ak-directory-deepening/build.py`：exit 0（只本批输出）。之后措辞收紧脚本exit 0；已同步formal-manifest。未执行任何前轮脚本。
- `python3 work/ak-directory-deepening/check.py`：exit 0；最新输出check-results.txt。
- `git diff --check --`上列五个本批范围：exit 0。case-change.diff为自身前后差异，不混根代理工作。

## 局限与下一证据

没有实机、测听、校准时间、设备版本核实或联网服务登录。EQ参数保存/删除/重名与EQ Sync生成、校准是否写文件或数据库、校准取消与失败、Folder的目标选择/确认/冲突/撤销及标签编辑保存都未闭环。分类次序/排序只明确入口。M3U仍D1，不借其格式措辞承诺成功导入。V1.22日期/锁键保留日本/全球差别，旧手册设置不冒充现版。睡眠、下载与USB角色旧16F不重复造任务。

State changed：仅本批研究文档和work归档，正式文件列表如上；无主表/旧批/全局修改，无外部设备/账号/服务写入。

## 实际读取文件

- AGENTS.md
- README.md
- docs/requirements.md
- docs/research/interaction-library/template.md
- docs/ideas.md（Astell/SR35相关行）
- docs/research/interaction-library/README.md（相关文本检索）
- docs/research/interaction-library/cases/M-065-astell-kern.md
- docs/research/interaction-library/evidence.jsonl（M065及URL核重）
- docs/research/interaction-library/coverage.csv（M065）
- work/ak-controls-deepening/SR35_instruction_book_ENG.pdf
- docs/research/interaction-library/batches/shanling-controls-deepening-evidence.jsonl（字段示例）
- docs/research/interaction-library/batches/shanling-controls-deepening-inventory.csv（字段示例）
- work/shanling-controls-deepening/old-coverage-updates.json（建议格式）
- work/shanling-controls-deepening/old-evidence-additional-captures.json（建议格式）
