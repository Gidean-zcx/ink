# 第35批：洪恩识字与不背单词学习链来源审计

日期：2026-10-27。范围仅 M-029 洪恩识字 App、M-018 不背单词 App。目的为列出仍缺的具体观察及候选出处，不修改正式 docs、不提出产品设计决策、不替代 root 验收。无新 E/F；无安装、登录、提交、播放、媒体下载或产品操作。

**结论：两款产品都没有从已有资料中找到尚未打开、且已确认能覆盖“错答→提示→重试→结束”的官方教程直链。** 本轮只读本地记录并作三组普通公开搜索，未对任何候选网址发起 `open`、HTTP GET 或浏览器访问。没有重试旧 401/403/412/429、验证码或登录路线。新发现一个待核身份的公开视频 URL，另有未取得直链的教程名称线索；两者都不能升级为行为证据。

## 1. M-029：目前还差哪些步骤

范围沿用本地案例：大陆 iOS/小米 Android 商店旧快照均列 4.5.2 / 2026-10-23；未安装；官网图片/视频没有对应构建。36 行 inventory 是已知域及历史分支，不是完整菜单。

| 关联任务 / inventory | 已有材料的上限 | 同一构建仍缺的具体步骤 | 取证价值 |
| --- | --- | --- | --- |
| F-M029-01；IHLC-I01/I03～I07 | 当前五环节声明；旧静态选择控件；历史“二”正常选字、“雨”辅助入口 | 选定同一字→进入练习→故意错选→系统具体文案/声音/图形反馈→提示如何触发→相同题目再次作答→通过；须看错误后选项是否保留、能否连续重试及跳过出口 | 最高；直接补错答主链。现有正常分支和宣传控件不能完成这项 |
| F-M029-01；IHLC-I06 | 开发者称有笔画纠错；2020作者称有古字/问号入口 | 写错一笔→判定位置/笔序/轨迹→示范或提示→擦除/重写动作→何时接受→写不出来如何结束 | 高；必须与单独产品“洪恩写字”区分 |
| F-M029-01；IHLC-I07 | “说”从字到词句是内容声明 | 进入说环节→开始/停止录音→提交→拒绝权限/识别错误反馈→重录或跳过→通过/结束 | 高；语音权限条款不等于这一套交互已实现 |
| F-M029-01；IHLC-I08～I10/I34 | 作者称“二”练习正常答对后有场景结果；当前有每日回顾/阶段检测/挑战声明 | 本字最终完成页→字状态变化→奖励到账→下一字/复习的实际入口；重复完成的奖励及错字回流；回顾何时出现、能否跳过、退出后如何找回 | 高；不合并历史“三次”与当前阈值，不将地图完成等同掌握 |
| F-M029-04；IHLC-I02/I11 | 年级/书册/课文→单字→整课测验属于开发者声明 | 选课确认→测验题型→错答与提示→补练→通过/未通过页→返回教材及进度；更换教材与中途退出后的保存范围 | 高，但当前连测验入口都未观察 |
| F-M029-01；IHLC-I12 | 绘本静态图可见目标字和开始阅读 | 单字完成是否直接/解锁进入绘本→逐页阅读→音频入口与暂停→结束→重新打开的页码/声音断点 | 关联5–6岁自主读听；没有纯听或断点直接证据 |
| F-M029-02；IHLC-I19～I23/I26 | 2020教学环节设置路径、3.0.1设置页签 | 保存环节配置→当前字是否立即变化；到时提醒→结束/延长；课内退出与账号退出分别处理；断网/强退后重进同一字、题组和绘本 | 高；旧“离线缓存”页签不证明缓存内容及离线进度保存 |

### 已知官方 URL 的当前状态——不能再次列成“尚未打开”

| URL | 既有记录及停止点 | 本次处理 |
| --- | --- | --- |
| https://h5static1.ihumand.com/website/pc/cn/shizi/assets/media/cn_video.198bceb2.mp4 | 原代理浏览器启动失败后，root 已于同日正常流式取得并抽看10/30/50/70秒；86.122667秒，1920×1080，HTTP206；未听音、未保存MP4。E-IHVP-01，30秒为旧四环节宣传片段，非错答流程 | **已抽样打开，不是新候选。** 未连续查看和未听音是剩余范围，但无依据声称未看部分一定有错误分支。仅复读记录，没有重播 |
| https://www.ihuman.com/shizi/ | 已读正文、静态图库、公开脚本线索；上述视频从此发现 | 没有新的学习帮助直链；不重复抓取 |
| https://www.ihuman.com/agreement/privacy/operator.html | 已有3.0.1官方设置/隐私入口图 | 管理路径，不能补逐题纠错 |
| https://www.ihuman.com/agreement/privacy/feedback.html | 已读首页右上反馈入口 | 未取得学习问题目录、具体答复或工单；不外发询问 |
| https://www.ihuman.com/support/ | 官方页此前仅给联系方式 | 不把联系方式当公开FAQ正文，不联系 |

