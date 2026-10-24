#!/usr/bin/env python3
"""The players' own Bicycle, surfing and fishing poses - FireRed's Red and Leaf, HeartGold's Ethan and Lyra, Emerald's
Brendan and May - in World/actions.png, and Ethan and Lyra walking in World/chars.png (hg_ethan, hg_lyra).

  - FireRed's rows (red bike, green bike, red surf, green surf, red fish, green fish) are already there
    (tools/frlg_sprites.py); this puts the bike rows in walking order and adds, for Ethan, Lyra, Brendan and May in
    turn, a bike row, a surf row and a fishing row.
  - HeartGold's come from pret/pokeheartgold's files/data/mmodel/mmodel (the NSBTX textures CYCLEHERO, SWIMHERO,
    FISHINGHERO... read by the little TEX0 reader below); Emerald's from pret/pokeemerald's
    graphics/object_events/pics/people/{brendan,may}.

Each row, 32x32 cells:
    bike   down step, stand, step / up step, stand, step / left step, stand, step   (right = left mirrored)
    surf   down, up, left
    fish   left x4, up x4, down x4   (the cast, then the rod out)

    player_poses.py <pokeheartgold> <pokeemerald>    (run after tools/frlg_sprites.py and tools/hgss_sprites.py)
"""
import os
import re
import struct
import subprocess
import sys

from PIL import Image

HG, EM = sys.argv[1], sys.argv[2]
WORLD = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "World")
MM = {"hero": 69, "heroine": 70, "cyclehero": 71, "cycleheroine": 72, "swimhero": 73, "swimheroine": 74,
      "fishinghero": 79, "fish_heroine": 80}   # (include/constants/mmodel.h)


def nsbtx_bytes(n):
    p = os.path.join(HG, "files/data/mmodel/mmodel/mmodel_%08d.NSBTX" % n)
    if os.path.exists(p) and os.path.getsize(p) > 0:
        return open(p, "rb").read()
    # (a blobless clone: ask git for the file)
    return subprocess.run(["git", "-C", HG, "show", "HEAD:files/data/mmodel/mmodel/mmodel_%08d.NSBTX" % n],
                          capture_output=True, check=True).stdout


