# M-064 iBasso DX180：目录、应用与USB用途交接

查阅/观察日期2026-10-27。本批收束，正式文件及建议JSON已冻结。仅文档和本地资料变化；无安装/卸载、账号、购买、连接设备、发文件或升级动作。没有下载APK、固件或媒体。没有重跑Huawei、Anki或hifi旧构建脚本。

## 交付数量及文件

根目录 `/home/workspace/reader`。

- `docs/research/interaction-library/cases/M-064-ibasso.md`：原12F保留，补F01/06/09/10；新增F13安装、F14卸载、F15USB对外供电。三项是历史手册D2，不是实操成功记录。
- `docs/research/interaction-library/batches/ibasso-controls-deepening-evidence.jsonl`：**1新E，E-IBDC-01**；日本MUSIN一手公告，地区/版本分开。
- `docs/research/interaction-library/batches/ibasso-controls-deepening-coverage.csv`：**3新F**；全部hands_on=false、D2/static_reference_only，20页只有文字。
- `docs/research/interaction-library/batches/ibasso-controls-deepening-inventory.csv`：**129行**，明确拆成13原目录（含父子）、41正文功能域、37实看静态菜单、37中文版本声明、1区域解释。层级/功能/菜单有重叠，不能累计成129功能完成。
- `docs/research/interaction-library/artifacts/ibasso-controls-deepening/source-notes.md`。
- 同artifact目录六张 `dx180-printed-{17,18,20,21,34,42}.png`。

仅建议根合并，未写旧批：

- `work/ibasso-controls-deepening/existing-coverage-updates.json` 和同名CSV：**4旧F、14字段**。F01/06各改evidence_ids、known_branches、gaps、checked_on；F09/10各改known_branches、gaps、checked_on。逐字段before精确匹配旧批，深度/编号不变。
- `work/ibasso-controls-deepening/additional-captures-updates.json`：**3旧E**，E-IBCD-01复读PDF及六静态页，E-IBCD-08重新取得同SHA中文日志，E-IBA-03取得英文规范URL目录。原PDF未重新联网下载；同文档不新编号。
- `work/ibasso-controls-deepening/material-hashes.json`：冻结时全部正式文件与主要原件SHA。
- `manual-metadata.json`、`manual-text.json`、`render-manifest.json`、`fetch-log.json`、`inventory-counts.json`、`check-result.json` 记录取得和验证。

## 核心原件与定位

