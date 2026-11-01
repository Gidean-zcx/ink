# Ground News 四篇帮助正文抓取报告

- 时间：2026-10-27（UTC 请求时间见各 `*-status.json`）
- 范围：仅 `help.ground.news` 四个已核 article URL；每个 URL 恰好一次匿名 GET，分两批、每批最多 2 并发，连接超时 10 秒、读取超时 25 秒。
- 结果：四篇均 HTTP 200、`text/html`，未登录、未提交表单、未下载图片、未访问 `ground.news` 产品搜索。

## 响应与正文状态

| ID | HTTP / bytes / SHA-256 | 帮助页更新时间 | 提取状态 | 正文边界 |
| --- | --- | --- | --- | --- |
| 335105 | 200 / 103528 / `a6fa6a035b6e2b80cb9a9585afb6a49cc2088ffcd47584f9fb087a9fc95cdefb` | Monday, March 16 2026 | 24 行；14 个文章区链接；0 个文章区图片 URL | 只提取 `.article-content`，不含导航、反馈按钮和脚本 |
| 648513 | 200 / 103947 / `ee924539f9fa2261c4682f11b70ed3371365643b64d67783cb919d79554c2235` | Monday, March 9 2026 | 27 行；12 个文章区链接；1 个文章区图片 URL | 只提取 `.article-content`，不含导航、反馈按钮和脚本 |
| 485057 | 200 / 103957 / `529c89b97eeab5b0f074341e6b1b88f39198b768d1550740966ec0b865c31930` | Monday, March 16 2026 | 28 行；12 个文章区链接；0 个文章区图片 URL | 只提取 `.article-content`，不含导航、反馈按钮和脚本 |
| 593857 | 200 / 97266 / `779fb2cc82544fd5bb163dcd475bacd7dae187d5e2248efe0ee8233cf4c6c948` | Monday, March 9 2026 | 2 行；1 个文章区链接；0 个文章区图片 URL | 只提取 `.article-content`，不含导航、反馈按钮和脚本 |

正文、链接和图片 URL 分别保存在 `<id>-body.txt`、`<id>-links.json`、`<id>-images.json`。原始响应与头信息保存在 `<id>.html` 和 `<id>-headers.json`；未请求图片 URL。

## 停止条件核查

第一批脚本把 HTML 的 Content-Security-Policy 中通用 `recaptcha` 域名误判成挑战信号，因而先停止。对已保存 HTML 的本地检查确认：两页都有正常标题、编辑日期和 `.article-content`；`window.RecaptchaSiteKey` 为空，没有挑战页面文案。修正检测条件后才请求第二批；第一批没有重试。四个响应均未出现 401、403、412、429 或实际挑战页。

## 命令与退出状态

1. `sed -n '1,220p' work/research/batch41-ground/inventory-audit.md`：exit 0。
2. `python3 work/research/batch41-ground/fetch.py`：exit 0；第一批两个 URL 各请求一次，保守停止。
3. `rg` 检查已保存 HTML 的挑战标记、标题和 `.article-content`：exit 0；确认 `recaptcha` 只在正常 CSP/配置中。
4. `python3 work/research/batch41-ground/extract_saved.py`：exit 0；本地提取第一批正文、文章区链接与图片 URL。
5. `python3 work/research/batch41-ground/fetch-second-batch.py`：exit 0；第二批两个 URL 各请求一次。
6. `python3 work/research/batch41-ground/extract-second-batch.py`：exit 0；按同一文章区边界重新提取第二批。
7. `python3` 合并四份状态为 `results.json` 并生成本报告：exit 0。

## 未知与状态变化

- 未知：页面内截图的实际视觉内容；按约束只登记图片 URL，没有下载。
- 未知：帮助正文是否与特定 App 构建完全一致；页面只提供帮助页编辑日期。
- 未进行产品操作，因此无法验证按钮反馈、返回位置、同步或异常恢复。
- 状态变化仅限 `work/research/batch41-ground/` 新增抓取、提取、状态和报告文件；未修改 `docs/`。
