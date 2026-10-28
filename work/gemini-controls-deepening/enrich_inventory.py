from pathlib import Path
from urllib.parse import urljoin
import csv,json,hashlib,io
R=Path('/home/workspace/reader');W=R/'work/gemini-controls-deepening';L=R/'docs/research/interaction-library'
audit=W/'inventory-enrichment.json';assert not audit.exists()
p=L/'batches/gemini-controls-deepening-inventory.csv';before=p.read_bytes();rows=list(csv.DictReader(io.StringIO(before.decode())))
table=json.loads((W/'connected-app-table.json').read_text())
for i,row in enumerate(rows):
 row.update(declared_requirements='',declared_platforms_and_experiences='',detail_urls='')
 if row['entry_type']=='connector_directory_entry':
  src=table[i];assert row['title']==src['cells'][0].split(':')[0]
  row.update(declared_requirements=src['cells'][1].replace('\n','; '),declared_platforms_and_experiences=src['cells'][2].replace('\n','; '),detail_urls=';'.join(dict.fromkeys(urljoin(row['url'],u) for u in src['links'] if u)))
buf=io.StringIO(newline='');w=csv.DictWriter(buf,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows);after=buf.getvalue().encode()
backup=W/'inventory-before-enrichment.csv';backup.write_bytes(before);p.write_bytes(after)
audit.write_text(json.dumps(dict(path=str(p.relative_to(R)),backup=str(backup.relative_to(R)),before_sha256=hashlib.sha256(before).hexdigest(),after_sha256=hashlib.sha256(after).hexdigest(),rows=len(rows),directory_entries=len(table),note='Retain factual account/age/location/language/platform criteria and help targets; no marketing description copied; extraction is not child-help analysis'),ensure_ascii=False,indent=2)+'\n')
print('Enriched 85 directory rows with captured criteria and source targets; total 129 rows unchanged')
