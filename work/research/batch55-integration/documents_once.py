from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
title='第五十五期：全局入口与执行环境';brief='055-global-entry-and-execution-context.md'
summary='快捷指令补语音/按键/组件、共享输入、提醒回访与配置组织；修正预览范围，新增14条设计参考。'
for p in [R/'README.md',L/'README.md']:
    ss=p.read_text().splitlines();i=next(i for i,s in enumerate(ss) if s.startswith('- [第五十四期'))
    ss[i]=f'- [{title}](/home/workspace/reader/docs/briefs/{brief})：{summary}';p.write_text('\n'.join(ss)+'\n')
p=R/'docs/briefs/README.md';s=p.read_text();assert title not in s;p.write_text(s.replace('- [第五十四期',f'- [{title}](/home/workspace/reader/docs/briefs/{brief})：{summary}\n- [第五十四期',1))
p=L/'inventories/README.md';s=p.read_text();old='19分组及95主题，共114节点；第47批后49主题已读、46主题仅目录，保留原链接和版本 | 欢迎链接无版本，其余主题为10.0/27；含非功能页，不是全部动态App动作，目录未读项不升级'
new='19分组及95主题，共114节点；第55批后95主题均有正文记录，含欢迎/版权两项范围资料 | 欢迎请求链接无版本、选择器27；其余主题10.0/27；概念/版权不计独立功能，全部动态App动作与实际分支仍未穷尽'
assert old in s;p.write_text(s.replace(old,new,1))
p=L/'data-dictionary.md';s=p.read_text();anchor='- raw_work_path 等临时原件路径相对项目根目录。'
assert anchor in s;s=s.replace(anchor,'- 如 `raw_encoding=gzip`，`raw_sha256` 对应实际保存的压缩文件，`decompressed_sha256` 对应解压后的HTTP原文；`extracted_work_path` / `extracted_sha256` 对应提取正文。核正文身份应解压并检查目标ID/版本，不能只核文件存在。\n- 同一指南的欢迎/版权等非操作附页，可作为目录范围的附加采集；须保留各自 `source_url` 和 `role`，不当主页面正文或独立功能证据。\n'+anchor,1);p.write_text(s)
p=L/'design-reference-guide.md';s=p.read_text();anchor='| 媒体已回放或图点已出现，整套学习资料是否已保存 |'
new='''| AI任务从语音、按键或组件启动后，在哪里继续操作 | [Shortcuts F20～23](cases/M-043-shortcuts.md) | [入口、锁屏与执行环境](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-11～13/15 | 配置、出现入口和实际执行分开；文档有解锁、补输入与转主App条件，未实测时延或恢复 |
| 当前页面共享给AI的到底是什么 | [Shortcuts F03/08](cases/M-043-shortcuts.md) | [输入类型与剪贴板](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-14/24 | 类型影响入口；剪贴板范围/到期不证明其他App副本被清除 |
| 提醒、打开编辑器、导入模板是否已经执行任务 | [Shortcuts F17/19/25](cases/M-043-shortcuts.md) | [Gallery、URL和提醒回访](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-16/20/21 | 提醒需点按，导航与执行有不同入口；加入集合不证明业务数据已写入 |
| AI移除入口、删文件夹或复制配置，会影响哪些对象 | [Shortcuts F11/24](cases/M-043-shortcuts.md) | [组织、删除与副本](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-17～19 | Remove保留All，删夹可保留内容；副本权限/自动化继承与跨端恢复未知 |
| 导入资料时，字段与时间怎样对应到目标对象 | [Shortcuts F18/26/27](cases/M-043-shortcuts.md) | [联系人映射与时间格式](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-22/23 | 原示例变量有歧义；秒/毫秒、时区、显示精度及部分写入必须分别核，未执行导入 |
'''
assert anchor in s and 'RP-M043-11～13' not in s;s=s.replace(anchor,new+anchor,1)
s=s.replace('| 取消、错误、没有输出分别说明什么 | RP08/10，E-SCDF-04/06/07/19～21 | 回调/预览不是业务保存证明；取消无输出不等于没有副作用 |','| 取消、错误、没有输出分别说明什么 | RP08/10，E-SCDF-04/06/07/19～21；第55批补E-SCEN-03 | 回调/预览不是业务保存证明；两篇27帮助对编辑器外预览范围有差异，见[补证](artifacts/shortcuts-entry-and-organization/source-notes.md) |')
p.write_text(s)
extra={
L/'analysis-status.md':f'第55批快捷指令补原余46主题（44篇操作/概念、欢迎/版权2范围页），原95主题均有正文记录；新增9D2/44E，补10旧F、目录证据2次附加采集，14新RP并修订原预览RP。当前27F均D2，原114目录规模不变；文档预览、联系人变量引用和日期措辞差异保留。没有运行指令、创建提醒/联系人或观察图片；全部65组仍部分覆盖，见[简报55](../../briefs/{brief})。',
L/'backlog.md':'第55批快捷指令：原95主题均有正文记录，Siri/控制中心/Action键/Pencil/Back Tap/搜索/桌面/共享/手表入口不再列作仅目录。下一层需同设备验证语音重名、锁屏取消、组件输入/内存错误转App、手表手机依赖、共享实际对象、提醒链接在改名/删除后的行为、移除/删夹/复制的跨端后果，以及批量导入缺字段/重跑/部分失败。两篇27帮助预览范围不一致、联系人例子变量引用疑点保留；日期单位与时区须核。原目录之外的跨运行存储、动态App动作仍缺，不能以95篇正文覆盖当全产品完成。',
L/'batches/README.md':f'第55批 `shortcuts-entry-and-organization`：44新E/9D2、10旧F更新；原46目录条目补读，欢迎/版权关联目录证据的2次范围采集，其余44为操作/概念正文。原114行不增加；新增14RP、修订RP-M043-10的预览范围。HTML用gzip保存并分别核压缩/解压/提取正文SHA，无新视觉或实操。见[简报55](../../../briefs/{brief})。',
R/'docs/ideas.md':f'2026-10-27继续I-021/I-022：补全局语音/按键/组件入口、共享输入、提醒回访、配置组织和结构化写入，保留执行环境与结果条件；修正外部预览的文档范围，见[第五十五期简报](/home/workspace/reader/docs/briefs/{brief})。将唤起、补输入、移交执行、反馈和作用对象分别提供给系统AI，是服务G-002的候选研究问题；未确定Plus按键、手势、权限或运行时，没有新增已确认需求。',
L/'verification-log.md':'第55批：根读取现有需求、案例与目录，确认第54批manifest与正式状态一致，前轮为progress。按原剩余46主题逐项匿名GET均200，45文章正文ID匹配、46选择器为iOS27；欢迎页不含article-section，初提取仅1字符后停止批量，检查HTML后按passion-point-section至toc-hidden-content内容区提取，再继续其余45项。全部正文已读，欢迎/版权仅范围材料；不把图片alt或空图标当观察。原始HTML压缩保存，压缩/解压/文本SHA分别登记。旧Completion正文定点复读后，修订F06及RP10的预览范围差异；联系人映射与时间措辞不擅自纠正为已验证行为。没有运行scheme、指令、示例API或写入外部应用，也未使用子代理。低磁盘时仅删除networkx/algorithms、onnxruntime/transformers、numpy/lib下610个有源py的可再生pyc，共10,125,501字节；全部源及其他包文件SHA不变，审计work/research/batch55-integration/cache-cleanup.json。最终文件/链接检查与根来源/索引核查保存在该工作目录；这些检查不证明实际产品行为。'
}
for p,v in extra.items():p.write_text(p.read_text().rstrip()+'\n\n'+v+'\n')
print('Updated research entrances, inventory status, field definitions and continuation records.')
