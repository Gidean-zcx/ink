# settings-d137 报告

（子代理不能写报告文件，由主会话照它交回的原文存进分支。）

# settings-d137 报告

D-137 两件事都做了：设置首页在默认字号下放进两页，孩子在控制中心有了设置入口。

## 做了什么
- **首页放进两页**：
  - 存储、备份、关于和更新这几行不天天用，收进一行"系统"。为了凑够两页，家人、语音和 AI 也收了进去，这一点要你定，见下文。
  - 首页留下：网络和连接（Wi-Fi、移动网络、蓝牙）、声音和通知（音量、勿扰）、显示和阅读（亮度、暖光、字号、锁屏和壁纸），最后是"系统"。分组和当前值都没动。
  - "系统"这一行的值写着它里面有什么：家人、语音和 AI、存储、备份、关于和更新。值太长时换行放在名字下面，不截断。
  - "系统"上面不再加一个同名的组标题"系统"。
  - 点开"系统"是一页列表，按原来首页的画法画：每行在自己的分组标题下，带当前值和箭头；点一行和以前从首页点一样，打开它原来的页。
  - 哪行在哪一页，仍然只在登记表 settings_registry.c 里写一次。新加了两个标记 INK_SR_SYSTEM、INK_SR_KID，加了一项 id "system"。
- **孩子的入口**：
  - 孩子的控制中心顶行多了齿轮，点了走 settings.open {}。这个能力本来就允许孩子用，AI 也走这一个。
  - 打开的设置首页只列 INK_SR_KID 标记的 8 行：亮度、暖光、音量、勿扰、按键提示音、振动、来电铃声、闹钟铃声。每一行都是设置服务本来就允许孩子改的项（child_may_change），测试里逐行核对。
  - 家长才能动的项还是要家长：孩子搜得到，但显示"家长管理"、打不开；settings.open system 也会被拒。孩子的规则没变。
  - 大人的齿轮照旧走 shell.settings。

## 验收表
| 编号 | 结论 | 证据 | 命令 |
|---|---|---|---|
| 1 默认字号首页页数 | 通过：2 页（原来 3 页） | 【实测】settings-1 打印 "size 1: the home has 2 pages"；测试里检查默认字号下必须 2 页 | `python tests/settings_1.py --build build` |
| 2 最大字号首页页数 | 3 页（原来 4 页） | 【实测】"size 3: the home has 3 pages"。任务只要求默认字号两页，这里只报数 | 同上 |
| 3 系统页 | 通过 | 【实测】点"系统"进 SetSystem；默认和最大字号都一页放下 5 行，在各自组下，每行有值；点"存储"进 SetStorage | settings-1（两种字号都 PASS） |
| 4 孩子入口只给孩子、只列孩子的项 | 通过 | 【实测】大人的齿轮是 shell.settings；孩子的齿轮是 settings.open {}，点开只有孩子的行，没有 wifi、system，也没有"家长管理"；孩子的首页正好 8 行，每行 child_may_change；孩子打开 system 被拒 | `python tests/control_center.py --build build`（6 段全过）；settings-1 第 7 段 |
| 5 触摸和 AI 一致 | 通过 | 【实测】点"系统"和 AI 调 settings.open system，得到的 rows 完全相同；孩子点齿轮和 AI 调 settings.open {}，rows 完全相同；首页"系统"行的 capability 是 settings.open | settings-1、control-center kid |
| 6 反向对照 | 通过 | 【实测】settings-1 反向 12/12 都按预期失败（原 7 项加新 5 项：孩子拿到大人的首页、孩子首页列了不允许孩子改的行、存储留在首页变 3 页、"系统"上加了组标题被挤到第 3 页、系统页列了首页的行），产品源码没被改动。control-center 新加 2 项（孩子齿轮走大人入口、两边入口对调）都按预期失败；恢复后五段走查重新全过 | `python tests/settings_1_reverse.py --build build`；`python tests/control_center_reverse.py build <python> child-gear-is-parents gear-entries-swapped` |
| 7 只刷新变的部分（D-127） | 通过 | 【实测】settings-1 "the changed value redraws only its row: True"。系统页也用同一套只改一行的刷新 | settings-1 |
| 8 字号一起缩放 | 通过 | 【实测】"ui-scale: 64 files, 0 violations" | `tools/ui_scale_check.py` |
| 9 gcc | 通过 | 【实测】"gcccheck: 8 compiled, 0 with findings"（4 个产品文件和 4 个测试文件） | `ink-codex/tools/gcccheck.py build ...` |
| 10 全量测试 | 449/450 | 【实测】唯一失败 autotest-cap（相机），基线同样失败，见上 | ctest -j2 |
| 11 S31 | 通过 | 有 @690a023 的屏幕输出层；镜像多 788 字节 | build.sh |

