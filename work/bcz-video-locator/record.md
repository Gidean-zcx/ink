# 百词斩教程 892 媒体地址定位记录

- 查阅日期：2026-10-27（Asia/Shanghai）
- 已知官方教程：https://www.baicizhan.com/little_class/article/892/share_view
- 范围：只定位公开 HTML 中的媒体标签、媒体地址和型号文字；没有请求或播放媒体。

## 本地既有资料检查

查阅 `AGENTS.md`、`README.md`、`docs/requirements.md`、`docs/research/interaction-library/cases/M-022-baicizhan-device.md`，并检索 `work/` 与 `docs/research/` 中的 `article/892`、`E-BCDVC-01`、视频标签和百词斩相关文件。既有资料只保存书目/文字摘记和“内嵌视频未观看”的边界；没有发现教程 892 的 HTML 原件或已定位的媒体 URL。旧记录明确硬件型号未标，不能映射到 Mini/Max。

## 一次匿名 GET

命令：

```sh
mkdir -p work/bcz-video-locator && curl --silent --show-error --location --max-redirs 5 --connect-timeout 15 --max-time 45 --dump-header work/bcz-video-locator/article-892.headers --output work/bcz-video-locator/article-892.html --write-out 'http_code=%{http_code}\nurl_effective=%{url_effective}\ncontent_type=%{content_type}\nsize_download=%{size_download}\nnum_redirects=%{num_redirects}\n' 'https://www.baicizhan.com/little_class/article/892/share_view' | tee work/bcz-video-locator/curl-result.txt
```

退出状态：0。

结果：HTTP 200；最终 URL 与请求 URL 相同；`text/html; charset=utf-8`；下载 30023 bytes；0 次重定向。

页面原件：`article-892.html`，SHA-256 `0edec306e2f287cb77ce5e6456a5dd5a4f7029ed34f4ab512dc9b55410be6d33`。

## 离线定位结果

- `article-892.html:1088` 有 1 个 `<video id='video_0'>`，其内有 1 个 `<source>`。
- 公开媒体候选：`https://7n.bczcdn.com/editer_uploads/media/202211/75f293134700.mp4`，标签声明 `type='video/mp4'`。
- 同行还有播放封面图 `https://7n.bczcdn.com/bcztv/bcztv_20160801_ada.png`；`article-892.html:1110` 的页面脚本会在点击封面后显示 video 并调用 `play()`。本次没有点击、播放或请求 MP4/封面。
- `<iframe>`：0；其他 `.mp4`/`.m3u8`/`.webm` 地址：0。
- 标题与正文明确写“单词机单词本使用教程”“单词机”，发布日期在 `article-892.html:1082` 为 `2022年11月8日`。
- 页面没有 `Mini`、`Max`、`BCZ-Ace`、`BCZ-Ace-1S` 或 `BCZ-Ace-Max` 字样。因此媒体对应的具体硬件型号仍不明确，不能据页面 HTML 归入 Mini 或 Max。

## 边界与失败

没有网络失败。没有请求媒体文件，所以没有媒体文件 HTTP 状态、大小或散列；没有通过视频画面核查设备型号。没有运行旧脚本，也没有访问 52audio、京东、抖音或 dcj2 路线。
