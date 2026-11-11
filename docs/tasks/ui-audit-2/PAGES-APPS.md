# 应用逐页审计

证据、严重度、C/R 编号的含义沿用 [PAGES](PAGES.md)。截图只证明导出画面；源码说明的时序没有当作真机实测。

## 时钟

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| L01 闹钟列表 Clock | 【实现·图】`i18n-apps_apps_zh_clean-Clock.png`：四分段、下一个闹钟、列表、添加 | 基本结构熟悉；下一个闹钟与第一行重复而没突出名称，R10 Clock | 低 | C02行内时间大字、名称和重复小字；下一闹钟摘要只在有帮助时显示 |
| L02 闹钟空态 | 【实现·图】`i18n-apps_apps_zh_clean-Clock-empty.png` | “下一个闹钟：还没有闹钟”与空提示重复，R01 | 低 | C11保留一次空说明和一个“加闹钟”主动作 |
| L03 新建 ClockAlarm | 【实现·图】`i18n-apps_apps_zh_clean-ClockAlarm-new.png` | 时±分±要逐次点；预设和星期同时显示导致重复模式难懂，R10 | 高 | C13直接设置时分；重复用C02“只响一次／每天／自选星期”，选后回表单；C09保存 |
| L04 编辑 ClockAlarm | 【实现·图】`i18n-apps_apps_zh_clean-ClockAlarm.png` | “关闭”和“删除闹钟”分别在星期网格、底栏，容易把停用和删除混淆，R10 | 中 | 启用独立C08；星期只管重复；删除另组写清对象，C09保存 |
| L05 闹钟名称错误 | 【实现·码】`system/apps/clock/clock.c:474–503` | 保存失败只回一段提示，需保留时间、星期和名称；R02表单错误 | 中 | 错误31/43放名称下或保存上，C03；保留草稿、不跳回列表，输入尺寸交已有任务 |
| L06 ClockRing | 【实现·图】`ui-scene-ClockRing_clean-scene.png`：再睡10分钟／这次不再响 | 文字清楚、按钮等宽；“按电源键再睡10分钟”说明明确；R10 | 低 | 保持两动作，C01/C09；大字号按说明高度上移，不改已定实体键语义 |
| L07 ClockTimer 设定 | 【实现·图】`i18n-apps_apps_zh_clean-ClockTimer.png`；【实现·码】`clock.c:274–299` | 只有1/5/15/30分钟，不能设45秒；R10计时器有时分秒输入 | 高 | C13时／分／秒及预设；底栏只有开始和取消；复用现有 `clock.timer_set(seconds)`，不新增服务 |
| L08 ClockTimer 运行／暂停 | 【实现·码】`system/apps/clock/clock.c:228–239,274–299`：向上取整分钟、按分钟调度，按钮只移除点击标记 | 少于1分钟仍显示1分钟，不能判断剩余秒；运行时仍看到无效开始，R10 | 高 | C13 `mm:ss`，运行给暂停／取消、暂停给继续／取消；秒级更新局限数字区域，频率与功耗待复核 |
| L09 ClockTimer 结束／重启 | 【实现·码】`system/apps/clock/clock.c:251–273,281` | 重启提示和正常计时说明混杂；结束与手动取消应分清，R10 | 中 | C11就地“时间到／已取消”，可再来一次；不把重启后的取消写成仍在计时 |
| L10 ClockStopwatch 初始／计次 | 【实现·图】`i18n-apps_apps_zh_clean-ClockStopwatch.png`；【实现·码】`clock.c:371–390` | 未开始仍给计次和清零外观；显示十分之一秒但运行每秒刷新，R10秒表 | 中 | C01按状态显示开始，运行后暂停和计次；C02计次序号＋时长；秒刷新显示秒，暂停时可显示精度，不假装连续十分之一秒 |
| L11 ClockWorld 有城市 | 【实现·图】`i18n-apps_apps_zh_clean-ClockWorld.png` | 列表只有小时间，删除每行按钮太抢眼；R10世界时钟 | 中 | C02城市/当地时间/日期差，删除移到编辑；列表保持当前页面和页码 |
| L12 ClockWorld 空 | 【实现·图】`i18n-apps_apps_zh_clean-ClockWorld-empty.png` | 添加入口已有；“最多选6个城市”比空态更突出，R01 | 低 | C11先说如何加，限制放选择页脚 |
| L13 ClockWorld 选择 | 【实现·图】`i18n-apps_apps_zh_clean-ClockWorld-pick.png` | 每个候选都一个加入，选好了另一步；无选中摘要，R04/R10 | 中 | C02勾选后原位改“已加入”，底栏“完成”；复用已有时区列表，无新城市检索机制 |

