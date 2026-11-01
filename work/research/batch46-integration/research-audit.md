# Batch46取证与执行审计

上一goal turn为progress：Batch45正式12新增/20修改，10D2与原任务/来源补证已验收。本批继续全65组范围中的Sony功能目录缺口，没有缩小整体目标或标完成。

root先读Batch45 HANDOFF及当前目录/案例/缺口，用现有82行Sony台账选择此前directory_only条目。没有重启已失败worker路由或为同一失败任务换模型；本批取证、材料筛选、组装和核查均由root完成。

实际读取work/sony-controls-deepening/extracted/manual-pages.json的11–20、25–37、53–65、71–86、92、107–108、113–114、126–127页，共59页；用旧PyMuPDF读取work/sony-controls-deepening/raw/guide-existing.pdf，渲染27/29/32/53/56/57/113页，全部实际view_image查看。没有安装工具或整册复制；来源和哈希见body-review-scope.json、rendered-pages.json和正式observed-pages.json。

web.run打开四篇Sony官方正文：TP1000733590、TP1000733557、TP1000735087、TP1000733547；普通urllib HTTPS获取均200，保存原HTML和正文。没有挑战页、401/403/412/429或绕过重试；未下载外部图片/视频或执行产品操作。四页与PDF对应规则核对，一般JS提示不影响正文已取得；未把HTML和PDF当独立互证。http-manifest.json记录URL、状态、原件/正文哈希，未将HTTP元数据当发布日期。

integrate_once.py、documents_once.py各执行一次，退出0；merge_batches.py/check_docs.py随后退出0。正式改动仅研究文档、索引与静态素材。现有要求、AGENTS及原始证据不改；未创建产品代码/工具链，未进行任何账户、下载音乐、安装App、更新或恢复动作。原目录39行更新保留ID/标题/URL/原页码，36行原仅目录获得正文。

最终检查还包括固定PDF和提取页一致性、四HTTP正文/原件与证据哈希、七图原件/归档一致、RP引用及版本/未知字段、任务与案例ID、原目录变更范围、46简报链接与需求哈希。检查只证明文档一致，不证明设备行为；结果见root-final-verification.json/final-check.json/final-manifest.json。
