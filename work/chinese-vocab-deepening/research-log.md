# 本轮公开取证与失败记录

日期：2026-10-26。只做文档研究，无登录、验证码发送、购买、应用安装、APK 下载、外部账户修改。正式材料范围为 M-016/M-018 两案例、新 chinese-vocab-deepening 批次与五张必要静态图。旧 vocab 批次只读；主索引由 root 处理。

## 本地查阅与命令

- `cat docs/research/interaction-library/cases/M-016-baicizhan.md docs/research/interaction-library/cases/M-018-bbdc.md`：退出 0。
- `rg 'M-016|M-018' docs/research/interaction-library/catalog.csv`：退出 0。
- 阅读 `AGENTS.md`、`README.md`、`docs/requirements.md`、interaction-library 的 `template.md`、`batches/README.md`、旧 `vocab-evidence.jsonl` 与 `vocab-coverage.csv`。以 Python hashlib 将两案例与两旧批次初始散列保存在 `baseline.json`；两案例原文复制为 `.before`。
- `head -n 1 .../extended-readers-evidence.jsonl && head -n 3 .../vocab-coverage.csv && rg 'E-BCZH.*892|E-BCZH.*单词本' .../vocab-evidence.jsonl && rg 'M-016|M-018' .../catalog.csv`：退出 1，原因是中间 rg 无匹配；随后单独 catalog 查询退出 0。不是数据解析失败。
- 搜寻约定时对不存在的 `methodology.md`/`evidence-schema*` 执行 rg：退出 2；随后读取实际 `batches/README.md`，退出 0。未创建这些猜测文件。
- `python3 work/chinese-vocab-deepening/write-batch.py`：退出 0，新增 14 E、2 F、5 静态图，另生成 5 条旧覆盖行的 root 更新建议。没有写入旧批次或 master。
- 有界检查命令为 `python3 work/chinese-vocab-deepening/check-batch.py`；结果见 `validation.json`。
- 上述检查已退出 0：14 条证据、2 新流程、5 条待 root 更新旧行，字段/引用/文件与图像散列通过；两旧 vocab 批次散列未变。
- `git diff --check -- docs/research/interaction-library/cases/M-016-baicizhan.md docs/research/interaction-library/cases/M-018-bbdc.md docs/research/interaction-library/batches/chinese-vocab-deepening-evidence.jsonl docs/research/interaction-library/batches/chinese-vocab-deepening-coverage.csv docs/research/interaction-library/artifacts/chinese-vocab-deepening`：退出 0，无输出。`git status --short --` 同一范围退出 0，显示这些资料尚未受 Git 跟踪，所以空 diff 不能单独作内容检查；上面的 Python 检查直接读取文件并检查链接、字段和行尾空白。

## 官方网页和图像

使用 web.open 获取正文：百词斩文章 915（2023-02-21）、894（2022-11-21）、892（2022-11-08）、925（2023-03-23）；不背中国区 App Store、魅族官方商店、自制词书重定向页。精确 URL 与章节登记在正式新证据批次。网页未输出系统构建号时保持未知。

使用 Python `urllib.request.urlopen(..., timeout=30)` 获取官网公开 HTML，保存于本工作目录：百词斩首页/历史 information，不背 index，文章 925、魅族商店。均 HTTP 200。没有运行页面 JavaScript。文章 925 的 img 标签用于定位两张必要图，魅族 HTML 只作开发者/版本正文核对，没有查看其商店截图。

五张图通过公开 HTTP GET 取得并实际使用 `view_image` 查看；URL、文件、SHA-256 和可见区域在正式静态观察记录。最初 web 图像链接有 400/超时/内部解析错误；HTTP 获取官方公开原图成功。不是通过登录或绕开访问限制取得。前三图分别为 566064、370300、860819 字节；后两图为 79323、80879 字节，均 HTTP 200。

## 公告与社交入口限制

- `https://learnywhere.cn/bb/production/notification`：GET HTTP 200，976 字节；仅脚本壳，没有公告正文。官网确实链接此入口。
- 壳中公开脚本 `https://4app.beingfine.cn/static/js/app.8a73937b.js`：一次 GET HTTP 403 后停止；没有保存/执行脚本，也没有推断内部 API 或 App 操作。
- 官网所链 `https://www.zhihu.com/org/bu-bei-dan-ci-app/activities`：一次 GET HTTP 403；停止此路线。没有官方文章正文。
- `https://www.bbdc.cn/lexis_book_index`：公开打开重定向到 `/lexis/login?redirectUrl=http://www.bbdc.cn/lexis_book_index`；未提交表单。登录页静态提示不等于实际触发错误或恢复账号。
- 魅族搜索索引曾显示 5.11.4；打开正文则为 5.11.7、2026-10-15。采用正文，并保留差异；没有对 App 本身做版本检查。

## 搜索缺口

以下官方域名/账号定位查询没有提供足以写完整操作链的官方材料：

- `site:zhuanlan.zhihu.com "不背单词App" "词书"`
- `site:mp.weixin.qq.com "不背单词" "学习计划"`
- `site:bbdc.cn "help"`
- `site:learnywhere.cn "复习" -privacy -agreement`
- `site:weibo.com "不背单词App" "记错了"`（工具返回 robots.txt 阻止）
- `site:bbdc.cn "复习" "不认识"`
- `site:learnywhere.cn "不背" "离线"`
- `site:baicizhan.com "全拼默写" "提示"`
- `site:baicizhan.com "撤销" "斩"`
- `site:bbdc.cn "帮助" "拼写"`

其中首次百词斩查询误写为 `site.baicizhan.com`，后按正确 `site:baicizhan.com` 重查。第三方教程、商店用户评论及法律协议中同名“撤销”词不作为官方交互事实。没有声称已穷尽全部公众号或应用内帮助。

## 协作与未改项

曾尝试派出一个 worker 做有界独立核查，返回 agent thread limit reached；没有成功启动助手，未换付费路线重试。其后本地完成查阅、编写和自检。root 负责抽核事实、修改旧 coverage 的 5 行及合并主索引。两案例的主学习任务均没有升级为已验证完整闭环，也没有 D3/D4。
