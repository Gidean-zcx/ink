# 固定来源

查阅日期：2026-11-11。原始固定版本整文件复制；music-2 的最小栈工作区补丁见下。

## dr_libs @ f12e197fd462e4c22873fbd70d11b1d2f2c1dc3c

- `dr_mp3.h` → `dr_mp3.h`：SHA-256 `7a6cc38b0afdb55cfc9eec991d9169d67beb8fd910be2995121510cdaa99c356`
- `dr_flac.h` → `dr_flac.h`：SHA-256 `383a3549cf589c571bae61cb69932156439108104aee1a7d47f93869b1e7d209`
- `dr_wav.h` → `dr_wav.h`：SHA-256 `39bc4414fe662f49f726beade73fbd012295824b5f6f1042526cde6fe644cbb1`
- `LICENSE` → `dr_libs-LICENSE`：SHA-256 `dd1c647e6f767f8ff4b2dfae0fed314726600a01e0cf1ef556afddd5fa96ff15`

许可证选择：MIT-0。

## stb @ 4e9b368bfc071e97206ea5c6adba68dd0cbf8897

- `stb_vorbis.c` → `stb_vorbis.c`：SHA-256 `b11d4a3c16996c2e6d63bd96c8da5e929f2c2ab8fc99788414a09e41ba50561f`
- `LICENSE` → `stb-LICENSE`：SHA-256 `bebfe904b14301657e4e5d655c811d51fd31b97c455b9cc2d8600d6bac6cff63`

许可证选择：MIT。


- `dr_flac.h` 原始 SHA-256：`3f010d3142a5b9df80a0bd8b325de54317468b1aff85abb257c86dbfefe9bd2a`。
- `dr_wav.h` 原始 SHA-256：`7c9660d75361ad6175e6217c63a1075c9512445c06bc951197fc479179f76b39`。
- `stb_vorbis.c` 原始 SHA-256：`4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636`。

## music-2 工作区补丁（2026-11-12）

- 上面的映射哈希是当前项目文件；原始哈希另列，来源提交和许可证不变。
- `dr_flac.h`：原打开函数移到带 `drflac_init_info *` 的内部函数；原入口通过同一分配回调分配初始化状态，返回前释放，所有错误出口同样释放。不改解码、协议或 seek 算法。
- `dr_wav.h`：4 KiB 读取工作区成为 `drwav` 的成员，由外部内存中的 `ink_format` 持有，关闭时随实例释放。打开时的未知长度扫描、读取时的跳过缓冲、seek 的丢弃缓冲和四种 s16 转换路径复用它，原容量不变。
- `stb_vorbis.c`：只将文件打开函数的临时 `stb_vorbis` 状态通过现有 `malloc/free` 接缝放到外部内存，成功复制到解码器或失败清理后释放。不改解码算法；原 512 KiB arena 仍由实例关闭时释放。
