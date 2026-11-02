"""E-ink fast-drive simulator: compares PaperBoy-style counter driving with the
proposed integrator + hysteretic multi-level error diffusion.

Panel model (per pixel, per field):
  p   particle position / optical state in [0,1] (0 black, 1 white)
  u   drive in {-1,0,+1} for this field
  p  += u * k / T0 * stiction * (1 + beta*u*r/N) * (1 + noise), clipped to [0,1]
  r   leaky integral of u (residual DC field); only matters when beta/gamma > 0
  k   fixed per-pixel mobility (unknown to the controller)
  stiction: first field after resting moves less, more so after long dwell
"""
import ctypes, os, numpy as np
from scipy.ndimage import gaussian_filter, affine_transform

HERE = os.path.dirname(os.path.abspath(__file__))
lib = ctypes.CDLL(os.path.join(HERE, "libkernels.so"))
P = np.ctypeslib.ndpointer
f32p = P(np.float32, flags="C_CONTIGUOUS"); u8p = P(np.uint8, flags="C_CONTIGUOUS"); i8p = P(np.int8, flags="C_CONTIGUOUS")
ci, cf = ctypes.c_int, ctypes.c_float
lib.fs_binary.argtypes = [f32p, u8p, ci, ci, f32p]
lib.hed.argtypes = [f32p, u8p, u8p, ci, ci, ci, cf, f32p]
lib.drive_counter.argtypes = [u8p, u8p, u8p, i8p, ci, ci]
lib.drive_integrator.argtypes = [u8p, u8p, i8p, ci]

class PQBNParams(ctypes.Structure):
    _fields_ = [("D", ci), ("D1", ci * 8), ("invD", ci), ("marg", ci)]
libbn = ctypes.CDLL(os.path.join(HERE, "libpqbn.so"))
libbn.pqbn_init.argtypes = [ctypes.POINTER(PQBNParams), cf, cf, cf, ci]
libbn.pqbn_q8.argtypes = [u8p, u8p, u8p, u8p, i8p, i8p, ci, ctypes.POINTER(PQBNParams), ctypes.c_uint32]

liblut = ctypes.CDLL(os.path.join(HERE, "libpqbnlut.so"))
liblut.pqbn_lut_build.argtypes = [u8p, cf, cf, cf]
liblut.pqbn_lut_phase.argtypes = [u8p, u8p, u8p, u8p, i8p, u8p, i8p, ci, ctypes.c_uint32, ci]
liblut.pqbn_lut_phase.restype = ci
u16p = P(np.uint16, flags="C_CONTIGUOUS")
liblut.pqbn_lutg_build.argtypes = [u16p, ci, ci, cf, cf, cf]
i32p = P(np.int32, flags="C_CONTIGUOUS")
liblut.pqbn_lut2_build.argtypes = [u8p, i32p, cf, cf, cf]
liblut.pqbn_lut2_phase.argtypes = [u8p, i32p, u8p, u8p, u8p, u8p, i8p, u8p, i8p, ci, ctypes.c_uint32, ci]
liblut.pqbn_lut2_phase_ev.argtypes = [u8p, i32p, u8p, u8p, u8p, u8p, u8p, i8p, u8p, u8p, i8p, ci, ctypes.c_uint32, ci, ctypes.POINTER(ci)]
liblut.pqbn_lutg_phase.argtypes = [u16p, ci, ci, u8p, u8p, u16p, u8p, i8p, u8p, i8p, ci, ctypes.c_uint32, ci]

class PQBNv2Ctx(ctypes.Structure):
    _fields_ = [("W", ci), ("H", ci), ("ntx", ci), ("nty", ci),
                ("lut", ctypes.c_void_p), ("step", ctypes.c_uint8 * 4), ("marg", ci),
                ("fb4", ctypes.c_void_p), ("st", ctypes.c_void_p), ("tau", ctypes.c_void_p),
                ("tile_dirty", ctypes.c_void_p), ("tile_busy", ctypes.c_void_p), ("tile_last", ctypes.c_void_p),
                ("drive", ctypes.c_void_p), ("row_active", ctypes.c_void_p), ("u_dbg", ctypes.c_void_p)]
