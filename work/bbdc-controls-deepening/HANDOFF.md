# M018 不背单词补证交接（冻结）

2026-10-27。本轮收束为 **0 新 E、0 新 F**；原 5 条仍为 3×D1、2×局部D2，没有深度升级。新增 34 项来源覆盖台账，给 root 的建议为 **4 条旧 F / 13 个字段**、**3 条旧 E / 4 次 additional_captures**。只改本案例与本批文档；旧批、master、全局资料未修改。没有登录、安装、接口调用、提交、视频/音频下载或产品操作；静态观察 0。

## 正式改动（5 个文件）

1. `docs/research/interaction-library/cases/M-018-bbdc.md`：补版本原件日期、纠错措辞、组量边界、账号条件及来源取得差异。
2. `docs/research/interaction-library/batches/bbdc-controls-deepening-inventory.csv`：34 项，15 商店能力/版本、13 政策记载入口、3 网页导航、3 未解任务。不是完整帮助或设置菜单，也不是 34 个完成任务。
3. `docs/research/interaction-library/batches/bbdc-controls-deepening-evidence.jsonl`：0 字节，明确无新增 E。
4. `docs/research/interaction-library/batches/bbdc-controls-deepening-coverage.csv`：仅标准表头，明确无新增 F。
5. `docs/research/interaction-library/artifacts/bbdc-controls-deepening/source-notes.md`：短摘记，区分原 HTML、内嵌数据与政策索引。

正式文件和全部原件/派生文件的逐文件 SHA-256、字节数见 `work/bbdc-controls-deepening/all-file-hashes.json`。该清单生成时 28 个文件，不包含清单自身及随后写入的本 HANDOFF。

## root 合并建议

- `work/bbdc-controls-deepening/existing-coverage-updates.json`：逐字段 before/after；配套 CSV 使用真正 LF。F01、F02、F04、F05 各改 known_branches、gaps、checked_on；F04 另加 E-BBD-01 支持日界日志。共 13 字段。F03 未重复改行。
- `work/bbdc-controls-deepening/additional-captures-updates.json`：E-BBDC-01 官网 HTML 一次；E-BBD-01 商店 HTML 一次；E-BBDC-03 政策 HTML 壳和搜索工具响应各一次。只追加，不改旧采集方式；旧记录保留历史来源状态。
- E-BBDCD 前缀已在全库主表及批次查重，无占用，本轮没有使用。无案例视觉审计反向依赖，因此没有迁移审计字段。
- 建议 JSON 的 SHA：旧 F 为 `56d932f294d4f8b53e4ddd67b96b78f19d7ed7e5e57dc15d10afb70a4f71fea3`；旧 E 为 `cb9c52adb124f7f6fbb0e66b00cafcec00b169ef3babd6ed5647e6ca11b412a2`。

## 核心复核入口

**1. 商店日期、尝试修复与分组尺寸**