## 日历和提醒

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| D01 Upcoming 接下来 | 【实现·图】`ui-borders_07_clean-app-upcoming.png`；【实现·码】`system/apps/calendar/calendar.c:95–163` | 长标题和右上“看月历”争空间；R20 Calendar 列表标题和切换有清楚区域 | 中 | C10短标题“接下来”，看月历C07次入口；C02按日期分组，新增C01 |
| D02 Upcoming 空／时间未设 | 【实现·码】`system/apps/calendar/calendar.c:147–162` | “请先设置时间”没有同屏直达设置；R01失败要有下一步 | 中 | C11已有 `shell.time` 入口，空清单直接加一件；二者不混为无事项 |
| D03 Calendar 月历／选日 | 【实现·码】`system/apps/calendar/calendar.c:106–146` | 七列小格需要兼顾可点日期与农历、下方事项；截图缺失，不能称布局实测通过；R20 | 中 | C13选中日期黑底、今天描边、事项小点；月切换C01，保留所选日期；日格约87px，小于9mm的限制写明并补真实点选 |
| D04 ReminderEdit 新建／编辑 | 【实现·码】`system/apps/calendar/calendar.c:243–288` | 标题、日期、结束、响铃、全天、四种重复全挤一屏；“清除”对象不明，R20/R02 | 中 | C03内容，C02时间与重复，C08全天/响铃；“清除结束时间”；低频项到已有选择页，C09保存 |
| D05 ReminderTime 起始／结束 | 【实现·码】`system/apps/calendar/calendar.c:258–262` | 年月日时分只±，选一个近月日期也多次点击，R10/R20 | 中 | C13按字段选/输入，页头写“开始时间／结束时间”；选好回原表单，保留另一端 |
| D06 提醒错误／取消 | 【实现·码】`system/apps/calendar/calendar.c:282–288,315–354` | 错误提示区域在重复块后，需确认大字号不被底栏遮挡，R02 | 中 | C03错误随内容增高，C09底栏固定；取消与删除语义分开，仍走原能力 |

