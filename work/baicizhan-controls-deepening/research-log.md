# 本轮取得方式与命令记录

2026-10-27；公开文档研究。未安装、登录、执行App学习、发送消息/反馈、执行前端脚本或下载音视频。正文JSON是官方帮助前端普通GET的公开内容，不是账户/学习接口。

## 读取与基线

已读AGENTS.md、README.md前45行、requirements.md前80行、ideas.md学习相关命中、M016完整案例、6条旧F和16条旧E、`work/chinese-vocab-deepening/research-log.md`、`work/baicizhan-device-controls-deepening/research-log.md`、旧独立静态审计。使用`cat`、`sed -n`、`rg`和内联Python JSON/CSV筛选，exit0。基线3份及baseline.json在本目录；没有视觉审计依赖案例。旧审计5图未重看。

前缀检索`rg -l 'E-BCCT|E-BCCD' docs/research/interaction-library/evidence.jsonl docs/research/interaction-library/batches/*-evidence.jsonl`无命中exit1，这是未占用而非读取失败。一次对旧work HTML的宽rg输出截断，未据截断内容新增事实；随后按本轮派生txt逐页读。未重试旧403/不可访问路径。

## 搜索范围

web检索为官方域恢复已斩、答错复习、帮助离线、斩词、误斩、学习设置、复习提示、帮助与反馈，以及“百词斩”帮助中心/斩护卫/官方撤销。前3条为空；后续找到已有历史公告与官方/qa。第三方聚合教程不采用。`web.open https://apps.apple.com/cn/app/id557545298`返回工具内部不可访问（无HTTP状态）；后续使用旧E已有完整URL普通GET200，不是绕过401/403。

## 网络取得：全部HTTP200/命令exit0

实际依次执行：

```bash
python3 work/baicizhan-controls-deepening/capture.py qa.html https://www.baicizhan.com/qa
python3 work/baicizhan-controls-deepening/capture.py app-store.html 'https://apps.apple.com/cn/app/百词斩-背单词-学英语日语韩语法语/id557545298?platform=watch'
python3 work/baicizhan-controls-deepening/capture.py privacy.html https://passport.baicizhan.com/privacy/privacy.html
python3 work/baicizhan-controls-deepening/capture.py introductions.html https://passport.baicizhan.com/privacy/introductions.html
python3 work/baicizhan-controls-deepening/capture.py qa-index.js https://7n.bczcdn.com/baicizhan/h5/fe_qa/assets/index-CvNBM2bC.js
python3 work/baicizhan-controls-deepening/capture.py qa-bczApp.json 'https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=bczApp'
python3 work/baicizhan-controls-deepening/capture.py qa-rhyme-mode.jpg https://vol.bczcdn.com/adv/8bc113e183b9952c6ac0ce8329cb266f.jpg
```

7个`.capture.json`保存URL/final_url/HTTP/bytes/SHA。JS静态读取定位fetchQAList，未执行。内联Python将JSON按类别展开qa-text.txt、pretty.json及商店TitledParagraph版本条目；exit0。Q49实际view_image，原1125×3346、展示689×2048；未看其他QA图。

正文核查包括：`cat qa.html`；`sed -n '1,185p' app-store.txt`与`183,280p`；`sed -n '322,354p' privacy.txt`；`nl -ba qa-text.txt | sed -n '1,82p'`与`130,222p`；全部qa-text.txt读入；`cat bcz-info-reused.txt bcz-reused.txt bcz-925-reused.txt`。上述路径均在本work目录，exit0。旧3原件是本地复读，不记新GET。

## 整理 / 验证

`python3 work/baicizhan-controls-deepening/build.py`第一次exit1：原JSON含空answer块，二元解包失败；只写出本轮部分新文件，尚未改case。过滤空块后同命令exit0；产出2E/2F/55目录行，4旧F共17字段建议，6旧E追加采集。随后使用apply_patch补回案例的既有历史路径与边界，未重新构建旧批。

`diff -u work/baicizhan-controls-deepening/case-before.md docs/research/interaction-library/cases/M-016-baicizhan.md` exit1表示已预期的差异。一次`rg -n ... scripts/check_docs.py | head -n 40`因不存在该路径，rg exit2/管道exit0；没有运行该脚本。后续只使用本轮check.py。没有重跑其他产品脚本。

最终预检命令与输出见HANDOFF和check-result.json；检查不联网、不改正式资料或旧台账。正式改动仅M016 case、本轮3批次文件、短摘记/独立视觉审计/1图。主表、全局文档和旧批未改。
