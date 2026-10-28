# M022：公开帮助分支与型号边界

查阅2026-10-27。未使用设备、安装、登录、执行网页脚本或账户/学习接口；无新图像/音视频观察。

## E-BDCQ-01：dcj2 是何种帮助

此前已存官方`https://www.baicizhan.com/qa`入口HTML及其脚本`https://7n.bczcdn.com/baicizhan/h5/fe_qa/assets/index-CvNBM2bC.js`。本次仅本地复读；脚本字符偏移226396起的`sT()`把`deviceVersion=2.0`选择为`dcj2`，`ES.fetchQAList`以GET读取`/feedback_api/get_qa_info`。该参数不等于已核设备固件2.0；代码也没有Mini、Max或BCZ-Ace型号对应表。另一sxj分支未取，未猜测其他platform值。

按已存在的公开分支匿名GET：`https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=dcj2`，HTTP200。原件`work/baicizhan-device-controls-deepening-2/qa-dcj2.json`，SHA256 `774b63ea53111bdf9366af03e54563612949813c8266dd7d11da6f1a510c9bd9`。

`data.question_categories`含5类23问，全部文字已读，hot_questions为空、banner为空。正文自称超级单词机，未标SKU/修订日/build；Q10146/48/49涉及主机、手柄和扫描笔，Q10158—62讲投屏。不能把这一资料当Mini或3.52英寸Max手册。

防混用定位：Q10153离线打卡回传；Q10163设备收藏与App收藏范围；Q10166句子不同步；Q10144退出登录式解绑和会员账户限制；Q10145图背离线/小学模式限制；Q10148扫描笔圆键1秒。这里只保留范围和定位，不将这些步骤、数值或状态用于Mini/Max的F。23问逐项目录列为excluded_model_scope，不表示已覆盖23个目标功能。

## 旧E复读与目录覆盖

E-BCZH-01：复读`work/baicizhan-device-controls-deepening/01-official-home.html`及派生txt；Mini、3.52英寸Max和系列离线/词书/发音仍是宣传。22个href含重复，未给手册/帮助入口；不能据此证明售后或App内没有手册。

E-BDCD-01：复读同目录`02-52audio-ace-max.html`及txt68—88行；作者Wen5/2025-05-23，明确BCZ-Ace-Max。文字列键名、音量键和复位孔；孔或键存在不证明恢复操作、数据清除范围或退出位置。未重新请求此前403图片，不新增视觉事实。

Mini/Max目录17项分别标明旧静态记录、正文宣传、独立作者描述或既有历史证据记录。历史计划/同步教程仍无型号，关于页同步仍只沿用既有E记录；本轮没有其新原件，不伪称重新取得。完整型号化操作手册仍缺，3条原F均D1，0新增F。
