# M022 本轮交接

2026-10-27；已完成有限资料补查。仅文档状态改变；没有新增操作完成结论。

- 新 E：1（E-BDCD-01），新 F：0；case 保留 F-M022-01/02/03 全 D1，共 7 E。九功能域为研究问题目录，不是已取得的完整手册目录。
- 唯一 case：`docs/research/interaction-library/cases/M-022-baicizhan-device.md`。
- 新批次：`docs/research/interaction-library/batches/baicizhan-device-controls-deepening-evidence.jsonl`、`baicizhan-device-controls-deepening-coverage.csv`；coverage 仅表头，因为补证沿用 F02。
- root 旧行建议：`work/baicizhan-device-controls-deepening/existing-coverage-updates.csv`，F02 的 `evidence_ids`、`known_branches`、`gaps`、`checked_on` 四字段逐项 before/after。`depth=D1`、`hands_on=false`、`visual_status=static_official_images_only` 不变，新来源没有图片观察。
- 没有改 master、根文档、其他 case 或旧批次；四份旧 vocab 批次 SHA 检查均相同。正式 artifact 新增 0。

## root 抽核入口

1. `https://www.52audio.com/archives/239551.html`：独立原作者 Wen5、2025-05-23，非官方手册。“开箱”正文；原件 `02-52audio-ace-max.html`；解析文本 `02-52audio-ace-max.txt` 第 64–65 行作者/日期、第 72 行型号、第 78–80 与 83 行实体键文字。只证明作者如此描述；没有动作前后反馈、固件、误斩撤销/复习影响，不外推 Mini 或所有 Max 版本。该 URL 在旧 E 中查重无命中。
2. `https://danciji.baicizhan.com/`：本轮复查 HTML 200；`01-official-home-links.json` 是 22 个 href（含重复），没有手册/帮助入口。只代表所读公开首页，不证明应用内或售后没有帮助。沿用旧 E-BCZH-01，不增重复来源。

三张候选图分别为原文第 5 张物品图、第 6 张正面、第 11 张背面型号；各一次 GET 均 HTTP 403，详见 `image-fetch-results.json`，停止且没有换方式重试。未调用 view_image，未保存图片，未将文内图片图注说成观察。此前京东/抖音失败端点本轮未重试。其他检索排除理由、确切 URL 和 12 条查询见 `research-log.md`。

## 命令与结果

- `python3 work/baicizhan-device-controls-deepening/fetch.py`：exit 0；两 HTML HTTP 200。
- `python3 work/baicizhan-device-controls-deepening/fetch-images.py`：exit 0；三 HTTP 403 已记录；不是获取成功。
- `python3 work/baicizhan-device-controls-deepening/write-batch.py`：exit 0。
- `python3 work/baicizhan-device-controls-deepening/check-batch.py`：exit 0，PASS；JSONL、编号、before 值、UTF-8/LF、本地链接、原件 SHA、旧批次 SHA、无图像/无升深度均过。
- `git diff --check -- docs/research/interaction-library/cases/M-022-baicizhan-device.md docs/research/interaction-library/batches/baicizhan-device-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/baicizhan-device-controls-deepening-coverage.csv`：exit 0。目标文件为 untracked，另做上述文件级检查，不能仅用此命令声称内容已验。
- 初次错文件名 cat 及无匹配查重 rg 分别 exit 1，均已说明；无脚本失败未处理。

## 限制与后续

本轮没有找到 Mini/3.52 英寸 Max 的型号化官方完整任务材料，无法补答错重试、宠物收益、退出续接、解绑或同步冲突。有效下一途径是型号/修订页对应的盒内手册、公开原厂帮助正文或同型号连续操作演示；没有执行外联、购买、登录、发送资料、安装或硬件操作。

## 原件 SHA-256

- `work/baicizhan-device-controls-deepening/01-official-home.html`：`47e7718d62b9b0e94707740198d4335181cfc730842a884da62d26845e41e828`
- `work/baicizhan-device-controls-deepening/02-52audio-ace-max.html`：`12fd996dcc2af3cdf35f8eaf894a74163db488d1e14f6580a03c0d29b55519fb`

## 交付文件 SHA-256

- `docs/research/interaction-library/cases/M-022-baicizhan-device.md`：`62c74d6a962ce3a46e78a98e40ce8f2da319b7f1292c2b62a367a3158c22af91`
- `docs/research/interaction-library/batches/baicizhan-device-controls-deepening-evidence.jsonl`：`55d7ecffce073c7c203540bfbdf68d2e7a8bbd22399347c3295916c156868057`
- `docs/research/interaction-library/batches/baicizhan-device-controls-deepening-coverage.csv`：`aa0cf671b21fa0390e9b0cfb58c6cecf3a0d7df66eaad894707f17ea224f6025`
- `work/baicizhan-device-controls-deepening/existing-coverage-updates.csv`：`f9484331b7c7ae1fb6cd952f8f16078d29da9cc8f3cf2f08526b14851270013b`
- `work/baicizhan-device-controls-deepening/research-log.md`：`4ab8038e21b3cd9254ffaf6a8049deeba1c2cb8187634656c0bb1494ca4e347d`
