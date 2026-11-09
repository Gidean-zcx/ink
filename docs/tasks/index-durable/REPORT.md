# index-durable 执行报告

基线：`1705e7ca4c9e3fde0ff8465f636893a8d9567db4`。当前分支：`claude/codex-index-durable`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 新建、改名后断电恢复 | 通过【实测】 | 原样的 `audit5-b5-02` 四个进程各 10 项检查通过；旧格式迁移、再次启动均保留原编号 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(audit5-b5-02|index-durable)$'` |
| 2 界面活动时文件操作增量 ≤100 ms | 通过【实测，模拟器】 | 同基线同夹具、每种操作各 5 次，均确认与实际刷新交叠；见下表 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_durable/run.py --build build` |
| 3 全量、内存检查、S31 构建 | 未通过；内存检查和 S31 仍在执行 | 普通全量 274 项：267 通过、4 失败、3 跳过；改用依赖齐全的中枢 Python 后，跳过的 3 项补测通过，合计 270/274 | `PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；日志 `work/index-durable/full-tests.log`、`hub-tests.log` |

耗时单位 ms，最大值比较（模拟器虚拟时间，不是实机或光学时延）：

| 操作 | 基线最大值 | 修复最大值 | 增量 |
| --- | ---: | ---: | ---: |
| 新建 | 9.811 | 17.181 | 7.370 |
| 改名 | 13.379 | 23.874 | 10.495 |
| 删除 | 10.971 | 18.327 | 7.356 |

证据：`build/test-out/index-durable/summary.json`、`build/test-out/audit5-index/`。`file-index-3` 最大持锁：普通建库 79,375 μs，界面活动 154,993 μs，原 250 ms 上限未改；`storage-order`、`file-index-regressions` 均通过。

反向对照【实测】：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audit5_fix/run.py --build build --case index --baseline 1705e7ca4c9e3fde0ff8465f636893a8d9567db4` 退出 1。基线 create-reboot 失败 3 项（统计、数量、新编号），rename-reboot 失败 2 项（统计、原编号）；对应修复版均通过。日志：`work/index-durable/reverse-index.log`。

## 没做到的

- `files-app` 的 write 阶段在 `files_app.c:30` 失败：点“我的”后仍停在 Files，未进入 FileList。临时诊断定位到原测试第 230 行的 `wait_page("FileList")`。232 B 追加元数据、216 B 复用字段、复用目录元数据三版均在该处失败，按总规则 §7 停止继续尝试；没有修改原测试、点按脚本或时限。第三轮证据：`build/files-app-3pwofl6p/summary.json`。
- 全量另有三项新回归：`backup-restore-recovery` 在 `device.c:240` 导出合成成员表失败；`settings-b` 在 `settings_b.c:226` 图像锁屏样式上仍有文字；`camera-gallery` 在 `camera_gallery.c:236` 下一页短按未生效，连带 4 项分页/缩略图检查失败。原样全量日志中保留全部失败。
- 【实测，单文件对照】把 `file_service.c` 换回基线并重新链接，files write 为 193 项通过，camera write/Camera 为 159 项通过，backup quick 的 seed/恢复/重启均通过，settings write/restart 为 355/20 项通过。这确认上述四项是本次回归；不是完整基线全量。证据：`work/index-durable/baseline-regressions/summary.json`。尚未确认共同根因；额外索引完成通知与输入/页面刷新交叠是待复核假设。
- 大小、修改时间相同并非内容身份的证明。多个缺失旧记录匹配时分配新编号；单个旧记录与多个新文件元数据相同仍有误认风险。旧格式在旧路径已经消失时没有可用于恢复的元数据，不能承诺找回原编号。
- 未操作设备，未运行 Linux 宿主测试；S31 编译不能替代实机验证。

## 和设计不一致的

- 尚未发现接口或分层偏离。按钮与 AI 继续使用同一文件服务；本任务不新增页面或能力。上述回归使验收第 3 条未满足。

## 改过的已有测试

- 未修改已有测试断言、预算或时限。只在 CMake 重新注册原样的 `audit5-b5-02`，并新增 `index-durable` 对照和旧格式迁移测试。

## 要用户定的

- 暂无；本任务按任务书授权实施。

## 顺手发现

- 首版尝试把记录从 216 B 加到 232 B，触发额外批写和现有写盘次数预算失败；已撤去，现保持 216 B 和原集合预算。扫描复用目录读取已有的修改时间，避免另做一次卡查询。

## 新增的机制

- 卡上空文件 `.file-index-dirty`：任务书 §1 要求的持久失效标记。修改前同步卡文件；后台确认索引及完成记录均落盘后删除，不在编辑锁里等待内部闪存。
- 索引 schema 2：任务书 §2 要求的大小、修改时间与旧格式处理；利用旧路径字段的空余 16 B，记录仍为 216 B。原虚拟路径映射最长 155 B，新的路径字段可放 183 B 和结尾零。
- 重扫时在现有 PSRAM 遍历对象中临时保存缺失编号（最多 4 KiB），用于识别改名；随原遍历释放。未加任务、线程、队列、配置、重试器或常驻缓存。

## 要改的文档

- 合并前需要处理上述四项新回归；本报告不建议将当前分支按“全通过”合入。复核后再同步 B5-02 的实际状态与元数据识别边界。共享文档、历史审计和需求均未改。
