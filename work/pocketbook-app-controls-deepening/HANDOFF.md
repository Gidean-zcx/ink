# M003 PocketBook App与设置补充交接

2026-10-27，范围已收束。仅PB743K3 / InkPad Color 3，6.10.2854路径的既有英文95页手册；没有重新判断最新固件，实际安装构建未知。正式11文件已由formal-manifest.json冻结，原件9文件由raw-manifest.json核对。本批只修改本案例并新增本批文件，没有改主表、旧批或全局文档。

## 交付与数量

- 案例：`docs/research/interaction-library/cases/M-003-pocketbook.md`，由9F增至16F；旧F01/03内容精化。
- 新批：`docs/research/interaction-library/batches/pocketbook-app-controls-deepening-evidence.jsonl`：5条E-PBACD-01～05，全部是E-PBK-01同PDF衍生的实际查看静态摘页，supersedes=null，不增加独立来源。
- 新批：`docs/research/interaction-library/batches/pocketbook-app-controls-deepening-coverage.csv`：7条F-M003-10～16，全部D2、hands_on=false。分别为Libby借还、PIN恢复、用户档案、读屏导航、Scribble、相框照片列表、重置/格式化对象。
- 新批：`docs/research/interaction-library/batches/pocketbook-app-controls-deepening-inventory.csv`：保留65个原目录名及文件/印刷页码，26行新增本轮正文控件范围；父章与子项有包含关系，非26个完成任务。剩余行明确继承旧目录范围。
- 独立摘记：`docs/research/interaction-library/artifacts/pocketbook-app-controls-deepening/source-notes.md`；逐图审计：同目录`observed-pages.json`；图为`manual-p29.png`、`manual-p37.png`、`manual-p41.png`、`manual-p76.png`、`manual-p77.png`。
- 本work保留baseline、12旧E/9旧F快照、原65项目录、选读正文、图片、精确旧行建议、检查脚本及两个manifest。`assemble.py`只执行过一次，不要重跑；后续定点澄清已直接写入正式文件。

## 原件与实际观察

核心原件：`work/research/interaction-depth/pocketbook-6.10.2854.pdf`，95页，SHA256 `87bf56b461cd298b7c53421c5b21f72f1498e4c060d4eea89844545667030101`。

原URL：<https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/manual/User_Manual_InkPad_Color_3_EN.pdf>，复用旧E-PBK-01；既有E-PBCD-01仍属历史同URL记录，本批未新建整册手册E。PDF元数据2025-12-26只是文件时间；前批6.10.2854 WW发布说明2026-01-05未在本批重新阅读或下载。

本轮用既有PyMuPDF提取并读文件/印刷28–30、34–41、69–90，共33页。文字留在`raw/selected-manual-text.json`，元数据在`raw/manual-metadata.json`；既有`work/pocketbook-controls-deepening/pb743k3-text.json`也读作定位。文件页与印刷页相同。

实际调用view_image成功的本地路径只有以下5张；图像原件位于本work/raw，正式副本与原PNG散列一致：

| 新E | 文件/印刷页 | 本地实际查看图 | SHA256 | 观察边界 |
| --- | --- | --- | --- | --- |
| E-PBACD-01 | 29 | work/pocketbook-app-controls-deepening/raw/manual-p29.png | 91a642c829ee86c443cca95da9520f27becca19aa56823da9374f3886de31347 | 相框加图/播放/齿轮图标；未执行 |
| E-PBACD-02 | 37 | work/pocketbook-app-controls-deepening/raw/manual-p37.png | 71c8ebced51aafd6c375ebb3d15cd8840daaca2b5965c2ba39730d8ded46c654 | Download/Place Hold示例；Return Early仅正文 |
| E-PBACD-03 | 41 | work/pocketbook-app-controls-deepening/raw/manual-p41.png | 6ce55bf9aa6ad416835838c01cf305dbdf8cd5a0d3b0fe2a3ebc600bf595b554 | New与保存句同形纸张图标，保留歧义 |
| E-PBACD-04 | 76 | work/pocketbook-app-controls-deepening/raw/manual-p76.png | 1eca66a5bfc0b806f48886b4338ec5f68a38474fde51c95fe6857d2ad74978c5 | 读屏off/快捷on为示例，不是默认 |
| E-PBACD-05 | 77 | work/pocketbook-app-controls-deepening/raw/manual-p77.png | 5b374e901b219cf0946255d1b43ec36042bb4f28078d030db44502f30d73562b | 焦点、激活、滑动及保持手势说明 |

没有连续观察、设备操作或测时。仅看图不能确认各按钮的执行结果，也不能确认外设和安装构建。

## 旧行合并建议格式

`old-coverage-updates.json`含2对象，每对象为`flow_id`、完整`before`、完整`after`、`changed_fields: {字段: {before, after}}`。共7字段：

- F-M003-01：known_branches、gaps、checked_on（3字段）。补退出设置节应用变更的总则、阅读手势可配置、HID键映射；不推定默认、适配或后台保留。
- F-M003-03：evidence_ids、known_branches、gaps、checked_on（4字段）。补Scribble图形注释关系及E-PBACD-03；不把My Pictures路径变成Notes输出格式或可回导证明。

