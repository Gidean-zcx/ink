# 公开检索记录与未命中边界

查阅日期：2026-11-12。只使用匿名公开网页、API、Git refs 和文本源码；未登录，未联系作者，未请求固件下载地址。完整的84条中英文 Web 关键词在 [evidence.json](evidence.json) 的 `web_search_queries`，公开请求的状态和哈希在 `requests`。搜索结果会变，未命中不能证明从未发布。

## 具体查了哪里

| 检索组 | 操作与取得的证据 | 结果／限制 |
| --- | --- | --- |
| 注册记录 | 重新 GET [公开注册接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all)，按旧快照 ID 对应 F 编号，抽取说明、历史链接、公开署名 | 53条；本任务仍覆盖原指定39条记录，不把新测试项加进来。没有请求其中任何固件、头像或附件 URL |
| CrossLink 的 Gitee 替代路径 | 官网 `www` 域名 DNS 失败；改用 [Gitee 仓库 API](https://gitee.com/api/v5/repos/daixinchun/crosslink)、[固定树 API](https://gitee.com/api/v5/repos/daixinchun/crosslink/git/trees/eaeecd22761dd2d2884cbdb02acd066d4bd71bc4?recursive=1)、提交 API、README 文本及 [xteink-x3 发布历史](https://gitee.com/daixinchun/xteink-x3/releases) | 发布仓固定树仅三份文件，但中文README明确指向 `daixinchun/xteink`；继续通过该仓API读到固定540项源码树、8份应用代码，见S26。网页404不代表API不可读；Gitee forks API 返回500，未登录重试 |
| CrossLink 真正的 Gitee 源码仓 | 发布仓中文README的blob指向 [daixinchun/xteink](https://gitee.com/daixinchun/xteink)；匿名API元数据、固定树、8个Git blob | 提交5a93c1bc、构建版本20260424；坚果云、数据胶囊、阅读同步、字体和风眼主题代码均已逐份核SHA-1。没有下载bin目录或固件；当前Metalio版本仍未对应 |
| CrossLink 的 GitHub 旧版、fork | [DaisonChun/crosslink](https://github.com/DaisonChun/crosslink) 的固定树和 raw；fork列表取得 BaconSoong、leosongyyy、adam-ikari、reira-lai、ck00031，逐个 `git ls-remote` 查全部公开分支 | 4个默认 HEAD 同为107e1729；adam-ikari 是2d4dc951；BaconSoong 另有电池修复分支。另读后两种不同分支的 platformio.ini，仍是C3配置；没有取得当前 Pixel／Metalio 发布版源码对应证据。精确 refs 见 JSON |
| CrossLink 的再上游 | Gitee [allocated/crosspoint_chinesetype](https://gitee.com/allocated/crosspoint_chinesetype) 元数据、GitHub [icannotttt/crosspoint-chinesetype](https://github.com/icannotttt/crosspoint-chinesetype) 的只读过滤克隆与选定文本 | 有中文阅读／字体实现。README:78 明确展示版本不含后续 TXT、图片等代码。它是部分来源，不是当前功能列表的全部代码 |
| Mogo／PageRest 官网 | Mogo 首页、文档、首页 JS 页脚；PageRest 首页、JS、页脚第三方许可；Read Pico 厂商的 PageRest 文档及作者链接 | 两站本次均HTTP200，前次403不沿用。Mogo明确链接kiikoread；PageRest许可列的是网站依赖，不是固件第三方清单 |
| PageRest 的旧仓库／同名误命中 | 厂商文档直链 `fengcheng01/read_pico_firmware` 返回404，过滤克隆失败；换作者显示名 DerrickFC 地址也不可读。GitHub名称搜索第一页20条，共29条；`elfenixsoy/PageRest` 的树包含 pizza、dish、restaurant 页面资产 | 不能把餐厅网页当固件源码。没有以“搜不到”推定它从未开源；当前失效的明确仓库地址保留在报告里。没有抓取未登录可见范围之外的内容 |
| ThingOS／ThingsOS 作者 | 注册署名 flowflic；读同名 GitHub账号公开仓库列表，找到 [ThingsOS](https://github.com/flowflic/ThingsOS)；完整 Git 元数据历史3次提交、全部 refs、当前树、README／LICENSE／第三方声明 | 无 C/C++/构建文件；历史也只涉及说明、许可与图片。Pico说明和Plus注册版不能直接等同。排除 `ccrisan/thingos` Linux系统、鸿蒙 ThingOS 等同名项目，未拿它们当上游 |
| RickyOS 的旧版与发布材料 | 官网使用授权页面→明确 GitHub 地址；当前完整树、固定文本、Release 元数据、公共 refs | 1.1线有真正应用代码；公开 Release 至v1.1.5，README和构建版本有差异；没有任何1.2源码对应证据。只读附件名称与元数据，未下载其中任何bin |
| BoBoOS 的开发文档 | [首页](https://bobo.sh.creativone.cn)、公开 [developer/docs](https://bobo.sh.creativone.cn/developer/docs)、GitHub／Gitee名称搜索、creativone相关关键词 | 取得 `.crosspoint/plugins` 和 manifest/device/plugin.js 约定，以及 ELF/MCP 包说明；文中提及固件仓库的 app-sdk 路径，但未给出可取得该代码的公开链接。协议线索与源码分开记录 |
| wenbos058、paouke 组 | GitHub/Gitee同名用户仓库接口；ModernClassicOS、lnkboot/Inkboot、布丁/Pudding、墨集/Moji、Eink4Boy、ardmos 中英文与同名搜索 | wenbos058接口404；paouke均为公开空列表；不能认为同名账号已验证属于作者。GitHub合并名称搜索6个结果均为音乐、恒温器、工作脚本等同名误命中，不用于血缘结论 |
| 开哥十类应用 | [CloudZao-Projects](https://github.com/Changkai9888/CloudZao-Projects) 的固定完整树、README、接入规范；录音机、浏览器、天气、五子棋、贪吃蛇、记事本、点读机、亮证、黑猩猩测试、森林里的朋友关键词 | 树有口袋source.zip、relink.zip、亮证public-compliance.zip和字体源码，未列出其余应用独立源码。对口袋source包只请求末尾范围，服务器501，未读取整包；没有把合规包猜成全部应用实现 |
| 游戏来源 | retro-go-tab5 README和GBA/gnuboy源码→retro-go-claw4、gpSP、HowBoyAdvance；另核Peanut-GB与Ardens | 找到了可读的运行、存档、输入、AVR／JIT代码。F26 与这些仓库的身份对应仍未知；没有下载ROM／BIOS／游戏／固件 |
| 微读来源 | CrossMux／RickyOS 微读 README的致谢→weread.koplugin、papers3-weread；按文件核登录、章节、下载恢复和进度代码 | 这是明确“参考”来源，不等于逐行复制；公开版和闭源版功能区别保留。没有连接微信读书、读取真实书架或调用账号接口 |
| GitLab | `GET /api/v4/projects?search=Metalio`、`search=PageRest`，以及CrossLink查询；Web检索固件名+source／源码+gitlab.com | Metalio／PageRest返回空数组，CrossLink接口500；没有发现可确认的相关源码，不声称检索了私有项目或全部可能别名 |
| 音频与框架候选 | ESP32-audioI2S、ESP-ADF、ESP-Brookesia、Launcher、xiaozhi-esp32、LVGL官方仓库固定文本 | 只作功能替代候选。没有因为都有音乐、手机样式或AI工具，就说闭源固件实际用了这些框架 |

## 中文社区和英文搜索的覆盖

GitHub、Gitee 和 GitLab 的直接读取之外，Web 查询使用中文名称、英文名称、固件、源码、开源、source、fork、based、作者名及设备名组合；覆盖任务列出的 B站、小红书、知乎、酷安、贴吧和 V2EX 的公开索引。

| 平台 | 已执行关键词示例 | 本次能作证据的范围 |
| --- | --- | --- |
| GitHub／Gitee | `Crosslink reader Gitee whimco CrossLink Pixel 风眼 开源`；`Eink4Boy OR ardmos OR ModernClassicOS OR lnkboot`；`墨集 Launcher 源码` | 关键发现均回到仓库／作者站点核查，未用搜索摘要代替代码。Gitee搜索ModernClassicOS、Eink4Boy、BoBoOS的公开API为空 |
| B站 | `Mogo 固件 site:bilibili.com`；`开哥 固件 site:bilibili.com` | 返回含大量同名／无关条目，未取得可确认原作者的源码链接；未下载视频。S14 README另给出作者的B站演示入口，只记入口，不当成实机体验证据 |
| 小红书 | `CrossLink Pixel 源码 site:xiaohongshu.com`；`PageRest site:xiaohongshu.com`；`ModernClassicOS site:xiaohongshu.com` | 公开索引未提供可核源码；S06 README直接给出作者的功能介绍短链，但未将短链背后的未读内容作为结论依据 |
| 知乎 | `CrossLink source code site:zhihu.com`；`Mogo E4p 开源 site:zhihu.com` | 没有可确认相关源码的命中；同词跨领域文章排除 |
| 酷安 | `墨水屏 固件 开源 site:coolapk.com`；`布丁 E-INK4 site:coolapk.com` | 未取得可核的原作者仓库链接；不能据此否定登录后帖子或客户端内容 |
| 贴吧 | `E-INK4 固件 site:tieba.baidu.com` | 未取得相关可核源码链接；没有把搜索无结果写成全站无结果 |
| V2EX | `Metalio 固件 site:v2ex.com`；列举ModernClassicOS／lnkboot／Eink4Boy／ardmos的域名限定查询 | 结果只有同名账号和其它ESP32项目等，未建立身份关系；未把账号同名当作者本人 |

## 访问和证据边界

- GitHub树API后续出现403限流，停止这条路线；后续仅用标准匿名Git过滤克隆／refs和固定raw文本，不使用任何用户凭据。过滤克隆不检出文件、不初始化子模块；最终检查所下载Git包中没有blob对象，选定源码另以文本GET取得。
- 新克隆全部在被忽略的本任务 `work/` 内，已有上游副本未切换提交、未 fetch、未写入。没有运行第三方安装器、构建脚本、网页JS或串口功能。
- HTTP200未自动算“有源码”：ThingsOS说明仓、PageRest网站许可证、Gitee CrossLink说明仓均单独核类型。HTTP404／500／DNS失败未用空白结果替代，失败仍记录。
- `evidence.json`只保留文本哈希、必要短摘录、公开仓库／网址与查询；不保留注册接口的用户ID、头像／固件下载地址，也不提交Gitee重定向的临时签名参数、主机路径或第三方正文。
