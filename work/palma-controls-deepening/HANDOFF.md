# M010 Palma 2 深化交接

2026-10-27。固定Palma 2黑白6.13英寸/Android13；实际固件和NeoReader构建未知，不采用Palma 2 Pro/Palma 3的能力。4新E、0新F；8旧F共31字段建议；14旧E additional_captures建议；137目录记录。新E均含supersedes:null，所有hands_on=false。未改主表、旧批、索引、全局文档或需求。

## 文件

正式修改仅cases/M-010-palma.md；新正式文件（均相对docs/research/interaction-library）：

- batches/palma-controls-deepening-evidence.jsonl
- batches/palma-controls-deepening-coverage.csv：0新行，仅表头
- batches/palma-controls-deepening-inventory.csv：73个PDF书签目录项+59篇旧官方PC分类索引/14节+5缺口；目录不是Palma 2功能全集
- artifacts/palma-controls-deepening/source-notes.md
- 同artifact目录：mobile-v4-p001.png、mobile-v4-p086.png、mobile-v4-p095.png、mobile-v4-p097.png、palma2-smart-button.jpg

work/palma-controls-deepening下：case-before.md、baseline.json、coverage-before.json、evidence-before.json、old-coverage-updates.json、old-evidence-additional-captures.json、formal-manifest.json、inventory-summary.json、captures.jsonl、image-fetch.json、raw/原件与提取文件、check.py、build.py、capture.py。

旧F建议精确字段：01/02/07各改evidence_ids、known_branches、gaps、visual_status、checked_on；08改evidence_ids、known_branches、gaps、checked_on；05/06/09/10各改known_branches、gaps、checked_on。共8行31字段，depth及hands_on不变。新增静态状态均用现有static_reference_only。每条均含完整before/after和changed_fields。

案例基线SHA256 `bac9893b5f4ddef8b34c025a6316816b258be12aa18c94f11e1bd43ed9c1c001`；baseline.json记录反向扫描，未发现既有E引用本案例路径/散列。最终所有正式文件散列在formal-manifest.json。

## 来源复用与新E映射

- E-PALMA-01、02、15同URL再取，分别留palma-product.html、palma-launch.html、mobile-v4-existing.pdf，**只给additional_captures**，不另立整册/产品页E。
- E-PALMA-04/09/10/11/12/13/14/16及E-BXCD-06/14本地复读已存官方JSON；E-BXCD-01复读已存59篇分类索引。三项联网+十一项本地=14旧E追加记录，不更新其旧采集日期。
- E-PLCD-01：新的通用Troubleshooting URL，只采音量翻页/USB问答；旧JSON全文已有，本轮本地读取。Notes恢复问答排除。
- E-PLCD-02：新的E Ink Tablets Sleep and Battery URL，与既有E-BXCD-14的PC类同名页比较Power/Display入口；不是本机当前菜单证明。
- E-PLCD-03：官方Palma 2产品页中的Smart Button图片，derived_from E-PALMA-01；新图片URL。
- E-PLCD-04：**derived_from E-PALMA-15**的四页静态观察；URL故意与旧PDF相同，原PDF不重复作为新来源。新E只覆盖实际看过的4页，附observed_artifacts及散列。

## 根优先复核与页码

PDF原件：work/palma-controls-deepening/raw/mobile-v4-existing.pdf，103页，SHA256 `66a194e882c852677433b96a8d5ac128adf9f9cfb652f73da6738d7f6f937808`，原URL https://ebookreader.dk/images/upload/Onyx%20BOOX%20Palma%202%20Pro%20User%20Manual.pdf 。封面是系列通用V4.0；作者元数据ONYX INTERNATIONAL INC.，创建/修改2024-12-10，不当作发布日期。文件名含Pro不作为型号适用依据。

页码全部是1-based PDF页序：实际查看p1封面、p86智能键、p95电源图、p97导航图。所看四页无印刷页号；没有建立印刷页映射，PDF73条outline属于书签目录而非冒充印刷目录。原摘页在work/palma-controls-deepening/raw/，正式副本在上述artifact目录；观察审计source-notes.md。只读到的其他正文页在提取件mobile-v4-pages.json/mobile-v4-existing.txt与目录coverage_status中区分，不标visual。

关键Power正文p94–95给睡眠后蓝牙在线播放应保留Wi-Fi、蓝牙、音频的条件；图中Wi-Fi/Bluetooth均Turn On、Audio为Pause，仅图示值。实际下级选项、开关优先级、TTS暂停后句子和锁屏恢复均未知，不说开启后三项必然续播。

