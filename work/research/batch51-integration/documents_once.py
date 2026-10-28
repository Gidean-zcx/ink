from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch51-integration'
assert not (W/'DOCUMENTS_APPLIED').exists()
bullet='- [第五十一期：传出、同步与恢复的不同后果](/home/workspace/reader/docs/briefs/051-transfer-sync-and-recovery.md)：两代QUADERNO补连接、文件和恢复链；播放器旧安装照片分清包、确认与登录状态，新增13条设计参考依据。'
for p in [R/'README.md',L/'README.md']:
    ss=p.read_text().splitlines();idx=[i for i,s in enumerate(ss) if s.startswith('- [第五十期：')];assert len(idx)==1;ss[idx[0]]=bullet;p.write_text('\n'.join(ss)+'\n')
p=R/'docs/briefs/README.md';s=p.read_text();i=s.index('- [第五十期：');p.write_text(s[:i]+bullet+'\n'+s[i:])
def add(p,t):p.write_text(p.read_text().rstrip()+'\n\n'+t.strip()+'\n')
add(L/'inventories/README.md','第51批原[Gen.2目录](../batches/quaderno-gen2-ink-inventory.csv)更新38行、[Gen.3C目录](../batches/quaderno-controls-deepening-inventory.csv)更新25行，均只认所列正文页，不因父项起页已读而算整章。[Astell&Kern原目录](../batches/ak-directory-deepening-inventory.csv)170/171两项补正文复读及旧机静态图，兼容日期仍为2025-05-01；全部目录总行数保持不变。')
add(L/'backlog.md','''## 第51批后的文件与安装状态缺口

上轮列出的Gen.2 p35–39、85–92、110–113已核读；Gen.3C p38–46、89–94、103–105已补读/复读。下一层需同构建验证：初次USB配对的完整确认/拒绝，传入同名与中断半文件、重传编号/完整性，临时预览异常退出，模板删除对旧笔记的影响，文件移动后的关联，回收站阈值，旧备份导入后数据与进度，以及同步删除/重绑/部分失败。

Astell&Kern Open APP两篇正文现已重新保存，旧SP1000M安装确认照片也已看过；不再写本地完全缺少这些正文。SR35实际安装权限、失败与卸载/重装数据仍未闭合，兼容F07仍D1。M3U FAQ已读，不重复当未读项；下一增量应取得SR35具体放置/扫描/路径修复或实际错误记录。内部歌单、当前队列、文件、离线授权和秒点恢复仍分别检查。''')
add(L/'analysis-status.md','第51批两代QUADERNO复用原PDF核35页正文，新增5D2，补7旧任务；Astell&Kern复读2帮助正文和4旧机照片，补2旧任务、1派生静态E，兼容任务仍D1。4旧E追加采集、13新RP；三份原目录共65行就地更新，目录总行数不变。全部65组仍部分覆盖，没有新增实操，见[简报51](../../briefs/051-transfer-sync-and-recovery.md)。')
add(R/'docs/ideas.md','2026-10-27继续I-021/I-022：QUADERNO两代补连接目标、临时显示/传出、模板、备份同步与恢复后果；播放器旧安装照片区分包、确认和登录状态。原任务/目录就地补证，新增13条按设计问题取用的依据，见[第五十一期简报](/home/workspace/reader/docs/briefs/051-transfer-sync-and-recovery.md)。这些对AI执行和资料保护的启发保持建议，未确定账户/备份制度或进入全系统设计；65组仍部分覆盖。')
add(L/'verification-log.md','''## 第51批：两代文件生命周期与旧安装照片

2026-10-27。前轮属于有证据进展，当前正式文件与第50批final-manifest一致后保存新基线。root逐页核Gen.2 p35–39/85–92/110–113、Gen.3C p38–46/89–94/103–105；共35页选定正文，未看PDF图或执行PC/设备动作。两个原PDF的SHA与此前一致；本次页表不替代历史阅读记录。

worker只读盘点M-065既有资料并给精确页/状态，不改正式资料。root据缺口重新取得faq_974/1014正文，检查页内型号/日期；四幅公开原照片HTTP200并逐图查看后归档。网页工具对其中一图超时，直接公开GET成功；不是权限拒绝后的绕行。图像只有菜单、包列表、安装按钮和登录入口，不能提升为连续或实际操作。没有访问APK下载站、安装软件、账户或外部写入。

原目录更新时首个脚本的Unicode词边界漏匹配“印刷p”形式，root在核更新分布时发现并补齐25条Gen.3C记录；修正记录见work/research/batch51-integration/integration-corrections.json。另将备份同日文件名描述明确为追加编号，不写成加号。

根验证检查F/E/RP、四次追加采集及图像SHA、两代选页/型号范围、目录就地变更和需求不变；命令与结果在本批verify_root.py、root-final-verification.json。统一文件/链接检查使用work/research/interaction-depth/check_docs.py，结果见本批final-check.json。它们只核资料身份和一致性，不证明真实保存、安装、恢复或学习收益。目标保持进行中。
''')
(W/'DOCUMENTS_APPLIED').write_text('Applied once.\n')
print('Updated entry points and documented remaining scope; requirements unchanged.')
