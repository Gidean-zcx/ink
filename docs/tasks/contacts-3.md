# contacts-3：联系人照 Fossify Contacts 重写——一个人多个号码带标签、详情页先给动作、收藏、导入导出 vCard、重复的看得见能合并；电话、短信、勿扰读同一份新格式

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先，第 11 节最先）和 `docs/tasks/vendor-common.md`（第 11 条）。

**D-140（总规则第 11 节）**：交付计划写的是“照着重写”（`docs/tasks/delivery-plan.md:66`）。所以：
- **第一步**：把本任务书附带的对照表落成 `docs/tasks/contacts-3/对照表.md`，**单独提交，提交前不写任何代码**。参照产品每一页、每个操作、每种状态（空、加载、出错、没网、孩子）各一行，每行写“照做 / 按设备改（怎么改、为什么）/ 不做（为什么）”。
- **搬代码**：计划没有给联系人标“搬代码”或“片段可搬”，Fossify 也没有能在这台设备上跑的代码（见下面“搬代码”一节的证据）。要复用的是电话已经原样拷进来的 LilyPhone 号码比对，**直接调用，不再拷第二份**。
- **照着重写**：按对照表做，做完逐行验收，每行配截图或测试名。报告写对照表做到了几行、哪几行没做。

## 为什么

用户 2026-11-12 说：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”（D-129）；“我需要你先把当前的壳和现有已有的应用移植或参考重写搞到成熟可用”（D-131）。2026-11-13 傍晚又说：“对 按着实现 参考安卓也行 但是要注意设备特性 不要生搬硬套导致使用体验反而不佳”（D-140）。升级要自动迁移旧数据（D-138）。

调研见 `docs/tasks/replan-sysapps/APPS.md` A03 和 `CURRENT.md` A03。现在的联系人每人只有一个号码，没有号码标签、收藏、vCard 导入导出和重复处理。APPS.md 给的参照路径是：从应用、拨号、短信三个入口找到同一个人 → 选号码 → 行动；导入时重复的要看得见怎么处理。

phone-3 的报告（`docs/tasks/phone-3/REPORT.md:99-100`）交来一件事：`contacts.edit` 不收号码，拨号页和最近通话的“加为联系人”只能打开空白页。notify-svc-1 的报告（`docs/tasks/notify-svc-1/REPORT.md:19`、`:68`）说，勿扰的“收藏的人来电放行”（枚举值 3）在等联系人提供收藏接口，现在按“谁都不放行”处理。

## 现状（2026-11-14 按 `3baaf0e2` 核对过的代码）

整个应用是一个文件 `system/apps/contacts/contacts.c`，327 行，3 个页面，13 个能力。

- **数据**：集合 `phone.contacts`（`:13-14`），按家人分开，`schema 1`，AI 可读、不可写，最多 200 条。记录是 `{"name","number","note"}`。
  - 读一条用 `read_contact`（`:41-51`）。没有 `number` 的记录直接读不出来。
  - 写：`record`（`:82-129`）。号码只许数字、空格、`-()`，以及开头的 `+`（`:99-101`）。姓名最多 32 字，号码最多 24 位，备注最多 64 字（`:308`）。
  - `contacts.put` 的标题就写着“每人一个号码”（`:315`）。
- **列表**：`build_rows`（`:63-71`）一边排序一边反复读前面记录的 JSON（`:68`），200 人时最坏要解析约两万次 JSON。搜索框每次内容变化都会整列重建一遍（`:72-75`）。排序按 UTF-8 字节，不是拼音。每行左边有一个头像圆圈写首字（`:183-189`），每页 3 行（`tests/contacts/run.py:26-28`）。
- **详情**：`:247-254`。姓名、号码、备注三行总是画出来，没有备注也占一行（`:248`）。底部三个药丸按钮“打电话”“发短信”“编辑”（`:250-253`）。
- **编辑**：`:256-269`。三个输入框都是普通文字键盘，号码框没用电话键盘（`:257`；`INK_TEXT_PHONE` 已有，`system/include/ink/sys/textinput.h:19`）。下面三个按钮：“保存联系人”“删除联系人”“选择这个号码”。删除照 D-105 弹确认（`:316` 的 `ask`）。
- **选人**：`contacts.pick` 打开列表的“挑选”模式，点一行调 `contacts.select`（`:192`）。`contacts.select` 返回 `{id, number}`，并发出 `contacts.selected` 事件（`:125`）。电话的“联系人”页签打开的是“电话模式”的列表（`ink_contacts_open`，`:327`），点一行进详情，不是选号码。phone-3 的对照表 H1 定的是：“选好回到拨号页带号码”。
- **被动刷新**：别处改了联系人后，`refresh`（`:76-81`）用 `ink_ui_redraw`，按着手指时会等手指抬起（tap-loss-1，`system/include/ink/sys/ui.h:88`）。
- **注意**：电话应用 `phone` 的应用登记也在这个文件里（`:303`），所有 `contacts.*` 能力都挂在应用 `phone` 下（`:310-322`）。重写时要保留。
- **谁在读联系人**：
  - 电话：`system/apps/phone/phone.c:178-195` 的 `load_contacts` 是唯一读联系人记录的地方，只读 `name` 和 `number`，注释写着“contacts-3 will change their shape”。读出来的表交给 LilyPhone 的 `contacts_find_by_number`、`contacts_display_name`（来电查名、通话页、最近通话），还有拨号页的匹配 `match`（`:280-292`）。表的大小是 `lily_compat.h:13-20` 里 `contact_t {name[97], number[25]}` 乘以 `CONTACTS_MAX 200`，放在 PSRAM 里（`phone.c:36`、`:44-46`，`CAND_MAX` 由 `CONTACTS_MAX` 推出）。“加为联系人”现在有**三处**调 `contacts.edit`，号码都带不过去：拨号页快照里的动作 `:358`（`P.actions[…]`，capability 写死 `contacts.edit`）、拨号页按钮处理 `:486`（`add_contact`）、最近通话详情的动作 `:800-801`。`tests/phone_3/phone_3.c:473` 断言拨号页快照里含 `contacts.edit`。
  - 短信：`system/apps/sms/sms.c` 在 `:78`（输号码时提示）、`:213`、`:349`（会话里显示名字，exact 模式）、`:385`（返回 -1 当作“还没读进来”）调 `ink_contacts_match`。`sms.recipient` 把参数原样转给 `contacts.select`（`:157`，失败一律记成 FAILED，不带说明），它的参数格式只有 `id`（`:458`）。`sms.pick` 调 `contacts.pick`（`:160`）。
  - 勿扰：`system/services/notify/notify_model.c:116` 用 `ink_contacts_match(number, true, …)` 判断来电是不是联系人。`INK_NOTIFY_CALL_STARRED` 在 `system/include/ink/sys/notify.h:26` 有定义，但没人处理。`notify.dnd_set` 已经能把 `calls` 设成 3（`notify_model.c:215`）。`notify_model.c` 编进 `ink_system`，和 `contacts.c` 在同一个库里，不需要给测试另加桩。
  - `ink_contacts_match` 的 exact 模式是 `strcmp`（`contacts.c:59`）。所以存的是 `138 0000 0001`、来电是 `+8613800000001` 时，短信和勿扰都认不出是同一个人；电话用 LilyPhone 的比对就认得出。三个入口不一致。
  - 全局搜索：`system/ui/search.c:84-99` 读 `name` 和 `number` 两个键，点开结果调的是 `contacts.edit`（`:269`），进的是编辑页，不是只读的详情。
  - 扫码：`system/apps/camera/scan.c:62-67` 识别出 vCard 或 MECARD 的二维码后，用 `name`、`number` 调 `contacts.put`。
  - 时间到期后仍能读：`system/policy/cap.c:150-154`。
