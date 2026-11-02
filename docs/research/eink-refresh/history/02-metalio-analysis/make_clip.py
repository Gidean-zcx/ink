import pickle, subprocess, numpy as np
from PIL import Image, ImageDraw, ImageFont
logs = pickle.load(open("logs.pkl", "rb")); frames = pickle.load(open("frames.pkl", "rb"))
F = ImageFont.truetype("/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc", 16, index=2)
names = list(logs.keys())
labels = ["目标画面（LVGL 28 fps）", "现架构 DU20（默认档）", "现架构 DU8（快速档）", "异步积分器 N=8"]
H, W = 608, 342; TOP = 58; GAP = 6
CW, CH = 4 * W + 3 * GAP, H + TOP
ff = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "gray", "-s", f"{CW}x{CH}",
                       "-r", "250/3", "-i", "-", "-c:v", "libx264", "-preset", "veryfast", "-crf", "24",
                       "-pix_fmt", "yuv420p", "metalio_scroll_compare.mp4"], stdin=subprocess.PIPE)
T = len(logs[names[0]])
for ph in range(T):
    c = Image.new("L", (CW, CH), 24); d = ImageDraw.Draw(c)
    d.text((8, 4), f"Metalio 面板时序模拟：12 ms/相，列表甩动滚动 + 点按反馈（半分辨率裁切）   t = {ph*12/1000:5.2f} s", fill=230, font=F)
    tgt = (frames[min(ph // 3, len(frames) - 1)] * 255).astype(np.uint8)
    imgs = [tgt] + [logs[n][ph] for n in names]
    for k, (im, lab) in enumerate(zip(imgs, labels)):
        x = k * (W + GAP)
        d.text((x + 6, 32), lab, fill=220, font=F)
        c.paste(Image.fromarray(im), (x, TOP))
    ff.stdin.write(c.tobytes())
ff.stdin.close(); ff.wait()
