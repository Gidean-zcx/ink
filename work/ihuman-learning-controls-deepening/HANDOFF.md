# M029 第28批交接：历史正常分支与可得功能目录

2026-10-27。已收束为 **2新E、0新F、0新视觉观察；2旧F/8字段，1旧E追加本地复读capture；36目录条目**。原5F深度保持3D1/2D2，F01仍D1。未写旧批、主表或global；没有账户、安装、设备、提交动作或保存远程音视频。新资料现冻结，root合并后不要重跑write_batch/precheck。

## 文件与建议格式

- `docs/research/interaction-library/cases/M-029-ihuman.md`：增加本轮边界及旧例，不覆盖历史说明。
- `docs/research/interaction-library/batches/ihuman-learning-controls-deepening-evidence.jsonl`：E-IHLC-01/02；supersedes均null，visual_observed/hands_on均false。
- 同目录 `ihuman-learning-controls-deepening-coverage.csv`：只有标准表头，0新F。
- 同目录 `ihuman-learning-controls-deepening-inventory.csv`：36个有出处的功能域/历史分支，区分developer_claim、旧静态入口、历史文字和未决；不是完整当前App菜单。
- `docs/research/interaction-library/artifacts/ihuman-learning-controls-deepening/source-notes.md`：独立短摘、版本、获取差异、失败与视觉边界。
- `work/ihuman-learning-controls-deepening/existing-coverage-updates.json`：2条，每条 `flow_id/source_batch/changes/replacement_row`，changes逐字段before/after。F01与F02均改evidence_ids、known_branches、gaps、checked_on；不改depth/visual_status。
- `work/ihuman-learning-controls-deepening/existing-evidence-additional-captures.json`：E-IHCD-01仅追加旧商店HTML本地复读，含 `before_exists/before/after/append`，不是新抓取或新版本核验。
- 原5F/12E及case baseline、获取记录、脚本、check结果与manifest均在本work目录。旧视觉审计指向独立static-observation-log.md，不依赖case哈希；未迁移或改图。

## 新E精确来源与原件边界

1. **E-IHLC-01** `https://www.bilibili.com/opus/425918819985666271`，标题《快来识字吧！识字APP测评--第一期》，发布账号懂课鸭、正文署名小张解课，2020-08-21 18:22。web open已返回可读全文；定位洪恩识字→课程体验，工具正文142–221行。作者以“二”述正常学习；未知平台、App版本、错误反馈。后续原站普通GET **412**，停止；未请求配图/其他地址。
2. **E-IHLC-02** `https://post.smzdm.com/p/aoo8one6/`，标题《孩子识字敏感期，懒家长如何助力？》，晞爸买买买，2020-04-25 15:08:34。web open已返回可读正文；定位软件概况/玩认练写/关于软件设置，工具正文98–176行。作者以“雨”述练习和辅助入口；每日五字/强制任务是用户配置。原站普通GET **200但正文仅1697字节TencentCaptcha/WafCaptcha壳**，停止，未执行验证码/换身份。

重要：没有保存这两篇完整原站HTML。`raw/E-IHLC-01-web-reading-record.json` 与 `raw/E-IHLC-02-web-reading-record.json` 是对已返回web正文的URL/署名/日期/段落定位与极短摘录记录，不伪称原网页；E的raw_kind明确这一点。原站壳文件 `raw/smzdm-2020.html` 仅证明访问限制，不能证明文章内容。SHA只证明本地记录字节。不要将这两E写成看过文章截图或完整录像；没有新图片。

## 候选与停止路线

共核5个候选（上限6），详见exploration-log.json；无后续扩源。

