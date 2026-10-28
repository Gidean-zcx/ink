from pathlib import Path
exec(Path('work/huawei-controls-deepening/fetch.py').read_text().split('urls=')[0])
urls={}
for n in ['15879068','15879066','15879070','15879069','15892864','15879072','15879074']:
 urls['help-'+n+'.html']='https://consumer.huawei.com/cn/support/content/zh-cn'+n+'/'
urls['support-product.js']='https://consumer.huawei.com/etc/designs/huawei-cbg-site/clientlib-support-v3/multichannel-product-detail.min.a85cb92293989c016b91455efb79d525.js'
exec(Path('work/huawei-controls-deepening/fetch.py').read_text().split('def fetch(it):')[1].split('with concurrent.futures')[0].join(['def fetch(it):','']))
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as p:res=list(p.map(fetch,urls.items()))
(out/'fetch-extra-log.json').write_text(json.dumps(res,ensure_ascii=False,indent=2)+'\n')
for r in res:print(r['file'],r.get('status',r.get('error')),r.get('article_body'),r.get('final_url'))
