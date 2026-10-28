from pathlib import Path
p=Path('/home/workspace/reader/docs/research/interaction-library/comparison.md')
s=p.read_text();prefix,rest=s.split('## 横向检索\n',1);table,suffix=rest.split('\n## 版本与冲突',1)
rows=[r for r in table.splitlines() if r.startswith('|')]
header='\n'.join(rows[:2]);rows=rows[2:]
assert len(rows)==53
groups=[('学习与反馈',[2,6,8,9,16,17,26,27,36,40,46,47]),('读听与中断恢复',[3,11,12,18,19,24,43,44,52,53]),('新闻与内容选择',[21,22,37,38,39,48]),('AI上下文与执行结果',[1,5,20,23,29,31,32,41,49]),('同步备份与成员数据',[4,13,14,25,34,42,50,51]),('导入笔记与内容关联',[15,28,30,35,45]),('系统交互与研究方法',[7,10,33])]
assert sorted(i for _,indices in groups for i in indices)==list(range(1,54))
rows[43]='| 定时停止是否有固定截止点？ | [iBasso F-M064-06](cases/M-064-ibasso.md) 的旧手册写操作后重计时，但1.04/1.05更新已改变计时功能，不能套给新版；[Yoto F-M037-02](cases/M-037-yoto.md) 换卡不重置。 | 分开系统关机、播放器计时及固定截止；先核固件、当前设置和交互影响。新版文案疑点仍待实测。 | R-018、G-002 |'
extra={
 '学习与反馈':'| 同一产品的所有题型能否共用纠错规则？ | [小猿 F-M028-02/05/06](cases/M-028-xiaoyuan-ai.md) 的历史体验材料提供不同题型的局部路径，版本未固定。 | 分清练习、测验、语音识别失败和知识错误；是否重做、何时前进、记录哪次结果均需逐题型确定。 | R-014/019、G-001/002 |',
 '新闻与内容选择':'| 关闭个性化推荐是否就得到个人报纸？ | [掌阅 F-M014-06](cases/M-014-ireader.md) 的2024政策给出关闭路径，仍保留非个性化内容栏目。 | 关闭兴趣画像、选择来源、编辑选题和扩大视野是不同目标；不能把开关状态当信息价值指标。 | R-020、G-001 |',
 'AI上下文与执行结果':'| 标称离线的设备能否首次直接使用？ | [有道2.0 F-M025-05](cases/M-025-youdao-pen.md) 的历史机构说明有首次联网激活条件。 | 设备激活、账号登录、内容准备、后续离线能力分别查询；不能只把“离线”当一个布尔值。 | R-009、G-002 |'
}
out=prefix+'先按问题域进入，再沿 F 编号查任务、E 编号查原始出处；新增证据补回原任务，避免反复复制事实。\n\n'
out+=' | '.join(f'[{title}](#{title})' for title,_ in groups)+'\n\n'
for title,indices in groups:
 out+='## '+title+'\n\n'+header+'\n'+'\n'.join(rows[i-1] for i in indices)+'\n'
 if title in extra:out+=extra[title]+'\n'
 out+='\n'
out+='## 版本与冲突'+suffix
p.write_text(out)
print('53 original comparison rows retained; 7 topic sections; 3 new research questions; timer scope corrected')
