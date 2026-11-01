from pathlib import Path
import json,shutil
ROOT=Path('/home/workspace/reader');WORK=ROOT/'work/boox-controls-deepening';p=ROOT/'docs/research/interaction-library/cases/M-004-boox-neoreader.md'
s=p.read_text();assert '## 本轮控制路径补强' not in s
shutil.copyfile(p,WORK/'case-before.md')
s=s.replace('目录覆盖主要任务域，**未核定该构建的完整功能总数**。手册入口需本机 Settings → User Manual 或 send2boox 登录，本轮未登录取得机型专用最新手册。[E-BOOX-19]', '''本轮逐条登记公开 **User Guide (ePaper Tablet PCs)** 的 **14 节、59 篇**，见[目录 CSV](/home/workspace/reader/docs/research/interaction-library/artifacts/boox-controls-deepening/functional-inventory.csv)。API 分页已结束，条目数与各节一致；“59 篇”是公开帮助文章数，不是本机完整功能数或已验证任务数。Notes／阅读数据有同名导向文章，保留文章 ID，不人为合并计数。[E-BXCD-01]

目录完整性仅限这个公开分类；**V4.1.2 专用整册仍未取得**。官方仍要求 Settings → User Manual 或登录对应 send2boox 服务器按机型下载，未登录；搜索找到的商品聚合“手册”和 FCC 单页不当操作手册。[E-BOOX-19；E-BXCD-02] 相机扫描文章只列其他具体型号；InkSpire、Work Profile 的本机适用未核；Talk Insights 等后续能力不倒填到固定基线。下表补公共目录未逐项研究的范围。

| 公开目录节 | 篇数 | 当前覆盖边界 |
| --- | --- | --- |
| General Settings | 11 | 账户／更新已查；语言输入、连接、睡眠、显示、前灯、音量、鼠标、投屏等仅部分条件或目录 |
| Interface and Navigation | 2 | F02；桌面布局、Dock／小组件未完整研究 |
| Optimize Speed and Display | 3 | F03；对比度／颜色完整参数未研究 |
| Apps | 8 | 优化／管理条件见 F03、14；安装卸载仅入口 D1；浏览器、PushRead、Calendar Memo、Work Profile 未形成完整任务 |
| File Transfer | 3 | F04 及原第三方云分支；手机传输、异常重试未完整 |
| Split Screen | 2 | F08；帮助型号例外单列 |
| Reading | 16 | F05–08、11–12、14 的局部；版式、翻页区、状态栏、PDF/EPUB排版、浮动栏、Dashboard等仍有目录级缺口 |
| Note-Taking | 4 | F09–10、13；Text Notes、Tag、全部新工具未完整研究 |
| Stylus | 2 | 校准／InkSpire仅目录及适用性核查缺口 |
| Data Syncing and Backup | 2 | F11–13，仍缺冲突／覆盖恢复实测 |
| Keyboard Cover | 2 | 配件连接及快捷键未研究，不能从平板功能推配件条件 |
| Camera (Smart Scanner) | 1 | 其他型号专属文章，不合并到 Note Air5 C |
| Screenshot and Screen Recording | 2 | 未实际录屏；本轮只读静态图 |
| Lab Features | 1 | 后续版本入口，不纳入 V4.1.2 操作结论 |''')

u={int(x['flow_id'].split('-')[-1]):x['replacement_row'] for x in json.loads((WORK/'old-coverage-updates.json').read_text())}
actions={
2:'系统栏右侧打开控制中心、左侧通知中心；Edit 管理快捷项。Settings → System Navigation 配置 Navigation Bar／Bottom Navigation／Side Gestures；隐藏导航栏先底部上滑调出。导航球双击回桌面。',
3:'控制中心 → EinkWise；NeoReader 中央菜单 → Refresh。第三方 app 内 E Ink Center → App Optimization，或桌面长按 app → Optimize；按问题调整 DPI／全刷／方向，支持恢复原 DPI、关闭 Master Switch 或 Reset All。',
6:'长按选段标记，Contents → Annotations／Handwriting 管理导出。补充：Progress → Blank Page 可在 PDF 加空白页；可从 Preview 或书签旁入口删除新增页。',
7:'中央菜单 → Progress，用页码／滑条／章跳转；Prev View／Next View 逐次前后返回。Contents 可回目录／书签，书签图标有独立标记状态。',
8:'中央菜单 → Split View → Handwritten/Text Notes 建关联笔记。Current Doc 另为同文档两窗。控制中心 Split Screen → Swap Sides／Exit；系统分屏开 Notes 则独立，返 Home 也是系统分屏退出途径。',
9:'Notes 书写／擦除／Undo/Redo。Exit 保存后回 Notepad View；More → Save 是另一个手动保存入口。Share and Export 输出 PNG/PDF/.note；More → Page- 删除当前页。',
14:'NeoReader → Progress → TTS；系统 TTS Output 配引擎。按问题分别检查 App Optimization → Others → Stay active，以及 Settings → Apps & Notifications 的权限／通知／App Startup／Freeze。系统更新仍先备份。'
}
lines=[]
for line in s.splitlines():
 if line.startswith('| F-M004-'):
  cells=line.split('|');n=int(cells[1].strip().split('-')[-1])
  if n in u:
   r=u[n];cells[4]=' '+actions[n]+' ';cells[5]=' '+r['known_branches']+'。 ';cells[6]=' '+r['evidence_ids'].replace(';','；')+' ';cells[7]=' '+r['gaps']+'。 ';line='|'.join(cells)
 lines.append(line)