## 备忘

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| N01 Memo 列表 | 【实现·图】`i18n-apps_memo_zh_clean-Memo.png` | 标题摘要有但没有时间或录音类型提示；R18 Notes 用摘要和日期帮助找回 | 中 | C02标题／首行＋修改时间、录音图标，两个底部动作同高C01 |
| N02 Memo 空 | 【实现·图】`i18n-apps_memo_zh_clean-Memo-empty.png` | “还没有备忘，写一条吧”却主按钮“说一句”，R01下一步文案不一致 | 低 | C11“说一句或写一条”，保留离线录音入口 |
| N03 MemoDetail 文本／录音 | 【实现·图】`memo-record_main_clean-MemoDetail.png` | 编辑／删除／转成清单三按钮挤在一起；录音与转成文字的结果关系不明，R18 | 中 | C02音频和正文分组，C09编辑主动作，删除/转清单收少用操作，间距19 |
| N04 MemoEdit | 【实现·图】`i18n-apps_memo_zh_clean-MemoEdit-back-zh.png` | 自动保存符合规范，但输入区、保存栏和分页挤在同一边；R02长文 | 中 | C04统一长文，保存状态留一行，页码不覆盖正文；输入规格归已有任务 |
| N05 MemoRecord | 【实现·图】`memo-record_main_clean-MemoRecord.png` | “正在记”可辨，取消叫“不要了”可误解已有录音会被删除；R18 Voice Memos | 低 | C01“取消这次／记好了”，说明本次录音；不改已定分钟时长显示 |
| N06 MemoPending | 【实现·图】`memo-record_main_clean-MemoPending.png` | 已保存且能回听成立；“连上网后”没说需要家庭服务器，R09真实处理位置 | 中 | C11说明“录音已在设备上，连上家庭服务器后整理”，保留返回/回听，不承诺必然成功 |
| N07 MemoChecklist | 【实现·图】`i18n-apps_memo_zh_clean-MemoChecklist.png` | 圈选可理解，但无已完成数和清单标题，R18 Reminders清单 | 低 | C02整行可切换完成，已完勾、必要时划线；页头或说明行给完成数 |
| N08 MemoUndo | 【实现·图】`i18n-apps_memo_zh_clean-MemoUndo.png` | 已删一条和撤销明确，符合非破坏性撤销模式，R01 | 低 | 保持现有撤销能力，C09和新建底栏留29，不新增确认卡 |
| N09 MemoSaveFail | 【实现·图】`i18n-apps_memo_zh_clean-MemoSaveFail.png`；【实现·码】`memo.c:385–407`、`textfield.c:434–439` | 页上“这条没存上”，框底却“已存在这台设备”；草稿保存与备忘入库是两层，用户不能区分，R09 | 高 | 明确“草稿在设备上，尚未存入备忘”；统一这一处状态呈现，失败才给重试，不能简单删掉真实草稿保护 |
| N10 提取提醒／写短信草稿 | 【实现·码】`system/apps/memo/memo.c:383–434`；规范 §7.6 | 当前详情主要编辑/删除/清单，未见设计要求的逐件提醒建议UI；R18 Notes/Reminders按对象行动 | 高 | 先标功能缺口；未来按真实提取结果逐件展示、加入后原位替换，不让UI编时间或直接发送短信 |

## 文件与选取器

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| F01 Files 根页 | 【实现·图】`ui-borders_10_clean-app-files.png` | 家庭／我的／孩子的文件平级；成人可见不代表孩子共享，R16 Files按位置分组 | 低 | C02位置标题及当前家人；传入与连电脑C09分组，不能改成员可见规则 |
| F02 FileList | 【实现·码】`system/apps/files/files.c:154–192,350–374` | 点文件进入“操作”，不是先打开内容；信息只显示原始字节，R16 Files常用打开优先 | 中 | C02文件名＋简短类型/易读大小；可打开文件直接打开，更多才操作；不支持的仍给信息页 |
| F03 FileList 空／等／错 | 【实现·码】`system/apps/files/files.c:195–228,337–374` | 读取失败清空列表，用户容易当文件没了；R16 | 中 | C11区分空与读失败，保留目录和手动重读；不创建新缓存机制 |
| F04 FileActions／改名 | 【实现·码】`system/apps/files/files.c:243–266` | 一打开就给可编辑文件名及多种操作，打开、预览、改名混在一页，R16 | 中 | C02先展示文件信息和打开，改名单独进入同页编辑态C03；保存／取消C09；统一输入归已有任务 |
| F05 FileMove／拷贝目标 | 【实现·码】`system/apps/files/files.c:341–348` | 有完整目录路径、放到这里；缺来源摘要，移动与拷贝易混，R16 | 中 | C10“移动××／拷贝××”，C02目标路径用用户可读位置；C09确认写目的地名 |
| F06 多选／批量结果 | 【实现·码】`system/apps/files/files.c:186–190,350–363` | 下划线表示选中；多选按钮五项分两行，R16通常勾选＋数量 | 中 | C02勾选和已选数，C09只列当前可执行动作；部分失败逐项结果保留，不写笼统成功 |
| F07 FileName 新建文件／文件夹 | 【实现·码】`system/apps/files/files.c:327–334` | 一个无外标签输入，文本默认带.txt，R02/R16 | 中 | C03外标签“名称”，后缀不可误删提示；C09创建，不新增文件格式 |
| F08 FileEdit 文本编辑／保存错误 | 【实现·码】`system/apps/files/files.c:90–145,336,389` | 复用长文草稿，但草稿保存不等于写回原文件；R02/R09 | 中 | C04同一保存状态层次，明确“草稿已存／文件已更新”，失败保留文字；需补状态截图 |
| F09 FilePicker 图片／大字号／孩子 | 【实现·图】`ui-file-picker_clean-scene.png`、`ui-file-picker_clean-large-text.png`、`ui-file-picker_clean-child-text.png` | 选图只有相机图标和文件名，没有缩略图，R11 Photos选图以图识别 | 中 | C02用已有图像缩略能力提供缩略图，仍一次选择回调用页；不复制照片 |
| F10 FilePicker 空／不可读／取消 | 【实现·码】`system/ui/file_picker.c:66–174` | 选取器要区分没有匹配与读取失败，返回须保留调用页位置；现有前后图支持位置恢复 | 低 | C11分开状态；保留当前会话、成员限制及恢复机制，不新建导航层 |

