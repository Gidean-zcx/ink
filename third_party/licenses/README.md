# 设置里的开源许可来源

`catalog.json` 是界面清单；名称、版本、许可、原文所在文件、来源和 SHA-256 都在其中。依据根 `CMakeLists.txt` 的第三方构建目标、各 `third_party/*/README.md`、`UPSTREAM_TAG` / `UPSTREAM_COMMIT`、字体登记，以及 `tools/imedict/sources.lock.json` 整理，查阅日期为 2026-11-10。

`tools/licenses.py` 校验原文哈希后，在构建目录生成只读头文件；设备与主机构建使用同一份清单。许可原文保留原语言，界面按当前字号分页。stb 采用其双许可中的 MIT；TLSF 提取头文件内完整声明。清单标明仅用于模拟器的组件，不列仅供运行时选型试验的 Lua、QuickJS。

本目录新增的两份原文来自已固定版本的只读构建依赖，没有下载或升级：

| 文件 | 来源 | 版本 | 许可 | SHA-256 |
| --- | --- | --- | --- | --- |
| SDL2.txt | https://github.com/libsdl-org/SDL/blob/release-2.32.10/LICENSE.txt | 2.32.10 | zlib | 97f35b302b361680ec1e891e95d2d52097bb95abff361434916d99dc1305f127 |
| ImGui.txt | https://github.com/ocornut/imgui/blob/v1.91.9b/LICENSE.txt | 1.91.9b | MIT | c80c5789748d955c4a650562baa0d750494e2c7128c2ca66aaebe2e3c4e198bf |