- **会挡路的限制**：
  - 一次能力调用的参数文字最多 416 字节（`system/include/ink/sys/args.h:21-22`）。姓名 96 字节加备注 192 字节，再加 5 个号码，最坏约 509 字节，放不下。所以号码列表要用单独的能力写，见“要做的”第 1 条。
  - `.vcf` 在文件索引里算 `INK_FS_OTHER`（`system/services/file/file_service.c:1659-1670`）。文件选择器只按类型列，不按扩展名（`system/include/ink/sys/file_picker.h:9`），标题是“选一个文件”（`system/ui/file_picker.c:188`）。
  - 记录各自带版本号。老备份（版本 1）能还原到版本 2 的集合；版本 2 的备份还原不到老固件（`system/services/data/datastore.c:1118-1119`）。memo-3 已经这样升过 1 到 2（`system/apps/memo/memo.c:22`，`.schema = 2`）。
  - 没有“汉字转拼音”的公开接口，文本服务的拼音引擎只会从拼音出汉字（phone-3 对照表 A5 也是这个结论）。
- **构建**：主工程 `CMakeLists.txt:1755-1759`（源文件、`sim-contacts`、ctest `contacts`）和 `:473`（i18n 扫描列表）。S31 用通配收 `system/*.c`（`ports/esp32s31/components/ink_system/CMakeLists.txt:2`、`:21`），新文件自动编进去，但 i18n 列表（同文件 `:37`）要手动加。`python3 tools/ui_scale_check.py system/apps/contacts` 现在是 0 发现；这条检查默认不查应用（`tools/ui_scale_check.py:339`），要手动跑。
- **字体**：font-pack-1 之后字体分区有大约 2 MB 余量，应用分区剩 14%（`docs/tasks/font-pack-1/REPORT.md:75`）。不再要求 0 新字，但要少加，报告写新增了几个字。

## 成熟来源（固定提交，浅克隆到临时目录读，不进仓库）

- **Fossify Contacts**（第一参照，GPL-3.0，只照行为重写，不拷代码），`github.com/FossifyOrg/Contacts` 提交 `5664636dbc78dd0c0b86ccc25b554943b24feb96`，下面的路径都在 `app/src/main/kotlin/org/fossify/contacts/` 下。旧任务书的行号有两处不对，已改正。
  - `activities/ViewContactActivity.kt`：
    - `gotContact` `:191-245`：有号码时才出“发短信”“打电话”（`:197-198`、`:232-233`）。
    - `setupFavorite` `:301-324`：点星立即写，再点取消。
    - `setupPhoneNumbers` `:341-422`：合并同名重复人的号码，按末 9 位去重，按类型排；点一行打电话，长按复制，默认号码有标记。
    - `setupNotes` `:685-696`：没有备注就不画。
    - 菜单 `:93-133`：编辑、分享、删除。
    - `deleteContactWithMergeLogic` `:834-849`：删除前确认。
  - `activities/ContactActivity.kt`：`trySendSMS` `:146-165`（一个号码直接发；多个号码时有默认用默认，否则弹单选框）；`deleteContact` `:132-140`。
  - `activities/EditContactActivity.kt`：
    - `initContact` `:213-251`；`gotContact` 里带号码进来新建或追加 `:263-273`（`ACTION_INSERT`、`ADD_NEW_CONTACT_NUMBER`）。
    - 离开前问“保存还是不保存” `:385-404`；菜单（保存、分享、删除）`:406-442`。
    - 号码：`setupPhoneNumbers` `:594-620`，设默认 `:622-634`、`:636-666`（只有一个号码时不显示默认标记，`:640-643`），类型选择 `:1049-1072`（9 种，含自定义），加一个号码并弹键盘 `:1486-1497`。
    - 保存 `:1169-1213`（全空时提示 `fields_empty`），空号码保存时丢掉 `:1247-1271`，收藏切换 `:1551-1559`。
  - `activities/InsertOrEditContactActivity.kt`：挑选（`ACTION_PICK`）`:46-64`；点一个人就把号码追加进他的编辑页 `:288-316`；“新建联系人” `:329-358`。
  - `activities/MainActivity.kt`：三个页签（联系人、收藏、分组），搜索框，浮动按钮，菜单（排序、筛选、拨号盘、视图、设置）；从外部打开 `.vcf` 直接导入 `:380-397`。
  - `fragments/MyViewPagerFragment.kt`：空状态文案 `:67-84`；收藏页只留加星的人 `:131-143`；搜索 `onSearchQueryChanged` `:310-356`（姓名、昵称、规整后的号码、邮箱、地址、备注、公司，开头命中的排前）；字母快速滚动条 `:266-290`。
  - `fragments/FavoritesFragment.kt:50-59`：“添加收藏”弹多选框。
  - `adapters/ContactsAdapter.kt:99-131`：长按多选后的菜单（编辑、加收藏、加分组、分享、群发短信、发邮件、快捷方式、全选、删除、移出收藏），删除确认 `:200-214`。
  - `helpers/VcfImporter.kt`：`importContacts` `:46-299`。一个文件多个人（`:54`）；姓名用结构化的 `N`，只有 `FN` 时当公司名（`:56-61`、`:271-279`）；号码和类型 `:66-92`；类型对照 `getPhoneNumberTypeId` `:344-370`；首选号码 `getPreferredValue` `:405-413`；备注 `:193`；结果分三种，全成、部分、全败（`:294-298`）。**它不查重**，什么都插进去，重复靠显示时合并。
  - `helpers/VcfExporter.kt`：`exportContacts` `:32-219`，默认写 vCard 4.0（`:37`）。`FN` `:54-63`；号码带类型和 `PREF` `:78-85`；备注 `:165-167`；类型对照 `:221-231`。
  - `dialogs/ImportContactsDialog.kt:74-84`：结果用提示条。`dialogs/ExportContactsDialog.kt:31`：默认文件名 `contacts_<日期时间>`；`:71-75`：文件名重了要改。
  - `activities/SettingsActivity.kt`：导入 `:301-334`、导出 `:336-389`（没人可导出时提示 `no_entries_for_exporting`）、“合并重复联系人”开关 `:248-254`。
