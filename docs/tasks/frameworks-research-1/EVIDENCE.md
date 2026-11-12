# 复核记录与命令

查阅日期：2026-11-12。所有命令均由本轮执行；本页仅记录观察，不把调研当移植验收。

## 范围和版本

- 开工时 `git status --short` 无输出；分支为 `claude/codex-frameworks-research-1`，HEAD 与给定基线 `6171c2bf8d59238c95f2d548663f8b2ab3c6f43a` 一致。【实测】
- GitHub 只读查询 `repo:Gidean-zcx/ink is:open (输入法 OR 通知 OR 设置 OR frameworks)` 返回空列表。该查询不是所有未结 Issue/PR 的穷尽清单。【实测】
- 已读任务规则、任务书、项目入口/要求/流程、相关既有调研与实际代码。交互决策板 URL 无法由网页工具访问；本任务禁止修改共享决策文件，因此没有假称同步了新的选择。
- 新的只读研究检出放在 `work/frameworks-research-1/`；只展开文本源码/元数据，未下载或运行固件镜像。唯一运行的上游程序是本页的 **macOS 宿主机 C++ 探针**，不是设备固件。没有触碰串口、设备、付费接口、真实密钥、push 或 PR。
- 原有三份只读副本开工与复核时 HEAD 分别为：Mogo `864f2f9dc75980a67d2d6b67b17365020b7a50ce`；kiikoread `83942eb45d13064a47aca7ff04c2faf8fd51f98a`；口袋仓库 `90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8`。没有移动这些副本的 HEAD。【实测】

## 源码核查怎么复现

固定仓库和版本、文件行号、完整文件 SHA-256 在 [sources.json](sources.json)；人读入口是 [SOURCES](SOURCES.md)。相同仓库的多个版本分别记载。典型命令（把仓库、提交、文件替换为清单对应值）：

```sh
git clone --depth 1 --filter=blob:none --no-checkout <仓库HTTPS地址> work/frameworks-research-1/<名称>
git -C work/frameworks-research-1/<名称> fetch --depth 1 origin <固定提交>
git -C work/frameworks-research-1/<名称> show <固定提交>:<文件>
nl -ba <文本文件> | sed -n '<起行>,<止行>p'
git -C work/frameworks-research-1/<名称> ls-tree -r --name-only <固定提交>
```

不要对整个过滤克隆执行需要取所有 blob 的大小统计；单个所需字典可定点读取/统计，不取固件镜像。官方 AOSP Git 服务命令行连接超时后改查 Lineage 保留的 `cm-10.1` 分支，报告明确注明该身份；没有伪造官方最新分支版本。

本轮对照包括这些查询，负向结论只针对列出的源码范围：

```sh
rg -n -i 't9|9key|nine' work/frameworks-research-1/fcitx-android/app/src/main/java
rg -n -i 'search' work/frameworks-research-1/brookesia/app/brookesia_app_settings
rg -n 'guess_sentence|predicted|pinyin_train' work/frameworks-research-1/libpinyin/src/pinyin.h
rg -n 'Memorize|MakeSentence|enable_user_dict' work/frameworks-research-1/rime/src/rime/gear/script_translator.cc
rg -n 'sizeof\(size_t\)|fread' work/frameworks-research-1/aosp-pinyin-lineage/jni/share/spellingtrie.cpp
rg -n 'learn|MAX_CANDS|USER_FREQ' system/services/text/text_service.c
```

