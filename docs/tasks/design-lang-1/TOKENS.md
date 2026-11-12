# 主题令牌变化

【实现】单位为 684×1216 画布上的逻辑像素，文字设置在其后生效。
`INK_FONT_LIST` 数值保留 36，以兼容阅读器公开的 `font_size=36` 合同；共用字体入口将其归一为 `INK_FONT_BODY=38`，界面共用同一字体实例。中粗源字体仍由现有生成器生成。状态与注记合为 31，列表、正文、小标题实际渲染合为 38。53、72、106、211 保留。

| 令牌 | 改前 | 改后 |
| --- | ---: | ---: |
| `INK_STATUS_BAR` | 62 | 108 |
| `INK_FONT_STATUS` | 29 | 31 |
| `INK_FONT_SUBTITLE` | 43 | 38 |
| `INK_LINE_STATUS` | 38 | 43 |
| `INK_LINE_LIST` | 53 | 62 |
| `INK_LINE_SUBTITLE` | 58 | 62 |
| `INK_CIRCLE` | 101 | 108 |
| `INK_CONTROL` | 101 | 108 |
| `INK_PRIMARY` | 125 | 108 |
| `INK_ROW_DOUBLE` | 134 | 136 |
| `INK_ROW_TRIPLE` | 新增 | 164 |
| `INK_GROUP_TITLE` | 新增 | 64 |
| `INK_FIELD_HEIGHT` | 101 | 108 |
| `INK_FIELD_PAD` | 19 | 24 |
| `INK_FIELD_LABEL_GAP` | 10 | 8 |
| `INK_FIELD_RADIUS` | 29 | 16 |
| `INK_FIELD_CLEAR` | 106 | 108 |
| `INK_FIELD_STROKE` | 3 | 2 |
| `INK_FIELD_FOCUS_GAP` | 5 | 0 |
| `INK_SEGMENT` | 115 | 108 |
| `INK_SLIDER` | 115 | 108 |
| `INK_GAP_XS` | 5 | 8 |
| `INK_GAP_S` | 10 | 8 |
| `INK_GAP_M` | 14 | 16 |
| `INK_GAP` | 19 | 24 |
| `INK_GAP_L` | 29 | 32 |
| `INK_GAP_XL` | 38 | 48 |
| `INK_GAP_XXL` | 58 | 64 |
| `INK_BLOCK` | 77 | 64 |
| `INK_BLOCK_L` | 115 | 108 |
| `INK_RADIUS_KEY` | 29 | 16 |
| `INK_RADIUS_CARD` | 48 | 16 |
| `INK_RADIUS_PANEL` | 67 | 32 |
| `INK_STROKE` | 3 | 2 |
| `INK_STROKE_FOCUS` | 新增 | 3 |
| `INK_DIVIDER` | 新增 | 2 |
| `INK_SWITCH_W` | 新增 | 96 |
| `INK_SWITCH_H` | 新增 | 56 |
| `INK_SWITCH_ON` | 新增 | 40 |
| `INK_SWITCH_OFF` | 新增 | 24 |
| `INK_SLIDER_TRACK` | 新增 | 2 |
| `INK_SLIDER_FILL` | 新增 | 8 |
| `INK_SLIDER_KNOB` | 新增 | 32 |
| `INK_PAGE_DOT` | 新增 | 8 |

共用样式：静态表面与卡片不自动画框；图标透明无圆底；主、次、文字三种按钮均至少 108 高，圆角 16；搜索与普通输入统一矩形，焦点同一路径加粗到 3；轨道为 96×56，开与关的钮分别为 40 和 24；分段只留一圈框；五页以内圆点，其余 2/7；空状态居中、至多一个文字动作。
次要文字色从黑改为 `#666666`；分隔线独立令牌为 2 px、`#888888`，从文字起点缩进。
单行输入横向内边距 24；108 高容器配 62 行高，纵向居中，不能同时保留上下各 24 加描边。多行输入上下留 24。