- **Fossify Commons 7.1.3**（Contacts 自己钉的库，`gradle/libs.versions.toml:20`；`github.com/FossifyOrg/Commons` 标签 `7.1.3` = 提交 `227e7d3cdea2343679eb0863bb597fc7a9200b4d`）。不算新调研，是顺着 Fossify 的固定版本读：
  - `helpers/ContactsHelper.kt:72-83`：“合并重复”默认开（`BaseConfig.kt:546-548`），按显示名不分大小写归成一组，只显示资料最全的那条。
  - `:1579-1590`：`getDuplicatesOfContact`。
  - `extensions/Context-contacts.kt:332-351`：`initiateCall`，多个号码时有默认用默认，否则弹框选。
- **Lineage Contacts**（对照），`github.com/LineageOS/android_packages_apps_Contacts` 提交 `c03937e4d35924455b6e27d34fd91b1426580f4b`，`src/com/android/contacts/quickcontact/QuickContactActivity.java`：
  - `:1012-1070` `bindContactData`：先绑这个人，再在后台生成各条资料。
  - `:1730-1802`：每个号码一行，点行打电话，旁边的图标发短信（“这个人”和“某个号码”分开）。
- **LilyPhone**（已在电话里原样拷了，本任务只调用），`github.com/dparnell/lilyphone1` 提交 `de10fc3882292b9be1cfcd14632726590fdef262`，`src/apps/phone_store.cpp`（SHA-256 `026492851b85169a6b7c16fe251b06c55ac336cd090c715f93864847a0e8db0d`，本机只读副本 `ink-hw/upstream/oss/lilyphone1`）：
  - `phone_number_match` `:111-135`：已在 `system/apps/phone/vendor/lilyphone/phone_store.c:23-47`，`MATCH_DIGITS` 是 11。
  - `contacts_find_by_number` `:190-196`、`contacts_display_name` `:198-203`：同一文件 `:49-62`。

## 搬代码（总规则第 11 节）

**结论：本任务没有新的原文要拷。**写进 `docs/tasks/contacts-3/SOURCE.md`，理由如下：

1. **Fossify Contacts**：Kotlin 加 Android 的 Contacts Provider，导入导出靠 ez-vcard（Java 库，`VcfImporter.kt:54` 的 `Ezvcard.parse`，`VcfExporter.kt` 的 `Ezvcard.write`）。这些在 S31 上都跑不了，只能照行为重写。另外它是 GPL-3.0。
2. **LilyPhone**：
   - 号码比对的三个函数已经由 phone-3 原样拷进 `system/apps/phone/vendor/lilyphone/phone_store.c`，用 C 编进 `ink_system`；S31 用通配收进来，主工程在 `CMakeLists.txt:1810`。联系人和电话在同一个库里，所以联系人直接调 `phone_number_match`，声明用 `lily_compat.h:21` 已有的那行。**不再拷第二份。**
   - 其余函数不搬：`contacts_read`（`:138-163`）、`contacts_save`（`:165-188`）、`contacts_add`（`:223-250`）、`contacts_update`（`:252-269`）、`contacts_remove`（`:271` 起）、`contacts_reorder`（`:207-221`）。证据：它们读写 SPIFFS 上的一个 tab 分隔文件，每改一条整文件重写（`:165-188`），我们的存储是数据服务（`ink_db_put`，按家人分开，有版本号）。`contacts_reorder` 是按名字插入排序，电话的 `load_contacts` 里已经有同样的插入排序。phone-3 的 `SOURCE.md` 也写了“改通讯录是联系人应用的事”。
3. **vCard 读写是我们自己写的**，放在单独的文件 `system/apps/contacts/contacts_vcf.c`，接口是纯函数，主机上能单测。必须做到 ez-vcard 替 Fossify 做的这几件事：
   - 续行展开：下一行以空格或制表符开头就是续行；
   - QUOTED-PRINTABLE 解码，包括行尾 `=` 的软换行；
   - `CHARSET=UTF-8`；
   - `item1.TEL` 这样的分组前缀；
   - `TEL;CELL:`（2.1 的写法）、`TEL;TYPE=cell,voice:`、`TEL;TYPE="cell"`、`TEL;VALUE=uri:tel:+86…`；
   - 转义 `\n`、`\,`、`\;`；
   - 跳过 `PHOTO` 这类长块，不把它们读进内存。
4. **`SOURCE.md` 记什么**：
   - 每一页对应 Fossify 的哪个文件哪几行（对照表里的出处，汇总一遍）；
   - LilyPhone 复用了哪三个函数、在哪个提交、文件哈希，以及“只调用不拷贝”；
   - LilyPhone 其余函数为什么不搬（上面第 2 条）；
   - vCard 读写照了 Fossify 的哪些行为、哪些没照（类型只留 4 种、导出写 3.0 等，和对照表 E、F 两节一致）。
