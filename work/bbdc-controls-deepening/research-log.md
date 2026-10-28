# M018 本轮命令、取证与停止记录

日期 2026-10-27。工作目录 `/home/workspace/reader`。只有文档和本地原件写入；没有登录、发送、安装、下载 App/音视频或调用学习/账户接口。没有新图像观察。未调用 helper。

## 本地读取与写入

- 读取 `README.md`、`docs/requirements.md`，按不背/词书/同步/备份/掌握检索 `docs/ideas.md`；读取 `cases/M-018-bbdc.md`、`template.md`、`batches/README.md`、旧 `vocab-*` 与 `chinese-vocab-deepening-*`。常用命令为 `cat`、`sed -n`、`rg -n`、`rg --files`；这些读取退出 0。读取旧 `work/chinese-vocab-deepening/research-log.md` 后没有重跑旧脚本或旧失败路线。
- 一次错误读取命令 `sed -n '1,140p' work/youdao-controls-deepening/existing-evidence-updates.json && cat work/youdao-controls-deepening/existing-coverage-updates.json && head -n 4 docs/research/interaction-library/batches/youdao-controls-deepening-inventory.csv && sed -n '1,80p' docs/research/interaction-library/template.md && sed -n '1,110p' work/bbdc-controls-deepening/appstore-cn.txt`：首个文件不存在，退出 2，后续因 `&&` 未执行。随后 `rg --files work/youdao-controls-deepening work/ibasso-controls-deepening | rg 'updates|capture|inventory|HANDOFF'` 找到实际 `additional-captures-updates.json`；只读格式参考，退出 0，没有运行那些批的脚本。
- `python3 - <<'PY' … PY` 内联读取全部批次，保存 `case-before.md`、`baseline.json`；全库核前缀 E-BBDCD 未占用、M018 为 9E/5F、无案例视觉审计反向依赖：退出 0。前缀只预留，本轮未发新编号。
- `python3 - <<'PY' … PY` 内联 `HTMLParser` 提取原 HTML 文本、公开导航、内嵌 JSON；生成 `appstore-cn-embedded.json` 与 `appstore-version-records.json`，后者 25 个版本对象：退出 0。没有执行网页脚本。
- `python3 work/bbdc-controls-deepening/build.py`：退出 0，建立 5 个正式文件（含原案例修改），0E/0F、34 项目录、4F/12 字段初稿与 3E/4capture 建议。
- 随后 `python3 - <<'PY' … PY` 只作本轮字段修正：补采集版本/日期，校正案例的本轮/上一批用语，F04 加 E-BBD-01 支持凌晨日界；更新建议为 **4F/13字段**。退出 0。`build.py` 是初次生成记录，不重跑覆盖后续校订。
- `python3 work/bbdc-controls-deepening/check.py`：首次 2751/2751 PASS、退出 0；F04 引用修订后再次检查，结果以 `validation.json` 为准。检查只校文档/散列/字段，不是产品测试；大多数断言为全库审计反向依赖检查。
- `git diff --check -- docs/research/interaction-library/cases/M-018-bbdc.md docs/research/interaction-library/batches/bbdc-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/bbdc-controls-deepening-coverage.csv docs/research/interaction-library/batches/bbdc-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/bbdc-controls-deepening`：退出 0。范围在 Git 中未跟踪，空 diff 不能替代内容检查。
- 一次 `diff -u work/bbdc-controls-deepening/case-before.md docs/research/interaction-library/cases/M-018-bbdc.md; git status --short -- …` 输出预期案例差异和五个未跟踪路径，组合命令退出 0 来自末尾 git；不把它解释为两份案例相同。最终单独获取 diff 子进程退出码保存在 `final-diff-status.json`。

## 匿名公开请求

使用内联 Python `urllib.request.urlopen(Request(url, headers={'User-Agent':'Mozilla/5.0'}), timeout=30)`，每目标一次，HTMLParser 仅解析本地响应。没有被拒后换方式重试：

| URL / 取得方式 | 结果 | 本地原件 |
| --- | --- | --- |
| `https://www.bbdc.cn/index`，HTTP GET | 200，35980 字节 | `index.html`，另有解析文本；登录表单隐藏节点不当视觉观察 |
| `https://learnywhere.cn/lexis/privacy`，HTTP GET | 200，976 字节脚本壳 | `privacy.html`；无正文 |
| `https://apps.apple.com/cn/app/不背单词-四六级考研等英语单词学习/id698570469?platform=mac`，URL 字符以 urllib.parse.quote 编码后 GET | 200，655769 字节 | `appstore-cn.html`；没有下载其中媒体或脚本 |

`captures.json` 保存 URL、实际响应 URL、大小和 SHA-256。App Store 另以 `web.open` 取得正文；政策 `web.open` 仅壳，搜索结果却有政策文字。`privacy-web-search-response.json` 是最后一次精确查询的工具响应序列化，**不是原站 HTTP 正文**；其 `.txt` 只是去 JSON 包装的派生文本。正式源摘记保留这一差别。

上轮已记录的 `app.8a73937b.js` 403、知乎 403、微博 robots 和自制词书登录路线本轮均未重试；官网 HTML 只再次确认链接。没有下载或执行网页中任何 App、APK、音视频。没有把权限说明推定为口语测评/听写的真实界面。

## 有限搜索范围

以下 `web.search_query` 为只读搜索；没有 shell exit 码。除最后一项保存精确工具响应外，其余仅作为发现/排除，不新建 E。第一项 `site.bbdc.cn` 写法错误，后面以 `site:bbdc.cn` 正确条件补查。

1. `site.bbdc.cn "帮助" "词组"`
2. `site:learnywhere.cn 不背 单词 帮助 -privacy -agreement`
3. `不背单词 官方 帮助中心 词组 拼写 撤销`
4. `site:bbdc.cn 帮助 词组 -kids`
5. `site:learnywhere.cn "不背" "help"`
6. `"不背单词" "官方" "帮助中心" "词书"`
7. `"不背单词" "找回" "官方"`
8. `"不背单词App" "听写" site:mp.weixin.qq.com`
9. `"不背单词" "词组" site:learnywhere.cn`
10. `"不背单词" "帮助" site:bbdc.cn -kids -privacy`
11. `"不背单词" "帮助" site:is-cool.cn`
12. `"不背单词App" "学习计划" 官方教程`
13. `"不背单词App" "词组" "教程"`
14. `"不背单词" "帮助文档"`
15. `"不背单词App" "复习" site:bilibili.com`
16. `site:learnywhere.cn/lexis/privacy "更新日期" "第三方"`
17. `site:learnywhere.cn "不背" "账号设置" "密码"`
18. `site:bbdc.cn 不背 "听写" "复习"`
19. `"不背单词App" 官方 "导出PDF"`
20. `site:learnywhere.cn/lexis/privacy "账号设置" "账号信息" "老用户"`

8–11 合并查询返回空结果。其余主要返回既有官网/商店/政策，或第三方教程/下载页/分析论文；没有把它们编入官方流程。B 站搜索页出现账号自称官方及纪念片线索，但没有实际查看账号主页或观看，未作为功能证据。其它相似品牌、小小不背和轻听的操作不合并到本 App。

## 正式限制与下一证据

本轮不是完整帮助目录抓取。34 项台账分开已有来源的能力声明、政策入口和网页导航，最后 3 项明确是尚无入口的缺口。下一有效来源是带端/版本的官方逐题帮助或连续教程，包括计划保存、误评纠正、提示后重试、错词结束、离线/跨日中断及换机恢复；继续复制产品卖点不能提升深度。