libv2 = ctypes.CDLL(os.path.join(HERE, "libpqbnv2.so"))
libv2.pqbn_v2_build.argtypes = [u8p, u8p, cf, cf, cf]
libv2.pqbn_v2_phase.argtypes = [ctypes.POINTER(PQBNv2Ctx), ctypes.c_uint32]
libv2.pqbn_v2_phase.restype = ci
libv3 = ctypes.CDLL(os.path.join(HERE, "libpqbnv3.so"))
libv3.pqbn_v3_build.argtypes = [u8p, u8p, cf, cf, cf]
libv3.pqbn_v3_phase.argtypes = [ctypes.POINTER(PQBNv2Ctx), ctypes.c_uint32]
libv3.pqbn_v3_phase.restype = ci

class PQBNv4Ctx(ctypes.Structure):
    _fields_ = [("W", ci), ("H", ci), ("ntx", ci), ("nty", ci),
                ("dec", ctypes.c_void_p), ("exk", ctypes.c_void_p), ("step", ctypes.c_uint8 * 4), ("marg", ci),
                ("fb4", ctypes.c_void_p), ("hot", ctypes.c_void_p), ("cold", ctypes.c_void_p), ("tau", ctypes.c_void_p),
                ("tile_dirty", ctypes.c_void_p), ("tile_plan", ctypes.c_void_p), ("tile_nmax", ctypes.c_void_p),
                ("drive", ctypes.c_void_p), ("row_active", ctypes.c_void_p),
                ("plan_budget", ci), ("plan_cursor", ci), ("stat_planned", ctypes.c_uint32),
                ("exk_m", ctypes.c_void_p), ("tau_nib", ctypes.c_void_p), ("band", ctypes.c_void_p), ("line_mode", ci),
                ("auto_motion", ci), ("backlog", ci), ("n_motion", ci)]
libv4 = ctypes.CDLL(os.path.join(HERE, "libpqbnv4.so"))
libv4.pqbn_v4_build.argtypes = [u8p, u8p, u8p, cf, cf, cf]
libv4.pqbn_v4_build.restype = ci
libv4.pqbn_v4_phase.argtypes = [ctypes.POINTER(PQBNv4Ctx), ctypes.c_uint32]
libv4.pqbn_v4_phase.restype = ci
libv4.pqbn_v4_init.argtypes = [ctypes.POINTER(PQBNv4Ctx), ctypes.c_uint32]
libv4.pqbn_v4_build_motion.argtypes = [u8p, u8p, u8p]
libv4.pqbn_v4_motion_enter.argtypes = [ctypes.POINTER(PQBNv4Ctx), ctypes.c_uint32, u8p]
libv4.pqbn_v4_motion_exit.argtypes = [ctypes.POINTER(PQBNv4Ctx), ctypes.c_uint32, ctypes.c_void_p]


