"""Draws johto.png (the Johto story map, 1142x857 like map.png) from the Gold/Silver town map
(johto_native.png, 160x144): land, mountains, routes, sea and towns are read from it, then drawn in
the FireRed-style palette of the Kanto map, with the Kanto map's town markers.

    python art_source/johto_map/make_johto_map.py
Writes johto.png next to story.txt and prints the town / landmark positions for story.txt.
"""
import os, numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
nat = np.array(Image.open(os.path.join(HERE, "johto_native.png")).convert("RGB")).astype(int)
H, W, _ = nat.shape
def near(c, t=60): return np.abs(nat - c).sum(2) < t
SEA, LAND, MTN, ROAD, TOWN, NAVY = 0, 1, 2, 3, 4, 5
cls = np.full((H, W), -1)
cls[near([1, 3, 138])] = SEA
cls[near([0, 249, 1]) | near([1, 248, 2]) | near([54, 225, 48])] = LAND
cls[near([120, 57, 1])] = MTN
cream = near([222, 249, 159]) | near([223, 239, 170])
cls[cream] = ROAD
cls[near([23, 12, 91])] = NAVY
# cream inside the sea is the waves' highlight, not a road
seaCount = ndimage.uniform_filter((cls == SEA).astype(float), 5) * 25
cls[cream & (seaCount >= 4)] = SEA
# the frame: keep the inside (x 8..151, y 8..135); the notch top-right (outside Johto) becomes mountains
x0, x1, y0, y1 = 8, 152, 8, 136
cls = cls[y0:y1, x0:x1].copy()
h, w = cls.shape
notch = np.zeros_like(cls, bool)
notch[:15, 47:] = True
cls[notch] = MTN
# the "JOHTO" label bottom-right: sea
cls[116:, 100:] = SEA
# towns: navy-outlined 8x8 squares
towns = []
lab, n = ndimage.label(cls == NAVY)
for i in range(1, n + 1):
    ys, xs = np.nonzero(lab == i)
    if 24 <= len(ys) <= 40 and xs.max() - xs.min() in (7, 8) and ys.max() - ys.min() in (7, 8, 9, 10, 11):
        towns.append(((xs.min() + xs.max()) / 2 + 0.5, (ys.min() + ys.max()) / 2 + 0.5))
        cls[ys.min():ys.max() + 1, xs.min():xs.max() + 1] = TOWN
# other navy: the sea route's markers are sea, coastlines are land
sc2 = ndimage.uniform_filter((cls == SEA).astype(float), 5) * 25
cls[(cls == NAVY) & (sc2 >= 6)] = SEA
cls[cls == NAVY] = LAND
# unknown: the nearest known class
unk = cls < 0
if unk.any():
    idx = ndimage.distance_transform_edt(unk, return_distances=False, return_indices=True)
    cls = cls[idx[0], idx[1]]
# roads through towns stay roads; towns become road under the marker
cls[cls == TOWN] = ROAD

# ---- render at 1/5 of the final size, then scale x5 (chunky pixels like map.png)
OUT_W, OUT_H = 1142, 857
S = OUT_H / h                      # native px -> final px (uniform)
mw = int(round(w * S)); ox = (OUT_W - mw) // 2
cell = 5
gw, gh = OUT_W // cell + 1, OUT_H // cell + 1
ys = ((np.arange(gh) * cell + cell / 2) / S).astype(int).clip(0, h - 1)
xs_ = ((np.arange(gw) * cell + cell / 2 - ox) / S)
g = np.empty((gh, gw), int)
for j in range(gw):
    x = xs_[j]
    xi = int(np.clip(x, 0, w - 1))
    col = cls[ys, xi]
    if x < 0 or x >= w:   # the margins: west of Johto is sea, east is Kanto's mountains
        col = np.where(cls[ys, 0 if x < 0 else w - 1] == SEA, SEA, MTN if x >= w else SEA)
    g[:, j] = col
g = ndimage.median_filter(g, size=3)   # no one-cell slivers
land = g != SEA
# height: distance from the sea, plus the mountains, smoothed into soft hills
dist = ndimage.distance_transform_edt(land)
hgt = np.minimum(dist, 14) / 14.0 + (g == MTN) * 0.55
hgt = ndimage.gaussian_filter(hgt, 2.2)
rng = np.random.default_rng(7)
hgt += ndimage.gaussian_filter(rng.normal(0, 1, hgt.shape), 3) * 0.08
greens = [(24, 132, 8), (57, 173, 8), (82, 206, 8), (115, 231, 33), (173, 247, 57)]
roadc = [(231, 165, 0), (239, 189, 57), (247, 214, 82), (239, 231, 115)]
img = np.zeros((gh, gw, 3), np.uint8)
lv = np.clip((hgt * 4.2).astype(int), 0, 4)
coast = land & (dist <= 1.0)
for k, c in enumerate(greens): img[land & (lv == k)] = c
img[coast] = greens[0]
# mountains: darker, with ridges
mt = (g == MTN) & ~coast
rn = ndimage.gaussian_filter(rng.normal(0, 1, hgt.shape), 2.0)
img[mt] = greens[0]
img[mt & (rn > 0.12)] = greens[1]
img[mt & (rn > 0.32)] = greens[2]
# roads, a little lighter in places
road = g == ROAD
shade = ndimage.gaussian_filter(rng.normal(0, 1, hgt.shape), 4)
rl = np.clip(((shade + 0.4) * 2.2).astype(int), 0, 3)
for k, c in enumerate(roadc): img[road & (rl == k)] = c
# the sea: stripes
sea = ~land
for y in range(gh):
    img[y][sea[y]] = (165, 181, 255) if (y // 1) % 2 else (156, 214, 255)
big = Image.fromarray(img).resize((gw * cell, gh * cell), Image.NEAREST).crop((0, 0, OUT_W, OUT_H))
# town markers: the Kanto map's own (Pallet Town's), pasted at each town
kanto = Image.open(os.path.join(ROOT, "map.png")).convert("RGBA")
gem = kanto.crop((237, 618, 286, 667))
gemA = np.array(gem).astype(int)
bgc = gemA[0, 0, :3]
alpha = (np.abs(gemA[..., :3] - bgc).sum(2) > 40) | (np.arange(49)[None, :] * 0 > 1)
# keep only the marker: the square around its centre
mask = np.zeros(alpha.shape, bool); mask[1:48, 1:48] = True
gemA[..., 3] = np.where(mask & alpha, 255, 0)
gem = Image.fromarray(gemA.astype(np.uint8), "RGBA")
def P(x, y): return (ox + x * S, y * S)
out = []
for (tx, ty) in towns:
    cx, cy = P(tx, ty)
    big.paste(gem, (int(cx - 24), int(cy - 24)), gem)
    out.append((round(cx), round(cy)))
big.convert("RGB").save(os.path.join(ROOT, "johto.png"))
print("towns (native -> map):")
for (tx, ty), (cx, cy) in zip(towns, out): print(f"  {tx:5.1f},{ty:5.1f} -> {cx}, {cy}")
def M(x, y): return tuple(round(v) for v in P(x - x0, y - y0))
print("transform: map = (%d + (x-8)*%.4f, (y-8)*%.4f)" % (ox, S, S))
