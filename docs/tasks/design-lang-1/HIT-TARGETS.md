# 点按检查结果

【实测·模拟器】检查 799 个有效控件或原生矩阵按键，257 个宽或高不足 108。命令返回 1，未通过。两档分别为 100%（后缀 1）和 120%（后缀 3）。

| 页面及字号档 | 不足数量 | 最小宽 | 最小高 | 能力或用途举例 |
| --- | ---: | ---: | ---: | --- |
| Ai-1 | 1 | 231 | 43 | 星星邮差 · 目录 |
| Ai-3 | 1 | 275 | 52 | 星星邮差 · 目录 |
| Calendar-1 | 30 | 86 | 108 | calendar.select |
| Calendar-3 | 30 | 86 | 108 | calendar.select |
| Clock-1 | 3 | 132 | 107 | clock.timer、clock.stopwatch、clock.world |
| Clock-3 | 3 | 129 | 107 | clock.timer、clock.stopwatch、clock.world |
| ClockTimer-1 | 1 | 304 | 107 | shell.back |
| ClockTimer-3 | 1 | 304 | 107 | shell.back |
| ImePinyin-1 | 43 | 55 | 90 | 今天、进、今、近 |
| ImePinyin-3 | 39 | 58 | 108 | 进、今、近、金 |
| MemoEdit-1 | 36 | 58 | 90 | text.voice_start、keyboard.key |
| MemoEdit-3 | 35 | 58 | 108 | text.voice_start、keyboard.key |
| Phone-1 | 3 | 158 | 107 | phone.open、phone.recents、phone.contacts |
| Phone-3 | 3 | 158 | 107 | phone.open、phone.recents、phone.contacts |
| PhoneCall-1 | 4 | 125 | 43 | 静音、键盘、耳机未连、录音 |
| PhoneCall-3 | 4 | 125 | 52 | 静音、键盘、耳机...、录音 |
| PhoneRecents-1 | 3 | 158 | 107 | phone.open、phone.recents、phone.contacts |
| PhoneRecents-3 | 3 | 158 | 107 | phone.open、phone.recents、phone.contacts |
| SetDisplay-1 | 4 | 152 | 107 | settings.set |
| SetDisplay-3 | 4 | 152 | 107 | settings.set |
| SetNetwork-1 | 3 | 202 | 107 | settings.set |
| SetNetwork-3 | 3 | 202 | 107 | settings.set |

完整坐标、标签、能力和控件深度在 `build/design-lang-1/after/targets.json`。新采集驱动不把没有回调的装饰性电量格和信号格算成控件；原生键盘按每个有效按键检查。
这是画布与祖先容器裁切后的几何检查，不能证明相邻控件重叠时仍有同样大的实际命中区域，也不包含被页面覆盖的像素级遮挡分析。此次已有 257 项不达标，不将结果写成通过。
