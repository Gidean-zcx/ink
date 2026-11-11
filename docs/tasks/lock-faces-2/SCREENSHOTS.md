# 截图索引

模拟器 `clean-` 帧的原始尺寸参考图，没有缩放或改像素。测试走真实设置定义；100% 对应字号 1，120% 对应字号 3。图片只用合成壁纸与合成成员资料。

| 页面／表盘 | 100% | 120% |
| --- | --- | --- |
| 带预览的单选列表及小组件开关 | [图](screenshots/picker-size-1.png) | [图](screenshots/picker-size-3.png) |
| 这一页（0） | [图](screenshots/face-0-size-1-bg-0.png) | [图](screenshots/face-0-size-3-bg-0.png) |
| 时钟和待办（1） | [图](screenshots/face-1-size-1-bg-0.png) | [图](screenshots/face-1-size-3-bg-0.png) |
| 壁纸（2） | [图](screenshots/face-2-size-1-bg-0.png) | [图](screenshots/face-2-size-3-bg-0.png) |
| 简约时钟（3） | [图](screenshots/face-3-size-1-bg-0.png) | [图](screenshots/face-3-size-3-bg-0.png) |
| 待机时钟（4） | [图](screenshots/face-4-size-1-bg-0.png) | [图](screenshots/face-4-size-3-bg-0.png) |
| 月历（5） | [图](screenshots/face-5-size-1-bg-0.png) | [图](screenshots/face-5-size-3-bg-0.png) |

`bg-0` 指表盘叠加开关关闭；壁纸样式（2）仍按原规则显示已选择的图片，尚未选图时回退到“这一页”。矩阵先跑 100% 再跑 120%，后者已经有前一轮加载的合成图片。

补充：[跨日进入闰二月](screenshots/leap-month.png)、[换到孩子](screenshots/child.png)、[成人空间关闭](screenshots/adult-closed.png)、[本人验证后继续第 5 页](screenshots/reading-resumed.png)、[英文月历和节气](screenshots/calendar-en.png)、[英文选择页](screenshots/picker-en.png)。

20 张，单张最大 53,583 字节，均小于 300 KB。证明模拟器绘制与接线结果；设备残影和刷新耗时仍需实机核对。
