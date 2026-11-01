from pathlib import Path
import csv, json, hashlib

ROOT = Path('/home/workspace/reader')
LIB = ROOT / 'docs/research/interaction-library'
W = ROOT / 'work/bbdc-controls-deepening'
A = LIB / 'artifacts/bbdc-controls-deepening'
B = LIB / 'batches'
A.mkdir(parents=True, exist_ok=True)
baseline = json.loads((W/'baseline.json').read_text())
case = LIB / 'cases/M-018-bbdc.md'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(case) == baseline['case_sha256'], 'case changed since baseline'
assert not baseline['reverse_dependencies']
assert not baseline['prefix_collisions']

notes = '''# 不背单词：公开原件与索引的取得边界

查阅 2026-10-27；仅文档，没有安装、登录、提交、播放或静态界面观察。沿用 E-BBDC-01、E-BBDC-03、E-BBD-01；没有新增来源编号。

| 旧 E / 原始 URL | 本次实际取得 | 核对定位与限制 |
| --- | --- | --- |
| E-BBDC-01 / https://www.bbdc.cn/index | HTTP 200 HTML | 顶部及移动导航、产品段落。登录表单有隐藏节点；解析文本不证明页面已显示该表单。用户评价排除。 |
| E-BBD-01 / https://apps.apple.com/cn/app/不背单词-四六级考研等英语单词学习/id698570469?platform=mac | HTTP 200 HTML，另经 web 打开正文 | `script#serialized-server-data` → `data[0].data.shelfMapping.mostRecentVersion.seeAllAction.pageData.shelves[0].items`。每项 primarySubtitle 对应版本，secondarySubtitle 对应日期；共 25 项，起于 5.9.3，止于 5.11.6。这是可取得窗口，不是全部版本历史。 |
| E-BBDC-03 / https://learnywhere.cn/lexis/privacy | HTTP 200、976 字节脚本壳；web.open 亦仅壳；另保存 web.search 返回的政策索引文本 | 不能称本地 HTML 包含政策正文。索引标更新 2026-08-28、生效 2026-09-04，App 构建未知。第二章（一）、（三）、（五）和第六章（一）/（二）为主要定位。 |

商店内嵌数据把 5.11.6 定位到 2026-09-18；纠错按钮条目是尝试性修复，不是验证成功。5.9.8 的分组尺寸不能改写为每日计划量；5.9.17 的日数据边界不能改写为清空队列或强退行为。

政策同页分别使用账号设置和账号信息入口名称，未给客户端构建。其密码登录分支限特定老账号；账户识别、跨产品共享及用户可执行备份是不同问题。政策也提服务方备份，仍没有个人备份文件、回滚或恢复完成步骤。

正式目录只登记来源有写出的项目，并将日志能力、政策入口、网页导航和未解任务分开。未读到应用内帮助目录或各题正文；不得把权限清单推成听写或口语测评的实际 UI。旧 403 脚本、知乎和已知登录入口本次没有重试。无新视觉审计；不依赖案例全文散列。
'''
(A/'source-notes.md').write_text(notes)

fields = ['item_id','inventory_kind','functional_domain','known_entry','source_locator','evidence_ids','flow_ids','platform_version','coverage_state','raw_work_path','gaps','next_evidence','checked_on']
rows=[]
def add(kind, domain, entry, loc, es, fs, scope, state, raw, gaps, nxt):
 rows.append(dict(zip(fields,[f'M018-I{len(rows)+1:02}',kind,domain,entry,loc,es,fs,scope,state,raw,gaps,nxt,'2026-10-27'])))

apple='work/bbdc-controls-deepening/appstore-cn.html'
policy='work/bbdc-controls-deepening/privacy-web-search-response.json'
index='work/bbdc-controls-deepening/index.html'
specs=[
 ('词书计划','入口未写','5.11','F-M018-03','期限/日量编辑、切书和漏学补排'),
 ('单元筛选','入口未写','5.9.15;5.11.6','F-M018-03','确认/清空范围及实际选词与展示的关系'),
 ('复习组尺寸','入口未写','5.9.8','F-M018-01','100词/组不是每日额度；分组中断和未完处理'),
 ('学习后与复习后拼写设置','入口未写','5.9.8','F-M018-01','分别开关的位置、保存和返回'),
 ('错词专项','完成学习后的功能声明','5.11','F-M018-02','错词集合来源、空集合、重试和终止'),
 ('拼写提示','入口未写','5.11.3','F-M018-02','提示阶梯与提示后评分'),
 ('返回上一词','学习/复习的功能声明','5.9.4','F-M018-02','是否撤销答案、调度和计数'),
 ('纠错按钮','尝试性修复记录','5.11.6','F-M018-02','适用题型、按钮恢复及更改调度未证'),
 ('单词本学习模式','入口未写','5.9.4','F-M018-03','Learn/Review/备考速刷队列是否共用'),
 ('批量移动与复制','入口未写','5.9.5','F-M018-03','选择、确认、重复词及撤销'),
 ('PDF输出','入口未写','5.9.11;5.9.12','F-M018-03','字段、选择范围、失败和重导；不是可恢复备份证明'),
 ('关联笔记','词间关联的功能声明','5.9.19','F-M018-03','编辑/删除传播和断网冲突'),
 ('学习日边界','日数据更新时间记录','5.9.17','F-M018-04','04:00不证重排计划或中断重置'),
 ('词组搭配','产品描述；没有操作入口','产品描述','F-M018-01','独立训练、反馈和完成条件'),
 ('班级与成就','存在性/页面更新声明','5.9.13;5.11.3','F-M018-04','奖励规则、补签、能力含义'),
]
for domain,entry,loc,fs,gap in specs:
 add('developer_release_or_description',domain,entry,loc,'E-BBD-01',fs,'CN iPhone/iPad;日志各版本分开','D1_body_read',apple,gap,'对应版本官方操作帮助或有版本的连续示范')

