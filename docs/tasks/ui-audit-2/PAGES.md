# 逐页及状态审计

基线与证据边界见 [REPORT](REPORT.md)。所有截图名均相对于 `$INK_DESIGN/shots-20261111/`，不是本次运行产物；【实现·图】为本次实际阅图，【实现·码】为基线源码核对。【估算】只用于尺寸、步数及改法；没有设备实测。严重度：高＝现有路径做不了或明显别扭；中＝能完成但辨识、效率或一致性有明显问题；低＝局部改善或本样本未见阻断。每行都给出具体参照和改法；C 编号对应 [统一组件规格](COMPONENTS.md)，R 编号对应后续的 [参照来源](REFERENCES.md)。

## 外壳、锁屏和成员门

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| S01 主屏 Main | 【实现·图】`ui-scene-Main_clean-scene.png`：大时钟、继续读、两行应用、固定 Dock | 已定 D-106 保留大时钟；问题是空书“还没有读到一半的书”没有告诉怎么开始，R05 书库以书为对象 | 中 | 保留整体布局；继续读空态写“选一本书”，同位置进书架，C01/C11；有进度仍一步续读 |
| S02 主屏大字号 | 【实现·图】`ui-scene-big-Main_clean-scene.png`：继续读底边和第一排图标贴靠 | R01 按钮需留可分辨的间隔，放大不能挤掉布局间距 | 中 | 按实际行高重新算网格顶部，至少留 C02 的14间隙；必要时应用网格分页，Dock不变 |
| S03 今日 Today | 【实现·图】`ui-scene-Today_clean-scene.png`：五卡全为空，继续读黑块最显眼 | R05 继续阅读应指向可读内容；空卡缺下一步 | 中 | 空卡写“选书／选要听的／加一件”，各调用对应现有能力；C11，无成功中转页 |
| S04 今日编辑 TodayEdit | 【实现·码】`system/ui/shell/navigation.c:2133–2181`：点击列表项切换卡片，页脚另有完成 | R04 设置应看出当前选项，不能靠记住已点哪些 | 中 | C02 行尾勾选、显示已选数及上限；结束回今日，选卡不超过两阶段 |
| S05 全部应用 Apps | 【实现·图】`ui-scene-Apps_clean-scene.png`；`calculator-apps20_clean-AppsSecond.png` | 分类清楚但联系人没有独立入口；大字号第一页工具消失需翻页，分组延续不够清楚 | 高 | 联系人入口交 `comm-ux-2`；其余保留 C02 网格及固定分组顺序，显示“工具（续）”，搜索 C05 |
| S06 图标长按 IconMenu | 【实现·码】`system/ui/shell/navigation.c:2192–2218`：打开／放到今日屏面板 | iOS 主屏快捷菜单使用对象名和具体动词，不能让“接着做”不知接着什么，R01 | 低 | C10 写应用／书名，C01“打开××／放到今日屏”；被限制应用无菜单 |
| S07 控制中心 Control | 【实现·图】`ui-scene-Control_clean-scene.png`：滑条、勿扰／叫名字／更多、网络、底部两出口 | 叫名字呈黑按钮但无“开”字，和网络导航胶囊混在一起；R04 开关和导航不同 | 中 | 切换项 C08 或明确“叫名字：开”；更多仍原位展开，网络用 C02，保留一页常用项 |
| S08 控制中心更多 | 【实现·码】`system/ui/shell/navigation.c:1088–1102`：插入暖光和飞行，下面通知下移 | R04 同类设置应稳定，展开后底部内容可能从视野消失 | 中 | 明确“更多设置”小节，计算可用高度；低频项分页而非覆盖底部；C07/C09 |
| S09 通知 Notifications | 【实现·图】`ui-scene-Notifications_clean-scene.png`：摘要、每条删除、全部已读／清除 | 未读和已读主要看状态，卡中无时间；R06 通知需可辨来源和时间 | 中 | C02 标题、摘要、时间和黑点；删除独立热区；普通清除直接做，C09 |
| S10 通知空态 | 【实现·码】`system/ui/shell/navigation.c:1531–1615`：列表分页和页脚动作 | 空列表仍应避免“全部清除”等可用外观，R01 | 低 | C11“还没有通知”，无可操作记录时隐藏／禁用批量动作，保留返回 |
| S11 锁屏保留页 LockPage | 【实现·图】`ui-scene-LockPage_clean-scene.png`：正文底部遮条，紧急呼叫与相机外观可点 | 规范锁定时触摸无效；R01 可点外观与实际响应矛盾 | 中 | 保留电源／AI键唤醒规则；先写清“按键唤醒后可拍照／紧急呼叫”，未唤醒热区不画按钮外观；C11 |
| S12 时钟锁屏 Lock | 【实现·图】`ui-scene-Lock_clean-scene.png`：日期时钟、0条通知胶囊 | 0条仍像可点通知；R06 空态不应伪装可操作对象 | 低 | 0条用普通文字；唤醒后的紧急入口保留，C02/C11 |
| S13 壁纸锁屏 LockImage | 【实现·码】`system/ui/shell/wallpaper.c:1–67`、`settings_family.c:195–215` | 图像可能削弱锁定／唤醒提示的对比，不能凭无图说已合格，R01 | 中 | 固定白底状态条，字31、边3，C11；补深浅壁纸截图核对，仍不让锁定时直接操作 |
| S14 正在播放 MainPlaying | 【实现·图】`activity-card_clean-MainPlaying.png`：书名和暂停替代继续读 | R07 媒体卡应直接暂停和回内容；本图成立 | 低 | 保持原位，标题长时C02截断；暂停热区C01，不新增面板 |
| S15 播放中的控制中心 | 【实现·图】`activity-card_clean-ControlPlaying.png` | 媒体主动作可辨；下方按钮较密，R01 | 低 | 维持共用活动卡，和其余行留14；C02/C09 |
| S16 播放锁屏 LockPlaying | 【实现·图】`activity-card_clean-LockPlaying.png`：画着暂停、Dock仍露出 | 需唤醒才能暂停，暂停外观容易误导；与 S11 同一根因 | 中 | 锁定阶段用状态图标代替可按外观；唤醒后复用原活动卡；C11 |
| S17 通话返回条 MainInCall | 【实现·图】`activity-card_clean-MainInCall.png`：黑色回到通话条 | 长号码挤压“回到通话”，R01 要保住动作名称 | 低 | 对方名省略，动作单独固定宽；整条调用同一返回能力，C02 |
| S18 正在做的事 Tasks | 【实现·图】`activity-card_clean-Tasks.png`：等网络、准备、完成列表 | “等网络”项无就地说明，“完成”与进行中相隔较大，R01 等待需告知还能做什么 | 低 | C02 两组连续列表，保持任务名和阶段，取消仅对可取消项开放；不加后台重试 |
| S19 选人 Members | 【实现·图】`ui-scene-Members_clean-scene.png`：人头像，底部“设置或修改密码” | 主任务是选谁使用，却把设置放大；规范要求“加一位家人”一行 | 中 | 主区只选人，管理入口用C02文字行；密码设置归家人页，不额外设确认卡 |
| S20 大人回来 MembersBack | 【实现·图】`ui-scene-MembersBack_clean-scene.png`：上次成员大头像，说明回刚才的地方 | 不显示恢复目的地；规范 §7.2 要写清回到哪儿 | 低 | 用真实安全摘要“继续阅读”而不泄露正文；头像按C01，另一人列表C02 |
| S21 密码 Pin | 【实现·图】`ui-scene-Pin_clean-scene.png`；【实现·码】`member_pages.c:152–176` | “确定”在键盘上方；四空圈与4～8位规则不匹配，R02 数量和输入应一致 | 中 | C06显示4～8位提示，C12稳定数字网格；确认与取消同一底栏，禁止AI代输 |
| S22 密码错误 PinWrong | 【实现·图】`ui-scene-PinWrong_clean-scene.png`：原位错误、忘记密码 | 反馈明确；仍受 S21 布局影响，R02 | 低 | 保留就地错误及原键位；错误说明31/43，不让表单跳位 |
| S23 等待 PinWait | 【实现·码】`system/ui/shell/member_pages.c:35–40,71–76,164–181` | 等待说明存在，但键盘和继续操作须明确不可用，R02 | 中 | C06写“还需等待×秒”，不可用键不绑动作；无需整屏倒数刷新，重试时才恢复键盘 |
| S24 设置／修改密码 PinSet | 【实现·码】`system/ui/shell/member_pages.c:158–181,190–245` | 原密码、新密码与再次输入需清楚区分；R02 持续字段标签 | 中 | C06依次写明原因和阶段，保留固定数字键位；不展示给AI |
| S25 忘记密码 PinForgot | 【实现·码】`system/ui/shell/member_pages.c:140–150`：请家长验证 | 规范说手机上重置，实现是本机家长验证；大人本人无另一个家长时恢复路径需核对 | 高 | 如实写当前可用方法与所需角色；无可用验证者时提供已有家庭服务器重置说明；不自定新认证机制 |
| S26 换人 MemberSwitch | 【实现·码】`system/ui/shell/member_pages.c:128–135`：通用换人说明 | 规范只在有进行中任务时说明“收好了什么”；当前文案不列实际对象 | 中 | 使用现有任务结果填真实“暂停了××／草稿已保存”；无需任务时直接换人，遵守D-105 |
| S27 电源菜单 PowerMenu | 【实现·图】`ui-scene-PowerMenu_clean-scene.png`：关机重启锁屏＋两个日志动作 | 日志按钮边框相贴，诊断和日常电源混为一组；R01 空间和主次不清 | 中 | 电源三项保留，诊断另组留29，按钮间19；日志导出显示目的地和结果，C01/C09 |
| S28 关机 PowerOff | 【实现·图】`ui-scene-PowerOff_clean-scene.png`：已关机、开机提示、阅读位置 | 语义正确；长书名大字号会换行，R05 位置需可读 | 低 | C11保留白底“已关机”，书名允许两行并预留高度 |
| S29 空电关机 PowerOffEmpty | 【实现·码】`system/ui/shell/power_pages.c:90–137` | 普通关机和需先充电要有区别，R01 状态需对应下一步 | 中 | C11主说明“请先充电”，保留不会自动恢复播放／对话的事实，不加可点外观 |
| S30 低电 BatteryLow | 【实现·码】`system/ui/shell/power_pages.c:62–89` | 打断需告知剩余电量和继续方式，R01 | 低 | 保留百分比与“继续用”，C01；不把低电提示加成常驻机制 |
| S31 保存关机 BatterySaving | 【实现·码】`system/ui/shell/power_pages.c:90–137` | 关键是“正在保存”与“已关机”不能混写，R01 | 中 | C11使用真实阶段，保存结束才显示关机；不承诺未验证的剩余时间 |
| S32 充电 Charging | 【实现·码】`system/ui/shell/power_pages.c:138–166` | 状态可保持安静，不能把非触控关机画面画成可操作页 | 低 | 普通文字电量＋充电图标；沿用既有更新节奏，不增加频繁刷新 |

