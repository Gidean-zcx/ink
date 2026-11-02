"""Realistic full-resolution UI scenarios for the kernel benchmark.
UI is portrait 684x1216 (LVGL 4bpp); the panel is landscape-native 1216x684, so every UI frame is
rotated into panel orientation exactly as the device does. A vertical UI scroll therefore shifts
content along panel rows and touches every panel row.
Output per scenario: <name>.bin = int32 nframes, then per LVGL frame: packed 4bpp panel frame
(684 rows x 608 bytes, low nibble = even x) + tile dirty map (171 x 38 bytes, 32x4 tiles).
"""
import sys, struct, numpy as np
from PIL import Image, ImageDraw, ImageFont
from skimage import data, color, transform

UW, UH = 684, 1216            # UI portrait
PW, PH = 1216, 684            # panel landscape
FONT = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
f = lambda sz: ImageFont.truetype(FONT, sz, index=2)
OUT = sys.argv[1]              # output directory
FRAMES = 70                   # 2.5 s of LVGL frames at ~28 fps

def to_panel4(ui):            # ui float [0,1] (UH x UW) -> panel 4bpp levels (PH x PW)
    t4 = np.clip(np.rint(ui * 15), 0, 15).astype(np.uint8)
    return np.ascontiguousarray(np.rot90(t4, 1))          # UI y -> panel x

def pack(p4): return (p4[:, 0::2] | (p4[:, 1::2] << 4)).astype(np.uint8)

def tiles(prev, cur):
    ch = prev != cur
    return ch.reshape(PH // 4, 4, PW // 32, 32).any(axis=(1, 3)).astype(np.uint8)

def reading_page(seed, height=6000):
    rng = np.random.default_rng(seed)
    img = Image.new("L", (UW, height), 255); d = ImageDraw.Draw(img)
    words = ("墨水屏 刷新 算法 像素 波形 灰阶 残影 直流 平衡 延迟 滚动 阅读 书架 章节 笔记 "
             "物理 量化 抖动 蓝噪声 积分器 驱动 面板 温度 标定 电子纸").split()
    y = 120
    for i in range(80):
        if i % 5 == 2: d.rounded_rectangle((16, y - 12, UW - 16, y + 100), 20, fill=215)
        d.text((32, y), f"第 {i+1} 章  " + "".join(rng.choice(words, 3)), fill=0, font=f(30))
        d.text((32, y + 48), " ".join(rng.choice(words, 6)), fill=90, font=f(24))
        d.line((20, y + 108, UW - 20, y + 108), fill=170, width=2)
        y += 124
    return np.asarray(img, np.float32) / 255.0

def text_page(seed):
    rng = np.random.default_rng(seed)
    img = Image.new("L", (UW, UH), 255); d = ImageDraw.Draw(img)
    chars = "的一是在不了有和人这中大为上个国我以要他时来用们生到作地于出就分对成会可主发年动同工也能下过子说产种面而方后多定行学法所民得经十三之进着等部度家电力里如水化高自二理起小物现实加量都两体制机当使点从业本去把性好应开它合还因由其些然前外天政四日那社义事平形相全表间样与关各重新线内数正心反你明看原又么利比或但质气第向道命此变条只没结解问意建月公无系军很情者最立代想已通并提直题党程展五果料象员革位入常文总次品式活设及管特件长求老头基资边流路级少图山统接知较将组见计别她手角期根论运农指几九区强放决西被干做必战先回则任取据处府";
    y = 90
    while y < UH - 60:
        x = 36
        line = "".join(rng.choice(list(chars), 21))
        d.text((x, y), line, fill=0, font=f(29)); y += 50
    return np.asarray(img, np.float32) / 255.0

def header(img, title):
    out = (img * 255).astype(np.uint8).copy(); im = Image.fromarray(out); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, UW, 88), fill=255); d.text((28, 22), title, fill=0, font=f(34)); d.line((0, 88, UW, 88), fill=0, width=4)
    return np.asarray(im, np.float32) / 255.0

