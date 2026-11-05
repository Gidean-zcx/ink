# 界面框架与系统壳执行报告

基线：`fdff779f2f2746ab10194e32a00b7dc343365e2c`。仅在本地任务分支实施；不写设备、不调用付费接口、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| U1 | 已实现，反向验证待做 | 【实测】规格检查 25 个文件、0 违规；基线一致 | `python3 tools/ui_scale_check.py` |
| U2 | 待验证 | 已查看 Foundation、Components、Main、Today、Control、Settings 渲染图 | 图像检查 |
| U3 | 已实现，体积超过目标 | 【实测】中文三字重、数字 Outfit；初次生成约 7.0 MiB，超过 SP-05 的 4 MiB | `cmake --build build -j8` |
| U4 | 已实现，反向验证待做 | 【实测】从 Icon、IconSheet 提取 80 个图标，三种画法、五种尺寸；几何有哈希 | `node tools/icongen/extract.mjs "$INK_DESIGN" assets/icons` |
| U5 | 待验证 | — | — |
| U6 | 待验证 | — | — |
| U7 | 待验证 | — | — |
| U8 | 待验证 | — | — |
| U9 | 待验证 | — | — |
| U10 | 已有工具，正在对齐 | 【实测】首次抽查 7 屏均超过 12/255，继续修版式和缺字 | `python tools/ui_compare.py --scenes Main Today Control Settings ImePinyin AiThink ClockRing` |
| U11 | 基线通过，新实现验证中 | 【实测】基线 86/86；更换主页后旧触控坐标失效，已更新入口，阅读和 AI 重点场景恢复通过 | `ctest --test-dir build --output-on-failure -j8` |
| U12 | 待验证 | — | — |
| U13 | 待验证 | — | — |

## 没做到的

任务执行中；全量回归、内存检查、30 屏对照和反向对照尚未完成。

## 和设计不一致的

IconSheet 的旧尺寸（58、92 等）、头像 200、来电圆按钮 176 不在当前规格表中；遵守现行规格，不采用旧值。电源菜单设计有 12 px 悬空边距，与“面板贴底”冲突，按规范贴底。

## 改过的已有测试

已更新 `tests/scenarios/` 中旧主页“阅读”入口的坐标到 Dock；断言、预算上限未改。`home_basic.script` 改为实际的三屏往返和打开书架，保持两次触控、七次翻屏的要求。锁标记从自行绘制的顶层归入系统浮层，`power_menu.c` 的查找位置随之调整；文字要求不变。

## 要用户定的

D-95 的锁屏验证计时与现有服务规则仍有差异；本任务保留保守的现有验证规则，未改需求或决策。

## 顺手发现

基线的主页依赖固定演示坐标；新界面暴露了旧测试依赖控件树位置的问题，正在按实际行为替换定位方式。

服务层增量（逐项）：显示服务增加 `ink_display_commit_staged` 和 PSRAM 预览缓冲；AI 服务把面板绘制委托给系统壳，并提供点话筒入口；能力关口通过 `ink_ui_action_page` 区分非模态 AI 面板和独占浮层。

资源来源（查阅 2026-11-05）：Noto Sans CJK [Sans2.004](https://github.com/notofonts/noto-cjk/tree/Sans2.004/Sans/OTF/SimplifiedChinese)，Medium SHA-256 `ca094f6b0001fb048ca39ddd797a0cdb0179e1e55c6561e111c49c3e6a61d7b7`，Bold `b5f0d1a190a7f9b43c310a8850630af12553df32c4c050543f9059732d9b4c0a`，下载文件只留在忽略的 `build-deps/`；[Outfit](https://github.com/google/fonts/tree/main/ofl/outfit) 字体 SHA-256 `fc7287273e66929776e2ba54f144fe699080bec29f61bf649d70d871468aeade`，附 OFL；[NanoSVG](https://github.com/memononen/nanosvg/tree/239e102ec2c691f2902e20ace2ed36ee4a35cfe6) 用于构建时转换，附 zlib 许可证。
