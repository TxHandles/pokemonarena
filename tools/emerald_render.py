#!/usr/bin/env python3
"""Renders Emerald maps from the pret/pokeemerald decompilation (data/layouts, data/tilesets) into PNG
pictures, one pixel per GBA pixel (16 px per tile). The same as tools/frlg_render.py, with Emerald's sizes:
512 tiles and metatiles in a primary tileset, 6 primary palettes, 16-bit metatile attributes
(behavior in bits 0-7, layer type in bits 12-15).

    emerald_render.py <pokeemerald dir> <out dir> [LayoutName ...]

For each layout it writes <Name>.png (both tile layers) and <Name>_over.png (just the top layer of the
metatiles drawn over people: tree tops, roofs - transparent elsewhere)."""
import json, os, struct, sys
from PIL import Image

NUM_TILES_PRIMARY = 512
NUM_METATILES_PRIMARY = 512
NUM_PALS_PRIMARY = 6


def load_pal(path):
    lines = open(path).read().split()
    n = int(lines[2])
    v = list(map(int, lines[3:3 + n * 3]))
    return [tuple(v[i * 3:i * 3 + 3]) for i in range(n)]


class Tileset:
    def __init__(self, folder):
        self.folder = folder
        self.tiles = []
        tp = os.path.join(folder, "tiles.png")
        if os.path.exists(tp):
            im = Image.open(tp)
            if im.mode != "P":
                raise SystemExit("tiles.png not indexed: " + folder)
            tw, th = im.size[0] // 8, im.size[1] // 8
            px = im.load()
            for ty in range(th):
                for tx in range(tw):
                    self.tiles.append([[px[tx * 8 + x, ty * 8 + y] & 15 for x in range(8)] for y in range(8)])
        self.anim_frames(folder)
        d = open(os.path.join(folder, "metatiles.bin"), "rb").read()
        self.metatiles = [struct.unpack_from("<8H", d, i * 16) for i in range(len(d) // 16)]
        a = open(os.path.join(folder, "metatile_attributes.bin"), "rb").read()
        self.attrs = [struct.unpack_from("<H", a, i * 2)[0] for i in range(len(a) // 2)]
        pd = os.path.join(folder, "palettes")
        self.pals = [load_pal(os.path.join(pd, "%02d.pal" % i)) if os.path.exists(os.path.join(pd, "%02d.pal" % i))
                     else [(0, 0, 0)] * 16 for i in range(16)]

    def anim_frames(self, folder):
        """Animated tiles (water, flowers, the sea) are left blank in tiles.png and copied in by the game
        while it runs: put each animation's first frame where tileset_anims.c puts it."""
        self.anims = []
        ad = os.path.join(folder, "anim")
        if not os.path.isdir(ad):
            return
        for name in sorted(os.listdir(ad)):
            f0 = os.path.join(ad, name, "0.png")
            if os.path.exists(f0):
                self.anims.append((name, f0))


def find_tilesets(root):
    found = {}
    for kind in ("primary", "secondary"):
        base = os.path.join(root, "data/tilesets", kind)
        for f in os.listdir(base):
            found[f.replace("_", "").lower()] = os.path.join(base, f)
    return found


# where tileset_anims.c copies each animation's frames: (tileset folder, anim folder) -> first tile
ANIM_DEST = {}


def load_anim_dests(root):
    """reads src/tileset_anims.c: QueueAnimTiles_<Set>_<Anim> copies into (TILE_OFFSET_4BPP(N))"""
    import re
    src = open(os.path.join(root, "src/tileset_anims.c")).read()
    frames = {}   # gTilesetAnims_General_Flower_Frame0 -> "data/tilesets/primary/general/anim/flower/0"
    for m in re.finditer(r"(gTilesetAnims_\w+?)\[\]\s*=\s*INCGFX_U16\(\"([^\"]+)\"|(gTilesetAnims_\w+?)\[\]\s*=\s*INCBIN_U16\(\"([^\"]+)\.4bpp\"\)", src):
        nm, p = (m.group(1), m.group(2)) if m.group(1) else (m.group(3), m.group(4))
        frames[nm] = p.replace(".4bpp", "").replace(".png", "")
    tables = {}   # gTilesetAnims_General_Flower -> [frame names]
    for m in re.finditer(r"(gTilesetAnims_\w+)\[\]\s*=\s*\{(.*?)\};", src, re.S):
        tables[m.group(1)] = re.findall(r"(gTilesetAnims_\w+)", m.group(2))
    for m in re.finditer(r"AppendTilesetAnimToBuffer\((gTilesetAnims_\w+)\[[^\]]*\],\s*\(u16 \*\)\(BG_VRAM \+ TILE_OFFSET_4BPP\(([^)]+)\)\)", src):
        tab, dest = m.group(1), m.group(2)
        fl = tables.get(tab)
        if not fl or fl[0] not in frames:
            continue
        try:
            n = eval(dest.replace("NUM_TILES_IN_PRIMARY", str(NUM_TILES_PRIMARY)))
        except Exception:
            continue
        ANIM_DEST[frames[fl[0]]] = n


def anim_tiles(root, path):
    p = os.path.join(root, path + ".png")
    if not os.path.exists(p):
        return []
    im = Image.open(p)
    px = im.load()
    out = []
    for ty in range(im.size[1] // 8):
        for tx in range(im.size[0] // 8):
            out.append([[px[tx * 8 + x, ty * 8 + y] & 15 for x in range(8)] for y in range(8)])
    return out


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


def vram(root, prim, sec):
    """the 1024 tiles as the game has them in VRAM, animations' first frames copied in"""
    if not ANIM_DEST:
        load_anim_dests(root)
    v = [None] * 1024
    for k, t in enumerate(prim.tiles[:NUM_TILES_PRIMARY]):
        v[k] = t
    for k, t in enumerate(sec.tiles[:1024 - NUM_TILES_PRIMARY]):
        v[NUM_TILES_PRIMARY + k] = t
    for ts in (prim, sec):
        rel = os.path.relpath(ts.folder, root)
        for name, _ in ts.anims:
            key = os.path.join(rel, "anim", name, "0")
            if key in ANIM_DEST:
                for k, t in enumerate(anim_tiles(root, key)):
                    if 0 <= ANIM_DEST[key] + k < 1024:
                        v[ANIM_DEST[key] + k] = t
    return v


def render_layout(root, lay, found):
    prim = tileset(root, lay["primary_tileset"], found)
    sec = tileset(root, lay["secondary_tileset"], found)
    pals = prim.pals[:NUM_PALS_PRIMARY] + sec.pals[NUM_PALS_PRIMARY:]
    tiles = vram(root, prim, sec)
    W, H = lay["width"], lay["height"]
    blocks = struct.unpack("<%dH" % (W * H), open(os.path.join(root, lay["blockdata_filepath"]), "rb").read()[:W * H * 2])
    img = Image.new("RGB", (W * 16, H * 16), pals[0][0])
    over = Image.new("RGBA", (W * 16, H * 16), (0, 0, 0, 0))
    pi, po = img.load(), over.load()
    cells = []

    def draw(entry, ox, oy, dst, rgba):
        tl = tiles[entry & 0x3FF]
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
            layer = attr >> 12 & 15          # 0 normal (top layer over people), 1 covered, 2 split
            behavior = attr & 0xFF
            ox, oy = bx * 16, by * 16
            for k in range(4):
                draw(mt[k], ox + (k & 1) * 8, oy + (k >> 1) * 8, pi, False)
            for k in range(4):
                draw(mt[4 + k], ox + (k & 1) * 8, oy + (k >> 1) * 8, pi, False)
                if layer in (0, 2):
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
