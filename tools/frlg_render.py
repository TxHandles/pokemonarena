#!/usr/bin/env python3
"""Renders FireRed/LeafGreen maps from the pret/pokefirered decompilation (data/layouts, data/tilesets)
into PNG pictures, one pixel per GBA pixel (16 px per tile).

    frlg_render.py <pokefirered dir> <out dir> [LayoutName ...]

For each layout it writes <Name>.png (both tile layers) and <Name>_over.png (just the top layer of the
metatiles drawn over people: tree tops, roofs, the tops of signs - transparent elsewhere)."""
import json, os, struct, sys
from PIL import Image

NUM_TILES_PRIMARY = 640
NUM_METATILES_PRIMARY = 640
NUM_PALS_PRIMARY = 7


def load_pal(path):
    lines = open(path).read().split()
    n = int(lines[2])
    v = list(map(int, lines[3:3 + n * 3]))
    return [tuple(v[i * 3:i * 3 + 3]) for i in range(n)]


class Tileset:
    def __init__(self, folder):
        self.folder = folder
        gfx = folder if os.path.exists(os.path.join(folder, "tiles.png")) else os.path.join(os.path.dirname(folder), "condominiums")   # Silph Co. shares the Condominiums' tiles
        im = Image.open(os.path.join(gfx, "tiles.png"))
        if im.mode != "P":
            raise SystemExit("tiles.png not indexed: " + folder)
        self.tw, self.th = im.size[0] // 8, im.size[1] // 8
        px = im.load()
        self.tiles = []
        for ty in range(self.th):
            for tx in range(self.tw):
                self.tiles.append([[px[tx * 8 + x, ty * 8 + y] & 15 for x in range(8)] for y in range(8)])
        d = open(os.path.join(folder, "metatiles.bin"), "rb").read()
        self.metatiles = [struct.unpack_from("<8H", d, i * 16) for i in range(len(d) // 16)]
        a = open(os.path.join(folder, "metatile_attributes.bin"), "rb").read()
        self.attrs = [struct.unpack_from("<I", a, i * 4)[0] for i in range(len(a) // 4)]
        pd = os.path.join(gfx, "palettes")
        self.pals = [load_pal(os.path.join(pd, "%02d.pal" % i)) for i in range(16)]


def find_tilesets(root):
    found = {}
    for kind in ("primary", "secondary"):
        base = os.path.join(root, "data/tilesets", kind)
        for f in os.listdir(base):
            found[f.replace("_", "").lower()] = os.path.join(base, f)
    return found


_cache = {}
def tileset(root, name, found):
    key = name.replace("gTileset_", "").lower()
    if key not in _cache:
        _cache[key] = Tileset(found[key])
    return _cache[key]


def metatile_info(prim, sec, mid):
    if mid < NUM_METATILES_PRIMARY:
        return (prim.metatiles[mid] if mid < len(prim.metatiles) else (0,) * 8), (prim.attrs[mid] if mid < len(prim.attrs) else 0)
    j = mid - NUM_METATILES_PRIMARY
    return (sec.metatiles[j] if j < len(sec.metatiles) else (0,) * 8), (sec.attrs[j] if j < len(sec.attrs) else 0)


def render_layout(root, lay, found):
    prim = tileset(root, lay["primary_tileset"], found)
    sec = tileset(root, lay["secondary_tileset"], found)
    pals = prim.pals[:NUM_PALS_PRIMARY] + sec.pals[NUM_PALS_PRIMARY:]
    W, H = lay["width"], lay["height"]
    blocks = struct.unpack("<%dH" % (W * H), open(os.path.join(root, lay["blockdata_filepath"]), "rb").read()[:W * H * 2])
    img = Image.new("RGB", (W * 16, H * 16), pals[0][0])
    over = Image.new("RGBA", (W * 16, H * 16), (0, 0, 0, 0))
    pi, po = img.load(), over.load()
    cells = []

    def tile(entry):
        t = entry & 0x3FF
        if t < NUM_TILES_PRIMARY:
            return prim.tiles[t] if t < len(prim.tiles) else None
        t -= NUM_TILES_PRIMARY
        return sec.tiles[t] if t < len(sec.tiles) else None

    def draw(entry, ox, oy, dst, rgba):
        tl = tile(entry)
        if tl is None:
            return
        hf, vf, pal = entry >> 10 & 1, entry >> 11 & 1, entry >> 12 & 15
        cols = pals[pal]
        for y in range(8):
            row = tl[7 - y if vf else y]
            for x in range(8):
                c = row[7 - x if hf else x]
                if c == 0:
                    continue
                dst[ox + x, oy + y] = cols[c] + ((255,) if rgba else ())

    for by in range(H):
        for bx in range(W):
            b = blocks[by * W + bx]
            mid, coll, elev = b & 0x3FF, b >> 10 & 3, b >> 12 & 15
            mt, attr = metatile_info(prim, sec, mid)
            layer = attr >> 29 & 3          # 0 normal (top layer over people), 1 covered, 2 split
            behavior = attr & 0x1FF
            ox, oy = bx * 16, by * 16
            for k in range(4):              # the bottom layer
                draw(mt[k], ox + (k & 1) * 8, oy + (k >> 1) * 8, pi, False)
            for k in range(4):              # the top layer (into the picture; over people too when "normal")
                draw(mt[4 + k], ox + (k & 1) * 8, oy + (k >> 1) * 8, pi, False)
                if layer == 0:
                    draw(mt[4 + k], ox + (k & 1) * 8, oy + (k >> 1) * 8, po, True)
            cells.append((mid, coll, elev, behavior))
    return img, over, cells


def main():
    root, out = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3:])
    os.makedirs(out, exist_ok=True)
    found = find_tilesets(root)
    lays = json.load(open(os.path.join(root, "data/layouts/layouts.json")))["layouts"]
    for lay in lays:
        if "primary_tileset" not in lay or "NULL" in (lay["primary_tileset"], lay["secondary_tileset"]):
            continue
        name = lay["name"].replace("_Layout", "")
        if only and name not in only:
            continue
        img, over, _ = render_layout(root, lay, found)
        img.save(os.path.join(out, name + ".png"))
        if over.getbbox():
            over.save(os.path.join(out, name + "_over.png"))
    print("done")


if __name__ == "__main__":
    main()
