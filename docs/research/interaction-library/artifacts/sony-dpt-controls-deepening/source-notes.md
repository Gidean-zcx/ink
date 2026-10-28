# Sony DPT 1.6：本轮来源摘记

查阅2026-10-27。固定RP1/CP1、指南4-597-942-19(1)；不是当前安装版本。原件留work，以下为研究者概括。

## 获取与读取范围

复用99页PDF，按目录核读操作和故障正文6–95页，法律/规格作背景。新GET36份HTML均200；最初只抽article漏步骤，离线改为id_content后全部有正文，原始HTML不变。PDF完整目录与网页73条叶入口对应；模板入口重复，72个唯一URL。网页正文抽检文档栏、锁屏、蓝牙、文件管理、橡皮、页管理、临时查看、side-note、更新和只读故障；其余以同版PDF核读为主，不能以抓到HTML充当新实操。

## 静态观察

实际查看p15、56、61。p15工具栏有历史前后箭头及其页码、页位置条、工具组、底部文件名；不是连续跳转。p56显示笔橡皮键位置，下方电子纸对比图不完整，整笔画擦除结论取文字规则。p61为校准文字步骤，无界面连拍，故只留work。正式归档p15/56。

## 任务定位与状态含义

| 任务 | 页码 | 本轮概括 |
| --- | --- | --- |
| F-M059-11 传入PDF并传回电脑 | 24, 26, 95 | 只传PDF、单文件≤1GB；All documents/搜索结果页不能作为传入位置；断开连接可能损坏数据，空间不足先查容量。 |
| F-M059-12 整理文件且识别同步关系变化 | 33, 36 | PC App剪贴只在App内，不等于电脑文件剪贴；同步目录改名或移动会删除同步配置，继续同步须重设。删除入口已知，确认与回收未见。 |
| F-M059-13 定位正文并回到跳转前页面 | 15, 16, 42, 48 | 大小写不区分；手写与图片内文字不可搜。普通划页不生成跳转历史；无PDF书签时目录按钮不出现。 |
| F-M059-14 放大批注与缩略页跳转 | 43, 44, 45 | 放大可写/擦/高亮但不可翻页；工具圈选放大不能再次改比例，须退出重选。缩略通常16页，并排状态9页。 |
| F-M059-15 写画、高亮、整笔画擦除与撤销 | 54, 55, 56, 57, 91 | 写完即保存；擦中部分线会去掉整条线，不是像素擦除。换页清撤销。扫描图/图片文字/手写不能文本高亮；低笔电、正在保存或PC操作可阻止批注。 |
| F-M059-16 从批注列表回访并处理不可编辑 | 52, 59, 60, 92 | DPT-S1便笺/书签不随关闭限制变可编辑；受保护文档/低空间只读、低笔电、PC正操作也是不同原因。条目多可能慢，无计时实测。 |
| F-M059-17 创建独立笔记并增删页 | 63, 64, 68 | PDF带笔记属性；即使原文打开，从Home创建也不形成关联。Delete Current Page后手册说不能恢复；双页视图不可增删页。 |
| F-M059-18 添加PDF模板并选择默认模板 | 35, 69, 70 | 多页PDF只取第一页；密码保护PDF不能加入；模板不是原多页资料的完整导入。 |
| F-M059-19 填写PDF表单 | 62, 98 | 支持五类表单对象；文本框明确保存按钮，不能由手写即时保存规则代替。 |
| F-M059-20 在电脑只读查看设备文档 | 53 | 文件临时放在电脑，关闭Digital Paper App时删除；不是Transfer导出或独立备份。 |
| F-M059-21 把设备画面显示到电脑 | 17, 32 | 该入口显示设备屏幕并可保存图像；蓝牙比USB/Wi-Fi慢是手册定性说明。 |
| F-M059-22 校准笔触位置 | 61, 85 | 第一次描线抬笔才显示；确认页仍偏移可Recalibrate返回校正并再验，不能把未落笔显示当笔失效。 |
| F-M059-23 设置锁屏并用已登记卡片解锁 | 10, 11, 12 | 只登记一张；换卡先注销。唤醒可密码或登记FeliCa解锁；注销需密码。忘记密码的手册服务移除会清全部文档，费用/现今可用性未核。 |
| F-M059-24 连接电脑并在休眠后恢复连接 | 19, 20, 21, 22, 23, 73, 74, 76, 77, 88 | Wi-Fi/蓝牙各有睡眠保持设置；Windows可自动蓝牙重连，Mac需系统操作。多设备用Tools→Switch Digital Paper→Add/Switch，不能同时传文件；Re-pair另机可能删除原同步设置。 |
| F-M059-25 检查更新与核对设备版本 | 73, 74, 75 | 固件早于1.2先升级PC App再升设备；自动通知可配置，通知不是已升级。 |
| F-M059-26 按文件信息检索资料 | 30, 50 | 设备查文件名/标题/作者；PC还查文件夹名，结果双击文件夹展开、文档交电脑PDF阅读器；不是全文OCR检索。 |
| F-M059-27 无响应时复位并辨认数据风险 | 72, 82, 93 | 指南明确复位可能丢设置、批注和笔记。另一个Initialize Digital Paper菜单清存储、恢复默认并自动关机，不能与普通复位视为同一动作。 |

