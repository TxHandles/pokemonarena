"""Read a plain + numbered pixel chart pair (numbers mark the filled squares; the legend gives each colour
and its count) into a one-pixel-per-square image, colours snapped to the legend.
    python tools/chart_pair.py plain numbered out.png cols rows legend_x count1,count2,..."""
import sys
from PIL import Image
import numpy as np
from collections import Counter
plain, num, out, cols, rows, lx = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6])
exp = [int(x) for x in sys.argv[7].split(',')]
P = np.array(Image.open(plain).convert('RGB')).astype(int)
N = np.array(Image.open(num).convert('RGB')).astype(int)
pitch = P.shape[0] / (rows + 1.0)
cells = np.zeros((rows, cols, 3), int); filled = np.zeros((rows, cols), bool)
for r in range(rows):
    for c in range(cols):
        x0, y0 = pitch * (c + 1), pitch * (r + 1)
        rim = np.concatenate([P[int(y0 + 3):int(y0 + 7), int(x0 + 3):int(x0 + pitch - 3)].reshape(-1, 3),
                              P[int(y0 + pitch - 7):int(y0 + pitch - 3), int(x0 + 3):int(x0 + pitch - 3)].reshape(-1, 3)])
        cells[r, c] = np.median(rim, 0)
        bp = P[int(y0 + 5):int(y0 + pitch - 5), int(x0 + 5):int(x0 + pitch - 5)]
        bn = N[int(y0 + 5):int(y0 + pitch - 5), int(x0 + 5):int(x0 + pitch - 5)]
        filled[r, c] = np.abs(bp - bn).sum(2).max() > 150 or np.abs(cells[r, c] - 255).sum() > 24   # a number, or not the white paper
pal = []
for k in range(len(exp)):
    y0 = pitch * (k + 1)
    pal.append(np.median(P[int(y0 + 3):int(y0 + 7), lx:lx + 12].reshape(-1, 3), 0))
pal = np.array(pal)
idx = np.argmin(((cells[:, :, None] - pal[None, None]) ** 2).sum(3), 2)
cnt = Counter(idx[filled].tolist())
print('pitch', round(pitch, 2), 'counts (colour, got, chart):', [(k + 1, cnt.get(k, 0), exp[k]) for k in range(len(exp))])
o = np.zeros((rows, cols, 4), np.uint8)
o[filled, :3] = pal[idx[filled]].round(); o[filled, 3] = 255
im = Image.fromarray(o, 'RGBA'); im = im.crop(im.getbbox()); im.save(out); print(im.size)
