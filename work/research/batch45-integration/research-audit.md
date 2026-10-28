# Batch45：取证路线与失败记录

2026-10-27。正式成果只来自既存PB743K3固定手册正文复读和7页实际静态查看，没有新增独立来源URL、账户、付款、设备操作或产品实现。

## 限定搜索没有新增核心链

此前同批web.run检索不背单词的官方上手/帮助与拼写、百词斩单词机Max说明书与养狗教程、Meebook官方/厂商/FCC资料。首轮误用bbdc.cn的域名查询没有据此认定官网；随后改用产品名和帮助关键词。只在搜索结果中看到的线索没有当已读正文。

- 52audio旧Ace/Max评测239551已是E-BDCD-01；旧受限图片不重试。
- 旧短视频自动转录及养狗等级/时长/金币数字不能确证指定型号/版本，不采用。
- 什么值得买avgwkxm9、al38n2g8、a6z0wmkg等结果标自动生成；不作功能依据。
- PHP泛化上手文与非官方irez-bbdc-plus仓库不证明官方App行为；未安装或调用接口。
- Meizu应用商店摘要提自定义书等，只是线索；未打开确认版本/正文，未新增M018任务或证据。
- Meebook的Manualslib M7为旧路线；不重试已受限入口。manuals.plus/ae产品页有规格疑点，不采用；device.report/manual/8820578、manualsproguide与Goodereader目录未作为已读/已验证手册。

搜索结果不计新F/E。下一轮应取得同型号具体版本的完整教程或实际操作材料，避免反复读取同一宣传和旧日志。

## helper失败与root接手

/root/quizlet_remaining_modes执行有限本地目录核对失败：python3 /tmp/batch45_inventory.py因Photo Frame文本精确匹配断言退出1；后续JSON解析/校验因结果文件未生成失败（json.tool报告退出2；另一校验FileNotFoundError退出1）。没有产出可验收的report.md或inventory.json，没有改变正式资料。该路由停止，不复用或换付费模型重跑。

root查看脚本后发现其部分候选来自旧目录，且Meebook条目多已记录；没有运行该脚本或接受其选择。root对照当前65行目录、既存PDF/text JSON重新选择具体任务，并在正式文件中保留旧F、更新27行目录。此前失败的其他worker路由也未重启。

root最初查找pdftoppm未找到（command检查退出1），随后采用已存work/research/tools/pymupdf渲染，无安装。integrate_once.py与documents_once.py均实际运行一次并退出0。documents_once.py首次apply_patch因补丁换行格式验证失败，未修改文件；修正补丁后写入并执行。没有以脚本成功代替内容核查。

## root证据核查

原PDF SHA256对齐87bf56b461cd298b7c53421c5b21f72f1498e4c060d4eea89844545667030101。读取页集合由integration-result.json和E-PBK-01的追加采集保存，共48页；7个render页面已逐一view_image查看，正式observed-pages.json注明静态与未知分支。后续另核18/19、22/23、30/32、41/42、58、78/79及49/50、56/57、59–61正文，确认网络、下载、授权、页码、确认和调光条件；删去无依据的字距描述后才入库。

脚本检查仅验证文件、引用、编号、哈希和记录一致性；没有执行任何产品行为。最终结构结果、前后清单和需求不变哈希见本目录final-check.json、root-final-verification.json及final-manifest.json。