## 新网页来源

| E | 标题 | 定位 |
| --- | --- | --- |
| E-DPCD-01 | [1.6官方帮助首页与完整叶目录](https://helpguide.sony.net/dpt/rp1/v1/en/index.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-02 | [Setting the screen lock](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178332.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-03 | [Disabling the screen lock via FeliCa card/FeliCa-compatible device](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178338.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-04 | [Document screen](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178330.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-05 | [Connecting via Wi-Fi/Connecting using a USB cable](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178315.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-06 | [Connecting via Bluetooth](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001273123.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-07 | [Connecting a second and subsequent Digital Paper](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001690498.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-08 | [Transferring a document from a computer](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178322.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-09 | [Transferring a document to a computer](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178319.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-10 | [Displaying the screen of the Digital Paper on the computer via Digital Paper App](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001690538.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-11 | [Changing the organization of documents/folders on the Digital Paper](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178310.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-12 | [Adding/editing a note template](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178312.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-13 | [Zooming in on a document](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001273122.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-14 | [Display page thumbnails](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178328.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-15 | [Searching for a string of text within a document](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178301.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-16 | [Displaying a document on the computer](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178341.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-17 | [Handwriting on a document](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178298.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-18 | [Adding a highlight](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178297.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-19 | [Using the eraser](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178309.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-20 | [Displaying the annotation list](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178308.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-21 | [Limiting edits to annotations](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178307.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-22 | [Calibrating the pen](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178306.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-23 | [Using the PDF form](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001690500.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-24 | [Creating a new note](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178282.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-25 | [Adding/deleting a page from a note](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178283.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-26 | [Selecting defaults for the note template](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178279.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-27 | [Changing the settings on the Digital Paper](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178278.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-28 | [Changing the Digital Paper settings using the Digital Paper App](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178281.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-29 | [Updating the software](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178296.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-30 | [Setting up the Digital Paper App](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178280.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-31 | [The screen is not appearing](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178304.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-32 | [Cannot add annotations](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178285.html) | id_content；对应PDF章节见目录台账 |
| E-DPCD-33 | [Cannot edit or delete annotations](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178286.html) | id_content；对应PDF章节见目录台账 |

## 限制

没有运行Digital Paper App、连接设备、配对、上传、删除或复位。当前OS与服务供应、失败回执、速度、触感和备份回导仍未知。旧原文改名与同步删改冲突没有依据，不能用同源后继产品QUADERNO补齐。所有产品启发仍是建议。
