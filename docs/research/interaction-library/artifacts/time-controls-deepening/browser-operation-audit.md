# M047：K–1公开文章听读与查找操作核查

观察日2026-10-26。F-M047-01、F-M047-04；E-TKCD-01、E-TKCD-02、E-TKCD-03。

根代理用Playwright实际操作公开网页。D4/hands_on=true仅限此桌面浏览器任务，不代表原生App、手指触摸、电子纸实机、儿童完成或整个任务通过。未登录、未提交年龄/课堂作业，也未打印或下载音频文件。

环境：Linux、Chromium Headless Shell156.0.8078.4、1280×950、新建匿名上下文；Playwright包1.64.1，trace内标1.64.0-beta-1791568591000。前端构建未知。正常公开[植物文章](https://www.timeforkids.com/k1/spotlight-on-plants-k1/)初始HTTP200；文章2026-10-16。英语/西语是这篇内容的两个入口，不代表每篇具有同样工具。

**未实际听音。** 证据是可见播放控件、计时/进度、高亮、页面滚动和语言正文。DOM未找到audio/video节点，不能以空列表断言没有声音。声源、准确发音、声音语种、语速听感及同步精度未知。

## 顺序记录

共31次捕获、18张必要截图正式归档，29帧作视觉判读；11/12仅核结构。截图与随后DOM读取存在时间差，不把一秒差值当产品问题。

| 捕获 | 动作 | 观察与限制 |
| --- | --- | --- |
| 01-article-initial（原件work） | 直接打开公开K–1植物文章，HTTP200 | Read Aloud、西语、打印入口；尚未展开播放器 |
| [02-read-aloud-start](browser/02-read-aloud-start.png) | 点击Listen to audio，等2秒 | 出现Pause、.9x和0:48；标题/段落高亮，随后页面随进度滚动 |
| 03-pause（原件work） | 间隔后点击播放/暂停控件 | 实际是Pause、0:49；文件名是当时意图，不能称暂停成功 |
| [04-pause-later](browser/04-pause-later.png) | 再次截帧 | 仍Pause，截图0:21、后取DOM0:20，range=60；位置已到后文 |
| 05-speed-menu（原件work） | 点语速入口 | 显示.5x/.75x/.9x/1x/1.25x；未选新档位 |
| 06-paused-confirmed（原件work） | 点击Pause，等300ms | 显示Play、0:01、range=100；接近末尾，不能分离暂停与自然结束 |
| [07-after-short-play-attempt](browser/07-after-short-play-attempt.png) | 点击Play后等2.5秒，再找Pause超时，之后截帧 | 显示Play、0:49、range=0；没有记录到该次预期暂停，保留失败 |
| 08-replay-running（原件work） | 再次点Play，等1.8秒 | Pause；截图0:47，后取DOM0:46，range=10；自动回到前文 |
| [09-controlled-pause](browser/09-controlled-pause.png) | 立即点击Pause | Play、0:46、range=10 |
| [10-pause-stability](browser/10-pause-stability.png) | 再等1.5秒 | 仍Play、0:46、range=10，暂停状态稳定 |
| 11-speed-menu-reopened（原件work） | 点击语速并查找button类型1.25x失败后，再点入口 | 本帧未见选项；只读结构核对，不作视觉结论 |
| 12-speed-hover（原件work） | 尝试悬停语速控件 | 错误button选择器仍未命中；只读结构核对，不作视觉结论 |
| [13-speed-menu-current](browser/13-speed-menu-current.png) | 再次点击语速；读取实际role | 菜单显示；选项实际role=radio，不是button |
| [14-speed-selected](browser/14-speed-selected.png) | 点击radio类型1.25x | 语速文字/aria改1.25x；仍Play、range=10、0:46 |
| [15-seek-forward-paused](browser/15-seek-forward-paused.png) | 聚焦进度条并按Enter | range从10变20，仍Play；时间文字变0:08，不统一解释为剩余时间 |
| [16-close](browser/16-close.png) | 点击关闭播放器 | 显示Read Aloud；隐藏DOM仍含旧进度与语速，不能当可见或已持久保存 |
| [17-reopen](browser/17-reopen.png) | 再次点Read Aloud，等1.2秒 | 展开并播放，回标题；range=0、.9x、0:48，未延续20/1.25x |
| 18-reopen-paused（原件work） | 立即点Pause | Play、range=0、.9x、0:48 |
| 19-reload-closed（原件work） | 刷新并等1.3秒 | 播放器收起；此前range已是0，单独此帧不能证明丢失非零位置 |
| 20-reload-play（原件work） | 点Read Aloud，等1秒 | Pause、range=0、.9x、0:48 |
| 21-reload-pause（原件work） | 点Pause | Play、range=0、.9x、0:48 |
| 22-spanish（原件work） | 点En Espanol，等1.5秒 | URL带lang=es，标题/正文为西语，播放器收起；有In English |
| [23-spanish-play](browser/23-spanish-play.png) | 在西语页点Read Aloud，等1.6秒 | Pause、0:57、.9x，西语标题高亮；没有实际听音，不能判断声音语种或质量 |
| 24-spanish-pause（原件work） | 点Pause | Play、0:57、range=0；只核界面状态 |
| [25-before-reload-nonzero](browser/25-before-reload-nonzero.png) | 点In English，启动后暂停；进度条Enter三次，选择1.25x | 英文URL带lang=en；Play、range=30、1.25x、0:16，建立非零对照 |
| [26-after-reload-nonzero](browser/26-after-reload-nonzero.png) | 刷新，再启动1秒并暂停 | Play、range=0、.9x、0:48；非零位置和语速未恢复 |
| [27-search-entry](browser/27-search-entry.png) | 点Search | 同网址展开全屏输入层；Enter提交提示；HTML中的原文仍在，不代表可见 |
| [28-search-plants](browser/28-search-plants.png) | 输入plants并Enter，等1.8秒 | 进入/k1/?text=plants，K–1选中；首条植物、第二条动物等，有Audio/Spanish标识 |
| [29-search-empty](browser/29-search-empty.png) | 首次定位两个同placeholder输入失败；改用searchbox输入zzqvnonexistent并Enter，等1.5秒 | URL和搜索框带该字符串，仍有植物/真菌等普通文章；未见明确无结果提示。文件名不代表空结果 |
| [30-search-cleared](browser/30-search-cleared.png) | 清空可见searchbox并Enter，等1.2秒 | URL text为空，K–1保留；首屏植物/真菌 |
| [31-return-article](browser/31-return-article.png) | 点击植物文章标题，等1.2秒 | 回英文正文顶部，播放器收起；未见已读或任务完成记录 |

## 失败与解释边界

日志含3次5秒定位超时：近末尾重放后寻找Pause一次；误将语速radio按button查找两次。另有一次两个同placeholder输入导致严格匹配失败。纠正后使用正常控件完成；没有强制点击、改页面业务变量或直接调播放/搜索接口。这些定位错误不是产品故障。

页面错误日志有9次Load error及一次音频请求ERR_ABORTED；没有确定其原因，不能据此宣称完整音频链可靠，也不能把所有错误解释为音频失败。刷新/导航可能取消请求，但本次没有因果定位。失败原始记录保留。

03/04的文件名含pause但实际仍在播放；06位于末尾；真正可比较的暂停是08→09→10。19的刷新前range为0，不用于证明非零断点丢失；有效刷新对照是25→26。29名称含empty但画面非空。

进度条range的值按0–100读取；播放中时间文字下降，但暂停后按Enter时出现0:08或0:16，不把该控件在所有状态统一称为剩余秒数。没有检查其内部计时公式。关闭和刷新各做了一次有效对照；不能推广为所有文章、账户或浏览器的规则。

搜索不命中语义的字符串仍返回普通文章，是本次观察；原因、服务端是否执行搜索、分词/回退规则和更多等待后的变化均未知。没有把结果页URL当检索成功。

[动作日志](browser/trace-actions.json)含101条所选调用，其中31次截图、22次等待，并非101次点击；[观察状态](browser/observed-states.json)、[原trace散列](browser/raw-trace-manifest.json)及[页面错误](browser/browser-errors.json)一并保存。原trace没有启用连续截图/快照，没有视频；work原件不承诺长期保留。
