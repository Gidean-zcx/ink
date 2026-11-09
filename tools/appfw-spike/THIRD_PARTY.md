# 第三方记录

查阅和下载日期：2026-11-05。运行时源码均不提交，单个提交文件小于 5 MB。

MicroQuickJS 补充查阅和下载日期：2026-11-09。使用上游固定提交，不跟随 main；归档和所有解压文件逐字核对，生成器产物放构建目录，不改上游源码。

| 名称 | 地址与固定版本 | 校验 | 许可与位置 |
| --- | --- | --- | --- |
| Lua | <https://www.lua.org/ftp/lua-5.4.8.tar.gz>；5.4.8 | 归档 SHA-256 `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae` | MIT；[原始许可](licenses/Lua-5.4.8.txt)；不打开 io/os/debug/package，不引入 C 动态模块 |
| MicroQuickJS | <https://github.com/bellard/mquickjs/tree/4119f6819ccb5bae2d21a18a106e89522d18ad45>；提交 `4119f6819ccb5bae2d21a18a106e89522d18ad45`；[源码归档](https://codeload.github.com/bellard/mquickjs/tar.gz/4119f6819ccb5bae2d21a18a106e89522d18ad45) | 归档 SHA-256 `911ebe922ec84acf530a4db64ea3963fbcc4f9ae84e176ecfa819273fb5fa638` | MIT；[原始许可](licenses/MicroQuickJS.txt)；编译 `mquickjs.c`、`dtoa.c`、`libm.c`、`cutils.c`，不接 REPL、浏览器或 Node |
| LVGL | <https://github.com/lvgl/lvgl/tree/v9.5.0>；9.5.0 | 与产品同一个只读依赖检出，提交 `85aa60d18b3d5e5588d7b247abf90198f07c8a63` | MIT；[原始许可](licenses/LVGL-9.5.0.txt)；复用产品目标和内存桥，不使用商业 XML 运行时 |
| jsmn | [已有副本](../../third_party/jsmn/jsmn.h) | 沿基线 `39a349d` | 复用产品 `ink_json`，不下载、不改依赖 |
| Noto Sans CJK | <https://github.com/notofonts/noto-cjk>；沿用提供的 TTC，face 2 的 name 表为 2.004 | 字体 SHA-256 `b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a` | SIL OFL 1.1；[原始许可](licenses/NotoSansCJK-OFL.txt)，取自上游 `Sans/LICENSE` |

中文字体沿用产品生成的 Noto Sans CJK 位图资源；不提交字体文件或设计稿。Lua 归档未打补丁；所有实验桥代码都在本目录。
