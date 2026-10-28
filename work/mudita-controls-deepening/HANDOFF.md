# M012 Mudita Kompakt 交接（2026-10-27）

本批完成：22 新E，3 新F（08–10），7 旧F补证建议。case现在10F，均D2、hands_on=false。主表、旧批、根文档未修改；没有运行设备/产品代码、安装、刷机、登录、拨号、发短信或提交。只新建/改写研究文档、原件、静态图及本批工作脚本。

## 正式文件

- `docs/research/interaction-library/cases/M-012-mudita.md`
- `docs/research/interaction-library/batches/mudita-controls-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/mudita-controls-deepening-coverage.csv`（仅F08、09、10）
- `docs/research/interaction-library/artifacts/mudita-controls-deepening/function-inventory.csv`（111行；15手册目录+81支持文章+15发布功能域）
- `docs/research/interaction-library/artifacts/mudita-controls-deepening/manual-p03.png`
- `docs/research/interaction-library/artifacts/mudita-controls-deepening/install-complete.png`
- `docs/research/interaction-library/artifacts/mudita-controls-deepening/observations.md`

目录阅读状态：30 text_read（15手册章节/15发布域）、16 body_read、1 prior_body_read、64 index_only。目录条数不是完成任务数。手册未标OS；官方1.6条目明确附件对应1.6.1；桌面4.0.0只由发布说明固定，现行帮助/截图不冒充该精确构建。

## 旧覆盖行交根合并

`work/mudita-controls-deepening/old-coverage-updates.json` 保存F01–07的逐字段 before/after 和完整 replacement_row。before取本批开始时的旧reader-coverage.csv；首次检查逐字段匹配。根代理并行合并后，终检已逐行精确匹配replacement_row（after_already_applied），建议文件保留原before/after。

- 所有7行：补 `evidence_ids`，精化 `known_branches`、`gaps`，`checked_on`为2026-10-27。
- F04、F05：`visual_status`从not_observed改static_reference_only；依据仅手册p3静态图。
- F01、02、03、06、07：visual_status保持not_observed。尤其安装完成图不被算作文件删除操作画面。
- task、depth=D2、hands_on=false、case_path、编号均保持；未将旧流程再次写入新coverage。
- 新F08为应用安装/图标/安装包/卸载边界；F09更新和失败清空恢复；F10备份与选择恢复。

## 关键复核原件

1. 官方1.6发布入口：
   https://support.mudita.com/en/support/solutions/articles/77000598634-muditaos-k-1-6
   `work/mudita-controls-deepening/release-1.6.html`，SHA256 `784cbfff82c8f7c7c3c5d1ee2bf05d260fd71cf15914b17f94b77bf61dc2086a`。
   定位Why 1.6.1及Key fixes：1.6.0暂停扩大推送；1.6.1修正未接来电计数；退出Offline+通常不再要SIM PIN，但重启后立刻退出仍要。
   英文附件 https://support.mudita.com/helpdesk/attachments/77279297918 ，`release-1.6.pdf`，SHA256 `0667012d506810e4972a53d8e8c62c43a9e744fd4b55ea37cbb91a8d05fba3d9`，6页；p1变化日期2026-08-24、p4Phone、p3Maps、p5SMS/录音。附件题名仍K1.6；不把变化日期当网页修改日。未视觉查看附件。

2. 第三方app FAQ：
   https://support.mudita.com/en/support/solutions/articles/77000577356-does-mudita-kompakt-support-third-party-apps-
   `apps.html`，SHA256 `d2c31681f6b58ce29ae30f85a4eb6f220543321b3fe64bb034bb5d1a3a5f6049`。
   定位Uninstalling sideloaded apps：设备/Center当前无内置卸载；恢复出厂移除全部并清数据；社区管理器仅转述线索，未核查。不同于删除APK安装包（install.html Step10），也不同于隐藏图标（hidden.html）。不把K2.0预告当已发布。

补充：

- 官方12页手册 `manual.pdf` SHA256 `2e72d237dd063b30bc18d325c4fcfcbd69ce1f62c864ed7c878429a7b7a9801e`，原URL见E-MDCD-01。p3已实际看图；p4目录/无障碍、p2实体键，p9电池。元数据2026-01-27，不代表OS构建。
- Offline+ FAQ `offline-faq.html` SHA256 `7b535ee59e4397ff721c97d3b58d2b9f0a612298cf88b5ea0e84d967cee4eb64`。分清GSM/麦克风硬件停用与相机/Wi-Fi/蓝牙软件停用，不宣称全部硬件断电。
- 备份 `backup.html` SHA256 `33b6ee6fc7f281996e43bf75ecce905eb914d19a52d6159b7edb9d33cd03ee13`；本地实际正文Modified Oct3，先前web缓存日期不采用。
- `center-product.html` SHA256 `e8308343e16ef186b9d562ec3254e75c5e4686b9d4fb609ced74215a50cc285f`：仅Kompakt段五类数据；Pure段的settings不移用。
- `center4.html` SHA256 `ec7b42c56bb090ca2736bdc04c37fb8b8fb64ab332ed0f2957f8cb8312be63c5`：4.0.0新增Calendar events备份。
- 官方安装完成PNG，原件与正式图相同SHA256 `bf294a52bbed14dd26ff3cf60b1505460fefcacc0cbdd91e82a888fc4073da8e`；实际view_image见完成弹窗、Close和可断开提示。无连续安装过程/构建号。

