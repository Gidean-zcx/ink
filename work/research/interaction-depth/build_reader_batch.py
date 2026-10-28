import csv,json,re,hashlib
from pathlib import Path
root=Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
out=lib/'batches';out.mkdir(exist_ok=True)
names={'M-001':'M-001-kindle.md','M-003':'M-003-pocketbook.md','M-006':'M-006-supernote.md','M-012':'M-012-mudita.md'}
scopes={'M-001':'Kindle reading / Scribe handwriting; feature-specific generations and formats; build unknown',
'M-003':'PocketBook PB743K3 InkPad Color 3; manual URL 6.10.2854; generic FAQ qualified',
'M-006':'Supernote Manta/Nomad; historical help qualified by device-specific Chauvet changelog',
'M-012':'Mudita Kompakt; Quick Start unpinned; released 1.x separated from 2.0 preview'}
locators={
'E-KDL-01':'Navigation; Dictionary; Word Wise; Vocabulary Builder; TOC',
'E-KDL-02':'Active canvas; sticky notes; writing tools',
'E-KDL-03':'Send to Kindle vs USB; device/content compatibility tables',
'E-KDL-04':'Multiselect pages; add/move/share; 2025+ notes',
'E-KDL-05':'Share; Convert to text; download link expiry',
'E-KDL-06':'Quick Actions Sync; Sync off notebook restrictions',
'E-KDL-07':'Notebook missing/unchanged/template troubleshooting',
'E-KDL-08':'Getting started; importing; sending back; manual transfer limitation',
'E-KDL-09':'2025+ requirement; home pinned notebook; last page',
'E-PBK-01':'Printed pp13,25–27,32,51–55,65–68; TOC pp2–4',
'E-PBK-02':'Notes export; TTS download; Send-to-PocketBook activation/white list; Cloud',
'E-PBK-03':'Applications; model specifications; no performance verification',
'E-SUP-01':'Create/delete headings keywords stars; Main Layer; no undo statement',
'E-SUP-02':'Lasso link; toolbar; target; navigation; edit',
'E-SUP-03':'Categories; Sources; create from documents/notes/manual; delete category',
'E-SUP-04':'System backup/restore; Export folder; older destructive restore note',
'E-SUP-05':'Chauvet 3.24.38 dated 2025-08-22; later link-fix entries',
'E-SUP-06':'Before sync; avoid conflicts; A5 X and Nomad example',
'E-SUP-07':'Display & Input > Gestures; side/screen; left/right handed',
'E-MUD-01':'English manual printed pp2–4; navigation, Offline+, accessibility',
'E-MUD-02':'Control Center sound volume; separate sound functions',
'E-MUD-03':'To call a contact; new-number branch not expanded',
'E-MUD-04':'To send SMS to a contact; keyboard note',
'E-MUD-05':'To send SMS to a new number',
'E-MUD-06':'To send MMS to a contact; image attachment',
'E-MUD-07':'Delete conversations; two Delete steps',
'E-MUD-08':'Center > Manage Files > E-books > Add file; unsupported selection',
'E-MUD-09':'Center music transfer; unsupported file selection',
'E-MUD-10':'Select files > Delete > Delete file',
'E-MUD-11':'Version 1.4 release scope only',
'E-MUD-12':'Future 2.0 preview; app management/calendar plans, not release proof',
'E-MUD-13':'Center sideloading described; no compatibility experiment',
'E-MUD-14':'Version 1.6 release entry; attachment not used for interaction claims',
}
index_only={'E-KDL-09','E-MUD-05','E-MUD-06','E-MUD-09','E-MUD-10','E-MUD-12','E-MUD-13'}
evidence=[];flows=[]
for pid,name in names.items():
    content=(lib/'cases'/name).read_text()
    for m in re.finditer(r'^- (E-[A-Z]+-\d{2})：\[([^]]+)\]\((https?://[^)]+)\)(.*)$',content,re.M):
        eid,title,url,tail=m.groups()
        record=dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',
            source_type='official_help',access_mode='official_index_text' if eid in index_only else 'opened_text',
            version_scope=scopes[pid],app_build=None,firmware_version=None,source_updated_on=None,
            locator=locators[eid],note=title+'；具体事实与限制见案例 '+pid,hands_on=False,visual_observed=False,
            archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
        if eid in {'E-KDL-01','E-PBK-01','E-MUD-01'}: record['source_type']='official_manual'
        if eid=='E-PBK-01':
            record.update(access_mode='official_pdf_local_text',firmware_version='manual path 6.10.2854; installed firmware unknown',
                raw_work_path='work/research/interaction-depth/pocketbook-6.10.2854.pdf',
                raw_sha256=hashlib.sha256((root/'work/research/interaction-depth/pocketbook-6.10.2854.pdf').read_bytes()).hexdigest())
        if eid=='E-SUP-05':record.update(source_type='official_changelog',firmware_version='Chauvet 3.24.38 restore change',source_updated_on='2026-10-23',qualifies=['E-SUP-04'])
        if eid=='E-MUD-12':record.update(source_type='official_announcement',release_state='preview_not_verified_released')
        if eid in {'E-MUD-11','E-MUD-14'}:record['source_type']='official_release_entry'
        evidence.append(record)
    for line in content.splitlines():
        m=re.match(r'\| (F-M\d{3}-\d{2}) (.*?) \| (.*?) \| (.*?) \| (.*?) \|$',line)
        if not m:continue
        fid,task,steps,branches,refs=m.groups()
        prefix=re.search(r'E-([A-Z]+)-',refs).group(1)
        ids=[]
        for part in re.split('[、，；;]',refs):
            mm=re.search(r'(?:E-[A-Z]+-)?(\d{2})',part)
            if mm:ids.append('E-'+prefix+'-'+mm.group(1))
        flows.append(dict(flow_id=fid,product_id=pid,task=task,depth='D2',evidence_ids=';'.join(ids),
            known_branches=branches,gaps='见案例的范围/分支/后续观察；未实测',
            visual_status='static_reference_only' if fid in {'F-M003-01','F-M003-05'} else 'not_observed',
            hands_on='false',case_path='cases/'+name,checked_on='2026-10-26'))
pburl=next(e['url'] for e in evidence if e['evidence_id']=='E-PBK-01')
for num,page in [(4,13),(5,32)]:
    p=lib/f'artifacts/M003-manual-p{page}.png'
    evidence.append(dict(evidence_id=f'E-PBK-{num:02}',product_id='M-003',title=f'Official manual static page {page}',url=pburl,
        checked_on='2026-10-26',source_type='official_manual_figure',access_mode='local_pdf_render_viewed',version_scope=scopes['M-003'],
        app_build=None,firmware_version='manual path 6.10.2854',source_updated_on=None,
        locator=f'Printed/PDF page {page}; static figure only',note='Actual static-page observation; no device operation or continuous flow',
        hands_on=False,visual_observed=True,archived_artifact=str(p.relative_to(lib)),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),
        capture_state='static_page_excerpt_archived',supersedes=None,derived_from='E-PBK-01'))
assert len(evidence)==35,len(evidence)
assert len(flows)==27,len(flows)
(out/'reader-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evidence))
with (out/'reader-coverage.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(flows[0]));w.writeheader();w.writerows(flows)
print({'records':len(evidence),'flows':len(flows)})
