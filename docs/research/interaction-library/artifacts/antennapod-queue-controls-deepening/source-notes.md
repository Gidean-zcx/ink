# AntennaPod：队列、过滤与下载请求的证据范围

2026-10-27。本批严格静态研究3.12.3 / commit 175e5ef8fe3e125a5014d70ef0c57cf85661c71b；未安装或运行。3篇官方帮助不指定app构建；6个新源码原件及复用的SubscriptionFragment均按旧官方tree的Git blob哈希核对。只读这10个来源，未扩展到下游下载服务、数据库写入、过滤器调用方或新的备份/计时源码。

## 帮助与版本

| 来源 | 本轮支持 | 限定 |
| --- | --- | --- |
| E-APQD-01 Multiple queues | 当前帮助明确不支持多队列。 | 不据文章标题虚造建队列流程；未核未来版本或未合并issue。 |
| E-APQD-02 Subscription tags | 单播客齿轮→Tags是赋标签入口；标签不能生成独立队列。 | 未取得命名/保存/删除确认全链；本批新F仅研究已有标签浏览。 |
| E-APQD-03 Refreshing podcasts | 全局刷新周期/关闭，单播客Keep Updated；关闭后不参与自动及Queue/Episodes发起的批量刷新，仍可单独刷新。 | 页面称默认12小时，未确认3.12.3真实初始化值；刷新目录不等于媒体下载。 |

## 源码定点结论

| 证据/文件 | 已读入口与变化 | 明确缺口 |
| --- | --- | --- |
| E-APQD-04 QueueFragment | 锁定警告有取消及不再提示；Keep Sorted开启时隐藏锁菜单。排序选择回调即存偏好/请求重排，Random关闭Keep Sorted，换回非随机可在当前对话框恢复。拖动先改内存，结束时才请求DB移动。清队列先确认。 | 不证明排序/清空可撤回；未读DBWriter本批相关方法，锁定不是防删除保证；当前播放项固定规则未知。 |
| E-APQD-05 EpisodeMultiSelectActionHandler | 入队只选有媒体且未标队列的项；下载按传入列表顺序请求且排除已下载/本地源。删除对已下载/本地媒体调用删除方法，下载中未完成项则调用取消；移出Inbox把New改Unplayed。 | 调用方的数量提示由分支计数产生，不等待逐项成功；单项提示被此helper略过。本地文件警告只见调用，文案/确认未读。并发/失败回滚未知。 |
| E-APQD-06 ItemFilterDialog | 选项变化立即调用onFilterChanged；确认仅dismiss；Reset先传空集再清选中。 | 这是抽象控件；未读当前屏幕调用方，不能保证每处入口、保存范围或关闭即回滚。本批不据此新增完整过滤任务。 |
| E-APQD-07 FeedItemFilterQuery | 各维度AND；Unplayed包含New；Paused按position>0；Downloaded看下载时间字段；所有订阅状态都未选时回落Subscribed。 | SQL状态不检查物理文件存在；字段不是实时播放状态，不证明当前UI已选了这些条件。 |
| E-APQD-08 DownloadLogAdapter | 成功项不显示重试；失败项之前的列表记录若有同类型/ID成功也隐藏。点重试先隐藏按钮，再取源/媒体；有对象才请求刷新或下载，找不到只写日志。 | 未读日志列表的排序提供方，严格时间先后未核；按钮消失/下载中提示不等于成功。没有核自动退避和服务内网络/权限条件。 |
| E-APQD-09 DownloadLogDetailsDialog | OK退出，复制全部详情或点击URL/技术原因复制；源存在且允许才显示前往播客，已订阅开本机页，其他状态开在线页。 | 旧帮助的长按URL在浏览器打开与此点击复制不同；长按行为本批未核。原对象缺失可保留日志标题，URL仍unknown；加载异常只日志。 |
| E-APCD-13追加：SubscriptionFragment | 选已有标签重载订阅；普通订阅页onPause存last_tag，归档从root开始。标签不在已过滤结果时退回root；结果数量下降时结束多选。只有存在实际自定义标签才显示标签条。 | root仍是当前过滤条件下集合，不保证展示全库；赋标签及长按管理下游未读。onPause保存不是异常终止保证；空库与加载失败区别未实测。 |

多选入口的长按/点击细节属于未读adapter，不能补造。Queue中的多选动作发出后结束选择模式；订阅页委托批量handler，选择≤1才结束，具体批量实现未读。归档页隐藏部分菜单属于静态条件，不保证当前所有Android布局。

无图像观察、无连续流程、hands_on=false、visual_observed=false。官方帮助正文经web读取并普通HTTP归档，6个源码GET均200；本地复用没有重新请求。没有设备/账户动作、剪贴板复制、媒体下载、外部发送或代码执行。网页帮助并非源码版本互证；完整定位和散列见本批E、read-ranges.json及manifest。