所有39个公开HTTP获取原件的确切URL、相对路径、状态、字节数、SHA：`work/mudita-controls-deepening/source-manifest.json`，原始获取记录 `fetch-log.jsonl`。正式evidence的archived_artifact只指library相对长期路径；原件均用raw_work_path/raw_sha256。

## 实际命令与退出状态

本轮只读/生成使用：

- `rg --files ...`定位；`rg -n 'E-MDCD' docs/research/interaction-library`初次查冲突：exit1表示无匹配。未重用其他批前缀。
- `python3 work/mudita-controls-deepening/fetch.py <filename=url ...>`：逐请求确切参数见fetch-log.jsonl各行的path/url，39次原件请求均HTTP200。脚本保留请求与哈希，未下载APK/固件/视频。
- HTML用本地stdlib HTMLParser读取，具体脚本 `work/mudita-controls-deepening/parse_html.py`。最后执行：
  `python3 work/mudita-controls-deepening/parse_html.py work/mudita-controls-deepening/center-index.html work/mudita-controls-deepening/release-1.6.html work/mudita-controls-deepening/update.html work/mudita-controls-deepening/factory-reset.html work/mudita-controls-deepening/apps.html`：exit0（有正则字符串SyntaxWarning，不影响结果；无包安装）。
- `PYTHONPATH=work/research/tools/pymupdf python3`读取PDF并渲染手册p3；exit0，已有工具，未新装。末次用pymupdf.open校核两PDF分别12/6页，exit0；`manual.pdf`创建与修改元数据为20260127091847/52+01:00，release PDF无日期元数据。
- `python3 work/mudita-controls-deepening/build_batch.py`：exit0，生成22E/3F/7建议/111目录。随后定点修正MMS的Plus是新建入口、删除软连字符，并将附件Date of change单列document_change_date。**不要重跑生成器覆盖后续审校。**
- `python3 work/mudita-controls-deepening/check_batch.py`：exit0。检查22E前缀无其他批碰撞，3新F顺序，7旧行before匹配，36新旧证据引用、111目录、相对链接、正式图片路径/哈希、39原件哈希、无D3/D4或hands_on true。结果在check-results.json。
- `git diff --check -- docs/research/interaction-library/cases/M-012-mudita.md docs/research/interaction-library/batches/mudita-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/mudita-controls-deepening-coverage.csv docs/research/interaction-library/artifacts/mudita-controls-deepening`：exit0。目标在git中均未跟踪，故这项不代替自定义内容检查。
- `git status --short -- docs/research/interaction-library/cases/M-012-mudita.md docs/research/interaction-library/batches/mudita-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/mudita-controls-deepening-coverage.csv docs/research/interaction-library/artifacts/mudita-controls-deepening work/mudita-controls-deepening`：exit0；work被忽略，正式目标显示??。

发生过的失败：初次Web工具开附件返回500/Internal Error；普通公开附件URL随后urllib成功HTTP200。一次混合Python单元在已成功取得PDF后尝试import bs4，ModuleNotFoundError导致exit1，随后改用stdlib，没有安装。未出现401/403/429，未绕过访问限制。未启helper/worker。

## 未知与状态

音频焦点/来电后自动恢复、普通接听拒接全过程、短信草稿与离线重试、来电/短信补收、媒体路由、侧载app后台/覆盖升级/权限、备份合并/覆盖/损坏/跨版本/断线、媒体或第三方app数据备份均不作承诺。更新恢复出厂保留OS版本，与固件回滚分开。文档能确定的“不支持”只用于明确FAQ（RCS、当前内置卸载等）；只读目录项不被写成不支持。

本地研究文件状态已改变；产品、账户、服务和用户设备状态未改变。

## CSV 换行最终核查

根代理提醒后以read_bytes()/repr/count及DictReader核查。转换前coverage为4个实际LF且4个CRLF，inventory为112/112；两个文件的字面量`\\r\\n`序列计数都是0，DictReader分别读3/111数据行。用`csv.DictWriter(..., lineterminator='\n')`实际改写后：coverage为4个LF，inventory为112个LF，CRLF和字面量序列都为0；数据行数仍3/111。该Python检查/改写exit0。现check_batch.py含换行断言，最终执行exit0；check-results.json保存计数。

## 并行合并状态补记

添加LF断言后的第一次重跑，旧批before不变断言触发（Python exit1；同一shell后续git检查exit0不能覆盖该失败的含义）。只读比较发现7条旧行的每个变更字段都已精确匹配after；这是根代理并行合并状态，不是本代理写旧批。检查器现分别接受并报告完整before或完整after，拒绝其他差异；原建议文件未改。最终独立执行`python3 work/mudita-controls-deepening/check_batch.py` exit0，7行均after_already_applied；随后独立git diff --check exit0。
