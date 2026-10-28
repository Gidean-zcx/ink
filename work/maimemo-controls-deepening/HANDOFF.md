# M017 墨墨软件补证交接

日期：2026-10-27。已收束，没有后续网络操作待执行。新增 13 E（E-MMCD-01～13）、2 F（F-M017-08/09）。F01/F02/F07 补回旧编号；F07 建议从 D1 到 D2，仅限静态接口契约，不是调用或成功结果。案例现 9 F、25 E，均无实机/图片观察。没有修改旧批、主表、根文档或其他案例。

## 文件

- `docs/research/interaction-library/cases/M-017-maimemo.md`
- `docs/research/interaction-library/batches/maimemo-controls-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/maimemo-controls-deepening-coverage.csv`
- `docs/research/interaction-library/batches/maimemo-controls-deepening-inventory.csv`
- `docs/research/interaction-library/artifacts/maimemo-controls-deepening/source-notes.md`
- 工作交接：`work/maimemo-controls-deepening/existing-coverage-updates.csv`、`validation.json`、`hash-manifest.json`、`fetch-results*.json`、`schema-fetch.json`。

旧行建议 CSV 有 13 个字段，正常 UTF-8/LF：F01/F02 各 evidence_ids、known_branches、gaps、checked_on 四项；F07 另加 depth，共五项。before 来自启动时主 coverage 的只读快照。没有修改所属旧批。请 root 合并后将案例末的交接说明改为完成状态。

目录计数：软件侧栏 30 节点 = 4 父项 + 24 软件叶项 + 2 排除的单词机叶项；另 7 平台/开放支持入口、24 API 方法。61 不是完成的功能数；未读、正文无过程、排除项各有状态。

## 根可抽核的主要原件

1. E-MMCD-10：[API YAML](https://open.maimemo.com/api_bundle.yaml)，来自 [公开主页](https://open.maimemo.com/) 内 `elements-api apiDescriptionUrl="/api_bundle.yaml"`；没有打开凭据 URL、控制台或调用任何 `/open/api/` 方法。
   - 原件 `work/maimemo-controls-deepening/16-api_bundle.yaml`，SHA-256 `8612a857ee586d588cf1a826fa71721c6e10ca1c1a6593a8873e075fed58f7bc`。
   - 第 1–32 行：OpenAPI 3.1.0/info.version=v1、凭据入口、频控；1644–2931 是软件方法。
   - 2091–2153 CreateNotepad 请求/200对象；2540–2566 GetStudyProgress 初始化与自动同步；2572–2638 GetTodayItems；2744–2794 AddWords（added_count、额度/重复）；2799–2841 AdvanceStudy（LVL10 与未加入的影响）。
   - 3797–3908：StudyProgress、StudyTodayItem、StudyRecord，完成数与每日最多计一次的 study_count。规范只有声明，没有真实用户数据；释义/助记删除未列 response 结构。24 方法自动解析清单在 `api-operations.json`。
2. E-MMCD-08：[PDF 帮助](https://memodocs.maimemo.com/docs/QbA7wb0gKiTSSyk66RzcYJxln3g)。原件 `10-pdf.html`，SHA-256 `bdfc64a77df400002deb1a3b10d52af84f73fd166155a5f710784629e6d522b9`；正文提取 `10-pdf-main.txt`。
   - 二（导出流程/首页预览）、五（内容）、七（消耗）、八（保存/退出/闪退）。开头笼统故障不补偿与八的闪退资格保留并列，适用边界不明。
   - 未把原文疑似笔误的使用权句解释成授权；未执行导出，也没有扣次数或保存文件。
3. E-MMCD-05～07：[筛选](https://memodocs.maimemo.com/docs/Uv65whPGIih1x2kMNOCc99k4nUe)、[超级释义](https://memodocs.maimemo.com/docs/AXhuwXpfFibceakKPpdc6VbVnFc)、[考点义项](https://memodocs.maimemo.com/docs/TrSUwLo8wiwrUtkkmSBcMipgncb)。定位正文及超级释义一、四、六、七。最低 5.5.65 平台未标；不能映射 HarmonyOS 的 5.5.19.0069。旧版释义缓存与超级释义不同；固定内容与默认内容不同。

所有 18 HTML + 1 YAML 的原件、正式文件 SHA 在 hash-manifest.json，E 各自也有原件 SHA；正式 artifact 是短定位笔记，不是整页复制或截图。

## 取得方式、命令与退出状态

主要实际命令（路径均在项目根 `/home/workspace/reader`）：

- `python3 work/maimemo-controls-deepening/fetch.py` → exit 0，6 个公开帮助 GET 均 200。
- `python3 work/maimemo-controls-deepening/fetch2.py` → exit 0，9 个公开帮助/文档壳 GET 均 200。
- `python3 work/maimemo-controls-deepening/fetch3.py` → exit 0，3 个公开帮助 GET 均 200；自评与 iOS 日志复查不重复增 E。
- YAML 单次内联 `requests.get('https://open.maimemo.com/api_bundle.yaml',timeout=(5,18))` → Python exit 0/HTTP 200；结果写 schema-fetch.json。只下载文档。
- 正文提取用标准库 HTMLParser 限定 `<main>`；API 用环境已有 `yaml.safe_load` 遍历 paths，未安装依赖，exit 0。
- `python3 work/maimemo-controls-deepening/write_batch.py` → exit 0。
- `python3 work/maimemo-controls-deepening/update_case.py` → exit 0。此为本次局部替换脚本，不要合并后重跑。
- `python3 work/maimemo-controls-deepening/check_batch.py` → exit 0/PASS，13E/2新F/9总F/61目录/13旧字段。逐项校验 SHA、E 无碰撞、F 引用、相对 artifact、CSV LF、案例本地链接、旧 vocab 两批字节未变。
- `git diff --check -- docs/research/interaction-library/cases/M-017-maimemo.md docs/research/interaction-library/batches/maimemo-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/maimemo-controls-deepening-coverage.csv docs/research/interaction-library/batches/maimemo-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/maimemo-controls-deepening/source-notes.md` → exit 0。文件包含未跟踪项，内容校验以 check_batch.py 为准。
- 起始 `rg -n 'E-MMCD' docs/research/interaction-library --glob '*.jsonl' --glob '*.csv'` → exit 1，正常无既存编号；其余 cat/tail/rg/解析检查 exit 0。

web 工具部分帮助打开超时（400 Timeout）、API 文档壳无正文、YAML 提示工具不可读；随后普通 HTTP 取得正文。没有 401/403/429，没有认证绕过或重试受限接口。HTTP 的 200 与哈希只证明原件取得，不证明产品功能。

## 未知与状态边界

自评撤销、拼写判错重试、后台/跨日/断网续学、组队奖励、发卡及正常签到全链仍未知。滑回上词不等于记忆反馈回滚。iOS 日志顶部仍止于历史 5.4.40；HarmonyOS 日志有更晚日期，不能拼成一个版本。超级释义现行与计划措辞差异保留。API 精确 scope、错误码、等待时间、幂等、撤销与数据恢复没有得到契约答案。

本轮改变的是研究文件。没有改变用户/远端账户、学习记录、下载额度、文件云库或任何设备状态。
