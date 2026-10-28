from pathlib import Path
import hashlib,json
root=Path('/home/workspace/reader');work=root/'work';docs=root/'docs'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
formal={}
for p in docs.rglob('*'):
 if p.is_file():
  key=(p.stat().st_size,sha(p));formal.setdefault(key,[]).append(str(p.relative_to(root)))
before=[];other=[]
for p in work.rglob('*'):
 if not p.is_file() or 'batch44-storage-audit' in p.parts:continue
 relstr=str(p.relative_to(root))
 if relstr.startswith('work/research/tools/') or any(x in p.parts for x in ['node_modules','.git']):continue
 size=p.stat().st_size
 if size==0:continue
 key=(size,sha(p));matches=formal.get(key)
 if not matches:continue
 row={'delete_path':relstr,'bytes':size,'sha256':key[1],'retain_paths':matches}
 (before if '/before/' in '/'+relstr else other).append(row)
wanted={
'work/research/batch38-week/desktop-profile/Default/Cache','work/research/batch38-week/desktop-profile/Default/Code Cache','work/research/batch38-week/desktop-profile/Default/GPUCache','work/research/batch38-week/desktop-profile/Default/Shared Dictionary/cache','work/research/batch43-time-sample/profile/Default/Cache','work/research/batch43-time-sample/profile/Default/Code Cache','work/research/batch43-time-sample/profile/Default/GPUCache','work/research/batch43-time-sample/profile/Default/Shared Dictionary/cache'}
caches=[]
for p in work.rglob('*'):
 if p.is_dir() and str(p.relative_to(root)) in wanted:
  total=sum(x.stat().st_size for x in p.rglob('*') if x.is_file())
  caches.append({'delete_directory':str(p.relative_to(root)),'bytes':total,'reason':'Chromium transient cache only; preserve surrounding profile storage/session state','retain_path':None})
out={'checked_on':'2026-10-27','no_deletion_performed':True,'before_exact_duplicates':sorted(before,key=lambda x:x['delete_path']),'other_work_exact_duplicates':sorted(other,key=lambda x:x['delete_path']),'browser_cache_directories':sorted(caches,key=lambda x:x['delete_directory'])}
out['totals']={'before_files':len(before),'before_bytes':sum(x['bytes'] for x in before),'other_files':len(other),'other_bytes':sum(x['bytes'] for x in other),'cache_directories':len(caches),'cache_bytes':sum(x['bytes'] for x in caches),'candidate_bytes':sum(x['bytes'] for x in before)+sum(x['bytes'] for x in other)+sum(x['bytes'] for x in caches)}
(root/'work/research/batch44-storage-audit/candidates.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(out['totals'],ensure_ascii=False))