5. **改电话的胶水**：`system/apps/phone/vendor/lilyphone/lily_compat.h` 是我们写的胶水，不是原文。改 `CONTACTS_MAX` 的含义不动原文改动的计数，但要在 `system/apps/phone/vendor/lilyphone/SOURCE.md` 的“胶水”一节补一句（`:66-67` 两行）：表现在一个号码一行；`load_contacts` 改为经 `ink_contacts_each` 读。

## 要做的（每一行的决定见对照表）

1. **记录格式升到版本 2，老记录自动能用（D-138）**：
   - 把集合定义里的 `.schema` 从 1 改成 2（`contacts.c:13-14`，照 memo-3 的 `memo.c:22`）；老备份（版本 1）仍能还原，版本 2 的备份还原不到老固件，这点写进报告。
   - 新记录：`{"name","number","note","numbers":[{"n":"…","t":"mobile|home|work|other"}],"default":<下标，没有就不写>,"star":true}`。
   - 顶层 `number` 始终等于默认号码；没设默认时等于第一个号码。它是给老读法用的副本，全局搜索、删除确认卡、老固件都读它，只由 `record` 写。
   - 读的时候不看版本号，看有没有 `numbers`：没有就当成“一个号码、类型手机、没有默认、没收藏”。
   - 不在开机时把 200 条全部重写一遍：老记录照常能用，下次写入时变成版本 2。这样 D-138 不用人做任何事，也不额外磨损闪存。
   - 每人最多 5 个号码，同一人的号码按 `phone_number_match` 不能重复。集合上限仍是 200 人。
2. **一份读法，三个入口认同一个人**（APPS.md 的参照路径）：
   - `ink_contacts_match(query, exact, name, number)` 签名不变，返回值的约定不变：-1 表示还没读进来，0 表示没有，否则是编号。exact 模式改用 LilyPhone 的 `phone_number_match`，和电话一样，从此短信、勿扰、电话认人的规则一样。这是有意改行为，要做反向对照。非 exact 模式：姓名包含，或者任一号码包含；`number` 填命中的那个号码。
   - 新增 `int ink_contacts_each(const char *member, void (*fn)(void *ctx, uint32_t id, const char *name, const char *type, const char *number, bool star), void *ctx)`：每个号码回调一次，**按姓名（UTF-8 字节）排好序给**，同一个人的号码相邻，默认号码在前；只读已缓存的集合，可以在界面任务里调，返回规矩同上。排好序给，是为了电话那边 1000 行的表不用再做二次方的插入排序。
   - 新增 `bool ink_contacts_starred(const char *number)`：当前家人里，这个号码是不是收藏的人的。
   - 两个新函数都照 `ink_contacts_match` 的老办法，由调用方用 `extern` 声明，不新加头文件。
   - **电话**（只动这几处）：
     - `load_contacts`（`phone.c:179-195`）的函数体换成调 `ink_contacts_each`，一个号码填一行，照旧调 `lily_contacts_bind`；因为回调已按名字排好，原来的插入排序循环去掉，改成顺序追加。函数名和各处的 `    load_contacts(); /* this member` 那一行不改，`tests/phone_3/run.py:94-97` 的反向变异靠它。
     - `lily_compat.h` 的 `CONTACTS_MAX` 改成 200 × 5 = 1000 行，PSRAM 表约 128 KB（`contact_t` 122 字节 × 1000，加 `book_id`、`cand`），报告写实测增量。
     - **三处**“加为联系人”（`:358` 拨号页快照里的动作、`:486` 按钮处理、`:800-801` 最近通话详情的动作）改调 `contacts.add_to`，用 `args_json` 把号码带过去（`ink_ui_action_spec_t` 有 `args_json` 字段；`phone.c` 里加放参数的缓冲，照 `P.dial_args` 的做法）。
     - 因此 `tests/phone_3/phone_3.c:473` 要改：断言快照里含的能力名从 `contacts.edit` 改成 `contacts.add_to`，并加一条断言号码在参数里。这是 phone-3 的测试里唯一改动的一处，其余 `phone-3`、`phone-3-match`、`phone-3-reverse` 原样通过。
   - **勿扰**：`notify_model.c:116` 加上 `|| (calls == INK_NOTIFY_CALL_STARRED && ink_contacts_starred(number))`，一行，再加一行 `extern` 声明。`tests/notify_model.c` 末尾追加一条。
3. **列表**（Fossify 的联系人页签和收藏页签）：
   - 一张单列分页列表，用 `ink_ui_list` 和共用翻页条，盖板键能翻。
   - 收藏的人排在最前，行上写“收藏”两个字，不靠星形或颜色；收藏、其余各自按名字排。中文排序仍按字节，写进报告。
   - 每行写姓名，第二行写默认号码，多个号码时后面写“等 3 个号码”；同名的人写“同名”。
   - 打开页面时，把每人的姓名、默认号码、号码个数、收藏标记读一遍，放进 PSRAM 里的行表。排序和搜索只用这张表，不再一边排一边解析 JSON。
   - 顶上搜索框：输入一变就筛（照 Fossify），只刷列表区。搜姓名、任一号码（只比数字）、备注；开头命中的排前。
   - 底部主按钮“添加联系人”；顶栏右边文字按钮“导入导出”。
   - 整页空用共用空状态“还没有联系人”，配“添加联系人”。搜不到写“没有匹配的联系人”。
   - 点一行进详情（Fossify 默认就是这样，也是“打开先只读”的规则）。**挑选模式**（`contacts.pick`）和**电话模式**（电话的“联系人”页签）点一行是选号码，见第 6 条（电话模式照 phone-3 对照表 H1）。
4. **详情页**（Fossify `ViewContactActivity`，号码行照 Lineage `:1730-1802`）：
   - 只读。标题是姓名，顶栏右边两个文字按钮：“收藏”或“已收藏”、“编辑”。
   - 下面先是号码，每个号码一行：类型和号码，默认的写“默认”；行下面两个文字按钮“打电话”（调 `call.dial`）、“发短信”（调 `sms.compose`），参数都是 `{"number":…}`。默认号码排第一，其余按手机、家里、工作、其他排。
   - 然后是备注，没有就不画。
   - 有重复时多一块：“还有 1 位同名的联系人：张三 · 138…”，配按钮“合并到这位”（见第 8 条）。
   - 删除不放这里，放在编辑页（D-105 确认照旧）。
   - 放不下时号码区按页翻，不缩字。
   - 找不到这个人（已被删）时写“找不到这个联系人”，配“回到列表”。
   - 点“收藏”调 `contacts.star`，立即写，只刷这个按钮。
