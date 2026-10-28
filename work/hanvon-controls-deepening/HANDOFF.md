# M008 Hanvon controls deepening — 2026-10-27

本批已冻结，仅资料研究。2新E（E-HVCD-01/02），0新F；原6F全部D1。更新建议涉及F01/03/04/06，各4字段，共16字段；2旧E追加本地复读记录。未改主表、旧批、索引、README、需求或全局简报。不要重跑build.py：构建后另作了历史日期及来源归因修订；合并前只运行check.py。

## 合并文件与建议

正式文件（相对repo）：

- docs/research/interaction-library/cases/M-008-hanvon.md
- docs/research/interaction-library/batches/hanvon-controls-deepening-evidence.jsonl
- docs/research/interaction-library/batches/hanvon-controls-deepening-coverage.csv（只有真实换行的表头，0新任务）
- docs/research/interaction-library/batches/hanvon-controls-deepening-inventory.csv
- docs/research/interaction-library/artifacts/hanvon-controls-deepening/source-notes.md

建议文件（只交给root，未写回主表/旧批）：

- old-coverage-updates.json：完整before/after与changed_fields；4行均仅改evidence_ids、known_branches、gaps、checked_on。深度、视觉状态和hands_on保持原值。
- old-evidence-additional-captures.json：E-HANV-03与E-HND-01；before_additional_captures及append_additional_captures。本地复读不是再次联网捕获，不替换旧获取日期/身份。
- baseline.json、case-before.md：案例基线SHA256 `6ec0b9991ea172b3389b0a529d41e105f0bade1e7794518dba685f64c777f2b9`；基线扫描148个JSON/JSONL，未发现案例反向散列依赖。
- formal-manifest.json：5个正式文件最终散列；raw-manifest.json：本轮work原件/提取件散列；captures.jsonl与access-failures.json：HTTP状态与正文可读性分别记录。

目录53条：来源索引5、文章栏目15、报告控件3、入口父组1/子入口8、功能报告4、官方能力5、旧静态控件2、条件报告3、缺口7。栏目/父子/缺口不是新增任务；未宣称已经取得完整机内设置目录。所有新E均有supersedes:null、hands_on:false、visual_observed:false。

## 根复核重点

1. https://itsjzx.com/ai/44320 ，非洲小野马2025-05-06。本地raw/itsjzx.txt开箱段可查中央圆键与另外八键、电源键；会议段可查微信先绑定。普通GET200，有完整正文。HTML SHA256 `8141902bb602149a6afb83490a947c0fd4fbe5457704116f53eb4f1f9295eeff`；文本 `b605d35f5ae46efafbb32607b7c41de13adbba4b2380c0a916146e3d1f4e973c`。
2. https://zhongce.sina.com.cn/article/view/189952 ，木木滚滚gg2025-05-30 19:26。raw/sina-review.txt操控段是灵动条各入口清单，不能据此推后台保活/返回保存。普通GET200。HTML SHA256 `24062272f14886675c052892fa2fd7b3f75c54aa28fb53a03e5ffc304cbaf99d`；文本 `b5485d81d27ba6ae06f68f4d9640cb1f696d0d2575c46bf93928c9a43cd6ffe0`。

两文都由作者自述使用且标题/正文一致为N10 Pro二代，但带推荐性语言；供稿/商业关系未核，不当作官方操作指南。Android14仅作者报告，固件与App构建未知。九键自定义的笼统/细分描述分开归因；笔键“手触”含义未知，不能解释成启用不存在的屏幕手触层。

复读的旧官方原件为work/ai-notebooks-research/hanv399.html（SHA256 `328baec8de537c41ed0a9828c9f959cd84330a66bf68aef889ee5da264b40ebb`），只取N10 Pro二代段，不挪用后面的Clear6 Turbo。旧评测原件为work/child-information-deepening/hanvon-review.html（SHA256 `bb960e513f3b9db260253c950015fd42d107fa8dda3945f238180789b4fbe1a0`）。旧TTS/分屏照片本轮未重看，不新增visual或观察时间。

