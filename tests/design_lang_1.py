"""Capture 30 real pages, inspect targets, and assemble clean before/after pairs.

Run before changing product sources with --label before, then --label after.
All raw trees, metrics and full frames stay in the ignored build directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SCENES = dict(zip(
    'Main Today Apps Control Lock Settings SetDisplay SetSound SetNetwork ImePinyin Ai Confirm PowerMenu SetAbout '
    'Sms SmsThread Phone PhoneRecents PhoneCall Contacts MemoEdit Clock ClockTimer ClockStopwatch '
    'Calculator Calendar Weather Gallery Camera Files'.split(),
    '主屏 今日 全部 控制中心 锁屏 设置 显示和亮度 声音 网络 备忘和键盘 AI面板 确认卡 电源菜单 关于 '
    '短信列表 短信对话 拨号 最近通话 通话 联系人 备忘编辑 闹钟 计时 秒表 '
    '计算器 日历 天气 相册 相机 文件'.split()))


def compile_driver(build, area):
    flags = {}
    for line in (build / 'CMakeFiles/sim-ui-scenes.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / 'CMakeFiles/sim-ui-scenes.dir/link.txt').read_text())
    obj = area / 'driver.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-c', str(ROOT / 'tests/design_lang_1.c'), '-o', str(obj)], cwd=build, check=True)
    link = [str(obj) if word.endswith('/tests/ui_shell_scenes.c.o') else word for word in link]
    binary = area / 'capture'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    return binary


def capture(build, label):
    area = build / 'design-lang-1' / label
    area.mkdir(parents=True, exist_ok=True)
    binary = compile_driver(build, area)
    results = []
    for size in (1, 3):
        for scene in SCENES:
            out = area / f'{scene}-{size}'
            if out.exists():
                raise SystemExit(f'Refuse to overwrite existing evidence: {out.name}')
            env = {**os.environ, 'INK_UI_SCENE': scene, 'INK_UI_SCENE_TEXT': str(size),
                   'INK_DESIGN_TREE': str(out / 'tree.json')}
            run = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                '--script', str(ROOT / 'tests/ui_shell.script'), '--hub', str(ROOT / 'tests/ui_shell.hub.json'),
                '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
                '--out', str(out), '--quiet', '--no-random-outages'], env=env, capture_output=True, text=True, timeout=120)
            (out / 'process.log').write_text(run.stdout + run.stderr)
            data = json.loads((out / 'metrics.json').read_text())
            metrics = data['metrics']
            assert metrics.get('test.finished') == 1 and metrics.get('test.snapshot.settled') == 1, (scene, size, run.stderr[-1000:])
            assert (out / 'snap-scene.png').exists() and (out / 'tree.json').exists(), (scene, size)
            results.append({'scene': scene, 'size': size, 'exit': run.returncode,
                            'failures': metrics.get('test.fail', 0), 'warnings': data.get('warnings', [])})
            print(label, scene, size, 'exit', run.returncode, 'failures', metrics.get('test.fail', 0), flush=True)
    (area / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')


def targets(build, label):
    failures = []
    page_owned = []
    checked = 0
    for tree in sorted((build / 'design-lang-1' / label).glob('*/tree.json')):
        data = json.loads(tree.read_text())
        for target in data['targets']:
            checked += 1
            if target['width'] < 108 or target['height'] < 108:
                scene = tree.parent.name.split('-')[0]
                owner = ''
                if target['cap'] == 'calendar.select':
                    owner = '月历日期格（页面任务）'
                elif target['cap'] == 'keyboard.key':
                    owner = '原生键盘按键（keyboard-2）'
                elif scene in ('ImePinyin', 'MemoEdit') and (target['cap'] == 'text.voice_start' or
                        (not target['cap'] and target['text'] in ('今天', '进', '今', '近', '金', '仅', '更多'))):
                    owner = '键盘候选或语音键（keyboard-2）'
                elif scene == 'Ai' and target['text'] == '星星邮差 · 目录' and target['depth'] == 1:
                    owner = '背景阅读页的自绘目录链接（任务禁止修改 apps/）'
                item = {'scene': tree.parent.name, **target}
                if owner:
                    page_owned.append({**item, 'owner': owner})
                else:
                    failures.append(item)
    assert checked, 'capture first'
    output = {'checked': checked, 'undersized': len(failures) + len(page_owned),
              'undersized_shared': len(failures), 'failures': failures, 'page_owned': page_owned}
    (build / 'design-lang-1' / label / 'targets.json').write_text(json.dumps(output, ensure_ascii=False, indent=2) + '\n')
    print(f'{checked} targets checked; shared below 108 × 108: {len(failures)}; page-owned: {len(page_owned)}')
    return bool(failures)


def compare(build, destination):
    font = ImageFont.truetype(str(Path(os.environ['INK_DEPS']) / 'build-deps/fonts/NotoSansCJK-Regular.ttc'), 28, index=2)
    destination.mkdir(parents=True, exist_ok=True)
    manifest = []
    links = ['# 设计语言改前改后\n', '每张图左为基线、右为修改后；保留完整画布，不画调试框。默认字号 100%，最大字号 120%。原始 PNG、控件树与运行日志保留在构建目录，可按报告命令复现。\n']
    for size, description in ((1, '默认字号'), (3, '最大字号')):
        thumbnails = []
        links.append(f'## {description}\n')
        for scene, title in SCENES.items():
            frames = [build / 'design-lang-1' / label / f'{scene}-{size}/clean-scene.png' for label in ('before', 'after')]
            pair = Image.new('L', (1384, 1264), 255)
            draw = ImageDraw.Draw(pair)
            draw.text((16, 4), f'{title} · {description} · 改前', font=font, fill=0)
            draw.text((708, 4), f'{title} · {description} · 改后', font=font, fill=0)
            hashes = []
            for i, path in enumerate(frames):
                with Image.open(path) as frame:
                    assert frame.size == (684, 1216)
                    pair.paste(frame.convert('L'), (i * 700, 48))
                hashes.append(hashlib.sha256(path.read_bytes()).hexdigest())
            filename = f'{scene}-{size}.png'
            pair.save(destination / filename, optimize=True)
            assert (destination / filename).stat().st_size < 300_000, filename
            manifest.append({'scene': scene, 'size': size, 'before_sha256': hashes[0], 'after_sha256': hashes[1]})
            thumbnails.append(pair.resize((692, 632)))
            links.append(f'- [{title}]({filename})\n')
        for start in range(0, len(thumbnails), 5):
            sheet = Image.new('L', (692 * 5, 632), 255)
            for i, thumb in enumerate(thumbnails[start:start + 5]):
                sheet.paste(thumb, (i * 692, 0))
            filename = f'overview-{size}-{start // 5 + 1}.png'
            sheet.save(destination / filename, optimize=True)
            assert (destination / filename).stat().st_size < 300_000, filename
            links.append(f'\n![整页对比 {start + 1}–{start + 5}]({filename})\n')
    (destination / 'SCREENSHOTS.md').write_text('\n'.join(links))
    (destination / 'screenshots.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    print('60 full-page pairs and 12 contact sheets written')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--label', choices=('before', 'after'), default='after')
    parser.add_argument('--capture', action='store_true')
    parser.add_argument('--check-targets', action='store_true')
    parser.add_argument('--compare', type=Path)
    args = parser.parse_args()
    directory = args.build.resolve()
    if args.capture:
        capture(directory, args.label)
    if args.compare:
        compare(directory, args.compare)
    if args.check_targets:
        raise SystemExit(targets(directory, args.label))
