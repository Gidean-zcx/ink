# 卡上的拼音词库

查阅与核对日期：2026-11-10。生成器只在电脑端运行；设备沿用 `IKD1` 格式与现有拼音引擎，没有 Python 依赖。

## 来源和许可

| 输入 | 固定版本 | 用途与许可 |
| --- | --- | --- |
| [jieba dict.txt](https://github.com/fxsjy/jieba/blob/v0.42.1/jieba/dict.txt) | 0.42.1 | 单字、词语、原始词频；[MIT](licenses/jieba-MIT.txt) |
| [pypinyin](https://github.com/mozillazg/python-pinyin/tree/v0.55.0) | 0.55.0 wheel | 按词组确定读音；[MIT](licenses/pypinyin-MIT.txt) |
| [pinyin-data](https://github.com/mozillazg/pinyin-data/tree/fa9761fff402f8560196b1ba085c437c52b56d7c) | pypinyin 对应子模块提交 | 单字读音；[MIT](licenses/pinyin-data-MIT.txt) |
| [phrase-pinyin-data](https://github.com/mozillazg/phrase-pinyin-data/tree/cee0ed6e6e4898580cafd2bd5e3723e20b214aa0) | pypinyin 对应子模块提交 | 多音字词组读音；[MIT](licenses/phrase-pinyin-data-MIT.txt) |
| [common.tsv](common.tsv) | 本仓库提交 | 本项目编写的日常短语及设备词；1000 是保留优先级下限，属于人工估计，不能当作语料统计 |

下载地址与 SHA-256 在 [sources.lock.json](sources.lock.json)。下载文件和已缓存的文件都校验哈希；失败即停止。pypinyin 的代码与 JSON 数据从校验过的 wheel 提取，不使用电脑上其他版本的包。原始数据只在忽略目录 `build-deps/imedict` 中，仓库只收工具、人工短语与许可证。四份许可证随输出复制到同一目录，分发时一起携带。

## 生成与裁剪

默认 CMake 构建会生成 `build/test-content/.system/ime/base.ikd`。也可以单独生成：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/imedict/build_base.py \
  --cache build-deps/imedict --out build/test-content/.system/ime/base.ikd
```

Linux 去掉 `taskpolicy -c utility`。首次需要下载，以后可离线复用哈希匹配的缓存。删除输出可强制重建。

规则：保留 1～12 字、全部在 U+4E00～U+9FFF 内的词；生成读音及裁剪之前，复用字体生成器的 `ascii,punct,pinyin,gb2312` 字符集过滤含缺字的词，保证随包字体可显示；按原始词频降序、同频按文字排序。单字保留拼音数据中的多种读音，词组使用 pypinyin 的词组读音（人工短语已明确逐字音节）。每个词组同时加入全拼和每个音节的首字母，`zh/ch/sh` 的首字母分别为 `z/c/s`，ü 写 `v`，不带声调或隔音符。键最长 40 字母，匹配当前服务组合串上限。

每加入一词，精确计算它的所有键、词条和文字对文件大小的增量；下一词放不下就截止，整个文件含头不超过 `DICT_MAX = 1 MiB`。存储频率为 `max(1, isqrt(原始频率 × 4096))`，上限 65535，避免把常用词全部饱和到同一个值；单字的非首读音再降为四分之一（最小为 1），防止“见”的次读 `xian` 排在“先”前面。这是保守排序估计，不是读音语料频率。同分沿用确定的输入顺序。输出旁的 `base.json` 记录数量、大小和哈希。

## 放到卡上

和字体一样先在电脑端生成资源，再打包或复制到资源所在介质；区别是字库在 assets，拼音词库在内容卡。卡根目录放成：

```text
content/.system/ime/base.ikd
content/.system/ime/jieba-MIT.txt
content/.system/ime/pypinyin-MIT.txt
content/.system/ime/pinyin-data-MIT.txt
content/.system/ime/phrase-pinyin-data-MIT.txt
```

即将 `build/test-content/.system/ime/` 整个目录复制到卡的 `content/.system/ime/`。设备看到的逻辑路径为 `/content/.system/ime/base.ikd`，不能再多套一层 `test-content`。模拟器使用 `--content build/test-content`。本任务没有接触设备或写卡。

第一次需要词库时仍由现有后台执行器读入 PSRAM，内存紧张时可回收；坏文件或缺卡仍走 19 字兜底表。`build/mini-content` 单独保留原有引擎顺序测试的人工小词表。

## 质量复现与边界

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" \
  --test-dir build --output-on-failure -j2 -R 'ime-lexicon|integration-text'
```

[固定测试集](../../tests/ime_lexicon/cases.tsv)包含单字、词语、短句、多音字、`v` 拼写、简拼和一个未收录短句。每行目标既作为首选期望，也检查能否出现在前五；只把消耗全部输入字母的候选计为命中。手写测试集用于回归，部分日常词和人工词表重合，不能推论真实用户所有输入的命中率。报告保留每行实际前五候选与命中结果，同时运行无词库对照，并要求删掉词库后的正式验收失败。相同输入重新生成后逐字节对比。加载计时单独调用现有引擎的加载入口，从请求到已加载探针（含后台排队、读卡和校验，以 20 ms 轮询），排除键盘绘制；查询和选词仍走公开文本服务。

没有统计模型组句、模糊音、词法切分或实时新词更新；未收录短句需要按词或单字选候选。1 MiB 裁剪会丢失低频词、生僻字、专名和新词，部分繁体字也可能保留；词组表没有覆盖的多音字仍可能错读。同音词及简拼冲突只能按词频排，人工短语优先级不能解决所有语境。后续用户选词仍由原来的成员词频学习调整。加载时间和内存结果是模拟器证据，不能当作实机性能。

分层和 AI：本次只替换构建资源。按钮、AI 仍通过同一个文本服务与能力表，未新增页面、能力或绕过成员隔离的入口；拼音引擎和读卡机制的边界不变。
