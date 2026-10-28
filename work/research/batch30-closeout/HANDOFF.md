# Batch30 冻结交接

2026-10-27。整体goal“完成目前已知产品的详细分析”仍active；全部65组partial。用户要求逐功能交互与可反复查阅依据，尚未满足全产品全功能完成。未新增已确认需求、原生App/设备操作或系统实现。

本批已完成并合并：

- M057 work/meebook-reading-controls-deepening：5E、1D2、4旧F/12字段、20目录；manifest 7项。所有一次acquire/fetch_images/build已经运行，不能重跑。
- M056 work/xiaoyuan-camera-preview：1派生E、0F、1旧F/4字段、五正式视频摘帧；manifest 11项。worker失败capture-result.json与root成功root-capture-result.json分别保留；root_capture_once.py/build_once.py已执行，不能重跑。
- M035 work/hiby-library-controls-deepening：0E、1D2、4旧F/11字段、4旧E追加、145原ID目录快照。formal-manifest 5项/raw-manifest 8项。已冻结，勿重跑precheck。
- M018 work/bbdc-learning-controls-deepening：3E、0F、2旧F/8字段、9项增量目录、五正式历史图。frozen-manifest.sha256 46项；已冻结，勿重跑precheck/write-batch。

本目录merge_helpers_once.py已应用一次，helpers-merged.json为成功标记，premerge/存受影响旧批。主表merge_batches.py已经重建。verify_batch.py为只读可复核，77条manifest、11旧F/35字段、4旧E及76CSV/7306行引用PASS。需求散列未变。总计860F（767D2/78D1/15D4）、1434E、1266URL、198静态E、18浏览器E。

简报：docs/briefs/030-learning-stages-and-device-state.md。README/库入口/目录/比较/缺口/ideas/analysis-status/verification-log已更新。新增data-dictionary.md仅解释现有字段与已实施校验，没有实现新产品schema。design-reference-guide.md的10问题各补摘记与原来源；template.md不再把hands_on统称人类亲测。全部正式文件检查见final-summary.json。

root已验证关键事实：

- 不背5正式图均实际view_image；2017拼写放弃/开始、2021作者流程/版本与静态题面分开，四次/五次不拼算法。M018两旧主任务仍D1。
- 智慧拍八抽样点均实际查看，数学与作文是独立片段；五帧正式，其余work。M056仍5D1/1D2，非同题、非连续、非产品操作；未听音/保存MP4。
- M7作者报告、KOReader固定版本问题与一般Wiki分开；图片DNS失败、Reddit普通GET403停，不补造截图或实机兼容。
- HiBy歌词和返回有文字路径，无缝/编码条件分别，未说明返回后声音/位置，不推断恢复成功。

辅助路线：audio_interactions与vocab_interactions已完成冻结；batch28_validation导航只读审查完成，下一次manifest任务工具400失败，已停止该路线，root接管验证完成，不需要再次派同一核查。先前dragonbox_controls工具不可用路线继续不重试。当前没有存活浏览器/长进程。

后续研究按现有backlog补能改变设计判断的同题错误/提示/结果/退出恢复；M008汉王、M022 Mini/Max仍薄弱，已知受限路线不得重复绕过。同源旧文件复读沿原E、同任务沿原F，静态抽样不升D3。若无公开材料则如实保留缺口，不用新链接或任务数量冒充完成。
