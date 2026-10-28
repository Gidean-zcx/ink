# Gemini：笔记、待办与撤销的作用范围

查阅2026-10-27。只读两篇 Android 英文官方帮助的正文；通过网页工具取得并保存文本，没有下载 HTML、查看图像或登录操作。原件标识及失败记录见[source-audit.json](source-audit.json)。安装构建、灰度、写入时延未知。

| 证据与定位 | 可支持的细节 | 不能据此得出的结论 |
| --- | --- | --- |
| E-GENR-01，[笔记帮助](https://support.google.com/gemini/answer/15230597?co=GENIE.Platform%3DAndroid&hl=en)，What you need / Connect / Tips | 登录并开启活动；Keep另须同账号Workspace连接。多个笔记应用默认最近访问者，@可指定。OEM连接有机型、安装和语言条件。 | 不能按品牌推定所有设备可用；共享入口不表示全部动作相同。 |
| 同源，What Gemini Apps can’t do / Create / Add item / Get content | 从对话生成笔记、向Keep既有列表追加、按主题检索。不能分享、查看绘图或添加图片；Samsung/Xiaomi另有修改/删除限制，他人拥有的共享笔记不能删除。 | 未核同名消歧、重复请求、批量部分失败及文件同步。笔记可查不表示全部附件可读。 |
| 同源，Add item末尾Tip | 通知提供Undo，或通过回复链接回列表编辑；Live后台创建的笔记不能由Gemini撤销。 | 通知是操作后的反馈，未证明存在写前确认；撤销时限、跨进程恢复未知。 |
| E-GENR-02，[任务与提醒帮助](https://support.google.com/gemini/answer/15230285?co=GENIE.Platform%3DAndroid&hl=en)，Connect / Add / Show or update | Google Tasks与Samsung Reminder；连接、最近应用/@指定、由前文或图片建待办；可查询、改名/日期、标完成或按主题删除。 | 这里的待办对象不是后台代理工作流；通知最终触达、时间解析与删除确认未知。 |
| 同源，Add末尾Tips | 通知Undo或打开链接编辑；Live后台动作不可在Gemini撤销。 | 文档没有证明取消使全部副作用回滚，也没有实际对象回读。 |

笔记页Tips出现“calendar events”措辞，与页题不一致；不据此给日历补证。日历帮助本次返回429后停止，Workspace正文未取得；先前搜索中发现的Tasks工作流/Skills/Schedules仅为待查线索，没有纳入已读或D2。本次不重试访问失败页面。

原85连接器目录只校正具体已读条目的状态：早前Phone/Messages/Device assistance已读不能继续写未读；本次读过的OEM笔记帮助与Reminder作局部更新。Workspace只读到笔记/待办子帮助，不宣称其完整帮助已读。