## 设置的每一页及选择状态

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| T01 Settings | 【实现·图】`ui-borders_14_clean-settings-shell.settings.png`：家人黑行＋三组设置 | 大字号值省略到“还没…”、“使用时能…”；R04 设置摘要应可辨 | 中 | C02值改短语，长说明进下页；保持设置分组，不另建功能中心 |
| T02 SetDisplay 首屏 | 【实现·图】`ui-scene-SetDisplay_clean-scene.png` | 亮度、字号、刷新明确；自动锁屏点击即跳下一个时长，R04 选择不应靠循环 | 中 | 亮度保留C08/C13，自动锁屏用C02进入明确单选；不改已定字号范围 |
| T03 SetDisplay 大字号／后页 | 【实现·图】`ui-scene-big-SetDisplay_clean-scene.png`；【实现·码】`navigation.c:1288–1328` | 预览句末单字单独一行、相关设置分到页2，R02 输入与预览要按内容布局 | 中 | C13预览高度按实测行数；自动锁屏／今日屏仍可翻页但页码指示清楚 |
| T04 SetSound 首屏 | 【实现·图】`ui-borders_16_clean-settings-shell.sound.png` | “蓝牙耳机”是跳转、“喇叭”是立即切换却都画箭头；R04 控件语义混用 | 中 | 输出设备单选用勾，进入耳机管理才用箭头，C02；当前声音路由文字保留 |
| T05 SetSound 提示／铃声／通知 | 【实现·码】`navigation.c:1356–1394`：振动强度和铃声点按循环 | R04 iOS Sounds 明示当前铃声和候选，循环会产生意外播放 | 中 | C02打开C13选择，点选才试听；振动强度C07，普通设置不确认 |
| T06 SetNotifications | 【实现·图】`ui-borders_26_clean-settings-shell.notify_settings.png` | 顶部说“电话和闹钟照常”，下方又有电话／闹钟开关；R06 通知类别要区分提醒和功能 | 中 | 文案明确“只关通知记录，不关来电或响铃”，C08行值写清；不改变已有策略 |
| T07 SetNetwork 主屏 | 【实现·图】`ui-scene-SetNetwork_clean-scene.png` | 开关、上网偏好、扫描、设置设备堆在一起；大字号“找附近的 Wi…”截断，R04 | 高 | Wi-Fi保留一组，网络列表紧接其后；设备加入家庭移为次入口，扫描按钮全宽C01；SIM/移动网络独立页 |
| T08 Wi-Fi 扫描／列表／失败 | 【实现·码】`navigation.c:1463–1506`：结果每组2项，在主设置块之后 | 找到网络后仍可能要翻页才能选；R08 Wi-Fi扫描结果在当前列表直接可选 | 高 | 同页C02结果替换占位状态，加载／空／失败C11，已连接置顶；不改连接服务 |
| T09 Wi-Fi 密码 | 【实现·码】`navigation.c:1463–1473`：网络页内插入密码框，按完成连接 | SSID和提交目的不够固定；R02/R08 密码后应明确“加入” | 中 | C06标签“××的密码”，回车“连接”，失败保留目标且让本人重输；输入规格交 `ui-input-2` |
| T10 KnownNetworks 空 | 【实现·图】`ui-borders_18_clean-settings-shell.known_networks.png` | 一句“还没有已存的网络”，没有去选网；R08 | 中 | C11提供“找附近的Wi-Fi”返回网络列表，避免死页 |
| T11 KnownNetworks 有网／错误 | 【实现·码】`navigation.c:1396–1418`：每网另排连接、忘掉两按钮 | 每项额外一行降低密度，错误无直接重读；R08 网络详情管理常藏在单行 | 中 | C02点网连接，尾侧详情内放忘掉；读失败C11重试，不加后台重试 |
| T12 SetMobile 状态 | 【实现·码】`system/ui/shell/mobile_network.c:46–94` | APN、注册状态和运营商查询动作同页，状态与可编辑项需区分，R04 | 中 | 状态C02只读，APN一行“编辑”；查询显示就地等待结果，不把原始状态码当用户文案 |
| T13 SetMobile APN编辑 | 【实现·码】`system/ui/shell/mobile_network.c:58–94` | 多字段缺一致标签和提交层次，R02 | 中 | C03三项持久标签，密码项C06；C09保存／取消，规格交 `ui-input-2` |
| T14 SetBluetooth 空／扫描 | 【实现·图】`ui-borders_19_clean-settings-shell.bluetooth.png` | “搜索耳机”可理解，但发现／已配对／连接中需分组而非只留空白；R04 | 中 | C02两组列表、C11扫描进度；没有设备给一次手动重扫 |
| T15 SetBluetooth 已配对／忘记 | 【实现·码】`system/ui/shell/bluetooth_page.c:84–157` | 忘记功能只靠页尾提示长按发现，R01 隐藏常用操作难找 | 中 | C02尾侧详情入口含“忘记”；不重复增加确认卡，权限仍按现有策略 |
| T16 SetVoice 唤醒／打断 | 【实现·图】`ui-scene-SetVoice_clean-scene.png`：不支持叫名字，顶部仍列使用时／一直／关 | R04 不可用功能仍呈可切换外观；容易误以为设置成功 | 高 | 不支持时保留原因文字并禁用对应控件；支持时C07和C08；无硬件参数建议 |
| T17 SetVoice 其余页 | 【实现·码】`system/ui/shell/settings_pages.c:12–27,248–267`：语音选项、速度等 | 可选声音用点按循环，R04 | 中 | C13明确候选项；试读和音量走既有能力，不新增音频机制 |
| T18 SetStorage | 【实现·图】`ui-borders_21_clean-settings-shell.storage.png` | 多项“正在整理”可误认为同一无期限后台任务，清理动作也像读数；R01 | 中 | C02读数分别显示读取／失败；清理写清对象与可回收量，完成原位替换，不删个人文件 |
| T19 Backup状态入口 | 【实现·码】`system/ui/shell/settings_pages.c:292–314` | 已区分等服务器回执和真正已备份，符合R09的状态可追溯 | 低 | 保留真实状态，失败点入对应页；不要将“等待回执”改成成功 |
| T20 SetAbout 首屏 | 【实现·图】`ui-borders_22_clean-settings-shell.about.png` | 语言点整行立即换下一种，用户无法先看可选项，R04 | 中 | C02语言选择单页，两种语言并列名；原设置只显示当前值 |
| T21 SetAbout 后页 | 【实现·码】`system/ui/shell/settings_pages.c:34–44`：许可、自动下载、清空设备 | 高风险清空应与常规项隔开并说明范围，R01 | 中 | 独立危险组、文字“清空这台设备”；不改重置能力，继续走本人验证和确认 |
| T22 DeviceDetails | 【实现·图】`ui-borders_24_clean-settings-shell.device.png` | 信息齐；型号长串包含工程英文但不是用户任务，R04 About 常按字段清楚展示 | 低 | C02只读行，型号两行换行、容量与单位对齐，不把信息行画成按钮 |
| T23 Licenses | 【实现·图】`ui-borders_25_clean-settings-shell.licenses.png` | 多页列表合理，哈希串比用户需要的版本更显眼，R04 | 低 | C02名称＋版本／许可证，哈希留详情；全文仍可读 |
| T24 LicenseText | 【实现·码】`system/ui/shell/settings_pages.c:391–428` | 长英文按片段分页，需保证明确页码和回列表，R02文本视图 | 低 | 保留C09分页，不连续滚动；前后文按完整行断开 |
| T25 SetPrivacy 第一／后页 | 【实现·图】`ui-scene-SetPrivacy_clean-scene.png`、`ui-scene-big-SetPrivacy_clean-scene.png` | 已诚实披露未知保存期限；孩子记录删除入口在后页，不宜隐藏行动项，R09 | 中 | C02先列可操作“查看记录”，下面分组短文；不能虚构期限或已开放的删除能力 |
| T26 SetTime | 【实现·图】`ui-borders_27_clean-settings-shell.time.png` | 自动时间开启仍露可执行手动设置，自动时区与手动状态混排，R04 | 中 | C08自动项下显示来源/结果；手动入口禁用并说明，或本人进入时明确切为手动 |
| T27 SetTimeEdit | 【实现·图】`ui-borders_28_clean-settings-shell.time_edit.png` | 年月日时分都只能±，设远日期点击很多，R10日期选择 | 高 | C13点字段直接输入／明确选择；保留±微调，选好后一底部主动作，输入组件沿用已有接口 |
| T28 SetTimeZones | 【实现·图】`ui-borders_29_clean-settings-shell.time_zones.png` | 城市跨3页，无搜索且UTC占一个城市行，R10 | 中 | C02城市＋UTC偏移＋勾；有限列表先分地区，不先引入搜索服务；支持现有查询时才用C05 |
| T29 SetReturn | 【实现·图】`ui-borders_30_clean-settings-shell.return.png` | 标题很长、“已选择”孤立在按钮下方，R04 单选列表 | 中 | 页头“重新验证”，正文写原因；C02单选勾，1/2/5/15分钟同一列表 |
| T30 Wallpaper／锁屏样式 | 【实现·图】`ui-borders_35_clean-settings-shell.wallpaper.png`；【实现·码】`settings_family.c:195–215` | 点“锁屏时显示”就轮换，自动锁屏重复入口；无壁纸仍有移除入口，R04 | 中 | C07三种样式，壁纸缩略图＋C02选择；没选时禁用移除，统一自动锁屏单选 |
| T31 Wallpaper 选择照片 | 【实现·码】`system/ui/shell/settings_family.c:195–215`、`wallpaper.c:1–67` | 缺实际裁切／深浅图预览证据，R11 Photos 用预览确认选取对象 | 中 | 借现有FilePicker展示缩略图与名称，选后在原页预览，不新增图像编辑服务 |
| T32 Records | 【实现·图】`ui-borders_36_clean-settings-shell.records.png` | “全部删除”没在按钮里说只删活动记录；“我／孩子”未指明哪个孩子，R09 | 高 | C02显示所选家人名，删除改“删除这位家人的活动记录”，确认页同对象；多孩子选择待主会话定 |
| T33 RuntimeRecords | 【实现·图】`ui-borders_37_clean-settings-shell.runtime_records.png` | 页面明确不能查看或删除，却从列表像正常能力进入；R09不可用需在入口说明 | 中 | C11诚实说明当前不可用和返回；上级标状态，不虚画列表；后端能力缺口另列 |
| T34 AiMemory | 【实现·图】`ui-borders_38_clean-settings-shell.ai_memory.png` | 无法查看和忘掉，和规范 §7.11有差距，R09 | 高 | 入口标不可用；后续接真实记忆能力才提供C02记录和删除，不把空态当完成 |
| T35 家人 SetMembers | 【实现·图】`ui-borders_31_clean-settings-shell.family.png` | 选人、加人和改密码同样卡片；R04 Accounts 按对象及管理分组 | 中 | 家人C02列表，管理另组；切换和编辑分清动词，C10/C09 |
| T36 加家人表单 | 【实现·码】`system/ui/shell/settings_family.c:239–278` | 名称和角色要持续可辨，不能让角色整行循环，R02/R04 | 中 | C03姓名＋C07大人/孩子，C09添加；权限改变照本人验证，输入归已有任务 |
| T37 SetKid 选择孩子／概要 | 【实现·图】`ui-borders_32_clean-settings-shell.kid_settings.png` | 时长和音量上限点击就改、只给数字60，R04 | 高 | C02显示孩子名、每日时长、音量百分比；点入C13选择，不靠循环预设 |
| T38 SetKidApps | 【实现·图】`ui-borders_33_clean-settings-family.kid_apps.png` | 能用/禁用是文字行；电话例外没有显式锁定语义，R04 | 中 | C08开关；电话“始终可用”只读；禁止项不在孩子应用页出现 |
| T39 SetKidPeriods | 【实现·图】`ui-borders_34_clean-settings-family.kid_periods.png`；【实现·码】`settings_family.c:165–191` | 一个时段的开始/结束/启用分多页，编辑还需点按轮换，R10 | 高 | 一页C03/C13开始结束＋C08启用；列出“21:00—次日07:00”避免跨日误解，保存仍走现有能力 |

