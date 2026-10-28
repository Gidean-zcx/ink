# M022 Mini / Max 本轮有限补查

查阅日 2026-10-27。仅公开资料；未登录、购买、绑定、安装、调用学习/账户接口、写固件、下载远程视频或实操硬件。无新 helper。完整官方菜单/操作手册仍未取得，不宣称已穷尽应用内帮助或所有社交渠道。

## 取得与定位

- 官方硬件首页 `https://danciji.baicizhan.com/`：web.open 与 requests GET 均取得正文，后者 HTTP 200，原件 `01-official-home.html`。复核现行 Mini/3.52 英寸 Max 范围；标准库 HTMLParser 提取 22 个 href（含重复），没有手册/帮助链接。没有执行页面 JS 或调用其后台。此结果不排除应用内帮助，不重复新增旧 E-BCZH-01。
- 原作者拆解 `https://www.52audio.com/archives/239551.html`：web.open 与 requests GET 均取得正文，HTTP 200；Wen5、2025-05-23，标题/开箱段注明 BCZ-Ace-Max。原件 `02-52audio-ace-max.html`，可读文本 `.txt`，原图 URL/DOM class 在 `02-52audio-ace-max-links.json`。只采用有限的实体键文字描述；未把作者观点、参数宣传、芯片或传感器存在推成任务能力。新增 E-BDCD-01 并回 F02。
- 三张必要候选图并行各请求一次，均 HTTP 403，停止该图片源，未换 Referer/代理/镜像重试，没有 view_image，没有下载成功的图片或正式 artifact。精确 URL、定位与状态见 `image-fetch-results.json`。候选分别为开箱第 5 张物品图（wp-image-239598）、第 6 张正面（239602）、第 11 张背面型号（239554）；**这些是原 HTML 的位置，不是已观察图注**。

## 检索范围

以下为本轮实际 web.search_query 查询，结果只作为发现入口；引用以可读正文为准：

1. `百词斩 单词机 Mini Max 使用说明 教程`（domains=baicizhan.com）
2. `百词斩 单词机 Max 宠物 操作 说明书`
3. `百词斩 单词机 Mini 解绑 返回 按键 官方`
4. `百词斩 单词机 Max 使用教程 官方 3.52`
5. `"百词斩" "Ace-Max" "说明书"`
6. `"百词斩" "Mini" "用户手册"`
7. `"百词斩" "Max" "常见问题" -site:php.cn -site:douyin.com`
8. `site:baicizhan.com "单词机" "宠物"`
9. `"BCZ-Ace-Max" "说明书" -site:52audio.com`
10. `"百词斩单词机" "帮助" site:baicizhan.com`
11. `"百词斩单词机" "操作指南" -site:php.cn -site:douyin.com`
12. `"百词斩单词机Mini" "说明书"`

## 排除、重复与历史失败

| 入口 | 处理与具体理由 |
| --- | --- |
| `https://www.baicizhan.com/little_class/article/892/share_view` | 已有 E-BCDVC-01；2022 教程没有硬件型号，本轮不新增 E，不用它补现版 Mini/Max。 |
| `https://www.php.cn/faq/2542297.html` | 索引出现设备步骤，但未提供可核同型号/构建及原始前后画面；不采纳自动同步时长或菜单。 |
| `https://www.xlhs.com/app/13975.html` | App 聚合下载/介绍页；不能填硬件空白，不下载。 |
| `https://post.smzdm.com/p/aeoweezm/`、`https://post.m.smzdm.com/zz/p/amo6z5dv/`、`https://post.smzdm.com/p/al4omz88/` | 聚合/概述/购物内容，型号和构建范围不清；不能拼出设备计划数量与操作规则。 |
| `https://www.douyin.com/shipin/7339795182898153508`、`https://www.douyin.com/shipin/7488187357968173071` | 搜索聚合/自动文稿，未观看原视频，不采用报错修复、宠物规则或等待时间。 |
| `https://www.douyin.com/shipin/7298730318046742562` | 超级单词机线索，不属本轮 Mini/3.52 英寸 Max 范围；未观看。 |
| `https://www.sohu.com/a/898104311_100193168` | 同一 52audio 拆解的转载，不计第二份独立证据。 |
| 京东 `100132972367`、`100157977045`、`100222253287` | 已核 `work/vocab-device-deepening/research-log.md` 的上轮不可访问记录；本轮不重试。 |
| 抖音 `video/7647777974028345585` 及上轮作者页 | 上轮无正文/认证未核；本轮不重试，不把聚合 AI 文稿标为观看。 |

## 确切命令与退出

所有路径相对 `/home/workspace/reader`，工具执行 cwd 相同。

- 初始误读 `cat docs/research/interaction-library/cases/M-022-baicizhan-hardware.md` 返回 1，文件名错误；随即定位并读取正确的 `M-022-baicizhan-device.md`。未因此创建错误文件。
- `pwd && ls work/baicizhan-device-controls-deepening && sed -n '1,260p' docs/research/interaction-library/cases/M-022-baicizhan-device.md`：0。
- `sed -n '1,110p' AGENTS.md && sed -n '1,110p' README.md && sed -n '1,160p' docs/requirements.md && sed -n '1,180p' work/vocab-device-deepening/research-log.md && rg -n 'R-014|R-015|R-019|R-021|单词机|宠物' docs/ideas.md`：0；规则/范围/旧失败核对。
- 读取 `template.md`、资料库 README、batches/README、旧 case/evidence/coverage 及本轮 before 快照：0。
- `rg -n '239551|BCZ-Ace-Max|E-BDCD' docs/research/interaction-library --glob '*.jsonl' --glob '*M-022*'`：1，表示补证前无匹配，排除来源/编号碰撞；不是网络失败。
- `python3 work/baicizhan-device-controls-deepening/fetch.py`：0；两个 HTML 均 200，requests 超时 `(5,18)`，各一次。返回状态和 SHA-256 见 `fetch-results.json`。
- `python3 work/baicizhan-device-controls-deepening/fetch-images.py`：0；脚本正常记录三次 HTTP 403，**退出 0 不等于图像取得成功**。没有保存图像内容。
- `apply_patch` 修改唯一 case：成功。
- `python3 work/baicizhan-device-controls-deepening/write-batch.py`：0；1 新 E、0 新 F、F02 的 4 个字段 before/after，正常 LF。
- 最终检查命令与结果见 `validation.json` / `HANDOFF.md`；不重跑任何旧批次 writer，不改主索引或旧批次。

## 本轮能改变的结论

仅增加 BCZ-Ace-Max 原作者实体键描述的出处，挂原 F02。Mini/Max 主任务仍为三 F / 全 D1；没有原厂学习、答错/重试、宠物奖励、退出恢复、解绑与同步冲突闭环。完整目录未知。后续最小有效材料是型号/修订页与操作页配套的盒内说明书、原厂公开帮助正文或同型号连续演示；本轮不向外联系、不登录、不购买以取得材料。
