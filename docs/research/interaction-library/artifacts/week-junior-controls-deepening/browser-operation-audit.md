# M048公开样刊：浏览器操作核查

观察日：2026-10-26。E-WJCD-02、E-WJCD-03、E-WJCD-04；F-M048-05、F-M048-06、F-M048-07。

这是根代理用Playwright控制真实网页并逐张查看截图的记录。`hands_on=true`只表示自动化网页实际操作，`operation_method=automated_public_browser`；不是人在设备上的手感试用，也不是手机App实测。D4按这些有限任务登记，不表示全部成功分支、故障或整个产品已完成。

环境：Linux，Chromium Headless Shell 156.0.8078.4，1200×900视口，匿名临时上下文，正常联网。Python安装分发版本为Playwright 1.64.1；trace内部版本字段为1.64.0-beta-1791568591000，分别保留。网站未给前端构建号。内容为美国版2026-10-23样刊、340期、界面32页；仅观察所列版面，没有逐页读完一期。

入口由[官网样刊按钮](https://theweekjunior.com/in-the-magazine)经`ema.gs/WiL`导向[公开阅读器](https://reader.emagazines.com/issue/the-week-junior/october-23-2026?ipt=F0826FC3-4129-46CF-A5F3-FF70A931F1FF)；URL中的ipt来自公开样刊链接，没有猜测其他资源或绕过订阅。未登录、订阅、投票、投稿、分享或听取音频。

## 实际序列与失败

| 截图 | 动作 | 观察结果 |
| --- | --- | --- |
| [01-initial](browser/01-initial.png) | 打开官网公开样刊链接 | 新上下文从封面1/32开始 |
| [02-next](browser/02-next.png) | 点击Next page | 页码变2，显示对应新闻版面 |
| [03-thumbnails](browser/03-thumbnails.png) | 点击Show page thumbnails | 侧栏及底部横向缩略图展开，显示封面和成对页范围 |
| [04-after-close](browser/04-after-close.png) | 点击缩略图右上关闭；随后寻找折叠状态下的页码输入框超时 | 留在第2页；自动定位失败，不算成功跳转 |
| [05-search-open](browser/05-search-open.png) | 隐藏Toggle navigation定位超时后，点击可见搜索图标(40,38) | 侧栏展开，底部页码输入框可见 |
| [06-jump14](browser/06-jump14.png) | 在可见pageNumberInput填14并按Enter | 页码14和对应科学版面，URL为#p14 |
| [07-search-nobel](browser/07-search-nobel.png) | 搜索框输入Nobel并Enter | 5个结果组，含页码、计数、缩略图及标亮摘要；当前页仍14 |
| [08-result15](browser/08-result15.png) | 点击结果Page 15，等待1秒后捕获 | 结果项高亮，但页图、页码与URL仍为14；未观察到目标页打开 |
| [09-search-empty](browser/09-search-empty.png) | 输入zzqzxq-no-such-term并Enter | 结果数0，阅读页仍14 |
| [10-save-place](browser/10-save-place.png) | Clear search；点击Save my place | 出现书签成功和本设备下次访问回此页的提示 |
| [11-front](browser/11-front.png) | 点击Go to first page | 回封面，页码1，未重新保存书签 |
| [12-reopen-same-context](browser/12-reopen-same-context.png) | 同一浏览器上下文重新打开无页码的公开期刊入口 | 回第14页；一次实测支持手动书签优先于之后浏览到的封面 |
| [13-new-context](browser/13-new-context.png) | 同一浏览器新建空上下文，打开同一入口 | 从封面1/32开始；没有复制任何原上下文状态 |
| [14-last-page](browser/14-last-page.png) | 回到原上下文点击Go to last page | 32/32和封底；之后Previous page被展开侧栏拦截而超时，未产生回31/再前32截图 |

两个隐藏控件定位各超时30秒，末尾Previous page点击因侧栏拦截超时5秒；这些是本次自动操作的失败，不能直接推定普通用户必然遇到产品缺陷。未强制点击、未直接调用站点内部跳页函数。Page 15结果未跳转只记录本次1秒等待后的状态，原因未定，也没有将其补写成成功。

## 复查方法与判断边界

1. 用上述视口、新浏览器上下文打开公开入口；点Next，再点Thumbs并关闭。点击左上搜索图标展开侧栏，在可见页码框填14并Enter。
2. 输入Nobel，观察结果数量及页码组；点击Page 15后核对主图、页码、URL三者。再输入无匹配字符串，最后清空。结果随期刊入口更新可能改变，复查须固定同一期。
3. 在14页点Save my place，等成功提示退去，回封面；重新打开无页码的期刊入口。另建空上下文打开相同入口，对比是否回到14。
4. 末页入口只证明可以定位32页；本次没有“全部阅读完成”的判定或结算反馈。

本轮主序列执行1次，另有1个空上下文对照。没有重复统计稳定性、测延迟、离线、浏览器进程重启、清除站点数据或跨设备同步。书签在同一上下文重访有效、空上下文从封面开始；底层是cookie、localStorage还是其他存储没有核查，因此不宣称永久本地保存或支持账号同步。

正式保留[观察状态与截图散列](browser/observed-states.json)、[主操作日志](browser/trace-actions.json)、[新上下文日志](browser/fresh-context-trace-actions.json)。原始trace含网络/页面快照，仅留work；[原件散列与路径](browser/raw-trace-manifest.json)不冒充原件已正式归档。正式资料足以复查动作和可见结果，不把这些静态截帧当独立来源，也没有宣称已归档连续视频。