截图（不在仓库里），都在 `<临时目录>` 下，-1 是默认字号，-3 是最大字号：
- 首页第 1 页：home-1.png、home-3.png
- 首页最后一页，有"系统"行：home-last-1.png、home-last-3.png
- 系统页：system-1.png、system-3.png
- 孩子的设置首页：kid-home-1.png、kid-home-3.png
- 孩子的控制中心，有齿轮：cc-kid-1.png、cc-kid-3.png
- 孩子点齿轮之后：cc-kid-settings-1.png、cc-kid-settings-3.png
- 改之前的首页：before-home-1.png、before-home-3.png

## 没做到的
- 最大字号下首页还是 3 页，任务只要求默认字号两页。
- 内存检查（ASan）没跑，任务书没要求。

## 和设计不一致的
- 系统页没有放在 settings_pages.c 的列表页框架里。先试过那样做：列表页的分组标题是加粗大标题，再加上固定格高，默认字号下 5 行要占 2 页，样子也和首页不一样（没有箭头）。最后改用首页自己的画法画系统页（settings_body.c 里加了 SetSystem 一页），于是 settings_pages.c 没改。
- 页号是 PAGE_KNOWN+1。page_t 定义在 shell_internal.h，不在允许改的文件里，所以在 settings_body.c 里用 `#define PAGE_SYSTEM ((page_t)(PAGE_KNOWN + 1))` 取了值。

## 改过的已有测试（都是因为有意改了行为，强度没降）
- tests/settings_1.c
  - walk_home 按孩子或大人分别对照 INK_SR_KID 或 INK_SR_HOME（原来只看 INK_SR_HOME）。
  - 新增检查：孩子的行必须是允许孩子改的设置；大人首页不出现孩子专属的行；孩子首页正好 8 项；默认字号必须 2 页。
  - 新增系统页一段，包括触摸和 AI 一致。
  - 孩子不能打开 system。
  - 记录页数（settings1.home_pages、kid_pages）。
- tests/settings_1.py：多打印一行页数。
- tests/settings_1_reverse.py：page-only-row 那项要找的源码行换成新写法（换上去的内容不变），另加 5 项。
- tests/control_center.c：kid 一段前面加了大人齿轮的检查，后面加了孩子齿轮、它打开的行、AI 看到同样的行。
- tests/control_center_reverse.py：加 2 项。
- tests/i18n_lang.c：中英首页要对照的 9 行里，语音和 AI、备份、关于和更新换成亮度、字号、系统；"语音行每个值都完整"的检查改到英文系统页上做，并且要求系统页显示 Family & Children、Voice & AI、Backup、About & Updates，没有字被截断或重叠。
- tests/settings_b.c：先点"系统"，并检查进了 SetSystem，再点"家人"。
- tests/ui_shell_regressions.c：叫名字的值原来在首页第 3 页找，现在到 settings.open system 的页上找。

## 新增的机制
- 登记表里两个位置标记：INK_SR_SYSTEM（这行在"系统"页）、INK_SR_KID（这行在孩子的首页）。D-137 要求的。
- 一个新页 SetSystem，用首页的画法，不带搜索框。D-137 要求的。
- 没有新任务、状态机、文件格式、配置项，也没有新能力。系统页只用已有的 settings.open {"id":"system"} 打开。

## 要用户定
- 除了存储、备份、关于和更新，我还把家人、语音和 AI 收进了"系统"。只收前三项的话，默认字号下首页还是放不进两页（显示组 4 行加上"系统"会挤到第 3 页）。另一种做法是把锁屏和壁纸、字号收进去，保留家人和语音。
- "铃声"我理解成来电铃声和闹钟铃声两行都给孩子。振动强度没放进孩子首页：它也允许孩子改，在声音页里能找到。
- 孩子首页按登记表的分组顺序排，声音组在前、显示组在后。要让亮度排第一，需要调整组的顺序。

