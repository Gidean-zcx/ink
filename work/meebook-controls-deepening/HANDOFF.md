# M057 Meebook 有界研究交接（冻结）

2026-10-27；只读公开资料，固定M7。没有新增E/F，也没有新的可计完整任务；六旧F继续原深度。本轮价值为分层目录、22条型号日志隔离、两条后台入口/数据结果分开，以及可复查的获取失败与原件边界。不要为了数量把声明变成流程。

## 合并对象

- 修改case一份，新增三批次文件和一份短摘记，共5个正式文件。新evidence为空文件，新coverage只有表头，表示0新增，而非生成失败。
- inventory 49行：指南19＝4正文栏目＋7基本操作＋4操作子项＋4维护子项；M7销售商版本日志22＝V1.5.3的4新增/5优化＋V1.5.2的2新增/11优化；另8研究缺口不是官方功能目录。不能称已取得完整M7系统手册。
- `old-coverage-updates.json`：6旧F完整before/after与逐字段changed_fields，18字段（每条known_branches、gaps、checked_on）。depth/evidence_ids/visual_status/hands_on均未变；主表/旧批由root合并。
- `old-evidence-additional-captures.json`：4旧E追加建议，每条附before_additional_captures；没有改旧E顶层或旧批。E01/E04有成功原件；E02/E03的403文件只作failed_response，raw_work_path=null，不能归档成正文。
- 新E前缀E-MECD本轮前检无占用，最终未分配；没有新图/连续观察/D3/D4。

## 正式文件与SHA-256

| 文件 | SHA-256 |
| --- | --- |
| `docs/research/interaction-library/cases/M-057-meebook.md` | `61b1abf5f9658df97000b63fa1b1add3101a8f430669fbde33cdbc66f1084d58` |
| `docs/research/interaction-library/batches/meebook-controls-deepening-evidence.jsonl` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `docs/research/interaction-library/batches/meebook-controls-deepening-coverage.csv` | `aa0cf671b21fa0390e9b0cfb58c6cecf3a0d7df66eaad894707f17ea224f6025` |
| `docs/research/interaction-library/batches/meebook-controls-deepening-inventory.csv` | `96341f4bbb665bdfc820710488ea61403a46a5acbc39085246e572f1467580c5` |
| `docs/research/interaction-library/artifacts/meebook-controls-deepening/source-notes.md` | `e096a28910f56b3e5fbbfc8339a089e799fc9b6738f72b060d45c81e304da545` |

## 原件与失败响应

| key | HTTP | 路径 | SHA-256 |
| --- | --- | --- | --- |
| official | 200 | `work/meebook-controls-deepening/raw/official.html` | `517793993ff05808ea982eba377d0140a9a74179aefe030c0109137959c19276` |
| quick | 403 | `work/meebook-controls-deepening/raw/quick.html` | `3483b1775080474fb721d0801cb29ff4c046cc5e4d51711bbca77e80615b27ee` |
| firmware | 200 | `work/meebook-controls-deepening/raw/firmware.html` | `5a8a79916d8e0f9940496b7ba044a7cf34e07ae61ff1d2f0d5635a4266eaf099` |
| metadata | 403 | `work/meebook-controls-deepening/raw/metadata.html` | `f46951a741d217ab40193f7cdb453d41671e3bb30d7a61888ba11f03b0eb7673` |
| index | 403 | `work/meebook-controls-deepening/raw/index.html` | `885c20651c0793e4431dc2afacc822c4010a0b7b74b9bcd5dc053e42e2e209e3` |
| m7-manual | 403 | `work/meebook-controls-deepening/raw/m7-manual.html` | `d94869db0f1e55f9f739ce5526f6118cff2cd296ea084ef6b984ff5470e2ac8d` |
| m7-original | 403 | `work/meebook-controls-deepening/raw/m7-original.html` | `a73a0546e63a3815c18bc7884daab46c82772ad7429e9425e0620e8fc0da5d3e` |

原文链接/最终跳转/日期/内容类型/字节数见`captures.jsonl`。成功HTML有同名txt和links.json；M7段另存`raw/firmware-m7.txt`，其sha256为`d23e9ee701f81b8c26e6a0070f2db564d1779e08ac78a9d973b941d96431ce60`。HTML和抽取文本hash分别保存，未把文本hash当原件hash。

监管镜像在web读取时报告原PDF哈希7461db01fbc9f46a6745de12f99c81e69b0833b6001713344af6a21121509984及12页，但本地没有PDF，不能验证该值，亦无实际看过的页码。它的P6说明书元数据与M7型号关联仍保留；不删除共享模板风险。

## 关键复核来源

