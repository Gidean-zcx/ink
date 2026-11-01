# Batch42 儿童学习闭环候选清点

日期：2026-10-27。范围：仅本地只读核对 M-029 洪恩识字、M-030 Khan Academy Kids、M-050 DragonBox Numbers 的案例、coverage、inventory、E 记录及正式来源摘记；未联网，未修改 `docs/`。

## 结论

筛出 **4 条**仍值得定点补看的公开原址，全部属于 M-030。它们不是完整“答错→提示→重试→退出恢复”现成证据，只可能补其中的提示入口、课后尝试记录或返回状态；取得后仍需保持对应边界。

M-029 与 M-050 没有符合本轮条件的新候选：洪恩未观察的官方介绍视频后来已抽样并判定为宣传剪辑，其他关键图片路线已有 403/412 或超时记录；DragonBox 的 Run 视频已遇登录验证，Dashboard 原页直取已有 403，其他正文/PDF均已读且没有未观察的专属错误闭环材料。

## 候选（按相关性排序）

| 优先 | 产品 | 公开原址 | 旧 E / F / 目录 ID | 已有取得状态 | 可能补的具体缺口 | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | M-030 Khan Kids | https://khankids.zendesk.com/hc/en-us/articles/4457349308059-Chinese-Parent-Guide-%E5%AE%B6%E9%95%BF%E6%8C%87%E5%8D%97-%E5%BC%80%E5%A7%8B%E4%BD%BF%E7%94%A8%E5%8F%AF%E6%B1%97%E5%AD%A6%E9%99%A2%E5%84%BF%E7%AB%A5%E7%89%88 | E-KLCD-07；F-M030-02/04/10；KLC-I070，细项 KLC-C07～11/C16 | 正文已读；`visual_observed=false`、inventory=`body_read_this_batch/not_observed` | 若原页当前仍有指南截图/附件，可核 Kodi 求助入口、活动说明措辞、Play/主界面返回状态，以及重装/进度声明所在画面；最接近“提示”和“退出后恢复”两端 | 不能从截图推出错几次触发提示、提示后是否由孩子再答、同步已完成或同题断点恢复 |
| 2 | M-030 Khan Kids | https://khankids.zendesk.com/hc/en-us/articles/4403614100109-Progress-reports-in-the-Khan-Academy-Kids-app | E-KLCD-10；F-M030-13；KLC-I108，细项 KLC-C17/C18 | 正文已读；`visual_observed=false`、inventory=`body_read_this_batch/not_observed` | 可能观察 Assignments/All Progress 的课程级“历次尝试”入口、勾号/百分比、低分课程与 Basic 再分配的可见关系，可帮助区分首次正确、重试记录和成人补练 | 教师报告不是儿童即时错答画面；即使看到尝试列表，也不能证明提示使用、独立答对或家庭版同样记录 |
| 3 | M-030 Khan Kids | https://khankids.zendesk.com/hc/en-us/articles/360048828572-Learn-more-about-the-Learning-Path | E-KLCD-02；F-M030-02；KLC-I057，细项 KLC-C01 | 正文已读；页面正文曾显示 `OlloStudentView.PNG`，但 E 为 `visual_observed=false`，inventory=`body_read_this_batch/not_observed` | 可补 Learning Path 的起点/Play 控件和返回路线的静态状态，限定困难时“more practice/basic lessons/additional scaffolding”与用户实际看到的入口 | 单张图不能证明错误次数、同题提示、重试结算、课内断点或调级算法；正文已经读过，不应重复计正文来源 |
| 4 | M-030 Khan Kids | https://sites.google.com/khanacademy.org/khanacademykids/getting-students-into-the-app/three-ways-to-play | E-KHD-04；案例仅作为旧证据列出，未映射独立 inventory ID；相关范围 F-M030-01/02/06 | 培训模块文字已读；嵌入 `Student_5Minute_Intro_to_KhanKids.mp4` 未播放，`visual_observed=false`；先前工具对 Drive 媒体不可读，但未记录 401/403/412/429 | 若公开媒体可正常匿名读取，可区分 Learning Path、Library、Assignments 三种入口及活动结束/返回到哪一层；可补“退出恢复”究竟是下一课、原活动还是教师指定任务的观察目标 | 只接受一次正常公开读取；若仍不可读即停止。它是教师培训演示，不保证包含故意错答或同题恢复，也不能替代固定构建实操 |

## 排除记录

### M-029 洪恩识字