[中国区 App Store](https://apps.apple.com/cn/app/不背单词-四六级考研等英语单词学习/id698570469?platform=mac)，同旧 E-BBD-01。

- 原件 `work/bbdc-controls-deepening/appstore-cn.html`，HTTP 200，655769 字节，SHA `3f11560658cfcd9f94ca8e9359f193456b8f2b0ff2ca3ce28c7871b07429218b`。
- 文本派生 `appstore-cn.txt`；内嵌 JSON 派生 `appstore-cn-embedded.json`；25 项版本窗口 `appstore-version-records.json`。
- 原 HTML 定位：`script#serialized-server-data` → `data[0].data.shelfMapping.mostRecentVersion.seeAllAction.pageData.shelves[0].items`。每个对象的 primarySubtitle 为版本，secondarySubtitle 为带年的日期。第一项 5.11.6：2026-09-18；正文写修复尝试，不是成功保证。
- 5.9.8：组尺寸与学后/复习后拼写分设；5.9.17：日数据边界；均不能替代每日计划或中断恢复规则。
- 本案例固定 iPhone/iPad 商店描述，不把 `platform=mac` 参数当独立 Mac 交互证明。Android 5.11.7/2026-10-15 仍是前一批记录，本轮未复抓魅族页。

**2. 政策索引与原址响应分歧**

[不背单词隐私协议](https://learnywhere.cn/lexis/privacy)，同旧 E-BBDC-03。

- 直取 `work/bbdc-controls-deepening/privacy.html`：HTTP 200、976 字节、SHA `6aa7a91f42f7ca1a7ba8b0563661a976d59008b2f704ec3d1434f1f8a0ef3ebe`，只脚本壳。`web.open` 也只壳。没有再次请求旧 403 脚本。
- `privacy-web-search-response.json` 是搜索工具响应的 JSON 字符串，34815 字节，SHA `0cfc9429187f424247194a556c1b53c2251ebde9ad4cb711a1102fe3d52c9b38`；`.txt` 是便于检索的同响应文本。它不是原站 HTML，不能称已取得官方正文原件。
- 索引标政策更新 2026-08-28、生效 2026-09-04，App 构建未知。第二章（一）给旧账号密码条件和一种账户入口；第六章（一）/（二）使用另一名称，未统一。第二章（三）/（五）支持旧帮助/数据入口。第六章（三）的服务方备份表述不证明用户可恢复。

**3. 官网导航**

[官网](https://www.bbdc.cn/index)，同旧 E-BBDC-01。`index.html` HTTP 200、35980 字节，SHA `4ee092ce7e67d895ecb56f3705f14d88e74d9e6a402d15721ca9823ff4443fa8`。仅登记真实导航；未触发登录挂钩。HTML 内有隐藏表单，解析出的字段不等于观察到表单显示。用户评价排除。

## 命令与合并前检查

详细读取、请求、20 条搜索查询及一次路径错误见 `work/bbdc-controls-deepening/research-log.md`。关键命令实际退出状态：

```text
python3 work/bbdc-controls-deepening/build.py
exit 0（一次初稿构建；之后有本轮手工字段校订，不重跑覆盖）

python3 work/bbdc-controls-deepening/check.py
exit 0，最终 2761/2761 PASS

git diff --check -- docs/research/interaction-library/cases/M-018-bbdc.md docs/research/interaction-library/batches/bbdc-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/bbdc-controls-deepening-coverage.csv docs/research/interaction-library/batches/bbdc-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/bbdc-controls-deepening
exit 0

diff -u work/bbdc-controls-deepening/case-before.md docs/research/interaction-library/cases/M-018-bbdc.md
exit 1（预期有差异；case.diff 与 final-diff-status.json 已保存）
```

`validation.json` 检查字段/引用、原件与正式摘记散列、旧批散列、空增量表、LF、案例链接、原件/索引差异，以及全库是否将 M018 case 用作审计文件。大量 PASS 是审计反向依赖断言，不是产品测试数量。原始 `case-before.md` 和 `baseline.json` 已保全。Git 所列正式资料未跟踪，所以检查不依赖空 Git diff。

合并前可以运行本批 `check.py`；root 合并建议后 before 条件会改变，**不要再运行 build/check 覆盖或要求旧 before 仍相等**。正式材料和建议已冻结，后续最多补本工作交接记录。

## 限制与状态

主学习和纠错没有取得 D2 全链；提示、拼写提交判定、空错词集合、跨日中断、离线提交、收藏原句、导出字段、换机/账号合并/回滚仍未知。公开帮助目录未取得；政策入口列表不能替代实际设置树。旧脚本/知乎 403、微博 robots、自制词书登录路线本轮未重试。第三方教程、下载页、平台索引及视频线索没有编成官方步骤。不存在“因为未找到，所以不支持”的结论。

状态变化仅本地文档与取证原件；没有外部账户或产品状态变化。当前案例 SHA `5ff40f5ab1b1141b6ae561cfd9ee11d0a929da6ef02af5b40f1b30da777867c2`；正式 source-notes SHA `134db89f2ce646ffc806efa6caaaf139e959656c9def7afecba315cf0e1cc700`。
