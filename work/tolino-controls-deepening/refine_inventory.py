from pathlib import Path
import csv,json
p=Path('docs/research/interaction-library/batches/tolino-controls-deepening-inventory.csv');rs=list(csv.DictReader(p.open()));changes=[]
manual={2:[1],6:[12],10:[7],11:[7],12:[7],13:[7],14:[7],15:[4],16:[4],17:[4],18:[4],21:[2,10],22:[2,10],23:[6],24:[12,13],25:[12],26:[12],27:[13],28:[12],29:[14],30:[14],31:[8],32:[7],33:[7],34:[7],35:[7],36:[7]}
en={'1':[4],'1.1':[4],'1.2':[4],'1.3':[4],'1.4':[4],'2.1':[3],'2.2':[8],'2.3':[2],'2.4':[6],'2.5':[2],'2.6':[2],'2.7':[12,13],'2.8':[2,8],'2.9':[7],'2.10':[11],'2.11':[2],'3':[16],'3.1':[16],'3.2':[16],'3.3':[16],'3.4':[16],'3.5':[16],'4.1':[13],'4.2':[13],'4.4':[17],'4.5':[9],'4.6':[7],'4.7':[6],'4.11':[3],'4.12':[18],'5.1':[7],'5.2':[15],'6':[5],'6.1':[5],'6.2':[5],'7':[7,14,15],'7.1':[15],'7.2':[14],'7.3':[14],'7.4':[14],'9.1':[2],'9.2':[2],'9.3':[7],'9.4':[7],'9.6':[7],'9.7':[9],'9.8':[2,8]}
de={'1.3':[17],'1.4':[17],'1.5':[13],'1.7':[9],'1.8':[3],'1.9':[7],'1.10':[18],'1.11':[6],'2.1':[8],'2.2':[2],'2.3':[3],'2.4':[2],'2.5':[6],'2.6':[7],'2.7':[12,13],'2.8':[2],'2.9':[2],'2.10':[2,8],'2.11':[2,8],'2.12':[11],'3.2':[7],'3.3':[15],'4.1':[2,8],'4.3':[2,8],'4.4':[7],'4.5':[7],'4.6':[2],'4.7':[2,8],'4.8':[2],'4.9':[7],'4.11':[9],'5':[4],'5.1':[4],'5.2':[4],'5.3':[4],'5.4':[4],'6':[16],'6.1':[16],'6.2':[16],'6.3':[16],'6.4':[16],'7':[7,14,15],'7.1':[14],'7.2':[15],'7.3':[14],'7.4':[14],'8':[5],'8.1':[5],'8.2':[5]}
readde={'1.3','1.4','2.12','2.13','5','5.1','5.2','5.3','5.4','6','6.1','6.2','6.3','6.4','7','7.1','7.2','7.3','7.4','8','8.1','8.2'}
for r in rs:
 old=dict(r);src=r['source'];loc=r['locator']
 if src.startswith('English'):ns=manual.get(int(r['entry_id'][-3:]),[])
 elif src=='faq':ns=en.get(loc,[])
 elif src=='faq-de':
  ns=de.get(loc,[]);r['body_review_status']='已核相关正文段；非实机' if loc in readde else '标题已登记；本德文条目未深入正文；关联F可能来自英文资料'
 else:ns=[14]
 r['flow_ids']=';'.join(f'F-M058-{n:02}' for n in ns)
 r['coverage']='task_reference_partial' if ns else 'inventory_only_or_nonfunctional'
 if r!=old:changes.append(dict(entry_id=r['entry_id'],before=old,after=r))
with p.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
Path('work/tolino-controls-deepening/reviewed-inventory-changes.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2)+'\n')
print('Reviewed source-heading mappings',len(rs),'corrected',len(changes))
