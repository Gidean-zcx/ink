# M004 BOOX 控制路径补强交接

2026-10-26。固定 Note Air5 C / Android 15 / V4.1.2 基线；没有得到该构建的专用整册，实际设备构建未知。公开 ePaper Tablet PCs 指南为14节59篇；目录完备只指这一分类，不是该型号功能全集。

交付14个新E（E-BXCD-01–14）、0新F；7条旧F更新（02/03/06/07/08/09/14），4张实际看过的共用帮助静态图，均非Note Air5 C固件验证。D1安装/卸载渠道与菜单入口没有充当完整D2流程；所有hands_on=false，无D4/连续流程/耗时。

## 文件和状态

- docs/research/interaction-library/cases/M-004-boox-neoreader.md
- docs/research/interaction-library/batches/boox-controls-deepening-evidence.jsonl
- docs/research/interaction-library/batches/boox-controls-deepening-coverage.csv：只有11字段表头，刻意0新行。
- docs/research/interaction-library/artifacts/boox-controls-deepening/functional-inventory.csv：59条/14节。
- 同artifacts目录 source-notes.md、control-center.jpg、app-optimization-display.jpg、app-management.jpg、neoreader-refresh.png。
- work/boox-controls-deepening/old-coverage-updates.json：7条逐字段before/after与replacement_row。均改evidence_ids/known_branches/gaps；F02/03/14另改visual_status为static_reference_only。task/depth/hands_on不变。
- 同work目录：原API JSON、既有正文抽取、image-fetch.json、case-before.md、manifest.json、check-results.json、build_records.py/update_case.py/check_records.py。

仅这些授权case/batch/artifacts和自己的work有写入；没有修改master、旧batch或根文档。没有提交git或产品操作。正式archived_artifact相对interaction-library，指向长期保留的研究摘要、索引或静态图；原JSON留work，另列raw_work_path/raw_sha256。

## 主要原件与哈希

- work/boox-controls-deepening/epaper-pc-articles.json
  - HTTP200；https://booxsupport.zendesk.com/api/v2/help_center/en-us/categories/38115205868180/articles.json?per_page=100
  - 59条，next_page=null。
  - SHA256 02e30182beee444963ce691bbec9523380dcf91000c32685e27bea33f08a2cd8
- work/boox-controls-deepening/categories.json
  - HTTP200；https://booxsupport.zendesk.com/api/v2/help_center/en-us/categories.json?per_page=100
  - SHA256 5e2942088f48fbfc42c5cd23983dbefb2f431ef5fa1e3ea5f6c1e883ce65164f
- work/boox-controls-deepening/sections.json
  - HTTP200；https://booxsupport.zendesk.com/api/v2/help_center/en-us/sections.json?per_page=100
  - SHA256 26779a7df53408c21d0f35405ffb238519598505f1d0b46b9920bc05c01d2bf6
- work/boox-controls-deepening/manual-access.json
  - HTTP200；https://booxsupport.zendesk.com/api/v2/help_center/en-us/articles/8569203926804.json
  - SHA256 35140033fe52f764de3da172d42a8c877c74fcf28b3811cb7b9749dc28c9beae
- 复用的旧公开原件 work/eink-systems-research/boox-articles.json
  - 本轮只读，没有重新获取或写入，用于另一分类同名EinkWise对照。
  - SHA256 a6bbfd6cb4e8ff122152ccac6f0b504608734649ef826877f8024613de275390

四张图均正常HTTP200，原始地址、文件及SHA在image-fetch.json和manifest.json；已用view_image实际查看。web打开System Split Screen一次Internal Error；此前分类API已正常获得正文，后未重试该网页。没有401/403/429、代理、登录或下载固件/APK/远程视频。

## 根代理优先复核

1. 同标题跨类别手势差异：
   - PC分类 https://booxsupport.zendesk.com/hc/en-us/articles/10701257505044-EinkWise ，文章ID10701257505044，Access EinkWise/Method2写bottom right。
   - E Ink Tablet分类 https://booxsupport.zendesk.com/hc/en-us/articles/10990010331156-EinkWise ，旧原件文章ID10990010331156同节写bottom left。
   - 两页都提醒可自定义。不是两条可同时套入Note Air5 C的默认入口，更不能当一次实测纠错。
2. https://booxsupport.zendesk.com/hc/en-us/articles/10701299044372-App-Optimization 的Display/Reset All/Master Switch，与 https://booxsupport.zendesk.com/hc/en-us/articles/10701262170644-App-Management-Settings 的Reset all app optimization。
   - 单App临时关闭、单App恢复原值、所有App重置三范围不同。
   - Application只说可管理/卸载，没有卸载后文件或账户保留规则。安装页也只列渠道。

另外已落实：NeoReader Current Doc两窗标记在刷新后同步（不能外推关联Notes逐笔同步）；Notes Exit保存回Notepad View，More→Save是单独入口。当前通用Refresh页HD/Speed/Smooth与NeoReader页Regal/Speed不混为一张本机模式表。Flexible Split Screen帮助名单没有Note Air5 C，保持未知而非断言不支持。

## 准确命令与退出状态

- `rg --files docs/research/interaction-library/cases work | rg -i 'boox|neoreader|note.air|go.10|onyx'`：exit0，定位正确case与旧JSON。
- `cat docs/research/interaction-library/cases/M-004-boox-neoreader.md`：exit0。
- `cat README.md docs/requirements.md`：exit0；输出曾与其他结果合并导致截断，但此前同会话全文已读，当前重点已复核。
- `rg -n 'E-BXCD' docs/research/interaction-library`：exit1，无命中（初始前缀检查）。
- 原件获取为`python3 - <<'PY'`内联脚本，以urllib.request.urlopen对上述确切API地址读取并Path.write_bytes保存；分类/章节/59文章/手册入口各HTTP200，脚本exit0。没有重试访问拒绝。
- 选定4张静态附件同样用urllib正常HTTP GET，单次逐项读取；脚本exit0。实际URL与状态保存在image-fetch.json。
- `python3 work/boox-controls-deepening/build_records.py`：exit0，14E/0F/7旧行/59目录/14节/4图。
- `python3 work/boox-controls-deepening/update_case.py`：exit0，只修改M004；保留14个F编号。此脚本有防重复断言，不要再跑。
- `python3 work/boox-controls-deepening/check_records.py`：exit0，PASS；校验前缀、F编号、字段、相对归档路径、原件路径、引用、URL语法、SHA及观察标记。
- `git diff --check -- docs/research/interaction-library/cases/M-004-boox-neoreader.md docs/research/interaction-library/batches/boox-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/boox-controls-deepening-coverage.csv docs/research/interaction-library/artifacts/boox-controls-deepening`：exit0。文件目前未跟踪，该命令不能替代实际内容检查。
- `git status --short -- docs/research/interaction-library/cases/M-004-boox-neoreader.md docs/research/interaction-library/batches/boox-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/boox-controls-deepening-coverage.csv docs/research/interaction-library/artifacts/boox-controls-deepening work/boox-controls-deepening`：exit0；正式文件为untracked，work被git忽略但存在。

## 未完成边界

专用4.1.2手册及实际固件；面板退出后选区/页锚、完整模式默认值与参数优先级、优化重启/卸载后保留；通知/冻结/自启动/Stay active优先级；TTS焦点/锁屏/蓝牙中断恢复；安装失败与卸载重装数据关系；源文件移动/删除后的关联及系统回收保存。均明列未知，不因目录或静态图片补成已验证任务。
