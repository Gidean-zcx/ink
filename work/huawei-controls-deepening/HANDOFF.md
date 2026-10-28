# M-061 Huawei MatePad Paper 有界研究交接

日期：2026-10-27。本批已收束；不要重跑 fetch。仅本地文档/资料文件变化，无登录、账户、设备、购买或固件状态变化。未调用 helper；未运行此前 Anki 批。未修改旧批、主表或全局总结。

## 交付与合并建议

正式文件均相对 `/home/workspace/reader/`：

- `docs/research/interaction-library/cases/M-061-huawei-paper.md`：原 10F 保留，补 F01/02/06/08 条件，新增 F11–17；共17F/21E编号，非完整覆盖声明。
- `docs/research/interaction-library/batches/huawei-controls-deepening-evidence.jsonl`：13 新 E，E-HWCD-01～13。
- `docs/research/interaction-library/batches/huawei-controls-deepening-coverage.csv`：7 新 F，F-M061-11～17，均为文档 D2；F12、16 仅静态参考，其余未观察；hands_on 全 false。
- `docs/research/interaction-library/batches/huawei-controls-deepening-inventory.csv`：25 项已发现功能域/来源与未决清单，**不是完整官方手册目录**。
- `docs/research/interaction-library/artifacts/huawei-controls-deepening/source-notes.md` 及五张 PNG 原图。

根代理待合并（我没有应用到旧批）：

- `work/huawei-controls-deepening/existing-coverage-updates.json`：F01/02/06/08，15 个逐字段 before/after；同名 CSV 可读视图，正常 LF。F01/06/08 更新 evidence_ids、known_branches、gaps、checked_on；F02 更新后三项。深度与编号不变。
- `work/huawei-controls-deepening/additional-captures-updates.json`：E-HWP-01～06、08 共 7 条旧本地 HTML 复读元数据，network_refetched=false，原始采集日期保留2026-10-26。本轮 checked_on=2026-10-27。E-HWP-07 原本地文件是搜索壳，未加正文快照。
- `work/huawei-controls-deepening/material-hashes.json`：原件与交付 SHA-256。各新 E 同时带 raw_work_path/raw_sha256；正式 archived_artifact 使用 library 相对路径，不填 work。

## 根可复核入口及关键定位