policy_items=[
 ('账户控制','头像→账号设置','第二章（一）','F-M018-04','与第六章名称不同；验证、解绑和恢复结果未知'),
 ('账户信息','头像→头像→账号信息','第六章（一）/（二）','F-M018-04','与第二章入口关系未说明'),
 ('账号标识','头像→更多设置','第二章（一）2','F-M018-04','ID不能替代已认证的账号恢复凭据'),
 ('学习数据','我的学习→我的数据','第二章（五）','F-M018-04','沿用局部D2；计算口径和断网刷新未知'),
 ('学习内容','我的内容','第六章（一）4','F-M018-03','未给删除/导出/恢复步骤'),
 ('酷币明细','头像→我的酷币','第六章（一）2','F-M018-04','流水纠错及奖励公式未知'),
 ('权益查看','头像→装备/大会员权益','第六章（一）3','','权益刷新和恢复购买未知；未操作交易'),
 ('帮助','头像→更多设置→帮助与反馈','第二章（三）','F-M018-05','仅说明模块式帮助；具体目录和题文未取得'),
 ('推荐控制','更多设置内','第二章（十一）1','','开关反馈和跨端持久化未知'),
 ('壁纸随动','更多设置内','第二章（十一）6','','Android条件；没有测响应和耗电'),
 ('权限入口','更多设置内','第六章（四）','','权限恢复不等于数据恢复'),
 ('账号注销','更多设置内','第六章（五）','','确认/取消/冷静期/误删恢复未知'),
 ('协议入口','更多设置内','第八章1','','取得的是索引文本，不是App内页面观察'),
]
for domain,entry,loc,fs,gap in policy_items:
 add('policy_named_entry',domain,entry,loc,'E-BBDC-03',fs,'政策2026-08-28；未给App构建','official_index_text_only',policy,gap,'对应当前端设置截图与官方帮助正文')

for domain,entry,es,state,gap in [
 ('我的生词','HTML导航的登录挂钩','E-BBDC-01','html_navigation_only','未触发控件；不证弹窗或词表'),
 ('自制词书','/lexis_book_index','E-BBDC-01;E-BBDC-04;E-BBD-03','html_navigation_and_prior_login_redirect','本次不重复登录路线；导入格式/校订/去重仍未知'),
 ('公告','官网公告链接','E-BBDC-01;E-BBD-04','html_navigation_prior_body_unavailable','旧脚本403停止；无本轮公告正文'),
]:
 add('public_navigation',domain,entry,'官网顶部/移动导航',es,'F-M018-03' if domain!='公告' else 'F-M018-05','公开官网；构建未知',state,index,gap,'官方可匿名访问的正文')

for domain,fs,gap,nxt in [
 ('听写与听词','F-M018-01','未取得能区分只听/听写/拼写的官方操作链','有题型名称/输入/判定/结束的官方教程'),
 ('离线和中断','F-M018-01;F-M018-02','下载范围、断网提交、强退续学与跨日队列未知','官方缓存/未完成学习FAQ'),
 ('账户与学习恢复','F-M018-04','换号/账号合并、换机重装、冲突、版本回滚均未知','绑定/换机/删除恢复帮助，需明确数据对象'),
]:
 add('unresolved_task',domain,'没有取得入口','本轮缺口核查','',fs,'未核定','unresolved','',gap,nxt)

with (B/'bbdc-controls-deepening-inventory.csv').open('w',newline='') as f:
 writer=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');writer.writeheader();writer.writerows(rows)