**本轮未找到新的洪恩识字官方教程/帮助/公开视频直链。** 搜索出现“洪恩英语”“洪恩AI快读/大阅读”“洪恩写字”等不属于此 App 的材料，均不替代；媒体的“官方账号”标识也不是洪恩厂商身份。

旧失败不重试：`https://www.bilibili.com/opus/425918819985666271` 原站412；`https://post.smzdm.com/p/aoo8one6/` GET200但验证码壳；2022退出教程图片403；学校PDF404和近期设置文超时。前两篇已有可读搜索工具正文，仅为历史第三方文字，不把历史直接GET失败转换成新抓取入口。

## 2. M-018：目前还差哪些步骤

范围沿用本地案例：iOS中国/澳大利亚区旧快照5.11.6（中国区内嵌日期2026-09-18）、Android魅族旧快照5.11.7 / 2026-10-15。34项功能台账加9项历史映射不是43个现行菜单。2017、2021、2022/2023材料不可拼成现版连续链。

| 关联任务 / inventory | 已有材料的上限 | 同一构建仍缺的具体步骤 | 取证价值 |
| --- | --- | --- | --- |
| F-M018-01/02；M018-L01/L04/L05，I05 | 历史作者图示选错→答案→继续；静态选义/不认识/例句界面 | 实际选错同一词→判错画面→揭示答案或解释→继续的具体操作→该词再次出现→答对后移出/保留队列 | 最高；作者流程图不是动态判错证据 |
| F-M018-02；I06、L03 | 2017灯泡图及作者“发音提示”描述；5.11.3称优化提示页 | 拼错提交→错误标记→首次/再次提示具体给出什么→保留或清空输入→重拼提交→通过；提示后分数/调度变化与重试上限 | 最高；历史图并排是不同词，不能当同词提示前后 |
| F-M018-01；I04、L02 | 2017辨识完成弹窗有放弃/开始拼写 | 当前学后/复习后拼写开关位置及保存→辨识完成→选择拼写→最后一个词通过→整组结算；选择放弃后是否也计完成、以后能否找回该组 | 高；辨识完成、拼写完成与学习日完成须分别取证 |
| F-M018-02；I05、L06 | 5.11有学完后错词专项声明；历史作者写重学次数 | 错词集合来自何处→专项入口→空集合→重错→提示→通过→剩余数变化→终止/返回；是否影响原学习组 | 高；2017五次与较晚四次不统一为现版算法 |
| F-M018-02；I07/I08、L08 | 返回上一词功能声明；5.11.6“尝试修复”纠错按钮 | 返回→重新评价→确认/取消→原答案、计数、错词集合及复习日期是否变化；“记错了”在哪种题型出现 | 高；返回外观不等于撤销，修复记录不等于成功 |
| F-M018-01/02；I03/I13/I33、L09 | 组大小/04:00日边界声明 | 分组未完成时退出→再次打开；离线错答提交→联网；跨04:00→队列和进度；后台、强退分别处理 | 高；日界不是自动清空/重排的证据 |
| F-M018-03；I01/I02 | 词书计划、范围筛选存在 | 选书/单元→确认→保存每日计划→开始实际抽词；切书/漏学时如何保留与补排 | 作为主链前置；现有公告壳不能补这项 |
| F-M018-04；I15/I19/I21/I34、L07 | 数据/酷币入口与历史奖励对象；有限跨产品共享 | 当前组结算→学习数据和奖励实际增加→重复/放弃如何计数；换机重装→重新登录→具体词组和进度恢复 | 奖励不是掌握；轻听共享不能扩成所有进度双向同步 |

### 未打开的候选与缺少直链的线索

| 候选 | 精确 URL / 可定位名 | 官方性及当前取得方式 | 是否有希望补主链 |
| --- | --- | --- | --- |
| A：不背十周年纪念片 | https://www.bilibili.com/video/BV12H4y1n7rh/ | 本轮普通搜索返回标题、2024-04-10、发布者“不背单词App”，简介自称官方账号；**只读搜索索引，未打开/播放，尚未独立核验账号认证或官网反链**。此前日志只记纪念片线索，本地本轮查找未见该具体BV网址 | 低：标题/简介是纪念性质，没有逐题教学章节或错答记录；不能据“官方”自述称已确认教程。不优先于可核具体任务的帮助 |
| B：不背学堂内“快速上手”视频 | **未找到官方直链**；仅定位教程名称 | 下载站索引转载的5.5.9日志有此名称；本地官方App Store旧HTML只在5.9.13提到“不背学堂”的班级打卡，没有“快速上手”。官方域名/公众号定向搜索没有返回其原文URL | 条件性较高：标题与教程目的相符，但目前只是待核线索，不能把转载日志当官方事实；需真实公开原厂教程链接及版本，不能猜路由、进登录或装App |
| C：App内帮助模块 | **未找到独立公开FAQ URL**；已知入口为头像→更多设置→帮助与反馈，Android旧描述另称设置→帮助中心 | 来源为既有官方产品说明/政策索引，未进入App，不统一平台名称 | 若以后有匿名网页对应拼写/错词/未完成组问题，可直接针对缺口；当前只有容器入口 |

