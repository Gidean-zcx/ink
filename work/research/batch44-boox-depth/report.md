# Batch 44：BOOX 官方帮助静态附件采集

日期：2026-10-27。只执行普通匿名 GET 并记录元数据；没有登录、账户操作或修改 `docs/`。本任务没有打开或查看图片内容，因此不得把下载称为视觉观察。

## 结果

### home

- URL：`https://booxsupport.zendesk.com/hc/article_attachments/10725493990676`
- HTTP：200；Content-Type：`image/jpeg`；无重定向。
- 大小：188317 bytes；SHA-256：`239d7b8fc1560807591d481f7c02c56dc07e212766e003a56514d45a858b5e51`。
- 本地路径：`work/research/batch44-boox-depth/images/home.jpg`。
- 失败：无。
- 本地出处核对：附件 ID 在 `work/eink-systems-research/boox-articles.json` 中存在。

### screen-record

- URL：`https://booxsupport.zendesk.com/hc/article_attachments/25403784340116`
- HTTP：200；Content-Type：`image/jpeg`；无重定向。
- 大小：165100 bytes；SHA-256：`575deec83ec0e8460aca3dbc9d6c7f948486f66b078f6f939589ca7cf6b83ed5`。
- 本地路径：`work/research/batch44-boox-depth/images/screen-record.jpg`。
- 失败：无。
- 本地出处核对：附件 ID 在 `work/eink-systems-research/boox-articles.json` 中存在。

### volume

- URL：`https://booxsupport.zendesk.com/hc/article_attachments/25175232660116`
- HTTP：200；Content-Type：`image/jpeg`；无重定向。
- 大小：326114 bytes；SHA-256：`5e16b6ee8f0fd0fd532d498bc7baf0a7413f4a52b8bcb2a5609688c8acf1f3e1`。
- 本地路径：`work/research/batch44-boox-depth/images/volume.jpg`。
- 失败：无。
- 本地出处核对：附件 ID 在 `work/eink-systems-research/boox-articles.json` 中存在。

### screensaver-types

- URL：`https://booxsupport.zendesk.com/hc/article_attachments/10726276107156`
- HTTP：200；Content-Type：`image/jpeg`；无重定向。
- 大小：164851 bytes；SHA-256：`fae41d5c31af195fc58fc9aebc83dffbe0c77a0c5122c73176fd5520a8447b63`。
- 本地路径：`work/research/batch44-boox-depth/images/screensaver-types.jpg`。
- 失败：无。
- 本地出处核对：附件 ID 在 `work/eink-systems-research/boox-articles.json` 中存在。

### screensaver-images

- URL：`https://booxsupport.zendesk.com/hc/article_attachments/10726306716820`
- HTTP：200；Content-Type：`image/jpeg`；无重定向。
- 大小：129013 bytes；SHA-256：`c03c9b8127d881c260589f821a7b23082f4e88fd753ff97e420c4cd4b47e36af`。
- 本地路径：`work/research/batch44-boox-depth/images/screensaver-images.jpg`。
- 失败：无。
- 本地出处核对：附件 ID 在 `work/eink-systems-research/boox-articles.json` 中存在。

### mirror-step

- URL：`https://booxsupport.zendesk.com/hc/article_attachments/27963633162900`
- HTTP：200；Content-Type：`image/jpeg`；无重定向。
- 大小：309687 bytes；SHA-256：`1b5046a0fc79e6f12a2bde2d224c41030dcff2d77b8b890279e8c11ebe8933cd`。
- 本地路径：`work/research/batch44-boox-depth/images/mirror-step.jpg`。
- 失败：无。
- 本地出处核对：附件 ID 在 `work/eink-systems-research/boox-articles.json` 中存在。

### mirror-browser

- URL：`https://booxsupport.zendesk.com/hc/article_attachments/27963693945876`
- HTTP：200；Content-Type：`image/jpeg`；无重定向。
- 大小：177188 bytes；SHA-256：`42c1bc6ba4de5982ddc3a0e9a81083c654824f2cb59128517d50f4b18c275d4f`。
- 本地路径：`work/research/batch44-boox-depth/images/mirror-browser.jpg`。
- 失败：无。
- 本地出处核对：附件 ID 在 `work/eink-systems-research/boox-articles.json` 中存在。

## 边界

- 七个文件均为 `image/jpeg`，单文件均小于 3 MB。没有 GIF 或视频请求。
- 七个 URL 各请求一次；均 HTTP 200，无 401/403/412/429 或验证页，因此没有触发停止主机条件。
- `captures.json` 只记录获取与元数据；尚未进行视觉检查、控件解读或机型适用判断。
- 现有文章正文缓存只用于确认附件 ID 出处；未改写 sources 或正式证据。

## 命令与退出码

- `/tmp/batch44_images.py`：exit 0。依给定顺序逐 URL 调用一次 `requests.get(..., stream=True)`；3 MB 上限；拒绝 GIF/视频；写入图片和 `captures.json`。
- Python 本地出处检索：包含在同一脚本中；exit 0；七个附件 ID 均命中 `work/eink-systems-research/boox-articles.json`。
- 最终 `python3 -m json.tool captures.json`、请求计数/HTTP/content-type/大小/SHA复算、`file` 类型检查及 `git diff --check`：见验证输出。

## 状态变化

- 新增 `work/research/batch44-boox-depth/captures.json`、本报告及 `images/` 下七张 JPEG。
- 正式文档与原本地缓存未修改；外部账户状态未改变。外部只发生七次匿名 GET。
