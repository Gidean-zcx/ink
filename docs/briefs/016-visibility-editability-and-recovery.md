# 第十六期：可见、可改与可恢复

2026-10-27。本轮深化Sony DPT-RP1/CP1、QUADERNO Gen.3C和HUAWEI MatePad Paper。三者都能处理文档，但文件所在位置、显示状态、编辑条件与恢复范围不同。以下是文档和静态图研究，没有设备操作，也不构成已经认可的产品设计。

## 看见内容，不等于已经保存了可恢复的副本

Sony的电脑App双击设备文档，会在电脑PDF阅读器中打开只读临时副本；关闭配套App后，该副本被删除。永久传出文件和显示设备画面另有入口。因此“电脑上打开过”不能作为备份完成的证据。[官方只读查看说明](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178341.html)

QUADERNO传出到手机后，接收App可能不显示批注；官方建议改用支持批注的阅读器。MatePad Paper则有另一种情况：部分贴纸与背景转换成相近灰度，内容实际仍在，但看起来消失。两者都提示应先区分内容缺失与呈现问题，不能立即重复导入或判定同步失败。[QUADERNO手机传出](https://www.fmworld.net/digital-paper/mobileapp/helpguide/contents/QHG00007.html)、[华为灰度显示问题](https://consumer.huawei.com/cn/support/content/zh-cn16009712/)

可复用任务：Sony F-M059-20/21，QUADERNO F-M060-10，华为 F-M061-13。灰度问题有笔记App版本与可改色贴纸条件，不代表所有缺图都由颜色造成。

## 同样叫“删除”或“恢复”，对象与路径可能不同

| 操作 | 本轮查明的条件 | 对后续设计的问题，尚未定案 |
| --- | --- | --- |
| 从设备删文件、从电脑删文件 | QUADERNO设备回收站可恢复部分删除；PC App删除及指定同步删除路径不经过设备回收站。 | AI解释恢复可能性时，需要知道动作来源、传播路径及现存副本。 |
| 擦笔迹、删笔记页 | Sony橡皮可删整条笔画，换页会清空撤销；笔记删页则被手册明确标为不可恢复。 | 擦除范围、顺序撤销、页面删除和备份回退分别说明，不能共用“撤销即可”。 |
| 修复损坏文档 | QUADERNO的特定恢复入口针对曾升级2.1.00.26250FP造成的问题，可生成一个或两个修复副本并保留原文件。 | “修复完成”仍需核对正确副本；不能把专项修复工具当通用数据恢复。 |

依据：[QUADERNO指南](https://www.fmworld.net/digital-paper/support/download/pdf/B5FL2101.pdf)第46–47、99–102、121页；[Sony擦除](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178309.html)与[笔记页管理](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178283.html)。对应F-M060-04/08/16与F-M059-15/17；实际误删和恢复结果未验证。

## “继续刚才”要保留正确的位置和运行条件

Sony把目录、页码和文内链接跳转记入返回历史，普通划页不记。华为区域版帮助中的录音标记则定位音频时间点，并允许移动或删除；尚无依据证明它会同时定位对应笔迹或转写句子。阅读位置、音频位置和撤销历史不能因为都叫“刚才”就合成一个状态。[Sony文档工具栏](https://helpguide.sony.net/dpt/rp1/v1/en/contents/TP0001178330.html)、[华为录音标记](https://consumer.huawei.com/nz/support/content/en-gb15879066/)

QUADERNO手机直连使用Wi-Fi；切到其他App会断开直连。其“稍后批量传入”先建立待传列表，尚未等于文件进入设备。华为同步排查也明确区分账号、前台联网、版本、空间和电量等条件；这些排查步骤不是成功保证。[QUADERNO连接帮助](https://www.fmworld.net/digital-paper/mobileapp/helpguide/contents/QHG00001.html)、[华为同步排查](https://consumer.huawei.com/cn/support/content/zh-cn15931676/)

对系统AI的启发是：在报告“找到了、保存了、恢复了”之前，核查相应内容对象、位置、模式与实际结果。具体接口和存储方式仍待产品方向认可后决定。

## 本批资料怎样复用

- [Sony案例](../research/interaction-library/cases/M-059-sony-dpt.md)：文档导航、笔画编辑、模板与笔记页、表单、临时副本、连接和复位。
- [QUADERNO案例](../research/interaction-library/cases/M-060-quaderno.md)：手机收发、标记检索、关联笔记、扫描和特定版本恢复。
- [华为案例](../research/interaction-library/cases/M-061-huawei-paper.md)：笔记录音、同步、跨端文件、PDF显示调整、系统手势。

各案例都有逐项来源目录、F任务与E证据。Sony固定1.6指南；QUADERNO的官网公开日期2026-04-01与PDF内发行月2025-04分开记录；华为大陆与区域帮助的型号、系统和App条件保留。没有把同品牌或相近产品的规则互相补齐。

本批继续补回原任务，不把目录条目数当功能完成数。所有65组仍为部分覆盖；实际触控、手写效果、断电恢复和当前安装版本仍需更强证据。文件与附件检查见[核查记录](../research/interaction-library/verification-log.md)。
