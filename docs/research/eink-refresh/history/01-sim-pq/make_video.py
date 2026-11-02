"""Side-by-side comparison video: target | baselines | proposed, simulated panel output."""
import subprocess, numpy as np
from PIL import Image, ImageDraw, ImageFont
from sim import Method, run, CLEAN
from scenes import gb_to_panel, gb_target, PATTERN_PB

FONT = ImageFont.truetype("/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc", 17, index=2)  # SC
TW, TH, LBL = 480, 360, 26
W, H = 2 * TW, 2 * (TH + LBL) + 34


def tile(img):
    im = Image.fromarray(img)
    if im.size != (480, 288):
        im = im.resize((TW, TH), Image.NEAREST)
    out = Image.new("L", (TW, TH), 40)
    out.paste(im, (0, (TH - im.size[1]) // 2))
    return out


def compose(frames4, labels, title):
    canvas = Image.new("L", (W, H), 24)
    d = ImageDraw.Draw(canvas)
    d.text((10, 6), title, fill=235, font=FONT)
    for k, (f, lab) in enumerate(zip(frames4, labels)):
        x, y = (k % 2) * TW, 34 + (k // 2) * (TH + LBL)
        d.text((x + 8, y + 2), lab, fill=225, font=FONT)
        canvas.paste(tile(f), (x, y + LBL))
    return np.asarray(canvas)


def scene_frames(kind):
    if kind == "gb":
        G = np.load("G.npy")
        tgt = gb_target(G)
        Iin = np.ascontiguousarray(np.repeat(1.0 - G.astype(np.float32) / 3.0, 3, axis=2))
        pb = np.ascontiguousarray(gb_to_panel(G, PATTERN_PB, True)[:, ::3])
        cases = [(Method("PB", "pattern", "counter"), pb), (Method("INT", "pattern", "int"), pb),
                 (Method("PQ", "pqf", "pq", mu=0.05, marg=3), Iin)]
        target = (tgt * 255 + 0.5).astype(np.uint8)
        labels = ["目标画面 (Game Boy 4 灰阶)", "PaperBoy 原版：计数器 + 3 像素二值图案",
                  "积分器驱动 + 同样图案（零额外成本）", "本方案 PQ：物理量化器"]
        recs = [run(m, F, lambda t: tgt[t], CLEAN, rr=3, record=True, flush=False)["rec"] for m, F in cases]
    else:
        F = np.load(kind + ".npy")
        cases = [Method("PB-FS", "fs", "counter"), Method("PB-BN", "bn", "counter"),
                 Method("PQ", "pqf", "pq", mu=0.05, marg=3)]
        target = (F * 255 + 0.5).astype(np.uint8)
        labels = ["目标画面", "PaperBoy 式计数器 + Floyd-Steinberg 抖动",
                  "PaperBoy 式计数器 + 蓝噪声抖动", "本方案 PQ：物理量化器"]
        recs = [run(m, F, lambda t: F[t], CLEAN, record=True, flush=False)["rec"] for m in cases]
    return target, recs, labels


def main(out="eink_compare.mp4"):
    titles = {"V": "场景 1 / 3：自然视频（平移、缩放、淡入淡出、运动物体、最后 1 秒静止）",
              "X": "场景 2 / 3：阅读 / UI（静止、2 像素/场 平滑滚动、翻页、静止）",
              "gb": "场景 3 / 3：Game Boy 类游戏（背景卷动、精灵、闪烁道具）"}
    ff = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "gray",
                           "-s", f"{W}x{H}", "-r", "60", "-i", "-", "-c:v", "libx264", "-preset", "veryfast",
                           "-crf", "16", "-pix_fmt", "yuv420p", out], stdin=subprocess.PIPE)
    stills = {}
    for kind in ("V", "X", "gb"):
        target, recs, labels = scene_frames(kind)
        for t in range(len(target)):
            fr = compose([target[t]] + [r[t] for r in recs], labels, titles[kind] + "   （模拟面板输出，60 场/秒）")
            ff.stdin.write(fr.tobytes())
        stills[kind] = [target[-1]] + [r[-1] for r in recs], [target[200]] + [r[200] for r in recs], labels
    ff.stdin.close(); ff.wait()
    return stills


if __name__ == "__main__":
    stills = main()
    np.save("stills.npy", np.array([0]))
    import pickle; pickle.dump(stills, open("stills.pkl", "wb"))
