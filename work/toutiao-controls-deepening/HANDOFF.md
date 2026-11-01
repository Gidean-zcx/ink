# M052 今日头条交接：合并前冻结（2026-10-27）

有限批次完成：3新E、1新F（F-M052-07个人信息副本申请，D2只限文档）、22项读者功能来源台账；独立73节点创作者帮助目录（包含父子项，只读1篇FAQ正文）。原F01–05补18字段；E-TT-01/04各1追加采集；原F06保持。0图片/0实操。主阅读链仍D1，未补造长按、频道按钮、续读或同步结果。

## 修改文件与SHA256

仅以下6个正式文件；旧批/master/全局文档未改：
- `docs/research/interaction-library/cases/M-052-toutiao.md` — `b7161998ed8ccb9fb770f8b9b06f9799b8d81b1a06d22ce287267d726a580b60`
- `docs/research/interaction-library/batches/toutiao-controls-deepening-coverage.csv` — `ca6d8aa40027b4be29d4010727579b0a7f3a49555bf5e02fe040b0549cdccea3`
- `docs/research/interaction-library/batches/toutiao-controls-deepening-evidence.jsonl` — `c1b69112594339fd0af878f43734ab286e166d421a406384c0ebc8519b1268eb`
- `docs/research/interaction-library/batches/toutiao-controls-deepening-inventory.csv` — `227d0c2597afb34659461c23051fca32f8d7d05fb2edbab1cb2641ec6fde3a22`
- `docs/research/interaction-library/artifacts/toutiao-controls-deepening/creator-help-index.csv` — `807f00c48d713a2438e25551ade04cd0eaffa4e4df3aa4a90f40c9df2bc779f1`
- `docs/research/interaction-library/artifacts/toutiao-controls-deepening/source-notes.md` — `65ed9f19d0d7a6c834810aaa1ad06065e7ff924c6616a2a74d1094c712fa7218`

## 合并建议路径

- `work/toutiao-controls-deepening/existing-coverage-updates.json`：18逐字段before/after，旧编号与深度不变。
- `work/toutiao-controls-deepening/existing-evidence-additional-captures.json`：E-TT-01为本地旧原件重读；E-TT-04为同URL新GET、iOS/iPadOS商店版本日期，未称安装版本。不修改旧主字段。
- `work/toutiao-controls-deepening/baseline.json`及`case-before.md`：初始6F/4E与原文/旧批SHA；无旧E审计路径引用case，E-TTCD前缀原未占用。

逐F字段：
- `F-M052-01`：known_branches, gaps, checked_on
- `F-M052-02`：evidence_ids, known_branches, gaps, checked_on
- `F-M052-03`：evidence_ids, known_branches, gaps, checked_on
- `F-M052-04`：evidence_ids, known_branches, gaps, checked_on
- `F-M052-05`：known_branches, gaps, checked_on

## 核心核验入口

1. 旧`work/feed-interactions/tt-privacy.txt` §6.1.2（约206–220行）、§6.2.2（224–225行）：总推荐、位置/搜索/关注特征、丰富度、个性化广告、X理由、无痕搜索/阅读作用不同。标签效果是“不再或者减少”，没有确认长按、具体理由枚举、单篇/作者/关键词作用范围。频道增减排序只声明存在；相邻清兴趣句不能当频道编辑路径。
2. 同原件§7.2.1（242行起）及§7.2.2（260行起）：我的内容查删各类记录；个人副本申请路径与邮箱链接/拒绝邮箱分支。F07没有真实申请、验证或接收，文件格式/内容覆盖/链接有效期/导回恢复未知。
3. E-TTCD-01官方`Personal_information_management.html`：未标日期，清兴趣写成删除操作使用行为。2026政策限定历史兴趣/推测偏好；保留措辞差异，不能据旧指引声称清空全部记录。
4. E-TTCD-02官方`e327f6fd-d5b9-4090-b44e-d65dc891020a.html`：2024-05-06更新、05-13生效，§1.3.2部分入口省略设置且用“永久清除”；同段仍说明重新积累兴趣。由旧政策HTML链接找到，不是当前18.8.0界面证据。
5. E-TTCD-03 `https://baike.toutiao.com/detail/195/282/0`：2025-02-21更新的创作者FAQ。RENDER_DATA内menuList含平铺记录与嵌套Children，直接遍历曾产生170重复项；正式目录已只从7个ParentId=0根遍历为73唯一ID/1已读正文。另7个不连入该树的节点未计，不推定其历史/显示状态。未调用任何菜单/账户API。创作信用分不是阅读学习分，客服中心入口也不补成读者帮助全集。
6. 商店原HTML18.8.0的time datetime=2026-10-22；近期日志只有一般稳定性修复。排除了商店用户评论/描述中的引述好评，不据其声称“30%陌生内容”等效果。

## 取得限制与下一类材料