(B/'bbdc-controls-deepening-evidence.jsonl').write_text('')
cf=list(baseline['flows'][0]);cf.remove('batch')
with (B/'bbdc-controls-deepening-coverage.csv').open('w',newline='') as f:csv.writer(f,lineterminator='\n').writerow(cf)

text=(W/'case-before.md').read_text()
text=text.replace('查阅：2026-10-26。','查阅：2026-10-26；本轮复核 2026-10-27。',1)
text=text.replace('正文同为 5.11.6，日期显示“9月18日”；','正文同为 5.11.6，日期显示“9月18日”；本轮原 HTML 内嵌版本记录明确为 2026-09-18；',1)
text=text.replace('两端不合并为同一版本。','Android 条目本轮未重新获取，仍为上轮记录；两端不合并为同一版本。',1)
text=text.replace('5.11.6 “记错了”相关记录 D1','5.11.6 “记错了”相关记录 D1')
text=text.replace('可筛单元；“默认全选”展示曾修复 D1','可筛单元；日志称修复“默认全选”展示 D1')
text=text.replace('返回上一词及“记错了”相关记录 D1','返回上一词及尝试修复“记错了”按钮的记录 D1')
text=text.replace('单词本可进入不同学习模式；复习分组选项 D1','单词本有不同学习模式；复习 100 词/组选项 D1；不是每日量')
text=text.replace('5.11.6 的修复说明证明开发者曾处理纠错按钮问题，不能证明本轮已复现问题或修复完全。','5.11.6 对纠错按钮写的是“尝试修复”；只证明开发者记录过修复尝试，不能写成按钮已恢复，更没有本轮复现或验证结果。')
text=text.replace('具体设置入口未知 | 宣传称安排复习；','具体设置入口未知；5.9.8 的 100 词/组是复习分组尺寸，不是每日计划 | 宣传称安排复习；')
text=text.replace('返回、改判、错词专项是三个待区分的动作，','5.11.6 对纠错按钮仅记修复尝试；返回、改判、错词专项是三个待区分的动作，')
text=text.replace('同步冲突、延迟、断网队列、重装恢复未知。E-BBDC-03','政策索引另列账户入口与老账号条件，见本轮附录；同步冲突、延迟、断网队列、重装恢复未知。E-BBDC-03',1)
text=text.replace('未提交或读取应用内 FAQ |','未提交或读取应用内 FAQ；本轮政策原址直接取得的仍仅脚本壳 |',1)
text=text.replace('本轮新增证据，查阅日均为 2026-10-26：','上一批新增证据，查阅日均为 2026-10-26：')
text+='''
## 2026-10-27 目录复核与恢复缺口

本轮没有新增 E/F，原 5 条深度不变。可取得的[目录台账](../batches/bbdc-controls-deepening-inventory.csv)包含 34 项：15 项商店功能/日志、13 项政策记载入口、3 项网页导航及3项未解任务。它不是完整设置菜单，也不表示完成 34 个任务；没有取得应用内帮助的逐题目录和正文。[取证短摘记](../artifacts/bbdc-controls-deepening/source-notes.md)区分原 HTML、内嵌数据与搜索索引。

- **版本核定**：E-BBD-01 的原 HTML 内嵌版本窗口共 25 项（5.9.3～5.11.6），为每项提供完整日期。最新一项为 2026-09-18；不据此宣称安装端已更新。主任务仍缺入口、输入、反馈与结束链。
- **复习和计划**：组大小、日数据边界、每日计划是不同对象。04:00 的数据更新时间不能证明跨日后自动中止、清空或重排；完成学习后能练错词也不能证明完成即掌握。分组内退出、提示后判定及误评撤销仍未知。
- **账户条件与名称差异**：E-BBDC-03 的搜索索引正文把邮箱/ID加密码留给未通过其他列明方式注册的老用户；第二章与第六章分别使用“账号设置”和“账号信息”。本轮未取得客户端构建，不统一这两个入口，也不由网页登录表单推定现版 App 的恢复方法。共享数据范围沿用 F04；账号可登录不等于学习数据已恢复。
- **导出和恢复**：词表的 PDF 出口、跨产品同步、服务方备份各自存在证据边界。仍缺个人可取得的备份文件、还原入口、覆盖确认及恢复完成状态；同样没有收藏原句、听写判分、离线续学的官方完整链。

政策 URL 本次 HTTP 200 只返回 976 字节脚本壳；web.open 同样没有正文。搜索工具返回可读政策索引，已单独保存响应与散列，不能把它称为本地 HTML 正文。没有重试上轮 403 的脚本和知乎入口，也没有重新进入已知登录路线。新增检索仍主要落在商店或第三方教程；B 站索引中的官方账号线索只有纪念片，未观看，也不能当教学。没有建立穷尽声明。

下一类有效材料是可匿名读取、带平台与构建的官方学习/设置 FAQ，或原厂连续教程及其可读说明；要能看清计划保存、拼写提示/重试、错词结束和重装恢复状态。仅增加功能宣传不能补这些缺口。本轮没有看静态图或执行产品动作；旧 E 均未将本案例用作视觉审计文件，无需迁移审计散列。
'''
case.write_text(text)

