# Batch41：新闻阅读与数据保留

2026-10-27。正式编辑和根校验已完成；不要重跑 integrate_once.py / prose_once.py。目标仍 active，65组均部分覆盖，不能标全功能完成。

- 新增46E：E-INRS-01～42、E-GNRS-01～04。新D2仅F-M044-17邮件源生命周期；16旧F补证，旧深度/操作等级未变。
- 总计929F：834D2、77D1、18D4；1631E / 1463 distinct URLs。221静态E（含既有1合成）、19浏览器E；本批13幅官方静态图属4个E，无新实操。
- RP由26到38，旧26行未变。12新增细节围绕阅读/归档、网页预览/保存、过滤记录/历史范围、隐藏、批注、离线、邮件源及音频清队列。
- 245 Markdown / 1958 local links，现有merge/check exit0，errors[]。根额外核46原件散列、RP/附件/编号与旧深度；requirements SHA不变：e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087。
- 47正式文件变化：24新、23改，无删除；范围/散列在changes.json、final-manifest.json，根结论在root-verification.json，统计在final-check.json。

## 入口与事实边界

正式简报 docs/briefs/041-news-reading-and-retention.md；原问题检索 docs/research/interaction-library/design-reference-guide.md；交互依据表 reference-points.csv。新材料在artifacts/inoreader-reading-state与ground-source-scope。

Inoreader：原38目录正文就地标已读，42新来源并非42新功能。12幅图均由根查看；普通RSS历史保留不套Newsletter。邮件取消帮助说删地址/失去历史访问，功能页说90天保留可重关注，尚未统一访问/后台保留/版本含义。2024过滤图30天提示与当前新文章说明可能是创建/编辑/后续处理差别，不能断言冲突已解决或当前行为。归档图0%是可见选择状态，无实际结果。2024阅读博客含2025-06图；发布日期不作构建，博客日期改放source_published_on，source_updated_on未知。

Ground：4篇原目录已读，仍21F；明确联网浏览，原语言不同于翻译/中文UI。套餐文仍写Vantage移动即将推出，不当现行权益保证。概述三评级机构不消除旧方法论差异。只有套餐概览图，没有操作。

两站抓取器最初因全局reCAPTCHA/CSP误判挑战，根/worker从已保存正常正文核正；没有真实挑战绕过，前两页未重复请求。原误判及纠正均留work/正式来源记录。先前Ground产品域403、Khan403仍不重试。

## 后续可推进方向

本批取得的是细节文档与静态证据，没有关闭取消/失败/恢复、同一当前构建实操及儿童效果缺口。继续由已知未读目录或可匿名操作的缺失分支推进；不要重复同一短帮助或按按钮增F。后续应把全库其余任务逐渐补入问题检索层，当前38行不是全库状态机。没有账号/付费/安装/对外发信；没有启动完整系统设计。

Worker batch41_ground_inventory 独立本地验证已通过，报告validation.md；根已读取并复核最终47文件散列一致，没有实质问题，不因校验通过升级研究深度。无持续抓取/浏览器进程。