## 相册、相机和扫码

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| G01 Gallery 网格 | 【实现·图】`i18n-apps_apps_zh_clean-Gallery-back-zh.png` | 缩略图可辨，但没有时间分组/选中反馈线索，R11 Photos | 中 | 保留三列静态分页，日期作小标题，按压边框反馈；不增加连续滚动 |
| G02 Gallery 空 | 【实现·图】`i18n-apps_apps_zh_clean-Gallery-empty.png` | 右上拍照入口已有，说明正确，R11 | 低 | C11保留明确拍照动作，页码1/1不画无效箭头 |
| G03 Gallery 读取／失败 | 【实现·码】`system/apps/gallery/gallery.c:76–125` | 错误要求返回后重试，多走一层，R01 | 中 | C11同页手动“重读照片”，不自动重试；保留已有媒体执行器 |
| G04 Photo 查看 | 【实现·图】`i18n-apps_apps_zh_clean-Photo.png` | 最显眼是“问这张图”，无前后照片操作；R11 Photos首先浏览图 | 中 | C10保留返回，图内不遮盖；前后图用已有分页语义，AI动作移次入口，新增翻图能力须按R-061同步 |
| G05 Photo 删除／问图 | 【实现·码】`system/apps/gallery/gallery.c:145–174,204–234` | 图像删除不能找回需保留D-105确认；问图需显示当前对象，R09/R11 | 中 | C01动作名带当前图，执行回执原位；AI只读当前成员授权图片，不能从锁屏相机访问旧图 |
| V01 Camera 取景 | 【实现·图】`i18n-apps_apps_zh_clean-Camera.png`；【实现·码】`camera.c:102–106,135` | 预览间隔固定3000ms，取景和快门无法及时对应移动目标；R15 Camera连续可见取景 | 高 | 优先复核现有输出能力及原厂实现后调取景节奏；只刷取景矩形、不加新线程；具体fps不能由这次截图决定 |
| V02 Camera 拍摄／保存失败 | 【实现·码】`system/apps/camera/camera.c:27–42,99,122–134` | 保存结果是取景下方一行，拍摄后相册图标没有最近照片提示，R15 | 中 | 快门C01按下/忙/完成原位，成功更新小缩略图，失败明确“没保存”；禁止把取景更新当拍照成功 |
| V03 Camera 锁屏受限 | 【实现·图】`i18n-apps_apps_zh_clean-Camera-locked.png` | 本次0张却仍画相册图标，容易误解可看旧照片，R11/R15 | 中 | 明写“本次拍摄”，0张时禁用回看；保持无扫码、无AI和旧相册入口，C01/C11 |
| V04 Camera 锁屏回看 | 【实现·图】`i18n-apps_apps_zh_clean-Camera-locked-photo.png` | 相册图标和本次序号关联不明显，R11 | 中 | 顶部小字“本次第1/1张”，返回明确“继续拍”；同页前后本次照片，不扩大权限 |
| V05 Camera 不可用 | 【实现·码】`system/apps/camera/camera.c:110–117` | 无设备只大字和关闭，未说明可返回原任务，R01 | 低 | C11原因＋返回，C10一致页头；不假装有图像 |
| V06 Scan 未识别／重扫 | 【实现·图】`i18n-apps_apps_zh_clean-Scan.png`；【实现·码】`scan.c:82–96` | 先点扫码再单次识别，失败看不到目标取景框；R15扫码通常在取景中对准 | 高 | 在现有相机取景内给静态识别框和手动识别，失败保留取景，C11；不擅自新增持续识别任务 |
| V07 Scan 网址／文字 | 【实现·码】`system/apps/camera/scan.c:82–96` | URL提示“设备没有浏览器”，系统已有网页应用；结果区还启用了连续滚动，R17/R16 | 高 | 用C04分页结果，网址主动作调用现有远程网页或如实说明限制，复制次动作；新原子导航需明确登记 |
| V08 Scan Wi-Fi／联系人／电话动作 | 【实现·码】`system/apps/camera/scan.c:43–80,163–177` | “是否执行下面操作”一概问句，动作对象应更具体；R08/R09 | 中 | C02展示网络名、联系人或号码，动作写“连接××／保存联系人／拨打××”；Wi-Fi凭据不进AI，对外动作按D-105区分来源 |