s='\n'.join(lines)+'\n'
addition='''## 本轮控制路径补强

本轮不新增 F，集中补原编号；下述路径属于**当前通用帮助层面的 D2**，不是 V4.1.2 实机确认。帮助里的设备例图、默认值与型号名单必须连同限制阅读。[E-BXCD-01–14]

**F02：区分面板、返回与保存。** 通知中心 Clear All 是通知列表动作，不是删笔记；侧边内滑返回默认未启用，需要先配置。导航球在控制中心缺失时，可 Edit 后拖入快捷项。文档内返回历史见 F07，退出 Notes 见 F09；双击导航球回 Home，资料没有说明是否同时结束进程或持久保存。关闭面板的确切触摸位置、打断选区后是否保留尚未观察。[E-BXCD-03]

**F03：把显示问题和撤回范围说清楚。** 当前 ePaper PC 帮助默认右下上滑进 EinkWise；另一 E Ink Tablet 分类的同名页写左下。均提醒可自定义，不将两者拼成 Note Air5 C 的统一默认。[E-BXCD-04] 通用刷新页列 HD／Speed／Smooth；NeoReader Refresh 帮助却列 Regal／Speed。记录两种上下文，不把名称硬对齐或声称本机同时具有所有模式。[E-BXCD-05、12]

第三方界面被裁切时，优化帮助建议降低 DPI；过小时提高，也能选原 DPI。**Master Switch 暂停整套优化；单 app Reset All 重置配置；Apps & Notifications → Reset all app optimization 涉及所有 app**，不是同一恢复范围。方向强制跟随系统仍错乱时，帮助要求关闭该选项；Cloud Config 不保证每个 app 都有配置。未取得各开关即时生效、重启或卸载后的保存规则。[E-BXCD-06、07] 对 NeoReader，Apply to All Documents 区分当前文档／全部文档；Full Refresh 的 Deep 选项、触发频率与一次手动 Full Refresh 也分开。静态图中的开关值只是图示状态。[E-BXCD-12]

**F08／09：回读原文与退出保存。** Current Doc 明确两窗口手写、标记、书签在页面刷新后同步；这句话不能外推到“独立 Notes 与原书每笔同步”。关联笔记、另一个文件及两个 app 分屏要分别建模。系统分屏横向为左右窗、纵向为上下窗；未兼容 app 不能强推成功。帮助的 Flexible Split Screen 型号名单只列 Note Air3 C／Note Air3／Tab Ultra C Pro，未列 Note Air5 C，因此该项本机适用仍未知，不能反向断言不支持。[E-BXCD-09、10]

Notes 的 Exit 保存并回笔记列表，More → Save 手动保存，Page- 不可撤销。旧表把“More → Save 或 Exit”并写，本轮分开入口。Home、切 app、系统回收与断电时的写入完成点仍没证据；不以“自动保存”概括所有退出方式。[E-BXCD-13]

**F14：后台问题不是一个开关。** 权限、通知、自启动、优化中的 Stay active、Freeze 与睡眠连接策略分别存在；冻结阻止后台操作／通知。没有文档给出这些开关冲突时的优先级，也不能由 Stay active 推断 TTS 在锁屏、失去音频焦点或蓝牙断开后自动续播。Sleep and Battery 帮助属于通用 Tab 系列，列睡眠连接延迟设置，但 Note Air5 C V4.1.2 的入口与联网时序未核。[E-BXCD-06、07、14]

安装／卸载与数据保留仅达到以下范围，**不新计一条完整 D2 流程**：[E-BXCD-07、08]

| 生命周期节点 | 可核查入口／声明 | 尚缺的状态与失败分支 |
| --- | --- | --- |
| 安装 | 官方列 Google Play 和浏览器／电脑传入 APK；产品页确认本机内置 Google Play | 来源许可、安装确认、被拒／签名或版本冲突、安装后首启；没有下载 APK 或登录 |
| 权限与通知 | Settings → Apps & Notifications 内分别管理 | 拒绝后应用提示、重新授予后的恢复、后台通知到达均未观察 |
| 文件默认打开 | 同页 Default app | 取消默认、仅一次／总是选择、原文件与关联数据如何跟随未说明 |
| 卸载 | 同页 Application 可管理或卸载 | 确认文案、书籍下载／账号／偏好／优化配置去留、重装恢复均未知 |
| 数据保护 | F12 的 .booxc 是书库及阅读数据；F13 是 Notes 同步 | 不能把两者称为所有第三方 app 的卸载恢复包 |

'''
s=s.replace('## 分支、错误与版本冲突',addition+'## 分支、错误与版本冲突')
s=s.replace('只读官方正文及 Zendesk 公共 API 原文；没有操作设备、测耗时、观看连续视频或把产品图算成操作记录。菜单中的 AI/智能词不能证明 G-002 的全系统数据和动作可访问。', '''只读官方正文及公开 API 原文，并实际查看四张共用帮助静态图：控制中心、App Optimization 的 Display 页、Apps & Notifications、NeoReader Refresh。**图中型号／固件不明，未识别为 Note Air5 C，不能证明 V4.1.2 的布局或默认值**。全程 hands_on=false，无连续视觉流程、D4 或耗时测量；菜单中的 AI/智能词也不能证明 G-002 全系统访问。

静态图与取证摘要见[本批资料](/home/workspace/reader/docs/research/interaction-library/artifacts/boox-controls-deepening/source-notes.md)。前三图是共用设备示意，第四图可见 Regal／Speed、Normal／Deep、频率与 Apply to All Documents。只记录控件存在，不推断点击效果。正式归档使用相对资料库路径；API 原件在 work，以 raw_work_path/raw_sha256 记录。''')
s=s.replace('每步记录对象 ID、页锚、文件及回退结果。','每步记录对象 ID、页锚、文件及回退结果。另选一个布局裁切的第三方 app，比较原 DPI、Master Switch、单 app 重置与全 app 重置；用可丢弃数据观察卸载后内容及配置去留。当前只完成资料补强。')
s+='''
本轮 E-BXCD-01–14 为同一官方来源体系的目录／正文快照和静态取证，不因新编号算独立交叉验证；详细来源、定位与归档见新批 evidence JSONL 和上面的取证摘要。

| 新 E | 官方来源／定位 |
| --- | --- |
| E-BXCD-01 | [公开指南目录](https://booxsupport.zendesk.com/hc/en-us/categories/38115205868180-User-Guide-ePaper-Tablet-PCs)，14节59篇，按API逐条登记。 |
| E-BXCD-02 | [手册下载](https://booxsupport.zendesk.com/hc/en-us/articles/8569203926804-User-Manual-Download)，机内／登录网站两入口。 |
| E-BXCD-03 | [Navigate Your Device](https://booxsupport.zendesk.com/hc/en-us/articles/25213043150996-Navigate-Your-Device)，控制／通知中心、导航栏、手势及导航球。 |
| E-BXCD-04 | [EinkWise](https://booxsupport.zendesk.com/hc/en-us/articles/10701257505044-EinkWise)，本分类默认右下；另一分类[同名页](https://booxsupport.zendesk.com/hc/en-us/articles/10990010331156-EinkWise)写左下，只用已有公开快照比较。 |
| E-BXCD-05 | [Refresh Modes](https://booxsupport.zendesk.com/hc/en-us/articles/10701257029780-Refresh-Modes)，当前通用模式／全刷。 |
| E-BXCD-06 | [App Optimization](https://booxsupport.zendesk.com/hc/en-us/articles/10701299044372-App-Optimization)，DPI／Master Switch／Reset All／Others。 |
| E-BXCD-07 | [App Management](https://booxsupport.zendesk.com/hc/en-us/articles/10701262170644-App-Management-Settings)，权限／通知／启动／冻结／全app重置。 |
| E-BXCD-08 | [安装渠道](https://booxsupport.zendesk.com/hc/en-us/articles/10701308914964-Download-and-Install-Apps)，仅渠道声明。 |
| E-BXCD-09 | [系统分屏](https://booxsupport.zendesk.com/hc/en-us/articles/10701452686484-System-Split-Screen)，API正文；web读取一次Internal Error后未重试。 |
| E-BXCD-10 | [阅读器分屏](https://booxsupport.zendesk.com/hc/en-us/articles/10701493183764-Split-View)，Current Doc、关联Notes与退出。 |
| E-BXCD-11 | [Book Progress](https://booxsupport.zendesk.com/hc/en-us/articles/10701495414804-Book-Progress)，视图历史／PDF空白页。 |
| E-BXCD-12 | [NeoReader刷新](https://booxsupport.zendesk.com/hc/en-us/articles/10701492644372-Refresh-Settings)，当前／全部文档范围。 |
| E-BXCD-13 | [Handwritten Notes](https://booxsupport.zendesk.com/hc/en-us/articles/10701578837268-Handwritten-Notes)，Exit及More的Save/Page-，不整页倒填版本。 |
| E-BXCD-14 | [Sleep and Battery](https://booxsupport.zendesk.com/hc/en-us/articles/10701171762452-Sleep-and-Battery)，通用睡眠连接条件。 |
'''
p.write_text(s)
print('Updated M004 only; preserved 14 flow identifiers; added no flows.')
