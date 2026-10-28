# Batch25 已合并并核查：2026-10-27

三对象：M028 小猿手机软件、M007 Viwoods AiPaper、M056 小猿学练硬件。全部原任务仍按证据限定，不宣告完整。目标“完成目前已知产品的详细分析”保持 active。

## 结果

- 新9F（全部D2）/27E；22旧F的89字段；16旧E追加capture及3元数据字段。
- 总65组/65部分案例；812F=718D2+79D1+15D4；1343E、1180不同URL、167静态E、18浏览器E。
- 63个链接目录、6286行；本批112+578+33=723，含父子项/来源/控件，不是任务数。
- M028查看35新静态图＋3旧图；M007 helper查看PDF文件2–69共68静态页，包含目录；root抽核12关键页及V3.12/3.15正文。M056无新图/核心学习链证据。
- 第25期简报、README/library/比较/目录索引/缺口/审计/ideas均更新；requirements SHA保持e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087。

## 复核与已应用脚本

本work acquire.py、capture_images.py、build_batch.py、merge_reviewed.py、update_summaries.py均已执行成功，不能再跑一次性脚本。check_before.py和两helper check.py已在合并前PASS，不能在合并后重跑。applied-reviewed-updates.json与reviewed-before保存精确旧字段及批次字节；verification-result.json核对合并后主表/备份/需求/目录，PASS。

最终 python3 work/research/interaction-depth/check_docs.py：exit0，183 Markdown、1200本地链接、errors=[]；git diff --check exit0。全项目未跟踪文件较多，diff检查不能替代自定义文件/链接/哈希检查。本轮只有研究资料变化，无外部账户/设备操作。

M007 helper只读审阅第25期，指出68页含目录；root已将四处“功能页”改成静态页，重新文件/链接检查PASS。M056 helper只读交叉核M028政策四点，与root定位一致；两helper正式材料保持冻结。

## 下一步仍需证据

M028现版同题错误/提示/重答/退出重启及原生AI返回；M007当前版本具体权限、页组织确认、AI返回选区和恢复结果；M056同型号与固件的连续学习链。不可再用相同静态资料新增虚假完成率。下轮依全目录缺口选择其他薄弱对象；目标整体尚未完成。
