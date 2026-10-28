# M7：设置、实际模式与应用按键

查阅2026-10-27。五个公开来源，不是五次设备验证。没有操作M7、安装KOReader或修改按键文件；没有新图像观察。旧指南/销售商日志与本批历史作者体验、2026问题报告分别保存。

## E-MRCD-01：Helpix，2024-09-07，Alex

[原文](https://www.helpix.ru/opinion/202409/60517-meebook_m7.html)，定位Использование/Софт，暂存文本713–1045行。本次HTTP200；此前web再次定位超时，普通原址取回成功。作者接触过不同M7，未给各自系统构建。

作者描述下拉面板选Regal/Normal/Fast/A2，以顶栏模式标识查看状态；应用图标长按可优化，但他报告选定模式与进入App后的模式不一致，原因未定。User Settings的Hide Store和首页样式、Read返回书库有文字路径。原生Notes与第三方读书记录不同；文中的集中摘录是朋友制作的工具，不是原生导出。Wi-Fi传书的私网地址仅是作者实例；Factory reset只给入口，未给确认和恢复链。

原页选定七张相关图，第一张所用cdn1.picturebb.com发生DNS解析失败，随即停下，其余未请求，故没有截图支持。图像失败与型号版本差异见[采集记录](acquisition.json)、[图像记录](images-acquisition.json)。未采用作者的续航归因、厂商关系猜测、性能或灰阶效果作为技术结论。

## E-MRCD-02：KOReader问题16100，2026-09-22

[原始报告与回应](https://github.com/koreader/koreader/issues/16100)。作者自报M7/Android11、KOReader2026.07.1：开EPUB后按键不翻页，其他程序能识别92/93。成员给出291/292到RPgBack/RPgFwd的映射，说明自己无M7；报告者随后反馈有帮助并关闭问题。没有日志和完整配置前后录像。两组数字不可直接等同，不能据关闭状态断言所有M7已兼容，亦不证明上游代码已修复。

## E-MRCD-03/04：项目Wiki的应用内映射方法

[Keymapping](https://github.com/koreader/koreader/wiki/Keymapping)显示编辑日2026-10-13：在koreader/settings/event_map.lua为整数事件关联动作，按设备查实际键。只读定义与方法段，不套Kobo/PocketBook示例。

[Android tips and tricks](https://github.com/koreader/koreader/wiki/Android-tips-and-tricks)显示编辑日2026-03-17：不同设备可能使用不同键码，非launcher应用不能覆盖Home行为。这是项目文档范围；未核M7系统长按、Logo键角色或每个App。没有执行配置或验证重启加载/冲突/回退。

## E-MRCD-05：作者自配方案，不是内置产品能力

[Bluemoondragon07的M7体验及首条补充](https://www.reddit.com/r/eink/comments/1825l9d/meebook_m7_review_my_optimizations/)，历史帖、精确日期/固件未核。先由web工具读取正文与评论；之后原址普通GET403，停止，不尝试API/其他域名或登录。拒绝HTML不是正文原件。

作者另用书库管理器把书交给KOReader；并描述用外部按键映射器记录触发键、指定滑动动作、限制前台App、保存，再复制反向动作。软件名称/版本及权限初始化未核，坐标属于其设备配置，不作为通用方案；未证明原生书库共享进度。只保留文本记录，不声称看过帖内图片或亲自配置。

## 取用结论

F04补同型号问题与应用侧方法，仍D2、非实测；F02/05/06增加范围限定，不升级D1。新增F07仅为历史文字重建的刷新模式任务。首页、原生笔记导出、重置确认/恢复等只进聚焦目录，仍缺完整结果。系统配置、App实际状态和动作结果值得分别观察，这是给Plus原型的建议，不承诺相同兼容性或刷新效果。