## 天气与计算器

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| W01 Weather 正常 | 【实现·图】`weather_clean-WeatherPage.png` | 温度/降雨/未来3天清楚，数据更新时间与来源已有，R01可扫读原则 | 低 | 保持现有卡；刷新是次动作，减少大黑按钮的权重，C01/C02 |
| W02 WeatherOffline 旧数据 | 【实现·图】`weather_clean-WeatherOffline.png` | 已诚实显示无网和更新时间，但仍有高权重刷新，R01状态与动作一致 | 中 | C11“显示上次天气”，刷新手动次动作；不抹掉旧数据或新增自动重试 |
| W03 WeatherEmpty | 【实现·图】`weather_clean-WeatherEmpty.png` | 一步选城市已合理，R01 | 低 | 保留C11＋C01；不自动读位置 |
| W04 WeatherCity 初始／输入 | 【实现·图】`weather_clean-WeatherCityEmpty.png`、`weather_clean-WeatherCityTyping.png` | 键盘搜索键正确，但无清除，候选条挤压；R02搜索控件 | 中 | C05并复用C12，已选城市保持一行；输入组件交已有任务 |
| W05 WeatherCity 结果／无结果 | 【实现·图】`weather_clean-WeatherCity.png`；【实现·码】`system/apps/weather/weather.c:176–245` | 同名市有省国家可区分；失败/无结果应保留原查询，R02 | 低 | C02选城市，C11分无结果和网络错；点选后直接天气，不多成功页 |
| W06 WeatherToday 卡片 | 【实现·图】`weather_clean-WeatherToday.png` | 紧凑显示成立；卡片少更新时间，R01 | 低 | C02加简短“上次更新”或离线标记，不扩大卡片占屏 |
| Q01 Calculator 初始／运算 | 【实现·图】`calculator_clean-CalcEmpty.png`、`calculator_clean-CalcResult.png` | 四列规则，黑底等号明确；按钮字较大但运算符和数字同权重，R01 | 低 | 保留熟悉四列，C12数码规格；用字重区分运算，不引入新模式 |
| Q02 Calculator 长式 | 【实现·图】`calculator_clean-CalcLong.png` | 长算式跨两行不易定位光标/可否继续编辑；R02 | 中 | C04算式区域按完整行留高，结果独立，允许现有删除逐字修；不新加计算历史机制 |
| Q03 Calculator 错误 | 【实现·图】`calculator_clean-CalcDivZero.png`、`calculator_clean-CalcRange.png` | 错误清楚但替换了结果，需明确怎么纠正，R01 | 低 | 保留算式，C11加简短“修改算式再计算”，不清空用户输入 |