- 学校PDF `https://www.palmviewpri.moe.edu.sg/files/For%20Parents%20%282023%29/A%20sharing%20on%20the%20learning%20of%20Chinese%20characters%20Combined.pdf`：web404，未采用/未重试。
- `https://post.smzdm.com/p/arz6epzx/`：web 400 Timeout fetching，未采用/未重试。
- 两篇历史文章如上，正文/原站失败分开保存。
- 官网视频：来源页 **https://www.ihuman.com/shizi/**；精确资源 **https://h5static1.ihumand.com/website/pc/cn/shizi/assets/media/cn_video.198bceb2.mp4**。此前HTML已定位，原件 `work/children-loop-deepening/ihuman-site.html`，原SHA `a1591020b21acb1d8493015e07fda457596a066d9213390de424a67f60dd2ece`。
  本轮 `inspect_video.py` 启动Chromium前失败，未发网络请求，未取元数据/图/音频，也未保存MP4。缺失路径为 `/home/agentuser/.cache/ms-playwright/chromium_headless_shell-1248/chrome-headless-shell-linux64/chrome-headless-shell`；没有安装或换路重试。**无法判断该视频是宣传还是完整操作**。root已接管此单次浏览器核查，后续证据/补丁由root独立处理；本批未写视频E。

旧2018比较图及2022退出教程的4张403图未重试。重印2018旧文、下载站和营销比较只作搜索排除，没有新增E或迁移错误/奖励结论。

## 实际命令与退出状态

工作目录 `/home/workspace/reader`。

- `cat AGENTS.md`、`cat docs/research/interaction-library/cases/M-029-ihuman.md`、`rg --files docs/research/interaction-library work | rg 'ihuman|M-029|ihong|hong(en)?'`、`rg -n '洪恩|R-004|5–6|5-6|R-014|R-019' README.md docs/requirements.md docs/ideas.md`：组合exit0。README/requirements与ideas相关研究/儿童约束已读。
- Python只读主evidence/coverage的M029行：exit0，12E/5F；新work此前不存在。之后内联Python保存基线，exit0；只写本work。
- `cat work/ihuman-controls-deepening/HANDOFF.md`曾报文件不存在；其所在多命令调用最终exit0（后续读取成功），不把该不存在文件说成已读。实际读旧fetch-pages-result.json、fetch-selected-images-result.json、static-observation-log.md及case余段。
- `rg -n '425918819985666271|aoo8one6|E-IHLC' docs/research/interaction-library/batches`：exit1，无命中；前缀与两新URL未占用。
- web工具两次search（4条具体query见exploration-log），open学校PDF/近期文章/两历史文章；无shell退出码，返回正文与失败状态如上。没有在412/验证码后再访问这些来源。
- `python3 work/ihuman-learning-controls-deepening/fetch.py`：exit0；脚本捕获412和200验证码壳，具体状态/字节/SHA保存在http-captures.json。
- 只读查看root先前browser_session.py/video_capture.py，未重跑旧脚本。`python3`检查默认环境playwright返回None；`rg --files work | rg '(playwright|pyvenv|browser.*session|browser.*notes|cdp)' | head -n 35`定位现有依赖，未安装。
- `PYTHONPATH=work/research/tools/browser python3 work/ihuman-learning-controls-deepening/inspect_video.py`：exit1，浏览器可执行文件缺失，失败发生在网络请求前。没有降级模型或第二浏览器尝试。
- 一次内联Python尝试bs4解析旧apple.html：exit1，ModuleNotFoundError。随后改用Python标准库HTMLParser复读既存HTML的五步学习/家长管理段，exit0；没有安装或重新联网。
- `python3 work/ihuman-learning-controls-deepening/write_batch.py`：两次均exit0；第二次把36目录的版本范围逐类明确。冻结后不要再运行。
- `python3 work/ihuman-learning-controls-deepening/precheck.py`：exit0，177 PASS，核旧记录未变、8字段before/replacement、无升级、引用/散列/访问边界/旧独立视觉hash、36目录与0媒体。

最终仅检查命令：`python3 work/ihuman-learning-controls-deepening/precheck.py --manifest`。其实际exit与输出另存check-results.json；只读，合并后不要重跑，因为before条件将不再成立。限定文件git diff --check另记录，未跟踪文件不依赖该命令代替专属检查。

## 状态与限制

本批只增加资料，不改变产品状态或需求。原当前主链的错答→提示→重试→结算/奖励→复习及退出恢复仍未获得；历史正确分支只缩小“完全没有具体例子”的缺口。构建/平台、失败反馈、阈值、设置生效均未知。没有把作者建议、效果自述、星星兑换或时长样例当现行规则。
