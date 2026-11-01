# M013 微信读书本批交接（2026-10-27，合并前冻结）

完成本轮有限公开资料研究，停止扩源。4新E、2新F；32目录行包含父目录/子接口/版本声明和1缺口行，不表示32项完成。原F01–06共23字段建议、7旧E各1 additional_capture。旧F07保留。3张静态图实看；无产品操作/登录/Key/API调用。没有对子代理派任务。

## 变更范围

只改案例和本批新增文件。未改旧批、master、README、requirements、ideas或汇总。两份旧批完整SHA仍与基线相同；无既有E审计散列指向本案例。新视觉审计独立文件，避免后续case编辑破坏散列。

正式9文件：
- `docs/research/interaction-library/cases/M-013-weread.md` — `37c28efc49e7d13d56677fd9a96b690548edfbd417687282868e222112997942`
- `docs/research/interaction-library/batches/weread-controls-deepening-coverage.csv` — `10d8dff7307657cc866b29006217b98760827cce874b72804459a6df4c631806`
- `docs/research/interaction-library/batches/weread-controls-deepening-evidence.jsonl` — `fce51175b8a0c642ea3c8324d0c87de66d04a8e19771c2ca9a3ac50632a121c3`
- `docs/research/interaction-library/batches/weread-controls-deepening-inventory.csv` — `84ec9ec17968b708ce0c1dbaa33e0760c1bfdbe5bc020a6bc3cd5e1746c71187`
- `docs/research/interaction-library/artifacts/weread-controls-deepening/ai-citations.jpg` — `719779a935fb8db2f8026dc0b9ad25e8ae1f691411f7ae99ec1fc47f6b717889`
- `docs/research/interaction-library/artifacts/weread-controls-deepening/ai-entry.jpg` — `73c62fe09714d808f1039fb7607d108eec87c1f3e92f7c3e5488e726d9e1d779`
- `docs/research/interaction-library/artifacts/weread-controls-deepening/ai-selection.png` — `b54658ec4346689bb94dc6cd0aa48ca9e06db1bf3d6686fdbe7542e63f06c8ca`
- `docs/research/interaction-library/artifacts/weread-controls-deepening/source-notes.md` — `7de92903a20630c9c303c128fb53fb9643a68dc6d8f943ee795754750d1af9de`
- `docs/research/interaction-library/artifacts/weread-controls-deepening/visual-audit.md` — `df779bddfeecc47283dc2577b62a3d1e6c8654c6cd783bd4564f7c56e18e1b9d`

## 合并建议

- `work/weread-controls-deepening/existing-coverage-updates.json`：逐字段before/after（不是直接改旧行）。F01从D1建议D2仅限2024历史AI问书子链；普通查词/划线保存仍未知。静态图仅static_reference_only。
- `work/weread-controls-deepening/existing-evidence-additional-captures.json`：E-WRD-01新抓商店HTML；E-WRD-03—08重读已有固定提交原件，明确local_reread而非假称新GET。原E字段不覆盖。
- 新coverage仅F-M013-08/09，Web目录搜索/深浅背景；不重复创建旧AI/推荐/笔记任务。购书/公开点评等只入目录，未为数量加F。

逐F建议字段：
- `F-M013-01`：depth, evidence_ids, known_branches, gaps, visual_status, checked_on
- `F-M013-02`：evidence_ids, known_branches, gaps, checked_on
- `F-M013-03`：evidence_ids, known_branches, gaps, checked_on
- `F-M013-04`：known_branches, gaps, checked_on
- `F-M013-05`：known_branches, gaps, checked_on
- `F-M013-06`：known_branches, gaps, checked_on

## 根复核入口与边界

1. `https://cdn.weread.qq.com/web/wrweb-next/_nuxt/C_fW_JUQ.js`：`FeedbackModal`的question/answer常量，偏移654472/654693/654909/655148；当前`homepage.html`仍引用此bundle。提取文字，没有执行JS或操作网页。4项帮助是正文；未标日期，不是手机帮助全集。F08终点限发起本书搜索，不声称看过结果/返回；F09限文档深浅状态切换。
2. `https://developer.cloud.tencent.com/article/2433150`：`ai-rag.txt`“AI问书能力介绍”节（约66行起），输入问题/回答来源/阅读片段三段；2024-07-01发表、底部原始发表2024-06-28。作者profile1920198约312行自称官方账号；不额外宣称认证徽标。历史OS/构建未知，不能套10.2.3。文章性能/准确性宣称未采作操作事实。
3. 固定提交的`discover.md`：相似推荐count/maxIdx必须显式，后页sessionId；旧`book.md`输出段要求100且finishTime；旧`notes.md`计数含书签而内容排除；旧`readdata.md`作者时长字段为字符串，不能按末尾概括机械当秒。以上只读契约，未实际调用。
4. 商店10.2.3日期2026-10-26直接来自HTML time datetime，未按相对小时倒算。10.2.2为2026-09-17。Skill README1.0.4与SKILL1.0.5差异保留；新E03/04的2026-10-10是固定提交日期，未假称每篇文档的更新时间。

