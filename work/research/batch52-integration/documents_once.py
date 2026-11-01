from pathlib import Path
import json
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch52-integration'
title='第五十二期：学习资料如何进入，退出后留下什么'
brief='052-study-material-interactions.md'
summary='Quizlet补外部AI入口、扫描校订、临时/保存合并和资格限制；八项原目录补正文，新增七条设计参考依据。'
for p in [R/'README.md',L/'README.md']:
    s=p.read_text();lines=s.splitlines()
    idx=next(i for i,x in enumerate(lines) if x.startswith('- [第五十一期'))
    lines[idx]=f'- [{title}](/home/workspace/reader/docs/briefs/{brief})：{summary}'
    p.write_text('\n'.join(lines)+'\n')
p=R/'docs/briefs/README.md';s=p.read_text();needle='- [第五十一期'
assert title not in s;s=s.replace(needle,f'- [{title}](/home/workspace/reader/docs/briefs/{brief})：{summary}\n'+needle,1);p.write_text(s)
p=L/'design-reference-guide.md';s=p.read_text();anchor='| 打开设备文件，是否已经在电脑保存了副本 |'
added='''| AI对话生成卡片后，怎样知道已经保存到正确位置 | [Quizlet F01](cases/M-021-quizlet.md) | [外部AI生成与编辑入口](artifacts/quizlet-material-workflows/source-notes.md)；RP-M021-01 | 只有官方文档；账号归属、可见性、持久回执和重复生成未证 |
| 扫描资料怎样成为能检查的练习 | [Quizlet F03/05](cases/M-021-quizlet.md) | [人工配对、校订及草稿/发布](artifacts/quizlet-material-workflows/source-notes.md)；RP-M021-02/06 | 不能把识别框当准确配对；拒权、取消重扫及强退恢复未知 |
| 多套材料临时学一次，与建立新学习包有什么差别 | [Quizlet F04](cases/M-021-quizlet.md) | [临时组合、新集与复制资格](artifacts/quizlet-material-workflows/source-notes.md)；RP-M021-03～05 | 组合保存、源集保留和学习统计分别核；去重/进度继承没有依据 |
| 改评分规则会影响哪些活动和旧成绩 | [Quizlet F08](cases/M-021-quizlet.md) | [UK/US条件与偏好范围](artifacts/quizlet-material-workflows/source-notes.md)；RP-M021-07 | 设置作用范围有文档，历史成绩和长期调度回算未知 |
'''
assert anchor in s and 'RP-M021-01' not in s;s=s.replace(anchor,added+anchor,1);p.write_text(s)
updates={
L/'analysis-status.md':f'第52批补Quizlet原目录八篇正文，沿五个旧任务补分支，不新增F；8新E、2目录追加采集、8原目录行与7RP。总览只作功能关系，资格说明不作故障恢复闭环。Espresso补检未获得现版音频/翻译操作链，不升级深度。全部65组仍部分覆盖，见[简报52](../../briefs/{brief})。',
L/'backlog.md':'第52批Quizlet原目录八项已读，剩余优先核外部AI对象归属/回执/重复生成，扫描拒权/无字/取消重扫，临时组合的答题统计与保存新集的进度映射；普通Learn/Flashcards/离线旧材料缺口仍在。UK error checking是原目录第63项，不能误改第64项Study path。Espresso本轮搜索遇economist.com robots限制即停止该路线；主应用周刊下载、其他地区商店重复描述及第三方汇总不补独立Espresso当前音频控制链。没有新增正向证据或深度。',
L/'batches/README.md':f'第52批 `quizlet-material-workflows` 新增8官方帮助E，5旧F及2旧目录E在原批次补证，无新增任务；原68行目录仅更新8项，增加7RP。总览/资格/步骤分别记深度；没有图像或实际操作。见[简报52](../../../briefs/{brief})。',
R/'docs/ideas.md':f'2026-10-27继续I-021/I-022：Quizlet补扫描人工配对、外部AI进入编辑、临时组合/保存新集、复制资格及评分偏好范围。按设计问题新增七条可追溯依据，见[第五十二期简报](/home/workspace/reader/docs/briefs/{brief})。对成人复习、家长准备儿童资料和AI完成回执的启发保持建议；未新增已确认需求或进入全系统设计。',
L/'verification-log.md':'第52批：worker只读盘点Quizlet本地材料；root打开两个原目录并逐篇读取八篇官方帮助正文，保存工具响应及SHA。root核对后纠正助手盘点的UK error checking目录序号64→63；按原标题匹配更新。第一次整合脚本在目录更新前断言失败，原因是数据元组索引取到flow字段而非order；E/F和两次目录采集已写，随后只执行未应用的目录/RP尾段，避免重复追加。正式源审计见artifacts/quizlet-material-workflows/source-audit.json，记录差异见work/research/batch52-integration/。没有账户连接、登录、生成、扫描、答案提交或实际界面观察；Espresso搜索未产生可升级的操作依据。最终文件/链接检查与根验证结果由本批final-check.json、root-final-verification.json保存，仅证明资料一致性，不证明产品行为。'
}
for p,extra in updates.items():p.write_text(p.read_text().rstrip()+'\n\n'+extra+'\n')
p=W/'integration-corrections.json';j=json.loads(p.read_text());j['integration_recovery']={'first_exit':1,'cause':'byorder used tuple index 7 (flow IDs), expected 6 (order)','already_applied':'new evidence, five old flow updates, two directory captures','resumed_only':'inventory and RP','recovery_exit':0};p.write_text(json.dumps(j,ensure_ascii=False,indent=2)+'\n')
print('Updated current case entrances, design lookup, status, backlog, ideas and verification notes.')