## 顺手发现
- autotest-cap 的相机场景在基线 c4ad7dea 上也失败（"camera script returns home"），和本任务无关，需要另开任务查。
- 页面栈最多 6 层（system/ui/ui_task.c 的 MAX_ACTIVITIES 6），超出时 push 不报错，直接不动。现在多了一层"系统"：主屏 → 设置 → 系统 → 关于 → 隐私 → 记录，已经到 6 层；记录页如果还有下一层，就会点了没反应。我没有改。测试里也碰到过：反复 settings.open 不回主屏，就会把栈压满。
- 英文下"系统"行的值用 ", " 连接，中文用 "、"。

## 要改的文档（由主会话统一改）
- docs/system/capabilities.md 要重新生成：settings.open 的 id 枚举多了 "system"。没有新增能力。
- docs/tasks/backlog.md 记 D-137 完成。最大字号首页 3 页、页面栈 6 层的风险可以记为待办。

## 隐私检查
`git diff c4ad7dea..HEAD | grep -nE '/Users/|/private/tmp|MAC 地址格式'` 没有匹配，0 处。

## 复核修复（两路审查后的修复轮，修复代理交回的原文）

## 结果

**修好的问题（复核确认属实）：** D-137 加了 SystemSet 这一层以后，页面栈到了 6 层上限。查看和删除记录（Records）上的四行点了没有反应，AI 调用这几项能力时却回了成功。

**驳回：** 没有。

**全量 ctest，原样照抄：**
`99% tests passed, 2 tests failed out of 450`
- `unit-s31-uhci-negative`：机器负载高时偶发失败，单独重跑通过，第二次和其他相关测试一起跑也通过。
- `autotest-cap`：每次都失败，但这个失败在修复之前就有，跟本任务无关。证据见“复核修复”第 5 点。

**最后一次提交：** `5c0e8ddd Room for the deepest pages again after 系统 (D-137 review): 12 pages, and a full stack is never silent`，在 worktree `.claude/worktrees/wf_7b1bb08e-589-7`，分支 worktree-wf_7b1bb08e-589-7，没有推送。

---

## 报告：settings-d137（含复核修复）

说明：worktree 里找不到原来那份报告的正文。下面前半部分是我按 4 个 D-137 提交的说明重写的，后半部分“复核修复”是这次新加的。

### 做了什么（D-137）
1. **设置主屏缩到两页**（a3620818）
   - 不常用的家人、语音和 AI、存储、备份、关于和更新，收进主屏上的一行“系统”。这一行的值会写出里面有哪些项。
   - 打开“系统”是一页列表，按原来的分组排，每行带当前值，点哪行就打开哪一页，和以前从主屏点进去一样。
   - 孩子的主屏只列 INK_SR_KID 标记的 8 项：亮度、暖光、音量、勿扰、按键音、振动和两种铃声。都是孩子本来就能改的设置。
2. **控制中心加了孩子的齿轮**（ba8731f5）
   - 孩子那一行头部有个齿轮，走 settings.open {}，和 AI 用的是同一个入口。
   - 家长的齿轮还是走 shell.settings。
   - 孩子的面板快照里写出了齿轮。
3. **反向检查**（a74765be）：孩子主屏的行、“系统”页、主屏两页、孩子的齿轮。
4. **改了以前从主屏找语音、备份、关于、家人的测试**（b7962d5c），改成去“系统”页里找：tests/i18n_lang.c、tests/settings_b.c、tests/ui_shell_regressions.c。

### 复核修复

**1. 问题属实，原因如下**
- `system/ui/ui_task.c:40` 写的是 `MAX_ACTIVITIES 6`。`ink_ui_push` 遇到满栈直接 return，不报错，也不返回结果。
- 从主屏进去：主屏 → 设置 → 系统 → 关于和更新 → 隐私 → 查看和删除记录，正好 6 层。再点 Records 上的四行（shell.ai_log、shell.ai_home、shell.ai_memory、shell.runtime_records）都要再开一层，全被拒。
- 从应用里点齿轮进去（shell.settings 在当前栈上再开一层），到隐私就已经 6 层，连 Records 都打不开。
- 用户点了没有任何提示；AI 调 shell.ai_log 这类能力却回 DONE。
- 一处小修正：“和 AI 说过的话”（shell.ai_home）走的是 `ink_app_open`。那里满栈时会先回主屏再打开，所以这一行不会没反应，只是丢掉了原来的页面栈。另外三行确实点了没反应。

