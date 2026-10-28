from pathlib import Path

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
title = '第五十四期：媒体、权限与学习资料的交付'
brief = '054-media-permissions-and-study-content.md'
summary = 'Quizlet补媒体、组织、权限、成员与评价；原64篇目录文章均有正文记录，首页推荐仍D1，新增13条设计参考。'

for p in [R / 'README.md', L / 'README.md']:
    lines = p.read_text().splitlines()
    i = next(i for i, line in enumerate(lines) if line.startswith('- [第五十三期'))
    lines[i] = f'- [{title}](/home/workspace/reader/docs/briefs/{brief})：{summary}'
    p.write_text('\n'.join(lines) + '\n')

p = R / 'docs/briefs/README.md'
s = p.read_text()
assert title not in s
s = s.replace('- [第五十三期', f'- [{title}](/home/workspace/reader/docs/briefs/{brief})：{summary}\n- [第五十三期', 1)
p.write_text(s)

p = L / 'design-reference-guide.md'
s = p.read_text()
anchor = '| AI会话被最小化、离开后，内容是否还在 |'
new = '''| 媒体已回放或图点已出现，整套学习资料是否已保存 | [Quizlet F24/25](cases/M-021-quizlet.md) | [媒体与图示](artifacts/quizlet-media-and-sharing/source-notes.md)；RP-M021-18～20 | 按平台区分添加/学习；局部保存、整集提交、替换撤销和重载恢复分别核对 |
| 内容选中、加入文件夹和打标签分别改变什么 | [Quizlet F26](cases/M-021-quizlet.md) | [材料归集](artifacts/quizlet-media-and-sharing/source-notes.md)；RP-M021-21 | 高亮不证明提交；类似子文件夹不证明嵌套，删夹/移除与混合权限未知 |
| 发出链接、设置密码或嵌入页面后，对方能做什么 | [Quizlet F04/27](cases/M-021-quizlet.md) | [分享、查看、编辑与嵌入](artifacts/quizlet-media-and-sharing/source-notes.md)；RP-M021-22/23/30 | 文档入口有别；接收端身份、撤权后旧副本、嵌入进度和真实访问结果未验证 |
| 申请、接受邀请、退出和重新加入会怎样影响进度 | [Quizlet F21/29/30](cases/M-021-quizlet.md) | [班级与Study Groups](artifacts/quizlet-media-and-sharing/source-notes.md)、[Study with friends](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-24～26 | 两类群组路径分别引用；申请不证明批准，重加不证明旧进度恢复 |
| 搜索、评分与推荐分别能证明材料的什么价值 | [Quizlet F23/28/31/32](cases/M-021-quizlet.md) | [发现内容与评价](artifacts/quizlet-media-and-sharing/source-notes.md)、[个人集查找](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-27～29 | 评分/称号不证明准确性；首页推荐主链D1，学校偏好保存不证明推荐已改变或可控 |
'''
assert anchor in s and 'RP-M021-18～20' not in s
p.write_text(s.replace(anchor, new + anchor, 1))

p = L / 'backlog.md'
s = p.read_text()
old = '| Quizlet | 68目录节点；Write/Spell结束/提示、普通Test、AI Practice Test和Progress文档 | 当前完整同题流程、试卷题干答案校订、Test/Practice Test各自恢复、Study path Android分歧、删集联动及未读模式 |'
new = '| Quizlet | 原4分组64篇均有正文记录，另两篇关联材料；32任务为31D2/1D1 | 当前版本同题流程、媒体/题目校订与恢复、Study path Android分歧、权限变化/成员退出的后果，以及首页推荐主链；其他帮助类目尚未穷尽 |'
assert old in s
p.write_text(s.replace(old, new, 1))

extra = {
    L / 'analysis-status.md': f'第54批Quizlet补原目录剩余21篇及两篇关联文章，共23新E；新增8D2/1D1、补2旧F与3旧目录采集、13RP。原目录64篇均有正文关联，含两篇背景说明；原4分组+64文章另加2关联文章为70行，不是70项功能或完整产品覆盖。当前32任务为31D2/1D1，均无实际操作。其余类目、当前构建和异常恢复仍有缺口；65组全部部分覆盖，见[简报54](../../briefs/{brief})。',
    L / 'backlog.md': '第54批Quizlet：原登记64篇均有正文关联，后续不要再把媒体、图示、文件夹/标签、分享/密码/编辑权限、班级、评分及公开搜索列作仅目录。新任务仍缺媒体取消/替换/重录撤销与整集提交，图替换后的点位映射，删夹/标签迁移，撤权/改密码对旧会话和副本的后果，班级拒绝/撤回及重加后进度，群组满员/断线/离组数据，评分聚合与纠错。移动首页主链保持D1，缺入门问答、活动开始/结束、推荐理由/控制与返回位置；学校设置有地区范围，不由首页全球可用推导。其他Account/Billing/Teaching/Schools等类目仍未穷尽。',
    L / 'batches/README.md': f'第54批 `quizlet-media-and-sharing` 新增23E、8D2/1D1，补2旧F及E-QZCD-01/02/04采集；原目录21条更新正文，另2条linked_article指向原文章行11/28，共70行，原目录64篇数量不变。新增13RP；背景说明不增任务，全部无新视觉/实际操作。见[简报54](../../../briefs/{brief})。',
    R / 'docs/ideas.md': f'2026-10-27继续I-021/I-022：Quizlet补媒体、资料组织、分享/查看/编辑权限、成员关系、评分与推荐；原登记64篇正文有记录，但不等于产品交互已全部分析。见[第五十四期简报](/home/workspace/reader/docs/briefs/{brief})。围绕AI导入后的对象/权限/保存结果、家长交付儿童材料、成人推荐依据与知识边界新增可查证问题，仍为研究建议；未新增确认需求、账户制度或开始全系统设计。',
    L / 'verification-log.md': '第54批：root读取三处官方目录、原剩余21篇及两篇关联帮助正文，23来源保留原始网页工具响应、章节定位、canonical/entry URL及SHA；没有源HTML、图像、账户或外部写入。图片卡面、分享范围、权限措辞和旧系统排查差异原样保留；首页推荐主链不足，保持D1。worker盘点原21条，仅在指定本地范围判断未见正文；其首次验证错误假定工作目录只有自己的报告，遇根并行创建文件而退出1，无副作用，后纠正。根核原行号/标题与实际URL后完成读取整合，未将目录数当功能数。另一worker只读设计问题入口；请求中的reuse-points.csv不存在，按指南确认实际reference-points.csv，检索无匹配返回1，不是产品证据失败；跨批群组与个人搜索依据保留原来源链接。根负责最终来源和F/E/RP一致性核查，结果保存于work/research/batch54-integration/。磁盘不足时仅清理PyMuPDF与numpy/_core/tests两处__pycache__共86个可再生pyc、9,338,113字节；逐件核存在源py及未删包文件SHA不变，审计cache-cleanup.json。无正式证据删除；文件/链接/散列检查不证明真实产品行为。',
}
for p, paragraph in extra.items():
    p.write_text(p.read_text().rstrip() + '\n\n' + paragraph + '\n')
print('Updated case discovery, five design questions, history, backlog and verification notes.')
