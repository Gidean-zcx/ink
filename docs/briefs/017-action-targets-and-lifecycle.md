# 第十七期：动作作用于哪里，数据留在哪里

2026-10-27。本轮深化联想Smart Paper、山灵M1 Plus与iBasso DX180，重点是控件作用对象、应用生命周期、计时和恢复。资料仍来自文档、版本说明与静态页，没有设备操作；以下启发尚未成为本项目设计。

## 配套App里的设置，不一定控制设备

山灵Eddict Player手册明确：Playback Settings作用于App本地播放，不影响连接的山灵设备。页面中的Timed off、Resume mode，不能作为M1 Plus定时关机或精确续播的证据。SyncLink才是另一组远程控制入口，其通用说明还需核对具体机型、网络方式和版本。[官方Eddict手册](https://shanling.com/uploads/image/EddictPlayerAPP.pdf)第12、15页。

DX180也需要分层：日本经销方的V1.04公告把系统关机计时与Mango播放停止计时分开；中国日志中的对应措辞存在疑误，资料库保留原文差异。后续“始终有效”选项虽已被列入版本记录，跨重启、后台或切换播放任务的完整规则仍未知。[MUSIN公告](https://prtimes.jp/main/html/rd/p/000000144.000139013.html)、[iBasso版本记录](https://ibasso.cn/support/dx180firmware)。

这对“孩子听完这一段就停止”很关键：停止手机App、停止播放器声音、让整机休眠和整机关机，是不同结果。AI需要能确认作用对象及完成状态；具体实现仍待后续设计。

## 可安装、可删除、可恢复是三件事

DX180指南分别给出商店安装、浏览器下载后安装和系统设置卸载的步骤，但未交代卸载后用户数据保留及重装恢复。联想FAQ则指出定制WPS的卸载后重装限制。不能据“支持APK”推导所有App都能按同一种方式恢复。[DX180指南](https://www.ibasso.com/uploadfiles/download/DX180userguide.pdf)、[联想官方FAQ](https://support.lenovo.com/ca/en/solutions/ht514875-lenovo-smart-paper-lenovo-sp101fu-frequently-asked-questions-faqs)。后者本轮使用官方区域搜索索引，直接受限页面未重试。

联想还有一个有用的排查分支：部分“文件消失”被FAQ解释为账号状态导致隐藏，重新登录原账号可重新显示。这不能替代误删、损坏或云端恢复，但提醒我们区分“当前不可见”和“数据不存在”。对应F-M062-18/19与F-M064-13/14。

## 细节必须沿任务保存，不能压缩成一个“支持”

| 设计时会遇到的问题 | 本轮依据 | 尚不能承诺的部分 |
| --- | --- | --- |
| 写错、删页或合并图层后如何退回？ | 联想指南列十步撤销及独立页/图层工具。 | 没有说明哪些结构操作进入撤销历史，不能保证都能撤回。 |
| 锁屏后是否仍显示学习内容？ | 联想可显示最近浏览的速记页；时钟每分钟刷新。 | 与密码遮盖、家庭成员切换及实际耗电的关系仍待核。 |
| 让AI找到正在听的内容，是否就是找到断点？ | DX180曲目搜索、分类、当前文件定位各有入口。 | 文件位置、队列位置和音频时间不能互换；跨重启状态还要独立核验。 |
| 手机能遥控播放器，是否也能替它设置所有参数？ | Eddict区分本地播放设置与SyncLink控制。 | 通用App功能不自动成为M1 Plus的全部设备能力。 |

联想依据：[2023指南](https://download.lenovo.com/consumer/mobiles_pub/lenovo_smart_paper_ug_en_2023.pdf)第9、16–17页；旧指南不能视为当前固件实测。其他条目的逐页和版本定位见下列案例。

## 后续怎样查回依据

- [联想案例](../research/interaction-library/cases/M-062-lenovo-paper.md)：初次设置、手写编辑、分享、书库限制、锁屏、投影、应用与账号可见性；附目录台账和已查看的控件页。
- [山灵案例](../research/interaction-library/cases/M-063-shanling.md)：设备、MTouch与Eddict的范围分开，旧任务补证；计时和断点没有因为找到同名菜单就宣告完成。
- [iBasso案例](../research/interaction-library/cases/M-064-ibasso.md)：安装/卸载、USB供电方向、检索与排序、系统和Mango计时的版本差异。

查阅顺序为“设计问题→F任务→E证据→原文页码/版本”。功能目录保留未分析项与重叠父项；案例继续列出异常、恢复和实际体验的缺口。全部65组仍是部分覆盖，不能把目录行数或文档任务数当作全功能完成率。
