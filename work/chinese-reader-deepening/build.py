from pathlib import Path
import csv, json, hashlib, shutil

root=Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
work=root/'work/chinese-reader-deepening'
art=lib/'artifacts/chinese-reader-deepening'
art.mkdir(exist_ok=True)
for name in ['tango2-cover.jpg','tango2-overview.jpg','inkpalm-plus-a.png','inkpalm-plus-b.png','inkpalm-plus-c.png']:
    shutil.copyfile(work/name,art/name)
for src,target in [('ireader.md','M-053-ireader-hardware.md'),('moaan.md','M-054-moaan.md')]:
    s=(work/src).read_text()
    s=s.replace('FsF-7MceezhW8dzHaKROQEl.jpg','FsF-7MceezhW-9HxW8dzHaKROQEl.jpg').replace('原正确URL与归档以原证据索引为准。','详情20；原静态观察及归档保留。')
    s=s.replace('https://www.moaansmart.com/', 'http://www.moaansmart.com/')
    s=s.replace('官网记录用HTTPS规范URL；本轮实际成功获取为同地址HTTP，HTTPS失败不是正文已核读。','官网记录保留实际读取成功的HTTP原址；HTTPS失败不是正文已核读。')
    (lib/'cases'/target).write_text(s)

ev=[]
def add(id,p,title,url,source,access,scope,locator,note,date=None,fw=None,artifact=None):
    d=dict(evidence_id=id,product_id=p,title=title,url=url,checked_on='2026-10-26',source_type=source,access_mode=access,version_scope=scope,app_build=None,firmware_version=fw,source_updated_on=date,locator=locator,note=note,hands_on=False,visual_observed=bool(artifact),archived_artifact=f'artifacts/chinese-reader-deepening/{artifact}' if artifact else None,sha256=hashlib.sha256((art/artifact).read_bytes()).hexdigest() if artifact else None,capture_state='selected_static_image' if artifact else 'text_only',supersedes=None)
    ev.append(d)
add('E-IRD-01','M-053','掌阅官方SmartOS2.5更新简介与适配名单','https://www.bilibili.com/video/BV1v7f7YhEGn/','official_publisher_release_description','search_index_description_text','Smart4 Pro / SmartOS 2.5.0 historical release; other models not extrapolated','OCR、AI速读、笔记目录/同步、升级及型号名单','官方账号简介的搜索索引；直接正文412；非视频观察。原结果work/chinese-reader-deepening/ireader-2.5-search-result.txt。Ocean4长续航版未标2025，不外推。','2025-01-24','2.5.0')
add('E-IRD-02','M-053','Tango2系列认证店型号与图片目录','https://detail.youzan.com/show/goods?alias=36489xybpq8pfjz&from_source=gbox_seo','certified_brand_store','opened_text','Tango 2 series; configuration and firmware unknown','标题及店铺微信认证/官方旗舰店标识','系列商品页；不据共享图片把所有功能归于每个配置。')
for id,name,url,title,loc in [('E-IRD-03','tango2-cover.jpg','https://img01.yzcdn.cn/upload_files/2025/10/15/Fgpc1ircRrX5ieoOC_Nvyk-drM4I.jpg','Tango2封面与机身侧键','详情第1图'),('E-IRD-04','tango2-overview.jpg','https://img01.yzcdn.cn/upload_files/2025/10/15/FsUtGYW9nh-ONoxBPQUPH7bVCC_p.jpg','Tango2系列桌面入口示意','详情第2图')]:
    add(id,'M-053',title,url,'certified_brand_store_image','static_image_observed','Tango 2 series; firmware unknown',loc,'2026-10-26实际查看单帧宣传图；不是操作序列，不证明打开成功；系列示意未细分配置。',artifact=name)
