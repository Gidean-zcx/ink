from pathlib import Path
import difflib
R=Path('/home/workspace/reader');W=R/'work/khan-library-controls-deepening';L=R/'docs/research/interaction-library';p=L/'cases/M-030-khan-kids.md';s=(W/'case-before.md').read_text()
s=s.replace('**全部任务仍为D2文档重建；补看官方动画抽样静态帧，没有操作App或听到声音。**','**全部任务仍为D2文档重建；前轮观察官方动画抽样帧，本轮另看两张官方静态图，没有操作App或听到声音。**')
s=s.replace('主页另有Learning Path、角色房间和成人入口。','主页另有Learning Path、角色房间和成人入口。\n\n本轮[帮助目录与功能映射](../batches/khan-library-controls-deepening-inventory.csv)覆盖公开六栏目及全部可见分页：家长35条、教师35条、学区/IT 7条、多语言15条、公告11条、夏令营9条，共112条文章入口，包含重复列示；另列18个正文支持的功能/控件节点。索引标题、前轮正文记录、本轮正文核读分开，130行不表示130个完成任务，也不包含未列出的隐藏帮助或完整机内设置。栏目由E-KLCD-01支持。')
s=s.replace('同名书从Reading进入可能成为带问题的课程；模式中途切换、同页续接与逐字点读未核。','Reading可加入理解问题、点图，1–2年级还可能有选择题。Reading年级菜单可找Sight Words；Books的Early Readers可按成人建议先听后自主读。不是已证实的同页切换、自动复习或朗读评分；错答反馈及逐字点读仍未知。')
s=s.replace('| E-KHK-01；E-KHCD-03 |','| E-KHK-01；E-KHCD-03；E-KLCD-11 |',1)
s=s.replace('主页播放进入交替安排的活动；家长入口选择档案和学习级别。Library的彩色勾号表示掌握、进行中、发展中。','主页播放进入轮换活动；年级决定各主题起点，之后按表现调整，困难时可给基础活动及更多支持。家长可改档案级别；Library彩色勾号表示产品定义的进度标签。')
s=s.replace('无勾号不能直接判定数据已删。“掌握”是产品标签，没有家庭学习测评。','无勾号不能直接判定数据已删；官方未给错误次数、提示使用和调级阈值。“掌握”仍是产品标签，没有家庭学习测评；教师报告单列F-M030-13。')
s=s.replace('| E-KHK-02、03；E-KHCD-03 |','| E-KHK-02、03；E-KHCD-03；E-KLCD-02、07 |',1)
s=s.replace('可点Kodi听她接下来要说的话；设计文档描述思考手势、温和错误音、鼓励再试及分步动画示范。','旧英文指南说点Kodi听下一句话，中文指南称重复活动说明；两种措辞并列。设计文档描述思考手势、温和错误音、鼓励再试及分步动画示范。')
s=s.replace('点Kodi不等于已证实重播上句。','中文译文也不足以证明逐句重播上句或固定提示队列。')
s=s.replace('| E-KHD-01、03；E-KHCD-10 |','| E-KHD-01、03；E-KHCD-10；E-KLCD-07 |',1)
s=s.replace('F-M030-09 在家长账户下增加孩子','F-M030-09 新增、编辑和删除儿童档案')
s=s.replace('开始页New，或主页右上头像→成人区→名字、年龄→动物头像→Next启用新档案。','新增沿用New/头像→成人区→名字、年龄、头像→Next。编辑由For Parents或主页头像进入成人区，在My Account选档案改名字/头像。删除先在邮箱下启用Enable Delete，再点目标头像X。')
s=s.replace('不等于另建家长账户。选错已有成员如何撤回、档案删除、跨设备冲突、进度/作品独立范围尚未核实；新增不能当作找回旧进度的步骤。','删除帮助明确该用户进度和Learning Path历史会被清除，不是注销整个家长账号。改名/头像的保存回执、删除再确认/撤销、作品去留和跨端时序未给出；新增不能当找回进度。')
s=s.replace('| E-KHCD-07；E-KHK-02 |','| E-KHCD-07；E-KHK-02；E-KLCD-04～06 |')
s=s.replace('同步完成、退出时未上传数据、其他登录方式和过期链接未实测。','中文指南将邮件账号与重装/换机保留进度相联系，但没有同步完成回执。独立重装页使用旧式iOS删除菜单，Android只写到卸载；不能据此承诺未上传数据或作品可恢复。过期链接和其他登录方式仍未知。')
s=s.replace('| E-KHCD-08、09 |','| E-KHCD-08、09；E-KLCD-07、09 |')
insert='''## 背景音乐与课堂报告：两个独立任务

| 任务 | 入口、动作与文档预期状态 | 范围与缺口 | 证据 |
| --- | --- | --- | --- |
| F-M030-12 背景音乐静音与无声排错 | Home右下灰色音符→点击后图标划线，正文指背景音乐静音。无声帮助另要求排查设备静音、打开App时检查设备音量键及蓝牙/外接音频路由。 | 不证明朗读和提示音也关闭；恢复音乐的具体动作、跨档案/重启持久化未知。排错页含侧键及旧iOS菜单，不能套所有现行设备；未做声音测试。 | E-KLCD-03、08；D2，单张静态图 |
| F-M030-13 查看并导出课堂进度报告 | 教师视图Students→Class Reports→切Assignments或All Progress。前者点勾号看历次尝试，后者点百分比看历次尝试；后者汇集已完成的作业、Library及Learning Path课程。右上Email→填写To，文档预期收件邮件附ZIP，解压得浏览器可读HTML。 | 设备须配置邮件地址作为From；不是家庭版勾号界面，也不是可回导备份。正文未给完整发送确认、取消/失败提示、附件访问控制、离线同步冲突及分数算法。没有发送或操作学生记录。 | E-KLCD-10；D2，文字 |

教师报告还允许从课程标题/缩略图查看或编辑作业，低分Main课程可再分配Basic课程；这是教师决定，不证明系统把看提示后的答对算作独立掌握。家庭档案、课堂名册、报告附件和Gallery作品是不同数据对象。

对本家庭5–6岁儿童，能独立读中文不能推出能独立读英语。E-KLCD-07中文家长指南明确称App为英语版；多语言帮助目录中的“中文指南”只是说明译文。目录另列西语书籍但本轮未核其正文，不据此断言所有内容永远只有英语。Letters、Math、Reading、Logic+子目录是课程类别，未形成每种题型的完整错误→提示→重试链。

'''
s=s.replace('## 静态画面与本轮取证',insert+'## 静态画面与取证范围')
s=s.replace('10个帮助页的普通HTTP直取均返回403后停止；本轮帮助正文依据web工具成功读取的公开页面，原返回文本留work。','前轮10个帮助页的普通HTTP直取均返回403后停止；本轮没有重试这些HTTP路线。新增帮助正文依据web工具正常读取的公开页面，原返回文本留work。')
s=s.replace('section目录未展开为全功能目录。','本轮把公开六栏目及家长/教师第二页展开为可见索引，仍不是全功能操作目录。')
s=s.replace('## 后续必须补的观察','本轮另实际查看两张PNG：背景音乐页图显示主页右下划线音符和官方指示箭头；Books的单页书图显示Home、左右箭头及底部三角按钮，未标阅读模式，不能推断按钮作用或返回后的状态。改名页图片打开超时后停止，未观察。两图与前轮GIF离散帧分别记录，见[本轮独立摘记](../artifacts/khan-library-controls-deepening/source-notes.md)。\n\n## 后续必须补的观察')
s += '\n本轮新增E-KLCD-01～11的URL、日期、工具返回原件及SHA见[来源批次](../batches/khan-library-controls-deepening-evidence.jsonl)；Books正文与单张图沿用E-KHK-01追加采集。旧来源和前轮视觉审计均保留，来源更新时间不替代App构建号。\n'
p.write_text(s)
(W/'case.diff').write_text(''.join(difflib.unified_diff((W/'case-before.md').read_text().splitlines(True),s.splitlines(True),fromfile='case-before.md',tofile='cases/M-030-khan-kids.md')))
print('case updated; bytes',p.stat().st_size)
