from pathlib import Path
import sys,json,hashlib
sys.path.insert(0,str(Path('work/research/tools/pymupdf').resolve()))
import fitz
pdf=Path('work/hifi-extension-research/DX180userguide.pdf');doc=fitz.open(pdf)
w=Path('work/ibasso-controls-deepening')
(w/'manual-metadata.json').write_text(json.dumps({'raw_work_path':str(pdf),'raw_sha256':hashlib.sha256(pdf.read_bytes()).hexdigest(),'pages':len(doc),'metadata':doc.metadata,'checked_on':'2026-10-27','network_refetched':False},ensure_ascii=False,indent=2)+'\n')
texts=[];images=[]
for i,page in enumerate(doc):texts.append({'pdf_page':i+1,'printed_page':i,'text':page.get_text()})
(w/'manual-text.json').write_text(json.dumps(texts,ensure_ascii=False,indent=2)+'\n')
for printed in [17,18,20,21,34,42]:
 p=w/f'dx180-printed-{printed}.png';doc[printed].get_pixmap(matrix=fitz.Matrix(1.4,1.4)).save(p);images.append({'printed_page':printed,'pdf_page':printed+1,'raw_work_path':str(p),'raw_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'observed':False})
(w/'render-manifest.json').write_text(json.dumps(images,indent=2)+'\n');print('pages',len(doc),'rendered',len(images),'PDFsha',hashlib.sha256(pdf.read_bytes()).hexdigest())
