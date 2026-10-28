# 第二十六期：声音、档案与可复用的交互依据

2026-10-27。延续用户对研究深度的要求：此前的功能盘点不足以支撑设计；目前也没有做到每个产品、每个功能的完整交互验证。本期补Kindle Scribe、Mudita Kompakt和Khan Academy Kids，均为部分任务的文档重建及静态图观察，没有新增实机或App操作。

## 本轮具体深入到哪里

**听书至少有三种不同任务。** Scribe的Audible是有声书，Assistive Reader是TTS，VoiceView是读屏导航。下载到可以开始播放不等于整书离线可用；TTS打开导航或做批注会暂停，开关持续启用并不证明关菜单后自动播放。蓝牙断连、睡眠及AI插话后的恢复仍缺实际结果。[辅助朗读官方说明](https://digprjsurvey.amazon.co.uk/csad/help/node/TdHCSXXqsdhaVKt3Tc)；详细入口、下载取消、书签选择即播放和未知分支见[Kindle F11–14](../research/interaction-library/cases/M-001-kindle.md)。

**同名按钮不能概括成同一个动作。** Kompakt录制中的第一次Done是停止，命名后的第二次Done才保存；回放页Done只被说明为关闭画面，没有证据证明它停止播放。Scribe便签X保存，AI生成预览X却需要确认放弃。对本项目AI能力的启发是：必须知道当前对象与实际结果，不能靠按钮名称猜执行成功。这是研究建议，尚未确定接口设计。[录制说明](https://support.mudita.com/en/support/solutions/articles/77000596747-how-to-record-a-voice-recording-)、[回放说明](https://support.mudita.com/en/support/solutions/articles/77000596769-how-to-listen-to-a-voice-recording)、[AI笔记说明](https://digprjsurvey.amazon.com/csad/help/node/TZNl60K13ehpn0s1O0)；见[Kompakt F14](../research/interaction-library/cases/M-012-mudita.md)。

**家庭共享、儿童档案和家长限制不能合并。** Scribe指南介绍家庭及儿童建档，而英国帮助明确排除Scribe儿童档案，仅给家长PIN限制；保留来源冲突，没有补造成功路径。Khan Kids删除孩子档案会删除其进度和学习路线历史；教师报告则能查看多次尝试，邮件导出的HTML报告没有被证明能恢复学习数据。[Scribe限制](https://digprjsurvey.amazon.co.uk/csad/help/node/G27XUUGLQ5TG84XC)、[Khan删档案](https://khankids.zendesk.com/hc/en-us/articles/360006763332-How-do-I-delete-a-user)、[教师报告](https://khankids.zendesk.com/hc/en-us/articles/4403614100109-Progress-reports-in-the-Khan-Academy-Kids-app)；见[Khan F09/10/13](../research/interaction-library/cases/M-030-khan-kids.md)。

**不能因“有中文家长指南”就当适合孩子独立使用。** Khan中文指南描述的是英语应用；本家庭儿童能读中文，不能据此推定英语能力。背景音乐的划线音符也只证明音乐静音，不能推成关闭朗读和指导音。逐题错答、提示与奖励仍有缺口，不能由目录里的Math、Reading标签代替游戏策划分析。[中文家长指南](https://khankids.zendesk.com/hc/en-us/articles/4457349308059-Chinese-Parent-Guide-%E5%AE%B6%E9%95%BF%E6%8C%87%E5%8D%97-%E5%BC%80%E5%A7%8B%E4%BD%BF%E7%94%A8%E5%8F%AF%E6%B1%97%E5%AD%A6%E9%99%A2%E5%84%BF%E7%AB%A5%E7%89%88)。

## 以后怎样取用

新增[设计参考使用入口](../research/interaction-library/design-reference-guide.md)，按“我要设计什么”查资料，不要求按简报的时间顺序读完。横向问题指向具体产品任务F；F指向证据E；E记录原文位置、版本、取得方式、必要图像和缺口。正式设计再记录采用或放弃的理由。研究事实与本项目建议保持分开。

本批新增19条局部D2任务、39条证据记录，续补17条既有任务；看过13幅官方静态图。Kindle原58目录项回写阅读状态；Kompakt原111项补读18篇，仍46项只有目录；Khan六栏目/八分页112文章入口含重复，另列18个正文支持的功能或控件节点。目录行、图片、来源和任务不是同一种数量，均不能当作完成率。全部65组仍部分覆盖，准确总量见[案例索引](../research/interaction-library/case-index.md)。

下一种能显著提高置信度的材料是固定版本下的连续过程：同一本书读听切换及中断重进，同一道题故意答错后求助/重试/退出，录音保存前退出和删除后的恢复。公开材料没交代的行为继续留空，不用经验补写；当前未进入全系统设计。
