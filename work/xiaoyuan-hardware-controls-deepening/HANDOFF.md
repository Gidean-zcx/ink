# M-056 小猿学练硬件补强交接（2026-10-27）

## 结果与范围

0 新 E、0 新 F；保留 6 个任务、5 个 D1 与家长账号注销局部 D2。33 项来源/功能目录不是完整机内菜单，更不表示完成全部功能。硬件范围是已取得官网前端的 S6 条目及彩墨 E1，固件/课程版本未知；旧 S2、S1 与 E1 课程级别分开，未移植 M-028 小猿 AI 手机软件、T6 或其他同名新代。目录包含产品模块、旧静态记录、政策路径及版本记录，未取得完整硬件手册。

有限补查共 20 条查询，最后 4 条专找同型号作者实拍/实用记录。两个什么值得买候选正常打开超时；知乎自述体验候选 HTTP 403 后停止。未取得可核作者、样机/构建、连续界面原文；未从视频标题、摘要或购物汇编填流程。新网络原件仅家长 App Store 的一次 HTTP 200 GET。无登录、安装、购买、客服消息、个人接口请求、APK/音视频/固件下载或设备操作；本轮未查看任何新图片。

## 变动文件和合并建议

直接修改：
- `docs/research/interaction-library/cases/M-056-xiaoyuan-hardware.md`

新增正式文件：
- `docs/research/interaction-library/batches/xiaoyuan-hardware-controls-deepening-inventory.csv`（33 行）
- `docs/research/interaction-library/batches/xiaoyuan-hardware-controls-deepening-evidence.jsonl`（0 行）
- `docs/research/interaction-library/batches/xiaoyuan-hardware-controls-deepening-coverage.csv`（标准表头，0 行）
- `docs/research/interaction-library/artifacts/xiaoyuan-hardware-controls-deepening/source-notes.md`

以下只提交建议，未应用到旧批/master：
- `work/xiaoyuan-hardware-controls-deepening/existing-coverage-updates.json`：6 个旧 F 的 16 个字段逐项 before/after；没有深度或视觉状态升级。
- `work/xiaoyuan-hardware-controls-deepening/existing-evidence-additional-captures.json`：E-XYHW-01/02/05 共 3 次追加采集；01/05 是本地原件复读，02 是同 URL 新快照。均引用独立摘记而非可变案例文件。
- `work/xiaoyuan-hardware-controls-deepening/existing-evidence-field-updates.json`：E01 version_scope、E02 source_updated_on/note 共 3 字段。
- `case-before.md`、`coverage-before.json`、`evidence-before.json`、`baseline.json` 保留基线；`case.diff` 是与基线的精确差异。

旧 E 无依赖本案例 SHA 的 visual_audit_path；旧图和散列保持不动，不需要迁移审计。现有 `visual_observed=true` 仅保留原 E02/03/04 的前轮静态观察，本轮 additional_captures 和目录均 `not_observed`/false。没有创建新 E，前缀 E-XYHC 未使用。

## 核心可复查依据