- `https://h5static1.ihumand.com/website/pc/cn/shizi/assets/media/cn_video.198bceb2.mp4` 已由 E-IHVP-01 抽样观察两帧，并明确归类为官网宣传视频；重复观看不满足本轮“非重复视频、非纯宣传”。
- E-IHCD-04 的 2022 退出登录教程图片已返回 403；E-IHLC-01 的 B站直接 GET 为 412；什么值得买直取只有验证码壳；均属于已记录受阻路线。
- IHLC-I05/I08/I09 等确有错选、复习、错误回流缺口，但 inventory 只指向已经读过的 App Store 开发者说明，没有另一条已登记且正文未读/画面未看的明确公开原址。不能为它们猜链接。

### M-030 Khan Kids

- KLC-I005/006/018/028/034/041/046/049/050/051 已明确属于栏目 GET 403 后停止的路线；文章 URL 未解析，全部排除。
- E-KHCD-10 的错误音、分步动画和奖励 GIF 已经做过离散抽样，继续同源取帧属于重复视频/动画路线；现有证据仍不构成完整同题链。
- 其他 index-only 标题多为账号、语言资源、活动宣传或课堂管理；没有明确文章 URL，或不能补核心错误/恢复链，未列候选。

### M-050 DragonBox Numbers

- DBGP-07 / F-M050-08 的官方 Run 视频已遇播放器登录验证，未绕过，排除。
- E-DRGN-05 Child Activity Dashboard 虽标 `visual_observed=false`，但正式记录同时说明直接原页 HTTP 403 后停止；且它是跨 Kahoot Kids 应用概览，不是 Numbers 的错放、提示或局内恢复，排除。
- 历史指南 PDF、商店正文及两个作者报告均已读；它们明确没有错放动画、生命/提示、重试或保存时机。现有目录没有另一条未读且明确指向 Numbers 局内闭环的公开原址。

## 核查依据

- 案例：`cases/M-029-ihuman.md`、`cases/M-030-khan-kids.md`、`cases/M-050-dragonbox.md`
- inventory：`ihuman-learning-controls-deepening-inventory.csv`、`khan-controls-deepening-inventory.csv`、`khan-library-controls-deepening-inventory.csv`、`dragonbox-controls-deepening-inventory.csv`、`dragonbox-gameplay-deepening-inventory.csv`
- 来源边界：`artifacts/ihuman-learning-controls-deepening/source-notes.md`、`artifacts/ihuman-video-preview/source-notes.md`、`artifacts/khan-library-controls-deepening/source-notes.md`、`artifacts/khan-remaining-source-attempt/source-notes.md`、`artifacts/dragonbox-gameplay-deepening/source-notes.md`
- E 主表：`docs/research/interaction-library/evidence.jsonl`

## 执行命令与退出状态

1. 定位三个产品相关案例、coverage、inventory、E 与 source-notes：

   ```bash
   rg --files docs/research/interaction-library work/research | rg '(M-029-ihuman|M-030-khan-kids|M-050-dragonbox|ihuman|khan|dragonbox).*(inventory|coverage|evidence|source-notes|case|attempt|audit|manifest|\\.md$|\\.csv$|\\.jsonl$)' | sort
   ```

   Exit 0；结果保存为 `work/research/batch42-child-inventory/file-list.txt`。

2. Python 逐份读取三个案例、五个主要 inventory 和关键 source-notes，并按 `index_only`、`not_observed`、未读/未看/blocked 状态筛选：exit 0。

3. `rg` 定点检索“答错/错误/提示/重试/退出/恢复”及英文同义词，核对候选与既有缺口：exit 0。

4. Python 从 `evidence.jsonl` 读取 E-KLCD-02/07/10、E-KHD-04、E-KHCD-03、E-KHK-02/03、E-KHCD-10、E-DRGN-05 的 URL、取得方式、视觉状态和限制：exit 0。

5. `rg` 核对 KLC-I057/I070/I108、KLC-C01/C07～11/C16～18 及相关 F/E 映射：exit 0。

6. 写入本报告并计算 SHA-256：exit 0。

## 不确定性与状态变化

- 本轮没有打开线上页面，所以不能保证候选原址在 2026-10-27 此刻仍可匿名返回正文或媒体；只确认库内已有明确公开 URL，且未记录为指定 HTTP 拦截路线。
- “可能补什么”依据页面标题、已读正文和未观察资源位置，仅用于下一步取证排序，不表示页面内一定存在所需画面。
- 未检索新的互联网链接，没有推导未登记 URL，也没有重新读取全部竞品。
- 状态变化仅新增 `file-list.txt` 与本报告；未修改 `docs/`。
