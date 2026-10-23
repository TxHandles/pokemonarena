#!/usr/bin/env python3
"""Makes the game's PNGs smaller without changing a single pixel.

    png_shrink.py [folders or files...]      (default: everything the game ships, plus art_source/)

Most of the art is old handheld-game art with few colours: a PNG with up to 256 colours is saved with a palette
(a transparency table when it has see-through pixels), everything else just with the best compression. Every new
file is decoded again and compared with the old one pixel by pixel (RGBA, transparent pixels' colours too); it
only replaces the old file when it's identical and smaller. Run it again after a tool regenerates images."""
import io, os, sys
from multiprocessing import Pool
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = ["Pokemon", "World", "Badges", "Leaders", "art_source", "map.png", "johto.png", "hoenn.png", "sinnoh.png", "unova.png"]


def rgba_bytes(im):
    return im.convert("RGBA").tobytes()


def candidates(im):
    rgba = im.convert("RGBA")
    has_alpha = rgba.getextrema()[3][0] < 255
    out = []
    px = np.asarray(rgba).reshape(-1, 4)
    key = px.view(np.uint32).ravel()
    uniq, inv = np.unique(key, return_inverse=True)
    if len(uniq) <= 256:
        colors = uniq.view(np.uint8).reshape(-1, 4)
        order = np.argsort(colors[:, 3] == 255, kind="stable")   # see-through colours first: a shorter transparency table
        rank = np.empty_like(order)
        rank[order] = np.arange(len(order))
        colors = colors[order]
        p = Image.frombytes("P", rgba.size, rank[inv.ravel()].astype(np.uint8).tobytes())
        p.putpalette(colors[:, :3].ravel().tolist())
        buf = io.BytesIO()
        if has_alpha:
            alpha = bytes(colors[:, 3].tolist())
            last = max(i for i, a in enumerate(alpha) if a < 255)
            p.save(buf, "PNG", optimize=True, transparency=alpha[:last + 1])
        else:
            p.save(buf, "PNG", optimize=True)
        out.append(buf.getvalue())
    buf = io.BytesIO()
    (rgba if has_alpha else rgba.convert("RGB")).save(buf, "PNG", optimize=True)
    out.append(buf.getvalue())
    return out


def shrink(path):
    try:
        old = open(path, "rb").read()
        im = Image.open(io.BytesIO(old))
        im.load()
        want = rgba_bytes(im)
        best = None
        for data in candidates(im):
            if len(data) >= len(old) or (best and len(data) >= len(best)):
                continue
            if rgba_bytes(Image.open(io.BytesIO(data))) == want:
                best = data
        if best:
            open(path, "wb").write(best)
            return len(old), len(best)
        return len(old), len(old)
    except Exception as e:   # not a PNG we can read: leave it be
        print("skipped", path, e)
        return 0, 0


def main():
    targets = sys.argv[1:] or [os.path.join(ROOT, t) for t in DEFAULT]
    files = []
    for t in targets:
        if os.path.isdir(t):
            for d, _, fs in os.walk(t):
                files += [os.path.join(d, f) for f in fs if f.lower().endswith(".png")]
        elif t.lower().endswith(".png") and os.path.exists(t):
            files.append(t)
    with Pool() as pool:
        res = pool.map(shrink, files, chunksize=16)
    before = sum(r[0] for r in res)
    after = sum(r[1] for r in res)
    print(f"{len(files)} PNGs: {before / 1e6:.1f} MB -> {after / 1e6:.1f} MB ({100 * (1 - after / max(1, before)):.0f}% smaller)")


if __name__ == "__main__":
    main()