1. E-XYHW-01，[官网公开前端](https://www.xiaoyuanyingjian.com/static/js/main.12d356ac.chunk.js)：`work/learning-hardware/official-main.js`，SHA `f3e9acdba2af4c5de5cd342a81d0300d57c5748d868c75338b98d1f112d092ec`。只复读。解码文本字符偏移：26367 年级/教材导致资源差异；42700–44000 两型号规格；90400–91300 六学练模块；74858/75339 听写/听力图片 alt（本轮没看图）；108917–110800 阅读/阅读灯/小程序上传；179695 S2/S6 同组件。没有据前端推断真实题目状态。检索出现后续同名代际新闻线索，但未取其正文来改规格，所以将“当前 S6”收窄为已取得前端条目。
2. E-XYHW-02，[家长 App Store](https://apps.apple.com/cn/app/%E5%B0%8F%E7%8C%BF%E6%99%BA%E8%83%BD%E8%AE%BE%E5%A4%87/id6738277050)：新原件 `work/xiaoyuan-hardware-controls-deepening/parent-store.html`，SHA `eb9a6caaa2efc25caaf05036db03983050e21eef75fcd9e42ec2e5c23921de2d`。1.25.1 对应 `time datetime="2026-09-25"`；iPhone/iOS13.5+；`store-version-index.json` 25 条版本详情均泛化改善，不支持特定故障已修复或硬件恢复链。旧截图未重新看，评论未用于证据。
3. E-XYHW-05，[家长政策正文](https://solar-online.fbcontent.cn/megrez-online/megrez-gallery/megrez-resource/1a017eac33f2fb4.html)，2025-08-15：原件 `work/learning-hardware/parent-privacy-body.html`，SHA `cda62866625f96c6bffbfb25a7c095dbf788e5c1621f47cd118ddc1a161a3b01`。只复读。解析文本行75共享登录、217各关联产品分别注销/提出删除、274家庭资料修改、277备份更新时删除、281注销条件/验证码、282–285登录后的副本请求与身份核实后十五天答复。共享登录不证明进度迁移；服务端备份删除延迟不等于用户可恢复；副本请求不证明格式、交付范围或回灌能力。

正式摘记 SHA `b23dd9adc9131e36885fe67b7d61f85dbfdffad09b471c5e5fac119cc032c14c`。案例 SHA `b28c96eeffc18ed03881871cedaa780697013ade75e49c15a82384eb627005cf`。其余精确文件大小/SHA 由 `manifest.json` 列出；正式归档路径相对 interaction-library，work 原件单列。

## 实际命令和退出状态

工作目录 `/home/workspace/reader`。资料读取涉及 AGENTS.md、README.md、requirements/ideas 相关段、本例全部 F/E、旧 `work/learning-hardware/` 原件与失败记录。使用 `rg`、`cat`、`sed` 及 Python 只读解析，shell 均 exit 0。公开搜索/打开不是 shell 命令，没有进程退出码；逐查询与候选状态见 `discovery.json`。未重跑旧批脚本。

实际关键命令：

```sh
python3 work/xiaoyuan-hardware-controls-deepening/capture.py parent-store.html 'https://apps.apple.com/cn/app/%E5%B0%8F%E7%8C%BF%E6%99%BA%E8%83%BD%E8%AE%BE%E5%A4%87/id6738277050'
python3 work/xiaoyuan-hardware-controls-deepening/build.py
python3 work/xiaoyuan-hardware-controls-deepening/finish.py
python3 work/xiaoyuan-hardware-controls-deepening/check.py
sed -n '270,288p' work/learning-hardware/parent-privacy-body.txt
sed -n '70,79p' work/learning-hardware/parent-privacy-body.txt
sed -n '210,220p' work/learning-hardware/parent-privacy-body.txt
sed -n '1,180p' work/xiaoyuan-hardware-controls-deepening/case.diff
```

全部 exit 0。build 在冻结前因目录补充和措辞收窄重跑，最终为 33 行/16 个旧 F 字段；finish 保存 20 条查询和差异。版本解析初次误用 `__typename` 得零项但进程 exit 0，已改用实际 `$kind=TitledParagraph`，核出 25 项；没有把零项当没有日志。合并前首轮 check 为 237 PASS；冻结 manifest 后再检查散列，最终输出另见 `check-output.json`。检查覆盖旧 F/E before、原件/正式散列、范围/来源映射、LF/表头、案例链接及无视觉升级。

**合并前复现只运行上述 check.py；合并后 before 已变，不再运行 build/finish/check。** 根负责原件抽核与合并。工具源码保留用于审计，不是产品代码。manifest 只固定本批文件和所引原件，未冻结根并行维护的主表。

## 未决和状态

核心题目入口、手写识别纠正、错误提示、再答记分、完成奖励、暂退/重启/断网恢复依旧未知；家长限时的保存/生效/离线冲突、文件在设备端接收/打开/打印和换机恢复也未知。当前目录不是硬件完整菜单。下一类有用证据是带 S6/E1 型号与固件的随箱手册、机内指南或作者原创连续截图，至少覆盖同题错答至恢复。

状态仅改变本例文档和专属新材料/建议；无外部产品或账户状态变化，旧批/master/全局未修改。冻结后停止写正式资料与建议，等待根核验。
