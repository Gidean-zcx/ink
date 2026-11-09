# activity-card 执行报告

状态：功能、六项反向对照与相关回归通过；最终全量和内存检查进行中。基线 `50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 同一真实音频标题、暂停／接着放、停止消失 | `ctest --test-dir build -R '^activity-card$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 模拟调制解调器实际接通；触摸和 AI 返回 PhoneCall；挂断消失 | 同上 |
| 3 | 【实测】通过 | 声音继续；触摸和 AI 不能在锁屏暂停；电源键唤醒后可暂停 | 同上 |
| 4 | 【实测】通过 | 真实 job.list、阶段更新、job.cancel 回调、快照与家人隔离 | 同上 |
| 5 | 待验证 | 全量、内存、行数、截图 | 待补 |

## 没做到的

最终全量与内存检查待补。中间全量发现的备份刷新干扰已经修复，相关四项测试通过（`build/activity-card-targeted.log`）；未放宽预算或断言。

## 和设计不一致的

【实测】五屏对照均超过 12/255：MainPlaying 51.734、ControlPlaying 50.494、MainInCall 72.390、LockPlaying 51.062、Tasks 32.283。遵守现行字号、灰度规范，未缩小尺寸或调浅颜色追分。设计示例有章节、联系人名、阅读正文等；当前服务只提供音频文件名、通话号码和真实阶段，未编造。锁屏保留的前台是主屏，因此与阅读页底图不同。截图和拼图在 `build/activity-card-compare/`。

## 改过的已有测试

`tests/integration/ai_surface.c` 的 AI 可调用清单新增 `phone.return`、`shell.tasks`、`tasks.prev`、`tasks.next` 四项（均无参数），使既有逐项覆盖断言检查新增能力；已有断言和原期望未删改。

## 要用户定的

无。

## 顺手发现

无。

## 新增的机制

F6 要求的共用活动呈现和 Tasks 页面；复用媒体、电话、作业和列表组件，无新线程、定时器、文件格式、配置项或后台重试。

增量接口：`job.revision` 通知真实阶段变化；`phone.return` 返回现有通话页；`shell.tasks`、`tasks.prev`、`tasks.next` 开放页面和翻页。锁屏 AI 暂停／续播限制在媒体服务既有 `need_session` 入口集中处理；只在活动内容改变时重画主屏和控制中心。

【实测】六个独立撤回：隐藏主屏卡片、断开通话返回、移除锁屏媒体限制、替换正在听标记、取消按钮不调 job.cancel、撤回阶段通知，均构建退出 0、测试退出 1，并命中对应断言；恢复后通过。复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/activity_card_reverse.py --build build`。证据：`build/activity-card-reverse/summary.json`。

产品代码超过 100 行的原因：共用卡片、job.list 解析及分组分页、三屏接线和能力需要一起提供。更小方案是各页直接拼字符串、静态作业或不做分页，分别违反共用数据、真实作业和无滚动要求，未采用。

## 要改的文档

合并时更新 `docs/system/ui-dependencies.md` 的 F6 和 Tasks 状态，并重新生成 `docs/system/capabilities.md`（新增能力和 job.revision）；按总规则第 7 节未修改共享文档。生成预览在 `build/activity-card-capabilities.md`。