## 键盘和所有输入状态

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| I01 拼音 ImePinyin | 【实现·图】`ui-scene-ImePinyin_clean-scene.png` | 10键、9键、第三行每行重新均分，第二行字母变宽；R03手机全键盘按同一基本宽 | 高 | C12固定基本宽，第二行居中，语音候选条右侧；归 `ui-input-2` |
| I02 英文／大写 | 【实现·码】`system/ui/shell/keyboard.c:356–384` | 与拼音共用不等宽；中英状态只一个“中／英”键，R03 | 中 | C12一致键位、选中态区分大写和当前语言，英文保留半角；归已有任务 |
| I03 更多候选 ImeCandidates | 【实现·图】`ui-scene-ImeCandidates_clean-scene.png` | 更多／键盘／语音挤一组，下半块大量留白；R03候选应明确翻页／收起 | 中 | 候选按C12连续等高行分页，保留拼音和固定收起，不改变选词能力 |
| I04 编辑工具 ImeEdit | 【实现·图】`ui-scene-ImeEdit_clean-scene.png` | 箭头窄，复制剪切撤销同时排7项，和字母键无对齐，R02/R03 | 中 | C12工具条只放当前适用项，复制／粘贴等保留明确文字；不足则分页，不缩热区 |
| I05 符号 ImeSymbols | 【实现·图】`ui-scene-ImeSymbols_clean-scene.png` | 语音独占一行，切换用“字母”但符号页码无明显位置；R03 | 中 | C12复用候选条高度和右侧语音，明确1/2；数字和标点行宽稳定 |
| I06 密码 ImePassword | 【实现·图】`ui-scene-ImePassword_clean-scene.png` | 掩码正确且无语音，但密码使用大正文框；R02小量输入用单行 | 中 | C06单行框＋C12无候选键盘；不加AI、联想或粘贴入口 |
| I07 数字／电话号码键盘 | 【实现·码】`system/ui/shell/keyboard.c:346–354` | 前三行3键、末行4键，0和删除宽度突变；R03数字键一致 | 高 | C12三列，完成独立底栏；电话含+，普通数字按字段允许小数；归已有任务 |
| I08 ImeVoice 在听 | 【实现·图】`ui-scene-ImeVoice_clean-scene.png` | 两按钮清楚，但录音区信息少，麦克风入口宽125与其他功能键不一致 | 高 | C12缩语音键，录音状态文字就地；不画波形动画；“说完了／用键盘”同高 |
| I09 听写连接中／整理中 | 【实现·码】`system/ui/shell/keyboard.c:399–465` | 状态需和“说完了”可用性同步，R01等待应有退出 | 中 | 同一C12区域，连接／整理时只留适用动作，退出保留已得文字，不承诺即时识别 |
| I10 听写没网／没听清／额度／打断 | 【实现·码】`system/ui/shell/keyboard.c:422–465`；规范 §7.5 | 这几种失败不能都归“没听清”；R01不同原因应有可行动文案 | 中 | C11说明原因，“用键盘”始终同位置；若要重试由本人点，不自动重试，不改既有服务 |
| I11 长文 ImeLong | 【实现·图】`i18n-apps_apps_zh_clean-ImeLong.png` | 页头返回和右侧返回重复，正文页码与键盘分离；R02长文用统一文本视图 | 中 | C04长文编辑，C10仅一个返回；分页和保存状态靠同一底边，归已有任务 |
| I12 AI候选／联想 ImeAi | 【实现·码】`system/ui/shell/keyboard.c:298–345,466–512` | AI候选来源和普通词库需可辨，不能抢正文或在密码出现，R03/R09 | 中 | C12候选有简短来源标记，点选才入正文；错误回普通键盘，不新增建议机制 |
| I13 大字号键盘 | 【实现·图】`ui-scene-big-ImePinyin_clean-scene.png`、`ui-scene-big-ImeEdit_clean-scene.png` | 字变大而键宽不一致更突出；编辑工具条挤压 | 中 | C12只保持十列物理宽例外，字和其他行高按档位统一；工具条分页 |
| I14 孩子键盘 | 【实现·图】`ui-scene-child-ImePinyin_clean-scene.png` | 使用110%字，仍是成人不等宽布局；本图未证明儿童误触率 | 中 | 同C12，儿童可见能力不扩大；9宫格交后续既有计划，不能宣称已实现 |
| I15 单行／搜索宿主 | 【实现·图】`ui-input-host_clean-search-focused.png`、`ui-input-host_clean-url-long-cursor.png` | 聚焦用双边线；URL、搜索和其他短框的尺寸／标签仍不同，R02 | 中 | C03/C05，键盘出现时只移动必要宿主区域，退出回原查询和位置；归已有任务 |
| I16 选择／剪贴板菜单 | 【实现·码】`system/ui/textfield.c:301–350` | 菜单每项固定115宽；长文字和可用动作变化需避开键盘，R02/R03 | 中 | C12编辑条样式，按选择状态启用，密码不显示内容；不新增跨成员剪贴板 |

