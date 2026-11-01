# Batch54 handoff

2026-10-27。第54批已完成；总目标“完成目前已知产品的详细分析”保持active，全部65组仍partial。没有证据表明当前必须等待用户或已完成全产品分析，不应标complete/blocked。研究阶段未改变。

## 正式结果

- 最新简报：`docs/briefs/054-media-permissions-and-study-content.md`。
- 主表987F：888D2 / 79D1 / 20D4；1753E / 1581不同URL；230静态E / 21浏览器E；165RP、22列。
- M-021现32F，31D2 / 1D1；新F24～32，F32移动首页主链不足保持D1，无App/账户/图像或实际操作。
- 新23E为E-QZMS-01～23；原目录剩余21正文+两关联正文，包含两篇无独立F的背景文章。
- 原4分组+64文章均已关联正文；新69/70为linked_article，父行11/28，不当原官方章节增加。原目录不覆盖Account/Billing/Teaching/Schools全部类目。
- 2旧F（04/05）原位补证，3旧E（QZCD-01/02/04）追加目录采集；13RP（M02118～30）。
- 5新正式文件、17修改、0删除；requirements SHA仍为e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087，AGENTS未改。

## 已执行，不要重跑

`integrate_once.py`和`documents_once.py`均已完整应用。前者的变更日志分别为old-flow-updates.json、old-evidence-updates.json、inventory-updates.json。案例与简报经apply_patch更新。后续只能增量编辑；不得重复追加E、捕获或RP。

`python3 work/research/interaction-depth/merge_batches.py`退出0。
`python3 work/research/interaction-depth/check_docs.py`退出0：279MD、2353本地链接，errors=[]；结果final-check.json。
`git diff --check`退出0（正式资料很多未跟踪，另以manifest核验）。
`python3 work/research/batch54-integration/verify_root.py`退出0：核23篇身份/选定正文语句/重定向、全部165RP派生值、旧F/E改动、原64与关联2条目录、来源与包文件SHA、需求不变、正式文件差异。root-final-verification.json与final-manifest.json已生成。
这些检查只证明资料一致性，不证明产品行为或学习效果。中途只读schema探测误用inventory的title字段，KeyError退出1；实际字段functional_domain，修正读取后无数据变更，正式核验首次通过。

## 关键区别与后续缺口

- 网站录音松开保存并回放单段，整集仍有Create/Done；不能承诺局部/整集数据的实际持久化时点。手机主要学已有录音；图片卡面范围按平台保留。
- 免费图库两侧概述/释义步骤措辞差异；图示格式原文MBP未擅改；旧录音故障页有Win7/8步骤，非当前系统实测。
- Editable by和Visible to入口分开；Just me说明含private，不推导两字段联动；分享公开/密码集同页措辞差异保留。
- 班级申请须接受后访问私有资料，误退重加不保证恢复进度；Groups与Friends两文档路径分别引用，关系/版本替换未知。
- 评分可改不可删、不可关闭、作者不收通知；Top creator不是准确率认证。
- 首页全球13+、偏美国大学场景，学校设置只列13国学校；首页问答、推荐控制和活动进出/返回尚不完整，D1。不能套5–6岁儿童。
- 下一步可查其他产品的未读目录或有公开可操作的明确缺口；不要为了数量反复重读Quizlet原64篇，也不要把正文覆盖写为完整交互覆盖。

## Helpers

`batch54_quizlet_inventory`只读21行原目录。自身“目录仅报告”断言撞上根并发写入而失败，已纠正；根接管该路后读取来源并最终验证，没有模型级联。
`batch54_reference_check`只读问题索引、新摘记、新F和13RP；提示RP26跨批来源，根核E-QZAS-04并在指南直接链接旧摘记，也补个人搜索来源。请求中误写reuse-points.csv，正确为reference-points.csv，rg无匹配退出1属路径发现。无任何文件变化；无需等待这两个已完成helper。

## 磁盘

仅余约数MB；禁止大型抓取、安装或重复截图。此前安全清理86个可再生pyc共9,338,113字节，位置仅为work/research/tools/pymupdf/pymupdf/__pycache__及numpy/_core/tests/__pycache__，源py及所有其他包文件SHA已核不变。详情cache-cleanup.json。不要删除唯一证据或浏览器profile整体。verify_root不导入这些包，因此未重建缓存。
