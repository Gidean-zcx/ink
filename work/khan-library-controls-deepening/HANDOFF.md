# M-030 Khan Academy Kids：Library 与帮助目录交接

日期 2026-10-27。本批限 Khan Academy Kids，不迁入 Khan 主站。来源是官方公开帮助；未固定实装移动构建/地区，没有登录、注册、安装、外部消息、账户操作、视频或音频下载/播放。5–6岁可独立读中文不等于具备对应英语水平。

## 结果

- 11 新 E：E-KLCD-01～11；1 个目录来源、10 个新帮助正文。全部 supersedes:null。
- 2 新 F：F-M030-12 背景音乐静音/无声排错；F-M030-13 教师课堂报告查看/导出，均 D2。合并后本例13F，仍无D3或实机。
- 5 个旧 F 共18字段精确建议：F01/F02/F04/F09/F10。原编号复用，未写旧批。
- E-KHK-01 同URL追加完整书籍正文与单张静态图：1个 additional_captures 字段；另7个日期/视觉审计字段建议。其余旧E未动。
- 目录：6栏目、8分页，共112篇文章入口（含重复列示），另18正文功能/控件节点，总130行；不是130个完成任务或完整机内菜单。只读标题、旧正文记录、本轮正文核对分别标识。

## 允许改动的文件

已修改 `docs/research/interaction-library/cases/M-030-khan-kids.md`。

已新增：
- `docs/research/interaction-library/batches/khan-library-controls-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/khan-library-controls-deepening-coverage.csv`
- `docs/research/interaction-library/batches/khan-library-controls-deepening-inventory.csv`
- `docs/research/interaction-library/artifacts/khan-library-controls-deepening/source-notes.md`
- 同一 artifacts 目录 `mute.png`、`look-at-my-map.png`（仅两张实际查看的必要静态图）
- 专属 `work/khan-library-controls-deepening/` 原件、基线、解析摘段、脚本、建议及本交接。

主表、旧批、全局文档和前轮资料未修改。旧 E-KHCD-10 的视觉审计指向其独立旧摘记，散列核对未变；没有旧视觉审计依赖本次修改的 case。

## 根合并入口

- `existing-coverage-updates.json`：18个字段，逐条 source_batch/flow_id/field/before/after。
- `existing-evidence-additional-captures.json`：E-KHK-01追加采集；保留原始记录，正文原返回和静态图都能核验。
- `existing-evidence-field-updates.json`：E-KHK-01 checked_on/source_updated_on/visual_observed/visual_kind/visual_audit_path/visual_audit_sha256/observed_on；新增审计指向独立摘记，非case。
- `case-before.md`、`coverage-before.json`、`evidence-before.json`、`baseline.json`、`case.diff` 保存对照。
- `source-map.json` 对应20个取得结果（8目录页＋12正文）；12正文中书籍复用旧E，Sandy乐器/昆虫页只读名称介绍，不生成操作E/F。

## 关键原文抽核

