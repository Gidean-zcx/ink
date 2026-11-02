# 调研原始素材在哪里

2026-11-01 按决策 D-11，`work/` 目录（约 1.2 GB、9,800 个文件的截图、页面快照、抽帧与脚本输出）已移出主分支，原样保存在归档分支 [`archive/research-raw-2026-10`](https://github.com/Gidean-zcx/ink/tree/archive/research-raw-2026-10)。

- 只要素材：`git clone --single-branch --branch archive/research-raw-2026-10 https://github.com/Gidean-zcx/ink`
- 只要文档（推荐日常使用）：`git clone --single-branch --branch main https://github.com/Gidean-zcx/ink`
- 正式结论、案例与证据索引仍在 `docs/`；交互资料库引用的截图在 `docs/research/interaction-library/artifacts/`，未移动。
- 历史提交中仍含这些文件，因此完整克隆全部历史的体积不会立刻变小；如需彻底瘦身，需要改写历史（会影响所有已有分支），作为单独决策处理。
- 独立归档仓库建立后，归档分支将原样推送过去，本页链接随之更新。
