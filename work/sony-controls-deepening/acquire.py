from pathlib import Path
from urllib.request import Request,urlopen
from urllib.error import HTTPError
import hashlib,json,time
root=Path('work/sony-controls-deepening');base='https://helpguide.sony.net/dmp/1301/v1/en/'
pages=[('01-index','index.html'),('02-playback-methods','contents/TP1000733513.html'),('03-lyric-file','contents/TP1000733592.html'),('04-display-lyrics','contents/TP1000733549.html'),('05-usb-dac','contents/TP1000729844.html'),('06-dac-screen','contents/TP1000731800.html'),('07-microsd','contents/TP1000733525.html'),('08-format-card','contents/TP1000733595.html'),('09-restart','contents/TP1000733532.html'),('10-format-player','contents/TP1000733530.html'),('11-export-pc','contents/TP1000733563.html'),('12-library','contents/TP1000733528.html'),('13-queue','contents/TP1000733551.html'),('14-bluetooth-first','contents/TP1000733596.html'),('15-bluetooth-quality','contents/TP1000733244.html'),('16-streaming','contents/TP1000735013.html'),('17-playback-screen','contents/TP1000733553.html'),('18-settings','contents/TP1000733588.html')]
manifest=[]
for name,rel in pages:
 url=base+rel;dest=root/'raw'/f'{name}.html'
 try:
  with urlopen(Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=35) as response:
   data=response.read();status=response.status;final=response.url
  dest.write_bytes(data)
  row={'name':name,'url':url,'final_url':final,'http_status':status,'path':str(dest),'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'fetched_on':'2026-10-27'}
 except HTTPError as e:
  row={'name':name,'url':url,'http_status':e.code,'error':str(e)}
  manifest.append(row);print(json.dumps(row));break
 except Exception as e:
  row={'name':name,'url':url,'error':str(e)};manifest.append(row);print(json.dumps(row));break
 manifest.append(row);print(json.dumps(row))
(root/'source-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