官网首页普通GET200但只有身份初始化壳，没有读者正文。未执行ttwid注册或脚本；停止该路线。/feedback网页工具报不可访问（无HTTP状态），没有重试。本轮5次普通GET均200；其中首页不算成功取到功能正文。检索范围与排除线索见`exploration-log.json`；未把个人头条文章、百度经验、PHP中文网、聚合FAQ当官方事实。未定位既有本产品403记录，不把其他产品记录移入。

缺口仍是：主App固定OS/版本的长按对象菜单、频道编辑、关注排序、来源跳转、收藏创建/取消与失效内容、阅读结束/返回/重启、弱网提交及误操作恢复。下一类应是可匿名读取的官方读者FAQ或有版本标识的官方连续操作说明；本轮不扩泛搜，不用更多政策宣称阅读主链完成。

## 实际主要命令与退出状态

项目/基线读取：`cat AGENTS.md`、`sed -n '1,170p' README.md`、`sed -n '1,180p' docs/requirements.md`、`cat docs/research/interaction-library/cases/M-052-toutiao.md`、`rg -n 'M-052|E-TT|toutiao|今日头条' work --glob '*.json' --glob '*.md' --glob '*.py' --glob '*.csv' | head -n 65`及定向读取旧tt-privacy原件。成功命令组均exit0；失败记录rg未命中（无匹配，不是HTTP成功声明）。首次宽搜索输出过长，随后只读产品原件/结构化JSON，未把截断结果当完整目录。

实际普通GET逐条命令（保存原件/状态/最终URL，均exit0；capture.py异常也会记录状态，因此不单凭exit判断正文成功）：
```sh
python3 work/toutiao-controls-deepening/capture.py homepage.html https://www.toutiao.com/
python3 work/toutiao-controls-deepening/capture.py appstore.html https://apps.apple.com/cn/app/%E4%BB%8A%E6%97%A5%E5%A4%B4%E6%9D%A1/id529092160
python3 work/toutiao-controls-deepening/capture.py personal-management.html https://sf3-cdn-tos.douyinstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/Personal_information_management.html
python3 work/toutiao-controls-deepening/capture.py creator-help.html https://baike.toutiao.com/detail/195/282/0
python3 work/toutiao-controls-deepening/capture.py basic-service.html https://lf9-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/e327f6fd-d5b9-4090-b44e-d65dc891020a.html
```

落盘与预检命令：
```sh
python3 work/toutiao-controls-deepening/build.py
python3 work/toutiao-controls-deepening/check.py
python3 work/toutiao-controls-deepening/check.py > work/toutiao-controls-deepening/check-result.txt
python3 work/toutiao-controls-deepening/make-handoff.py
```

build两次exit0，第二次修正目录重复计数；check最终146 PASS/exit0。检查仅验证本批文件、LF、ID/URL去重、来源/原件散列、7F/22功能映射、73唯一目录ID/1已读正文、18旧F before与2旧E现值，以及旧批未变。没有全局索引合并、产品测试、安装或账户动作，没有调helper。

## 原件散列

完整正式/原件/工作文件散列在`file-hashes.json`，不包含清单自身和HANDOFF以免循环。5次GET原件如下：

| 原件 | SHA256 | 实际请求URL |
| --- | --- | --- |
| `work/toutiao-controls-deepening/homepage.html` | `d2a3d5b1ce4dfff150c262d2e04c369fa105eff5a0400cd1a13717f44be1e82f` | https://www.toutiao.com/ |
| `work/toutiao-controls-deepening/appstore.html` | `93ee7266605d55a66adbaaba0f09f66ee7eafb63c746eea330ed4da99228c67b` | https://apps.apple.com/cn/app/%E4%BB%8A%E6%97%A5%E5%A4%B4%E6%9D%A1/id529092160 |
| `work/toutiao-controls-deepening/personal-management.html` | `3a68f5f50cb76a134ada867942921ec6d51acbacb78a061b68724089e12834eb` | https://sf3-cdn-tos.douyinstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/Personal_information_management.html |
| `work/toutiao-controls-deepening/creator-help.html` | `159961445c2b9451da2beb3a73fff1848947abe557b03d5e0ef7bcf213cb8b93` | https://baike.toutiao.com/detail/195/282/0 |
| `work/toutiao-controls-deepening/basic-service.html` | `419b026d853da91663a8967e4c8d6286a4e778b0b3541ad29057dc56be01724c` | https://lf9-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/e327f6fd-d5b9-4090-b44e-d65dc891020a.html |

重读旧隐私原HTML SHA：`ce2f1862aa74849719fc85e27dae2176dac020fabf8bc93ded0f3f35529fae20`，与原E一致。

合并前可重跑本批check；root应用before/after后不要再运行build或该before检查。根可对照manifest只读验SHA并用自身全局检查验合并结果。

状态：只改变以上专属正式资料及work目录；旧批/master/全局索引、产品/账户/外部服务状态未改变。没有任何图片视觉判断。交付后冻结，由root复核与合并。
