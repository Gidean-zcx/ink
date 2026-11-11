# Metalio 书签模型

来源：[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus)，提交 `874538e`（v2.0.77），查阅日期 2026-11-12。MIT，原许可证见 [LICENSE](LICENSE)。

原始文件：`main/reader/book_bookmarks.h`、`main/reader/book_bookmarks.cc`（合计 197 行，其中实现 154 行）；根 `LICENSE`。

| 文件 | 原文 SHA-256 | 本地 SHA-256 |
| --- | --- | --- |
| `book_bookmarks.h` | `58811430866bae6fb9ce5eb3473ae7cfd9d6f1d2f7f9e4db4e6b4a129475bb3e` | `58811430866bae6fb9ce5eb3473ae7cfd9d6f1d2f7f9e4db4e6b4a129475bb3e` |
| `book_bookmarks.cc` | `3232e4ca256704727153a087d1e2778e42adc51153505cc2c1daeca53d6cd3cd` | `eeef4b268bb29025d98e72bd3b3841ef7cc34b3c14ddfdb61bcfe20650b30654` |
| `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |

适配改动：

- 头文件保持原文；保留源位置比较、页范围判定、256 条和 768 字节上限、增删和按 ID 倒序的原厂逻辑。
- 实现删除匿名命名空间内的 BKM1 文件编码、校验、文件读写和原子重命名函数（原文 28～82 行），不引入原厂文件格式。
- `Load`（原文 85～97 行）和 `Save`（98～132 行）接到 `apps/reader/bookmarks.cc` 的成员数据库。使用现有 JSON 记录和存储事务，每个书签一条记录，以书籍完整路径筛选。
- `Add` 的 `entry.id=last+1` 改为成员数据库统一分配 ID，防止同一成员不同书籍的记录编号相撞；其余 `Add`、`Remove` 函数保持原文。
- 编译定义 `EBOOK_EMULATOR` 去掉原厂 Lang 依赖；界面和回执使用项目中文词表与 i18n。仅该原厂文件启用 C++ 异常，保留原文内存不足处理，不改全局配置。

S31 原有配置关闭全局 C++ 异常，ESP-IDF 仍将抛出异常接到终止桩；保留 catch 不等于已在设备上验证内存耗尽恢复。本任务不改 SDK 配置。

原厂交互依据：`main/reader/book_session.cc:3700–3809` 的范围捕获、当前页标记、加签与源位置跳转。项目 EPUB 内容源把解析块映射回稳定章/块/块内 UTF-8 偏移；TXT 按原厂章和块为 0、偏移为文件字节。页面使用系统主题和分页组件，保留时间、章名、摘录、点按跳转及删除入口。
