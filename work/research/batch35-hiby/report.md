# 批35：HiBy R3II（2025）F04/F05 有界证据排查

日期：2026-10-27。结论：**没有找到此前遗漏、足以闭合F04队列/歌单编辑删除或F05设备导入/导出全链的操作正文；两者仍应保持D1。** 可考虑补两项文件准备条件，不建议新增F、重复E或提升深度。正式docs未修改。

## 结果与既有内容的区别

| 对象 | 本轮核到的原文 | 相对现有案例的增量/处理建议 |
| --- | --- | --- |
| F04 当前列表、加歌单、删除 | 中文指南714–715行及英文662–663行只列功能名称；自动滑到播放列表（中文661–664，英文609–612）已有正文，但已在批30入库。 | **无新操作增量**。菜单开启手势、选择目标/新建命名、重排、确认/取消、删文件还是引用、当前曲处理、撤销均未取得。不能把蓝牙/Wi-Fi长按“删除”移到歌曲列表。 |
| F04 内部/最近列表 | 中文FAQ128–140行说最近50曲、播放列表数及列表曲数/收藏不限；英文126–138行译文更简略。 | 已有容量边界，不是创建/删除步骤。没有以“无限”推成内存或扫描容量保证。 |
| F05 文件编码 | 中文FAQ405–411行、英文397–403行列m3u/m3u8、ASCII、Unicode16大小端、UTF-8有/无BOM；推荐UTF-8有BOM。 | 现案例已写推荐UTF-8 BOM。**可选精化**：推荐不等于强制，其余编码也在平台文档声明范围。不是任意文件可解析保证，不新增操作。 |
| F05 盘符/位置 | 中文FAQ375–399/413–431行、英文367–391/405–423行分别说明a为首卡、b为第二卡、c为OTG；绝对路径用设备盘符；相对路径以M3U所在目录为起点。 | 相对基准和b盘符型号边界已入库。可补c为平台OTG示例，但**不证明本机的实际挂载、识别或任何第二卡槽**。不要为了列映射新增F。 |
| F05 导入导出与解析位置 | 中文387行给`playlist_data`存放约定，416行绝对路径示例说M3U可放任意位置；431–445行相对路径示例仅列裸文件名。英文379/408/423–437行对应。 | 两段分别讲导入/导出文件约定和路径解析，不宜拼成“任意位置M3U都会自动入库”。**可选精化**：仅移动绝对路径M3U不改变其指向；裸文件名示例需在歌曲同目录，不能推广为所有相对路径都必须同目录。均属文档含义/边界，设备发现和导入方式仍未知。 |

F05已有CUE/ISO分轨不支持、相对路径推荐、引用非副本的结论未重复算增量。本轮不建议改动旧F深度。若root选择补编码或位置区分，应沿用E-HBD-03/E-HBCD-02追加复读，而非新建来源。

## 来源、定位、适用边界

