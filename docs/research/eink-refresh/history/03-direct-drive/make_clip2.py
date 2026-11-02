"""Side-by-side simulated output on Metalio E-Ink4-Plus timing (12 ms phases, LVGL ~28 fps, 4bpp UI).
Panels: target | current EPDiy default (DU20 + gray settle) | current fast (DU8 + gray settle) |
        async integrator + blue-noise binary | PQ-BN (parallel, feasible) | PQ-ED (serial, reference).
The "current" panels play the repository's own DU20 / short-DU8 / gray8-GL tables through the
same panel model; global lock = the worker takes the latest frame only when idle and plays the whole
table. Panel model: full swing ~7.6 phases, per-pixel mobility +-4%, first-push stiction, noise.
"""
import sys, io, contextlib, subprocess, numpy as np
sys.path.insert(0, "/home/claude/eink_sim")
import sim
from sim import Panel, Method, CLEAN
from skimage import data, color, transform
from PIL import Image, ImageDraw, ImageFont

WFDIR = ("/tmp/claude-0/-home-claude/d162fb0d-4946-543b-9784-2d7efbacf79e/scratchpad/ext_metalio/"
         "Metalio-E-INK4-Plus/main/boards/metalio-e-ink4-plus/display/waveforms")
sys.argv = ["x", WFDIR]
with contextlib.redirect_stdout(io.StringIO()):
    exec(open("/home/claude/metalio_analysis/wf_dc.py").read())   # defines du20, g8gl, complete_du, short_du ...
DU20, DU8, GRAY8 = complete_du(du20), short_du(), g8gl

sim.N = 8
CFG = dict(CLEAN, T0=7.6)
H, W = 608, 342                 # half-resolution portrait (UI 684x1216)
PH_MS, LVGL_EVERY = 12.0, 3
FPS_OUT = 60
SC = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"

def font(sz): return ImageFont.truetype(SC, sz, index=2)

# ----------------------------------------------------------------------------- content
def gray_img(im, w, h):
    g = color.rgb2gray(im) if im.ndim == 3 else im / 255.0
    return transform.resize(g, (h, w), anti_aliasing=True).astype(np.float32)

def reading_page(seed=1):
    rng = np.random.default_rng(seed)
    img = Image.new("L", (W, 4200), 255); d = ImageDraw.Draw(img)
    f1, f2 = font(15), font(12)
    words = ("墨水屏 刷新 算法 像素 波形 灰阶 残影 直流 平衡 延迟 滚动 阅读 书架 章节 笔记 "
             "物理 量化 抖动 蓝噪声 积分器 驱动 面板 温度 标定 电子纸").split()
    y = 70
    for i in range(70):
        card = i % 5 == 2
        if card: d.rounded_rectangle((8, y - 6, W - 8, y + 50), 10, fill=215)
        d.text((16, y), f"第 {i+1} 章  " + "".join(rng.choice(words, 3)), fill=0, font=f1)
        d.text((16, y + 24), " ".join(rng.choice(words, 6)), fill=90, font=f2)
        d.line((10, y + 54, W - 10, y + 54), fill=170)
        y += 62
    return np.asarray(img, np.float32) / 255.0

def header(img, title):
    out = img.copy(); hb = Image.fromarray((out * 255).astype(np.uint8)); d = ImageDraw.Draw(hb)
    d.rectangle((0, 0, W, 44), fill=255); d.text((14, 10), title, fill=0, font=font(17)); d.line((0, 44, W, 44), fill=0, width=2)
    return np.asarray(hb, np.float32) / 255.0

def cover_grid():
    srcs = [data.astronaut(), data.coffee(), data.chelsea(), data.camera(), data.rocket(), data.cat(),
            data.astronaut()[::-1], data.coffee()[:, ::-1], data.camera()[::-1], data.chelsea()[:, ::-1],
            data.rocket()[:, ::-1], data.cat()[::-1]]
    img = Image.new("L", (W, 1900), 255); d = ImageDraw.Draw(img); f = font(12)
    cw, ch = 150, 200
    for i, s in enumerate(srcs * 2):
        r, c = divmod(i, 2); x, y = 14 + c * (cw + 14), 60 + r * (ch + 44)
        g = (gray_img(s, cw, ch) * 255).astype(np.uint8)
        img.paste(Image.fromarray(g), (x, y)); d.rectangle((x, y, x + cw, y + ch), outline=0)
        d.text((x, y + ch + 6), f"书名 {i+1} · 作者", fill=40, font=f)
    return np.asarray(img, np.float32) / 255.0