def decode_drive(drive, rows, h, w):
    d = drive.reshape(h, w // 4)
    c = np.stack([(d >> 6) & 3, (d >> 4) & 3, (d >> 2) & 3, d & 3], axis=-1).reshape(h, w)
    u = (c == 2).astype(np.int8) - (c == 1).astype(np.int8)
    u[rows == 0] = 0
    return u

class PQParams(ctypes.Structure):
    _fields_ = [("D", ci), ("D1", ci * 8), ("MU", ci), ("maxn", ci), ("marg", ci), ("rails_only", ci)]
libq = ctypes.CDLL(os.path.join(HERE, "libpqfixed.so"))
i16p = P(np.int16, flags="C_CONTIGUOUS")
libq.pq_init_params.argtypes = [ctypes.POINTER(PQParams), cf, cf, cf, cf, ci, ci]
libq.pq_field_q8.argtypes = [u8p, u8p, u8p, i8p, ci, ci, ctypes.POINTER(PQParams), ctypes.c_uint32, i16p]
class PQFast(ctypes.Structure):
    _fields_ = [("thr", ci * 8), ("invD", ci), ("half_mu_over_D", ci)]
libq.pq_fast_init.argtypes = [ctypes.POINTER(PQParams), ctypes.POINTER(PQFast)]
libq.pq_field_q8_wf.argtypes = [u8p, u8p, u8p, i8p, ci, ci, ctypes.POINTER(PQParams), ctypes.POINTER(PQFast), ctypes.c_uint32, i16p]
libq.pq_field_q8_fast.argtypes = [u8p, u8p, u8p, i8p, ci, ci, ctypes.POINTER(PQParams), ctypes.POINTER(PQFast), ctypes.c_uint32, i16p]
lib.pqbn_field.argtypes = [f32p, f32p, f32p, i8p, i8p, i8p, f32p, ci, ci, cf, cf, cf, cf, ci, ci, cf]
lib.pq_field.argtypes = [f32p, f32p, f32p, i8p, i8p, i8p, ci, ci, cf, cf, cf, cf, ci, ci, cf, ci, ci, f32p]

N = 4  # drive fields for a full swing (PaperBoy uses 4)

CLEAN = dict(T0=3.8, k_sigma=0.04, s0=0.3, s0_sigma=0.09, tau_d=120.0, noise=0.03, beta=0.0, gamma=0.0, tau_r=600.0)
DCSENS = dict(CLEAN, beta=0.01, gamma=3e-4)


class Panel:
    def __init__(self, h, w, cfg, seed=1, p0=1.0):
        rng = np.random.default_rng(seed)
        self.cfg, self.rng = cfg, np.random.default_rng(seed + 100)
        self.k = np.clip(rng.normal(1.0, cfg["k_sigma"], (h, w)), 0.88, 1.12).astype(np.float32)
        self.s0 = np.clip(rng.normal(cfg["s0"], cfg.get("s0_sigma", 0.0), (h, w)), 0.0, 0.6).astype(np.float32)
        self.p = np.full((h, w), p0, np.float32)
        self.r = np.zeros((h, w), np.float32)
        self.dwell = np.full((h, w), 1000.0, np.float32)
        self.dc = np.zeros((h, w), np.int32)  # exact running sum of u (model-independent)

    def step(self, u):
        c = self.cfg
        uf = u.astype(np.float32)
        drv = u != 0
        st = np.where(self.dwell > 0, 1.0 - self.s0 * (1.0 - np.exp(-self.dwell / c["tau_d"])), 1.0)
        fac = np.clip(1.0 + c["beta"] * uf * self.r / N, 0.3, 1.6)
        nz = 1.0 + c["noise"] * self.rng.standard_normal(u.shape).astype(np.float32)
        dp = uf * self.k / c["T0"] * st * fac * nz
        if c["gamma"]:
            dp = np.where(drv, dp, c["gamma"] * self.r / N)
        self.p = np.clip(self.p + dp, 0.0, 1.0).astype(np.float32)
        self.r = (self.r * (1.0 - 1.0 / c["tau_r"]) + uf).astype(np.float32)
        self.dwell = np.where(drv, 0.0, self.dwell + 1.0).astype(np.float32)
        self.dc += u


# ---------------------------------------------------------------------------
# Blue-noise mask (void-and-cluster), used by the ordered-dither baseline
# ---------------------------------------------------------------------------
def blue_noise(n=64, sigma=1.5, seed=0):
    path = os.path.join(HERE, f"bn{n}.npy")
    if os.path.exists(path):
        return np.load(path)
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:n, 0:n]
    dy, dx = np.minimum(yy, n - yy), np.minimum(xx, n - xx)
    K = np.exp(-(dx ** 2 + dy ** 2) / (2 * sigma ** 2))
    def energy(B):
        return np.real(np.fft.ifft2(np.fft.fft2(B) * np.fft.fft2(K)))
    B = (rng.random((n, n)) < 0.1).astype(float)
    while True:  # phase 0: make the initial pattern evenly spread
        E = energy(B)
        c = np.argmax(np.where(B > 0, E, -1e9)); B.flat[c] = 0
        E = energy(B)
        v = np.argmin(np.where(B == 0, E, 1e9)); B.flat[v] = 1
        if v == c:
            break
    rank = np.zeros(n * n, int); ones = int(B.sum())
    B1 = B.copy()
    for r in range(ones - 1, -1, -1):  # phase 1
        E = energy(B1); c = np.argmax(np.where(B1 > 0, E, -1e9)); B1.flat[c] = 0; rank[c] = r
    B2 = B.copy()
    for r in range(ones, n * n):  # phase 2/3
        E = energy(B2); v = np.argmin(np.where(B2 == 0, E, 1e9)); B2.flat[v] = 1; rank[v] = r
    M = ((rank.reshape(n, n) + 0.5) / (n * n)).astype(np.float32)
    np.save(path, M)
    return M