1. E-HWCD-04：[同步 FAQ](https://consumer.huawei.com/cn/support/content/zh-cn15931676/)，正文1–6及“仍无法解决”；原件 `work/huawei-controls-deepening/help-15931676.html`，SHA `166ce30c41aaec226e183018fb2ffd221af2e95bd2952666f1952af12d38ee43`。App版本、前台/账号/网络/容量/电量条件，Paper专属强停路径，局部改一笔、等待和复制均为排障尝试，非成功保证/备份。
2. E-HWCD-03：[NZ 录音标记](https://consumer.huawei.com/nz/support/content/en-gb15879066/)，Try it out 1–2、Note；原件 `record-nz.html`，SHA `74e2cb1281180bb82c0d4eec01b0fc0329116043d410258263652fe8143a752c`。标记只定位音频，未证笔迹/转写联动。
3. E-HWCD-02：[CN 笔记](https://consumer.huawei.com/cn/support/content/zh-cn15879070/) 与 [CA 同ID](https://consumer.huawei.com/ca/support/content/en-us15879070/)；CA Note 段才明确保存后 Move，CN只新建/套索/转文本/分享。CA 原件 `notes-ca.html` SHA `73e3686ac767c4e7dd4e3cfe027ae1e535eade57afa51cf1fc0e0b3856fbc522`。单篇明确图/PDF；多篇仅勾选后按提示分享，未扩大批量格式声明。
4. E-HWCD-07：[PDF 显示](https://consumer.huawei.com/cn/support/content/zh-cn15898848/)，裁边/对比度/去水印三段；原件 `help-15898848.html` SHA `78d9e404d78a178836f7a0f173e7a566b79dd9b898b03c09d93b88b211ab3958`。显示还原有文档；源PDF改写及导出效果未知。
5. E-HWCD-11：[灰度贴纸](https://consumer.huawei.com/cn/support/content/zh-cn16009712/)，问题/原因/方案；原件 SHA `433fc20f5b8e2236c51c20be3bcd904bdc00b695494c6f1f0a2d3f6ebaf4a708`。笔记12.4.2.265+、纸色与可改色下载贴纸特例；页头4.2不是Paper系统证明。

其他来源、具体正文定位与地区全部列正式 source-notes 和证据 JSONL。

## 地区、目录与观察边界

中文系统专属文档多为HarmonyOS2.1。不同页分别列Wi-Fi4+64/6+128/8+256及LTE6+128；不得从某页适用范围推所有机型。E13型号信息只列Wi-Fi4+64/6+128及HMW-W09；LTE硬件型号未核。CA笔记只Wi-Fi4+64、2.1。NZ录音/LK浮球/BD手势只Wi-Fi4+64，系统构建未列。共享同步/贴纸FAQ含本机专段，不表示安装版本已确认。

支持主页 E01 仅200目录壳，无完整PDF/帮助目录；E13仅型号；E12桌面正文已读但未做完整任务。25项inventory包括未找到/只有目录/正文部分覆盖，不代表25项完成。

实际查看五图（view_image，2026-10-27）：

- E03：`record-nz-image-1.png` 麦克风、`-2.png`圆录音、`-3.png`标记、`-4.png`方停止；原HTML tkb-content内0基img1–4。四个独立图标，不是连续界面。
- E07：`help-15898848-image-1.png`（0基img1）只示剪刀工具栏入口，未示裁框/拖动/保存。顶栏“没有SIM卡”不用于推断硬件通信装配。

图片 URL/哈希在 image-fetch-log.json 和 E.artifact_hashes；正式源图未加工。没有观看/下载视频、音频。

尚缺：完整官方指南、删除/回收/重装、同步冲突保留、录音离开与中断保存、音频与笔迹/文本关联、导出可编辑对象、源PDF写回、首次协同与断线写回、听书/锁屏/全部设置。同步不是备份。

## 取得与失败记录

匿名常规GET脚本共25请求（7+8+5+5），最终均HTTP200；其中三个中文文章15879066/15892864/15879072跳转搜索壳，不作正文，后改用搜索发现的真实官方区域URL。两个支持入口目录空壳。未遇401/403/429，未访问账户或调用公开前端提示的接口。

首次support-product.js被文本提取覆盖，fetch-extra-log明确将其raw字段置null，派生件不作原件；只另一次普通公开GET保存 `support-product-original.js`，SHA `3785953f098055dd3840d1760b24efd1495563a9cf51ba3d486be043fafca492`。仅读取文档目录函数线索，未调用服务接口。第三方指南镜像仅搜索发现，未下载/用于事实。

## 精确执行与退出状态

工作目录均 `/home/workspace/reader`。读AGENTS/README/requirements/相关ideas、旧案例与批次，及本轮 `cat`/`sed`/`rg` 内容检查均成功；下列主要执行命令 exit=0：

```text
python3 work/huawei-controls-deepening/fetch.py
python3 work/huawei-controls-deepening/fetch-extra.py
python3 work/huawei-controls-deepening/fetch-regional.py
python3 work/huawei-controls-deepening/fetch-images.py
python3 work/huawei-controls-deepening/build.py
python3 work/huawei-controls-deepening/update-case.py
python3 work/huawei-controls-deepening/check.py
```

本轮build最后重跑一次仅收紧“多篇分享格式”文字和重算SHA，没联网；update-case后有同一保守文字修订，当前正式case为最终版本，不应再用update-case覆盖。最终check输出 PASS、1477项断言，13E/7F/25目录/15旧字段/7旧采集/5实看图，旧extension-manuals两文件SHA未变。检查含ID跨批碰撞、引用、相对链接、每个原件/摘记/图SHA、字段before精确匹配、LF与旧批未动。

```text
git diff --check -- docs/research/interaction-library/cases/M-061-huawei-paper.md docs/research/interaction-library/batches/huawei-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/huawei-controls-deepening-coverage.csv docs/research/interaction-library/batches/huawei-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/huawei-controls-deepening
```

exit=0，无输出；这些路径目前均未跟踪，不能将此命令当完整内容校验，check.py做实质结构与散列检查。限自身路径git status exit=0，只列上述正式文件；work被忽略。其他协作文件未更改或检查。