候选B的第三方发现页（**不是官方教学来源，未打开**）：https://m.itmop.com/downinfo/566046.html 。本轮其他下载站转载没有增加官方性，不逐站扩查。没有将其中所谓拼写规则、错误标记或离线能力写入产品事实。

### 已知受限或已用 URL：不作为未开候选

- https://learnywhere.cn/bb/production/notification ：此前200/976字节脚本壳，其 `app.8a73937b.js` 请求403后停止。未重试或换域绕过。
- https://www.zhihu.com/org/bu-bei-dan-ci-app/activities ：此前403，未重试。
- https://weibo.com/3976409298 ：官网确实给这个链接，旧路线 robots 阻止；不是新未开教程。
- https://www.bbdc.cn/lexis_book_index ：此前转登录，未再进入；不是可匿名导入教程。
- https://learnywhere.cn/lexis/privacy ：既有原址为脚本壳、搜索索引正文另存；政策不替代错答/恢复帮助。
- https://sspai.com/post/76700 ：既有作者文字自称展示官方小窍门，但图请求403后停止。不能从第三方作者称呼认定已核官方正文，更不能重抓图。
- 本轮搜索返回 `https://app.flyme.cn/apps/public/detail?package_name=cn.com.langeasy.LangEasyLexis` 的5.11.3旧索引；未打开，不将其替换已核魅族5.11.7，也不把变更域名当教学新来源。

## 3. 实际读取与搜索记录

### 本地文件

除 `AGENTS.md`、`README.md`、`docs/requirements.md` 外，读过以下目标及其相关记录；只读，没有运行旧构建/采集脚本：

- `docs/research/interaction-library/cases/M-029-ihuman.md`
- `docs/research/interaction-library/cases/M-018-bbdc.md`
- `docs/research/interaction-library/batches/ihuman-learning-controls-deepening-inventory.csv`
- `docs/research/interaction-library/batches/bbdc-controls-deepening-inventory.csv`
- `docs/research/interaction-library/batches/bbdc-learning-controls-deepening-inventory.csv`
- `docs/research/interaction-library/artifacts/ihuman-video-preview/source-notes.md`
- `work/ihuman-learning-controls-deepening/HANDOFF.md`
- `work/ihuman-learning-controls-deepening/exploration-log.json`
- `work/ihuman-learning-controls-deepening/http-captures.json`
- `work/ihuman-video-preview/video-inspection.json`
- `work/children-loop-deepening/ihuman-site.html`
- `work/bbdc-controls-deepening/research-log.md`
- `work/bbdc-controls-deepening/HANDOFF.md`
- `work/bbdc-controls-deepening/index.html`
- `work/bbdc-controls-deepening/appstore-cn.html`
- `work/bbdc-controls-deepening/privacy-web-search-response.txt`
- `work/bbdc-learning-controls-deepening/candidate-register.json`
- `work/bbdc-learning-controls-deepening/HANDOFF.md`
- `work/bbdc-learning-controls-deepening/raw/c06-sspai2023.txt`
- `work/chinese-vocab-deepening/research-log.md`
- `work/vocab-research/bbdc-index.html`（只作关键词匹配，没有新增抓取）

### 精确 shell 命令与状态

以下均在 `/home/workspace/reader`，读取命令 exit 0，特别注明者除外。初次并行大输出有显示截断，随后对关键段和记录分别小范围读取；没有把截断视为完整读完。

