# M-020 Anki Desktop 控制与恢复补证交接

2026-10-27。**7新E（E-ANCD-01～07），0新F**；原22F均保留D2，没有软件运行或图片观察。候选E-AKCD已占用，使用全库未占用E-ANCD。只修改下述5个正式文件及本批work；未修改旧批、主表、全局文档，也无外部账户状态变化。

## 正式文件

- `docs/research/interaction-library/cases/M-020-anki.md` — SHA256 `020559f53e733ecf298d5c26657163467ea9a609617ede103fcfe2e847c82007`
- `docs/research/interaction-library/batches/anki-controls-deepening-evidence.jsonl` — SHA256 `ed788868179a12cb6d7678e62fdec9bc47b6bb657d4fc2ff6a8dbaffe9d78505`
- `docs/research/interaction-library/batches/anki-controls-deepening-coverage.csv` — SHA256 `aa0cf671b21fa0390e9b0cfb58c6cecf3a0d7df66eaad894707f17ea224f6025`
- `docs/research/interaction-library/batches/anki-controls-deepening-inventory.csv` — SHA256 `b8a0bcbeafb8ce39c9683ab12f05a208010070e4cb3a00207bd1f7430fecdf19`
- `docs/research/interaction-library/artifacts/anki-controls-deepening/source-notes.md` — SHA256 `01498c2eac1b71bf6f2dca8b322ec08f0598b61c0c36e1b012c64cc85860222d`

覆盖CSV只有正常LF标准表头。inventory是55个Desktop Manual页面入口（包括平台父子项，25页有正文、30页仅目录）+5个补充源；**不是功能完成数/百分比**。没有正式图片；source-notes是短摘记，不是整页归档。

## 待root合并的旧行建议

- `work/anki-controls-deepening/existing-coverage-updates.json`：F-M020-01/02/03/07/10/15/18/20/21共9旧F、36字段，均为evidence_ids、known_branches、gaps、checked_on；逐字段before/after并有source_batch。CSV同名方便审阅，正常LF。
- `work/anki-controls-deepening/additional-captures-updates.json`：E-AND-01/12/15/17及E-ANK-02/03共6旧E追加采集建议；不新编号重复同页，不替换原取得方式。
- 未应用上述建议。case末交接说明待root合并后可改为已合并。

## 核心抽核入口

