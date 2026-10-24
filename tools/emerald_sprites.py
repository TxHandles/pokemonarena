#!/usr/bin/env python3
"""Emerald's people on the overworld sheet (run after tools/frlg_sprites.py and tools/hgss_sprites.py).

    emerald_sprites.py <pokeemerald dir> [World dir]

Every person Emerald's maps use (from pret/pokeemerald's object-event graphics), plus Brendan, May,
Professor Birch, the Gym Leaders, the Elite Four and the Frontier Brains (and the Trick House's statues), go after the sheet's other people
as em_* rows of World/chars.png (32x32 cells, the 16x32 people in the middle: down step/stand/step, up x3,
left x3). Running it again replaces the em_* rows."""
import os
import re
import sys

from PIL import Image

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = sys.argv[1]
WORLD = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "World")
SRC = os.path.join(ROOT, "src/data/object_events")

ptrs = dict(re.findall(r"\[(OBJ_EVENT_GFX_\w+)\]\s*=\s*&(gObjectEventGraphicsInfo_\w+)",
                       open(os.path.join(SRC, "object_event_graphics_info_pointers.h")).read()))
info = {}
for m in re.finditer(r"(gObjectEventGraphicsInfo_\w+) = \{(.*?)\};", open(os.path.join(SRC, "object_event_graphics_info.h")).read(), re.S):
    body = m.group(2)
    g = lambda k: (re.search(r"\." + k + r"\s*=\s*([^,]+),", body) or [None, None])[1]
    try:
        info[m.group(1)] = {"w": int(g("width")), "h": int(g("height")), "images": g("images")}
    except (TypeError, ValueError):
        pass
pics = {}
for m in re.finditer(r"(sPicTable_\w+)\[\] = \{(.*?)\};", open(os.path.join(SRC, "object_event_pic_tables.h")).read(), re.S):
    frames = re.findall(r"(gObjectEventPic_\w+)", m.group(2))
    pics[m.group(1)] = frames[0] if frames else None
paths = {}
for m in re.finditer(r"(gObjectEventPic_\w+)\[\] = INC(?:GFX|BIN)_U\d+\(\"([^\"]+?)(?:\.4bpp|\.png)\"", open(os.path.join(SRC, "object_event_graphics.h")).read()):
    paths[m.group(1)] = m.group(2)


def sheet_of(gfx):
    gi = info.get(ptrs.get(gfx, ""))
    if not gi or not gi["images"]:
        return None, None
    p = paths.get(pics.get(gi["images"].strip()) or "")
    return (os.path.join(ROOT, p + ".png") if p else None), gi


def frames(path, w, h):
    im = Image.open(path)
    if im.mode == "P":   # colour 0 is see-through
        pal = im.getpalette()
        rgba = Image.new("RGBA", im.size)
        src, dst = im.load(), rgba.load()
        for y in range(im.height):
            for x in range(im.width):
                c = src[x, y]
                dst[x, y] = (0, 0, 0, 0) if c == 0 else (pal[c * 3], pal[c * 3 + 1], pal[c * 3 + 2], 255)
        im = rgba
    else:
        im = im.convert("RGBA")
    return [im.crop((k * w, 0, (k + 1) * w, h)) for k in range(im.width // w)]


EXTRA = ["BRENDAN_NORMAL", "MAY_NORMAL", "PROF_BIRCH", "ROXANNE", "BRAWLY", "WATTSON", "FLANNERY", "NORMAN", "WINONA", "TATE",
         "LIZA", "JUAN", "SIDNEY", "PHOEBE", "GLACIA", "DRAKE", "WALLACE", "STEVEN", "WALLY", "MAXIE", "ARCHIE", "SCOTT",
         "ANABEL", "TUCKER", "SPENSER", "GRETA", "NOLAND", "LUCY", "BRANDON", "MOM", "RIVAL_BRENDAN_NORMAL", "RIVAL_MAY_NORMAL"]
NAME = {"TRICK_HOUSE_STATUE": "statue", "BRENDAN_NORMAL": "brendan", "MAY_NORMAL": "may", "RIVAL_BRENDAN_NORMAL": "brendan", "RIVAL_MAY_NORMAL": "may"}


def main():
    chars = Image.open(os.path.join(WORLD, "chars.png")).convert("RGBA")
    names = open(os.path.join(WORLD, "chars.txt")).read().split()
    keep = [n for n in names if not n.startswith("em_")]
    rows = [chars.crop((0, i * 32, chars.width, i * 32 + 32)) for i, n in enumerate(names) if not n.startswith("em_")]
    used = set()
    for d in os.listdir(os.path.join(ROOT, "data/maps")):
        p = os.path.join(ROOT, "data/maps", d, "map.json")
        if os.path.exists(p):
            used |= set(re.findall(r'"graphics_id": "OBJ_EVENT_GFX_(\w+)"', open(p).read()))
    order = EXTRA + sorted(used)
    added = []
    for g in order:
        name = "em_" + NAME.get(g, g.lower())
        if name in added:
            continue
        path, gi = sheet_of("OBJ_EVENT_GFX_" + g)
        if not path or not os.path.exists(path) or gi["w"] != 16 or gi["h"] not in (16, 32) or "/pokemon/" in path or ("/people/" not in path and not path.endswith("misc/statue.png")):
            continue
        fr = frames(path, 16, gi["h"])
        if gi["h"] == 16:
            fr2 = []
            for f in fr:
                c = Image.new("RGBA", (16, 32), (0, 0, 0, 0)); c.paste(f, (0, 16)); fr2.append(c)
            fr = fr2
        if len(fr) >= 9:   # 0 down, 1 up, 2 left, 3-4 down steps, 5-6 up steps, 7-8 left steps
            cols = [fr[3], fr[0], fr[4], fr[5], fr[1], fr[6], fr[7], fr[2], fr[8]]
        else:
            d, u, l = fr[0], fr[1] if len(fr) > 1 else fr[0], fr[2] if len(fr) > 2 else fr[0]
            cols = [d, d, d, u, u, u, l, l, l]
        row = Image.new("RGBA", (288, 32), (0, 0, 0, 0))
        for k, c in enumerate(cols):
            row.paste(c, (k * 32 + 8, 0))
        rows.append(row)
        added.append(name)
    out = Image.new("RGBA", (288, 32 * len(rows)), (0, 0, 0, 0))
    for k, r in enumerate(rows):
        out.paste(r, (0, k * 32))
    out.save(os.path.join(WORLD, "chars.png"), optimize=True)
    open(os.path.join(WORLD, "chars.txt"), "w").write(" ".join(keep + added) + "\n")
    print(len(added), "Emerald people:", " ".join(added))


if __name__ == "__main__":
    main()
