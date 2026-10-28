# M010 Palma 2：版本、静态观察及材料来源

查阅/观察2026-10-27。未操作设备或App，无连续流程、帧时间轴或耗时。Palma 2黑白Android13保持原对象；不借Palma 2 Pro/Palma 3的手写、SIM或其他配置。机上固件及NeoReader构建仍未知。

## 原件与版本

- E-PALMA-01/02：原URL普通GET200再取，分别为Palma 2产品页/2024-10-22发布稿。只采本机正文，相关商品不采；宣传图不是当前固件画面保证。
- E-PALMA-15：原零售商URL取得103页PDF，SHA256 `66a194e882c852677433b96a8d5ac128adf9f9cfb652f73da6738d7f6f937808`。封面为Mobile ePaper Series Universal User Manual V4.0，作者元数据ONYX INTERNATIONAL INC.，创建/修改日2024-12-10；元数据不是发布日期。文件名含Palma 2 Pro不证明适用Pro或Palma 2。p2–3有版本差异说明，p94的Android11密码说法也不能套Android13。
- 通用帮助沿用旧官方公开JSON：work/eink-systems-research/boox-articles.json；PC分类完整索引沿用work/boox-controls-deepening/epaper-pc-articles.json（59篇、14节、next_page=null）。本轮未请求API、账户接口或全站重抓。目录完整只指这份已存分类索引/PDF书签，不是Palma 2功能全集。

## 实际静态观察

四张PDF摘页均由本轮原PDF渲染并实际view_image查看。所看页没有印刷页号，以下全用1-based PDF页序，未建立印刷页码对应。图片保存在本目录；不能把任意两页拼作连续执行过程。

| 依据 | 摘页 | 只观察到什么 | 限制 |
| --- | --- | --- | --- |
| E-PLCD-04 ← E-PALMA-15 | [封面](mobile-v4-p001.png)，PDF p1 | 系列通用V4.0署名 | 不是机型/已装固件证明 |
| 同上 | [Smart Button](mobile-v4-p086.png)，PDF p86 | 三种按法；图中短按Full refresh、双击AI Assistant、长按None | 示例值；没有进入动作、选择列表或按下结果 |
| 同上 | [Power](mobile-v4-p095.png)，PDF p95；正文p94–95 | 休眠后Wi-Fi、蓝牙、音频三项分开；示例前两项Turn On、音频Pause，自动休眠5分钟、自动关机Never | 未打开下级选项；未证明默认值。正文给睡眠后蓝牙在线播放的条件，未验证TTS、断点或恢复 |
| 同上 | [System Navigation](mobile-v4-p097.png)，PDF p97 | Gestures/Navigation Bar、More Settings、Side Gestures及Screenshots入口 | 未触发、无返回状态观察 |
| E-PLCD-03 ← E-PALMA-01 | [本机官方宣传图](palma2-smart-button.jpg) | Smart Button三种按法设置，三项显示None；旁列可用功能示意 | 本机宣传静态图，与V4.0例值不同不构成固件变化证明 |

根代理另实际查看Power摘页和本机宣传图，并复核PDF p1/86/94/95/97文本。此复核不另增E，不替代缺失的实机过程。

## 文字补证和排除

- E-PLCD-01：通用Troubleshooting，仅用第三方App把音量键用于翻页、USB从充电切File Transfer的问答。Notes回收站等其他问答不借给Palma 2。
- E-PLCD-02与既有E-BXCD-14：同名Sleep and Battery分属两类指南，自动休眠设置入口分别为Power和Display；保留差异，不给Palma 2拼成统一入口。
- E-BXCD-06与E-PALMA-13：Stay active、冻结、自启动是不同控制。资料未给它们与Power三项的优先级；开保活不等于音频恢复保证。
- E-PALMA-09/10/14/16本地复读：只补文字批注导出、书签反馈、文件接收和同步/备份条件；手写和Notes子流程不纳入。

历史论坛“Palma 2手册”条目及其独立附件普通GET均404，停止；搜索索引中的历史答复仅作发现线索，没有当作当前可读官方手册或员工身份证明。两条失败URL及哈希见work/captures.jsonl。没有换域重试、登录、提交、安装、下载固件/APK/音视频。

本批0新F；旧任务的通用文档分支仍有条件适用。听书暂停/返回后的句子、闹钟/蓝牙断连、强退/重启、文件同名冲突与恢复成功均未知。正式路径和哈希见work/formal-manifest.json与HANDOFF。
