# M025 普通大陆有道词典笔2.0：本轮交接

日期2026-10-27。有限资料探索已收束；**0新E、0新F、0旧F字段变更**，原5F的D1/D2边界保持。E-YDCD前缀已被旧批使用；E-YDCT查无占用，但本轮没有分配新编号。未取得同型号完整原厂指南或更强学习主链，不用目录/检索失败冒充功能证据。

正式文件与建议已冻结。根合并前可运行下列check；合并后不要重跑依赖旧before的check/build。本轮无账号/购买/登录、下载App/固件/音视频、发消息、删除或产品动作。没有新看图或实操。状态只改本地研究资料；未改旧批/master/全局文档，未重跑此前批次脚本。

## 文件与待合并内容

全部相对 `/home/workspace/reader`：

- `docs/research/interaction-library/cases/M-025-youdao-pen.md`：新增来源目录复核日期及本轮边界/台账链接；原5F内容和深度未升级。
- `docs/research/interaction-library/batches/youdao-controls-deepening-inventory.csv`：**20项覆盖台账**，真实入口/原件/旧F/E/缺口/下一类资料；不是完整手册目录。
- `docs/research/interaction-library/batches/youdao-controls-deepening-discovery.csv`：**8条独立探索/排除记录**，与功能目录分开。
- `docs/research/interaction-library/batches/youdao-controls-deepening-evidence.jsonl`：有意为空，0新E。
- `docs/research/interaction-library/batches/youdao-controls-deepening-coverage.csv`：标准表头，0新F。
- `docs/research/interaction-library/artifacts/youdao-controls-deepening/source-notes.md`：本地复读范围、失败限制、后续取证条件；没有新图片。

待根合并：`work/youdao-controls-deepening/additional-captures-updates.json`，**4旧E / 5份本地复读原件**：E-YDP-01两份（HTML/JS），E-YDP-02帮助JS，E-YDP-05入口HTML，E-YDCD-02广外HTML。before/after逐项提供，network_refetched=false；原采集日期2026-10-26，本轮checked_on=2026-10-27，visual_observed/hands_on均false。

`existing-coverage-updates.json` 为 `[]`，同名CSV只有表头，表示没有需改旧F字段。原图审计仍属于旧轮次，本轮没有重新查看或修改其旧visual元数据。`material-hashes.json`记录冻结时文件与原件散列。

## 关键原件、来源与散列

| 旧E | URL/定位 | 已保存原件及SHA-256 |
| --- | --- | --- |
| E-YDP-01 | [历史2.0产品页](https://c.youdao.com/dict/hardware/dictPen.html)，标题/使用说明跳转；本轮只读旧本地件 | `work/dictionary-research/youdao-2.html`；`bf9cd2c636296688be516c8289fe354b107ac068dcfd1311b00d91985bd3ade2` |
| E-YDP-01 | 同页引用v2.11/app.2389c148.js，多行/发音/语音/耳机/更新宣传 | `work/dictionary-research/youdao-app.js`；`c9a7c0e7aaf0965545104d6151bd2dec919683f3cd94a71827044a700e05b7db` |
| E-YDP-02 | [1.4.5帮助JS](https://shared.ydstatic.com/market/souti/dict_pen_sync/dict/1.4.5/dist/js/chunk-6b47e0f2.2e744c90.js)，只有登录/实时显示两个问题 | `work/youdao-controls-deepening/youdao-2-guide-1.4.5-help.js`；`406ada5aaa9b207adf8fbf7cf489451fda8128c07d5e501204588ebbb476d99f`，从原正式公开资源按字节复制，未联网重取 |
| E-YDP-05 | [帮助入口](https://shared.ydstatic.com/market/souti/dict_pen_sync/dict/1.4.5/dist/index_dict.html#/function_explain)，资源链接/桥接名 | `work/dictionary-research/youdao-guide.html`；`65607488c809b6e8f630d8ca2930d71bd54ed08e46307ad6c6f45f3daa322b60` |
| E-YDCD-02 | [广外借用公告](https://lib.gdufs.edu.cn/info/1133/6279.htm)，普通2.0标题/功能段和两个纸图图注 | `work/youdao-device-deepening/gdufs-2.html`；`7c2fa18793df5348d83f2b7de8ead319f6dbb188876ed903dba953768c0c8525` |

v2.11和1.4.5均为前端资源包，不是硬件/手机App版本。JS未运行，桥接名未被当真实按钮/成功响应，没有调用用户接口。旧E-YDP-03/04模板、技术文章及北外公告本轮没有重取原文，台账显式区分旧摘记与本轮复读。

## 探索结果与限制

- [现行首页](https://www.youdao.com/)web正文可读，含AI硬件链接，未取得普通2.0支持目录。
- `https://hardware.youdao.com/` 跳到上述首页；不表示旧产品停服。
- `https://c.youdao.com/` web返回 **403** 后停止该路线，没有再请求历史产品URL或绕过。
- 首页 `https://smart.youdao.com/` 链接被工具报告重定向至 `https://smartyoudao.com/collections/youdao-products`，工具500/non-retryable；没有访问目的页或借当前海外系列作事实。
- `https://post.smzdm.com/p/a5klmn0l/` 尝试正文返回 **Content length >1048577**；没有取得可用正文/新图片，不以搜索片段增加E/F。
- 桌面词典5.0、专业版/3代/海外型号与相似域名排除；没有用当前App同步能力填旧笔恢复。搜索结果不是完整“不存在”的证明。

下一类有效证据：能辨普通2.0型号/固件的完整随盒说明，或原厂连续操作材料，覆盖扫描起止/失败、候选校订、词条/发音、明确收藏完成、词本回取/复习和退出恢复；另需离线词库/队列以及导出/换机范围。现有材料仍不足完成这个学习闭环，缺口保持未知。

## 精确命令、退出与检查

工作目录 `/home/workspace/reader`。读取AGENTS、M025案例、旧E/F及work旧记录；此前同会话已读README、requirements及相关ideas。本轮`rg --files`定位原件，`rg -n 'E-YDCT' docs/research/interaction-library/batches`退出1表示无前缀命中；E-YDCD命中旧批正常。只读旧HTML/JS及定点文本，未执行其脚本。

实际构建与合并前检查：

```text
python3 work/youdao-controls-deepening/build.py
python3 work/youdao-controls-deepening/check.py
```

均 **exit0**。check输出 **110 PASS**：0新E/F/旧F变化、4旧E/5原件、20覆盖/8探索、无新观察，5份旧批SHA不变；还核对JSON/CSV、旧before、引用/路径、原件与摘记SHA、正常LF和末尾空格。构建非重复采集工具，不需重跑；合并前check仅检查本批及其旧字段依赖。

```text
git diff --check -- docs/research/interaction-library/cases/M-025-youdao-pen.md docs/research/interaction-library/batches/youdao-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/youdao-controls-deepening-coverage.csv docs/research/interaction-library/batches/youdao-controls-deepening-inventory.csv docs/research/interaction-library/batches/youdao-controls-deepening-discovery.csv docs/research/interaction-library/artifacts/youdao-controls-deepening
```

exit0，无输出。自身路径git status exit0，这些正式文件当前未跟踪，因此实质内容验证依靠check.py；没有把全仓共享变化归为本批。

联网只用web工具：7个搜索query原文见 `work/youdao-controls-deepening/search-queries.json`；open首页/hardware/c根、点击首页AI硬件链接、open单条作者评测。没有exec HTTP抓取；旧公开原件本地复读。失败结果分别记录，未把工具成功返回当正文获取成功。
