"""DC-balance audit of the Metalio E-Ink4-Plus waveforms.
Reads their LUT tables as data (no code from the repo is executed) and re-implements
their table builders (complete_du_build, short_du_build, my_waveform_trim, gl16_white_tick)
from the source, then reports net DC (white pushes - black pushes) per transition."""
import re, sys, numpy as np
W = sys.argv[1]

def load(fname, name, phases):
    txt = open(f"{W}/{fname}").read()
    i = txt.index(name)
    body = txt[txt.index("{", i): txt.index("};", i)]
    vals = [int(v, 0) for v in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", body)]
    assert len(vals) <= phases * 64 and len(vals) % 64 == 0, (fname, len(vals))
    if len(vals) < phases * 64:   # C zero-fills missing initializers
        print(f"note: {fname} initializes {len(vals)//64}/{phases} frames; rest zero-filled")
        vals += [0] * (phases * 64 - len(vals))
    raw = np.array(vals, np.uint8).reshape(phases, 16, 4)
    act = np.zeros((phases, 16, 16), np.int8)          # act[f][to][from]
    for frm in range(16):
        act[:, :, frm] = (raw[:, :, frm // 4] >> (6 - 2 * (frm % 4))) & 3
    return act

du20 = load("my_waveform_du.h", "my_du_25_0_data", 20)
gc48 = load("my_waveform_gc.h", "my_gc_25_0_data", 48)
gl48 = load("my_waveform_gl.h", "my_gl_25_0_data", 48)
g8gl = load("my_gray8_gl16.h", "e0470_gray8_gl16_data", 30)
g8gc = load("my_gray8_gc16.h", "e0470_gray8_gc16_data", 30)

def complete_du(src):                                # complete_du_build()
    out = np.zeros_like(src)
    for to in range(16):
        tb = 0 if to < 8 else 15
        out[:, to, :] |= src[:, tb, :]
    return out

def short_du():                                      # short_du_build()
    out = np.zeros((8, 16, 16), np.int8)
    for to in range(16):
        for frm in range(16):
            if to == frm: continue
            if to < 8: out[:7, to, frm] = 1
            else:      out[:8, to, frm] = 2
    return out

def trim(src, erase_max=11, sat_cut=5, white_sat_cut=0, hold=3):   # my_waveform_trim()
    P = src.shape[0]; seqs = {}; longest = 0
    for to in range(16):
        for frm in range(16):
            seq = list(src[:, to, frm]); sat = 2 if to == 15 else 1; er = 1 if to == 15 else 2
            a = 0
            while a < P and seq[a] == 0: a += 1
            z = P
            while z > a and seq[z - 1] == 0: z -= 1
            if a >= z: seqs[to, frm] = []; continue
            def run(s, e, v):
                n = 0
                while s + n < e and seq[s + n] == v: n += 1
                return n
            e = run(a, z, er); s = run(a + e, z, sat)
            keep_e = min(e, erase_max); want = white_sat_cut if to == 15 else sat_cut
            cut_s = max(0, min(want, s - 1)); keep_s = s - cut_s
            out = [er] * keep_e + [sat] * keep_s + seq[a + e + s:z]
            seqs[to, frm] = out; longest = max(longest, len(out))
    phases = longest + hold
    dst = np.zeros((phases, 16, 16), np.int8)
    for (to, frm), t in seqs.items():
        st = longest - len(t)
        for i, v in enumerate(t): dst[st + i, to, frm] = v
    return dst

def white_tick(d):                                   # gl16_white_tick()
    d = d.copy(); tick = -1
    for f in range(d.shape[0] - 1, -1, -1):
        if any(d[f, 15, frm] == 2 for frm in range(15)): tick = f; break
    if tick < 0: tick = d.shape[0] - 3 if d.shape[0] > 2 else 0
    d[tick, 15, 15] |= 2
    return d

def net(a):   # net[to][from] = white pushes - black pushes
    return (a == 2).sum(0).astype(int) - (a == 1).sum(0).astype(int)

def report(name, a, show=True):
    n = net(a)
    rt = np.array([[n[b, x] + n[x, b] for b in range(16)] for x in range(16)])   # x->b->x
    np.fill_diagonal(rt, 0)
    # path independence: net(a->b) should equal phi(b) - phi(a)
    phi = n[:, 0] - n[0, 0]
    pathdev = np.abs(n - (phi[:, None] - phi[None, :]))
    print(f"{name:<26} phases={a.shape[0]:2d}  0->15 net {n[15,0]:+3d}  15->0 net {n[0,15]:+3d}  "
          f"B/W round trip {rt[0,15]:+3d}  worst round trip {rt.flat[np.abs(rt).argmax()]:+3d}  "
          f"mean|round trip| {np.abs(rt).sum()/240:4.1f}  15->15 net {n[15,15]:+d}  0->0 net {n[0,0]:+d}")
    return n

print("Base tables (as shipped in headers):")
report("DU20 source", du20); report("GC16 source", gc48); report("GL16 source", gl48)
report("gray8 GL", g8gl); report("gray8 GC", g8gc)
print("\nTables actually used at runtime (rebuilt from their builder code):")
report("DU20 (complete_du_build)", complete_du(du20))
report("short DU8", short_du())
report("GL16 + white tick", white_tick(gl48))
t_gc = trim(gc48); t_gl = white_tick(trim(gl48))
report("fast GC (trim)", t_gc); report("fast GL (trim+tick)", t_gl)
d = complete_du(du20)
print("\nDU20 0->15 phases:", "".join(map(str, d[:, 15, 0])), " 15->0:", "".join(map(str, d[:, 0, 15])), " (1=black,2=white)")
print("fast GC phase count:", t_gc.shape[0], " fast GL:", t_gl.shape[0])