def nsbtx_frames(n):
    """the textures of one NSBTX (a TEX0 block), in their number order, with its first palette"""
    d = nsbtx_bytes(n)
    assert d[:4] == b"BTX0"
    t = struct.unpack_from("<I", d, 0x10)[0]

    def info(off):
        cnt = d[off + 1]
        p = off + 4 + 8 + cnt * 4
        esz = struct.unpack_from("<H", d, p)[0]
        p += 4
        ents = [d[p + i * esz:p + (i + 1) * esz] for i in range(cnt)]
        p += cnt * esz
        names = [d[p + i * 16:p + (i + 1) * 16].split(b"\0")[0].decode() for i in range(cnt)]
        return list(zip(names, ents))
    tex_info, tex_data = struct.unpack_from("<H", d, t + 0xE)[0], struct.unpack_from("<I", d, t + 0x14)[0]
    pal_info, pal_data = struct.unpack_from("<I", d, t + 0x34)[0], struct.unpack_from("<I", d, t + 0x38)[0]
    pal = info(t + pal_info)[0][1]
    poff = struct.unpack_from("<H", pal, 0)[0] << 3

    def colour(i):
        v = struct.unpack_from("<H", d, t + pal_data + poff + i * 2)[0]
        return ((v & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, ((v >> 10) & 31) * 255 // 31, 255)
    out = []
    for name, e in info(t + tex_info):
        v = struct.unpack_from("<I", e, 0)[0]
        off, w, h, fmt, tr = (v & 0xFFFF) << 3, 8 << ((v >> 20) & 7), 8 << ((v >> 23) & 7), (v >> 26) & 7, (v >> 29) & 1
        assert fmt == 3, "only 16-colour textures"
        im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        base = t + tex_data + off
        for y in range(h):
            for x in range(w):
                b = d[base + (y * w + x) // 2]
                i = (b >> 4) if x & 1 else (b & 15)
                if not (tr and i == 0):
                    im.putpixel((x, y), colour(i))
        out.append((int(re.search(r"(\d+)$", name).group(1)), im))
    return [im for _, im in sorted(out)]


def gba_frames(path):
    im = Image.open(path)
    pal = im.getpalette()
    w, h = im.height if im.width >= im.height else im.width, im.height
    rgba = Image.new("RGBA", im.size, (0, 0, 0, 0))
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            i = px[x, y]
            if i:
                rgba.putpixel((x, y), tuple(pal[i * 3:i * 3 + 3]) + (255,))
    return [rgba.crop((k * w, 0, (k + 1) * w, h)) for k in range(im.width // w)]


def cell(f):
    c = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    c.paste(f, ((32 - f.width) // 2, 32 - f.height))
    return c


def hg_rows(who):   # Ethan / Lyra: bike, surf, fish (HeartGold's frames: up 1-4, down 5-8, left 9-12, right 13-16)
    bike = nsbtx_frames(MM["cycle" + who])
    surf = nsbtx_frames(MM["swim" + who])
    fish = nsbtx_frames(MM["fishinghero" if who == "hero" else "fish_heroine"])
    walk = nsbtx_frames(MM[who])
    pick = lambda fr, ks: [fr[k - 1] for k in ks]
    return (pick(bike, [6, 5, 8, 2, 1, 4, 10, 9, 12]), pick(surf, [2, 1, 3]), pick(fish, [9, 10, 11, 12, 1, 2, 3, 4, 5, 6, 7, 8]),
            pick(walk, [6, 5, 8, 2, 1, 4, 10, 9, 12]))


def em_rows(who):   # Brendan / May (the GBA order: 0 down, 1 up, 2 left, then the steps; fishing as FireRed's)
    p = os.path.join(EM, "graphics/object_events/pics/people", who)
    bike = gba_frames(os.path.join(p, "mach_bike.png"))
    surf = gba_frames(os.path.join(p, "surfing.png"))
    fish = gba_frames(os.path.join(p, "fishing.png"))
    return [bike[k] for k in (3, 0, 4, 5, 1, 6, 7, 2, 8)], [surf[k] for k in (0, 2, 4)], fish[:12]   # (surfing: 0 down, 2 up, 4 left)


def main():
    A = Image.open(os.path.join(WORLD, "actions.png")).convert("RGBA")
    rows = [[A.crop((k * 32, r * 32, k * 32 + 32, r * 32 + 32)) for k in range(A.width // 32)] for r in range(6)]
    for r in (0, 1):   # FireRed's bike rows: from the GBA order into walking order (once)
        if A.height == 6 * 32:
            raw = rows[r]
            rows[r] = [raw[k] for k in (3, 0, 4, 5, 1, 6, 7, 2, 8)] + raw[9:]
    walks = {}
    for name, who in (("hg_ethan", "hero"), ("hg_lyra", "heroine")):
        b, s, f, w = hg_rows(who)
        rows += [[cell(x) for x in b], [cell(x) for x in s], [cell(x) for x in f]]
        walks[name] = w
    for who in ("brendan", "may"):
        b, s, f = em_rows(who)
        rows += [[cell(x) for x in b], [cell(x) for x in s], [cell(x) for x in f]]
    W = max(len(r) for r in rows)
    out = Image.new("RGBA", (W * 32, 32 * len(rows)), (0, 0, 0, 0))
    for r, row in enumerate(rows):
        for k, c in enumerate(row):
            out.alpha_composite(c, (k * 32, r * 32))
    out.save(os.path.join(WORLD, "actions.png"))
    # Ethan and Lyra walking: rows on the people sheet (replaced if they're there already)
    names = open(os.path.join(WORLD, "chars.txt")).read().split()
    C = Image.open(os.path.join(WORLD, "chars.png")).convert("RGBA")
    keep = [k for k, n in enumerate(names) if n not in walks]
    C2 = Image.new("RGBA", (C.width, 32 * (len(keep) + len(walks))), (0, 0, 0, 0))
    for i, k in enumerate(keep):
        C2.paste(C.crop((0, k * 32, C.width, k * 32 + 32)), (0, i * 32))
    new_names = [names[k] for k in keep]
    for name, fr in walks.items():
        y = len(new_names) * 32
        for k, f in enumerate(fr):
            C2.alpha_composite(cell(f), (k * 32, y))
        new_names.append(name)
    C2.save(os.path.join(WORLD, "chars.png"))
    open(os.path.join(WORLD, "chars.txt"), "w").write(" ".join(new_names) + "\n")
    print("actions:", len(rows), "rows; people:", len(new_names))


if __name__ == "__main__":
    main()