```sh
cat AGENTS.md
rg --files docs/research/interaction-library | rg 'M-029|M-018|inventor'
rg --files work | rg -i 'ihuman|bubei|bubei|batch[0-9]+|inventory'
cat docs/research/interaction-library/batches/ihuman-learning-controls-deepening-inventory.csv
cat docs/research/interaction-library/batches/bbdc-learning-controls-deepening-inventory.csv
cat docs/research/interaction-library/batches/bbdc-controls-deepening-inventory.csv
cat docs/research/interaction-library/cases/M-029-ihuman.md
cat docs/research/interaction-library/cases/M-018-bbdc.md
rg --files work | rg -i 'bbdc'
rg -n '^##|^###|未|失败|https://' docs/research/interaction-library/cases/M-029-ihuman.md
cat work/ihuman-learning-controls-deepening/HANDOFF.md work/ihuman-learning-controls-deepening/exploration-log.json work/ihuman-video-preview/video-inspection.json
cat work/bbdc-controls-deepening/research-log.md work/bbdc-controls-deepening/HANDOFF.md work/bbdc-learning-controls-deepening/candidate-register.json work/bbdc-learning-controls-deepening/HANDOFF.md
cat work/ihuman-learning-controls-deepening/exploration-log.json
cat work/ihuman-video-preview/video-inspection.json
sed -n '1,95p' work/bbdc-controls-deepening/research-log.md
sed -n '25,45p' docs/research/interaction-library/batches/bbdc-controls-deepening-inventory.csv
sed -n '1,38p' docs/research/interaction-library/cases/M-018-bbdc.md
sed -n '128,151p' docs/research/interaction-library/cases/M-029-ihuman.md
sed -n '1,35p' README.md
sed -n '7,22p' docs/requirements.md
rg -l '不背学堂|快速上手|小窍门|bilibili|教程|视频' work/bbdc-controls-deepening/index.html work/bbdc-controls-deepening/appstore-cn.html work/bbdc-controls-deepening/privacy-web-search-response.txt work/vocab-research/bbdc-index.html work/bbdc-learning-controls-deepening/raw/c06-sspai2023.txt
cat docs/research/interaction-library/artifacts/ihuman-video-preview/source-notes.md
cat work/ihuman-learning-controls-deepening/http-captures.json
```

以下查重 exit 1 表示无匹配，不是网页失败：

```sh
rg -l 'BV12H4y1n7rh|不背学堂.{0,20}快速上手' docs/research/interaction-library work/bbdc-controls-deepening work/bbdc-learning-controls-deepening work/chinese-vocab-deepening work/vocab-research
```

两次内联 Python 只读本地文本：第一次以 `HTMLParser` 提取两份官网 HTML 的 a/video/source/iframe 的 href/src/data-src；第二次在四份既有文本逐一查 `不背学堂`、`快速上手`、`小窍门`、`bilibili` 并打印最多两处上下文。均 exit 0；未执行网页脚本或请求新URL。结果为官方本地HTML只证不背学堂名称，不证快速上手视频；完整链接结果已在上面区分既读与候选。

### 普通公开搜索（无 shell exit 码）

使用 `web.search_query` 三次、每次四个查询；结果正常返回。没有对结果执行 open/click，没有下载。检索仅发现/排除候选，不提取第三方规则到正式案例。

1. `site:bilibili.com/video "不背单词App" "拼写"`
2. `site:bilibili.com/video "不背单词App" "学习" "官方"`
3. `site:bilibili.com/video "洪恩识字" "官方" "教程"`
4. `site:mp.weixin.qq.com "洪恩识字" "错" "复习"`
5. `"洪恩识字" "官方账号"`
6. `"洪恩识字" "新手指南"`
7. `"不背单词" "官方" "拼写提示"`
8. `"不背单词" "使用小窍门"`
9. `"不背学堂" "快速上手"`
10. `"不背学堂" site:learnywhere.cn`
11. `"不背学堂" site:bbdc.cn`
12. `"不背单词" "快速上手" site:mp.weixin.qq.com`

搜索结果不能穷尽所有官方渠道。没有找到可用直链不表示产品没有教程，也不表示用户的App中没有这些功能。

## 4. 状态与检查

`state_changed=true` 仅因为新增本审计文件；`docs_changed=false`，外部账号/产品状态变化为 false。正式E/F、inventory、case均未更新。曾尝试分派只读 worker，但工具返回 `agent thread limit reached`，没有子代理启动或执行；未换模型或继续尝试该路线，由本代理完成。

写入前读取的五个目标文件 SHA-256：

```text
c25ca69440bc60a6907dcd19f012ff7f690ce83f50b9dfc35a4a8d3ea97987a1  docs/research/interaction-library/cases/M-029-ihuman.md
c9674767b37c1e041cffb980a927a64adad5fafccddcc82951aedc1414798e29  docs/research/interaction-library/cases/M-018-bbdc.md
7dea7eeda1863c9cdaa184b2e6211aa92d854c633cd85f87a130767acf559ce0  docs/research/interaction-library/batches/ihuman-learning-controls-deepening-inventory.csv
0dd1421391e40ce4a5cac67d4b7ca70412278f6c62a948d3c22270f3e9307dfd  docs/research/interaction-library/batches/bbdc-learning-controls-deepening-inventory.csv
3a089a877943fca9f20424520dc5f0b8421e76f4e78daa4cc52b065dd303afef  docs/research/interaction-library/batches/bbdc-controls-deepening-inventory.csv
```

写入方式为 `apply_patch`，成功。随后只读检查本文件存在/UTF-8/换行/表格闭合、所列本地文件路径存在，以及上述五个正式文件散列未变；检查结果在本任务工具输出与交接中报告，不产生产品测试或正式研究结论。
