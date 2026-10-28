# TIME K–1整期样刊：公开网页操作审计

证据 **E-TKSM-01**；任务 **F-M047-06**。2026-10-27，匿名Chromium156.0.8078.4，Linux/headless，初始1280×950；只在状态20改为390×844。不是触屏、儿童使用、原生App或Plus实测，没有实际听音。

## 入口与记录

[TIME教育页](https://www.timeforkids.com/educators/)公开K–1样刊链接指向[May 8, 2026样刊](https://reader.emagazines.com/issue/time-for-kids-k-1/may-8-2026?ipt=c8869aa3-8f3d-4699-b112-1d4a6270a953)。ipt参数直接来自该公开链接，不是猜测、登录凭据或绕过权限。首次正常导航HTTP200；纸刊封面标May 2026/Oceans/Vol.16/No.28，四页。浏览器页面标题、实际URL和状态时间见[23个状态](browser/observed-states.json)；[25条命令](browser/trace-actions.json)包含检查/截图，不是25次用户操作。

正式保留15张已判读画面；其他原始PNG/HTML/逐状态JSON在work，散列见[原件目录](browser/raw-capture-manifest.json)。只执行可见控件点击、下拉选择、正常导航/刷新和浏览器关闭重启；DOM只读检查控件，不注入答案或存储。文中“播放”仅指界面运行状态。

## 动作、反馈与结束

| 状态 | 动作与观察 | 能支持什么及限制 |
| --- | --- | --- |
| 00 | 另查2026-10-16 G3《Packed with Care》文章；正文有朗读/西语/打印，本次未见级别选择器 | 只是这一页的阴性观察，不证明全站不支持分级；不计新增任务 |
| 01 | 公开样刊初次进入封面，1/4 | [封面](browser/01-k1-sample-initial.png)，明确有限一期，不等于账户书库 |
| 02–05 | Next两次并开菜单；02/03短时空白，URL和提取文字一度仍为p2；等待后05显示印刷第3页，菜单位置仍2/4 | [稳定画面](browser/05-menu-settled.png)优先；03文件名last-page4只是预期，实际不是第4页。不能仅凭动作返回或URL报告成功；未量化延迟或确诊原因 |
| 06–08 | 菜单Go to last page到印刷第4页；Save my place给本设备下次返回的成功提示；再点Next仍4/4 | [末页](browser/06-last-page-command.png)与[保存提示](browser/07-save-place-dialog.png)。未见积分或理解记录；点击末页不等于完成阅读。08只记录页码/URL没有变化 |
| 09 | 正常刷新当前纸刊页 | [仍在第4页](browser/09-saved-reload.png)，单次刷新本身不能排除URL的影响 |
| 10–11 | 菜单Mobile view打开新标签；10仍是原纸刊，切换至新标签才得到重排正文 | [新标签](browser/11-mobile-new-tab.png)是Saving Sea Turtles。纸刊#p4在重排为#p3，目录包括封面和两篇文章；不是同一页码单位 |
| 12–13 | Listen展开后时间推进、段落高亮/自动滚动；Pause在0:26；选择1.5x | [暂停画面](browser/13-paused-speed.png)。六档.75/1/1.25/1.5/1.75/2x，实际仅选1.5x；12截图短时0:00/0:00而稍后文字2:02，不能当加载失败；未核声音或对齐精度 |
| 14–15 | 关闭音频工具栏，再Listen | [重开0:28/1.5x、暂停图标](browser/15-audio-reopened.png)，从原位置附近自动继续；没有保留暂停意图。关闭期间是否持续运行没有单独采样，不做推断 |
| 16–17 | 在0:53/1.5x暂停，刷新页面后再Listen | [刷新前](browser/16-paused-before-reload.png)→[刷新后0:01/1x](browser/17-listen-after-reload.png)；本次重建页面没有恢复秒点/倍速；文章身份仍同一篇 |
| 18–20 | 关播放器→CONTENTS→选Meet Sea Turtles；随后改窄视口 | [目录](browser/18-contents.png)→[选文](browser/19-article-selected.png)→[窄版](browser/20-narrow-reflow.png)。按文章跳转并重新排文字/图片，不是把整张纸页缩小；没有模拟触摸或查所有窄屏控件 |
| 21–22 | 回原纸刊标签→Go to first page，实际回封面；关闭整个持久浏览器上下文，再以相同资料目录启动浏览器，访问不含文章路径/页锚的官方样刊入口 | [关闭前封面](browser/21-paper-cover-with-saved-page4.png)→[重启回第4页](browser/22-browser-restart-base-entry.png)。支持本次显式保存页跨浏览器进程恢复；最后浏览封面未覆盖它。不证明账户同步、其他设备、清缓存、重装、刷机或音频续播 |

## 失败与记录边界

命令数组索引19（从0计数）把目录标题当link定位而超时；其实际role为heading，随后经可见标题正常点击成功。索引22重复截图名被脚本拒绝，已有21未覆盖。两者属于研究操作错误，保留原日志，不是产品错误。

[网络摘要](browser/network-statuses.json)只记录挂接后的document/xhr/fetch，共75条：32个200、43个204；其中未见401/403/412/429。Mobile view新标签首次加载发生在监听挂接之前，因此不是完整网络审计。旧文章00有一条Load error，见[浏览器错误](browser/browser-errors.json)，未定位来源，不把它归因于样刊音频。后续新标签/重启并非所有事件种类均重挂；无日志不证明无错误。

body.inner_text及部分控件记录可能包含屏幕外/非当前文章的内容；本次以截图和可见标题判断当前文章。没有以DOM中的多篇文字当作同时可见，也没有把URL锚点直接等同印刷页码。音频元素查询返回空，未据此判断播放不存在。

未操作搜索、下载/打印、缩放、分享、订阅/账户、全部年级和其他期刊；未测试离线、故障重试、媒体结束后的自动下一篇、跨成员、跨设备或恢复出厂。没有填写成人年龄门。外部状态仅匿名阅读遥测与该浏览器的保存页状态；未创建账号、发送消息、购买或下载音频。浏览器已正常关闭（退出0）。