输入框宿主逐项覆盖见后续 [FORMS](FORMS.md)；这里审的是组件和键盘状态，不能替代每个表单的流程检查。

## 电话、短信、联系人（交已有 comm-ux-2，不重复拆任务）

| 编号／页面状态 | 现状与证据 | 差距及参照 | 严重度 | 建议改法 |
| --- | --- | --- | --- | --- |
| M01 Phone 拨号 | 【实现·图】`i18n-apps_apps_zh_clean-Phone.png` | 三入口已有，但输入无联系人匹配，号码区和大键盘贴靠；R12标准电话支持联系人检索 | 中 | C03电话输入、C12三列数字，输入时匹配；操作与号码规整交已有任务 |
| M02 PhoneRecents 有记录 | 【实现·图】`i18n-apps_apps_zh_clean-PhoneRecents.png` | 仅号码、打出、0分钟和时间，未显示联系人、未接突出和日期，R12 | 中 | C02名字／号码、未接图标、时间；同一联系人正常辨识；点击回拨语义明确 |
| M03 PhoneRecents 空 | 【实现·图】`i18n-apps_apps_zh_clean-PhoneRecents-empty.png` | 空页没有引导拨号或联系人；R12 | 低 | C11“还没有通话”，保留三入口C07 |
| M04 PhoneCall | 【实现·图】`i18n-apps_apps_zh_clean-PhoneCall.png`；【实现·码】`phone.c:371–384` | 只显示号码；“录音”仍像可按但说明接线未核实；R12通话控件须反映可用性 | 高 | 联系人名称和号码分层，录音禁用并说明，路由／静音C08状态；不承诺通话录音可用 |
| M05 PhoneKeypad 通话中 | 【实现·图】`i18n-apps_apps_zh_clean-PhoneKeypad.png` | DTMF数字比拨号盘小，收起键盘与挂断挤靠，R12 | 中 | C12同三列键规格，收起次动作和挂断留19，C09；输入内容保持秘密 |
| M06 PhoneIncoming | 【实现·图】`ui-scene-PhoneIncoming_clean-scene.png` | 接听大黑圆，拒接只小图标＋字；R12应清楚展示两种动作和来电者 | 高 | 两动作热区一致、拒接用描边圆，文字和图标同组；显示联系人名，C01/C09 |
| M07 PhoneEmergency | 【实现·图】`i18n-apps_apps_zh_clean-PhoneEmergency.png` | 快捷号码没有用途名，可能不知110/119/120/112差别；R12 | 中 | 按钮写号码＋简短用途且不挤键盘；不扩充或改变可拨号清单，不实际拨号 |
| M08 Sms 会话列表 | 【实现·图】`i18n-apps_apps_zh_clean-Sms.png` | 号码＋正文，没有联系人名、时间、未读；R13信息会话列表 | 高 | C02名字／号码、最后摘要、时间和未读点，右上新建，C10 |
| M09 Sms 空态 | 【实现·图】`i18n-apps_apps_zh_clean-Sms-empty.png` | 只有空文案和底部写短信，尚可开始但入口与列表不一致 | 低 | C11保留直接新建，常态右上同入口；不要求联系人 |
| M10 SmsRecipients 选收件人 | 【实现·图】`i18n-apps_apps_zh_clean-SmsRecipients.png`；【实现·码】`sms.c:193–206` | 没有任意号码输入；无联系人明确要求“请先在联系人里添加号码”，R13允许直接输入号码 | 高 | 同一个新建页顶端C03收件人，可直接输、匹配、加号选联系人，最多三阶段 |
| M11 SmsCompose | 【实现·图】`i18n-apps_apps_zh_clean-SmsCompose.png` | 收件人只是文字，正文单行在顶部、发送远在底部，还另有写长短信，R13 | 高 | 顶部可编辑收件人，正文C04底部条、右侧发送；超长原位扩行，不再跳独立长文 |
| M12 SmsThread | 【实现·图】`i18n-apps_apps_zh_clean-SmsThread.png` | 一条消息占巨大卡，回复要离开当前页，R13底部直接回 | 高 | 按内容高的左右消息块，分页，底部C04输入＋发送，时间／方向明确 |
| M13 短信发送中／失败／不确定 | 【实现·码】`sms.c:67–76,108–116,137–147` | 能禁用发送、显示服务反馈，但没有会话内失败重发动作；UI截图不能证明对方收到 | 高 | 每条显示真实发送阶段，确定失败才提供重发，不确定明确告知、由本人决定；追踪实际收件归真机通信验收 |
| M14 PhoneContacts 列表 | 【实现·图】`i18n-apps_apps_zh_clean-PhoneContacts.png` | 无独立应用入口、无搜索、记录按库遍历不是姓名序，R14 Contacts | 高 | 独立Contacts，C05搜索＋按名排序C02列表，电话内可共用这份内容 |
| M15 PhoneContacts 空 | 【实现·图】`i18n-apps_apps_zh_clean-PhoneContacts-empty.png` | 必须先进入电话才能创建，R14独立通讯录 | 高 | 应用列表→联系人→新建两步到表单，C11直接添加 |
| M16 联系人详情 | 【实现·码】`system/apps/contacts/contacts.c:154,190–220` | 点击姓名直接到编辑页，只有“选择这个号码”，缺直接打电话／短信，R14 | 高 | 详情C10姓名，两个明确动作打电话／发短信；编辑尾侧入口；不另加联系人服务 |
| M17 ContactEdit 新建 | 【实现·图】`i18n-apps_apps_zh_clean-ContactEdit-new.png` | 新建也写“编辑联系人”，只有姓名和号码、无备注；号码用普通文本输入，R14 | 高 | 标题新建／编辑分清，C03姓名、电话类型号码、备注C04；下一项顺序明确 |
| M18 ContactEdit 已有 | 【实现·图】`i18n-apps_apps_zh_clean-ContactEdit.png` | “选择这个号码／删除／保存”三条大按钮，详情和编辑混合，R14 | 中 | 详情与编辑分离；编辑页C09保存，删除置低优先组；有格式号码由现有通信规整处处理 |

其余应用、确认卡、首次开机、备份传输和 AI 记录继续补录。这里的严重度是基线问题，不预判其他分支正在做的修复已成功。
