# TIME for Kids：词汇卡与内容导航操作核查

观察2026-10-27（UTC日志为10月26日）；F-M047-01、F-M047-03；E-TKVD-01、E-TKVD-02。

Linux/Chromium Headless156.0.8078.4，1280×950，新匿名上下文；Playwright包1.64.1、trace内1.64.0-beta。网页构建未知。D4只表示此环境下实际操作与记录，不代表儿童实测、电子纸触控、全站或账户任务完成。

2018-02-16的G3–4文章用于词汇功能，K–1用于目录；不能把高年级词卡、配套2018测验或英语难度直接套给5–6岁儿童。没有提交出生日期、登录、分配课堂、购买、打印或下载音频文件。媒体只经正常网页原生控件启动；没有实际听音，不评发音/音质/内容准确性。

## 动作与状态

共31捕获，14张作实际视觉判读，12张必要截图归档；其余由当时DOM/动作日志核对，不能称31张都看过。截图和DOM取得有时间差，native计时文字不作为精确秒表。

| 捕获 | 动作 | 可核对结果及限制 |
| --- | --- | --- |
| 01-article（原件work） | 直接打开2018年G3–4公开文章 | HTTP200，未见可核定阅读级别选择器；URL的rl参数本身不证明切换 |
| 02-word-before（原件work） | 将首个预标词滚到视区 | 记录弹窗之前的正文位置；不是用户任意选词 |
| [03-consume-open](browser/03-consume-open.png) | 点击consume | 展开含音频、释义、词性、配图、例句及关闭键的卡片；卡片延伸到视区下方 |
| 04-after-escape（原件work） | 按Escape | DOM仍有一个可见词卡；未据此推所有键盘关闭均失败 |
| 05-word-audio-playing（原件work） | 点原生音频播放并等1.8秒 | 终端读取consume paused=false、时间约1.80秒；本帧未归档media字段，后用portrait保存完整对照 |
| 06-word-closed（原件work） | 点词卡X关闭 | 卡片消失；不是播放停止的证据 |
| 07-after-close-wait（原件work） | 关闭后等1.2秒 | 终端读取consume仍paused=false、约3.22秒；后续采用持久media状态复核 |
| 08-word-reopen（原件work） | 重新开consume | 操作间隔已使音频自然到结尾；不把结束归因于关闭 |
| [09-portrait-enter](browser/09-portrait-enter.png) | 关consume，对portrait按Enter | 键盘打开portrait词卡；不是通过全页Tab序列测试 |
| 10-portrait-click（原件work） | 再次点portrait | 词卡保持打开，未证明同词点击是开关 |
| [11-portrait-playing](browser/11-portrait-playing.png) | 点portrait音频播放，等1.2秒 | 可见暂停图标；随后DOM paused=false、time约1.36秒 |
| [12-portrait-closed-playing](browser/12-portrait-closed-playing.png) | 点X并等1秒 | 卡片消失，但portrait paused=false、time约2.58秒 |
| 13-replay-before-close（原件work） | 间隔后重新开始portrait并等0.7秒 | 第二次对照开始，paused=false、time约0.83秒 |
| 14-replay-after-close（原件work） | 关闭词卡并等0.7秒 | paused=false、time约1.88秒，关闭未停止底层媒体 |
| [15-reopen-during-play](browser/15-reopen-during-play.png) | 播放中重新打开词卡 | 仍paused=false、time约2.23秒，没有重头播放 |
| [16-word-paused](browser/16-word-paused.png) | 点原生暂停 | paused=true、time约2.24秒 |
| 17-outside-click（原件work） | 点卡片外标题 | 卡片仍可见；音频仍暂停在约2.24秒 |
| [18-reload-word-reset](browser/18-reload-word-reset.png) | 关卡、刷新，重新开portrait | paused=true、time=0；只证该匿名文章一次刷新结果，不推云端或账号规则 |
| 19-main-menu（原件work） | 关闭词卡，打开全局菜单 | 出现年级、主题、财商、成人区、资源和登录父入口 |
| [20-grade-options](browser/20-grade-options.png) | 展开年级入口 | 四个年级目录；它们不是单篇阅读难度选择 |
| 21-k1-list（原件work） | 点K–1目录 | 进入/k1/，保留未登录环境 |
| [22-k1-topics](browser/22-k1-topics.png) | 重新开菜单并展开Topics | 看到长主题目录；DOM记录37项，不代表逐项进入 |
| 23-k1-science（原件work） | 点Science | 进入K–1 Science；列表有日期/摘要/音频和西语标记，首条为2025年文章 |
| [24-science-page2](browser/24-science-page2.png) | 点Next | 进入page/2，首条为2023年文章；有限分页，未测所有末页规则 |
| 25-back-to-science（原件work） | 浏览器返回 | 回Science第一页；不推每一文章的阅读位置恢复 |
| 26-menu-again（原件work） | 打开菜单 | 核父级入口；不把隐藏调试角色字段当用户菜单 |
| [27-adult-gate](browser/27-adult-gate.png) | 点For Grown-ups | 同URL弹出出生月/年表单；未填写或提交 |
| 28-gate-escape（原件work） | 按Escape | 两个选择框仍可见；随后只读诊断把Python列表错当JS数组slice，报错，不是产品故障 |
| 29-gate-closed（原件work） | 点左上X | 关闭过渡帧仍有左侧装饰；后续已能打开资源菜单，不称本帧完全静止 |
| [30-resources](browser/30-resources.png) | 重新打开菜单并展开Resource Spotlight | 可见四个资源入口；未进入或注册这些关联服务 |
| 31-menu-closed（原件work） | 点菜单X | 结束本轮，随后关闭浏览器；未作提交、登录、课堂分配或打印 |

## 可复核的关键对照

portrait的11→12和13→14→15都记录媒体继续推进；16→17保持暂停，18刷新后回0。04/17/28分别记录Escape或卡片外点击后仍可见的DOM状态，不能推广成全站键盘可访问性结论。未测试音频结束自动收卡、双词同时播、系统打断或离线。

六次主文档响应均200。页面日志有6次Load error与8次媒体ERR_ABORTED，时间接近页面初载/刷新；未定位因果，不把它们全解释为词音失败，也不据成功推进证明播放链总可靠。

首次未分配PTY的进程stdin关闭，确认该进程已退出、没有网页操作后，以tty启动本次唯一有效会话。有效会话中一次只读诊断Python .slice报AttributeError，已保留动作日志；没有强制点击、修改业务变量、调用内容或账号接口。浏览器最终正常退出0。

[动作日志](browser/trace-actions.json)含81条筛选后的Playwright调用，含31截图和13等待，不是点击数。[状态](browser/observed-states.json)、[错误](browser/browser-errors.json)、[响应](browser/document-statuses.json)与[原trace散列](browser/raw-trace-manifest.json)可交叉检查。完整DOM/原截图在work，正式摘记不复制整篇新闻。