def chat(n, answer):
    img = Image.new("L", (UW, UH), 255); d = ImageDraw.Draw(img)
    d.rounded_rectangle((180, 120, UW - 20, 240), 24, fill=205)
    d.text((208, 150), "帮我总结一下墨水屏快刷的关键？", fill=0, font=f(26))
    x, y = 32, 290
    for ch in answer[:n]:
        if ch == "\n": x, y = 32, y + 52; continue
        w = d.textlength(ch, font=f(30))
        if x + w > UW - 28: x, y = 32, y + 52
        d.text((x, y), ch, fill=0, font=f(30)); x += w
    return header(np.asarray(img, np.float32) / 255.0, "百问 AI")

ANSWER = ("关键有三点：\n一、像素异步驱动，不等整段波形；\n二、量化器要知道像素真实能到达的状态，"
          "用蓝噪声在两个可达状态间选择；\n三、直流必须有界，靠积分器和到端再推来保证。\n"
          "这样阅读、滚动和流式出字都能又快又清楚。") * 2

def covers():
    srcs = [data.astronaut(), data.coffee(), data.chelsea(), data.camera(), data.rocket(), data.cat()]
    img = Image.new("L", (UW, 4000), 255); d = ImageDraw.Draw(img)
    cw, ch = 300, 400
    for i in range(16):
        s = srcs[i % 6]; g = color.rgb2gray(s) if s.ndim == 3 else s / 255.0
        g = (transform.resize(g, (ch, cw), anti_aliasing=True) * 255).astype(np.uint8)
        r, c = divmod(i, 2); x, y = 28 + c * (cw + 28), 120 + r * (ch + 88)
        img.paste(Image.fromarray(g), (x, y)); d.rectangle((x, y, x + cw, y + ch), outline=0, width=2)
        d.text((x, y + ch + 12), f"书名 {i+1} · 作者", fill=40, font=f(24))
    return np.asarray(img, np.float32) / 255.0

def flick(n, v0, decay=0.93, start=3):
    off, v, offs = 0.0, 0.0, []
    for i in range(n):
        if i == start: v = v0
        off += v; v *= decay
        if abs(v) < 1: v = 0
        offs.append(int(off))
    return offs

def write(name, uiframes):
    prev = to_panel4(uiframes[0])
    with open(f"{OUT}/{name}.bin", "wb") as fp:
        fp.write(struct.pack("<i", len(uiframes)))
        first = True
        for ui in uiframes:
            cur = to_panel4(ui)
            td = np.ones((PH // 4, PW // 32), np.uint8) if first else tiles(prev, cur)
            fp.write(pack(cur).tobytes()); fp.write(td.tobytes())
            prev = cur; first = False
    print(name, len(uiframes), "frames")

if __name__ == "__main__":
    page = reading_page(1)
    p0 = header(page[:UH], "我的书架 · 章节")
    write("static", [p0] * FRAMES)
    write("ai_stream", [chat(int(i * 2.2), ANSWER) for i in range(FRAMES)])
    tp1, tp2 = header(text_page(5), "阅读"), header(text_page(6), "阅读")
    write("page_turn", [tp1] * 3 + [tp2] * (FRAMES - 3))
    offs = flick(FRAMES, 70)
    half = []
    for o in offs:
        fr = p0.copy(); fr[UH // 2:] = page[UH // 2 + o: UH + o]; half.append(fr)
    write("scroll_half", half)
    write("scroll_full", [header(page[o:o + UH], "我的书架 · 章节") for o in offs])
    write("scroll_full_14fps", [header(page[offs[i - i % 2]:offs[i - i % 2] + UH], "我的书架 · 章节") for i in range(FRAMES)])
    cv = covers()
    write("covers_scroll", [header(cv[o:o + UH], "书库 · 封面") for o in flick(FRAMES, 60)])
