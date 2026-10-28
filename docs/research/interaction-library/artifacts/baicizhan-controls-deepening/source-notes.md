# M016 官方帮助、版本和数据边界短摘记

查阅2026-10-27；所有操作均为公开材料描述，没有安装或运行App。本轮仅查看Q49教学图，见独立visual-audit.md；其余配图只登记URL。

## 新帮助证据 E-BCCD-01 / 图片 E-BCCD-02

官方入口https://www.baicizhan.com/qa 的HTML只有帮助标题和前端资源引用。静态读取所引用的`https://7n.bczcdn.com/baicizhan/h5/fe_qa/assets/index-CvNBM2bC.js`：`ES.fetchQAList`调用`assistant.baicizhan.com/feedback_api/get_qa_info`，`sT()`选择非设备分支`bczApp`。仅普通匿名GET该地址并带`platform=bczApp`，未执行JS、统计POST、反馈或账户接口。入口、脚本和JSON均HTTP200。

原件`work/baicizhan-controls-deepening/qa-bczApp.json`的`data.question_categories`有5类50问；`hot_questions`8项为重复入口，不另计。49问有文字，Q49仅图；无发布日期、修订号或逐题OS/build。逐项定位用分类索引与question.id，台账包含5父项，未声称完成整个App。爱阅读11问保留栏目边界，积分不当主App铜板。

关键定位：Q11为已斩列表中选词恢复，不是最近一次作答的即时撤销；Q20区分登录身份、网络未读全和离线未上传前清缓存；Q51把完成计划与上传并列，10秒停留是传输建议，无确认回执。Q10的铜板补卡只作用于学习日历。Q22给收藏词的PDF出口，未给整本任意词表导出。具体动作见案例，缺口不按常识补全。

Q38/21给资源下载与缺失时联网补资源；不证明所有题型离线。Q12题型开关无构建；Q18否定导入词表的措辞与功能介绍“自定义上传”冲突。Q49实看一图，模式切回与书名范围仅限该图，不能证明当前构建；图中日期不作发布日期。

## 旧E追加原件 / 版本冲突

- E-BCZ-01：同URL新GET功能介绍；4段均是能力声明，不能补按钮流程。
- E-BCZ-02：复读前轮`work/chinese-vocab-deepening/bcz.html`，不是本轮GET；深度模式声明与2023公告不同。
- E-BCZ-03：同URL新GET中国区App Store。正文iPhone/iPad、最低OS13.0，版本7.10.25日期2026-10-19；TitledParagraph的版本/日期字段已提取到`app-store-version-index.json`。不采用评论、截图或watch参数推导平台。7.10.12的记忆曲线描述没有调度公式；7.10.25只有一般改进说明。
- E-BCZ-04：复读前轮`bcz-info.html`明示3.8.3的同步及两类离线范围，未新GET。
- E-BCZ-06：同URL新GET政策，更新/生效2026-09-04。派生`privacy.txt`322—354行：没有后台完整记录的集中查看入口；可按账号/类别/时段申请，经核验后15工作日内处理或说明。另有独立删除请求及保留例外，不能套Q20205“需注销”。经营者安全备份不等于用户可导回学习记录。
- E-BCD-07：复读2023-03-23原件`bcz-925.html`，文称拼错词会再现；没有错答提示、再答按钮或终点反馈。

上述原件和脚本/目录SHA见本轮manifest及追加采集建议。未将旧原件复读写成新网络采集；所有来源只支持其日期/构建范围。主学习链保持D1。