1. [删除档案](https://khankids.zendesk.com/hc/en-us/articles/360006763332-How-do-I-delete-a-user)，E-KLCD-06，帮助更新2026-02-27。`text/articles-360006763332.txt` 的web L25–30：成人区 My Account 下 Enable Delete→头像X；删除该档案进度和Learning Path历史。未写作品删除、父账户注销或可恢复保证。
2. [教师报告](https://khankids.zendesk.com/hc/en-us/articles/4403614100109-Progress-reports-in-the-Khan-Academy-Kids-app)，E-KLCD-10，2026-02-23。`text/articles-4403614100109.txt` L37–65为入口、两种报告和多次尝试；L67–74为Email、To、ZIP内HTML和设备邮件配置条件。未发送；不能套家庭账号或当回灌备份。
3. [背景音乐](https://khankids.zendesk.com/hc/en-us/articles/360006764152-How-do-I-mute-the-background-music)，E-KLCD-03，2026-02-26，L23及附件：只确认背景音乐静音。E-KLCD-08的音量建议是打开App时检查设备音量键，不是App内部音量菜单；其部分iOS排错步骤有旧硬件/系统条件。
4. [中文家长指南](https://khankids.zendesk.com/hc/en-us/articles/4457349308059-Chinese-Parent-Guide-%E5%AE%B6%E9%95%BF%E6%8C%87%E5%8D%97-%E5%BC%80%E5%A7%8B%E4%BD%BF%E7%94%A8%E5%8F%AF%E6%B1%97%E5%AD%A6%E9%99%A2%E5%84%BF%E7%AB%A5%E7%89%88)，E-KLCD-07，2026-02-23，L23称英文版、L27说明账号与进度保留、L39称点击Kodi重复活动说明。保留与旧英文“下一句”的措辞差异，不认定上一句回放，也不把中文帮助译文当中文课程。目录的西语书籍正文未读。
5. [Books](https://khankids.zendesk.com/hc/en-us/articles/4409036780955-Learn-more-about-the-books-in-the-Khan-Academy-Kids-app)，旧E-KHK-01新采集，2026-02-26，L30–43说明Books/Reading入口差别，L81仅说字母书部分图片可移动；未补未知错误反馈。E-KLCD-11“先听后自行读”属于成人练习安排，不是同页即时切换或自动流利度评分。
6. E-KLCD-01完整可见栏目取自前轮成功公开JSON与本轮8页web正文；原JSON `work/khan-controls-deepening/12-sections.json` SHA `56baffa85bdbda0a375fbbb98f481f82b89ae7e5cab17fb707fc3e1432b74131`，没有重新调用其接口或账户接口。

## 原件、静态观察与散列

web工具返回保存为JSON字符串，非直接下载HTML：
- `web-index-response.json` SHA `578274fc308b46b84580e19e4fa67096c4d7125fbcd792594d8e0702f576844b`
- `web-response-1.json` SHA `26c175bd1d031892ead417c122f7b1df700dcd053b0a8175dc4b9b2f4e83c838`
- `web-response-2.json` SHA `83aa5d229b599120b14a7a3e705511a25ac589ef9ad4822f3d8f2a69397c4c2e`

正式摘记SHA `dc0bf2de45ba29857a4c303a9fc1d65fdaba624a6a68bb57efa9e799e700d1a6`；case SHA `e1d69158992a32a2c3caae1b0481e92c5a1b3ba45904848fcaabc038dd50ecb8`。

本轮用 view_image 实际看两张：
- `mute.png`，普通GET附件4420861003803，HTTP200；SHA `cfddb5c7187007ebe5f2a6f5f87083f9d6ac6fffddf0a3cfa29fb0f7e5c8de6a`。Home、Library、角色、右下划线音符和官方蓝箭头；非点击前后对照。
- `look-at-my-map.png`，普通GET附件4409150302747，HTTP200；SHA `63852e61859312f3250de1b3dc98dc221435ef05d8c2ccc3f77578dcbc82ea0d`。单页Home/左右箭头/底部三角按钮，模式与三角按钮作用未知；未听音/翻页。

两图原件work与正式artifacts散列相同；图片未编辑。本轮只有这两张新观察，未重看旧GIF。姓名帮助附件360007362331在web工具超时后停止，未查看。旧10个HTTP403页面和旧404/视频不可读路线均未重试。

## 实际命令、失败与合并前检查

工作目录 `/home/workspace/reader`。先读 AGENTS/README/requirements及ideas相关段，当前case、18E/11F和旧fetch-results/目录原件。`rg`/Python只读解析均保存范围；初始组合命令最终exit1是E-KLCD/E-KLBD/E-KLCC前缀检索无匹配，工作目录不存在的test成功，不是取证失败。

实际关键命令：

```sh
python3 work/khan-library-controls-deepening/capture.py mute.png 'https://khankids.zendesk.com/hc/article_attachments/4420861003803'
python3 work/khan-library-controls-deepening/capture.py look-at-my-map.png 'https://khankids.zendesk.com/hc/article_attachments/4409150302747'
python3 work/khan-library-controls-deepening/build.py
python3 work/khan-library-controls-deepening/update_case.py
python3 work/khan-library-controls-deepening/check.py
sha256sum docs/research/interaction-library/cases/M-030-khan-kids.md docs/research/interaction-library/artifacts/khan-library-controls-deepening/source-notes.md work/khan-library-controls-deepening/web-index-response.json work/khan-library-controls-deepening/web-response-1.json work/khan-library-controls-deepening/web-response-2.json
```

两次capture及build/update_case均exit0。check首轮exit1：检查器查找字面“上一句”，正文实际保留“重播上句/两种措辞并列”；修正仅检查器的定位后exit0/537PASS。之后明确音量键措辞和来源平台范围，再运行检查仍exit0/537PASS。web open/click为工具读取，无shell退出码；原工具返回及失败见四份web-response文件。没有重跑任何旧批构建/检查。

冻结操作由本批freeze.py生成manifest，再执行同一check（增加逐冻结文件SHA检查），最终输出见 `check-output.json`。合并前根可复现：

```sh
python3 work/khan-library-controls-deepening/check.py
```

检查旧F/E before及原批一致、全部新源/图片/审计SHA、来源ID/URL无重复、目录与任务映射、CSV LF/标准表头、案例链接和13F范围。**合并后不再运行before检查或build/update_case；冻结后不再写正式文件和建议。**

## 未决与状态

Learning Path错误次数和降级阈值、示范后谁答、提示对评分/奖励的作用、课内保存与后台/断网差别仍未核；书籍即时读听切换/暂停/续接、删除档案撤销和作品命运、报告发送失败及回导未知。教师控制ON/OFF旧文档冲突完整保留。本轮没有把列表数量或更多D2当全产品完成。

只改变本案例和本批文档/原件/建议文件，外部账户、产品数据和系统设置无变化。最终产品判断与合并由根完成。
