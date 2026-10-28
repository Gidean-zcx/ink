# 单词机补强取证记录

查阅日：2026-10-26。范围 M-022 Mini/3.52 英寸 Max 与 M-023 MM-WM01。只做公开文档研究；未安装、登录、发送验证码、购买、绑定、固件写入或操作外部账号。没有本轮新图片/视频观察。

## 文件及命令

- `rg --files docs/research/interaction-library/cases | rg 'M-022|M-023'`：退出 0，定位两个既有案例。
- `rg 'M-022|M-023' docs/research/interaction-library/batches/vocab-evidence.jsonl docs/research/interaction-library/batches/vocab-coverage.csv docs/research/interaction-library/catalog.csv`：退出 0。既有最大 F 为 M022-03、M023-05。
- `cat docs/research/interaction-library/cases/M-022-baicizhan-device.md docs/research/interaction-library/cases/M-023-maimemo-device.md`：退出 0。
- 已沿用本轮此前阅读的 AGENTS、项目 README/requirements、交互模板与 batches/README 约定。Python hashlib 对两案例、旧 vocab 两批次保存 baseline.json，并复制案例为 `.before`。没有改这些旧批次。
- `rg --files work | rg 'vocab|maimemo|wordmachine' | head -n 60`：退出 0；读取原取证 `work/vocab-research/memohw-main.html` 的 Q12 文本和 img 标签，没有把旧 HTML 当新版发布日期依据。
- Python `urllib.request.urlopen(..., timeout=30)` 读取官方首页及两个日志：硬件首页 200/55044 字节，iOS 日志 200/123803 字节，Android 日志 200/125445 字节；临时 HTML 位于本目录。用标准库 re/html 去导航标签定位正文版本及链接，没有执行 JavaScript。
- `python3 work/vocab-device-deepening/write-batch.py && python3 work/vocab-device-deepening/check-batch.py`：最终退出 0；6 E、1 新 F、6 条旧覆盖行建议。字段/引用/本地链接与两旧批次散列通过，见 validation.json。正式新覆盖仅 F-M023-06。

## 正文及来源定位

web.open / click 取得：

1. `https://memodocs.maimemo.com/docs/support_wordmachine/`：帮助正文。Q3 资源更新与学习同步、Q4 实体键、Q9 无响应排查、Q12 云词本版本字符串是本次核心补充。与原 E-MMOH-01 同一来源；不重复计独立材料。
2. `https://www.baicizhan.com/little_class/article/892/share_view`：2022-11-08，App 配置后设备首页同步提示、选择单词本、新学。与 M016 E-BCD-06 同一材料；没有观看内嵌视频。型号未标，不套 Mini/Max。
3. `https://danciji.baicizhan.com/about`：关于/生态协同段，仅 D1 同步宣传；不引用用户评价或效果数字。
4. `https://memodocs.maimemo.com/docs/product-log-memo-ios`、`.../product-log-memo-android`：正文顶部为 2025-03-30 的 5.4.40、2025-04-28 的 5.5.00。检视到的“单词机”词主要在公共导航，不能作硬件发布记录。
5. `https://memodocs.maimemo.com/docs/OPLXw0KlkiaSUDkN69ScWREfnhc`：另一“单词机”目录项仅有标题/导航，不是第二份手册。

## 失败与未采用的线索

- 成功读取墨墨正文后，一次带行号的后续请求返回 `400 Timeout fetching`；没有反复重试该失败定位。已经取得的正文足以定位条款。
- 为核对 Q12，Python 请求第一张官方图 `https://memodocs-files.maimemo.com/static/clz1duta9002y10hatanjkmqe/assets/files/NgGgbPGOFoOJHRxfFfucGBJ5nIe?imageMogr2/thumbnail/!1200x`，连接持续阻塞且未取得文件。`ps -eo pid,etime,args | rg 'python3 -|vocab-device-deepening'` 退出 0 后定位本任务 PID 1928256；`kill -INT 1928256` 退出 0，原会话以 KeyboardInterrupt/130 退出。其余三个候选图循环未执行。没有重试、没有任何 `view_image` 调用，也没有把未见图像记为观察。
- 官网 Mini/Max 区链接的 `https://item.jd.com/100132972367.html`、`https://item.jd.com/100157977045.html`，以及另一商品 `https://item.jd.com/100222253287.html`，web.open 均返回工具不可访问；未重试、未访问购买流程。
- 搜索取得抖音相关聚合页 `https://www.douyin.com/shipin/7489267464094926863`，内含“Max 养狗常见问题解答”标题和平台 AI 文稿。原视频 `https://www.douyin.com/video/7647777974028345585` 与作者页 `https://www.douyin.com/user/MS4wLjABAAAAHFgpVND4IX8CumlkFYzWuM1DrjeKBXe_siW5_w4up5s` 打开均无正文。没有核定认证、原口述、日期和固件；不采用奖励数字、等待时长、离家/找回规则，不声称观看。
- PHP 教程站、转载页、用户评测与购物评论不作官方操作事实。

检索覆盖的关键查询包括 `site:baicizhan.com "单词机" "Mini" "绑定"`、`site:baicizhan.com "Max" "按键"`、`site:baicizhan.com "单词机" "说明书"`、`site:baicizhan.com "单词机" "配网"`、`site:learn.baicizhan.com "单词机"`、`"百词斩单词机" "使用手册" 官方`、`"百词斩单词机Max养狗常见问题解答" 百词斩`、`site:maimemo.com "单词机" "1.0."`。没有取得更多型号化官方完整操作材料，不宣称已经穷尽全部应用内或社交帮助。

## 状态与合并边界

仅改两个指定案例、新 `vocab-device-deepening-evidence.jsonl` / `coverage.csv`；工作文件均在本目录。未写 master 或其他案例。M022 历史下发同任务补证并入原 F03，不提高现 Mini/Max 主链等级；整理时曾拟 F-M022-04，已撤回且未进入最终覆盖索引，按批次约定不复用该草稿编号。M023 新增无响应排查 F06，与已有同步/解绑任务分开。

root 待更新旧 coverage 的 6 行仅变 `evidence_ids`、`known_branches`、`gaps`，完整行见 existing-coverage-updates.csv。尝试派出只读 worker 时返回 agent thread limit reached，没有启动助手，也未切换模型重试；后续本地完成。