`old-evidence-additional-captures.json`仅1对象E-PBK-01，格式为`expected_url`、`before_additional_captures`、`append_additional_captures`。新增1次2026-10-27本地复读，记录原PDF路径/散列、33页正文、5页实际图、摘记散列；没有HTTP请求或新修订。`old-evidence-field-updates.json`为空数组，无其他旧E字段变更。

案例基线SHA256：`2e1a70bc1541a4f41ca097bee98a9503f6f087e44bf8e925e912ade931a46e13`。基线全库E扫描未发现旧E以此案例路径/散列作为归档依赖，`baseline.json`中的reverse_refs为空。旧65项目录SHA256：`5489405b3b33cc6a46ff378710510a5f400a8b78c81630964c7602709fa01b29`。

## 复核优先定位与保留疑点

1. p34、37–38：Libby应用需联网；预览不需登录但不能借，借阅需馆卡。借阅电子书只是“通常”下载，无法下载则webview需网络；借阅音频只说明在线听。没有把下载错误后的自动降级、离线音频、到期笔记保留或跨端断点写成已验证。
2. p41：Back定义撤销上次动作；New与保存句用同形图标，不能确定是否同一动作或新建自动保存。Coloring自动保存不套Scribble，格式、取消、恢复与Notes回导仍未知。
3. p72–78：Forgot PIN需输错三次后点击才请求邮件；邮箱和客服条件不是离线恢复保证。Menu三击打开无障碍菜单与Power三击启动已启用的读屏快捷分开；快捷关闭效果未明。
4. p83–86：用户档案列保存若干阅读状态及网络，不证明文件/Cloud/密码隔离。Factory Reset清用户设置而保留书/图片是手册范围；Format Internal Memory删系统文件外数据。确认、取消、完成回执及笔记/档案细项未核。
5. p84、87–90：空闲锁屏/关机时间不能证明播放期间行为；更新表的card、SWUDATE.BIN和Abouut原文问题保留，不推出PB743K3额外卡槽，不执行更新或保证故障恢复。游戏只登记有限控件，不凭功能名称补判分/保存任务。

## 可重复检查及执行结果

从`/home/workspace/reader`运行，以下检查脚本只读，不联网、不开浏览器、不写文件；仅限root合并前，合并后before断言预期不再适用：

```sh
python3 work/pocketbook-app-controls-deepening/check.py
```

实跑exit **0 / PASS**。核对原件与正式散列、新E/F不冲突、65原目录名/页码、两条旧F逐字段before、E-PBK-01追加前值、图文范围、引用和本地链接、LF真实换行。结果：5新E、7新F、2旧F/7字段、1旧E追加采集、65目录/26正文补充、33文字页、5实际静态页、11正式文件、9原件文件、hands_on=false、network_requests=0。检查本身不改变状态。

```sh
git diff --check -- docs/research/interaction-library/cases/M-003-pocketbook.md docs/research/interaction-library/batches/pocketbook-app-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/pocketbook-app-controls-deepening-coverage.csv docs/research/interaction-library/batches/pocketbook-app-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/pocketbook-app-controls-deepening
git status --short -- docs/research/interaction-library/cases/M-003-pocketbook.md docs/research/interaction-library/batches/pocketbook-app-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/pocketbook-app-controls-deepening-coverage.csv docs/research/interaction-library/batches/pocketbook-app-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/pocketbook-app-controls-deepening work/pocketbook-app-controls-deepening
```

两项均exit **0**；这些正式路径在当前git为untracked，故diff空输出本身不足以校验内容，主检查使用baseline和manifest，不把git无输出当成全量通过。work被git忽略。

本批执行过的生成命令为`python3 work/pocketbook-app-controls-deepening/assemble.py`，exit0，仅一次。提取/渲染使用`PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY'`的本批内联脚本，exit0，读取既有PDF、选33页文字并以1.6比例渲染上述五页；没有新装工具。全部view_image成功，不适用shell退出码。最终针对性文字修正及manifest生成均exit0，没有重跑前批脚本。

唯一失败：最初定位并读案例后尝试`cat work/pocketbook-controls-deepening/HANDOFF.md`，该旧文件不存在，所在命令exit **1**。随后只读旧`scope-manifest.json`、旧检查脚本源码、建议和原件完成定位，没有执行旧脚本。原目录实测66个物理LF、0个字面量\\r\\n、DictReader65行；未擅改旧CSV。旧work文本扫描未见可用HTTP失败日志；本批不发网络请求，不重试任何历史外部路线。

## 冻结散列和状态

- 最终案例：`059e5d2e33481dd3faff112809a8f56574a626460bfbf16d5c9409decfdda4f5`
- 独立source-notes：`f773ab64456a13a2a23ad3a8fa2d77d8cf0610879c8a63394685378355936a40`
- formal-manifest.json：`5f28da90c9329497213585fadefc2e8d95f1b5406abdc491739deebfaee019f7`
- raw-manifest.json：`8c0b3386bd0a5a5410e8980b6bf1744a9249379c3eea032eac602d1028961b4b`
- check.py：`d40608034935e9e827136bf1e790fb12312364ad0379e159429fe19af5572c58`

状态改变仅限本例与本批研究文件；没有设备、账户、网络服务、固件、外部消息或产品代码变化。没有宣称整体研究目标完成。交接后不再修改正式冻结文件；root据材料复核并决定合并。