**2. 怎么修的**
- **页面栈上限从 6 改成 12**（`system/ui/ui_task.c`）。
  - 现在最深的路是：应用 → 设置 → 系统 → 关于和更新 → 隐私 → 记录 → AI 替你做过的事 → 某条回执，共 9 层。上面再盖一个 AI 页还剩两层余量。
  - 每层只多 16 字节：栈里只存两个指针，页面状态本来就是各页自己的静态数据，只有顶上一页持有屏幕。
- **满栈不再不出声**
  - `ink_ui_push` 改成返回 bool。满栈时打一条 WARN 日志（`page stack full (N): X not opened`），同时累计拒绝次数（新增 `ink_ui_push_refusals()`）。原来 66 个调用处不用改。
  - `system/policy/cap.c` 的 start() 会对比能力执行前后的拒绝次数。能力执行期间如果有页面没开成，而它还回了 DONE，就改成 FAILED，detail 是 "too many pages open"，say 是“打开的页面太多了，先回主屏再试”。这样 AI 不会再拿到假的成功。
  - 触摸走同一个回执：`system/ui/shell/airplane_card.c` 的 `ink_shell_refused` 认这个 detail，弹出现有的 Refused 卡片。标题“没有打开”，正文是上面那句话，按钮“知道了”。快照里也写着这些字。
  - 截图里看到的是现成的系统卡片样式，没有新造控件。
  - `assets/i18n/en.tsv` 补了两条英文。
- 新增 `ink_ui_depth()`，返回当前开了几层页面（含根页面），测试用它确认层数。

**3. 测试**
- 新增测试，在 tests/settings_1.c 第 1c 段，默认字号和最大字号都跑：
  - 从主屏：设置 → 系统 → 关于和更新 → 隐私 → 记录，确认 Records 在第 6 层。四行分别用手点一次、让 AI 调一次，都要打开对应页面（AiLog、AiHome、AiMemory、RuntimeRecords），层数正好加 1，返回后回到 Records。
  - 从时钟应用点齿轮进去，同样走一遍，Records 在第 7 层，四行同样都要能打开。
  - 把栈塞满以后：点“AI 替你做过的事”，页面不变、层数不变，弹出“没有打开”卡片，快照里有这句话，点“知道了”能关掉。
  - 满栈时 AI 调其中三项回 FAILED，带着原因，页面不变。shell.ai_home 照它原来的做法回主屏打开 AiHome，这一点也写进了测试。
- 结果：settings-1 默认字号 837 项检查、最大字号 448 项检查全过。主屏两页、孩子主屏两页，“只重画改了的那一行”也照样通过。
- **改过的现有测试：** 只在 tests/settings_1.c 里新增了第 1c 段和几个辅助函数，原有检查一条没动，没有放宽，也没有改时间。

**4. 检查**
- gcc 检查：ui_task.c、cap.c、airplane_card.c、tests/settings_1.c 四个文件都编过，0 处问题。
- 相关测试单独重跑全过：settings-1、settings-b、control-center、i18n-lang、unit-s31-uhci-negative。
- 全量 ctest 先清空了 build/test-out 并重新 configure：`99% tests passed, 2 tests failed out of 450`。

**5. autotest-cap 是以前就有的失败，跟本任务无关**
- 失败的是 camera 场景，`tests/device_autotest/sim.c:32` 的 “camera script returns home”。我临时加了打印：按下 home 键 2 秒后，页面还停在 Camera（深度 2）。
- 撤掉本次修复后照样失败。把 system/ 整个退回 c4ad7dea（D-137 之前、control-1 合并时的代码）也照样失败。
- main 上 c4ad7dea 之后的提交（f470a5fd、c5ba79c2）没有碰这块。
- 我没修，因为不在这次的范围里。建议单独派一个任务查：control-1 之后，相机页上的 home 键为什么没回主屏。临时加的打印已经撤掉。

**6. S31 固件**
- `ports/esp32s31/build.sh` 跑通。日志里有 “factory screen output layer @690a023”，没有“没有屏幕输出层”。
- ink-s31.bin 是 4,485,344 字节（总镜像 4,484,936 字节），应用分区还剩 0xb8f20 字节（14%）。
- 体积变化：主仓库里 14:28 的 build-s31 是 4,482,720 字节，现在多 2,624 字节。但那个对照版本也不包含 D-137 的 4 个提交，所以这次修复单独占多少没有分开测。
- build-s31-own 已删掉；没有 build-asan；build 保留着。
