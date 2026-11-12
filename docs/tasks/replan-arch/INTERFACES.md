# 兼容接缝和行数估算

以下是【实现】能点名的契约及【估算】接线量，不是已经实现的适配器。按方法名计数，不把重载另算；没有把一个头文件算成一个接口。自动枚举结果在 [source-evidence.json](source-evidence.json)；编号对应 [源码索引](SOURCES.md)。估算只计新增或修改的适配代码，不计原文复用的源码、资源、测试和文档，不承诺编译后的大小。

## 三种做法共有的平台契约

12 个入口：`ink_app_register`、`ink_cap_register`，活动的 `create`、`destroy`、`on_key`、`on_swipe`、`snapshot`、`settle`，`ink_ui_push`、`ink_ui_pop`、`ink_ui_semantics_snapshot`、`ink_ui_semantics_action`。另有 8 个可选生命周期回调：`on_start`、`on_foreground`、`on_background`、`on_freeze`、`on_resume`、`on_stop`、`save_state`、`restore_state`。共 **20 个可点名契约**，不是每个应用都要实现全部可选回调。依据 O01:36–79、O02:25–37,64–93、O03:121、O04:50–56。

能力数量按实际动作另计。例如现有正文能力表已有 **14 项**（O12:601–624），兼容接入不能只注册一个“点击三方界面”。新增书摘、统计、分类等动作仍要逐项登记；本任务没有把它们伪装成已经完成的 14 项。

## 甲：移入现有 LVGL 页面

不需要模拟三方显示 ABI。解析、分页、元数据等模块需要接文件、内存、字体与业务状态；用上面的 20 个平台契约组织页面。一个含书架、正文、目录、书签、排版、详情的六页样本，估算如下：

| 接缝 | 适配代码行数【估算】 | 依据与假设 |
| --- | ---: | --- |
| 注册及生命周期共用部分 | 200–400 | O01/O02；继续用现有应用管理器 |
| 六页布局、命中及交互重做 | 1,500–2,400 | 每页 250–400 行；原三方布局不能原样保留 |
| 保留三方业务/排版内核的接线 | 300–600 | K13 已区分分页、绘制和推屏 |
| 能力、快照与成员接线 | 400–800 | O03–O06；不以截图代替语义 |
| 合计 | **2,400–4,200** | 不覆盖全部 72 项功能，也不是完整移植报价 |

## 乙一：保留 CrossPoint 的 GfxRenderer / Activity

**显示至少 22 个方法名**，GfxRenderer 的实际 `display.*` 调用（C05，注释已排除）：

| 分组 | 方法 |
| --- | --- |
| 几何和帧缓冲，7 | `getDisplayWidth`、`getDisplayHeight`、`getDisplayWidthBytes`、`getBufferSize`、`getFrameBuffer`、`lendFrameBufferStorage`、`returnFrameBufferStorage` |
| 基本绘制/极性，3 | `clearScreen`、`drawImage`、`isInverted` |
| 提交与完成，4 | `displayBuffer`、`displayBufferAsync`、`waitRefreshComplete`、`supportsAsyncRefresh` |
| 灰阶，8 | `grayscaleCapabilities`、`displayGrayscaleBase`、`preconditionGrayscale`、`copyGrayscaleLsbBuffers`、`copyGrayscaleMsbBuffers`、`displayGrayBuffer`、`writeGrayscalePlaneStrip`、`cleanupGrayscaleBuffers` |

这 22 个只覆盖 GfxRenderer 的实际调用面。其他活动还可能直接用 C06 的 `refreshDisplay`、`setInverted`、`toggleInverted`、`deepSleep`、初始化及能力查询，不能将它当完整固件 ABI。两种灰阶 API 有重载。休眠、屏电源不能直通硬件。

**输入公开面 34 个方法名**（C07:49–131）：`update`、`powerSleepClick`、`wasPowerSingleClick`、`resetPowerButtonInput`、`injectRemoteClick`、`hasRemoteClick`、`wasPressed`、`wasReleased`、`wasLongPressed`、`consumeSuppressedRelease`、`isPressed`、`hasTouch`、`wasScreenTapped`、`wasScreenTouchPressed`、`wasScreenTouchDown`、`wasScreenLongPress`、`isScreenTouchHeld`、`wasScreenTouchReleased`、`rowTouch`、`colTouch`、`wasSwipe`、`wasBackGesture`、`wasHomeGesture`、`homeButtonAction`、`resetHomeButtonInput`、`wasMenuGesture`、`wasReaderMenuSwipeUp`、`wasLightPanelGesture`、`getHeldTime`、`getRenderer`、`mapLabels`、`mapDirectionalLabels`、`getPressedFrontButton`、`isNavDirectionSwapped`。可以保留其坐标/命中算法，只替换取事件的一端；无需从头实现 34 套算法。

**Activity 面 21 个方法名**（C01:31–72）：`onEnter`、`onExit`、`prepareForSleep`、`loop`、`render`、`requestUpdate`、`requestUpdateAndWait`、`skipLoopDelay`、`preventAutoSleep`、`requiresExclusiveStorageLoop`、`isReaderActivity`、`acceptsRemoteInput`、`handleForcedRefresh`、`isHomeActivity`、`handleHomeGesture`、`getScreenshotInfo`、`startActivityForResult`、`setResult`、`finish`、`onGoHome`、`onSelectBook`。查询可直接保留；调度、导航、阻塞等待必须接回现有 UI 活动栈/执行器。不能原样启动 C03 的第二个渲染任务。

