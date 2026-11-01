# Ground News 未读官方帮助入口本地审计

- 审计日期：2026-10-27
- 范围：仅核对本地 `M-046` 案例、`ground-controls-deepening-inventory.csv`、Ground News 的 E 记录、`work/ground-controls-deepening/` 既有抓取与摘记。
- 网络：未使用。
- 判断规则：只有 inventory 明确写为“仅目录发现，未读正文”，且同一 article URL 在 E 记录与 `work/ground-controls-deepening/article-*` 中均没有正文抓取，才列为待读。目录页中出现链接不算读过正文。

## 可继续取证的登记入口（4 个）

| 稳定 inventory ID | 现有 F | 现有 E | 官方 URL | 与优先问题的关系 | 当前证据边界 | 证明未读的本地记录 |
| --- | --- | --- | --- | --- | --- | --- |
| M046-A335105 | 无 | E-GRCD-01 | https://help.ground.news/en/articles/335105 | “Where Can I Read Ground News?”可能补足可用端/阅读载体边界，进而限定外链与返回问题；标题本身不证明任何返回行为 | E-GRCD-01 只证明该 article-card 存在于官方 Getting Started 分类；没有该 URL 的独立 E，没有 `article-335105.html/.txt`，正文和具体交互均未知 | `ground-controls-deepening-inventory.csv` 第 4 行明确“仅目录发现，未读正文”；`category-2273089-links.json` 仅登记链接 |
| M046-A648513 | 无 | E-GRCD-01 | https://help.ground.news/en/articles/648513 | 套餐功能总表可能限定收藏、通知、阅读历史或个性化控制的权限范围；标题不能证明其中一定包含这些能力 | 仅有目录级 E；没有同 URL 的正文 E 或 `article-648513.*`；功能清单、套餐差异、动作细节均未知 | inventory 第 7 行明确“仅目录发现，未读正文”；`category-2273089-links.json` 仅登记链接 |
| M046-A485057 | 无 | E-GRCD-01 | https://help.ground.news/en/articles/485057 | “How … Help Readers See News Differently?”与推荐/观点范围机制最接近，可能说明产品怎样组织或引导阅读；标题不能证明存在可操作推荐控制 | 仅有 Understanding Ground News 分类目录入口；没有独立 E、正文抓取或操作证据；算法、控件、反馈和撤销均未知 | inventory 第 11 行明确“仅目录发现，未读正文”；`category-2272705-links.json` 仅登记链接 |
| M046-A593857 | 无 | E-GRCD-01 | https://help.ground.news/en/articles/593857 | 非英语文章范围可能影响内部阅读、翻译与外部原文跳转的边界；不直接证明外链返回 | 仅有目录卡片；没有同 URL 的正文 E 或 `article-593857.*`；语言来源、打开位置、返回状态均未知 | inventory 第 18 行明确“仅目录发现，未读正文”；`category-2272705-links.json` 仅登记链接 |

以上只有 4 个符合“已登记、正文未读、且与本轮重点仍有合理关联”的入口。其余未读目录项主要是付款、礼品、账号、隐私、合作或诈骗说明，未纳入本次最多 8 个的候选。

## 本轮重点中已读或已有正文 E 的入口（排除，避免重复）

| 重点 | inventory / F / E | URL | 排除依据与仍存缺口 |
| --- | --- | --- | --- |
| 收藏与回访 | M046-A2044545 / F-M046-02 / E-GRND-02 | https://help.ground.news/en/articles/2044545 | inventory 标为“既有正文E，本轮只核目录”；案例已写移动/Web 保存入口与联网限制。取消收藏、同步冲突、原文删除和精确续读仍未知，但当前目录没有另一篇明确的未读“取消收藏”文章 |
| 取消关注 | M046-A911873 / F-M046-07 / E-GND-05 | https://help.ground.news/en/articles/911873 | 已有正文 E，案例已重建手机/iPad/Web 取消关注；不重复读取 |
| 推荐控制：减少主题 | M046-A1499905 / F-M046-08 / E-GND-04 | https://help.ground.news/en/articles/1499905 | 已有正文 E，案例已记录取消不保存与管理回改；具体逐个撤销反馈仍未实测 |
| 推荐控制：移除来源 | M046-A916353 / F-M046-08 / E-GND-06 | https://help.ground.news/en/articles/916353 | 已有正文 E，案例已记录 Remove Source→Update 与登录 Web 同步；不重复读取 |
| 外部阅读/浏览器扩展 | M046-A1750529 / F-M046-06 / E-GND-19 | https://help.ground.news/en/articles/1750529 | 已有正文 E；案例仍明确外站返回位置未知 |
| 通知 | M046-A898305 / F-M046-13 / E-GND-10 | https://help.ground.news/en/articles/898305 | 已有正文 E，案例已记录 Android/iPhone 入口与 Daily News Update 开关；精确定时、静默时段和系统级行为未知 |
| 阅读历史/隐藏已读 | M046-A3523649 / F-M046-03 / E-GRND-06 | https://help.ground.news/en/articles/3523649 | inventory 标为“本轮正文已读”，且本地已有 `article-3523649.html/.txt`；已读阈值、撤销、清除历史、统计延迟仍未知 |

## 证据核对结果

- E-GRCD-01 是“帮助中心完整可见目录”：8 个顶层类、3 个子类、65 篇独立 article-card。它的 `capture_state` 虽写 `public_body_read_no_product_operation`，这里的 body 指帮助索引/分类页正文，不代表每篇 article 正文已读；`source-notes.md` 也明确“目录项正文是否已读取另标”。
- 四个候选 URL 只出现在当前 inventory、旧集成快照和分类页链接 JSON/HTML 中；未找到独立正文 E，也未找到对应的 `work/ground-controls-deepening/article-<id>.html/.txt`。
- M-046 案例当前明确保留这些未知：取消收藏、外部原文返回位置、隐藏已读撤销、阅读历史清除与统计更新延迟、通知精确定时。现有未读目录标题没有直接对应这些缺口的专门文章，因此不能凭目录补结论。

## 执行记录

所有核查命令均在 `/home/workspace/reader` 执行，退出码均为 0；第一次组合命令中的 `python` 不存在并返回 127，随后改用 `python3`，该失败未产生文件或其他副作用。

1. `pwd && rg --files | rg '(^|/)(README\\.md|docs/requirements\\.md|.*ground.*|.*Ground.*|.*interaction.*|.*evidence.*|.*capture.*)$' | head -n 300`
2. `python3` 读取 `M-046-ground-news.md` 与 inventory，并用 `find` 列出 Ground 本地抓取/归档。
3. `python3` 按 `body_review_status` 枚举全部“仅目录发现，未读正文”行。
4. `python3` 筛选收藏、关注、通知、已读、浏览器、来源与 feed 相关登记项。
5. `rg --fixed-strings` 对候选 URL 检查 docs、work、E 记录和旧集成快照；`find` 检查对应 `article-<id>*`。
6. `rg` 核对 E-GRCD-01、`source-notes.md` 与候选 URL 是否已有 capture/evidence。
7. `sha256sum work/research/batch41-ground/inventory-audit.md`（写入后验证）。

## 未知与状态变化

- 未知：四篇候选正文的实际内容、截图、版本日期和具体交互；本轮未联网，未读取它们。
- 未知：帮助中心是否存在未被当前 65 篇目录收录的新文章；本地审计无法证明线上当前目录未变化。
- 状态变化：只新增本审计文件；未修改 `docs/`、既有抓取、E/F/inventory 或产品状态。
