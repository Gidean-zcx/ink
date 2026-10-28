# Anki：结束一轮、重建额外练习与统计范围

查阅2026-10-27。复读此前取得的官方Desktop手册原件，没有本轮网络请求、安装或操作；具体构建未知，仍是D2。旧E/F沿用，原件和原HTTP记录见工作目录work/research/batch32-anki；正式摘记只保存本次补到的行为与未知。

| 证据及原页 | 正文位置 | 有依据的操作/状态 | 仍不能证明 |
| --- | --- | --- | --- |
| E-ANK-01，[Studying](https://docs.ankiweb.net/manual/studying.md) | Decks、Study Overview，原件20–47行 | 选牌组→概览→Study Now；学习时S回概览，顶部Decks回列表。当天显示队列按New/Learning/To Review区分，出完当天应显示的卡后结束 | 未评分卡是否精确恢复、完成页文案、关闭/重启后的状态；当天结束不等于全部集合无待学卡 |
| E-AND-11，[Filtered Decks](https://docs.ankiweb.net/manual/filtered-decks.md) | Home Decks、Creating Manually，70–115行 | 概览Empty将卡归还原牌组并保留筛选容器；Rebuild按同一条件重新取卡；删过滤牌组归还卡且移除容器，不删卡 | Empty/Delete确认、撤销、重建失败反馈。Rebuild重跑筛选，不是承诺恢复原卡片集合；暂停/暂埋/已在其他过滤组的卡排除 |
| E-AND-14，[Stats](https://docs.ankiweb.net/manual/stats.md) | Statistics、Selecting Decks/Collection、Today，41–109行 | 顶部Stats或T打开；默认当前牌组及子牌组，可改牌组/全集、搜索过滤及历史范围。2.1.61+底部牌组选择器，2.1.28+搜索过滤。Today按回答次数计，一卡多答可多计，不随历史范围变化 | 选择是否持久化、全部图表、跨时区/同步。正文分别提到顶部历史范围与底部时间选择，未看图核布局；不能统一成一个已验证位置 |

对应F-M020-01、F-M020-12、F-M020-17。统计中的回答数、去重卡数与掌握数分开；删除笔记的历史仍计全集、不能再按原牌组归属统计。后续AI报“今天学完了”时建议同时说明集合、筛选和每日上限；这是本项目建议，不是新的已确认需求。