# ---------------------------------------------------------------------------
# Methods
# ---------------------------------------------------------------------------
class Method:
    """quant: 'fs' | 'bn' | 'hed' | 'pattern' | 'pq' ; drive: 'counter' | 'int' | 'pq'
    Operates on the drive grid (h, w); the panel may repeat each drive row rr times."""
    def __init__(self, name, quant, drive, nlev=2, mu=0.0, T0c=3.8, s0c=0.3, taudc=120.0, maxn=6, dcmax=100, lam=0.0, marg=1, settle=0, budget=0, motion=None, auto=False):
        self.budget, self.motion, self.auto = budget, motion, auto   # motion: scripted (enter, exit) phases; auto: kernel decides
        self.name, self.quant, self.drive, self.nlev, self.mu, self.dcmax, self.lam, self.marg, self.settle = name, quant, drive, nlev, mu, dcmax, lam, marg, settle
        self.T0c, self.s0c, self.taudc, self.maxn = T0c, s0c, taudc, maxn
        assert drive in ("int", "pq") or nlev == 2

    def reset(self, h, w):
        self.h, self.w = h, w
        n = h * w
        self.err = np.zeros(2 * (w + 2), np.float32)
        self.lvl = np.full((h, w), self.nlev - 1, np.uint8)   # previous chosen level (starts white)
        self.out = np.zeros((h, w), np.uint8)
        self.cnt = np.full(n, N, np.uint8); self.color = np.ones(n, np.uint8)
        self.pos = np.full(n, N, np.uint8)
        self.u = np.zeros(n, np.int8)
        self.ph = np.ones((h, w), np.float32); self.dw = np.full((h, w), 1000.0, np.float32)
        self.sat = np.ones((h, w), np.int8); self.dcs = np.zeros((h, w), np.int8)
        self.prevI, self.static_n = None, 0
        if self.quant in ("pqq", "pqf", "pqw"):
            self.qE = np.zeros((h + 1) * (w + 2), np.int16)
            self.qp = PQParams()
            libq.pq_init_params(ctypes.byref(self.qp), self.T0c, self.s0c, self.taudc, self.mu, self.maxn, self.marg)
            self.qt = PQFast(); libq.pq_fast_init(ctypes.byref(self.qp), ctypes.byref(self.qt))
            self.qph = np.full((h, w), 255, np.uint8)
            self.qmeta = np.full((h, w), 7 | 8, np.uint8)   # long rest, margin applied, dc=0
            self.qerr = np.zeros(2 * (w + 2), np.int16)
            self.field_no = 0
        if self.quant == "pqbnv4":
            assert w % 32 == 0 and h % 4 == 0
            bn = blue_noise()
            self.v4tau = np.ascontiguousarray(np.clip((bn * 16).astype(int), 0, 15).astype(np.uint8))
            self.v4dec = np.zeros(16384, np.uint8); self.v4exk = np.zeros(6 * 256, np.uint8); stp = np.zeros(4, np.uint8)
            assert libv4.pqbn_v4_build(self.v4dec, self.v4exk, stp, self.T0c, self.s0c, self.taudc) == 0
            self.v4fb = np.full((h, w // 2), 0xFF, np.uint8); self.v4prev = np.full((h, w), 15, np.uint8)
            self.v4hot = np.zeros((h, w // 2), np.uint8); self.v4cold = np.zeros(h * w, np.uint16)
            ntx, nty = w // 32, h // 4
            self.v4dirty = np.zeros(ntx * nty, np.uint8); self.v4plan = np.zeros(ntx * nty, np.uint16)
            self.v4nmax = np.zeros(ntx * nty, np.uint8); self.v4drive = np.zeros(h * w // 4, np.uint8)
            self.v4rows = np.zeros(h, np.uint8)
            c = PQBNv4Ctx(); c.W, c.H, c.ntx, c.nty = w, h, ntx, nty
            c.dec = self.v4dec.ctypes.data; c.exk = self.v4exk.ctypes.data; c.step[:] = list(stp); c.marg = self.marg
            c.fb4 = self.v4fb.ctypes.data; c.hot = self.v4hot.ctypes.data; c.cold = self.v4cold.ctypes.data
            c.tau = self.v4tau.ctypes.data; c.tile_dirty = self.v4dirty.ctypes.data; c.tile_plan = self.v4plan.ctypes.data
            c.tile_nmax = self.v4nmax.ctypes.data; c.drive = self.v4drive.ctypes.data; c.row_active = self.v4rows.ctypes.data
            self.v4band = np.zeros(2 * nty, np.uint16); c.band = self.v4band.ctypes.data
            self.v4ctx = c; self.field_no = 1
            libv4.pqbn_v4_init(ctypes.byref(c), self.field_no)
            c.plan_budget = int(self.budget); c.auto_motion = int(bool(self.auto))
            self.v4exm = np.zeros(7 * 256, np.uint8); self.v4tn = np.zeros(2048, np.uint8)
            libv4.pqbn_v4_build_motion(self.v4exm, self.v4tn, self.v4tau.reshape(-1).copy())
            c.exk_m = self.v4exm.ctypes.data; c.tau_nib = self.v4tn.ctypes.data
            self.v4sel = np.ones(ntx * nty, np.uint8); self.motion_phases = 0
            self.words_active = 0; self.tiles_planned = 0
        if self.quant in ("pqbnv2", "pqbnv3"):
            assert w % 32 == 0 and h % 4 == 0
            bn = blue_noise()
            self.v2tau = np.ascontiguousarray(np.clip((bn * 16).astype(int), 0, 15).astype(np.uint8))
            self.v2lut = np.zeros(32768, np.uint8); stp = np.zeros(4, np.uint8)
            self.vlib = libv2 if self.quant == "pqbnv2" else libv3
            (libv2.pqbn_v2_build if self.quant == "pqbnv2" else libv3.pqbn_v3_build)(self.v2lut, stp, self.T0c, self.s0c, self.taudc)
            self.v2fb = np.zeros((h, w // 2), np.uint8); self.v2prev = np.full((h, w), 15, np.uint8)
            self.v2st = np.full(h * w, 0x83FF, np.uint16)
            ntx, nty = w // 32, h // 4
            self.v2dirty = np.zeros(ntx * nty, np.uint8); self.v2busy = np.zeros(ntx * nty, np.uint16)
            self.v2last = np.zeros(ntx * nty, np.uint16); self.v2drive = np.zeros(h * w // 4, np.uint8)
            self.v2rows = np.zeros(h, np.uint8); self.v2u = np.zeros(h * w, np.int8)
            c = PQBNv2Ctx(); c.W, c.H, c.ntx, c.nty = w, h, ntx, nty
            c.lut = self.v2lut.ctypes.data; c.step[:] = list(stp); c.marg = self.marg
            c.fb4 = self.v2fb.ctypes.data; c.st = self.v2st.ctypes.data; c.tau = self.v2tau.ctypes.data
            c.tile_dirty = self.v2dirty.ctypes.data; c.tile_busy = self.v2busy.ctypes.data
            c.tile_last = self.v2last.ctypes.data; c.drive = self.v2drive.ctypes.data
            c.row_active = self.v2rows.ctypes.data; c.u_dbg = self.v2u.ctypes.data
            self.v2ctx = c; self.field_no = 1; self.tiles_processed = 0
            self.v2dirty_seen = lambda td: (self.v2busy > 0) | td.reshape(-1)
        if self.quant in ("pqbnlut2", "pqbnev"):
            self.prevt = np.full(h * w, 15, np.uint8); self.busy = np.zeros(h * w, np.uint8)
            self.evaluated = 0; self.phases = 0
        if self.quant in ("pqbnlut2", "pqbnev"):
            bn = blue_noise()
            tl = np.tile(bn, (h // bn.shape[0] + 1, w // bn.shape[1] + 1))[:h, :w]
            self.tau4 = np.ascontiguousarray(np.clip((tl * 16).astype(int), 0, 15).astype(np.uint8).reshape(-1))
            self.l2 = np.zeros(65536, np.uint8); self.lstep = np.zeros(4, np.int32)
            liblut.pqbn_lut2_build(self.l2, self.lstep, self.T0c, self.s0c, self.taudc)
            self.gq8 = np.full(h * w, 255, np.uint8); self.gc = np.full(h * w, 3, np.uint8)
            self.ldc = np.zeros(h * w, np.int8); self.lmf = np.ones(h * w, np.uint8); self.field_no = 0
        if self.quant == "pqbnlutg":
            bn = blue_noise()
            tl = np.tile(bn, (h // bn.shape[0] + 1, w // bn.shape[1] + 1))[:h, :w]
            self.tau4 = np.ascontiguousarray(np.clip((tl * 16).astype(int), 0, 15).astype(np.uint8).reshape(-1))
            self.glut = np.zeros(1 << (self.TB + self.QB + 6), np.uint16)
            liblut.pqbn_lutg_build(self.glut, self.TB, self.QB, self.T0c, self.s0c, self.taudc)
            self.gq = np.full(h * w, (1 << self.QB) - 1, np.uint16); self.gc = np.full(h * w, 3, np.uint8)
            self.ldc = np.zeros(h * w, np.int8); self.lmf = np.ones(h * w, np.uint8); self.field_no = 0
        if self.quant == "pqbnlut":
            bn = blue_noise()
            tl = np.tile(bn, (h // bn.shape[0] + 1, w // bn.shape[1] + 1))[:h, :w]
            self.tau4 = np.ascontiguousarray(np.clip((tl * 16).astype(int), 0, 15).astype(np.uint8).reshape(-1))
            self.lut = np.zeros(65536, np.uint8); liblut.pqbn_lut_build(self.lut, self.T0c, self.s0c, self.taudc)
            self.lst = np.full(h * w, 63 | (3 << 6), np.uint8); self.ldc = np.zeros(h * w, np.int8)
            self.lmf = np.ones(h * w, np.uint8); self.field_no = 0
        if self.quant == "pqbnq":
            bn = blue_noise()
            tl = np.tile(bn, (h // bn.shape[0] + 1, w // bn.shape[1] + 1))[:h, :w]
            self.tau8 = np.ascontiguousarray(np.clip(tl * 255.0, 0, 255).astype(np.uint8).reshape(-1))
            self.bp = PQBNParams(); libbn.pqbn_init(ctypes.byref(self.bp), self.T0c, self.s0c, self.taudc, self.marg)
            self.bph = np.full(h * w, 255, np.uint8); self.bdw = np.full(h * w, 7 | 8, np.uint8)
            self.bdc = np.zeros(h * w, np.int8); self.field_no = 0
        if self.quant == "pqbn":
            bn = blue_noise()
            self.tau = np.ascontiguousarray(np.tile(bn, (h // bn.shape[0] + 1, w // bn.shape[1] + 1))[:h, :w], dtype=np.float32)
        if self.quant == "bn":
            bn = blue_noise()
            self.bn = np.tile(bn, (h // bn.shape[0] + 1, w // bn.shape[1] + 1))[:h, :w].copy()

    def step(self, I):
        """One field: input image (or level map for 'pattern') -> drive u (h, w)."""
        if self.quant in ("pqq", "pqf", "pqw"):   # integer implementations (pq_fixed.c)
            u = self.u.reshape(self.h, self.w)
            if self.settle:
                same = self.prevI is not None and np.array_equal(I, self.prevI)
                self.static_n = self.static_n + 1 if same else 0
                self.prevI = I.copy()
            self.qp.rails_only = int(bool(self.settle) and self.static_n >= self.settle)
            I8 = np.clip(I * 255.0 + 0.5, 0, 255).astype(np.uint8)
            if self.quant == "pqw":
                libq.pq_field_q8_wf(I8, self.qph, self.qmeta, u, self.w, self.h, ctypes.byref(self.qp), ctypes.byref(self.qt), self.field_no, self.qE)
            elif self.quant == "pqf":
                libq.pq_field_q8_fast(I8, self.qph, self.qmeta, u, self.w, self.h, ctypes.byref(self.qp), ctypes.byref(self.qt), self.field_no, self.qerr)
            else:
                libq.pq_field_q8(I8, self.qph, self.qmeta, u, self.w, self.h, ctypes.byref(self.qp), self.field_no, self.qerr)
            self.field_no += 1
            return u
        if self.quant == "pqbnv4":
            t4 = np.clip(np.rint(I * 15.0), 0, 15).astype(np.uint8)
            self.v4fb[...] = (t4[:, 0::2] | (t4[:, 1::2] << 4))
            ch = (t4 != self.v4prev)
            self.v4prev[...] = t4
            td = ch.reshape(self.h // 4, 4, self.w // 32, 32).any(axis=(1, 3))
            self.v4dirty |= td.astype(np.uint8).reshape(-1)
            self.tiles_planned += int(np.count_nonzero(self.v4dirty))
            for (te, tx) in (self.motion or []):
                if self.field_no == te: libv4.pqbn_v4_motion_enter(ctypes.byref(self.v4ctx), self.field_no, self.v4sel)
                if self.field_no == tx: libv4.pqbn_v4_motion_exit(ctypes.byref(self.v4ctx), self.field_no, None)
                if te <= self.field_no < tx: self.motion_phases += 1
            if self.auto and self.v4ctx.n_motion > 0: self.motion_phases += 1
            self.words_active += libv4.pqbn_v4_phase(ctypes.byref(self.v4ctx), self.field_no)
            self.field_no += 1
            return decode_drive(self.v4drive, self.v4rows, self.h, self.w)
        if self.quant in ("pqbnv2", "pqbnv3"):
            t4 = np.clip(np.rint(I * 15.0), 0, 15).astype(np.uint8)
            self.v2fb[...] = (t4[:, 0::2] | (t4[:, 1::2] << 4))
            ch = (t4 != self.v2prev)
            self.v2prev[...] = t4
            # stand-in for LVGL invalidated areas: tiles containing any changed target pixel
            td = ch.reshape(self.h // 4, 4, self.w // 32, 32).any(axis=(1, 3))
            self.v2dirty |= td.astype(np.uint8).reshape(-1)
            self.v2u[...] = 0
            (libv2.pqbn_v2_phase if self.quant == "pqbnv2" else libv3.pqbn_v3_phase)(ctypes.byref(self.v2ctx), self.field_no)
            self.tiles_processed += int(np.count_nonzero(self.v2dirty_seen(td)))
            self.field_no += 1
            return self.v2u.reshape(self.h, self.w)
        if self.quant == "pqbnev":
            t4 = np.clip(np.rint(I * 15.0), 0, 15).astype(np.uint8).reshape(-1)
            ev = ci(0)
            liblut.pqbn_lut2_phase_ev(self.l2, self.lstep, t4, self.prevt, self.tau4, self.gq8, self.gc, self.ldc,
                                      self.lmf, self.busy, self.u, self.h * self.w, self.field_no, self.marg, ctypes.byref(ev))
            self.evaluated += ev.value; self.phases += 1
            self.field_no += 1
            return self.u.reshape(self.h, self.w)
        if self.quant == "pqbnlut2":
            t4 = np.clip(np.rint(I * 15.0), 0, 15).astype(np.uint8).reshape(-1)
            liblut.pqbn_lut2_phase(self.l2, self.lstep, t4, self.tau4, self.gq8, self.gc, self.ldc, self.lmf,
                                   self.u, self.h * self.w, self.field_no, self.marg)
            self.field_no += 1
            return self.u.reshape(self.h, self.w)
        if self.quant == "pqbnlutg":
            tt = np.clip(np.rint(I * ((1 << self.TB) - 1)), 0, (1 << self.TB) - 1).astype(np.uint8).reshape(-1)
            liblut.pqbn_lutg_phase(self.glut, self.TB, self.QB, tt, self.tau4, self.gq, self.gc, self.ldc, self.lmf,
                                   self.u, self.h * self.w, self.field_no, self.marg)
            self.field_no += 1
            return self.u.reshape(self.h, self.w)
        if self.quant == "pqbnlut":
            t4 = np.clip(np.rint(I * 15.0), 0, 15).astype(np.uint8).reshape(-1)
            liblut.pqbn_lut_phase(self.lut, t4, self.tau4, self.lst, self.ldc, self.lmf, self.u,
                                  self.h * self.w, self.field_no, self.marg)
            self.field_no += 1
            return self.u.reshape(self.h, self.w)
        if self.quant == "pqbnq":
            I8 = np.clip(I * 255.0 + 0.5, 0, 255).astype(np.uint8)
            libbn.pqbn_q8(I8.reshape(-1), self.tau8, self.bph, self.bdw, self.bdc, self.u, self.h * self.w,
                          ctypes.byref(self.bp), self.field_no)
            self.field_no += 1
            return self.u.reshape(self.h, self.w)
        if self.quant == "pqbn":
            u = self.u.reshape(self.h, self.w)
            lib.pqbn_field(I, self.ph, self.dw, self.sat, u, self.dcs, self.tau, self.w, self.h,
                           self.T0c, self.s0c, self.taudc, self.mu, self.maxn, self.marg, self.lam)
            return u
        if self.quant == "pq":
            u = self.u.reshape(self.h, self.w)
            # frame-level static detector (like PaperBoy's dirty-line tracking): after `settle`
            # unchanged frames, re-quantize to rails only (calibration-insensitive)
            if self.settle:
                same = self.prevI is not None and np.array_equal(I, self.prevI)
                self.static_n = self.static_n + 1 if same else 0
                self.prevI = I.copy()
            ro = int(bool(self.settle) and self.static_n >= self.settle)
            lib.pq_field(I, self.ph, self.dw, self.sat, u, self.dcs, self.w, self.h,
                         self.T0c, self.s0c, self.taudc, self.mu, self.maxn, self.dcmax, self.lam, self.marg, ro, self.err)
            return u
        if self.quant == "fs":
            lib.fs_binary(I, self.out, self.w, self.h, self.err)
        elif self.quant == "bn":
            self.out[...] = (I > self.bn)
        elif self.quant == "hed":
            lib.hed(I, self.lvl, self.out, self.w, self.h, self.nlev, self.mu, self.err)
        elif self.quant == "pattern":
            self.out[...] = I
        self.lvl[...] = self.out
        flat = self.out.reshape(-1)
        if self.drive == "counter":
            lib.drive_counter(flat, self.cnt, self.color, self.u, flat.size, N)
        else:
            tgt = (flat.astype(np.int32) * N // (self.nlev - 1)).astype(np.uint8)
            lib.drive_integrator(tgt, self.pos, self.u, flat.size)
        return self.u.reshape(self.h, self.w)


def run(method, frames, target_fn, cfg, sigma=1.2, record=False, flush=True, seed=1, rr=1, flush_input=None):
    """frames[t]: quantizer input on the drive grid (float image, or level map for 'pattern').
    target_fn(t): intended luminance on the panel grid. rr: panel rows per drive row."""
    T = len(frames)
    hd, w = frames[0].shape
    h = hd * rr
    method.reset(hd, w)
    panel = Panel(h, w, cfg, seed=seed)
    sq, shim, pulses = 0.0, 0.0, 0.0
    prevGR = prevGI = None
    rec = []
    for t in range(T):
        u = method.step(np.ascontiguousarray(frames[t]))
        if rr > 1:
            u = np.repeat(u, rr, axis=0)
        panel.step(u)
        pulses += np.mean(u != 0)
        GI = gaussian_filter(target_fn(t), sigma)
        GR = gaussian_filter(panel.p, sigma)
        sq += np.mean((GR - GI) ** 2)
        if prevGR is not None:
            shim += np.mean(np.abs((GR - prevGR) - (GI - prevGI)))
        prevGR, prevGI = GR, GI
        if record:
            rec.append((panel.p * 255 + 0.5).astype(np.uint8))
    res = dict(name=method.name,
               psnr=10 * np.log10(1.0 / (sq / T)),
               psnr_final=10 * np.log10(1.0 / np.mean((prevGR - prevGI) ** 2)),
               shimmer=shim / (T - 1) * 1000,          # x1e-3
               pulses=pulses / T * 100,                # % pixels driven per field
               dc_rms=float(np.sqrt(np.mean(panel.dc.astype(np.float64) ** 2))),
               dc_max=int(np.abs(panel.dc).max()))
    if flush:
        for val, key in ((1.0, "ghost_white"), (0.0, "ghost_black")):
            inp = flush_input(val) if flush_input else np.full((hd, w), val, np.float32)
            for _ in range(60):
                u = method.step(np.ascontiguousarray(inp))
                if rr > 1:
                    u = np.repeat(u, rr, axis=0)
                panel.step(u)
                if record:
                    rec.append((panel.p * 255 + 0.5).astype(np.uint8))
            res[key] = float(np.sqrt(np.mean((panel.p - val) ** 2)) * 100)  # % of full scale
    if record:
        res["rec"] = rec
    return res
