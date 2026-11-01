from pathlib import Path
import json,hashlib,difflib
R=Path('/home/workspace/reader');W=R/'work/xiaoyuan-hardware-controls-deepening';L=R/'docs/research/interaction-library'
queries=[
'"小猿学练机" "S6" "使用指南"','"小猿学练机" "S6" "错题" "重做"','"小猿" "E1" "说明书"','site:xiaoyuanyingjian.com "帮助"',
'"小猿彩墨学练机E1" "体验"','"小猿学练机S6" "提交" "错题"','site:yuanfudao.com "学练机" "教程"','site:xiaoyuanyingjian.com "S6"',
'"小猿" "E1" "2024" "体验" -site:sohu.com -site:jd.com','"小猿学练机" "S6" "教程" -site:sohu.com -site:jd.com -site:douyin.com -site:php.cn','"小猿学练机" "S6" "说明书" -site:sohu.com -site:jd.com','"小猿学练机" "E1" "返回" -site:sohu.com -site:jd.com',
'"小猿彩墨" "体验" "2025" -site:sohu.com -site:jd.com -site:smzdm.com -site:douyin.com','site:post.smzdm.com "小猿" "E1" "识字"','site:ithome.com "小猿" "E1"','site:bilibili.com/read "小猿学练机" "S6"',
'"小猿学练机" "S6" "我" "体验" -site:sohu.com -site:jd.com -site:douyin.com -site:php.cn','"小猿彩墨" "E1" "实测" -site:sohu.com -site:jd.com -site:douyin.com','"小猿学练机S6" "设置" "体验" -site:sohu.com -site:jd.com','"小猿" "E1" "开箱" "识字" -site:sohu.com -site:douyin.com']
record={'checked_on':'2026-10-27','method':'web search_query in five groups of four; final three candidate open calls; search is discovery only, not observed UI','queries':queries,'usable_new_operation_evidence':0,'new_images_observed':0,'videos_watched_or_downloaded':0,'candidates':[
{'url':'https://post.smzdm.com/p/anvdke93/','discovered_as':'同型号体验标题，索引正文含多来源转述','open_result':'web tool 400 Timeout fetching','action':'停止，不从摘要建立作者实操事实'},
{'url':'https://post.smzdm.com/p/anvonl3p/','discovered_as':'同型号测评标题，索引正文含多来源转述','open_result':'web tool 400 Timeout fetching','action':'停止，不从摘要建立作者实操事实'},
{'url':'https://zhuanlan.zhihu.com/p/2028848047052826615','discovered_as':'S6自述体验候选','open_result':'web tool 403 Forbidden','action':'停止该路线；作者/构建/原图未知'},
{'url':'https://www.douyin.com/video/7673430477051584998','discovered_as':'S6错题复盘视频索引','open_result':'not opened','action':'未观看/下载，不用标题或摘要补流程'},
{'url':'https://www.bilibili.com/video/BV1TVPNz9EhW/','discovered_as':'S6体验视频索引','open_result':'not opened','action':'未观看/下载，不记视觉观察'},
{'url':'https://www.bilibili.com/video/BV1nQ8Y6rEMr/','discovered_as':'多型号固件/维修索引','open_result':'not opened','action':'不下载固件；不作UI流程'},
{'url':'https://www.sohu.com/a/1080322261_122471764','discovered_as':'对比/购物型索引','open_result':'not opened','action':'未核定作者一手实拍，不采用连对/按钮说法'},
{'url':'https://www.sohu.com/a/1065061780_122645060','discovered_as':'E1购物型索引','open_result':'not opened','action':'不从汇编移植开放系统或拍照等说法'},
{'url':'https://www.9ji.com/product/490153.html','discovered_as':'S6包装清单含产品说明书','open_result':'not opened','action':'下一类取证线索；无手册正文，不新增E'},
{'url':'https://www1.xinhuanet.com/tech/20251219/cffba35ac6b34825b9b07caf4fbfa8f3/c.html','discovered_as':'同名新一代S6新闻索引','open_result':'not opened','action':'提示代际核对路径；不据摘要改已有官网规格/功能'},
{'url':'https://epaper.cqcb.com/attachment/202412/21/973294db-4876-4111-aac7-31f435831fed.pdf','discovered_as':'彩墨产品上市新闻索引','open_result':'not opened','action':'未下载；无操作链，不追加营销来源'}],
'limitations':'检索未命中和访问失败不证明产品无功能；停止相同失败路线。没有请求登录、账号或学习接口。'}
(W/'discovery.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
case=L/'cases/M-056-xiaoyuan-hardware.md'
(W/'case.diff').write_text(''.join(difflib.unified_diff((W/'case-before.md').read_text().splitlines(True),case.read_text().splitlines(True),fromfile='case-before.md',tofile='cases/M-056-xiaoyuan-hardware.md')))
print('Saved discovery.json (20 queries, 11 candidate records) and case.diff')