文件还需适配 C08 的 `HalStorage/HalFile`：打开、读写、定位、大小、同步、关闭、目录迭代、建目录、存在判断、改名、替换、删除等；完整头还有 USB 独占和容量接口。**没有核完所有书库、字体、网络、插件模块的传递闭包，不能给出完整应用的精确接口总数。** 已枚举的三个直接面为 **77 个方法名（22+34+21）**，再加共用平台契约、文件/设置等，不是一个十来个函数的垫片。

| 接缝 | 适配代码行数【估算】 |
| --- | ---: |
| 共用画面承载、平台契约 | 250–450 |
| 灰阶、帧缓冲、统一提交 | 450–850 |
| 输入事件桥 | 180–320 |
| 活动栈和调度桥 | 180–350 |
| 文件、设置、内存与时钟接线 | 600–1,100 |
| 一条阅读链的动作与快照 | 300–700 |
| 系统语言、字号/主题接线 | 100–250 |
| 合计 | **2,060–4,020** |

另一条较窄的入口是固定 SDK 的 **DrawTarget 11 方法**：`clipRect`、`setClipRect`、`measureText`、`lineHeight`、`inkBounds`、`fill`、`stroke`、`line`、`triangle`、`text`、`bitmap`（F01:709–746）。已有 `FreeInkApp::on/route` 和屏幕描述可继续用（F02:875–985）；接上现有灰度画布/字体预计 250–500 行。**这只覆盖 FreeInkUI 控件绘制，不能替代正文 GfxRenderer、文件、Activity 或 AI 适配，不能拿 11 当整套 CrossPoint 的数量。**

## 乙二：保留 kiikoread 的页面、ui_kit 和阅读内核

**页面回调 23 个**（K01:113–180）：`grayscale_ui`、`navigation_enabled`、`menu_handle_enabled`、`main_page_visible`、`edge_back_enabled`、`back_swipe_area`、`on_enter`、`on_exit`、`on_media_lost`、`on_media_ready`、`on_before_lock`、`on_power_short`、`render`、`present`、`on_gesture`、`wants_pinch`、`on_pinch`、`on_touch`、`on_key`、`on_main_key`、`on_key_long`、`on_tick`、`area_hint`。不是全都绘制函数；其中电源、媒体、页面请求必须由平台主导。

抽样 `ui_kit.c + book_layout.c + app_book.c` 的 **14 个 epd 方法名**：`epd_draw_circle`、`epd_draw_line`、`epd_draw_pixel`、`epd_draw_rotated_image`、`epd_fill_circle`、`epd_fill_circle_helper`、`epd_fill_rect`、`epd_get_pixel`、`epd_get_rotation`、`epd_height`、`epd_poweron`、`epd_rotated_display_height`、`epd_rotated_display_width`、`epd_width`。像素原语可保留为纯内存绘图，`epd_poweron` 只能请求平台，不能操作面板电源。

正文还直接涉及 **23 个 display 方法名**（K04；完整列表也在 JSON）：`display_main_gray_waveform`、`display_main_normal_waveform`、`display_main_transition_cover_bands`、`display_main_transition_gray_bands`、`display_main_transition_shelf_page`、`display_page_prepare_begin`、`display_page_prepare_end`、`display_pipeline_enabled`、`display_reader_fast_panels_enabled`、`display_reader_panel_gray4_prepare`、`display_reader_soft_turn_waveform`、`update_display_area_diff_with`、`update_display_area_full_with`、`update_display_area_with`、`update_display_fast_page`、`update_display_full`、`update_display_mode_diff`、`update_display_reader_panel`、`update_display_reader_panel_fast`、`update_display_reader_panel_gray4`、`update_display_text_turn`、`update_display_water_turn`、`update_display_with`。这些包含分阶段推屏和波形查询，**不能全部返回成功假装已显示**。要把最终像素交给统一合成，完成语义保持“画面已接纳/物理显示完成”的区别。

已列直接面 **60 个名字（23+14+23）**；另外还有文件/NVS、字体、内存、时间、后台预绘、网络和动作语义，60 不是完整依赖总数。K04:124–196 的头文件闭包和 :3868–3889 的预绘线程证明还有这些接缝。

| 接缝 | 适配代码行数【估算】 |
| --- | ---: |
| 共用画面承载、平台契约 | 250–450 |
| 页面回调桥 | 200–350 |
| packed 4bpp/旋转/纯内存绘图 | 350–650 |
| 提交、阶段合并及刷新入口 | 300–600 |
| 文件、成员进度与设置 | 450–900 |
| 预绘及耗时工作接现有执行器 | 200–450 |
| 动作与快照 | 300–700 |
| 文案语言、字号与命中布局 | 200–500 |
| 合计 | **2,250–4,600** |

## 丙：让 kiikoread 主循环拥有整机

保留原生 23 个页面回调和纯绘图部分，不需要把它们重新写一遍；但仍需重接上述 **23 个显示入口**以满足 D-127，并保留上述 **20 个平台契约**或等价桥，所以能明确点名的下界为 **43 个**。此外至少有 6 组装配域：启动/执行器，外壳/浮层/输入法，电话/音频，中枢/网络，成员/持久数据，升级/日志。这里的“6 组”不等于 6 个函数，完整闭包与精确总数不确定。

【估算】6 组各 400–800 行接线和行为调整，加显示 300–600 行、全局成员状态收口 800–1,500 行，合计 **3,500–6,900 行**；不计旧页面转换、保留原文代码和测试。主要未知是通话打断/恢复、冻结/省电与原生后台任务能否共享所有权；单靠把几个服务启动起来，不能验收这条路线。