| 来源 | 精确定位 | 原件与SHA-256 |
| --- | --- | --- |
| [DX180官方手册](https://www.ibasso.com/uploadfiles/download/DX180userguide.pdf) | 印刷1目录；17安装/18卸载/20USB供电；31–34排序导航/39最近50曲/42搜索；49–54 PEQ条件 | `work/hifi-extension-research/DX180userguide.pdf`；`bbbb517f695f36baa62d77585f38bbd6b9407576e96c4e756d965c96103f1da8` |
| [中国DX180日志](https://ibasso.cn/support/dx180firmware) | 1.04.237第1–2项；1.05.273第5项；全部5版本37项 | `work/ibasso-controls-deepening/firmware-cn.html`；`c9a111a08010b38a11934502d5d061bacd104d5ae65aa861b1e339fc7434d615`，与旧root原件相同 |
| [MUSIN一手新闻稿](https://prtimes.jp/main/html/rd/p/000000144.000139013.html) | 署名MUSIN；2025-07-22 17:00；DX180(v1.04)第1–2条；勿读成DX340段 | `work/ibasso-controls-deepening/musin-release.html`；`6744d2e08e5cc62ccb3eae49d377a8909fae4e485121992795e40fa670c3ada7` |
| [品牌经销名录](https://ibasso.com/where-to-buy/) | Japan段，Distributor Musin Co., LTD及其域名邮箱；只证发布方关系 | `work/ibasso-controls-deepening/distributors.html`；`c50b32d3f2dd001427f0902fde86a9b156bd965d11543f192124c37ee9c5d5c8` |
| [英文固件目录](https://ibasso.com/dx180-firmware/) | V1.01–V1.05及日期，规范路径对应旧?p=3371内容 | `work/ibasso-controls-deepening/firmware-en.html`；`ae03d36392cce123b57e19d74bdecadad6aff7657f37c1b97288db1e5959030a` |

PDF元数据生成/修改均显示2024-06-13、时区分别+09/+08；这只描述文件元数据，不等于官方发布/修订日期。历史About图仍只证明手册示例Android13，实际Mango/系统构建未知。

日本说明把系统关机与Mango按计时停止播放分开；作为独立区域解释补F06。**保留中文“定时暂时播放”疑误**，不把日本译意直接改成中国站更正。1.05“始终有效”只有声明，当前结束反馈、操作重计、后台事件和取消全链仍缺。中国标题v1.0与英文V1.01同日，台账保留原名。

## 实际看图与边界

使用view_image逐页查看印刷 **17、18、20、21、34、42**，文件页相应 **18、19、21、22、35、43**。六页都来自同一旧PDF，没有连续操作：

- 17：浏览器/空下载列表，未见安装成功或权限弹窗。
- 18：Settings/Apps/App info并排，Uninstall入口；未见确认或结束。
- 20：纯文字供电步骤，不是USB选项界面。
- 21：主设置和系统子菜单；Backup/Reset只入口，不填恢复链。
- 34：分类/存储/Recent Play/搜索侧栏。
- 42：关键词框和搜索历史区/删除样式图标，无查询结果或清除动作。

原9静态摘页不重新编号或复制，沿用既有引用。本轮录图的SHA在render-manifest、additional-captures与material-hashes一致。

## 不确定与停止路径

未证安装反馈/权限拒绝、卸载保留对象、重装恢复、USB停止/电量门槛/并行用途；不能用通用Android补完。Time排序语义、标签写回、搜索空结果/历史清理、时间断点均未知。PEQ数值范围不证明输入校验或音质收益。更新日志只D1，不把应用固定、ReplayGain、局域网和降级声明做新D2。

公开资料取得：脚本5个普通HTML GET，4个HTTP200；MUSIN站内新闻直接请求远端关闭，没有本地原件。web工具先已读 [站内新闻](https://musinltd.com/News/1165.html)正文（13:33），其下游 `https://musinltd.com/portal/article/index.html?id=812` 返回444，停止且未重试。PR TIMES同机构稿原HTML可读，时刻17:00分开保存。未遇401/403/429，未进入账号；厂家页底部登录控件不是本次登录操作。无固件包点击或下载。

## 命令、退出与检查

工作目录 `/home/workspace/reader`。本批脚本均已执行、exit=0：

```text
python3 work/ibasso-controls-deepening/capture.py
python3 work/ibasso-controls-deepening/render.py
python3 work/ibasso-controls-deepening/build.py
python3 work/ibasso-controls-deepening/inventory.py
python3 work/ibasso-controls-deepening/update-case.py
python3 work/ibasso-controls-deepening/check.py
```

capture的exit0表示完成记录，**不表示5源全成功**；实际失败在fetch-log。render使用既有 `work/research/tools/pymupdf`，没有安装依赖；出现fitz将弃用的FutureWarning，退出0，PDF63页与散列核对通过。update-case后仅修正功能目录对F13–15的关联，当前正式case为最终版本，勿再用脚本覆盖。

先读AGENTS/README/requirements/相关ideas、template、M064case、hifi-extension及hifi-controls旧F/E；用rg定位旧文件并核E-IBDC无碰撞，Python读取既有PDF文本与原HTML。一次诊断命令尝试rg `musin-news.txt` 报文件不存在（实际该URL抓取失败），管道head退出0；没有把缺失件当正文。其余读取结果无隐藏的取得成功声明。

合并前检查：**376 PASS，exit0**。涵盖ID碰撞、3新F及4旧F14字段、3旧E的before、129目录引用与分类、正式路径/链接、六实看页SHA、HTTP原件SHA、LF/空白、四份旧批SHA未变。旧批合并后不要再跑依赖before/散列不变的check，也不要重跑capture/build。

```text
git diff --check -- docs/research/interaction-library/cases/M-064-ibasso.md docs/research/interaction-library/batches/ibasso-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/ibasso-controls-deepening-coverage.csv docs/research/interaction-library/batches/ibasso-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/ibasso-controls-deepening
```

exit0，无输出；路径当前未跟踪，实质内容检查由check.py完成。限上述自身路径git status exit0，只显示交付文件；不把协作仓库整体状态当本批变更。主表、旧批、全局文档未修改。

根代理回执：已独立复核相关PDF正文、日本公告和中文日志，并实际查看印刷18/20；合并前重跑check得到376 PASS。根已将冻结建议合入旧批。本代理收到回执后只补此工作记录，不再运行build/check，也不编辑正式资料。