网络检索用过：`ESP32 googlepinyin`、`ESP32 pinyinime`、`ESP32 librime`、`ESP32 libpinyin` 及相应 GitHub 限域词；结果没有提供可核实的目标硬件成套运行证据。没有以空搜索结果断言世界上没有移植。原始 AOSP 仓库可由[官方网页](https://android.googlesource.com/platform/packages/inputmethods/PinyinIME/)查看；工程命令访问失败和网页可读可以同时成立。

## 口袋源码包与字典数值

源文件：`CloudZao-Projects@90150a24…/firmware-pocket/firmware-pocket-plus-1.0.7-plus.16-source.zip`，本次读取已有只读副本，文本解压到本任务 `work/`。ZIP 为 **2,018,663 B**，SHA-256 为 `a197b62e49a55e12d5ec1e225d77670a51fb317ab8f9df0e460459deadb23bba`。没有展开/执行二进制程序。【实测】

包内 `sources/pinyin-generation.json` 固定数据来源为 `rime/rime-pinyin-simp@0c6861ef7420ee780270ca6d993d18d4101049d0`，数据文件 SHA-256 为 `e341598343a0f0f2035bb1aafc34a7f3bb7887deeecb3f60796262aaa2983e6b`；`main/ime/notepad_ime.cc:12–41` 证明运行时只作最多六字母的单音节查表。【实现】

本次直接按 UTF-8 文件字节统计/读入得到：

| 文件/集合 | 结果【实测】 |
| --- | --- |
| Google `dict_pinyin.dat` | 1,068,442 B；SHA-256 `6bf0bbde4e3134cce38d08524a9f4dc1af40435243c8e36b38eb68d1e14462b2` |
| Google 原始 `rawdict_utf16_65105_freq.txt` | 3,570,346 B；不从文件名推导词条数 |
| 明月 `luna_pinyin.dict.yaml` | 889,896 B；SHA-256 `75bcf6eb3ff62b129882ed89cc22b2d80a5347aa72bcfa2ccc839bac298e7314` |
| Mogo / kiikoread `ui_ime_phrases.h` | 各 13,842 B、按行首词条计 487 项；两份完整文件 SHA-256 相同 |

计数方法是对这两份短语头执行 `re.findall(r'^\s*\{"', text, re.M)`，并计算文件字节数和 SHA-256；这是静态条目数，不是字表全部汉字数。

## Google 宿主机探针：失败也保留

【实测】Apple clang 的 C++11、`-O2` 编译退出 **0**，原文件有类型/格式等警告，没有使用项目 `-Werror` 标准。探针不改上游源代码；只提供空的 `cutils/log.h`，原来仅用于已关闭的性能日志。Mac 64 位加载同仓库字典时退出 **-6（SIGABRT）**，原话为：

```text
libc++abi: terminating due to uncaught exception of type std::bad_alloc: std::bad_alloc
```

没有得到候选输出或运行内存结果。首次观察和一次记录退出状态的复现结果一致；没有继续修上游、换词库或伪报测试通过。复现这个加载失败的最小探针可用：

```sh
mkdir -p work/frameworks-research-1/google-repro/include/cutils
printf '%s\n' '/* Disabled upstream performance log only. */' > work/frameworks-research-1/google-repro/include/cutils/log.h
git -C work/frameworks-research-1/aosp-pinyin-lineage show a08d08cdea4319fb74b55289e1b4e1b1a5249bc2:res/raw/dict_pinyin.dat > work/frameworks-research-1/google-repro/dict_pinyin.dat
cat > work/frameworks-research-1/google-repro/open.cpp <<'CPP'
#include "pinyinime.h"
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    if (!ime_pinyin::im_open_decoder(argv[1], argv[2])) return 3;
    ime_pinyin::im_close_decoder();
    return 0;
}
CPP
taskpolicy -c utility clang++ -std=c++11 -O2 \
  -Iwork/frameworks-research-1/google-repro/include \
  -Iwork/frameworks-research-1/aosp-pinyin-lineage/jni/include \
  work/frameworks-research-1/google-repro/open.cpp \
  work/frameworks-research-1/aosp-pinyin-lineage/jni/share/*.cpp \
  -o work/frameworks-research-1/google-repro/open
taskpolicy -c utility work/frameworks-research-1/google-repro/open \
  work/frameworks-research-1/google-repro/dict_pinyin.dat \
  work/frameworks-research-1/google-repro/user.dict
```

Linux 不用 `taskpolicy`；此失败结论仅针对本次 64 位 Mac，不推断 Linux/S31 的运行结果。

【实现】`SpellingTrie::load_spl_trie` 用 `sizeof(size_t)` 读两个字段，再按乘积 `new char[...]`；`DictList` 也有此模式，见 I01。【实测】字典前两项按小端 32 位解释为 **8、413**，按两个小端 64 位解释为 **1,773,821,493,256、71,900,996,941,791**。【估算】这足以指向原始 32 位词库与 64 位宿主的 ABI/文件格式不兼容；本次未用调试器捕获异常栈，不把整个移植失败笼统归为“内存不够”。S31 是另一种目标，必须单独验证。

## 交付检查

主工程全量测试原话：**“未运行（任务书明确豁免）”**。ASan/内存检查、S31 构建、`ink-codex/tools/gcccheck.py`：均**未运行（纯调研，无产品代码变化）**；宿主机加载探针失败不是这些检查的替代。产品代码 **0 行**，已有测试 **0 行**，新增产品机制 **0 项**。

最终文件/行号/哈希、图片限额、链接、修改范围与隐私扫描结果在 [REPORT](REPORT.md) 中记录。所有二进制编译产物、源码检出、临时日志和原图只留在忽略的 `work/`，没有提交。
