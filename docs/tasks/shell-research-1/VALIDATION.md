# 文档复核

本任务豁免主工程构建、全量和内存测试。下面检查文档本身，可以在仓库根目录直接复跑；不依赖本次未提交的下载缓存，不运行固件，也不访问设备。

```bash
taskpolicy -c utility python3 - <<'PY'
from pathlib import Path
from xml.etree import ElementTree as ET
import re

base = Path('docs/tasks/shell-research-1')
links = 0
for page in sorted(base.glob('*.md')):
    for target in re.findall(r'\]\(([^\s)]+)(?:\s+[^)]*)?\)', page.read_text()):
        if re.match(r'(?:https?://|mailto:|#)', target):
            continue
        name, _, anchor = target.partition('#')
        source = page.parent / name
        assert source.is_file(), (page.name, target)
        if re.fullmatch(r'L\d+', anchor):
            assert int(anchor[1:]) <= len(source.read_text().splitlines()), target
        links += 1

svgs = sorted(base.glob('*.svg'))
assert len(svgs) == 4
for page in svgs:
    root = ET.parse(page).getroot()
    assert (root.get('width'), root.get('height')) == ('684', '1216')
    assert root.get('viewBox') == '0 0 684 1216'
    assert not root.findall('.//{http://www.w3.org/2000/svg}image')
    assert not root.findall('.//{http://www.w3.org/2000/svg}script')
    assert not any('href' in key for node in root.iter() for key in node.attrib)
for page in base.iterdir():
    if page.is_file():
        assert page.stat().st_size < 5 * 1024 * 1024, page.name
print('Local file links checked:', links)
print('SVG canvases checked:', len(svgs))
print('Document checks: PASS')
PY
taskpolicy -c utility git diff --check cbbe3227c7579b8c9bb115ddbf9ff8d37ab41a97..HEAD
taskpolicy -c utility git diff --stat cbbe3227c7579b8c9bb115ddbf9ff8d37ab41a97..HEAD
taskpolicy -c utility git diff --name-only cbbe3227c7579b8c9bb115ddbf9ff8d37ab41a97..HEAD
taskpolicy -c utility git diff cbbe3227c7579b8c9bb115ddbf9ff8d37ab41a97..HEAD | grep -nE '/(Users|private/tmp)/|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}'
git status --porcelain
```

记录：文档检查 PASS；四张画布；diff 空白检查退出 0；变更仅 `docs/tasks/shell-research-1/`。隐私检查无输出、grep 退出 1，表示没有匹配，不能按命令失败解释。上面的路径表达式与总规则匹配目标相同，改写形式是为了不让检查命令本身命中。

本次视觉复核是两部分：公开原图/项目既有截图直接打开查看；原创 SVG 用同坐标和项目 Noto 字体生成临时 PNG 逐张查看并量文字边界。Quick Look 返回沙箱初始化失败，sips 返回 13，故没有声称浏览器或系统原生 SVG 渲染已通过。四张图无外部图片、脚本或字体下载，随报告可直接查看；不替代未来产品字号/长文本/真屏验证。

网络来源的成功、超时、403、404 与脚本壳见 SOURCES/REUSE。没有宣称所有外部 URL 都返回 200；固定源码链接和行号是复核依据，不能把改动后的上游 HEAD 当作同一版本。
