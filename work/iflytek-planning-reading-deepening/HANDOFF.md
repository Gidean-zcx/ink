# 第28批冻结交接

2026-10-27，根工作完成。权威简报 docs/briefs/028-reading-plans-and-interaction-evidence.md；全局审计 docs/research/interaction-library/verification-log.md 最后一节。

65组/全部partial，857F=764D2+78D1+15D4；1411E/1243不同URL；194静态视觉E、18网页操作E。本批11新D2/20E、4旧F补证、4旧E追加。70已链接CSV/6946行含历史与父子项，不是完成率。目标仍active，未进入系统设计，requirements SHA不变。

- M027 E-IFPR-01–04，F19–24；四条教程、58试采位置已看、35正式帧+1加载中的目录图，四视频来自T30Pro目录但实际固件未知。原70目录更新4行，两批共8条功能视频抽样，余26功能/24问题视频未看。浏览器49692关闭exit0。
- M004 work/boox-reading-controls-deepening已冻结，13E/5F，4静态图；旧F09/10、旧E-BOOX-16/E-BXCD-01追加已合。助手manifest只代表合入前；本批当前文件以根formal-manifest为准。
- M029 work/ihuman-learning-controls-deepening已冻结，2作者E/0F、36目录；旧F01/02、旧E-IHCD-01追加已合。直接原站412/验证码与其他失败未重试；作者正文只有web阅读记录而无原站全文快照。
- root另 work/ihuman-video-preview，1E/0F，F01仍D1。公开视频四采样、两正式帧，旧四环节剪辑不能当当前五环节教学链。浏览器脚本exit0完成，未存MP4/听音。助手最初binary失败不当网站拒绝，root单次接管完成。

一次性build_batch.py、merge_reviewed.py、update_summaries.py均已执行，严禁重跑。merge-result.json、coverage-update-applied.json保留精确前后行；正式文件清单formal-manifest.json。verify_batch.py是只读检查及刷新work检查结果，可在后续正式文件未改前使用；新批变更后其总数断言会过期。全局merge_batches.py可重复，check_docs.py可重复。

中途大量图工具输出被截断，之后逐小组实际看过所有试采；没有将未显示的首次输出冒充观察。最后修正M027当前四/八视频措辞、去重目录链接、接回分析状态表格，不改变E原件散列。42正式新增图像=37视频帧+4BOOX静态图+1型号目录图。

root自己核统计不同步、擦除/页删除作用对象与图片；worker仅做只读文件/ID/SHA/链接检查，全部exit0/state_changed=false。没有新产品操作、D3连续链或学习效果验证。用户要求按功能深挖仍未全部实现，不能因为批次入库标goal complete。

最终检查：verify_batch.py PASS/exit0；check_docs.py exit0，197 Markdown/1391本地链接/errors=[]；70 CSV/6946行，目录重复链接已去除；git diff --check exit0（未跟踪文件仅辅助）。详见final-checks.json。
