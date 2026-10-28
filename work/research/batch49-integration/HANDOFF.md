# Batch49 完成记录

2026-10-27，根目录`/home/workspace/reader`。上一轮分类progress，见turn-audit.json。本批完成AntennaPod筛选/标签/媒体删除/队列数据链补证、正式集成与检查；整体目标“完成目前已知产品的详细分析”保持active，65组全部partial。

## 当前结果

- 964F=866D2+78D1+20D4；1710E/1539不同主URL；229静态E（含历史1合成）/21浏览器E；114RP。
- M040原F02/03/05/13深化，F13扩展为整理标签/浏览恢复；新增F14全局或单播客筛选。M040共14D2、59E、8RP。没有编译/安装/运行AntennaPod，没有新视觉或D4。
- 新E-APST-01～14，各对应固定3.12.3提交的源码文件。E-APCD-03/13/14与E-APQD-04/05/06/07追加本地复读，合计9文件。
- 原队列inventory158行，更新12项、新增C28～31为162行；不是162功能。F/E原持有批次保持，原ID没有删除。
- 最新简报`docs/briefs/049-selection-confirmation-and-persistence.md`，两个README及49份简报目录、问题表、比较/待办/ideas均更新。requirements未变。

## 关键结论及边界

1. ItemFilterDialog选项变化即callback，Confirm只dismiss。全局调用方写SharedPreferences、页数重置1并重载；单播客调用方写该feed数据库过滤字段、事件重载。关闭不是撤回，其他SubscriptionsFilterDialog仍未读，不通用化。
2. 全局空条件回已订阅源，收藏额外包含所有源状态；单播客查询显式允许全部源状态但限feed。Downloaded/Paused依字段，不证明文件存在/实时播放。
3. TagSettingsDialog先改临时显示集合；OK加入输入框剩余文本并提交；Cancel无提交回调。空/重复/保留名不新增但不阻止OK提交当前列表。
4. 多订阅编辑仅显示共同标签，保存替换共同集合、保留独有标签，逐节目分别提交，不保证全批事务/部分失败恢复。原last_tag onPause和过滤回退条件保留。
5. 单集长按先上下文菜单，选Multi-select才启动；选择模式点击条目/副按钮切选，取选中项按列表顺序。未核旋转/重新加载恢复。
6. 所选含任一本地源时整组先警告，确认才调用批量删除；无本地源直接执行。数量在提交异步请求时累加，不等逐项完成；物理文件失败可仅日志并继续状态路径。
7. 清队列链只清队列表/发CLEARED更新UI，没有媒体删除或STOP调用；未读所有服务事件反应，不能保证一定续播。setQueue事务捕获SQL异常后调用方仍可能发事件，不用UI事件证明落盘。

固定提交`175e5ef8fe3e125a5014d70ef0c57cf85661c71b`。原件`work/research/batch49-antennapod-state`；正式`artifacts/antennapod-state-controls/{source-notes.md,source-audit.json}`。notes/audit均被E散列引用，修改须同步owner和主表。没有假定当前APK版本等于源码。

## 检查/脚本

- `integrate_once.py`和`documents_once.py`已执行，不得重跑追加。
- 可重跑merge_batches.py、check_docs.py、本批verify_root.py。
- root专检14新blob、9旧文件、7附加采集、M040任务唯一owner、114RP引用、162目录、49简报入口与requirements不变通过；最终数量/链接见final-check.json。
- before-manifest.json及final-manifest.json为正式前后SHA；root-final-verification.json列变化。正式预期5新/22改/0删（含验证日志）。不以git整体untracked替代文件检查。
- 初始核对266Markdown/2239链接；验证日志追加后的最终检查以final-check.json为准。

## 存储与helper

存储worker只读报告有效；root确认没有活Chromium后，仅删除`work/research/batch38-week/desktop-profile/Default/Cache`（83个再生缓存文件，26318084B）。保存各文件SHA和同profile其他文件SHA，删除后全部非Cache文件一致。原始资料/正式截图/浏览器阅读状态未删，旧Batch44归档仍在。cache-cleanup.json可回查，原Cache已不存在；不要误删其父profile。

DragonBox helper调用不存在的python（127），后来自己改python3；不再复用这条路线，root接手核对。helper提file(1)显示8页，root用已安装`work/research/tools/pymupdf`解析原件得到9页、SHA `def05eb635f225863d354b03913228a94896ca382aa50b4e3cf956239e451605`不变，故原正式9页正确。pdfinfo不存在，不安装工具。报告和root解析在本批work，未新增DragonBox F/E或升级玩法深度；旧403/YouTube登录路径不重试。

本批无运行中的网络/浏览器/exec，helpers已结束。磁盘清理后约24MB可用，仍须避免大下载或全树复制；不要碰工作区外数据。继续优先补影响设计的固定版本完整过程/取消/保存/恢复，而不是重复宣传或增加无意义功能ID。全系统设计仍待用户认可。