5. **编辑页**（Fossify `EditContactActivity`）：
   - 一个“姓名”框；每个号码一行，左边类型按钮写“手机 ▾”，右边号码框用电话键盘（`INK_TEXT_PHONE`）。下面“加一个号码”（最多 5 个，到了写“最多 5 个号码”）。然后“备注”框。最下面主按钮“保存联系人”。已有的联系人再多一个“删除联系人”。
   - 点类型按钮，弹共用底部面板（`ink_ui_panel`），四行“手机、家里、工作、其他”。有两个以上号码时，再多一行“设为默认号码”。
   - 删一个号码就是清空那个号码框（共用输入框自带 108 px 的清除键），保存时丢掉（照 Fossify `:1247-1271`）。号码框下的提示写“清空就删掉这个号码”。
   - 校验：姓名必填，至少一个号码，号码字符规则照旧。错误写在对应的框上（`ink_textfield_error`），字一个不丢。
   - **保存要调两次能力**（参数表 416 字节的限制）：先 `contacts.put`（姓名、备注、默认号码），回执给出编号和版本；号码列表或类型、默认有变化时，再调 `contacts.numbers`（带 `if_revision`）。最后一个回执回来才返回，只刷一次。第二步失败时留在本页，写“姓名已存，号码没存上，请重试”，框里的字都在。
   - 带号码进来：新建时预填（`contacts.edit {number}`）；已有的人追加在号码最后（`contacts.edit {id, number}`，照 Fossify `ADD_NEW_CONTACT_NUMBER`），并把焦点放在那个框上。
   - 有改动时按返回：照 Fossify 出一个面板，“保存”“不保存”两个按钮。要先核实外壳的返回能不能拦（`ink_activity_t` 只有 `on_key`，`system/include/ink/sys/ui.h:44`，顶栏返回走 `shell.back`）。拦不住就写进报告，不改外壳。
   - 别处同时改了这条：照旧用 `if_revision` 报“联系人已被修改，请返回列表重新打开后重试”。被删了：照旧退出编辑页（`refresh`）。
6. **选号码和“加到已有联系人”**（Fossify `InsertOrEditContactActivity`、Commons `initiateCall`）：
   - 挑选模式和电话模式下点一行：
     - 只有一个号码，或者设了默认：调 `contacts.select {id}`，直接返回；
     - 多个号码又没默认：调 `contacts.choose {id}`，进“用哪个号码”页，每行“类型 · 号码”，点一行调 `contacts.select {id, number}`。
   - `contacts.select` 新增可选参数 `number`，必须是这个人的号码之一（按 `phone_number_match`）。不给 `number`，又有多个号码、没默认时，回 `INVALID`，`say` 里列出各号码（“有 2 个号码：手机 138…、工作 010…，请说用哪一个”），AI 再带上 `number` 调一次。
   - 输出 `{id, number}` 不变；照旧发 `contacts.selected`。短信、电话收这个事件的代码不用改。
   - `contacts.add_to {number}`：列表进“加号码”模式，第一行“新建联系人”（调 `contacts.edit {number}`），点别的人调 `contacts.edit {id, number}`。
7. **导入导出 vCard**（Fossify `VcfImporter`、`VcfExporter`、设置里的导入导出）：
   - 列表顶栏“导入导出”进 `ContactsTransfer` 页，两行：“从存储卡导入（.vcf）”“导出到我的文件”，各写一行说明。
   - **导入**：
     - 点“从存储卡导入”调 `contacts.import_pick`，打开系统文件选择器 F9（`ink_file_picker_open(INK_FS_OTHER, …)`）。选好后调 `contacts.import {file}`；AI 可以直接给文件编号。
     - 在后台用 `ink_fs_lookup` 找到路径，检查扩展名是 `.vcf`（不分大小写），分块读进 PSRAM 再解析。
     - 认 `FN`；没有 `FN` 就把 `N` 的姓和名拼起来，中文不加空格；都没有就用 `ORG`。认 `TEL`（类型对照：`CELL` 是手机，`HOME` 是家里，`WORK` 是工作，其余都是其他；带 `PREF` 的是默认），认 `NOTE`（截到 64 字）。
     - 每人最多取 5 个号码，多出的计数。别的字段跳过，不算看不懂。
     - 查重：一张卡的号码全都已经在通讯录里（按 `phone_number_match`），就跳过并计数；部分号码已有，就只带新号码导成一个新联系人；同名不同号的也导成新联系人，列表和详情会标出同名（见第 8 条）。
     - 不是 UTF-8 的字符集（例如 GBK）、格式坏的卡，计入“看不懂”。没有号码的卡不导入，单独计数（我们的联系人必须有号码，Fossify 允许没有，列进“要用户定”）。到 200 人就停，计数。
     - 结果页一直留着，直到下一次操作：“导入 3 位；跳过 1 位（号码都已在通讯录里）；1 条看不懂”，为 0 的项不写，配“回到联系人”。
     - 文件不是 `.vcf`、没有存储卡、读不出来的，结果页写原因。
     - 选好文件后先画一行静态的“正在导入，请稍候”，不转圈、不跑计数，导完换成结果，只刷两次。
   - **导出**：
     - `contacts.export` 把当前家人的全部联系人写成一个文件，路径是 `/me/联系人-YYYYMMDD.vcf`，重名时依次加 `-2`、`-3`，不让人打文件名。用 `ink_fs_write_atomic` 写。
     - 写 vCard 3.0，UTF-8，CRLF 换行，超过 75 字节折行，折行不能切断汉字。字段：`FN`、`N`、`TEL;TYPE=CELL/HOME/WORK/VOICE`，默认号码加 `PREF`，有备注写 `NOTE`。收藏不导出，Fossify 也不导出。
     - 结果页：“已导出 12 位到 我的文件 › 联系人-20261114.vcf”。没人可导出时写“还没有联系人，没有可导出的”。没卡或写满时写原因。
     - 写进自己的空间不算“发出去”（D-105），不弹确认。
   - 导出后再导入同一个文件，全部算重复跳过（验收第 3 条）。
