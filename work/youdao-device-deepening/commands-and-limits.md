# M025本轮命令与边界

日期2026-10-26；只作公开文档研究。

- 读取：`cat AGENTS.md README.md docs/requirements.md docs/research/interaction-library/template.md docs/research/interaction-library/cases/M-025-youdao-pen.md`，exit 0。
- 原证据和编号：`rg E-YDP-`及`rg ^F-M025`定位dictionary旧批次；`rg E-YDCD docs/research/interaction-library/batches`在写入前exit 1，表示此前无此编号。
- 公开资料：web.search/open查看有道技术博客和两校借用公告；官方域说明书/收藏/使用引导末轮检索无结果。广外web.open后续一次超时；已有HTML请求200，未再重复访问正文。
- 原图：`timeout 45s python3 -u -`中的ThreadPoolExecutor(max_workers=3)匿名GET广外正文链接1/4/5，工具进程session 19528最终exit 0；三图均HTTP 200 image/png。原始URL见gdufs-image-urls.json，图片字节与SHA见批次和case审计。
- 静态观察：view_image检查1/4/5图，另对5图用original；不声称观看视频，不进行图像增强或补写细小文字。
- 写入：`python3 work/youdao-device-deepening/write-batch.py`。
- 校验：`python3 work/youdao-device-deepening/check-batch.py`及限定三个正式文件的`git diff --check -- ...`。
- 未登录、购买、绑定、安装或调用用户接口；没有固件/账户状态改变。图片只暂存work，正式图像审计位于case内。

限制：未取得普通2.0完整原厂手册、固件版本及校订/收藏/复习操作链；机构说明照片不是原厂网站手册，也不是连续操作观察。