add('E-IRD-05','M-053','掌阅官方2022笔记导出教程简介','https://www.bilibili.com/video/BV1xt4y1K78M/','official_publisher_tutorial_description','search_index_description_text','iReader hardware model unspecified; historical tutorial','简介导出渠道目录','只作历史导出功能目录；不据此确认Smart4 Pro/Ocean2025/Tango2当前菜单。','2022-11-06')
add('E-IRD-06','M-053','掌阅官方SmartOS2.4更新简介','https://www.bilibili.com/video/BV1DQSKYKECB/','official_publisher_release_description','search_index_description_text','SmartOS2.4; supported model list not established','字体/云同步/第三方应用适配条目','缺机型名单；保留目录线索，不用来补固定机型D2。','2024-11-20','2.4')
scope='Moaan legacy guide dated 2020-06-08; hardware model and firmware unspecified; not attributed to W7/W8/inkPalm'
add('E-MOD-01','M-054','墨案官网导航与用户指南目录','https://www.moaansmart.com/?list=15','official_site_navigation','http_original_text_via_urllib','Moaan product families; model-specific firmware unknown','产品导航、用户指南、公司页脚','HTTPS失败；同址HTTP200，原件work/chinese-reader-deepening/moaan-guide-index.html；产品目录不等于型号化操作手册。')
add('E-MOD-02','M-054','墨案官网电子说明书入口与访问缺口','https://www.moaansmart.com/?list=16','official_support_index','http_original_text_via_urllib','Moaan intelligent e-paper; model unspecified','常见问题的电子说明书链接','HTTP正文可读；目标https://pdf.maitube.com/pdf/?e=ag1Qt51lF/gq6返回403，停止。没有取得目标手册正文。')
guides=[('03',86,'三种文件传输方式','USB、FTP、多看WLAN','USB写books，FTP写book，保留冲突；乱码建议仅历史兼容说明。原件work/chinese-reader-deepening/moaan-transfer-2020.html。'),('04',87,'蓝牙接收书籍','内部存储books、通知栏打开及加入书架','接收文件与加入书架不同状态；机型未知。'),('05',88,'文件移动','文件管理长按、复制/剪切、目标目录粘贴','没有覆盖/取消/中断行为；机型未知。'),('06',89,'安装第三方应用','USB至Apps分类；浏览器至download','未验证渠道现状/安装权限；不保证任意APK兼容。'),('07',85,'PDF批注裁边重排','阅读中点击空白区后的底栏','只给入口；GIF未观看，不补造后续动作。'),('08',84,'笔记重命名','新建保存命名、左上菜单改名','旧界面型号未标；没有同名/取消分支。'),('09',83,'笔记缩略图预览','多页笔记的页码与4/9格','没有核实缩略图选择后的返回栈。'),('10',82,'笔记导出PDF','保存笔记缩略图省略号、导出、Note目录','独立导出文件与可编辑笔记关系未说明；不套W7/W8。')]
for n,num,title,loc,note in guides:
    add('E-MOD-'+n,'M-054','墨案2020指南：'+title,f'https://www.moaansmart.com/?show={num}','official_legacy_user_guide','http_original_text_via_urllib',scope,loc,'规范URL为HTTPS；实际HTTP原址正文读取。'+note,'2020-06-08')
commit='ca9246421618d51dd1fe9ab9883e7c564a3dd471'
for id,x,title,loc in [('11','b','迷你阅Plus中文手册型号/参数图','01产品介绍、02参数及10生产者'),('12','c','迷你阅Plus中文手册功能/异常图','04功能、05异常处理'),('13','a','迷你阅Plus中文说明书封面','封面型号；07有害物质章节')]:
    add('E-MOD-'+id,'M-054',title,f'https://raw.githubusercontent.com/panicopticon/MoaanInkPalmPlusRoot/{commit}/manual/cn/{x}_cn.png','manufacturer_style_manual_community_archive','static_manual_image_observed','inkPalm Plus / Android 11; firmware build unknown',loc,'实际查看厂商样式中文图；社区保存，非官网托管。固定提交2025-01-26为仓库时间，非固件日期；三图同一手册，非独立验证。',artifact=f'inkpalm-plus-{x}.png')