def chat_frame(n_chars, answer):
    img = Image.new("L", (W, H), 255); d = ImageDraw.Draw(img)
    d.rounded_rectangle((90, 60, W - 10, 120), 12, fill=205)
    d.text((104, 70), "帮我总结一下墨水屏快刷的关键？", fill=0, font=font(13))
    txt = answer[:n_chars]; x, y = 16, 140; f = font(15)
    for ch_ in txt:
        if ch_ == "\n": x, y = 16, y + 26; continue
        wch = d.textlength(ch_, font=f)
        if x + wch > W - 14: x, y = 16, y + 26
        d.text((x, y), ch_, fill=0, font=f); x += wch
    if n_chars < len(answer): d.rectangle((x + 2, y + 2, x + 4, y + 20), fill=0)   # caret
    return np.asarray(img, np.float32) / 255.0

ANSWER = ("关键有三点：\n一、像素异步驱动，不等整段波形；\n二、量化器要知道像素真实能到达的状态，"
          "用蓝噪声在两个可达状态间选择；\n三、直流必须有界，靠积分器和到端再推来保证。\n"
          "这样阅读、滚动和流式出字都能又快又清楚。")

def build_timeline():
    """Returns content frames (per LVGL frame) and caption per LVGL frame."""
    page, grid = reading_page(), cover_grid()
    wall = gray_img(data.astronaut(), W, H)
    frames, caps = [], []
    def add(img, cap): frames.append(np.clip(np.rint(img * 15) / 15, 0, 1).astype(np.float32)); caps.append(cap)
    # A: reading list 0-8 s
    off, v = 0.0, 0.0
    nA = int(8.0 * 1000 / (PH_MS * LVGL_EVERY))
    for f in range(nA):
        t = f * PH_MS * LVGL_EVERY / 1000
        if abs(t - 1.0) < 0.02: v = 38.0
        if abs(t - 3.6) < 0.02: v = -30.0
        off = max(0.0, off + v); v *= 0.92
        if abs(v) < 0.6: v = 0.0
        img = page[int(off):int(off) + H].copy()
        tap = (2.5 <= t < 2.8) or (5.4 <= t < 5.7)
        if tap: img[250:306, 8:W - 8] = 1.0 - img[250:306, 8:W - 8]
        cap = "阅读列表：甩动滚动" if v != 0 else ("点按反馈" if tap else "静止")
        add(header(img, "我的书架 · 章节"), cap)
    # B: cover grid 8-14 s
    off, v = 0.0, 0.0
    nB = int(6.0 * 1000 / (PH_MS * LVGL_EVERY))
    for f in range(nB):
        t = f * PH_MS * LVGL_EVERY / 1000
        if abs(t - 0.6) < 0.02: v = 34.0
        if abs(t - 2.6) < 0.02: v = 26.0
        off = max(0.0, off + v); v *= 0.93
        if abs(v) < 0.6: v = 0.0
        add(header(grid[int(off):int(off) + H], "书库 · 封面"), "封面网格：甩动滚动" if v != 0 else "静止")
    # C: AI streaming 14-17.5 s, page turn to wallpaper 17.5 s, static to 20 s
    nC = int(6.0 * 1000 / (PH_MS * LVGL_EVERY))
    for f in range(nC):
        t = f * PH_MS * LVGL_EVERY / 1000
        if t < 3.5:
            n = int(max(0, t - 0.3) * 42)
            add(header(chat_frame(n, ANSWER), "百问 AI"), "AI 回答流式出字")
        else:
            add(wall, "翻页到壁纸（整屏灰阶图）")
    return frames, caps

# ----------------------------------------------------------------------------- current architecture
BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])

class GlobalLock:
    """EPDiy-style worker as in their epd_refresh.cc: takes the latest frame only when idle and plays a
    whole table on the changed pixels (differential). mode 'gray': any gray pixel involved -> gray8 GL
    (30 phases), else DU20. mode 'i1': UI rendered 1-bit (LVGL I1, grays as 4x4 Bayer dots) -> DU20."""
    def __init__(self, mode):
        self.mode = mode
        self.back = np.full((H, W), 15, np.int16)
        self.busy = 0
        yy, xx = np.mgrid[0:H, 0:W]
        self.bayer = (BAYER4[yy % 4, xx % 4] + 0.5) / 16.0
    def render(self, tgt4):
        if self.mode == "i1":
            return np.where(tgt4 / 15.0 > self.bayer, 15, 0).astype(np.int16)
        return tgt4
    def step(self, tgt4, ph):
        tgt = self.render(tgt4)
        self.shown = tgt
        if self.busy == 0 and np.any(tgt != self.back):
            changed = tgt != self.back
            gray = changed & (((tgt > 0) & (tgt < 15)) | ((self.back > 0) & (self.back < 15)))
            self.plan = GRAY8 if (self.mode == "gray" and np.any(gray)) else DU20
            self.mask, self.to, self.frm = changed, tgt.copy(), self.back.copy()
            self.busy = self.plan.shape[0]; self.k = 0
        if self.busy:
            a = self.plan[self.k][self.to, self.frm]
            u = np.where(a == 2, 1, np.where(a == 1, -1, 0)).astype(np.int8)
            u[~self.mask] = 0
            self.k += 1; self.busy -= 1
            if self.busy == 0: self.back = self.to
            return u
        return np.zeros((H, W), np.int8)

