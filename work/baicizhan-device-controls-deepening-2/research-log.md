# M022 本批取得和退出记录

2026-10-27。旧同名批已冻结，因此本批使用`baicizhan-device-controls-deepening-2`，前缀E-BDCQ全库检索未占用。没有运行旧writer/build/check。没有助手、登录、安装、用户接口或设备动作。

## 实际读取与命令

- `cat AGENTS.md && sed -n '1,65p' README.md && sed -n '1,100p' docs/requirements.md`：exit0。
- 初始误读`cat docs/research/interaction-library/cases/M-022-baicizhan-hardware.md`：exit1，不存在；未创建此文件。随后`rg --files docs/research/interaction-library/cases work/baicizhan-device-controls-deepening docs/research/interaction-library/batches | rg 'M-022|baicizhan-device-controls-deepening|vocab-device-deepening'` exit0，找到正确M-022-baicizhan-device.md。
- `rg -n '学习|复习|恢复|单词' docs/ideas.md | head -n 15`：exit0。
- `cat docs/research/interaction-library/cases/M-022-baicizhan-device.md && cat work/baicizhan-device-controls-deepening/HANDOFF.md && cat work/baicizhan-device-controls-deepening/research-log.md`：exit0。
- 内联Python读取主coverage/evidence筛出3F/7E，并遍历batch查E-BDCQ前缀，无碰撞；exit0。基线在case-before.md、coverage-before.json、evidence-before.json、baseline.json；旧视觉证据没有引用case哈希。
- 内联Python只读前批已保存`work/baicizhan-controls-deepening/qa-index.js`，打印`sT=`、`qn=`、deviceVersion及fetchQAList定位，exit0。没有执行脚本。
- `cat work/baicizhan-device-controls-deepening/01-official-home-links.json && sed -n '1,180p' work/vocab-device-deepening/research-log.md && rg -n --max-columns 250 '帮助|教程|按键|同步|复习' work/vocab-device-deepening --glob '*.txt'`：末尾rg无匹配，整体exit1；前部JSON输出过长截断。之后只解析JSON的links数组重新完整读取22个href（含重复），不声称读完所有截断图像属性。
- `cat work/baicizhan-device-controls-deepening/01-official-home.txt && sed -n '68,88p' work/baicizhan-device-controls-deepening/02-52audio-ace-max.txt && sed -n '1,180p' docs/research/interaction-library/template.md`：exit0；未请求图片。源码及原HTML仅作为公开材料读取。

## 唯一新网络GET

已知官方前端代码选择`deviceVersion=2.0`→`dcj2`。只沿用这个现有参数，不探测未公开值。

```bash
python3 work/baicizhan-device-controls-deepening-2/capture.py qa-dcj2.json 'https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=dcj2'
```

exit0，HTTP200，11055字节；完整URL/final_url/bytes/hash在qa-dcj2.json.capture.json。JSON有5类23问，均已展开为qa-dcj2-text.txt并读完，未标日期/固件/确切SKU；没有附图。本次没有访问sxj分支，没有下载音视频或App。

web.open `https://www.baicizhan.com/qa?deviceVersion=2.0`返回工具内部“not accessible”，未提供HTTP状态；未据此声称获得正文。随后读的是已由前端定位的独立公开内容URL，不是绕过403/401/429。

web.search_query以下4项均无结果，不以索引当操作证据：

1. `site:baicizhan.com "单词机" "帮助" "Mini"`
2. `site:baicizhan.com "单词机" "Max" "同步"`
3. `site:baicizhan.com "deviceVersion" "2.0"`
4. `site:baicizhan.com "单词机" "误斩"`

没有重试已知京东商品、抖音原视频/作者、52audio图片403路径；未换代理、请求头或登录尝试。搜索无结果不证明不存在型号手册。

## 整理与检查

`python3 work/baicizhan-device-controls-deepening-2/build.py`：exit0，一次完成。产出1E/0F/45目录行、旧3F共8字段建议、旧2E本地复读追加建议；新增E仅为型号边界，不升级流程。随后内联Python保留F02原有全部缺口、用中性措辞调整case并同步case-after，exit0；没有重建或改旧batch。

最终check.py命令、exit及manifest见HANDOFF。正式文件限定M022案例、本批3批次文件与1份短摘记；原件和脚本在本work。主表、全局文档、其他案例与冻结的M016上轮文件未改。