## 失败与尚未回答

- https://zhuanlan.zhihu.com/p/1905680180795639326 普通GET403，650B；停止。失败体SHA256 `29e1d21dd69d42d9fc9f7bddefb11309e0ea415a9b3801bb1c3ceeee7985dc61`。
- https://post.smzdm.com/p/ak3z4588/ 普通GET200但1697B只有验证码脚本，正文提取0B；按不可读停止。原件SHA256 `ff9c58f37a6a88883f57fb0bb55673b6ea43ca7da2ad89ede5277817b93041b7`。未执行TencentCaptcha、其POST、浏览器或任何验证。
- 旧官网web405路径未再尝试；旧下载索引的未命中只作旧采集结果。没有取得本机完整手册/机内帮助或型号化固件日志。代际未知的228页N10pro预览和2024 N10恢复视频继续排除。
- 原生目录/书签/返回原文、TTS启动/到期/后台/中断、PDF/图片导出对象及再编辑、自动保存、同步删除、备份/重装/重置恢复仍未知。未写成不支持，也未用普通Android行为补齐。

## 精确命令与退出状态

cwd始终/home/workspace/reader。读取文件包括AGENTS.md、README.md、docs/requirements.md、template.md、现案例、主E/F中M008全部8E/6F、ideas/backlog相关行、旧fetch-report与上述正文；目录/摘要只是文档材料，没有启动设备或App。

| 命令 | 退出/结果 |
| --- | --- |
| `python3 work/hanvon-controls-deepening/capture.py itsjzx https://itsjzx.com/ai/44320 zhihu-compare https://zhuanlan.zhihu.com/p/1905680180795639326 smzdm-user https://post.smzdm.com/p/ak3z4588/` | exit0；记录200/403/200，不把进程成功当文章均可读 |
| `python3 work/hanvon-controls-deepening/capture.py sina-review https://zhongce.sina.com.cn/article/view/189952` | exit0；HTTP200正文 |
| `sed -n '1,260p' work/hanvon-controls-deepening/raw/itsjzx.txt` | exit0 |
| `sed -n '1,300p' work/hanvon-controls-deepening/raw/sina-review.txt` | exit0（原调用还读取既有批schema） |
| `cat work/hanvon-controls-deepening/raw/smzdm-user.html` | exit0；本地识别验证码，没有执行脚本 |
| `python3 work/hanvon-controls-deepening/build.py` | exit0；生成2E/0F/53目录与建议；随后apply_patch修正case历史时间/来源归因 |
| `python3 work/hanvon-controls-deepening/check.py` | exit0、PASS；合并前可重复，只读 |
| `git diff --check -- docs/research/interaction-library/cases/M-008-hanvon.md docs/research/interaction-library/batches/hanvon-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/hanvon-controls-deepening-coverage.csv docs/research/interaction-library/batches/hanvon-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/hanvon-controls-deepening/source-notes.md` | exit0；这些文件当前为未跟踪资料，另以内容/哈希检查覆盖，不能把空git diff当全部内容已审 |

普通本地读取例外：首次复合rg含不存在的work/ai-notebooks路径，exit2，改定位到work/ai-notebooks-research；前缀/URL查重无命中rg exit1属预期。读取参考建议文件时误读不存在的work/meebook-controls-deepening/old-evidence-updates.json，复合cat exit1，仅为本地路径错误，未重跑旧生成器或改变旧批。

检查已核原件/摘记散列、newE与全库ID/URL无冲突、4条旧F完整before与2条旧E追加基线、案例仍只有F01–06、相对链接、CSV列宽与真实换行。coverage为1行表头/0记录，inventory为54物理行/53记录，无字面量\\r\\n或CR。未运行产品构建/测试或全库校验器。

状态改变仅为本案例、本批正式文档及work研究文件；没有改外部账户、设备、数据、安装软件或下载APK/固件/音视频/图片。没有新需求或实现决策。总体目标仍由root负责，不在本批标为完成。
