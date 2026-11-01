from pathlib import Path
import json, hashlib, urllib.request, urllib.parse, urllib.error
from concurrent.futures import ThreadPoolExecutor
W=Path('/home/workspace/reader/work/kindle-audio-family-deepening')
assert not (W/'captures.json').exists(), 'one-time capture'
base='https://d1ergij2b6wmg5.cloudfront.net/kug/kindle_scribe_2024/v1/en-US/'
jobs=[('guide.html',base+'html/kug.html'),
 ('ai.html','https://digprjsurvey.amazon.com/csad/help/node/TZNl60K13ehpn0s1O0'),
 ('child.html','https://digprjsurvey.amazon.co.uk/csad/help/node/G27XUUGLQ5TG84XC'),
 ('assistive.html','https://digprjsurvey.amazon.co.uk/csad/help/node/TdHCSXXqsdhaVKt3Tc')]
for name,remote in [('home.png','chrome_en.png'),('reading.png','KPP Chrome.png'),('portrait.png','Portrait.png'),('landscape.png','Landscape.png'),('pageflip.png','KPP PFV.png'),('themes.png','themes.png'),('writing.png','toolbar.PNG'),('audible.png','audibleplayer.png'),('progress.png','reader_chrome_bottom_1.png'),('assistive-controls.png','Assistive_Reader_2_Scribe.png'),('assistive-setting.png','Assistive_Reader_1_Scribe.png')]:
 jobs.append((name,base+'image/'+urllib.parse.quote(remote)))
def get(job):
 name,url=job
 out=dict(name=name,url=url,checked_on='2026-10-27',access_mode='public_GET',hands_on=False,visual_observed=False)
 try:
  with urllib.request.urlopen(url,timeout=25) as r:
   b=r.read(); p=W/'raw'/name; p.write_bytes(b)
   out.update(status=r.status,content_type=r.headers.get('Content-Type'),last_modified=r.headers.get('Last-Modified'),raw_work_path=str(p.relative_to(W.parents[1])),raw_sha256=hashlib.sha256(b).hexdigest(),bytes=len(b))
 except Exception as e:out.update(error=str(e))
 return out
with ThreadPoolExecutor(max_workers=4) as ex: rows=list(ex.map(get,jobs))
(W/'captures.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(rows,ensure_ascii=False,indent=2))