## 阅读

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| B01 书架 | 【实现·图】`reader_basic-metalio-plus_clean-library.png`；【实现·码】`apps/reader/reader.c:166–196` | 与系统页头/列表不同，只有书名，缺最近进度和可见分页；R05 BOOX书库有最近、列表和组织 | 高 | C10页头、C02书名＋最近进度，静态分页；点击直接打开，优先保证读过的书可找到 |
| B02 空书架 | 【实现·码】`apps/reader/reader.c:187–192` | 提示把.txt放进 `/content/books`，向用户暴露工程路径，R05读书应按书和导入任务 | 高 | C11“还没有书”，主动作选现有传入／选书入口；不让用户输入目录 |
| B03 阅读页翻页 | 【实现·图】`reader_basic-metalio-plus_clean-page1.png`、`reader_basic-metalio-plus_clean-page13.png`；【实现·码】`reader.c:472–479` | 中间点击也翻下一页，缺阅读工具入口；R05 BOOX点中央出菜单，Kindle点顶部出工具栏，两者不同 | 高 | 可逆建议采用BOOX中央工具入口，左右翻页保留，AI不移动原文；正文边距按阅读令牌58，需保留现有分页位置 |
| B04 阅读设置 | 【实现·码】`apps/reader/reader.c:520–535,439–449` | 正文字体/边距/行距固定，未见可调用的阅读设置能力或页面；R05 BOOX可调字号和排版 | 高 | 明确功能缺口；先设计C13字号/行距/边距，按钮与AI共用能力，不能拿系统字号设置冒充已支持 |
| B05 打开失败 | 【实现·图】`reader_empty-metalio-plus_clean-cannot-open.png`；【实现·码】`reader.c:520–524` | 只有英文“cannot open book”，无返回/可行动原因，R01/R05 | 高 | C10＋C11中文错误，返回书架，格式不支持/文件不可读按真实回执分清 |
| B06 继续阅读 | 【实现·图】`reader_basic-metalio-plus_clean-home.png`；【实现·码】`reader.c:216–232,642–648` | 可从主屏一步续读已成立；书名长时需保住页码，R05最近阅读 | 低 | 保留现有 `reader.continue`，C02标题缩略；不改已定主屏时钟占比 |

## 远程网页

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| E01 Remote 浏览 | 【实现·图】`i18n-apps_apps_zh_clean-Remote.png` | 内容几乎全屏，底部“网址”才看域名；用户难辨当前来源，R17 Safari地址可识别 | 高 | 常驻紧凑域名条C10，翻页箭头标明是网页翻动还是历史返回，保留服务器渲染边界 |
| E02 RemoteAddress | 【实现·图】`i18n-apps_apps_zh_clean-RemoteAddress.png` | 页头大胶囊也像输入框，真实输入在下方；回车只叫完成，R02/R17 | 中 | C03 URL框＋“打开”键，标签明确；尾侧清除，输入规格交已有任务 |
| E03 RemoteType 普通／密码 | 【实现·图】`i18n-apps_apps_zh_clean-RemoteType.png`；【实现·码】`remote.c:739–752` | 全屏原生输入看不到网页原字段的标签和对象；R02 | 高 | C03/C06显示“向example.com的××字段输入”，提交前可见目标，秘密不进AI；原流程仍打完才发 |
| E04 RemoteLoading | 【实现·图】`i18n-apps_apps_zh_clean-RemoteLoading.png` | 有正在打开和关闭，尚可退出；具体等待时限不由静态图证明，R01 | 低 | C11保留域名和取消，超时才手动重试；不承诺1.5秒实测 |
| E05 RemoteOffline | 【实现·图】`i18n-apps_apps_zh_clean-RemoteOffline.png` | 5分钟可恢复已有说明，重连后落点需保留，R17页面状态 | 低 | C11明确重连恢复与重新打开的区别，保留现有会话期限，不新增缓存 |
| E06 RemoteEnded | 【实现·图】`i18n-apps_apps_zh_clean-RemoteEnded.png` | 文案说重新打开，按钮却“重新连接”，会话已失效也暗示能恢复，R01 | 中 | C01改“重新打开”，明确可能回不到原页面；不自动重放表单 |
| E07 RemoteBlocked | 【实现·图】`i18n-apps_apps_zh_clean-RemoteBlocked.png` | 原因和换地址明确；同样需要返回与域名的可靠识别，R17/R09 | 低 | C11保留原因，域名C10；孩子限制不显示可绕过按钮 |
