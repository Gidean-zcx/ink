# 实际命令与访问记录

工作目录：/home/workspace/reader。2026-10-27。没有运行其他批次build/check，没有安装、登录、账户接口、浏览器或媒体下载。本文列实际执行入口；较长生成程序保留为同目录.py，读取命令按实际参数列出。

- `cat AGENTS.md`、`cat README.md`、`cat docs/requirements.md`、`cat docs/research/interaction-library/cases/M-032-fenbi.md`；均exit0（首批输出过长，因此单独重读case）。
- `rg --files work docs/research/interaction-library | rg 'fenbi|M-032'`；exit0。
- `cat work/fenbi-controls-deepening/fetch-pages-result.json work/fenbi-controls-deepening/fetch-historical-result.json`、`cat work/fenbi-controls-deepening/fetch-pages.py`；exit0，仅读旧失败记录。
- `rg -n '粉笔|错题|R-007|R-021|G-001' docs/ideas.md docs/requirements.md`；exit0。
- `head -n 2 docs/research/interaction-library/coverage.csv`、`head -n 2 docs/research/interaction-library/batches/fenbi-controls-deepening-evidence.jsonl`；exit0。
- Python内联读主evidence/coverage，筛M-032：20E/10F。第一次错误用`source_url`键，exit1 KeyError；改读`url`后exit0。全库jsonl查`E-FBPC`无碰撞。保存baseline/case-before的Python命令exit0。
- web搜索16个查询（下列）并打开8个候选中的前4个正文；未发账号操作。搜索结果中的其他产品/考试系统、教师/申论、下载站转载仅筛除，不作采用来源。候选访问URL/返回状态/原件SHA见candidate-register.json。
- `python3 work/fenbi-practice-controls-deepening/fetch-candidates.py`：首次exit1括号SyntaxError，未执行HTTP；修正后exit0，3个HTTP200，Bilibili412后停止。
- `python3 work/fenbi-practice-controls-deepening/fetch-final-candidates.py`：exit0，4个HTTP200。至此8个候选，不扩源。
- `python3 work/fenbi-practice-controls-deepening/extract.py`：exit0，HTMLParser提取正文与真实img属性，不执行远端JS；部分此前内联HTMLParser等价预览也exit0。
- `cat work/fenbi-practice-controls-deepening/raw/c05-wrong2019.txt work/fenbi-practice-controls-deepening/raw/c06-quick2022.txt work/fenbi-practice-controls-deepening/raw/c07-print2020.txt work/fenbi-practice-controls-deepening/raw/c08-mock2020.txt`：exit0。
- `python3 work/fenbi-practice-controls-deepening/fetch-images.py`：exit0；只请求2张，第一张200，第二张403即停整个图片路线。该脚本不可重跑。
- `view_image`实际查看`/home/workspace/reader/work/fenbi-practice-controls-deepening/raw/c05-wrong2019-img1.jpg`，成功；无进程exit。其他图未看。
- `head -n 3 docs/research/interaction-library/batches/ihuman-learning-controls-deepening-inventory.csv`、`head -n 1 docs/research/interaction-library/batches/ihuman-learning-controls-deepening-evidence.jsonl`、`cat docs/research/interaction-library/template.md`：exit0，仅参考字段，未运行前批脚本。
- 本地HTMLParser读`work/fenbi-controls-deepening/kb-50542.html`并写新work提取文本：exit0；未重抓旧URL。
- `python3 work/fenbi-practice-controls-deepening/write-batch.py`：执行2次均exit0；第二次明确旧记录日期、补回返回目标未知，最终3E/0F/2旧F共11字段/1旧E采集/21目录/1静态图。
- `nl -ba work/fenbi-practice-controls-deepening/raw/c05-wrong2019.txt | sed -n '35,85p'`、`nl -ba work/fenbi-practice-controls-deepening/raw/c06-quick2022.txt | sed -n '35,70p'`、`nl -ba work/fenbi-practice-controls-deepening/raw/c03-baidu.txt | sed -n '35,62p'`：exit0。
- `cat work/fenbi-practice-controls-deepening/existing-coverage-updates.json`及`rg -n '本轮|E-FBPC|F-M032-0[49]|21项|D1' docs/research/interaction-library/cases/M-032-fenbi.md`：exit0。
- `python3 work/fenbi-practice-controls-deepening/precheck.py`：exit0，487/487 PASS。冻结清单后另运行`--manifest`，结果见precheck-result.json。
- `git diff --check -- docs/research/interaction-library/cases/M-032-fenbi.md docs/research/interaction-library/batches/fenbi-practice-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/fenbi-practice-controls-deepening-coverage.csv docs/research/interaction-library/batches/fenbi-practice-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/fenbi-practice-controls-deepening`：exit0，无输出。
- `sha256sum docs/research/interaction-library/cases/M-032-fenbi.md docs/research/interaction-library/artifacts/fenbi-practice-controls-deepening/source-notes.md work/fenbi-practice-controls-deepening/raw/c05-wrong2019.html work/fenbi-practice-controls-deepening/raw/c06-quick2022.html work/fenbi-practice-controls-deepening/raw/c03-baidu.html`：exit0，结果见HANDOFF/manifest。

## 搜索查询原文

1. 粉笔 行测 交卷 提示 漏题 练习 暂停 操作
2. site:kefu.easemob.com/v1/tenants/8342 "错题"
3. 粉笔 行测 答题卡 修改答案 练习 体验
4. site:kefu.easemob.com/v1/tenants/8342/knowledge/entries/html 行测 练习 暂停
5. site:kefu.easemob.com/v1/tenants/8342/knowledge/entries/html 答题 解析 收藏
6. site:woshipm.com 粉笔 答题 解析 暂停
7. site:fenbi.com 帮助 行测 交卷 继续
8. 粉笔 "暂停" "练习" "产品分析"
9. site:kefu.easemob.com/v1/tenants/8342 "历史"
10. site:fenbi.com "答题卡" "点击" -information-hub-detail
11. 粉笔 app "退出" "继续答题"
12. 粉笔 app "错题" "移除" "经验" "版本"
13. 粉笔 "产品体验" "交卷"
14. site:jingyan.baidu.com "粉笔" "交卷"
15. site:jingyan.baidu.com "粉笔" "错题" "移除"
16. site:jingyan.baidu.com "粉笔" "继续" "练习"