8. **重复的看得见、能合并**（代替 Fossify 默认开着的“显示时合并同名”）：
   - 同名：姓名去掉首尾空白后相同，英文不分大小写。行表排好序后同名的挨在一起，建表时就能标出来。
   - 同号：只在打开详情时，拿这个人的号码去比全部号码，不在每次搜索时比。
   - 详情页的重复块列出另一位或几位，按钮“合并到这位”调 `contacts.merge {id, from, if_revision}`：
     - 号码取并集，按 `phone_number_match` 去重，超过 5 个就拒绝并说明；
     - 备注用“；”接起来；
     - 有一位收藏，合并后就是收藏；
     - 最后删掉 `from`。
   - 资料一样不丢，所以不算 D-105 的“找不回来的删除”，不弹确认。合并后拆不回来，列进“要用户定”。
9. **能力表**（现有 13 个能力名不改；新能力都挂在应用 `phone` 下，`child_ok` 和现有的一样；全系统能力登记上限 `MAX_CAPS` 是 1024，放得下）：
   - **改的**：
     - `contacts.put`：标题去掉“每人一个号码”。新建时只有一个号码，类型手机；改的时候 `number` 替换默认号码（没有默认就是第一个），其余号码不动。老调用方（扫码、AI、测试）照常能用。
     - `contacts.select`：加可选的 `number`。
     - `contacts.edit`：加可选的 `number`。
     - `contacts.number`：加可选的 `index`，指聚焦第几个号码框。
   - **新的**：
     - 写数据：`contacts.numbers`（`id`、`if_revision`、`numbers`、可选的 `default`，写入，后台执行）、`contacts.star`（`id`、`star`）、`contacts.merge`（`id`、`from`、`if_revision`）、`contacts.import`（`file`）、`contacts.export`。
     - 导航：`contacts.choose`（`id`）、`contacts.add_to`（`number`）、`contacts.transfer`、`contacts.import_pick`。
     - 编辑页表单：`contacts.number_add`、`contacts.number_type`（`index`、`type`）、`contacts.number_default`（`index`）。
   - **快照**：
     - 列表每行写姓名、默认号码、号码个数、收藏、同名；
     - 详情写每个号码（类型、号码、默认）、备注、收藏状态、重复的那几位（带编号）；
     - 编辑页把两步保存的参数都放进动作里，AI 照着调就和按按钮一样；
     - 导入结果写各项计数。
10. **Fossify 有、这次不做的**：邮箱、地址、生日和纪念日、即时通讯、网址、公司和职位、分组、头像照片、铃声、昵称和分段姓名、自定义类型和传真、寻呼类型、分享名片、长按复制号码、长按多选（批量删除、批量收藏、群发短信）、快捷方式、排序方式、联系人来源、网格视图、自动备份、拨号盘按钮。报告里逐项写“没做到”，不许写“不需要”。

**做不完时的顺序**（从后往前停，写进报告）：第 1、2 条 → 第 4、5、6 条的选号码 → 第 3 条 → 第 7 条 → 第 8 条 → 第 6 条的“加到已有联系人”和第 5 条的“离开前问保存”。

## 按设备改（每条在对照表里有对应的行）

- **墨水屏**：
  - 不用过渡、头像放大动画、滚动、字母快速滚动条、转圈、跑数字；长列表按页翻，写页码，盖板键能翻。
  - 一次操作刷一次屏，只刷变了的那一块：搜索只刷列表区，收藏只刷那个按钮。打开类型面板一次，选好一次。
  - 被动更新（别处改了联系人）用 `ink_ui_redraw` 或 `ink_ui_after_press`，按着手指时等抬起；只有页面显示的内容真的变了才调 `ink_ui_content_changed`。
  - 按下的反馈只刷按下的那个控件。
- **灰阶**：收藏、默认、同名都写成字，不用星形、颜色或变淡。
- **触摸**：能点的 108 px。安卓靠长按、浮动按钮、溢出菜单、单选对话框、点星做的事，都换成写着字的固定按钮或共用底部面板。
- **提示**：不用自动消失的提示条（Fossify 的导入导出结果都是提示条）。结果写在页面上，留到下一次操作。
- **资源**：行表、vCard 读缓冲放 PSRAM（`INK_PSRAM_BSS` 或 `ink_app_alloc(…, true)`）；不后台轮询；整个应用都是本机数据，不联网，没有“没网”状态。
- **我们自己的规矩**：每个用户操作都是能力，按钮和 AI 走同一个入口；快照写清显示了什么；家人隔离、孩子照旧；所有东西跟系统文字大小一起放大（`ink_ui_px`），放不下就分页或换行，不缩字、不裁按钮。

## 共同要求

- **新设计语言**（design-lang-2 已合入）：只用令牌和共用控件（`system/ui/ink/ui_theme.h` 里的 `ink_ui_list_row`、`ink_ui_pagebar`、`ink_ui_empty`、`ink_ui_text_button`、`ink_ui_pill`、`ink_ui_panel`、`ink_ui_notice`，`system/ui/ui_list.c`，`system/ui/textfield.c`）。
  - 一屏最多一个主按钮。
  - 头像以外不画圆圈、不画粗框。列表的首字头像可以留，详情页不画大头像。
  - 没内容的区块不画，整页空才用共用空状态。
  - 不写死像素和颜色。手动跑 `python3 tools/ui_scale_check.py system/apps/contacts`，要 0 发现。
