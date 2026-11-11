# music-2：音乐播放器收尾——媒体执行器栈、孩子首次播放超时、共享文档和翻译

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`。

本任务从分支 `music-1-work` 开始（= `music-1` 的成果 + 最新主分支，构建文件冲突已由主会话解开）。`music-1` 已移植 dr_mp3/dr_flac/dr_wav/stb_vorbis 解码、kiikoread 改版的标签和歌词、播放页和 `music.*` 能力（见 `docs/tasks/music-1/REPORT.md`），但留下了这些问题，本任务修完：

## 要做的

1. **栈**（最要紧）：宿主机估算媒体执行器要用 10496 字节，配置只有 8192 字节，真机上会栈溢出。找出大块栈占用（解码器状态结构、解码缓冲等），改成在打开时用现有内存接口分配到 PSRAM、关闭时释放；**不要靠加大执行器的栈**。给出改后每种格式的栈峰值（宿主机估算）和 PSRAM 用量，确认低于配置且有余量。
2. **孩子首次播放回执超时**：`music-product` 里孩子冷启动第一次播放在 24.9 秒报 `timed out, it may still finish`。查清是解码首块太慢、执行器排队、还是成员切换后的权限/路径问题，修根因；不放宽超时、不改测试预算。
3. **共享文件同步**：重新生成 `docs/system/capabilities.md`；`tests/integration/ai_surface.c` 的能力表登记新增的 `music.*`；`tests/integration/apps.c` 等旧的应用数量断言按新增“音乐”应用更新；新文案加 `TR` 标记并在 `assets/i18n/en.tsv` 补英文。
4. **快进 10 秒重绑**：局部更新进度时按钮绑定的参数没有同步，修正（报告里写的那处）。
5. 全量里其余失败逐项处理；和本任务无关的偶发失败（例如 4G 宿主机测试在机器很忙时超时）单独复跑确认，写进报告。

## 范围和预算

- 只改：`system/services/audio/**`（含 `media_service.c` 的格式分发接入点、`codec_formats.c`、vendor 目录）、`system/apps/music/**`、`third_party/audio_formats/**`、`tests/music/**`、`tests/integration/ai_surface.c` 和 `tests/integration/apps.c` 里对应的几行、`assets/i18n/en.tsv`、生成的 `docs/system/capabilities.md`、`docs/tasks/music-2/`。不碰别的任务在改的文件。
- 胶水新增不超过 200 行。不新增任务或线程，不加大执行器栈。

## 验收

1. 主工程普通全量 ctest 过；`music-product` 稳定通过（连跑 3 次）；栈估算低于配置并写出余量。
2. S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小和应用分区余量。
3. 报告写清栈的改法、超时的根因、上机核对清单（主会话在真机放 MP3 和 FLAC）。