1. https://ereader.co.th/firmware/ —— 优先离线读`raw/firmware-m7.txt`及完整HTML。仅Meebook M7标题段两版；京东同步是指定App，自动旋转、蓝牙自动关闭均是声明；旁边P78/P10/M6的笔记/云盘/MDX/Regal不是M7。
2. https://www.manualslib.com/guide/4006924/meebook-m7-manual.html —— 本轮web工具先成功读取正文栏目/基本操作1–7/维护；后续GET403停止，没有成功HTML原件。可用旧E03与本轮短摘记核范围，**无需root重试403**。A2条件相邻歧义仍未解决，后台入口不代表未保存编辑保留。web输出并非本地原件，不能声称完整快照已归档。

官网成功原件`raw/official.html`只见语言图和隐私链接，未见M7目录；没有重新看长图。Good e-Reader仅web返回403，无本地响应文件。其余拒绝和PDF路径记录见表，均只一次正常请求，收到401/403/429即停止路径，无代理/换身份/挑战绕过。索引发现的公开PDF保存件分别正常读取后拒绝，没有取回内容。

## 基线和反向依赖

原case保留`case-before.md`，SHA-256 `062804d64a677c0b216f729fe8e3f23883a00b2b7fc4f760cd530e697dbc36b2`。`baseline.json`记录初次递归110个JSON/JSONL未发现case路径/旧hash引用；最终检查111个也为0，故没有旧E audit hash迁移。原6F/4E在`coverage-before.json`/`evidence-before.json`。未来若把可变case作为证据audit，应改用不可变artifact；本轮不需要改旧引用。

## 命令与退出状态

执行目录均为`/home/workspace/reader`。

- `cat AGENTS.md README.md docs/requirements.md docs/research/interaction-library/README.md docs/research/interaction-library/template.md`：exit0；另读case、旧批、ideas及旧write-batch.py中M057段，只读未运行旧脚本。
- `rg --files work/extended-readers`及M057/Meebook/失败码定位：读取成功；联合末尾`rg`检索E-MECD/E-MBCD/批次名前缀无命中，整条exit1（正常无匹配，不是工具失败）。
- 初次`python3 work/meebook-controls-deepening/capture.py ...`：exit1，缺bs4，发生在网络调用前；改用标准库HTMLParser，没有安装包。
- `python3 work/meebook-controls-deepening/capture.py official https://www.haoqingtech.com/ quick https://www.manualslib.com/guide/4006924/meebook-m7-manual.html firmware https://ereader.co.th/firmware/ metadata https://fccid.io/2A53EMEEBOOKM7/User-Manual/User-Manual-6662213 index https://device.report/shenzhen-haoqing-technology`：exit0；HTTP分别200/403/200/403/403。进程exit0只表示成功记录响应，不代表正文都取得。
- `python3 work/meebook-controls-deepening/capture.py m7-manual https://fcc.report/FCC-ID/2A53EMEEBOOKM7/6662390.pdf`：exit0，HTTP403，停止。
- `python3 work/meebook-controls-deepening/capture.py m7-original https://manuals.plus/m/7461db01fbc9f46a6745de12f99c81e69b0833b6001713344af6a21121509984.pdf`：exit0，302后403，停止。
- `cat work/meebook-controls-deepening/raw/official.html && sed -n '/Meebook M7/,/Meebook M6/p' work/meebook-controls-deepening/raw/firmware.txt | head -100 && rg -n 'M7|\.pdf' work/meebook-controls-deepening/raw/official.html`：最终exit1，仅官网HTML无M7/PDF匹配；前两项读取成功。
- 内联Python基线、型号段提取、前缀核查、交接/manifest写入：均exit0。只在本任务work和授权5个正式文件落盘。
- `python3 work/meebook-controls-deepening/build.py`：exit0；**仅本轮一次构建，交接后不要重跑**。
- `python3 work/meebook-controls-deepening/check.py && git diff --check -- docs/research/interaction-library/cases/M-057-meebook.md docs/research/interaction-library/batches/meebook-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/meebook-controls-deepening-coverage.csv docs/research/interaction-library/batches/meebook-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/meebook-controls-deepening/source-notes.md`：exit0，PASS。校验before一致、逐字段建议、原件/正式hash、ID/本地链接、空新批及CSV物理换行。

合并前可运行`python3 work/meebook-controls-deepening/check.py`（只读）。主表合并后before断言预期失败，不应自动改建议；root用自身合并审计确认。

CSV结果：coverage 0数据行/1物理LF，inventory49数据行/50物理LF；无字面量\r\n。raw/正式hash通过，6F引用集合不增不减。没有运行产品、安装工具或访问账号，不改变设备/账户状态；仓库研究文档与work改变，未修改master、旧batch、汇总、README或requirements。
