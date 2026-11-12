# 固定源码与行号

所有外部源码均为本次只读取证，查阅日期2026-11-12。使用 `blob:none`、`no-checkout` 克隆，仅按路径读取文本；没有下载/运行固件二进制。完整提交与文件SHA-256见 [sources.json](sources.json)。行号是下面固定提交的原文件行号。

链接证明代码/上游说明存在；不能证明已在Plus移植、体验测试或满足资源预算。README功能列为上游说明，核心流程另外给代码。未把整个文件列表当成逐文件读过。

## F01

[FossifyOrg/Phone](https://github.com/FossifyOrg/Phone/tree/25804aa81061868c0f5d5b4ad4eefc544507380c) · `25804aa81061868c0f5d5b4ad4eefc544507380c` · Kotlin / Android Views、Telecom。

- [app/src/main/kotlin/org/fossify/phone/activities/DialpadActivity.kt:295–399](https://github.com/FossifyOrg/Phone/blob/25804aa81061868c0f5d5b4ad4eefc544507380c/app/src/main/kotlin/org/fossify/phone/activities/DialpadActivity.kt#L295-L399)：号码/姓名匹配、SIM选择与快捷拨号。
- [app/src/main/kotlin/org/fossify/phone/activities/CallActivity.kt:469–520](https://github.com/FossifyOrg/Phone/blob/25804aa81061868c0f5d5b4ad4eefc544507380c/app/src/main/kotlin/org/fossify/phone/activities/CallActivity.kt#L469-L520)：音频路由显示。
- [app/src/main/kotlin/org/fossify/phone/activities/CallActivity.kt:653–738](https://github.com/FossifyOrg/Phone/blob/25804aa81061868c0f5d5b4ad4eefc544507380c/app/src/main/kotlin/org/fossify/phone/activities/CallActivity.kt#L653-L738)：通话状态与联系人显示。

## F02

[FossifyOrg/Messages](https://github.com/FossifyOrg/Messages/tree/d9510f44b03c82869856d89a0fcd30b880df9688) · `d9510f44b03c82869856d89a0fcd30b880df9688` · Kotlin / Android Views、系统短信接口。

- [app/src/main/kotlin/org/fossify/messages/activities/ThreadActivity.kt:343–369](https://github.com/FossifyOrg/Messages/blob/d9510f44b03c82869856d89a0fcd30b880df9688/app/src/main/kotlin/org/fossify/messages/activities/ThreadActivity.kt#L343-L369)：草稿保存。
- [app/src/main/kotlin/org/fossify/messages/activities/ThreadActivity.kt:685–705](https://github.com/FossifyOrg/Messages/blob/d9510f44b03c82869856d89a0fcd30b880df9688/app/src/main/kotlin/org/fossify/messages/activities/ThreadActivity.kt#L685-L705)：失败消息重新编辑/发送对象。
- [app/src/main/kotlin/org/fossify/messages/activities/ThreadActivity.kt:888–924](https://github.com/FossifyOrg/Messages/blob/d9510f44b03c82869856d89a0fcd30b880df9688/app/src/main/kotlin/org/fossify/messages/activities/ThreadActivity.kt#L888-L924)：长度/分段提示。
- [app/src/main/kotlin/org/fossify/messages/activities/ThreadActivity.kt:1585–1690](https://github.com/FossifyOrg/Messages/blob/d9510f44b03c82869856d89a0fcd30b880df9688/app/src/main/kotlin/org/fossify/messages/activities/ThreadActivity.kt#L1585-L1690)：普通和定时发送路径。

## F03

[FossifyOrg/Contacts](https://github.com/FossifyOrg/Contacts/tree/5664636dbc78dd0c0b86ccc25b554943b24feb96) · `5664636dbc78dd0c0b86ccc25b554943b24feb96` · Kotlin / Android Views、Contacts Provider。

- [app/src/main/kotlin/org/fossify/contacts/activities/ViewContactActivity.kt:198–249](https://github.com/FossifyOrg/Contacts/blob/5664636dbc78dd0c0b86ccc25b554943b24feb96/app/src/main/kotlin/org/fossify/contacts/activities/ViewContactActivity.kt#L198-L249)：详情电话/短信/邮件动作。
- [app/src/main/kotlin/org/fossify/contacts/activities/ViewContactActivity.kt:301–371](https://github.com/FossifyOrg/Contacts/blob/5664636dbc78dd0c0b86ccc25b554943b24feb96/app/src/main/kotlin/org/fossify/contacts/activities/ViewContactActivity.kt#L301-L371)：收藏和多号码。
- [app/src/main/kotlin/org/fossify/contacts/helpers/VcfImporter.kt:38–105](https://github.com/FossifyOrg/Contacts/blob/5664636dbc78dd0c0b86ccc25b554943b24feb96/app/src/main/kotlin/org/fossify/contacts/helpers/VcfImporter.kt#L38-L105)：vCard导入处理。

## F04

[FossifyOrg/Clock](https://github.com/FossifyOrg/Clock/tree/0511f508368ada82cdb587b0bf8df9d56cf9e6ad) · `0511f508368ada82cdb587b0bf8df9d56cf9e6ad` · Kotlin / Android Views、系统闹钟服务。

- [app/src/main/kotlin/org/fossify/clock/dialogs/EditAlarmDialog.kt:110–156](https://github.com/FossifyOrg/Clock/blob/0511f508368ada82cdb587b0bf8df9d56cf9e6ad/app/src/main/kotlin/org/fossify/clock/dialogs/EditAlarmDialog.kt#L110-L156)：星期重复选择。
- [app/src/main/kotlin/org/fossify/clock/dialogs/EditTimerDialog.kt:19–29](https://github.com/FossifyOrg/Clock/blob/0511f508368ada82cdb587b0bf8df9d56cf9e6ad/app/src/main/kotlin/org/fossify/clock/dialogs/EditTimerDialog.kt#L19-L29)：秒数展示与编辑入口。
- [app/src/main/kotlin/org/fossify/clock/dialogs/EditTimerDialog.kt:97–102](https://github.com/FossifyOrg/Clock/blob/0511f508368ada82cdb587b0bf8df9d56cf9e6ad/app/src/main/kotlin/org/fossify/clock/dialogs/EditTimerDialog.kt#L97-L102)：时长编辑回传秒数。
- [app/src/main/kotlin/org/fossify/clock/fragments/StopwatchFragment.kt:247–285](https://github.com/FossifyOrg/Clock/blob/0511f508368ada82cdb587b0bf8df9d56cf9e6ad/app/src/main/kotlin/org/fossify/clock/fragments/StopwatchFragment.kt#L247-L285)：计次列表及时间更新。
- [app/src/main/kotlin/org/fossify/clock/dialogs/AddTimeZonesDialog.kt:11–31](https://github.com/FossifyOrg/Clock/blob/0511f508368ada82cdb587b0bf8df9d56cf9e6ad/app/src/main/kotlin/org/fossify/clock/dialogs/AddTimeZonesDialog.kt#L11-L31)：世界时钟多时区选择。

## F05

[FossifyOrg/Calculator](https://github.com/FossifyOrg/Calculator/tree/bd2bf37edaff3e0c12538c3499760c90620c4dc2) · `bd2bf37edaff3e0c12538c3499760c90620c4dc2` · Kotlin / Android Views、EvalEx、BigDecimal。

- [app/src/main/kotlin/org/fossify/math/helpers/CalculatorImpl.kt:228–310](https://github.com/FossifyOrg/Calculator/blob/bd2bf37edaff3e0c12538c3499760c90620c4dc2/app/src/main/kotlin/org/fossify/math/helpers/CalculatorImpl.kt#L228-L310)：根号/表达式求值和历史写入。
- [app/src/main/kotlin/org/fossify/math/helpers/CalculatorImpl.kt:176–195](https://github.com/FossifyOrg/Calculator/blob/bd2bf37edaff3e0c12538c3499760c90620c4dc2/app/src/main/kotlin/org/fossify/math/helpers/CalculatorImpl.kt#L176-L195)：百分比单独语义。
- [app/src/main/kotlin/org/fossify/math/activities/MainActivity.kt:190–200](https://github.com/FossifyOrg/Calculator/blob/bd2bf37edaff3e0c12538c3499760c90620c4dc2/app/src/main/kotlin/org/fossify/math/activities/MainActivity.kt#L190-L200)：历史入口。

## F06

[FossifyOrg/Calendar](https://github.com/FossifyOrg/Calendar/tree/a437b8f2a354e0487043fd6b98734f9cfeb93933) · `a437b8f2a354e0487043fd6b98734f9cfeb93933` · Kotlin / Android Views、日历Provider。

- [app/src/main/kotlin/org/fossify/calendar/activities/EventActivity.kt:846–915](https://github.com/FossifyOrg/Calendar/blob/a437b8f2a354e0487043fd6b98734f9cfeb93933/app/src/main/kotlin/org/fossify/calendar/activities/EventActivity.kt#L846-L915)：多次提醒和重复编辑。
- [app/src/main/kotlin/org/fossify/calendar/helpers/IcsImporter.kt:143–190](https://github.com/FossifyOrg/Calendar/blob/a437b8f2a354e0487043fd6b98734f9cfeb93933/app/src/main/kotlin/org/fossify/calendar/helpers/IcsImporter.kt#L143-L190)：ICS属性和RRULE解析。
- [app/src/main/kotlin/org/fossify/calendar/helpers/IcsImporter.kt:250–276](https://github.com/FossifyOrg/Calendar/blob/a437b8f2a354e0487043fd6b98734f9cfeb93933/app/src/main/kotlin/org/fossify/calendar/helpers/IcsImporter.kt#L250-L276)：重复例外日期。

## F07

[FossifyOrg/Gallery](https://github.com/FossifyOrg/Gallery/tree/2688360242468f1d591eddf23740c879616bb828) · `2688360242468f1d591eddf23740c879616bb828` · Kotlin / Android Views、子采样图片和手势控件。

- [app/src/main/kotlin/org/fossify/gallery/fragments/PhotoFragment.kt:223–238](https://github.com/FossifyOrg/Gallery/blob/2688360242468f1d591eddf23740c879616bb828/app/src/main/kotlin/org/fossify/gallery/fragments/PhotoFragment.kt#L223-L238)：EXIF方向处理。
- [app/src/main/kotlin/org/fossify/gallery/fragments/PhotoFragment.kt:599–635](https://github.com/FossifyOrg/Gallery/blob/2688360242468f1d591eddf23740c879616bb828/app/src/main/kotlin/org/fossify/gallery/fragments/PhotoFragment.kt#L599-L635)：图片缩放手势与外层滑动分工。
- [app/src/main/kotlin/org/fossify/gallery/activities/MediaActivity.kt:1078–1142](https://github.com/FossifyOrg/Gallery/blob/2688360242468f1d591eddf23740c879616bb828/app/src/main/kotlin/org/fossify/gallery/activities/MediaActivity.kt#L1078-L1142)：逐项删除与回收站处理。

## F08

[FossifyOrg/Notes](https://github.com/FossifyOrg/Notes/tree/f7efe788832e9f5eee0fb67417e92f4dfef0257c) · `f7efe788832e9f5eee0fb67417e92f4dfef0257c` · Kotlin / Android Views、文本/清单编辑器。

- [app/src/main/kotlin/org/fossify/notes/fragments/TextFragment.kt:94–118](https://github.com/FossifyOrg/Notes/blob/f7efe788832e9f5eee0fb67417e92f4dfef0257c/app/src/main/kotlin/org/fossify/notes/fragments/TextFragment.kt#L94-L118)：离开页面自动保存。
- [app/src/main/kotlin/org/fossify/notes/fragments/TextFragment.kt:245–293](https://github.com/FossifyOrg/Notes/blob/f7efe788832e9f5eee0fb67417e92f4dfef0257c/app/src/main/kotlin/org/fossify/notes/fragments/TextFragment.kt#L245-L293)：保存实现。
- [app/src/main/kotlin/org/fossify/notes/activities/MainActivity.kt:252–315](https://github.com/FossifyOrg/Notes/blob/f7efe788832e9f5eee0fb67417e92f4dfef0257c/app/src/main/kotlin/org/fossify/notes/activities/MainActivity.kt#L252-L315)：文本/清单上下文动作。
- [app/src/main/kotlin/org/fossify/notes/activities/MainActivity.kt:596–675](https://github.com/FossifyOrg/Notes/blob/f7efe788832e9f5eee0fb67417e92f4dfef0257c/app/src/main/kotlin/org/fossify/notes/activities/MainActivity.kt#L596-L675)：搜索与上/下个命中。

## F09

[FossifyOrg/File-Manager](https://github.com/FossifyOrg/File-Manager/tree/1e43648eadfdbae862f1fd2f57ac68b3b7b35357) · `1e43648eadfdbae862f1fd2f57ac68b3b7b35357` · Kotlin / Android Views、文件与SAF接口。

- [app/src/main/kotlin/org/fossify/filemanager/activities/MainActivity.kt:191–247](https://github.com/FossifyOrg/File-Manager/blob/1e43648eadfdbae862f1fd2f57ac68b3b7b35357/app/src/main/kotlin/org/fossify/filemanager/activities/MainActivity.kt#L191-L247)：搜索和排序入口。
- [app/src/main/kotlin/org/fossify/filemanager/fragments/ItemsFragment.kt:305–399](https://github.com/FossifyOrg/File-Manager/blob/1e43648eadfdbae862f1fd2f57ac68b3b7b35357/app/src/main/kotlin/org/fossify/filemanager/fragments/ItemsFragment.kt#L305-L399)：目录内递归搜索与结果排序。

## F10

[FossifyOrg/Launcher](https://github.com/FossifyOrg/Launcher/tree/622d894113553ffdfc6befdb055087551ed8939b) · `622d894113553ffdfc6befdb055087551ed8939b` · Kotlin / Android、自绘桌面网格。

- [app/src/main/kotlin/org/fossify/home/views/HomeScreenGrid.kt:100–148](https://github.com/FossifyOrg/Launcher/blob/622d894113553ffdfc6befdb055087551ed8939b/app/src/main/kotlin/org/fossify/home/views/HomeScreenGrid.kt#L100-L148)：拖动/文件夹状态。
- [app/src/main/kotlin/org/fossify/home/activities/MainActivity.kt:137–190](https://github.com/FossifyOrg/Launcher/blob/622d894113553ffdfc6befdb055087551ed8939b/app/src/main/kotlin/org/fossify/home/activities/MainActivity.kt#L137-L190)：桌面、应用和小部件区域。

## F11

[FossifyOrg/Camera](https://github.com/FossifyOrg/Camera/tree/a57a2ffbbac1dd42231e3f030f738929ad139c6e) · `a57a2ffbbac1dd42231e3f030f738929ad139c6e` · Kotlin / Android CameraX、Views。

- [app/src/main/kotlin/org/fossify/camera/activities/MainActivity.kt:569–595](https://github.com/FossifyOrg/Camera/blob/a57a2ffbbac1dd42231e3f030f738929ad139c6e/app/src/main/kotlin/org/fossify/camera/activities/MainActivity.kt#L569-L595)：最近照片预览更新与交叉渐变。
- [app/src/main/kotlin/org/fossify/camera/activities/MainActivity.kt:160–190](https://github.com/FossifyOrg/Camera/blob/a57a2ffbbac1dd42231e3f030f738929ad139c6e/app/src/main/kotlin/org/fossify/camera/activities/MainActivity.kt#L160-L190)：照片/录像模式。
- [app/src/main/kotlin/org/fossify/camera/activities/MainActivity.kt:990–1032](https://github.com/FossifyOrg/Camera/blob/a57a2ffbbac1dd42231e3f030f738929ad139c6e/app/src/main/kotlin/org/fossify/camera/activities/MainActivity.kt#L990-L1032)：定时拍照过程。
- [app/src/main/kotlin/org/fossify/camera/implementations/CameraXPreview.kt:225–270](https://github.com/FossifyOrg/Camera/blob/a57a2ffbbac1dd42231e3f030f738929ad139c6e/app/src/main/kotlin/org/fossify/camera/implementations/CameraXPreview.kt#L225-L270)：预览/拍照绑定生命周期。

## A01

[LineageOS/android_packages_apps_Dialer](https://github.com/LineageOS/android_packages_apps_Dialer/tree/3b9a8e35d2bc2c804ee1c64f9b291630a00e333d) · `3b9a8e35d2bc2c804ee1c64f9b291630a00e333d` · Java / AOSP Android Views、Telecom。

- [java/com/android/dialer/smartdial/util/SmartDialNameMatcher.java:117–181](https://github.com/LineageOS/android_packages_apps_Dialer/blob/3b9a8e35d2bc2c804ee1c64f9b291630a00e333d/java/com/android/dialer/smartdial/util/SmartDialNameMatcher.java#L117-L181)：号码匹配。
- [java/com/android/dialer/smartdial/util/SmartDialNameMatcher.java:412–434](https://github.com/LineageOS/android_packages_apps_Dialer/blob/3b9a8e35d2bc2c804ee1c64f9b291630a00e333d/java/com/android/dialer/smartdial/util/SmartDialNameMatcher.java#L412-L434)：姓名T9匹配。
- [java/com/android/incallui/CallCardPresenter.java:394–446](https://github.com/LineageOS/android_packages_apps_Dialer/blob/3b9a8e35d2bc2c804ee1c64f9b291630a00e333d/java/com/android/incallui/CallCardPresenter.java#L394-L446)：区分真实通话状态的展示。

## A02

[LineageOS/android_packages_apps_Messaging](https://github.com/LineageOS/android_packages_apps_Messaging/tree/47eb88d3219eea82905f844baa9240925329ab6f) · `47eb88d3219eea82905f844baa9240925329ab6f` · Java / AOSP Android Views、短信/MMS Provider。

- [src/com/android/messaging/datamodel/data/MessageData.java:120–154](https://github.com/LineageOS/android_packages_apps_Messaging/blob/47eb88d3219eea82905f844baa9240925329ab6f/src/com/android/messaging/datamodel/data/MessageData.java#L120-L154)：草稿、发送、送达、失败等不同消息状态。
- [src/com/android/messaging/ui/conversation/ComposeMessageView.java:145–228](https://github.com/LineageOS/android_packages_apps_Messaging/blob/47eb88d3219eea82905f844baa9240925329ab6f/src/com/android/messaging/ui/conversation/ComposeMessageView.java#L145-L228)：附件、SIM和编辑区域的界面结构。

## A03

[LineageOS/android_packages_apps_Contacts](https://github.com/LineageOS/android_packages_apps_Contacts/tree/c03937e4d35924455b6e27d34fd91b1426580f4b) · `c03937e4d35924455b6e27d34fd91b1426580f4b` · Java / AOSP Android Views、Contacts Provider。

- [src/com/android/contacts/quickcontact/QuickContactActivity.java:1012–1070](https://github.com/LineageOS/android_packages_apps_Contacts/blob/c03937e4d35924455b6e27d34fd91b1426580f4b/src/com/android/contacts/quickcontact/QuickContactActivity.java#L1012-L1070)：独立联系人详情绑定。
- [src/com/android/contacts/quickcontact/QuickContactActivity.java:1730–1802](https://github.com/LineageOS/android_packages_apps_Contacts/blob/c03937e4d35924455b6e27d34fd91b1426580f4b/src/com/android/contacts/quickcontact/QuickContactActivity.java#L1730-L1802)：短信与电话等关联动作。

## A04

[LineageOS/android_packages_apps_Settings](https://github.com/LineageOS/android_packages_apps_Settings/tree/f82184028165ea108335a9745428c26fea3cec47) · `f82184028165ea108335a9745428c26fea3cec47` · Java / Android Preference、Dashboard。

- [src/com/android/settings/dashboard/DashboardFragment.java:95–178](https://github.com/LineageOS/android_packages_apps_Settings/blob/f82184028165ea108335a9745428c26fea3cec47/src/com/android/settings/dashboard/DashboardFragment.java#L95-L178)：设置控制器与页面显示分离。
- [src/com/android/settings/dashboard/DashboardFragment.java:345–410](https://github.com/LineageOS/android_packages_apps_Settings/blob/f82184028165ea108335a9745428c26fea3cec47/src/com/android/settings/dashboard/DashboardFragment.java#L345-L410)：设置项状态更新。
- [src/com/android/settings/core/SubSettingLauncher.java:55–130](https://github.com/LineageOS/android_packages_apps_Settings/blob/f82184028165ea108335a9745428c26fea3cec47/src/com/android/settings/core/SubSettingLauncher.java#L55-L130)：子设置页及标题/参数导航。

## C01

[Etar-Group/Etar-Calendar](https://github.com/Etar-Group/Etar-Calendar/tree/82ff55ea1fc035177054ce4b4aa8e0dd91059ef1) · `82ff55ea1fc035177054ce4b4aa8e0dd91059ef1` · Java / AOSP Calendar、Android Views。

- [app/src/main/java/com/android/calendar/calendarcommon2/EventRecurrence.java:31–92](https://github.com/Etar-Group/Etar-Calendar/blob/82ff55ea1fc035177054ce4b4aa8e0dd91059ef1/app/src/main/java/com/android/calendar/calendarcommon2/EventRecurrence.java#L31-L92)：重复频率、次数、日期条件模型。
- [app/src/main/java/com/android/calendar/event/EditEventHelper.java:307–394](https://github.com/Etar-Group/Etar-Calendar/blob/82ff55ea1fc035177054ce4b4aa8e0dd91059ef1/app/src/main/java/com/android/calendar/event/EditEventHelper.java#L307-L394)：编辑重复事件的作用范围。
- [README.md:17–70](https://github.com/Etar-Group/Etar-Calendar/blob/82ff55ea1fc035177054ce4b4aa8e0dd91059ef1/README.md#L17-L70)：月/周/日/议程、搜索等上游说明；不当本次实测。

## W01

[breezy-weather/breezy-weather](https://github.com/breezy-weather/breezy-weather/tree/484cbc53bd48c6dd735edcac13ddfc9ebdeb0e4a) · `484cbc53bd48c6dd735edcac13ddfc9ebdeb0e4a` · Kotlin / Android、Jetpack Compose、Material 3。

- [README.md:80–104](https://github.com/breezy-weather/breezy-weather/blob/484cbc53bd48c6dd735edcac13ddfc9ebdeb0e4a/README.md#L80-L104)：逐小时/分钟、预警和天气指标清单；上游说明。
- [app/src/main/kotlin/org/breezyweather/domain/weather/model/Weather.kt:34–92](https://github.com/breezy-weather/breezy-weather/blob/484cbc53bd48c6dd735edcac13ddfc9ebdeb0e4a/app/src/main/kotlin/org/breezyweather/domain/weather/model/Weather.kt#L34-L92)：空气质量/分钟降雨的结构化展示逻辑。
- [app/src/main/kotlin/org/breezyweather/domain/weather/model/Daily.kt:30–100](https://github.com/breezy-weather/breezy-weather/blob/484cbc53bd48c6dd735edcac13ddfc9ebdeb0e4a/app/src/main/kotlin/org/breezyweather/domain/weather/model/Daily.kt#L30-L100)：逐日信息和白天/夜晚关系。

## G01

[deckerst/aves](https://github.com/deckerst/aves/tree/9d3f1ff19f680db043c9d80831c6f222e0f8220c) · `9d3f1ff19f680db043c9d80831c6f222e0f8220c` · Dart / Flutter、Android媒体平台。

- [README.md:36–44](https://github.com/deckerst/aves/blob/9d3f1ff19f680db043c9d80831c6f222e0f8220c/README.md#L36-L44)：格式、元数据和按相册/标签搜索；上游说明。
- [lib/widgets/collection/collection_page.dart:35–117](https://github.com/deckerst/aves/blob/9d3f1ff19f680db043c9d80831c6f222e0f8220c/lib/widgets/collection/collection_page.dart#L35-L117)：筛选集合和浏览页面。
- [lib/widgets/viewer/entry_viewer_page.dart:11–58](https://github.com/deckerst/aves/blob/9d3f1ff19f680db043c9d80831c6f222e0f8220c/lib/widgets/viewer/entry_viewer_page.dart#L11-L58)：集合内单图与控制器。
- [lib/widgets/viewer/info/info_page.dart:1–115](https://github.com/deckerst/aves/blob/9d3f1ff19f680db043c9d80831c6f222e0f8220c/lib/widgets/viewer/info/info_page.dart#L1-L115)：图片元数据详情页面。

## N01

[laurent22/joplin](https://github.com/laurent22/joplin/tree/0e1489ce885ddf570da0b389c1cc57b98bf41307) · `0e1489ce885ddf570da0b389c1cc57b98bf41307` · TypeScript / 共享笔记业务；桌面/手机客户端。

- [README.md:7–17](https://github.com/laurent22/joplin/blob/0e1489ce885ddf570da0b389c1cc57b98bf41307/README.md#L7-L17)：笔记/待办/离线/交换/同步的上游说明。
- [packages/lib/models/Note.ts:72–140](https://github.com/laurent22/joplin/blob/0e1489ce885ddf570da0b389c1cc57b98bf41307/packages/lib/models/Note.ts#L72-L140)：笔记与待办属性。
- [packages/lib/services/search/SearchEngine.ts:631–665](https://github.com/laurent22/joplin/blob/0e1489ce885ddf570da0b389c1cc57b98bf41307/packages/lib/services/search/SearchEngine.ts#L631-L665)：全文搜索输入处理。
- [packages/lib/services/search/SearchEngine.ts:740–760](https://github.com/laurent22/joplin/blob/0e1489ce885ddf570da0b389c1cc57b98bf41307/packages/lib/services/search/SearchEngine.ts#L740-L760)：非字母语言搜索方式选择。

## X02

[TeamAmaze/AmazeFileManager](https://github.com/TeamAmaze/AmazeFileManager/tree/5f1aea6829978aff67f97bf6f00ddd9fa9e3723e) · `5f1aea6829978aff67f97bf6f00ddd9fa9e3723e` · Java、Kotlin / Android Views、前台文件任务。

- [README.md:22–34](https://github.com/TeamAmaze/AmazeFileManager/blob/5f1aea6829978aff67f97bf6f00ddd9fa9e3723e/README.md#L22-L34)：文件操作、标签页、搜索、归档的上游说明。
- [app/src/main/java/com/amaze/filemanager/asynchronous/services/CopyService.java:238–296](https://github.com/TeamAmaze/AmazeFileManager/blob/5f1aea6829978aff67f97bf6f00ddd9fa9e3723e/app/src/main/java/com/amaze/filemanager/asynchronous/services/CopyService.java#L238-L296)：复制/移动结果与失败集合。
- [app/src/main/java/com/amaze/filemanager/filesystem/compressed/CompressedHelper.java:90–151](https://github.com/TeamAmaze/AmazeFileManager/blob/5f1aea6829978aff67f97bf6f00ddd9fa9e3723e/app/src/main/java/com/amaze/filemanager/filesystem/compressed/CompressedHelper.java#L90-L151)：归档类型到解压器的分派。

## H01

[Neamar/KISS](https://github.com/Neamar/KISS/tree/231eff343320c22355a22e74853658291a3e8995) · `231eff343320c22355a22e74853658291a3e8995` · Java / Android、自有搜索/Provider。

- [README.md:3–12](https://github.com/Neamar/KISS/blob/231eff343320c22355a22e74853658291a3e8995/README.md#L3-L12)：搜索应用/联系人/设置、常用结果前置；上游说明。
- [app/src/main/java/fr/neamar/kiss/DataHandler.java:297–321](https://github.com/Neamar/KISS/blob/231eff343320c22355a22e74853658291a3e8995/app/src/main/java/fr/neamar/kiss/DataHandler.java#L297-L321)：同查询分派给各数据Provider。
- [app/src/main/java/fr/neamar/kiss/MainActivity.java:197–228](https://github.com/Neamar/KISS/blob/231eff343320c22355a22e74853658291a3e8995/app/src/main/java/fr/neamar/kiss/MainActivity.java#L197-L228)：收藏/历史变化刷新。

## E01

[AlayaElla/MetalioClaw4-AgentUI](https://github.com/AlayaElla/MetalioClaw4-AgentUI/tree/172ec530ff65e11521585fe3b8611daa20a78aa4) · `172ec530ff65e11521585fe3b8611daa20a78aa4` · C++ / ESP-IDF、LVGL；P4触屏。

- [main/display/agent_ui/apps/camera/camera_view.cc:44–89](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/main/display/agent_ui/apps/camera/camera_view.cc#L44-L89)：图库缩略图尺寸/容量和5种效果。
- [main/display/agent_ui/apps/camera/camera_gallery_repository.cc:116–177](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/main/display/agent_ui/apps/camera/camera_gallery_repository.cc#L116-L177)：图库枚举。
- [main/display/agent_ui/apps/camera/camera_controller.cc:112–166](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/main/display/agent_ui/apps/camera/camera_controller.cc#L112-L166)：取景与缩略图生命周期。
- [main/display/agent_ui/apps/files/files_view.cc:319–396](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/main/display/agent_ui/apps/files/files_view.cc#L319-L396)：可见文件列表行的构建。
- [main/display/agent_ui/apps/phone/phone_view.cc:270–303](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/main/display/agent_ui/apps/phone/phone_view.cc#L270-L303)：AT命令受理不冒称接通。

## E02

[dparnell/lilyphone1](https://github.com/dparnell/lilyphone1/tree/de10fc3882292b9be1cfcd14632726590fdef262) · `de10fc3882292b9be1cfcd14632726590fdef262` · C++ / Arduino、LVGL、GxEPD2；单色触屏/实体键盘。

- [README.md:22–45](https://github.com/dparnell/lilyphone1/blob/de10fc3882292b9be1cfcd14632726590fdef262/README.md#L22-L45)：电话/联系人/消息及UCS2的上游说明。
- [src/apps/phone_store.cpp:111–138](https://github.com/dparnell/lilyphone1/blob/de10fc3882292b9be1cfcd14632726590fdef262/src/apps/phone_store.cpp#L111-L138)：号码匹配规则。
- [src/apps/phone_store.cpp:190–251](https://github.com/dparnell/lilyphone1/blob/de10fc3882292b9be1cfcd14632726590fdef262/src/apps/phone_store.cpp#L190-L251)：来电查名和联系人排序/添加。
- [src/ui/ui_phone1.cpp:2807–2851](https://github.com/dparnell/lilyphone1/blob/de10fc3882292b9be1cfcd14632726590fdef262/src/ui/ui_phone1.cpp#L2807-L2851)：短信提交界面处理。
- [src/ui/ui_phone1.cpp:3331–3382](https://github.com/dparnell/lilyphone1/blob/de10fc3882292b9be1cfcd14632726590fdef262/src/ui/ui_phone1.cpp#L3331-L3382)：通话号码与联系人显示。
- [src/ui/ui_phone1.cpp:3880–3930](https://github.com/dparnell/lilyphone1/blob/de10fc3882292b9be1cfcd14632726590fdef262/src/ui/ui_phone1.cpp#L3880-L3930)：会话内消息删除和重绘。

## E03

[paulwilke/Phone-OS](https://github.com/paulwilke/Phone-OS/tree/903a451c759ad1ac1004da1951051ee106d00130) · `903a451c759ad1ac1004da1951051ee106d00130` · C++ / ESP32 Arduino、独立黑白界面。

- [README.md:1–80](https://github.com/paulwilke/Phone-OS/blob/903a451c759ad1ac1004da1951051ee106d00130/README.md#L1-L80)：T-Deck Pro MAX公开实现的功能/边界说明。
- [src/ui/screens/ComposeScreen.cpp:1–110](https://github.com/paulwilke/Phone-OS/blob/903a451c759ad1ac1004da1951051ee106d00130/src/ui/screens/ComposeScreen.cpp#L1-L110)：墨水屏短信编辑界面。
- [src/app/ContactBook.cpp:1–110](https://github.com/paulwilke/Phone-OS/blob/903a451c759ad1ac1004da1951051ee106d00130/src/app/ContactBook.cpp#L1-L110)：联系人业务实现。

## M01

[InfiniTimeOrg/InfiniTime](https://github.com/InfiniTimeOrg/InfiniTime/tree/9ad16304f5fd526b781a760cf1313813d232645e) · `9ad16304f5fd526b781a760cf1313813d232645e` · C++ / FreeRTOS、LVGL；PineTime。

- [src/components/stopwatch/StopWatchController.cpp:11–59](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/components/stopwatch/StopWatchController.cpp#L11-L59)：暂停/计次/累计时间。
- [src/displayapp/screens/Timer.cpp:118–155](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/displayapp/screens/Timer.cpp#L118-L155)：剩余时间显示。
- [src/displayapp/screens/Timer.cpp:183–199](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/displayapp/screens/Timer.cpp#L183-L199)：分和秒的输入。
- [src/components/alarm/AlarmController.cpp:56–90](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/components/alarm/AlarmController.cpp#L56-L90)：闹钟调度；未移用为本项目电源机制。

## M02

[ZSWatch/ZSWatch](https://github.com/ZSWatch/ZSWatch/tree/821c340366a0bc55b536f7feb5d45ca3836c44c3) · `821c340366a0bc55b536f7feb5d45ca3836c44c3` · C / Zephyr、LVGL、Zbus。

- [app/src/applications/calculator/smf_calculator_thread.c:107–153](https://github.com/ZSWatch/ZSWatch/blob/821c340366a0bc55b536f7feb5d45ca3836c44c3/app/src/applications/calculator/smf_calculator_thread.c#L107-L153)：四则/除零/结果格式化。
- [app/src/applications/calculator/calculator_ui.c:1–118](https://github.com/ZSWatch/ZSWatch/blob/821c340366a0bc55b536f7feb5d45ca3836c44c3/app/src/applications/calculator/calculator_ui.c#L1-L118)：数字和操作键UI。
- [app/src/applications/weather/weather_ui.c:99–148](https://github.com/ZSWatch/ZSWatch/blob/821c340366a0bc55b536f7feb5d45ca3836c44c3/app/src/applications/weather/weather_ui.c#L99-L148)：当前天气与预报布局。
- [app/src/applications/weather/weather_ui.c:211–314](https://github.com/ZSWatch/ZSWatch/blob/821c340366a0bc55b536f7feb5d45ca3836c44c3/app/src/applications/weather/weather_ui.c#L211-L314)：天气显示和加载状态。
- [app/src/applications/voice_memo/voice_memo_ui.c:65–136](https://github.com/ZSWatch/ZSWatch/blob/821c340366a0bc55b536f7feb5d45ca3836c44c3/app/src/applications/voice_memo/voice_memo_ui.c#L65-L136)：录音界面和闪动指示。
- [app/src/applications/voice_memo/voice_memo_app.c:46–102](https://github.com/ZSWatch/ZSWatch/blob/821c340366a0bc55b536f7feb5d45ca3836c44c3/app/src/applications/voice_memo/voice_memo_app.c#L46-L102)：录音事件接入。

## M03

[espruino/BangleApps](https://github.com/espruino/BangleApps/tree/9fe678997d86dfabb5aadc5fba272b7d5897a340) · `9fe678997d86dfabb5aadc5fba272b7d5897a340` · JavaScript / Espruino、Graphics/Layout、Storage。

- [apps/weather/app.js:101–160](https://github.com/espruino/BangleApps/blob/9fe678997d86dfabb5aadc5fba272b7d5897a340/apps/weather/app.js#L101-L160)：体感/高低温/风和数据年龄、过期状态。
- [apps/weather/lib.js:1–100](https://github.com/espruino/BangleApps/blob/9fe678997d86dfabb5aadc5fba272b7d5897a340/apps/weather/lib.js#L1-L100)：天气来源更新和过期数据处理。
- [apps/calendar/calendar.js:1–100](https://github.com/espruino/BangleApps/blob/9fe678997d86dfabb5aadc5fba272b7d5897a340/apps/calendar/calendar.js#L1-L100)：小屏静态月历。
- [apps/sched/lib.js:1–85](https://github.com/espruino/BangleApps/blob/9fe678997d86dfabb5aadc5fba272b7d5897a340/apps/sched/lib.js#L1-L85)：日期重复与计时调度。
- [apps/noteify/app.js:1–80](https://github.com/espruino/BangleApps/blob/9fe678997d86dfabb5aadc5fba272b7d5897a340/apps/noteify/app.js#L1-L80)：笔记和提醒联动。

## M04

[coredevices/PebbleOS](https://github.com/coredevices/PebbleOS/tree/aa0c27e8330d010e9202f9a706a56002db5861ee) · `aa0c27e8330d010e9202f9a706a56002db5861ee` · C / Pebble原生UI、系统服务。

- [pebble/apps/system/settings/menu.c:23–51](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/apps/system/settings/menu.c#L23-L51)：设置目录和当前值。
- [pebble/apps/system/settings/option_menu.c:26–80](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/apps/system/settings/option_menu.c#L26-L80)：选项行绘制/点击。
- [pebble/apps/system/launcher/default/launcher.c:1–120](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/apps/system/launcher/default/launcher.c#L1-L120)：应用列表/启动器接入。
- [pebble/apps/system/launcher/default/app_glance.c:10–58](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/apps/system/launcher/default/app_glance.c#L10-L58)：应用简览的更新和展示。
- [pebble/apps/system/alarms/alarms.c:214–267](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/apps/system/alarms/alarms.c#L214-L267)：闹钟列表。

## M05

[sqfmi/Watchy](https://github.com/sqfmi/Watchy/tree/d1d233c43b36cac23bccc6abeae998aa3e27724e) · `d1d233c43b36cac23bccc6abeae998aa3e27724e` · C++ / Arduino、GxEPD2、ESP32深睡唤醒。

- [src/Watchy.cpp:135–181](https://github.com/sqfmi/Watchy/blob/d1d233c43b36cac23bccc6abeae998aa3e27724e/src/Watchy.cpp#L135-L181)：实体按键菜单返回。
- [src/Watchy.cpp:289–348](https://github.com/sqfmi/Watchy/blob/d1d233c43b36cac23bccc6abeae998aa3e27724e/src/Watchy.cpp#L289-L348)：黑白菜单绘制。
- [README.md:1–18](https://github.com/sqfmi/Watchy/blob/d1d233c43b36cac23bccc6abeae998aa3e27724e/README.md#L1-L18)：上游手表和支持范围。

## B01

[espressif/esp-brookesia](https://github.com/espressif/esp-brookesia/tree/65d66c810ad706c9e2f82fe9177b11816acee546) · `65d66c810ad706c9e2f82fe9177b11816acee546` · C++ / ESP-IDF组件、JSON UI、System Core/Super。

- [README.md:37–49](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/README.md#L37-L49)：v0.8与历史release分工及S31版本列。
- [app/brookesia_app_settings/src/settings_app.cpp:69–82](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/app/brookesia_app_settings/src/settings_app.cpp#L69-L82)：扫描结果UI的PSRAM开销提示。
- [app/brookesia_app_settings/src/service/wifi.ipp:206–262](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/app/brookesia_app_settings/src/service/wifi.ipp#L206-L262)：Wi-Fi连接状态与可操作反馈。
- [app/brookesia_app_settings/src/service/wifi.ipp:432–491](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/app/brookesia_app_settings/src/service/wifi.ipp#L432-L491)：Wi-Fi扫描服务接入。
- [app/brookesia_app_files/src/files_app.cpp:25–59](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/app/brookesia_app_files/src/files_app.cpp#L25-L59)：文件动作范围。
- [system/brookesia_system_super/src/shell_app_launcher.cpp:154–218](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/system/brookesia_system_super/src/shell_app_launcher.cpp#L154-L218)：按应用身份生成启动器。

## B02

[espressif/esp-box](https://github.com/espressif/esp-box/tree/aae1b7a6d193d968cf53e42f291a60c4ca1c622c) · `aae1b7a6d193d968cf53e42f291a60c4ca1c622c` · C / ESP-IDF、LVGL示例。

- [examples/chatgpt_demo/main/settings/settings.c:1–96](https://github.com/espressif/esp-box/blob/aae1b7a6d193d968cf53e42f291a60c4ca1c622c/examples/chatgpt_demo/main/settings/settings.c#L1-L96)：示例设置存取；不是完整电话类应用。

## B03

[espressif/esp-brookesia](https://github.com/espressif/esp-brookesia/tree/2c0c83fad95b81c81ee560ba8af684ff32a402f4) · `2c0c83fad95b81c81ee560ba8af684ff32a402f4` · C++ / LVGL；v0.4.2历史P4例程。

- [examples/esp_idf/esp_brookesia_phone_p4_function_ev_board/main/main.cpp:13–16](https://github.com/espressif/esp-brookesia/blob/2c0c83fad95b81c81ee560ba8af684ff32a402f4/examples/esp_idf/esp_brookesia_phone_p4_function_ev_board/main/main.cpp#L13-L16)：内置应用示例名字。
- [examples/esp_idf/esp_brookesia_phone_p4_function_ev_board/main/main.cpp:71–80](https://github.com/espressif/esp-brookesia/blob/2c0c83fad95b81c81ee560ba8af684ff32a402f4/examples/esp_idf/esp_brookesia_phone_p4_function_ev_board/main/main.cpp#L71-L80)：安装的是simple/complex/SquareLine示例。

## B04

[espressif/esp-iot-solution](https://github.com/espressif/esp-iot-solution/tree/6e3cef29422a10255fef8919f51aa52914d599e7) · `6e3cef29422a10255fef8919f51aa52914d599e7` · C / ESP-IDF、摄像头/LCD示例。

- [examples/camera/video_lcd_display/README.md:1–65](https://github.com/espressif/esp-iot-solution/blob/6e3cef29422a10255fef8919f51aa52914d599e7/examples/camera/video_lcd_display/README.md#L1-L65)：官方视频采集到LCD示例边界。
- [examples/camera/video_lcd_display/main/app_video.c:1–112](https://github.com/espressif/esp-iot-solution/blob/6e3cef29422a10255fef8919f51aa52914d599e7/examples/camera/video_lcd_display/main/app_video.c#L1-L112)：缓冲/取帧到LCD的流程；不是完整相机产品。

## L01

[lvgl/lvgl](https://github.com/lvgl/lvgl/tree/53819fe4fd3f3a767420743cbfdb2816f3928931) · `53819fe4fd3f3a767420743cbfdb2816f3928931` · C / LVGL widgets与demos。

- [examples/widgets/calendar/lv_example_calendar_basic.c:1–34](https://github.com/lvgl/lvgl/blob/53819fe4fd3f3a767420743cbfdb2816f3928931/examples/widgets/calendar/lv_example_calendar_basic.c#L1-L34)：日历控件事件/标日；无日程业务。

## X01

[flipperdevices/flipperzero-firmware](https://github.com/flipperdevices/flipperzero-firmware/tree/3060cee9bac71d332dc7629191ae1aeb1edaac33) · `3060cee9bac71d332dc7629191ae1aeb1edaac33` · C / Furi GUI；128x64单色按键设备。

- [applications/main/archive/views/archive_browser_view.c:54–112](https://github.com/flipperdevices/flipperzero-firmware/blob/3060cee9bac71d332dc7629191ae1aeb1edaac33/applications/main/archive/views/archive_browser_view.c#L54-L112)：文件类型/当前选择和上下文菜单。
- [applications/main/archive/helpers/archive_files.c:80–114](https://github.com/flipperdevices/flipperzero-firmware/blob/3060cee9bac71d332dc7629191ae1aeb1edaac33/applications/main/archive/helpers/archive_files.c#L80-L114)：删除、改名、收藏的一致处理。
- [applications/main/archive/scenes/archive_scene_browser.c:1–120](https://github.com/flipperdevices/flipperzero-firmware/blob/3060cee9bac71d332dc7629191ae1aeb1edaac33/applications/main/archive/scenes/archive_scene_browser.c#L1-L120)：文件浏览和菜单事件。

## R01

[novnc/noVNC](https://github.com/novnc/noVNC/tree/e8b5024d4603587b29ffede4fbd28b1fa2939829) · `e8b5024d4603587b29ffede4fbd28b1fa2939829` · JavaScript / HTML5 Canvas、RFB、WebSocket。

- [core/rfb.js:360–423](https://github.com/novnc/noVNC/blob/e8b5024d4603587b29ffede4fbd28b1fa2939829/core/rfb.js#L360-L423)：视口与压缩质量设置。
- [core/rfb.js:450–545](https://github.com/novnc/noVNC/blob/e8b5024d4603587b29ffede4fbd28b1fa2939829/core/rfb.js#L450-L545)：连接/断开与键盘事件接口。
- [README.md:15–85](https://github.com/novnc/noVNC/blob/e8b5024d4603587b29ffede4fbd28b1fa2939829/README.md#L15-L85)：完整客户端的上游能力说明。

## R02

[apache/guacamole-client](https://github.com/apache/guacamole-client/tree/a8daafeb38b60b0acb4059c5218e6d5bacfaeb8f) · `a8daafeb38b60b0acb4059c5218e6d5bacfaeb8f` · Java Web应用、JavaScript / Guacamole显示协议。

- [guacamole-common-js/src/main/webapp/modules/Display.js:624–645](https://github.com/apache/guacamole-client/blob/a8daafeb38b60b0acb4059c5218e6d5bacfaeb8f/guacamole-common-js/src/main/webapp/modules/Display.js#L624-L645)：绘制任务按帧提交/取消。
- [guacamole-common-js/src/main/webapp/modules/Keyboard.js:25–115](https://github.com/apache/guacamole-client/blob/a8daafeb38b60b0acb4059c5218e6d5bacfaeb8f/guacamole-common-js/src/main/webapp/modules/Keyboard.js#L25-L115)：键盘输入事件。
- [guacamole-common-js/src/main/webapp/modules/Client.js:90–178](https://github.com/apache/guacamole-client/blob/a8daafeb38b60b0acb4059c5218e6d5bacfaeb8f/guacamole-common-js/src/main/webapp/modules/Client.js#L90-L178)：客户端连接/状态接口。

## R03

[ttalvitie/browservice](https://github.com/ttalvitie/browservice/tree/02601b047f7abdc90545c9dc669d0081d7968c34) · `02601b047f7abdc90545c9dc669d0081d7968c34` · C++ / CEF；图像返回给浏览器客户端。

- [README.md:3–15](https://github.com/ttalvitie/browservice/blob/02601b047f7abdc90545c9dc669d0081d7968c34/README.md#L3-L15)：2026-09-11停止维护；不下载旧二进制。
- [README.md:65–87](https://github.com/ttalvitie/browservice/blob/02601b047f7abdc90545c9dc669d0081d7968c34/README.md#L65-L87)：网页渲染、剪贴板、下载、字体和控制栏的说明。
- [src/control_bar.cpp:685–735](https://github.com/ttalvitie/browservice/blob/02601b047f7abdc90545c9dc669d0081d7968c34/src/control_bar.cpp#L685-L735)：地址与历史导航控制栏的布局。
- [src/browser_area.cpp:1–105](https://github.com/ttalvitie/browservice/blob/02601b047f7abdc90545c9dc669d0081d7968c34/src/browser_area.cpp#L1-L105)：浏览器画面/事件区域。

## K01

[Changkai9888/CloudZao-Projects](https://github.com/Changkai9888/CloudZao-Projects/tree/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8) · `90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8` · C++字体组件、接入协议、源码交付包。

- [README.md:7–33](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/README.md#L7-L33)：区分口袋、具体应用实验和各类交付物。

## R05

[dosyago/BrowserBox](https://github.com/dosyago/BrowserBox/tree/82cc396fffcd223cef603b5cf6c1c933dc42fd71) · `82cc396fffcd223cef603b5cf6c1c933dc42fd71` · 公开文档/交付入口；未确认当前实现源码。

- [README.md:26–33](https://github.com/dosyago/BrowserBox/blob/82cc396fffcd223cef603b5cf6c1c933dc42fd71/README.md#L26-L33)：当前商业软件说明，不能代替实现源码。

## R04

[m1k1o/neko](https://github.com/m1k1o/neko/tree/c7266f93c75be5c913914349093f8692624cfb74) · `c7266f93c75be5c913914349093f8692624cfb74` · Go、TypeScript / GStreamer、WebRTC；前端客户端。

- [README.md:1–100](https://github.com/m1k1o/neko/blob/c7266f93c75be5c913914349093f8692624cfb74/README.md#L1-L100)：完整远程浏览器/虚拟桌面项目说明。
- [client/core/src/client.ts:1–115](https://github.com/m1k1o/neko/blob/c7266f93c75be5c913914349093f8692624cfb74/client/core/src/client.ts#L1-L115)：远程连接和输入传输的客户端接口。
- [client/core/README.md:1–100](https://github.com/m1k1o/neko/blob/c7266f93c75be5c913914349093f8692624cfb74/client/core/README.md#L1-L100)：WebSocket/WebRTC与挂载控件说明。