# ----------------------------------------------------------------------------- main
def main():
    frames, caps = build_timeline()
    T = len(frames) * LVGL_EVERY
    names = ["目标画面（LVGL 4bpp）", "现架构·灰阶模式：全局锁，灰阶变化走 gray8 30 相", "现架构·二值 UI：I1 + Bayer 网点，全局锁 DU20",
             "方案①：异步积分器 + 二值蓝噪声", "方案②：PQ-BN 并行物理量化（可落地）", "理论参考：串行 PQ-ED（S31 跑不动）"]
    gl_def, gl_fast = GlobalLock("gray"), GlobalLock("i1")
    m_int = Method("int", "bn", "int"); m_int.reset(H, W)
    m_pqbn = Method("pqbn", "pqbnev", "pq", marg=2, T0c=7.6); m_pqbn.reset(H, W)
    m_pqed = Method("pqed", "pq", "pq", mu=0.05, marg=3, T0c=7.6, maxn=12); m_pqed.reset(H, W)
    panels = [Panel(H, W, CFG, seed=1) for _ in range(5)]
    GAP, LBL, TOP, BOT = 8, 26, 40, 8
    CW, CH = 3 * W + 2 * GAP, TOP + 2 * (LBL + H) + GAP + BOT
    ff = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "gray",
                           "-s", f"{CW}x{CH}", "-r", str(FPS_OUT), "-i", "-", "-c:v", "libx264", "-preset", "veryfast",
                           "-crf", "24", "-pix_fmt", "yuv420p", "/home/claude/metalio_analysis/metalio_algorithms_compare.mp4"],
                          stdin=subprocess.PIPE)
    fT, fL = font(18), font(14)
    next_out, out_dt = 0.0, 1000.0 / FPS_OUT
    stills = {}
    for ph in range(T):
        tgt = frames[min(ph // LVGL_EVERY, len(frames) - 1)]
        tgt4 = np.rint(tgt * 15).astype(np.int16)
        us = [gl_def.step(tgt4, ph), gl_fast.step(tgt4, ph),
              m_int.step(np.ascontiguousarray(tgt)), m_pqbn.step(np.ascontiguousarray(tgt)),
              m_pqed.step(np.ascontiguousarray(tgt))]
        for p, u in zip(panels, us): p.step(u)
        t_ms = ph * PH_MS
        while next_out <= t_ms:
            canvas = Image.new("L", (CW, CH), 26); d = ImageDraw.Draw(canvas)
            cap = caps[min(ph // LVGL_EVERY, len(caps) - 1)]
            d.text((10, 9), f"Metalio 面板时序模拟（12 ms/相）  t = {t_ms/1000:5.2f} s   {cap}", fill=235, font=fT)
            imgs = [tgt] + [p.p for p in panels]
            for k, (im, nm) in enumerate(zip(imgs, names)):
                x, y = (k % 3) * (W + GAP), TOP + (k // 3) * (LBL + H + GAP)
                d.text((x + 4, y + 4), nm, fill=225, font=fL)
                canvas.paste(Image.fromarray((np.clip(im, 0, 1) * 255 + 0.5).astype(np.uint8)), (x, y + LBL))
            fr = np.asarray(canvas)
            ff.stdin.write(fr.tobytes())
            for key, tt in (("list_scroll", 1300), ("list_static", 7900), ("grid_scroll", 9000), ("grid_static", 13900),
                            ("chat", 16000), ("wall", 18200), ("wall_end", 19950)):
                if abs(next_out - tt) < out_dt / 2: stills[key] = fr.copy()
            next_out += out_dt
    ff.stdin.close(); ff.wait()
    for k, v in stills.items():
        Image.fromarray(v).save(f"/tmp/claude-0/-home-claude/d162fb0d-4946-543b-9784-2d7efbacf79e/scratchpad/clip2_{k}.png")
    print("phases", T, "duration s", T * PH_MS / 1000)

if __name__ == "__main__":
    main()