- **能力和共享文件**：新能力进 `tests/integration/ai_surface.c` 的两张表，给 AI 的写清必填参数，不给 AI 的写理由。`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交。新文案的英文加进 `assets/i18n/en.tsv`。这些都只在末尾追加，合并冲突交主会话。
- **不变的规矩**：家人隔离；孩子用自己的联系人，现有权限照旧；确认卡只给 D-105 的高风险动作（删除联系人照旧确认；导出到自己的空间、合并都不弹）；按下反馈照 D-31；`contacts.open` 照旧锁屏时只给主人。
- **原地重写**：旧页面代码删掉，不留两套。可以拆成 `contacts.c`（数据、能力、对外接口、列表）、`contacts_pages.c`（详情、编辑、选号码、导入导出页）、`contacts_vcf.c`（vCard 读写）。电话的应用登记（`contacts.c:303`）保留。合不合入、新旧怎么换，等用户看过预览后由主会话定。
- **截图**：放 `docs/tasks/contacts-3/`，单张不超过 300 KB，要干净的整页图。默认字号、最大字号、孩子身份各一套。

## 范围和预算

- **只改**：
  - `system/apps/contacts/`（可以拆文件）；
  - `system/apps/phone/phone.c` 里的 `load_contacts` 函数体和三处“加为联系人”（`:358`、`:486`、`:800-801`，含放参数的缓冲）；
  - `system/apps/phone/vendor/lilyphone/lily_compat.h` 的 `CONTACTS_MAX`，以及同目录 `SOURCE.md` 胶水一节的两行；
  - `system/services/notify/notify_model.c:116` 的一行和一行 `extern`；
  - 联系人相关测试：新加 `tests/contacts_3/`，带明显是假的 `.vcf` 夹具，号码用 `00000000xxx` 或 `138 0000 0xxx` 这类；
  - 确实要改的已有测试（见验收第 9 条的清单，含 `tests/phone_3/phone_3.c:473`），在 `tests/notify_model.c` 末尾追加一条；
  - CMake 里新文件的那几行：主工程 `CMakeLists.txt:473`、`:1755-1759` 附近加新测试；S31 `components/ink_system/CMakeLists.txt:37` 的 i18n 列表；
  - `docs/tasks/contacts-3/`；
  - 共享文件的末尾追加。
- **不碰**：
  - `system/apps/sms/`（sms-3）；`system/services/comm/comm_service.c`；
  - 电话的其余部分和 `vendor/lilyphone/*.c` 的原文；
  - `system/ui/search.c`（全局搜索点开进编辑页的问题写进报告，由主会话合并时改 `:269` 一个词）；
  - `system/ui/file_picker.c`、`system/services/file/`，只调用；
  - `system/include/ink/sys/args.h`，不放宽 416 字节；
  - 主题和共用控件；
  - 外壳的返回处理。
- **产品代码不超过 750 行**：
  - 算法：上面“只改”的产品文件里新增和改动的行，删掉的旧代码不抵。
  - 交付计划里这一行写的是 500（`docs/tasks/delivery-plan.md:66`），旧任务书也是 500，这里放宽到 750，请主会话确认。原因有四：
    - 重复处理是这次明确加的；
    - 电话三处接线、勿扰接线和“加到已有联系人”是 phone-3、notify-svc-1 交来的；
    - 参数表放不下，号码只能单独开一个能力，编辑页要分两步存；
    - 联系人要给电话一份排好序的表。
  - 估算：数据和迁移、对外接口约 95 行；列表约 70；详情约 80；编辑页（多号码、类型面板、两步保存）约 110；选号码和加到已有约 40；导入导出页和接线约 70；vCard 读写约 140；合并约 40；能力和快照约 50；电话、勿扰接线约 20。合计约 715，留 35 行余量。超了按上面的顺序停下来报告，**不要靠把代码挤成一行凑数**。
- **固件增量不超过 15 KB**：旧的 327 行删掉后抵一部分。应用分区剩约 756 KB（14%），15 KB 在 10% 余量内。电话的 PSRAM 表增量单独写，不算固件。
- **起步**：从 `claude/system-layer` 的 `3baaf0e2` 开始，wave 2a 的 phone-3、memo-3、files-3、gallery-3、music-3 都已合入。同批任务中途合入就 rebase。

## 验收

1. **对照表**：先单独提交。做完后每行写上截图名或测试名，报告写做到了几行、没做几行、各自的原因。
2. **照参照路径走一遍**（APPS.md A03，新测试 `contacts-3`，每一步都截图并断言）：
   - 新建“测试甲”，两个号码：手机 `138 0000 0021`、工作 `010 0000 0022`；
   - 收藏他 → 回列表，他在第一行，写着“收藏”；
   - 电话拨号页输工作号的一部分，候选里有他；
   - 短信点“选联系人”，点他，进“用哪个号码”页，选手机号，写短信页的收件人是手机号；
   - 联系人详情里，点工作号下的“打电话”，通话页写“测试甲”；
   - 来电号码写成 `+8613800000021`，通话页、最近通话、短信会话都显示“测试甲”；
   - 三个入口找到的是同一个编号。
   - 同一条路径用能力调用、来源是 AI，再走一遍，结果一样。AI 不带 `number` 选他时回 `INVALID` 并列出两个号码。
3. **导入导出**（新的主机测试 `contacts-3-vcard` 测读写函数；模拟器里测整条流程）：
   - 夹具 `five.vcf` 有 5 张卡：一张 2.1 版、QUOTED-PRINTABLE 编码的中文名；一张 3.0 版带 `TYPE=WORK`；一张带 `PREF`；一张号码已在通讯录里；一张格式坏的。
   - 结果页写“导入 3 位；跳过 1 位（号码都已在通讯录里）；1 条看不懂”；列表多了 3 位，类型和默认都对。
   - 导出后再导入同一个文件，全部算重复。导出文件里的姓名、号码、类型、备注和库里一样。
   - 选到不是 `.vcf` 的文件、拔掉存储卡（`sim_sd_set_present`），结果页各写原因；写满（`sim_sd_set_free`）时导出结果页写原因。
4. **老数据**：
   - 直接写入版本 1 的记录：读出来是一个“手机”号码；电话来电查名、拨号匹配、短信显示名字、勿扰都照旧。
   - 对老记录调老参数的 `contacts.put`：变成版本 2，备注不丢。
   - 全局搜索仍能按姓名和默认号码搜到。
   - 版本 1 的备份能还原。
5. **认人规则一致**：
   - exact 模式下，`+8613800000001` 能找到存成 `138 0000 0001` 的人；`139 0000 0001` 找不到。反向对照：exact 换回 `strcmp`，第一条必须失败。
   - 勿扰 `calls=3` 时，收藏的人来电放行，没收藏的不放行。反向对照：去掉 `notify_model.c` 那一行，测试必须失败。
6. **家人隔离**：A 的联系人、收藏、导出的文件，B 都看不到；孩子只看到自己的。
7. **截图**：改前和改后各一套，包括列表（有收藏、有同名）、搜索无结果、整页空、详情（多号码、有备注、有重复）、编辑页（类型面板打开）、用哪个号码、导入导出页、导入中、导入结果、导出结果、加号码模式、找不到这个联系人。默认字号、最大字号、孩子身份各一套。
8. **刷新次数**：照 `tests/settings_1.c:283-291` 的打标记方式，报告每个主要操作刷了几次屏、刷了多大面积。目标：
   - 打开列表 1 次；搜索框内容每变一次 1 次（只刷列表区）；打开详情 1 次；收藏 1 次（只刷按钮）；
   - 打开“用哪个号码” 1 次，选好回到调用方 1 次；
   - 打开编辑页 1 次；加一个号码 1 次；类型面板打开 1 次、选好 1 次；保存 1 次（两步保存也只刷一次）；
   - 删除：确认卡 1 次，确认后 1 次；
   - 导入：选择器 1 次，正在导入 1 次，结果 1 次；导出 1 次；合并 1 次。
9. **构建和测试**：
   - 主工程普通全量 ctest 通过，另外手动跑一遍联系人目录的 `ui-scale`；
   - 用 ASan 跑 `contacts`、`contacts-3`、`contacts-3-vcard`、`phone-3`、`phone-3-match`、`phone-calls`、`sms`、`notify-model`、`search`、`ui-scene-*`，都要通过；
   - 反向对照 `contacts-3-reverse`，照 `tests/phone_3/run.py` 的做法逐个变异：exact 用 `strcmp`、不展开续行、不解 QUOTED-PRINTABLE、去掉收藏放行那一行、去掉版本 1 的读法。每个都必须让对应的测试失败。
   - 写 `contacts3_open_fail`、`contacts3_save_fail`（注入联系人数据库的读、写失败）前，先找到仓库里现成的存储故障手段（例如 `tests/integration/data.c` 用的）；没有可用的，就把这两条改成主机单元测试，并在报告里写明。
   - S31 宿主机测试通过；`ports/esp32s31/build.sh` 编译通过、没有新警告，日志里没有“没有屏幕输出层”。
   - 报告写：固件增量、电话 PSRAM 表增量、产品代码行数（按上面的算法）、`gcccheck` 结果、新增汉字数、改过的已有测试（逐个列出改前改后，只改定位方式和页面结构，业务断言一样严）、改名或新增的能力。
   - 预计要改的已有测试：
     - `tests/contacts/contacts.c:159-163`：编辑页的“选择这个号码”没了，改成从挑选模式的列表点；
     - `tests/contacts/run.py:26-40`：编辑页快照的字段和动作，每页行数可能变；
     - `tests/phone_calls/phone_calls.c:135-138`：电话模式下点列表行直接选号码，回到拨号页；
     - `tests/phone_3/phone_3.c:473`：快照里的能力名从 `contacts.edit` 改成 `contacts.add_to`（见“要做的”第 2 条）；
     - `tests/i18n_apps.c:189-194`：`ContactEdit` 的截取文字；
     - `tests/comm_ux2/comm_ux2.c:94-103`、`:134`、`:147`；
     - `tests/kid_3.c:253`。

## 预先列出的“要用户定”（报告里照这个格式补全）

- **中文按拼音排序**：系统没有汉字转拼音的公开接口，这次照旧按字节排。
- **Fossify 这次不做的**：见“要做的”第 10 条，逐项列。
- **没有号码的联系人**：Fossify 允许，我们导入时跳过并计数。
- **自定义号码类型、传真、寻呼**：导入时都归成“其他”。
- **每人最多 5 个号码**。
- **非 UTF-8 的 vCard**（老手机导出的 GBK）：计入“看不懂”。
- **合并后拆不回来**：资料不丢，但要拆开得手动编辑。
- **文件应用里点 `.vcf` 直接导入**：照 Fossify `MainActivity.kt:380-397`，要 files-3 的打开方式调 `contacts.import`。
- **勿扰设置页还没有“只放行收藏的人”的选项**：现在只能经 `notify.dnd_set calls=3` 设。
- **全局搜索点开联系人进的是编辑页**：`search.c:269` 改成 `contacts.view`（该能力已存在，带 `id`），由主会话合并时改。
- **短信 `sms.recipient` 带 `number`**：sms-3 可以给它加可选参数，原样转给 `contacts.select`。在那之前，AI 经 `sms.recipient` 选一个多号码又没默认的人，会得到 FAILED，看不到“请说用哪一个”（`sms.c:157` 把 `contacts.select` 的失败一律记成 FAILED）；直接调 `contacts.select` 才有说明。
- **产品代码预算从 500 放宽到 750**：见“范围和预算”。

## 和同批任务的关系

- **sms-3**：本任务不碰 `system/apps/sms/`。`ink_contacts_match` 签名不变，`contacts.pick`、`contacts.select`、`contacts.selected` 对单号码的人行为不变；多号码时多一步“用哪个号码”，都在联系人这边做完。sms-3 如果改了会话里显示名字的写法，照旧调 `ink_contacts_match(…, true, …)`，就能自动得到新的认人规则。
- **keyboard-2**：号码框用 `INK_TEXT_PHONE`，用什么键盘布局归 keyboard-2，本任务不改键盘。
- **settings-1-check、home-1-check、control-1-check**：可能动 `system/ui/search.c` 或外壳，本任务都不碰。
- **clock-3、calendar-3、small-apps-3**：没有交集。
- 大家都会动的只有 `assets/i18n/en.tsv`、`tests/integration/ai_surface.c`、`docs/system/capabilities.md` 和 CMake 的源文件列表，一律只在末尾追加，冲突交主会话处理。
- phone-3、notify-svc-1 都已合入，它们留给本任务的接口按上面第 2 条接上。phone-3 的测试 `phone-3`、`phone-3-match`、`phone-3-reverse` 除 `phone_3.c:473` 一条断言（能力名随“加为联系人”改调 `contacts.add_to`）外必须原样通过。
