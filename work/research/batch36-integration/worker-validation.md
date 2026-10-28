# Batch 36 worker mechanical validation

日期：2026-10-27。范围只读正式资料；未联网、未运行 `build_once.py`、未修改 `docs/`、未创建测试。

## 已完成

1. 命令：`python3 work/research/interaction-depth/check_docs.py`
   - 退出码：`0`
   - 结果：230个Markdown、1728个本地链接、65组产品、65个partial案例、906条F（813 D2 / 77 D1 / 16 D4）、1518条E、1350个不同来源URL、211条静态视觉证据、19条浏览器操作证据，`errors: []`。

2. 命令：Python只读展开 `work/research/batch36-integration/changes.json`
   - 退出码：`0`
   - 已确认清单声明数量：新7F、新1E、旧9F、旧9E；`applied.json`同样声明7/1/9/9。
   - 尚未完成逐条主表/owning file等值验证；见下方字段错误。

3. 命令：`rg -n 'requirements|e141320|sha256' ... && sha256sum docs/requirements.md`
   - 退出码：`0`
   - 当前 `docs/requirements.md` SHA256：`e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087`。
   - 与 `build_once.py` 第16行的冻结断言及 `root-verification.json` 中的记录相同。此项只证明当前字节哈希等于冻结值；没有独立的本批前后副本差异证明，且不解释需求内容事实。

## 中断：字段错误

按任务要求编写的一次性只读核查脚本尝试将三份目录按固定字段 `entry_id` 建索引，以检查64/82/15条记录和旧ID保留。命令退出码：`1`。错误原文：

```text
Traceback (most recent call last):
  File "<stdin>", line 52, in <module>
  File "<stdin>", line 8, in csvrows
KeyError: 'entry_id'
```

这表示至少一份当前目录的主键字段不是脚本假设的 `entry_id`。错误发生在目录验证阶段，后续同一脚本中的以下检查均未执行，不能报告为通过：

- 三目录64/82/15记录数及旧ID保留；
- 新7F/新1E/旧9F/旧9E与主表、各`owning_files`逐条等值；
- `E-RWGR-01` 的 `hands_on=false`、`local_fixture_run=true`；
- `F-M015-33`仍D2；墨案F05–08仍D1；
- 三个新增`source-notes.md`的本地链接、图像存在性及全部`artifact_hashes`重算。

依照委派要求“有脚本字段错误则立即交回root，不另换路由”，我没有读取表头后修改脚本、没有发起第二种验证，也没有继续散项核查。

## 哈希检查与事实验证的边界

本轮只实际完成了需求文件SHA核对。哈希相等只能证明被计算文件的字节与记录值一致，或当前值与冻结常量一致；它不能证明截图内容对应声明的交互，也不能证明历史手册行为仍适用于当前固件。`artifact_hashes`尚未重算，因此连附件字节一致性也不能在本报告中宣称通过。

## 状态变化与未知

状态变化仅为新建本报告 `work/research/batch36-integration/worker-validation.md`。正式文档未改。由于字段错误按要求立即停止，除全库`check_docs.py`与需求SHA外，其余指定门槛仍是未验证状态，应由root检查实际目录表头后决定如何修正核查脚本。
