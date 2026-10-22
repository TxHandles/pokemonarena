#!/usr/bin/env python3
"""Megas should look at least as big as the Pokemon they come from. The game sizes a picture from its frames
(up to 32 px: 4/3 units a pixel; bigger frames are fitted to 26 units) times the Pokedex size (height and
weight). Where a Mega would come out smaller than its base form, this writes "sprite_size" into the Mega's
pokemon.txt so it's about 10% bigger than the base form instead.   python tools/mega_sizes.py"""
import glob, os, re
from PIL import Image
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Pokemon')

def val(t, k):
    m = re.search(r'^' + k + r'\s*=\s*([0-9.]+)', t, re.M)
    return float(m.group(1)) if m else None

def onscreen(name):
    folder = os.path.join(ROOT, name)
    t = open(os.path.join(folder, 'pokemon.txt'), encoding='utf-8').read()
    md = max(max(Image.open(f).size) for f in glob.glob(os.path.join(folder, '[0-9]*_[1-4]_*_[12].png')))
    px = 4 / 3 if md <= 32 else 26 / md
    ss, h, w, size = val(t, 'sprite_size'), val(t, 'height'), val(t, 'weight'), val(t, 'size')
    ds = (max(0.6, min(1.7, 0.65 * h ** 0.35 + 0.35 * (w / 20) ** 0.2)) if h and w else size or 1.0)
    return md * px, ds, ss, t

for folder in sorted(glob.glob(os.path.join(ROOT, '*'))):
    name = os.path.basename(folder)
    p = os.path.join(folder, 'pokemon.txt')
    if not os.path.exists(p):
        continue
    t = open(p, encoding='utf-8').read()
    m = re.search(r'^mega_of\s*=\s*(.+)$', t, re.M)
    if not m:
        continue
    base = m.group(1).strip()
    bu, bds, bss, _ = onscreen(base)
    mu, mds, mss, _ = onscreen(name)
    t = re.sub(r'\nsprite_size\s*=.*', '', t)
    mega, want = mu * mds, bu * (bss or bds) * 1.1
    if mega < bu * (bss or bds) * 1.02:
        s = min(4.0, want / mu)
        t = t.rstrip('\n') + f'\nsprite_size = {s:.2f}   # (tools/mega_sizes.py) so it looks bigger than {base}\n'
        print(f'{name:18s} {mega:6.1f} -> {want:6.1f} (base {base} {bu * (bss or bds):.1f})')
    open(p, 'w', encoding='utf-8').write(t)
