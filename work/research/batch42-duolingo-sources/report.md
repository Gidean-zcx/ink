# Batch 42：Duolingo Blog 三篇来源采集

日期：2026-10-27。仅匿名公开 GET；没有登录、安装、图片/动图/视频下载，也没有修改 `docs/`。

## 既有证据复用核对

对正式 `docs/` 与现有 `work/` 下的 `*evidence.jsonl` 检索三个完整 URL，未命中任何 E 编号。`rg` 因无匹配返回 exit 1；因此三个 URL 均按任务要求各执行一次普通 GET。

## 采集结果

### Duolingo Launches a New Math App for Students and Adults

- URL：`https://blog.duolingo.com/duolingo-launches-math-app/`
- GET：1 次；HTTP 200；无重定向；Content-Type `text/html; charset=utf-8`。
- canonical：`https://blog.duolingo.com/duolingo-launches-math-app/`。
- 正文：已取得，6208 字符；`duolingo-launches-math-app.body.txt`；SHA-256 `731842d7fb17298ce8c25c1c6b852d037d6ed07d8da45b1f3cde69e702d08a90`。
- 原 HTML：`duolingo-launches-math-app.html`，117411 bytes；SHA-256 `5a08a930290eef208bf9bd12626751c26694087e3126781b884fcc86cb0cd3da`。
- 图片 URL：2 条，保存于 `duolingo-launches-math-app.images.txt`；未下载媒体；SHA-256 `8dfa94cbbc50b0a889050c0f6404c6b683be6fc2c6ebd0a114b21a58e1320524`。
- JSON-LD：1 块，`duolingo-launches-math-app.jsonld.json`；发布日期 `2022-10-26T04:01:00.000Z`；修改日期 `2026-10-01T10:05:06.000Z`；SHA-256 `8671d7718be83961d12ddc42f7f6909c79be294729fac3dbe3adeb67c38a8b17`。
- 响应头：`duolingo-launches-math-app.headers.json`；SHA-256 `764a35e830d7610ed9bf08929608b7ce70dc59da3b1e613382f1ef357368008f`。
- 未知：日期字段由当前页面 JSON-LD 声明，未另行验证编辑历史；HTML 没有提供 App 构建号；媒体内容未打开或核验。

### Teaching Owlgebra: developing math on Duolingo

- URL：`https://blog.duolingo.com/developing-math/`
- GET：1 次；HTTP 200；无重定向；Content-Type `text/html; charset=utf-8`。
- canonical：`https://blog.duolingo.com/developing-math/`。
- 正文：已取得，5819 字符；`developing-math.body.txt`；SHA-256 `9329fff60511cae215ee3b3e68818957deeb48f99669c960707ba1f60a3bb04e`。
- 原 HTML：`developing-math.html`，117282 bytes；SHA-256 `6532bb81e17d750828a987b0fcd3b5fcb37547c206cce415f9c746c1b071ec73`。
- 图片 URL：7 条，保存于 `developing-math.images.txt`；未下载媒体；SHA-256 `993332e6e5d36785da96b1121ec7f2facdda01398f40e37f19827d9e6d2f4fa2`。
- JSON-LD：1 块，`developing-math.jsonld.json`；发布日期 `2024-10-18T21:00:00.000Z`；修改日期 `2026-09-18T10:07:13.000Z`；SHA-256 `8ccb5409106063c290ae035b13cf25973a96dbfe38490db4ceab7aa92286fd8c`。
- 响应头：`developing-math.headers.json`；SHA-256 `16a106d5e6f63e05b2441a3838a806078ad0923ea6b716e5602f58bdcead27ef`。
- 未知：日期字段由当前页面 JSON-LD 声明，未另行验证编辑历史；HTML 没有提供 App 构建号；媒体内容未打开或核验。

### Duolingo Music Is Here - Learn More About the New Subject

- URL：`https://blog.duolingo.com/music-course/`
- GET：1 次；HTTP 200；无重定向；Content-Type `text/html; charset=utf-8`。
- canonical：`https://blog.duolingo.com/music-course/`。
- 正文：已取得，3927 字符；`music-course.body.txt`；SHA-256 `c18d7527709e0217dd24b5d181efbafb6ce2142d524af8cb352cf4396e07ce8a`。
- 原 HTML：`music-course.html`，117008 bytes；SHA-256 `88a5aad07aece46fba3a9cbb026bd3f8e649cf58668811f1c7484b6aabc87487`。
- 图片 URL：11 条，保存于 `music-course.images.txt`；未下载媒体；SHA-256 `f6a63dc014c801f2bdb092f8b3939b9c3d3afcc4da863b4f9dc210f7a3654f01`。
- JSON-LD：1 块，`music-course.jsonld.json`；发布日期 `2023-10-11T05:00:57.000Z`；修改日期 `2025-01-16T08:24:27.000Z`；SHA-256 `65c05d1cec6a3fe648c64203bb241ff25574968edb76ace1fb2a8eb50499cd4e`。
- 响应头：`music-course.headers.json`；SHA-256 `1158f93353a2fc61da197bd7b7b36971b0507227c6d9a23cedcc21773d205ee6`。
- 未知：日期字段由当前页面 JSON-LD 声明，未另行验证编辑历史；HTML 没有提供 App 构建号；媒体内容未打开或核验。

## 正文抽取范围

正文从已保存 HTML 的第一个 `article` 元素提取；排除 `script`、`style`、`noscript`、`svg`，相邻完全重复行折叠。标题、日期、作者及文章正文保留。页面导航、页脚和推荐文章未写入正文文件。图片列表只收集该 HTML 中 `img` 的 `src`、延迟加载地址与 `srcset` URL；没有网络下载这些媒体。

## 命令与退出状态

- 基线命令：`pwd`、列出 evidence 文件，并以 `rg -n -F` 搜索三个完整 URL。整体 exit 1；工作目录正确，exit 1 表示 URL 无匹配。
- `/tmp/batch42_fetch.py`：以 Python `requests.get` 默认匿名请求逐 URL 各一次，保存 HTML/headers 并解析 canonical、JSON-LD、图片 URL；exit 0。三个请求均 HTTP 200，无 401/403/429/验证页。
- 可选解析器探测：检查 `bs4` 并尝试使用，exit 1，证据为 `ModuleNotFoundError`；未安装依赖、未改文件，随后使用 Python 标准库处理。
- `/tmp/batch42_reextract.py`：仅重读已保存 HTML，去除正文中的脚本/样式文本并更新正文 SHA；exit 0；没有额外网络请求。
- 最终 `python3 -m json.tool results.json`、文件/SHA 一致性检查、`git diff --check`：见最终验证，均应为 exit 0。

## 状态变化

- 新增工作目录文件；未改正式文档或索引。
- 外部状态未改变；只发生三次匿名公开 GET。
- 图片、GIF、视频均未下载。
