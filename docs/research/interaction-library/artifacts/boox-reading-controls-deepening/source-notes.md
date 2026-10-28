# BOOX阅读与笔记：本批取证范围

2026-10-27。对象仍为Note Air5 C / Android 15 / V4.1.2功能基线，实际构建及专用手册未得。本轮读10篇既有官方ePaper Tablet PCs帮助正文，不刷新网站正文：原件为2026-10-26取得的work/boox-controls-deepening/epaper-pc-articles.json，SHA256 02e30182beee444963ce691bbec9523380dcf91000c32685e27bea33f08a2cd8。文章updated_at与edited_at是帮助元数据，不是设备固件发布日期。

9篇此前仅目录级的正文单独建E，均derived_from旧分类快照E-BXCD-01；Handwritten Notes沿用E-BOOX-16追加本地复读，旧E-BXCD-13保持历史引用。4张新附件普通匿名GET均200并实际view_image；图像机型/构建没有核明，不能认作Note Air5 C V4.1.2画面。没有实机、连续操作、点击结果、测时、账户、安装或同步动作。

## 各正文的有效范围

| 文章ID / 定位 | 本轮支持的任务或控件 | 不能据此推出 |
| --- | --- | --- |
| 10701497927316 / Library Modes、Layout Options、Recent Files | 扫描/目录两模式及图标；扫描模式菜单有书架/筛选排序；两模式切封面/列表入口不同；最近最多12文件。 | 切模式移动文件、自动扫描所有路径、书架与磁盘目录同一对象；Manage Library图示不能补成移动/删除/回收流程。 |
| 10701489683092 / 操作步骤 | NeoReader中央菜单→More→Settings/Global Settings；状态栏开关、项目选择与拖动排序。 | 预计剩余时间的计算、读完判定、模式退出或配置持久化。 |
| 10701488996500 / 操作步骤 | NeoReader中央菜单→菜单→Settings/Touch Settings，选择预设或自定义翻页区。 | 当前默认九宫格、冲突优先级或误触恢复。 |
| 10701486858388 / Theme、Zoom、Layout、OCR | 文中明确V4.1：PDF主题、固定版式专用Layout；双指缩放须启用，框选缩放是另入口。OCR列在线中英每日300页与离线30+语言无页限。 | 不把OCR宣传的无需等待/更高准确率当测量；离线包/额度耗尽/识别失败与回写未核，未建独立完整OCR任务。 |
| 25400769451540 / Basic、Advanced及子项 | 文中明确V4.1 Style；More Settings或再点主题细调，可复制设定到其他主题。静态图见Copy to、For this book only、Cancel/OK。 | 示例Bold选中不是默认；没有按OK或Cancel，不证明撤销或全库批量即时生效。 |
| 10701491126548 / Standard、Handwriting | 浮动栏General与Handwriting设置分开，可增删按钮/看说明；手写栏在手写状态出现。 | 与系统导航球混同、当前预设按钮全部适用本机、关闭后笔迹自动保存。 |
| 10701454503572 / 两组步骤 | Library右上统计入口；Settings/Library settings开关。正文明确统计仅此设备，不能跨设备同步。 | 阅读时间等于理解/完成；停用会清历史；阅读进度同步自动包含统计。 |
| 10701578837268 / Page Manager、Layers、Lasso、More等 | 页浏览与全屏页管理不同；图层正文称最多新增5层。Erase All Layers保留模板层；Page-明确不可撤销。当前套索有跨层Select All/Move to Layer；Notes链接可指向页并查历史。 | 当前新增套索/模板/页导入等全部已在V4.1.2；图层锁图标效果；把Page-不可撤销扩大到所有删除；套索跨层选择包含隐藏/锁定层与否。 |
| 15196391289108 / Text Notes工具 | 文中称V3.3.2引入。已有文本笔记可改样式/列表、查找替换、插链接/图/附件；More/Save；导出TXT/.note。录音固定在画布，可回放/删/导出分享。 | 新建首屏、导出目标/覆盖/取消、.note回导或录音与文字同步定位；其格式不同于手写PNG/PDF/.note。 |
| 10701570125588 / Add、Search、Management | 套索选笔迹→Tag→识别建议名/手输/最近标签→OK；Sidebar/Search查看本笔记标签并跳转；Notes列表Options/Tag Management管设备标签。 | 删标签同时删笔迹、跨本/云同步、识别错误回滚或无限标签数。 |

## 静态观察边界

- library-scan.jpg：菜单实际可读New Bookshelf、Manage Library、Filter and Sort、List Mode、Scan Book Covers、Library Settings、Sync Settings；这只是打开菜单的一帧。
- notes-page-manager.png：实际是Outline视图，12条示例条目、页号、三点菜单；未显示执行Copy/Move/Delete的结果。
- notes-layers.png：新增、眼睛、锁、垃圾桶和普通层/模板层可见；正文未说明锁行为，眼睛/垃圾桶也没有操作。
- epub-more-settings.png：Copy to对话框及上述选项；当前选择和数字值仅示例。

四图均为独立静态帮助图片，不组装成连续流程。完整来源、URL、定位、散列和实际查看记录在本批E及observed-images.json；功能台账保留旧59篇文章，并另列本轮正文标题/控件，父子项目不是新任务数量。