静态图分工：E-WRCD-02的`ai-entry.jpg`（1080×2126，查看工具缩为1040×2048）只证明问答及引用编号；`ai-citations.jpg`（1080×1665）仅来源列表；`ai-selection.png`（798×448）仅选区菜单。来源列表点击跳书段仅由文章文字支持；未看到返回原位。其余图未看、未登记。无D3/D4。

## 原件与散列

`work/weread-controls-deepening/file-hashes.json`记录正式文件、全部本批工作文件和7份重读旧原件。HANDOFF与清单自身不纳入循环散列。以下是10次公开GET的原件；均HTTP200，捕获命令退出0：

| 原件 | SHA256 | 原URL |
| --- | --- | --- |
| `work/weread-controls-deepening/web-entry.js` | `5026291bf1a7cd968ce07ec4b5d42f94ffc49ff7b85d5065f41b65c7b19e3ad7` | https://cdn.weread.qq.com/web/wrweb-next/_nuxt/C_fW_JUQ.js |
| `work/weread-controls-deepening/appstore.html` | `a6121bbc1bf47883dd30e3d8be7cf54d3d7493c8f392f85cff41e68f4e4bc0d0` | https://apps.apple.com/cn/app/id952059546 |
| `work/weread-controls-deepening/homepage.html` | `64dd03ca8670ac06e79dfad921f438d50b4e07473b03972677618bbbab2ef1dc` | https://weread.qq.com/ |
| `work/weread-controls-deepening/ai-rag.html` | `465d18a923998c8f637beb6714d21b166ed249581804395748e6a0bb3d0f64dd` | https://developer.cloud.tencent.com/article/2433150 |
| `work/weread-controls-deepening/review.md` | `8a86c3a70b05900b408ebec554de0e4177e9f0c8105f8d7e33ada693fd13a31f` | https://raw.githubusercontent.com/Tencent/WeChatReading/d98e66ee60a8c7bcd35da549d0643c4470439643/skills/review.md |
| `work/weread-controls-deepening/discover.md` | `81817b64669596dee8c168cce2d5dd0c8ccc13524ca6d5dec3224acca674b7a3` | https://raw.githubusercontent.com/Tencent/WeChatReading/d98e66ee60a8c7bcd35da549d0643c4470439643/skills/discover.md |
| `work/weread-controls-deepening/author.html` | `7ffc2af8d67475be10c5b4adf6f5d2477b2aeaba4f5e7f4ae1ba433c0832f4ee` | https://developer.cloud.tencent.com/user/1920198 |
| `work/weread-controls-deepening/ai-entry.jpg` | `73c62fe09714d808f1039fb7607d108eec87c1f3e92f7c3e5488e726d9e1d779` | https://developer.qcloudimg.com/http-save/yehe-1920198/d96def10e5ee4b5d9dac45a0180444d1.jpg |
| `work/weread-controls-deepening/ai-citations.jpg` | `719779a935fb8db2f8026dc0b9ad25e8ae1f691411f7ae99ec1fc47f6b717889` | https://developer.qcloudimg.com/http-save/yehe-1920198/306b66945599a14d56f79608071efbd9.jpg |
| `work/weread-controls-deepening/ai-selection.png` | `b54658ec4346689bb94dc6cd0aa48ca9e06db1bf3d6686fdbe7542e63f06c8ca` | https://developer.qcloudimg.com/http-save/yehe-1920198/255680d8155539d3a0b50e8776bab732.png |

## 精确主要命令与结果

读取/定位包括：`sed -n '1,220p' docs/research/interaction-library/cases/M-013-weread.md`、`cat work/chinese-apps/wrd-book.md work/chinese-apps/wrd-readdata.md`、`cat work/weread-controls-deepening/discover.md work/weread-controls-deepening/review.md`、`rg -n '^##|^###' work/chinese-apps/wrd-SKILL.md work/chinese-apps/wrd-notes.md`，及Python读取baseline/旧coverage/E与原件；成功项exit0。README/requirements/ideas/项目约定已在取证前读取。未改其内容。

