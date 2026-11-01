# 墨案官方媒体抽样：失败交接

日期：2026-10-27。按有界任务执行一次后停止；未联网重试，没有修改 `docs/`，没有保存 GIF/MP4 原文件。

## 执行结果

- 候选：`work/research/batch36-moaan/media-candidates.json` 中 `alt` 非空的 9 个官方 `img` URL。
- 浏览器：现有 Chromium `156.0.8078.4`；`PYTHONPATH=work/research/tools/browser`，`PLAYWRIGHT_BROWSERS_PATH=work/research/tools/browser-binaries`。
- 脚本为第 1 个 URL 建立了 `<img>`：`http://www.moaansmart.com/data/upload/9/e7/9e7d7f996484795dfddfceb11a74881c.gif`（方法一.gif），随后立即在等待条件调用处失败。响应事件数组为空，因此 HTTP 状态、类型以及请求是否完成均未知。
- 在取得尺寸和截图之前，本地脚本调用 `Page.wait_for_function()` 时发生参数签名错误：`TypeError: Page.wait_for_function() takes 2 positional arguments but 3 positional arguments (and 1 keyword-only argument) were given`。
- 脚本按失败即停策略退出 1。未继续其余 8 个 URL，也没有重新请求第 1 个 URL。`samples/` 中没有截图文件。
- 这是本地代码错误；没有证据表明站点拒绝、挑战或 HTTP 访问限制，也没有成功响应记录。不能声称 GIF 已加载、播放或呈现任何画面。

## 精确命令与退出状态

1. `cat work/research/batch36-moaan/media-candidates.json`，并用 `rg` 查找已有 Playwright 捕获范例：exit 0。
2. 创建一次性脚本 `work/research/batch36-moaan/capture_media_once.py`，随后执行：

   `PYTHONPATH=work/research/tools/browser PLAYWRIGHT_BROWSERS_PATH=work/research/tools/browser-binaries python3 work/research/batch36-moaan/capture_media_once.py`

   exit 1；精确错误如上。脚本写出 `work/research/batch36-moaan/media-sampling.json` 后终止。
3. `cat work/research/batch36-moaan/media-sampling.json` 并列出 `samples/`：用于核对失败状态和无截图；exit 0。

## 生成文件与状态变化

- `work/research/batch36-moaan/capture_media_once.py`：一次性采样脚本，内含 `media-sampling.json` 存在即拒绝重跑的保护。
- `work/research/batch36-moaan/media-sampling.json`：浏览器版本、首个URL空响应数组、错误和停止原因。
- `work/research/batch36-moaan/media-sampling-report.md`：本交接。
- `work/research/batch36-moaan/samples/`：已创建但为空。

没有修改正式资料。由于第一个 URL 已发生一次正常请求，本代理不修正脚本重跑，以免违反“每 URL 一次正常加载”和“代码失败后停，让 root 接手一次”的约束。

## 未完成

9 个媒体均未取得 0／2／5 秒元素截图、自然尺寸和截图哈希；第 1 个是否完成网络请求未知，其余 8 个没有进入加载步骤。root 若接手，应根据“每URL一次”约束保守决定是否跳过第 1 个 URL；现有日志不构成视觉证据。
