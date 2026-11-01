from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
title='第五十三期：音频学习、会话与游戏结果';brief='053-audio-learning-and-session-state.md'
summary='Quizlet补音频输入/生成、问答中断、游戏与共享结果；原Studying目录27项均有关联正文，新增10条设计参考。'
for p in [R/'README.md',L/'README.md']:
    ss=p.read_text().splitlines();i=next(i for i,s in enumerate(ss) if s.startswith('- [第五十二期'))
    ss[i]=f'- [{title}](/home/workspace/reader/docs/briefs/{brief})：{summary}';p.write_text('\n'.join(ss)+'\n')
p=R/'docs/briefs/README.md';s=p.read_text();assert title not in s;s=s.replace('- [第五十二期',f'- [{title}](/home/workspace/reader/docs/briefs/{brief})：{summary}\n- [第五十二期',1);p.write_text(s)
p=L/'design-reference-guide.md';s=p.read_text();anchor='| AI对话生成卡片后，怎样知道已经保存到正确位置 |'
new='''| AI会话被最小化、离开后，内容是否还在 | [Quizlet F19](cases/M-021-quizlet.md) | [对话与生成对象](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-11 | 文档限定US14岁以上；会话规则不替代卡组保存、刷新或跨端恢复 |
| 录音能否继续上传，选错语言怎样改 | [Quizlet F02/12/18](cases/M-021-quizlet.md) | [音频输入、重建与生成边界](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-08～10/17 | 本机上传草稿、重新生成、来源准确性和播放断点分开；没有实际录音或恢复 |
| 游戏得分和大家的学习结果能说明什么 | [Quizlet F20/21](cases/M-021-quizlet.md) | [加步奖励与群组统计](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-12/13 | 依据产品规则而非学习效果；不能推导个人长期掌握或儿童适配 |
| AI读哪些材料、解答来源是否经过核验 | [Quizlet F01/22/23](cases/M-021-quizlet.md) | [上下文范围、解答与查找](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-14～16 | 选中文字/符合条件标签页不等于全页面访问；题库与追加AI输出分别核对 |
'''
assert anchor in s and 'RP-M021-11' not in s;s=s.replace(anchor,new+anchor,1);p.write_text(s)
extra={
L/'analysis-status.md':f'第53批Quizlet补原目录八篇正文，新增六条D2、补三旧F；8新E、2旧E追加采集（含Khan失败范围复核）、10RP。原68行改11行，其中三项是既有游戏正文回填；Studying原27项均有正文记录，其他目录21项仍仅标题。Khan培训页正文原已读，图片三次403后停止，无新增画面/任务。全部65组仍部分覆盖，见[简报53](../../briefs/{brief})。',
L/'backlog.md':'第53批Quizlet：音频上传本机草稿/处理任务、错语言新建指南、Ask最小化/离开、Charms加步、共享会话结果、专家解答及范围搜索已有正文。仍缺原生播放/续播、录音来电、上传取消/强退/卸载、旧对象联动、对话刷新/写入回执、游戏错误反馈/结算/保存、共享撤回/历史和检索空结果。原Studying27项目录无“仅目录”标签，但既有短记并不等于当前全部分支已核。Khan E-KHD-04正文早已读，本次三幅Google Sites图片403停止；不要再将该页正文列作未读，也不要因新下载失败覆盖早期正常文本证据。',
L/'batches/README.md':f'第53批 `quizlet-audio-and-study` 新增8E/6D2，3旧F补证；原目录更新8新正文及3旧游戏状态，原68行不变，10RP。E-QZCD-03追加目录采集；E-KHD-04追加旧页复核/三图片403的范围，无新Khan任务或图像。见[简报53](../../../briefs/{brief})。',
R/'docs/ideas.md':f'2026-10-27继续I-021/I-022：补音频输入/生成、问答会话、共享学习结果和游戏奖励的具体状态，详见[第五十三期简报](/home/workspace/reader/docs/briefs/{brief})。将会话、内容、学习结果分别查询和确认，以及检验游戏奖励是否增加学习收益，是面向G-001/G-002的研究建议；未新增已确认需求或进入全系统设计。',
L/'verification-log.md':'第53批：root从Quizlet原Studying目录打开八篇官方正文，保存网页工具响应；没有源HTML、图片或实际账户操作。原目录八项补正文，另三项按既有E-QZGM记录纠正未读状态，不把旧阅读计新增。worker只读盘点Khan；root复核发现E-KHD-04原已读正文，只有视频未观察，纠正其“真正未读入口”表述。旧培训页复读后三图片请求均403，停止路线，无视频/图像观察或深度升级。原始身份、目录改动和引用字段由本批verify_root.py核查；统一文档/链接检查由check_docs.py执行，结果在work/research/batch53-integration/。磁盘约4MB时，确认无对应浏览器后仅删除batch38-week/desktop-profile/Default/Code Cache的4,584,014字节可再生缓存，其他profile文件逐项SHA不变，审计cache-cleanup.json；没有删除正式证据。'
}
for p,v in extra.items():p.write_text(p.read_text().rstrip()+'\n\n'+v+'\n')
print('Updated discovery and design-reuse entrances, historical status, backlog, and verification notes.')