add('E-MOD-14','M-054','inkPalm5原作者系统使用记录','https://post.smzdm.com/p/aqmg6o9k/','firsthand_user_report','opened_text','inkPalm5 / Android8.1; author build unspecified in 2022','墨案迷你阅inkpalm5——系统','只取该机段落；设置/应用/播放描述是作者当时观察，非官方验证；未看文内图片。','2022-12-11')
add('E-MOD-15','M-054','W7原作者借机记录','https://post.smzdm.com/p/a8559q7l/','firsthand_user_report','search_index_text','Moaan W7; firmware unknown; historical report','阅读、手写及导出功能描述','第三方借机记录，仅功能目录，不用于确定2020官网指南机型。','2021-06-01')
add('E-MOD-16','M-054','W8原作者上手文章','https://post.smzdm.com/p/akkm9evr/','firsthand_user_report','search_index_text','Moaan W8; firmware and exact publication date not pinned','笔记模板、笔端擦除、录音和导出段落','直接正文打开超时，读到搜索索引正文；未观察图片。作者版本能力不外推后续固件。')
add('E-MOD-17','M-054','墨案助手服务协议','https://platform.moaansmart.com/assistant-service-agreement.html','official_service_terms','opened_text','Moaan Assistant online service; device/app build unknown','服务范围与账号条款','只确认账号和在线存储/同步服务范围；不是操作帮助、备份成功或删除恢复保证。')
for x in ev:
    x['url']=x['url'].replace('https://www.moaansmart.com/', 'http://www.moaansmart.com/')
    x['note']=x['note'].replace('规范URL为HTTPS；实际HTTP原址正文读取。','实际HTTP原址正文读取；HTTPS失败。')
(lib/'batches/chinese-reader-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in ev))

fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
flows=[
('F-M053-06','M-053','Smart4 Pro 2.5.0按标题或页码定位笔记','D2','E-IRD-01','页列表/套索标题/页码入口','无标题、错字、删除标题和返回栈','cases/M-053-ireader-hardware.md'),
('F-M053-07','M-053','Smart4 Pro 2.5.0已同步笔记在手机查看','D2','E-IRD-01','手机掌阅App笔记本入口；已有同步内容为前提','设备上传完整链、账户、冲突、离线、编辑权限','cases/M-053-ireader-hardware.md'),
('F-M053-08','M-053','Smart4 Pro历史2.5.0更新检查入口','D2','E-IRD-01','联网检查；历史推送范围','安装确认、失败回滚、数据保留及当前推送版本','cases/M-053-ireader-hardware.md'),
('F-M054-06','M-054','墨案2020未标机型指南：传入与加入书架','D1','E-MOD-03;E-MOD-04','USB/FTP/WLAN/蓝牙；收到文件与加入书架分开','机型归属；books/book冲突、断传、重名、取消','cases/M-054-moaan.md'),
('F-M054-07','M-054','墨案历史文件整理/APK入口与inkPalm5应用菜单','D1','E-MOD-05;E-MOD-06;E-MOD-14','复制/剪切；APK传入或下载；用户报告单应用菜单','旧官网型号未标、权限失败、覆盖与卸载数据','cases/M-054-moaan.md'),
('F-M054-08','M-054','墨案2020未标机型指南：整理笔记并导出PDF','D1','E-MOD-08;E-MOD-09;E-MOD-10;E-MOD-15;E-MOD-16','命名、页预览、导出Note目录；W7/W8独立目录线索','机型归属、笔迹原稿/导出件、退出保存、覆盖与删除恢复','cases/M-054-moaan.md')]
with (lib/'batches/chinese-reader-deepening-coverage.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fields);w.writeheader()
    for id,p,task,depth,e,branches,gaps,path in flows:
        w.writerow(dict(zip(fields,[id,p,task,depth,e,branches,gaps,'none','false',path,'2026-10-26'])))
print(f'Wrote 2 cases, {len(ev)} E, {len(flows)} new F, 5 static artifacts. No master/old batch writes.')
