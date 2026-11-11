# 前后截图

均为真实系统页面的无窗口模拟器截图，合成 Wi-Fi 与号码，未导入设计稿。
`100` 为常规字号（设置值 1），`120` 为 120%（设置值 3）。

| 页面 | 字号 | 基线 | 修改后 |
| --- | --- | --- | --- |
| Wi-Fi 密码 | 常规 | [前](screenshots/before-Password-100.png) | [后](screenshots/after-Password-100.png) |
| Wi-Fi 密码 | 120% | [前](screenshots/before-Password-120.png) | [后](screenshots/after-Password-120.png) |
| 短信会话 | 常规 | [前](screenshots/before-SmsThread-100.png) | [后](screenshots/after-SmsThread-100.png) |
| 短信会话 | 120% | [前](screenshots/before-SmsThread-120.png) | [后](screenshots/after-SmsThread-120.png) |

基线只替换本任务的键盘／短信实现，在相同测试数据和依赖下重编译，不改检出源码。
手动复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" docs/tasks/polish-1/validate.py --build build --baseline`；去掉 `--baseline` 得修改后截图。
当前 ctest 不依赖 Git 历史。单张最大 47,142 字节，低于 300 KB。