changes={
 'F-M018-01':{'known_branches':'分别配置学后/复习后拼写；5.9.8复习100词/组不是每日计划；Android例句显示修复仍限旧记录','gaps':'逐题回忆/揭示/评价/反馈、听写与听词区分、原声音频、分组中断和跨日队列未知'},
 'F-M018-02':{'known_branches':'回上一词/只拼错词/提示页声明；5.11.6仅记尝试修复纠错按钮，不证修复成功','gaps':'返回是否撤销、空错词集合、提示阶梯、重试、按钮恢复和队列更新未知'},
 'F-M018-04':{'known_branches':'部分共享数据范围；政策索引列特定老账号密码条件和两种账户入口名称；04:00限日数据边界','gaps':'账号入口版本关系、身份合并、同步冲突、离线、重装和用户可执行备份/回滚未知'},
 'F-M018-05':{'known_branches':'iOS帮助路径；Android描述路径差异；2026-10-27政策直接请求仅壳、搜索索引仍有说明','gaps':'应用内帮助目录与具体FAQ正文、反馈受理状态、离线保存和工单恢复未知'},
}
updates=[]
for flow in baseline['flows']:
 if flow['flow_id'] not in changes:continue
 for field,value in {**changes[flow['flow_id']],'checked_on':'2026-10-27'}.items():
  updates.append({'flow_id':flow['flow_id'],'source_batch':flow['batch'],'field':field,'before':flow[field],'after':value,'reason':'原任务边界及取得方式补证；编号/深度不变，仅建议root合并'})
(W/'existing-coverage-updates.json').write_text(json.dumps(updates,ensure_ascii=False,indent=2)+'\n')
with (W/'existing-coverage-updates.csv').open('w',newline='') as f:
 cw=csv.DictWriter(f,fieldnames=list(updates[0]),lineterminator='\n');cw.writeheader();cw.writerows(updates)

captures=json.loads((W/'captures.json').read_text())
records={x['evidence_id']:x for x in baseline['evidence']}
ac=[]
for eid in ['E-BBDC-01','E-BBD-01','E-BBDC-03']:
 current=[]
 for cap in captures:
  if cap['evidence_id']!=eid:continue
  current.append({'capture_kind':'http_shell_only' if eid=='E-BBDC-03' else 'public_html_get','checked_on':'2026-10-27','url':cap['url'],'final_url':cap['final_url'],'http_status':cap['status'],'raw_work_path':cap['raw_work_path'],'raw_sha256':cap['raw_sha256'],'response_bytes':cap['bytes'],'locator':{'E-BBDC-01':'顶部/移动导航；表单节点非实际操作','E-BBD-01':'serialized-server-data > data[0].data.shelfMapping.mostRecentVersion.seeAllAction.pageData.shelves[0].items；版本标签/完整日期','E-BBDC-03':'976字节JavaScript壳；不含政策正文'}[eid],'archived_artifact':'artifacts/bbdc-controls-deepening/source-notes.md','sha256':sha(A/'source-notes.md'),'hands_on':False,'visual_observed':False})
 if eid=='E-BBDC-03':
  raw=W/'privacy-web-search-response.json'
  current.append({'capture_kind':'official_search_index_text','checked_on':'2026-10-27','url':records[eid]['url'],'raw_work_path':str(raw.relative_to(ROOT)),'raw_sha256':sha(raw),'upstream_http_body_verified':False,'locator':'web.search响应；第二章（一）/（三）/（五）、第六章（一）/（二）；与直接GET脚本壳分开','archived_artifact':'artifacts/bbdc-controls-deepening/source-notes.md','sha256':sha(A/'source-notes.md'),'hands_on':False,'visual_observed':False})
 before=records[eid].get('additional_captures');ac.append({'evidence_id':eid,'source_batch':records[eid]['batch'],'field':'additional_captures','before':before,'after':(before or [])+current,'reason':'同URL追加取得方式和原件散列，不重复编号；政策原件只壳而索引有正文'})
(W/'additional-captures-updates.json').write_text(json.dumps(ac,ensure_ascii=False,indent=2)+'\n')
counts={'new_evidence':0,'new_flows':0,'old_flows_proposed':len(changes),'old_flow_field_updates':len(updates),'old_evidence_proposed':len(ac),'new_captures':sum(len(x['after'])-len(x['before'] or []) for x in ac),'inventory_rows':len(rows),'images_observed':0}
(W/'counts.json').write_text(json.dumps(counts,indent=2)+'\n');print(counts)
