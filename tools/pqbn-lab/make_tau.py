#!/usr/bin/env python3
"""A 64x64 blue-noise threshold map, by void and cluster (Ulichney 1993), to drivers/display/pqbn/pqbn_tau.h.

The kernel's first map was a lattice, which shows as diagonal stripes on gray on the real panel."""
import numpy as np

N, SIGMA = 64, 1.9
x = np.fft.fftfreq(N)
k = np.exp(-2 * (np.pi * SIGMA) ** 2 * (x[:, None] ** 2 + x[None, :] ** 2))


def energy(a):
    return np.real(np.fft.ifft2(np.fft.fft2(a) * k))


def tightest(a):
    return np.unravel_index(np.argmax(np.where(a, energy(a), -np.inf)), a.shape)


def largest_void(a):
    return np.unravel_index(np.argmin(np.where(a, np.inf, energy(a))), a.shape)


rng = np.random.default_rng(20261106)
a = np.zeros((N, N), bool)
a[tuple(rng.integers(0, N, (2, N * N // 10)))] = True
while True:
    c = tightest(a)
    a[c] = False
    v = largest_void(a)
    if v == c:
        a[c] = True
        break
    a[v] = True
rank = np.zeros((N, N), int)
start = a.copy()
ones = int(a.sum())
b = start.copy()
for r in range(ones - 1, -1, -1):
    c = tightest(b)
    b[c] = False
    rank[c] = r
b = start.copy()
for r in range(ones, N * N // 2):
    v = largest_void(b)
    b[v] = True
    rank[v] = r
for r in range(N * N // 2, N * N):
    c = tightest(~b)
    b[c] = True
    rank[c] = r
tau = rank * 16 // (N * N)
assert all((tau == i).sum() == N * N // 16 for i in range(16))
with open("drivers/display/pqbn/pqbn_tau.h", "w") as f:
    f.write("/* 64x64 blue-noise thresholds 0..15, from tools/pqbn-lab/make_tau.py */\n")
    f.write("#ifndef INK_PQBN_TAU_H\n#define INK_PQBN_TAU_H\n#include <stdint.h>\n")
    f.write("static const uint8_t PQBN_TAU[4096] = {\n")
    for row in tau:
        f.write(",".join(str(v) for v in row) + ",\n")
    f.write("};\n#endif\n")