Palma 2宣传图原URL https://cdn.shopify.com/s/files/1/0604/6410/3654/files/09-1_4d4f5614-88bf-4f8e-b65e-83508b238a2a.jpg?v=1729483826 ，SHA256 `0031c40c6ecde6ec2a85dd3dfb29fcd97be59a11cba2085d00dee6bae270d4d8`。短按/双击/长按三项图示None；通用PDFp86例值不同，两者都不能证明出厂默认或一次固件变更。根已实际看Power和这张产品图，并读PDF p1/86/94/95/97文字；未因根复核新增E。

## 命令、失败与状态

cwd=/home/workspace/reader。已读AGENTS.md、README.md、docs/requirements.md、M010全文、主表全部10F/17E、既有BOOX HANDOFF/手册获取说明、分类/章节JSON与选定正文。没有运行任何旧Hanvon或BOOX构建器/检查器。

精确主要命令：

1. `python3 work/palma-controls-deepening/capture.py manual-thread https://help.boox.com/hc/en-us/community/posts/33990902752916-How-to-use-palma-2-any-user-manual`：exit0，但HTTP404，停止该页面。
2. `python3 work/palma-controls-deepening/capture.py mobile-manual 'https://help.boox.com/attachments/token/jKLQjC6lSvfdGRhcgFuU9UeXM/?name=BOOX+Mobile+ePaper+Series%2820241210%29.pdf'`：exit0，但独立附件HTTP404，停止。两份失败体SHA256均`fb2c1868a18f588912187a63f1fd0d23173711ffa0522eee25748bed9f05686a`；未换域或重试。这是搜索索引中发现的历史公开地址，非已获取手册。
3. `python3 work/palma-controls-deepening/capture.py mobile-v4-existing 'https://ebookreader.dk/images/upload/Onyx%20BOOX%20Palma%202%20Pro%20User%20Manual.pdf' palma-product https://shop.boox.com/products/palma2 palma-launch https://shop.boox.com/blogs/news/palma2-mindful-living`：exit0，三个HTTP200。
4. PDF提取/渲染用`PYTHONPATH=work/research/tools/pymupdf python3 - <<'PY'`内联脚本：pymupdf.open原PDF，逐页get_text、get_toc、get_page_labels；仅p1/86/95/97用Matrix(1.5,1.5)渲染。两次脚本均exit0；已有库，无安装。
5. 产品静态图用一次`requests.get(上述确切图片URL, timeout=35)`，内联Python exit0、HTTP200。随后view_image实际查看1张产品图与4张PDF摘页，均成功。
6. `python3 work/palma-controls-deepening/build.py`：exit0，4E/0F/137目录/8旧F31字段/14旧E。构建后用独立本地脚本收紧“不能证明默认值”措辞、统一CSV证据分隔符、补观察元数据并刷新散列。**不要重跑build.py**；它有案例基线断言。
7. 合并前只读检查：`python3 work/palma-controls-deepening/check.py`。预期检查before基线、散列、派生URL例外、编号/引用/相对路径及CSV真实换行；合并后旧before会自然不再适用。
8. 定向格式检查：`git diff --check -- docs/research/interaction-library/cases/M-010-palma.md docs/research/interaction-library/batches/palma-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/palma-controls-deepening-coverage.csv docs/research/interaction-library/batches/palma-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/palma-controls-deepening`。正式资料当前未跟踪，空diff不能代替内容/哈希检查。

首次复合命令最后执行前缀`rg -n 'E-PLCD|palma-controls-deepening' docs/research/interaction-library --glob '*.jsonl' --glob '*.csv' --glob '*.md'`无匹配exit1，前面的基线/目录命令成功；属于预期无碰撞，不是访问失败。当前其余读取/写文档命令exit0。最终check和diff退出码另见check-results.json。

未决：实际安装版本、指纹设置/失败、按App实体键配置与锁屏触发、保活/冻结/自启动/睡眠控制优先级、TTS句子/蓝牙/闹钟恢复；书库恢复的覆盖/重复名/失败、第三方App重装数据及删除传播。未借Notes回收站或Pro手写补足。

只改变授权案例、本批文档/静态摘页和work文件；没有登录、提交、账户/设备动作、安装或音视频/固件/APK下载。总体goal由root判断，本批未修改其状态。
