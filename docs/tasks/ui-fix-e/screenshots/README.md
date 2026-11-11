# 前后截图索引

全部由无窗口模拟器和合成数据生成；前图使用任务基线的四个原始产品文件，后图使用本次实现。120% 为实际设置 `display.font_size=3`，不是缩放图片。单张 PNG 最大 40,288 字节。

| 页面 | 前 100% | 后 100% | 前 120% | 后 120% |
| --- | --- | --- | --- | --- |
| 家人 | [before 100%](before-100-family.png) | [after 100%](after-100-family.png) | [before 120%](before-120-family.png) | [after 120%](after-120-family.png) |
| 活动记录 | [before 100%](before-100-records.png) | [after 100%](after-100-records.png) | [before 120%](before-120-records.png) | [after 120%](after-120-records.png) |
| 孩子的应用名单 | [before 100%](before-100-kid-apps.png) | [after 100%](after-100-kid-apps.png) | [before 120%](before-120-kid-apps.png) | [after 120%](after-120-kid-apps.png) |
| 禁用时段 | [before 100%](before-100-kid-periods.png) | [after 100%](after-100-kid-periods.png) | [before 120%](before-120-kid-periods.png) | [after 120%](after-120-kid-periods.png) |
| 对话记录空态 | [before 100%](before-100-history-empty.png) | [after 100%](after-100-history-empty.png) | [before 120%](before-120-history-empty.png) | [after 120%](after-120-history-empty.png) |
| 对话记录 | [before 100%](before-100-history.png) | [after 100%](after-100-history.png) | [before 120%](before-120-history.png) | [after 120%](after-120-history.png) |
| 文件位置 | [before 100%](before-100-files.png) | [after 100%](after-100-files.png) | [before 120%](before-120-files.png) | [after 120%](after-120-files.png) |
| 多选未选文件 | [before 100%](before-100-files-multi-empty.png) | [after 100%](after-100-files-multi-empty.png) | [before 120%](before-120-files-multi-empty.png) | [after 120%](after-120-files-multi-empty.png) |
| 多选已选文件 | [before 100%](before-100-files-multi.png) | [after 100%](after-100-files-multi.png) | [before 120%](before-120-files-multi.png) | [after 120%](after-120-files-multi.png) |
| 拷贝多个文件 | [before 100%](before-100-files-copy.png) | [after 100%](after-100-files-copy.png) | [before 120%](before-120-files-copy.png) | [after 120%](after-120-files-copy.png) |
| 移动单个文件 | [before 100%](before-100-files-move.png) | [after 100%](after-100-files-move.png) | [before 120%](before-120-files-move.png) | [after 120%](after-120-files-move.png) |
| 文件夹空态 | [before 100%](before-100-files-empty.png) | [after 100%](after-100-files-empty.png) | [before 120%](before-120-files-empty.png) | [after 120%](after-120-files-empty.png) |
| 文件夹读取失败 | [before 100%](before-100-files-error.png) | [after 100%](after-100-files-error.png) | [before 120%](before-120-files-error.png) | [after 120%](after-120-files-error.png) |
| 新建文本名称 | [before 100%](before-100-file-name.png) | [after 100%](after-100-file-name.png) | [before 120%](before-120-file-name.png) | [after 120%](after-120-file-name.png) |
| 文件操作 | [before 100%](before-100-file-actions.png) | [after 100%](after-100-file-actions.png) | [before 120%](before-120-file-actions.png) | [after 120%](after-120-file-actions.png) |
| 文本草稿和写回 | [before 100%](before-100-file-edit.png) | [after 100%](after-100-file-edit.png) | [before 120%](before-120-file-edit.png) | [after 120%](after-120-file-edit.png) |
| 选图片 | [before 100%](before-100-picker-images.png) | [after 100%](after-100-picker-images.png) | [before 120%](before-120-picker-images.png) | [after 120%](after-120-picker-images.png) |
| 选取器无匹配 | [before 100%](before-100-picker-empty.png) | [after 100%](after-100-picker-empty.png) | [before 120%](before-120-picker-empty.png) | [after 120%](after-120-picker-empty.png) |
| 选取器读取失败 | [before 100%](before-100-picker-error.png) | [after 100%](after-100-picker-error.png) | [before 120%](before-120-picker-error.png) | [after 120%](after-120-picker-error.png) |

时段双值经真实能力和家长密码确认保存后的补充截图：[100%](after-100-kid-periods-saved.png)、[120%](after-120-kid-periods-saved.png)。

文件写入失败回执注入：仅撤掉错误提示修复的反向对照图 [100%](reverse-100-file-write-failed.png)、[120%](reverse-120-file-write-failed.png)；修复后 [100%](after-100-file-write-failed.png)、[120%](after-120-file-write-failed.png)。该组反向图的其余代码保持修复后版本。
