# M014 掌阅 App 交接（冻结）

2026-10-27。限文档研究；0新E、0新F、0深度升级、0静态图观察、0实机/账号动作。手机F01–03保持D1，F04/05限MatePad Paper适配版，F06仍为2024政策局部路径。F06已按root审校改成“关闭推荐不证明清除阅读记录”，不把未说明删除写成已证明不删。

## 文件与合并项

正式修改仅5项：
- `docs/research/interaction-library/cases/M-014-ireader.md`
- `docs/research/interaction-library/batches/ireader-controls-deepening-evidence.jsonl`（空，0E）
- `docs/research/interaction-library/batches/ireader-controls-deepening-coverage.csv`（标准表头，0F）
- `docs/research/interaction-library/batches/ireader-controls-deepening-inventory.csv`（45行：17手机功能域、18墨水屏问题、4子分支、6版本条目；不是45条任务或完整手机目录）
- `docs/research/interaction-library/artifacts/ireader-controls-deepening/source-notes.md`

只交建议，未改旧批/主表/全局：
- `work/ireader-controls-deepening/existing-coverage-updates.json`：6个旧F、18字段，精确before/after。
- `work/ireader-controls-deepening/existing-evidence-additional-captures.json`：E-IRD-01/03与E-IPAD-01，3条追加采集；同URL不重编号。
- `work/ireader-controls-deepening/existing-evidence-field-updates.json`：E-IPAD-01 locator一字段修正；旧“第二节”实际为第一节内扩展业务，不改变功能判断。

## 核心依据与核验入口

1. https://apps.apple.com/cn/app/id463150061 ：本轮普通GET HTTP200，原件`work/ireader-controls-deepening/app-store.html`；HTML版本历史TitledParagraph确认为8.6.2、2026-10-22 UTC。`app-store-version-index.json`保留原日期字符串；txt103–142给顺序。8.5.1是标点停顿，8.5.2是外部音乐中断，不能反配。原件SHA `c11853a5986a75b5e8fc2f2227f3f07931d67fda841d374a34e20eddb943abef`。当前快照与旧商店引用/搜索索引分开，未采用评论/图片。
2. https://consumer.huawei.com/cn/support/content/zh-cn15939571/ ：复读旧HTML，未GET；`work/research/extension-manuals/huawei-15939571.txt`69–80定位型号/系统，84–157是4类18问。系统预装门槛和App1.8.4 LTE条件分开；F04刷新/归属/重复文件，F05隐藏/重排/撤销/分屏退出。本轮只读文字，不把相关图片算观察。HTML SHA `39e7af2a2b8d1c0593ca18b682cdab7d701c99516afeaa5fd10a14caa540068a`。
3. https://ah2.ireader.com/fe3/zybook/other/statement/privacyagreement_detail.html ：复读2024-06-27政策原件，未GET；`work/chinese-app-depth/ireader-privacy.txt`28游客、39–43记录/分享、62–94书童/推荐/权限、169后台备份。仅分享内容存储，不能推成所有批注同步；副本/后台备份不是恢复接口。HTML SHA `93d47df113271ae28e807eb354f45b3fac14e09a38dd0e7b79156a46c6226d1b`。

E-IRD-02年报沿用旧证据，本轮未重新取得，不增加capture。没有旧visual_audit_path依赖case；基线和完整匹配检查在baseline.json。无迁移需求。

## 检查与精确命令

最终`python3 work/ireader-controls-deepening/build.py`和`python3 work/ireader-controls-deepening/check.py`均exit0，后者245 PASS；此结果在F06范围修正和案例末尾中性资料状态修订后（第3次本批检查）。检查before匹配、受影响旧批未动、目录引用/计数、LF、6F集合、原件和正式sha以及同URL追加采集。`commands.md`列实际读文件/命令、前一次python子命令127和无状态修改的patch拒绝；这些失败均已处理。

合并前可重跑：
```
python3 work/ireader-controls-deepening/check.py
sha256sum -c work/ireader-controls-deepening/manifest.sha256
```
合并旧F/E后不要重跑依赖before的check/build。本批没有执行任何旧Toutiao/Huawei检查或构建。manifest列26个文件，含正式/工作/复用原件；不含其自身和本HANDOFF。本HANDOFF后不再修改正式资料/建议。

正式散列：
- case `e2ecf35cb089b4190c2b6b97c82830051dc59f1d5f1cff17855971662ffe1ee7`
- source-notes `a39c61c2ea45ed2a9bad9052dd0d45b6a8173ac7c8f3cdaf45f9382779c2c6c2`
- inventory `043a81c158aff557f8df7c22a6921110845a9d31ccb55aac621a7ff79caac64f`
- F建议 `4612676b898028304fe7a613879e9999dff8c6cea6d75a967342c17e48c5e406`
- E追加 `3ab422e1afe0d09eb624cbd05a8dd64d59612ce62330ee687c41c28689a6f6e4`
- E定位修正 `1dfb51f35eafac791c79f18bb64410bcabc4a7109625bd6f07c7008628d304b5`

## 不确定性与状态

未取得手机完整帮助/设置目录；选书选文/查词划线、导入解析、AI引用与纠错、听书控制及恢复仍缺全链。不把开发者修复声明当效果验证，不把FAQ“当前版本”扩张到所有构建。仅有限检索，查询/负结果为过程记录，不算新证据。旧主站挑战不重试；zhangyue.com robots路径停止，未转协议或伪造身份绕过。没有新增外部产品状态；本地仅本case/本批及work变化。

只读复核`docs/briefs/022-ai-actions-and-resume-conditions.md`第33/41行：掌阅段口径准确，明确手机主链资料不足及MatePad Paper FAQ不能外推iPhone；无修改、无新研究。
