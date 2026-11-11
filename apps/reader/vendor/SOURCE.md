# 阅读器复用来源

核查日期：2026-11-11。Metalio 固定提交 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`；CrossPoint 固定提交 `0333cbbe4649f80afafe83061bf1afe9fd372fcf`。

Metalio：CloudZao/Metalio-E-INK4-Plus（MIT，`metalio_core/LICENSE`）。CrossPoint：crosspoint-reader（MIT，`crosspoint/LICENSE`）；expat 2.7.3+ 的 MIT 原文见 `third_party/reader_expat/LICENSE` 与各文件头；miniz 3.1.2 的宽松授权在文件尾。无 GPL 代码。

## 原文文件

以下文件均逐字节复制，改动行数为 0；路径相对于仓库根目录。SHA-256 是入库文件的字节哈希。

| 本地文件 | 上游路径 | 提交 | SHA-256 |
| --- | --- | --- | --- |
| `apps/reader/vendor/metalio_core/LICENSE` | `LICENSE` | `874538eb54f7` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `apps/reader/vendor/metalio_core/book_read_error.h` | `main/reader/book_read_error.h` | `874538eb54f7` | `e7bb5a48baa178c350bd9f0df2e48466fc383c5f390f93015061d43ce7f25f76` |
| `apps/reader/vendor/metalio_core/epub_document.cc` | `main/reader/epub_document.cc` | `874538eb54f7` | `1acb5a463f002cead84b7921af5a63e9623e5136ebc3320c0bceae8240353ef9` |
| `apps/reader/vendor/metalio_core/epub_document.h` | `main/reader/epub_document.h` | `874538eb54f7` | `70239581328d6cb9f08b52d43f3d0683bc1a32e892aa1e970790342c908c5d4e` |
| `apps/reader/vendor/metalio_core/gb18030_tables.inc` | `main/reader/gb18030_tables.inc` | `874538eb54f7` | `e60927d5fda8cf1345551009392a9467a39d3bcec30e265aabc51a07c88d2d11` |
| `apps/reader/vendor/metalio_core/html_content.cc` | `main/reader/html_content.cc` | `874538eb54f7` | `b732e53363426f89dab6705e6eee5ead7c13a57f470f1f02a460f45fc2bf300a` |
| `apps/reader/vendor/metalio_core/html_content.h` | `main/reader/html_content.h` | `874538eb54f7` | `0f25131d532911f1213f0e89884e89f15e8a32463f30aedae8092bcc8bccf559` |
| `apps/reader/vendor/metalio_core/reader_types.h` | `main/reader/reader_types.h` | `874538eb54f7` | `1da1e2a1ab47228797853ee72eac1137ff98e081edcfc30526466932a5623a17` |
| `apps/reader/vendor/metalio_core/text_encoding.cc` | `main/reader/text_encoding.cc` | `874538eb54f7` | `fb2335a005597061536176bcf526a07f4ee9b601603d094553be00066f7fddd1` |
| `apps/reader/vendor/metalio_core/text_encoding.h` | `main/reader/text_encoding.h` | `874538eb54f7` | `b9bf23849d89a8501956fa59ba73b35c8120423bc62695e44295ea289425f57c` |
| `apps/reader/vendor/metalio_core/txt_chapter.cc` | `main/reader/txt_chapter.cc` | `874538eb54f7` | `23d698072f75d38a43bc6148d1a2b8cec64a06ccbb8e75a6c0c78805dd721a23` |
| `apps/reader/vendor/metalio_core/txt_chapter.h` | `main/reader/txt_chapter.h` | `874538eb54f7` | `fa38244c6a835c783528bed53052c0ac7bb3a49a34ecfe2890b47c9aee36920a` |
| `apps/reader/vendor/metalio_core/zip_reader.cc` | `main/reader/zip_reader.cc` | `874538eb54f7` | `55b272a84df2b4c4e9e3dd30c7f1c12f445c550196b464cf3d56ea4aca5ec22b` |
| `apps/reader/vendor/metalio_core/zip_reader.h` | `main/reader/zip_reader.h` | `874538eb54f7` | `b1dd99ee5d34b4f8884213be020753c674dd6d72099595379aaff589e77161d2` |
| `apps/reader/vendor/crosspoint/ContainerParser.cpp` | `lib/Epub/Epub/parsers/ContainerParser.cpp` | `0333cbbe4649` | `761665c7d72964fda9c8d02f4db173253464cce4c5d79da1804ad2633d7dbab1` |
| `apps/reader/vendor/crosspoint/ContainerParser.h` | `lib/Epub/Epub/parsers/ContainerParser.h` | `0333cbbe4649` | `577c83cdbb7a438c0ee68dcc75798ff2db4501dbb3c37289f279d9a3cce91b57` |
| `apps/reader/vendor/crosspoint/ContentOpfParser.cpp` | `lib/Epub/Epub/parsers/ContentOpfParser.cpp` | `0333cbbe4649` | `0737140d9805ba92bd88f26d7acb06a74146e6b7b01d15a3ae3fac698a02b46a` |
| `apps/reader/vendor/crosspoint/ContentOpfParser.h` | `lib/Epub/Epub/parsers/ContentOpfParser.h` | `0333cbbe4649` | `6f61f2b371d05bcb7a14e17d625a114d2b1306c93865d8fe40df26708aa57cc4` |
| `apps/reader/vendor/crosspoint/LICENSE` | `LICENSE` | `0333cbbe4649` | `b489ce1358e321a2ba270c4764cd2afbb5347eae44fbe26ea994e4684ab65bf0` |
| `apps/reader/vendor/crosspoint/TocNavParser.cpp` | `lib/Epub/Epub/parsers/TocNavParser.cpp` | `0333cbbe4649` | `8bba2d984aaccd62d0de45cb4d12f75850c350565031ae68bfcad3c7211fad66` |
| `apps/reader/vendor/crosspoint/TocNavParser.h` | `lib/Epub/Epub/parsers/TocNavParser.h` | `0333cbbe4649` | `bcf7ae3d0f9321c3f2422908b46b42729ef19e778bbdbd2a56a045a44d21a1d0` |
| `apps/reader/vendor/crosspoint/TocNcxParser.cpp` | `lib/Epub/Epub/parsers/TocNcxParser.cpp` | `0333cbbe4649` | `7184f3f90c6e6b700d0e1f13edec6a83a1d06f7f97add16116d1a633663699b3` |
| `apps/reader/vendor/crosspoint/TocNcxParser.h` | `lib/Epub/Epub/parsers/TocNcxParser.h` | `0333cbbe4649` | `abd394bedeb846d790f381cfa4232c2c9ff67fd76005104ee8a14ae0ec54190d` |
| `apps/reader/vendor/crosspoint/XmlParserUtils.h` | `lib/XmlParserUtils/XmlParserUtils.h` | `0333cbbe4649` | `32d630a37c61ddacc0822bd934c12642e509fc713d390542fc6d74a253946a67` |
| `third_party/reader_miniz/miniz.c` | `lib/miniz/third_party/miniz.c` | `0333cbbe4649` | `e2c1aeb66eef9191d8c3feb164db2def2335a61d039bf04ed849f6b042433b30` |
| `third_party/reader_miniz/miniz.h` | `lib/miniz/third_party/miniz.h` | `0333cbbe4649` | `b53b62ed122e559b8f679e3cb787a0b0035fe87a58f909da0e44931678f4e85f` |
| `third_party/reader_expat/.gitignore` | `lib/expat/.gitignore` | `0333cbbe4649` | `b907f24d7726d2b5228ff00807cd8545a32e17f3378b3e98feacc227acf667c4` |
| `third_party/reader_expat/Makefile.am` | `lib/expat/Makefile.am` | `0333cbbe4649` | `31695f595a9b4ee971f63f1cc5991a6addd6fcec792006793b647d0cc6204139` |
| `third_party/reader_expat/ascii.h` | `lib/expat/ascii.h` | `0333cbbe4649` | `42f8b392c70366743eacbc60ce021389ccaa333598dd49eef6ee5c93698ca205` |
| `third_party/reader_expat/asciitab.h` | `lib/expat/asciitab.h` | `0333cbbe4649` | `1cc0ae749019fc0e488cd1cf245f6beaa6d4f7c55a1fc797e5aa40a408bc266b` |
| `third_party/reader_expat/expat.h` | `lib/expat/expat.h` | `0333cbbe4649` | `b5223912a8045cfac5a4fefaa32502dafdfb255161a680722d3a62a9b15a4f72` |
| `third_party/reader_expat/expat_config.h` | `lib/expat/expat_config.h` | `0333cbbe4649` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `third_party/reader_expat/expat_external.h` | `lib/expat/expat_external.h` | `0333cbbe4649` | `7ed82825d053315c83ed58beb1bd8dcdc44116eeb938f170a67e3eac70fb64bf` |
| `third_party/reader_expat/iasciitab.h` | `lib/expat/iasciitab.h` | `0333cbbe4649` | `ad8b01e9f323cc4208bcd22241df383d7e8641fe3c8b3415aa513de82531f89f` |
| `third_party/reader_expat/internal.h` | `lib/expat/internal.h` | `0333cbbe4649` | `690fe0022c4f893680fa1815b33f8e8c5a65b3d1b93b8e38036bc9d496172807` |
| `third_party/reader_expat/latin1tab.h` | `lib/expat/latin1tab.h` | `0333cbbe4649` | `eab66226da100372e01e42e1cbcd8ac2bbbb5c1b5f95d735289cc85c7a8fc2ba` |
| `third_party/reader_expat/libexpat.def.cmake` | `lib/expat/libexpat.def.cmake` | `0333cbbe4649` | `22fdd83adabcbaeedc865c47c2a9021156d34001b2ab24fa8dd01d98801f0153` |
| `third_party/reader_expat/library.json` | `lib/expat/library.json` | `0333cbbe4649` | `0b65c1c0ff7b3d56422eb3254a71dd7fcb513f4e1bca482526782241683a3501` |
| `third_party/reader_expat/nametab.h` | `lib/expat/nametab.h` | `0333cbbe4649` | `d95bdc1c798a6829ac71f4cdd3546c4d76a0d341b30561cc93b815843deeec07` |
| `third_party/reader_expat/siphash.h` | `lib/expat/siphash.h` | `0333cbbe4649` | `bebb56e1dc4cc644e3c52a73739b53a31c7cfe539aef9a31fef9c42402dc4a47` |
| `third_party/reader_expat/utf8tab.h` | `lib/expat/utf8tab.h` | `0333cbbe4649` | `8cd26bd461d334d5e1caedb3af4518d401749f2fc66d56208542b29085159c18` |
| `third_party/reader_expat/winconfig.h` | `lib/expat/winconfig.h` | `0333cbbe4649` | `f7b8ac2a4566c712ca49dfc444ef1f90f9092c04081e77896868dc8647fe4cd0` |
| `third_party/reader_expat/xmlparse.c` | `lib/expat/xmlparse.c` | `0333cbbe4649` | `84ea12628363b14af5c798016b4f3b287ce964c79071a343583d4812ef45966b` |
| `third_party/reader_expat/xmlrole.c` | `lib/expat/xmlrole.c` | `0333cbbe4649` | `2f46a4b921a6069623b172535ac6f2d5c1bff977f95e71bfcbd3318c6be2d787` |
| `third_party/reader_expat/xmlrole.h` | `lib/expat/xmlrole.h` | `0333cbbe4649` | `fe301afd6ea715712d8a96621c047e00f98dc07848a339502810146abee5e8de` |
| `third_party/reader_expat/xmltok.c` | `lib/expat/xmltok.c` | `0333cbbe4649` | `7cff21c69341dd0f737a5c6ad927160aff771f786a57722d62294bccceb73c0e` |
| `third_party/reader_expat/xmltok.h` | `lib/expat/xmltok.h` | `0333cbbe4649` | `33e3aaadfedece1fcc34ebe025745c81f9496cef8aa18e8719354bf84c03d3cc` |
| `third_party/reader_expat/xmltok_impl.c` | `lib/expat/xmltok_impl.c` | `0333cbbe4649` | `addc821084209230353fdb8465f7b0719f7e348d5c51fded3fccc948945b6ead` |
| `third_party/reader_expat/xmltok_impl.h` | `lib/expat/xmltok_impl.h` | `0333cbbe4649` | `f05ad4fe5e98429a7349ff04f57192cac58c324601f2a2e5e697ab0bc05d36d5` |
| `third_party/reader_expat/xmltok_ns.c` | `lib/expat/xmltok_ns.c` | `0333cbbe4649` | `46c0d6d09520fea911b75658c51e5a4a2e4b2153765b1f5e809f16668822692f` |

## 按整函数抽取的原文

`apps/reader/vendor/crosspoint/FsPaths.cpp` 从 CrossPoint `lib/FsHelpers/FsHelpers.cpp` 原文取私有 `isHexDigit`、`hexValue`，以及完整 `decodeUriEscapes`、`normalisePath` 函数；只补标准头文件和 namespace 外壳，函数内部未改。SHA-256：`6cdab9931e21299cdc5031e898f01a70385585e85c3f4b7e6fd7be24b29a9020`。

`third_party/reader_expat/LICENSE` 从原文 `xmlparse.c` 的 MIT 声明提取，未改许可措辞。SHA-256：`2a852628cecfc10e373c4bffe3df5f3320b7b09c0aa4af97823efa3cbe0cb6da`。

## 适配边界

- `reader_runtime.h` 在编译时把原厂 stdio 名称接到 `reader_port.cc` 的只读 HAL 文件句柄；不修改原厂 ZIP 代码。兼容端口单独构建，不向应用暴露 HAL 或模拟器头。
- 宿主机独用 `shim/host` 中 IDF 兼容头，GB18030 双字节映射走 iconv；固件使用真实 IDF / FatFS。宿主机 C++ 解析通过 `vtk_host_call` 执行，不读宿主时间；主正文与 miniz/expat 工作内存走应用 PSRAM 分配。
- miniz 保留 zlib API，禁用归档、写入、时间、压缩 API；为规避 ESP ROM 固定符号，将 tinfl 与 CRC/Adler 导出名加 ink 前缀。同一配置用于固件和宿主机。
- CrossPoint 的 Print、日志、BookMetadataCache 用小兼容头接 TOC 结果；ContentOpfParser 只以 cache=nullptr 调用，因此其磁盘缓存分支不执行。ContainerParser、ContentOpfParser、NCX/NAV 解析器都保持原文。
- expat 按 CrossPoint 配置禁用一般实体，输入上下文为 0；锚点绑定使用 expat 回调和原厂 HtmlBlockParser。
- 不编译原厂 image_stream 解码链；占位接口明确返回不支持，正文以“［图片］”表示，未宣称图片解码完成。
- 原厂 vendor 目标独用 -w；内容源、阅读器和端口胶水沿用项目完整警告选项。

## 证据边界

未接触设备。固件构建、自动测试和上机清单见本任务 REPORT.md；不能把宿主机解析成功当成实机已验证。