每次实际网络取得命令如下（无远程音视频/安装包）：
```sh
python3 work/weread-controls-deepening/capture.py web-entry.js https://cdn.weread.qq.com/web/wrweb-next/_nuxt/C_fW_JUQ.js
python3 work/weread-controls-deepening/capture.py appstore.html https://apps.apple.com/cn/app/id952059546
python3 work/weread-controls-deepening/capture.py homepage.html https://weread.qq.com/
python3 work/weread-controls-deepening/capture.py ai-rag.html https://developer.cloud.tencent.com/article/2433150
python3 work/weread-controls-deepening/capture.py review.md https://raw.githubusercontent.com/Tencent/WeChatReading/d98e66ee60a8c7bcd35da549d0643c4470439643/skills/review.md
python3 work/weread-controls-deepening/capture.py discover.md https://raw.githubusercontent.com/Tencent/WeChatReading/d98e66ee60a8c7bcd35da549d0643c4470439643/skills/discover.md
python3 work/weread-controls-deepening/capture.py author.html https://developer.cloud.tencent.com/user/1920198
python3 work/weread-controls-deepening/capture.py ai-entry.jpg https://developer.qcloudimg.com/http-save/yehe-1920198/d96def10e5ee4b5d9dac45a0180444d1.jpg
python3 work/weread-controls-deepening/capture.py ai-citations.jpg https://developer.qcloudimg.com/http-save/yehe-1920198/306b66945599a14d56f79608071efbd9.jpg
python3 work/weread-controls-deepening/capture.py ai-selection.png https://developer.qcloudimg.com/http-save/yehe-1920198/255680d8155539d3a0b50e8776bab732.png
```

`capture.py`只保存普通GET响应，遇异常会记录HTTP状态；本轮没有失败状态，不存在用shell exit0掩盖HTTP403的情况。搜索是web工具查询公开帮助/官方AI/导入关键词；第三方封装/聚合FAQ和下载站排除，腾讯客服首页不是产品专属帮助目录。没有访问登录后页面或复试历史403路线。

本轮落盘/检查：
```sh
python3 work/weread-controls-deepening/build.py
python3 work/weread-controls-deepening/check.py
python3 work/weread-controls-deepening/check.py > work/weread-controls-deepening/check-result.txt
file work/weread-controls-deepening/ai-entry.jpg work/weread-controls-deepening/ai-citations.jpg work/weread-controls-deepening/ai-selection.png
python3 work/weread-controls-deepening/make-handoff.py
```

构建三次均exit0（最后一次明确commit日期字段）；初版检查逐旧E做重复断言10417 PASS，后改为集合核重避免夸大数量，最终257 PASS/exit0。这是文件/引用/散列/建议before验证，不是产品功能测试。未运行全局check_docs，因为主表由root合并；旧批/前几轮脚本没有重跑。实际view_image三图成功，不是shell命令。

已处理的本地探测错误：一次Python误用baseline['coverage']产生KeyError（该子过程exit1，但随后cat使组合shell最终exit0），随即用真实键flows重读成功；一次sed猜错旧批建议文件名，组合命令exit2；一次rg探测不存在scripts/tools目录报路径错误（管道/后续命令最终exit0），随后定位真实work/research/interaction-depth/check_docs.py只读成功。均不是来源访问失败、未造成正式文件变化；保留而不将它们写作成功检索。

## 可重复合并前预检及停止条件

在root未应用旧before建议时运行`python3 work/weread-controls-deepening/check.py`应257 PASS/exit0。检查本批字段/路径/LF、4E与全批ID/URL去重、9F案例映射、32行目录引用、原件/正式/视觉SHA、23建议before以及7旧E现值。根合并后不要再运行build或依赖before的check；可按file-hashes逐项只读验证并运行根自己的全局检查。

## 未解决

手机完整帮助目录未取得。导入文件格式/重复/失败回滚、查词反馈/误划线撤销、离线听书/中断位置/读听转换、重绑后的身份合并、备份与恢复仍缺官方完整链。历史AI是否沿用当前版本、Web帮助何时更新及保存/返回分支未知。Skill公开讨论相关正文可继续分析，但不替代手机操作；没有权限/网络/账号行为的实测结果。下一类材料应是固定手机版本官方帮助或授权连续观察，不继续重复泛搜。

状态：本地专属研究资料已改变；产品、账号、外部服务状态未改变。根负责核实和合并；交付后冻结正式文件及建议JSON。