1. [v3 FAQ](https://docs.ankiweb.net/faqs/the-2021-scheduler.md)：Enabling / Undo / Burying。原件`scheduler-2021.md`。回答与非回答动作可交错顺序撤销；Desktop23.10起只用v3；跨日学习卡可暂埋。当前Studying笼统描述保留范围差异。
2. [2.1.45发行说明](https://docs.ankiweb.net/releases/changes/changes/2.1.40-49.md)：原件`changes-2.1.40-49.md`第114行起；2021-07-30。多数动作多步撤销/重做、即时保存限新撤销动作；数据库检查/部分插件清栈。不能写为所有操作可恢复。
3. [Backups](https://docs.ankiweb.net/manual/backups.md)：原件`backups.md`的Deletion log；新格式条件是**晚于25.09.2创建的档案**，不是安装版本号。日志恢复评分/媒体未知；即时/自动备份不含媒体。
4. [Syncing](https://docs.ankiweb.net/manual/syncing.md)：Merging Conflicts和Media；先取消第二端同步、逐顶层导出、Download后apkg导回。媒体始终独立合并，不随单向方向替换；删除传播/重新下载/云端尚存恢复均有条件。
5. [模板错误](https://docs.ankiweb.net/manual/templates/errors.md)：Basics及Specific Issues。保存/导出检查有2.1.45门槛；重复类型删除原文cards/notes混用，未自行确定精确对象。
6. [维护者issue4228](https://github.com/ankitects/anki/issues/4228)：原件`issue-4228.html`/提取`issue-4228.txt`，dae 2025-07-25。只作失焦停止的历史机制说明；当前停止菜单/重进不补猜。[23.12发行](https://github.com/ankitects/anki/releases/tag/23.12)HTML日期2023-12-25，另确认布局关闭保存机会，无按钮视觉推断。

## 检查、命令与边界

准确命令/读过文件与web检索见`commands.md`。最终`python3 work/anki-controls-deepening/check.py`退出0，290项PASS；定向`git diff --check`退出0。JSON/CSV引用、E/URL无重号、22原F不增减、正式/原件SHA、LF及旧4批文件散列均核对。没有为文档改动创建产品测试。

异常仅本地两次：`python`不存在exit127；`bs4`缺失exit1，改python3/标准库后成功，未安装。Web读取器两页失败/一页find失败，正常公开MarkdownGET成功；没有401/403/429，无绕过。16个原件GET均HTTP200（不以状态代替正文核读）。初次总体阅读有输出截断，关键新增正文已分开读取；未声称遍读55页。

剩余未知：学习页Undo/Redo实际入口/题面恢复、跨同步/重启栈；自动推进手动停止/重进/音频配置；媒体工具垃圾箱与OS回收站措辞/期限；档案删除回收/降级支持范围；旧日志升级和模板删除恢复。现行手册未给统一修订日或测试构建。按任务保持部分D2，不能声称全产品完成。

## 原件哈希（均位于本批work）

| 文件 | HTTP | SHA256 |
| --- | --- | --- |
| llms.txt | 200 | `37277f3b5505aa8ab28719e28b3ac2bb9217288d8648beb974a93568ba2edbdc` |
| misc.md | 200 | `99a5287b3f03af696710b700e7854b9bbf718acf491cafd989a432f2aeb77c2b` |
| errors.md | 200 | `4f559a6ca6e7109b13fa031201ffa8092ee0072b3d20264630a18d6467766d1c` |
| files.md | 200 | `108e49dc1d26df4623a1fedc6bf1a14ffc065d21b8ca25af148630d59627940f` |
| syncing.md | 200 | `b9f2d7069200c44b339400f839efaeb16a23bd5a97b338020986d1e4ca985c9c` |
| backups.md | 200 | `1f398b3b52d6c53a4e9e8e2ff16134e07c7374f7b8073f9d5fec143f44da3cfb` |
| profiles.md | 200 | `23d6c6434bfde60295c8b874bcb63f007ec61b7be0212df4b4dd5fc964ff1c2f` |
| media.md | 200 | `4151e6313563d44de616562e1d7509a766e788fab606179ef3c381d44332ce5b` |
| deck-options.md | 200 | `2bb7c9f536b4b33a754fa363d75f7625b4001eda260aec92b3f5fe8975a8c2ae` |
| changes-2.1.40-49.md | 200 | `bc0bf8d3db127bd1e0043636026cb311e98ae88b05da960405f9f09aee01222c` |
| changes-23.12-plus.md | 200 | `3fffc9d697229aafe7765105ef3ecda5a43a46bbaac7fc001f4a950f738e107e` |
| missing-media.md | 200 | `460fac601c6a3cdee8e30e167733601c3cc7020d53a9272d3a970ff7d983f093` |
| sync-profiles.md | 200 | `200ff921c1fa4c6ec29ff29a2d7e59c8be84a2a404a06f491f0a5d9470e7f48f` |
| release-23.12.html | 200 | `87a9ddeebaa886ffb3625971bfa6a89118ff47251b9bf33406e27a558f39465e` |
| scheduler-2021.md | 200 | `58ed5992ed17d189c1ab01e4161bb7d6304be9c3650d9002a122dd4aaa9f0abb` |
| issue-4228.html | 200 | `98cc4997904c84fe11c66b53d80cd7471dcbe7eb5b6051a198598b40bf629470` |

原件URL/最终URL/类型/字节数在fetch-log.json、fetch-extra-log.json。检查结果在check-result.json，正式散列在formal-file-hashes.json，旧批起始散列在old-batch-hashes.json。