1. E-HBD-01：[英文型号指南](https://guide.hiby.com/en/docs/products/audio_player/hiby_r3ii_2025/guide/)，本地`work/hiby-controls-deepening/raw/manual-en.html`；提取`extracted/manual-en.txt`609–663行。
2. E-HBCD-01：[中文型号指南](https://guide.hiby.com/docs/products/audio_player/hiby_r3ii_2025/guide)，本地`work/hiby-controls-deepening/raw/manual-zh.html`；提取`extracted/manual-zh.txt`661–715行。
3. E-HBD-03：[英文X1600 FAQ](https://guide.hiby.com/en/docs/question/audio_player/x1600_r/)，本地`work/hiby-controls-deepening/raw/faq-en.html`；提取`extracted/faq-en.txt`126–138、367–437行。
4. E-HBCD-02：[中文X1600 FAQ](https://guide.hiby.com/docs/question/audio_player/x1600_r)，本地`work/hiby-controls-deepening/raw/faq-zh.html`；提取`extracted/faq-zh.txt`128–140、375–445行。
5. E-HBD-02：[已存型号固件记录](https://guide.hiby.com/en/docs/products/audio_player/hiby_r3ii_2025/firmware/)，本地`work/hiby-controls-deepening/extracted/firmware-en.txt`全文复读，没有队列/M3U操作更新；“修复若干问题”不作新增功能证据。

上述均为旧采集本地复读，不是2026-10-27重新GET。1.1_202501061153仅既有发布参照；未证明动态指南绑定该版，更未核实实际设备。中文/英文同源不是独立交叉验证。平台FAQ点名R3II2025，但跨机型盘符例子必须保留限制。正文HTML含原有NUL，使用rg -a；提取行号不是PDF页码。

原HTML锚链接检索：型号指南无playlist/m3u/queue/delete相关外链；FAQ相关结果全部为本页#锚点，包括Import/export及m3u parsing，没有发现可直接追查的新帮助页。没有联网搜索或请求；旧store/apps/help-center 403路径未重试。没有图像观察或设备操作。

## 实际读取的文件和命令

工作目录`/home/workspace/reader`。以下命令均exit 0；没有运行旧构建/合并/依赖before的检查。

```sh
cat AGENTS.md
sed -n '1,28p' README.md
sed -n '1,54p' docs/requirements.md
cat docs/research/interaction-library/cases/M-035-hiby.md
rg --files work | rg '(hiby|Hiby|HiBy)'
rg -a -n -i '(playlist|m3u|delete|remove|播放列表|歌单|删除|导入|导出)' work/hiby-controls-deepening/extracted/manual-en.txt work/hiby-controls-deepening/extracted/manual-zh.txt work/hiby-controls-deepening/extracted/faq-en.txt work/hiby-controls-deepening/extracted/faq-zh.txt work/hiby-controls-deepening/extracted/firmware-en.txt
nl -ba work/hiby-controls-deepening/extracted/faq-en.txt | sed -n '330,460p'
nl -ba work/hiby-controls-deepening/extracted/faq-zh.txt | sed -n '330,470p'
cat work/hiby-library-controls-deepening/read-ranges.json
rg -n '(403|404|失败|停止)' work/hiby-controls-deepening/HANDOFF.md work/hiby-library-controls-deepening/HANDOFF.md
nl -ba work/hiby-controls-deepening/extracted/manual-en.txt | sed -n '604,668p'
nl -ba work/hiby-controls-deepening/extracted/manual-zh.txt | sed -n '660,716p'
cat work/hiby-controls-deepening/extracted/firmware-en.txt
```

另有3次内联Python（均exit0）：
- `csv.DictReader`读取两版145项目录，输出020/070/107–112/126/127/144/145行；`json.loads`读取主evidence中M035的9记录URL/定位。只读，未修改主表。
- `html.parser.HTMLParser`逐个读取4份指南/FAQ原HTML的a标签href/text，按playlist/m3u/queue/delete/歌单/播放列表/删除筛选；结果仅FAQ页内锚点，无网络访问。
- `hashlib.sha256`对case、两目录、5份原HTML及对应5份提取文本计算散列，创建`work/research/batch35-hiby/inspected-files.json`，并用DictReader确认两目录各145行。

## 状态、散列与未决点

`inspected-files.json`记录13个核心已读文件的当前SHA256。案例SHA仍为`65514e618c2944b513e8f48eba3bfc791ddd553042bf2700cab674ec4877fda8`，与批30冻结值一致。主coverage/evidence、旧批、案例、目录、全局文档均未写入。本轮只创建本work目录下报告与检查清单：**state_changed=true（work报告）；docs_changed=false；external_state_changed=false；hands_on=false；new_visual_observation=false**。

仍缺真正的新证据：固定2025版本下打开当前列表→多选/添加/删除→确认/退出→文件与引用状态；M3U导入/导出按钮及自动发现规则、异常提示、成功反馈和重新载入后的结果。现有资料没有这些链，不能用一般文件系统常识补足。是否把两项条件说明并入正式库由root决定。
