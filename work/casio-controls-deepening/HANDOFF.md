# M024 CASIO 本轮交接

查阅2026-10-27；任务已收束，没有待执行的联网、设备或账户动作。只读复用2026-10-26取得的官方中文PDF，没有再次请求网站。没有安装、账户操作、API调用或固件/媒体下载。

## 交付范围与数量

- 新E：E-CACD-01—08，共8条章节/目录定位，**同一PDF，不是8份独立来源**。
- 新F：F-M024-10—16，共7条，全部D2；手写纠错、随记、便笺、内置文学、临时录音对比、保存演讲、异常复位。案例共16F，全部仍为D2。
- 完整PDF书签目录113行，含父子项、前后页与附录；另按178—182页分别登记E-W100的28个收录条目、E-W220的68个，包含工具及两机重复内容，共209行。不是209个独立或已完成功能。
- 旧F02/04/05/08/09逐字段建议共21项，**尚未应用至旧批/主表**：`work/casio-controls-deepening/existing-coverage-updates.json`；同值UTF-8/LF CSV同名。5个任务深度不变，F05新增静态页171观察建议将visual_status改为static_manual_pages_only。

## 正式修改文件（共8个）

1. `docs/research/interaction-library/cases/M-024-casio.md`
2. `docs/research/interaction-library/batches/casio-controls-deepening-evidence.jsonl`
3. `docs/research/interaction-library/batches/casio-controls-deepening-coverage.csv`
4. `docs/research/interaction-library/batches/casio-controls-deepening-inventory.csv`
5. `docs/research/interaction-library/artifacts/casio-controls-deepening/source-notes.md`
6. `docs/research/interaction-library/artifacts/casio-controls-deepening/manual-p87.png`
7. `docs/research/interaction-library/artifacts/casio-controls-deepening/manual-p117.png`
8. `docs/research/interaction-library/artifacts/casio-controls-deepening/manual-p121.png`

主台账、旧批、根文档未改。原件、选页文本、临时渲染、脚本、建议和检查放 `work/casio-controls-deepening/`。正式archived_artifact均为library相对路径；原件用raw_work_path/raw_sha256，完整散列见 `hash-manifest.json`。

## 根抽核入口

唯一手册URL：<https://www.casio.com/content/dam/casio/global/support/manuals/exword/pdf/003-zh-cn/e/E-W_series_full_CN.pdf>。

本轮原件 `work/casio-controls-deepening/E-W_series_full_CN-MA2307-A.pdf`，257页；与上批 `work/dictionary-research/casio-full.pdf`逐字节相同，SHA-256：`e61895383954b9d1ea4f1e7b61c95c4320231c23d446cf445dcf101ea0a0757c`。PDF/印刷页一致。封面为2023年7月MA2307-A；文件属性创建2023-07-19/修改2023-07-24不是固件日期。

优先复核：

- **E-CACD-05/F14，116—117页**：录音方块/输入键结束、2分钟上限、对比顺序、重录、X关闭后自动删录音；这与演讲持久保存不同。
- **E-CACD-06/F15，120—123页**：1—5分钟设置、超规定15秒停、重录/放弃确认、五槽保存/覆盖确认、播放结束回开头，删除细节未展开。
- **E-CACD-07/F16，34—36、228、231页**：复位先关机；未关机可能丢记录/设置，不反推正常复位必保学习簿。设置初始化有用户名/语言/日期时间例外，不等于全机清除。
- **E-CACD-02/旧F05，171页**：已查看原页图，删除表最后两行的文字确实分别是“删除显示中的文件夹的内容”/选中卡本，以及“删除当前文件夹的内容”/全部背诵卡；末行标签与功能范围矛盾不是提取错位。本轮不替CASIO决定实际按钮含义。
- **E-CACD-04/F13，178—182、195—198页**：两机均有世界文学，E-W220另列对照名著；书签规则不套所有内容。目录“世界文学作品500篇”与正文章名“世界文学500作品”如实注明。
- **E-CACD-01/F10，84—87、225、227页**：误识别候选、无正确候选删写、识别成功但字典无资料、手动/自动和触点校准分开；未测试识别时延/精度。

静态观察具体分工在source-notes：实际查看34、87、116、117、121、123、156、161、171、197共10页；仅正式留87/117/121三页，其他七页保留观察定位和work渲染SHA。没有连续操作、实机或D3。

## 命令与退出状态

工作目录均 `/home/workspace/reader`。

- `pwd`、`rg --files` 定位资料，`cat AGENTS.md README.md docs/requirements.md docs/research/interaction-library/template.md`、读取M024/README与旧E/F：exit0；起始 `rg -n 'E-CACD|E-CSCD' docs/research/interaction-library --glob '*.jsonl' --glob '*.csv'` exit1表示无前缀碰撞。
- `PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY' ...` 本地读取PDF、get_toc及分章get_text，创建基线/页文本并渲染九页：exit0。首次使用fitz仅有API弃用警告，后续改用已安装pymupdf；未安装依赖。
- `tools.view_image`逐张查看上述十页，均成功返回图；它无shell exit码。第171页为最后单独渲染并核对表格，渲染命令exit0。
- `python3 work/casio-controls-deepening/write_batch.py`：exit0，最终8E/7F/209目录/21旧字段；复制原件、写新批与短摘记，没有修改旧批。
- `python3 work/casio-controls-deepening/update_case.py`：exit0；更新本案例。
- `python3 work/casio-controls-deepening/check_batch.py`：exit0/PASS；检查E/F/目录编号、引用、本地链接、原件及图片/摘记SHA、UTF-8/LF、JSON与CSV同值、3旧批SHA不变。结果 `validation.json`。
- `git diff --check -- docs/research/interaction-library/cases/M-024-casio.md docs/research/interaction-library/batches/casio-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/casio-controls-deepening-coverage.csv docs/research/interaction-library/batches/casio-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/casio-controls-deepening`：exit0。含未跟踪文件，实际结构检查以check_batch为准。

写入脚本会重写本批，不要在root合并/修订后重跑。仅文档/资料检查，未运行产品测试。

## 仍未知与状态变化

容量已核，满额提示、自动淘汰、未保存退出、撤销/误删、跨设备迁移、外部导出、复位后学习簿/演讲保留仍未知。我的书库读卡失败有重插/换卡处理，但这不证明数据恢复；学习簿不存microSD。通用发音对比不是评分，内置训练“完成”仍不等于掌握。

2026-12-31 17:00停止无线下载条件沿用旧E-CASIO-10/11；既有内置/已存内容不受影响。此轮未联网重验，不据2023手册推断服务永远可用。

只有授权研究文件变动；设备、学习数据、云账户和额度状态均未改变。
