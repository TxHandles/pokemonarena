#!/usr/bin/env python3
"""Builds the overworld character sheets from pret/pokefirered's object-event graphics.

    frlg_sprites.py <pokefirered dir> <World dir>

World/chars.png      one row per person (16x32 frames): down step/stand/step, up x3, left x3 (right = left mirrored)
World/chars.txt      the row names, in order (the "sprites =" names world.txt uses)
World/actions.png    the player's bike / surf / fishing frames, 32x32 cells
World/fieldobj.png   Cut tree, smashable rock, Strength boulder, item ball (16x16 each)"""
import os, re, sys
from PIL import Image

root, world = sys.argv[1], sys.argv[2]
SRC = os.path.join(root, "src/data/object_events")

ptrs = dict(re.findall(r"\[(OBJ_EVENT_GFX_\w+)\]\s*=\s*&(gObjectEventGraphicsInfo_\w+)", open(os.path.join(SRC, "object_event_graphics_info_pointers.h")).read()))
info = {}
for m in re.finditer(r"(gObjectEventGraphicsInfo_\w+) = \{(.*?)\};", open(os.path.join(SRC, "object_event_graphics_info.h")).read(), re.S):
    body = m.group(2)
    g = lambda k: (re.search(r"\." + k + r"\s*=\s*([^,]+),", body) or [None, None])[1]
    info[m.group(1)] = {"w": int(g("width")), "h": int(g("height")), "images": g("images"), "inanimate": g("inanimate") == "TRUE"}
pics = {}
for m in re.finditer(r"(sPicTable_\w+)\[\] = \{(.*?)\};", open(os.path.join(SRC, "object_event_pic_tables.h")).read(), re.S):
    frames = re.findall(r"(gObjectEventPic_\w+)", m.group(2))
    pics[m.group(1)] = frames[0] if frames else None
paths = dict(re.findall(r"(gObjectEventPic_\w+)\[\] = INCBIN_U\d+\(\"([^\"]+)\.4bpp\"\)", open(os.path.join(SRC, "object_event_graphics.h")).read()))


def sheet_of(gfx):
    gi = info.get(ptrs.get(gfx, ""), None)
    if not gi or not gi["images"]:
        return None, None
    pic = pics.get(gi["images"].strip())
    p = paths.get(pic or "")
    if not p:
        return None, gi
    return os.path.join(root, p + ".png"), gi


def frames(path, w, h):
    im = Image.open(path)
    im = im.convert("RGBA") if im.mode != "P" else im
    if im.mode == "P":   # colour 0 is see-through
        pal = im.getpalette()
        rgba = Image.new("RGBA", im.size)
        src, dst = im.load(), rgba.load()
        for y in range(im.height):
            for x in range(im.width):
                c = src[x, y]
                dst[x, y] = (0, 0, 0, 0) if c == 0 else (pal[c * 3], pal[c * 3 + 1], pal[c * 3 + 2], 255)
        im = rgba
    n = im.width // w
    return [im.crop((k * w, 0, (k + 1) * w, h)) for k in range(n)]


# the people: everyone the maps use, plus the player, Leaf and the rival under their old names
alias = {"player": "OBJ_EVENT_GFX_RED_NORMAL", "leaf": "OBJ_EVENT_GFX_GREEN_NORMAL", "rival": "OBJ_EVENT_GFX_BLUE",
         "oak": "OBJ_EVENT_GFX_PROF_OAK", "oldman": "OBJ_EVENT_GFX_OLD_MAN_1"}
used = set()
for d in os.listdir(os.path.join(root, "data/maps")):
    p = os.path.join(root, "data/maps", d, "map.json")
    if os.path.exists(p):
        used |= set(re.findall(r'"graphics_id": "(OBJ_EVENT_GFX_\w+)"', open(p).read()))
names, rows = [], []
order = list(alias.items()) + [(g.replace("OBJ_EVENT_GFX_", "").lower(), g) for g in sorted(used)]
seen = set()
for name, gfx in order:
    path, gi = sheet_of(gfx)
    if not path or gi["w"] != 16 or gi["h"] not in (16, 32) or name in seen or "/pokemon/" in path:
        continue
    fr = frames(path, 16, gi["h"])
    if gi["h"] == 16:   # little ones: feet on the bottom of the 32-high cell
        fr2 = []
        for f in fr:
            c = Image.new("RGBA", (16, 32), (0, 0, 0, 0)); c.paste(f, (0, 16)); fr2.append(c)
        fr = fr2
    seen.add(name)
    pick = lambda k: fr[k] if k < len(fr) else fr[min(len(fr) - 1, [0, 1, 2][k % 3] if k >= 3 else k)]
    # FRLG order: 0 down, 1 up, 2 left, 3-4 down steps, 5-6 up steps, 7-8 left steps
    if len(fr) >= 9:
        cols = [fr[3], fr[0], fr[4], fr[5], fr[1], fr[6], fr[7], fr[2], fr[8]]
    else:   # people who never walk: standing frames only
        d, u, l = fr[0], fr[1] if len(fr) > 1 else fr[0], fr[2] if len(fr) > 2 else fr[0]
        cols = [d, d, d, u, u, u, l, l, l]
    row = Image.new("RGBA", (16 * 9, 32), (0, 0, 0, 0))
    for k, c in enumerate(cols):
        row.paste(c, (k * 16, 0))
    names.append(name)
    rows.append(row)
sheet = Image.new("RGBA", (16 * 9, 32 * len(rows)), (0, 0, 0, 0))
for k, r in enumerate(rows):
    sheet.paste(r, (0, k * 32))
sheet.save(os.path.join(world, "chars.png"))
open(os.path.join(world, "chars.txt"), "w").write(" ".join(names) + "\n")

# the player's actions: rows red bike, green bike, red surf, green surf, red fish, green fish (32x32)
act = []
for g in ["RED_BIKE", "GREEN_BIKE", "RED_SURF", "GREEN_SURF", "RED_FISH", "GREEN_FISH"]:
    path, gi = sheet_of("OBJ_EVENT_GFX_" + g)
    act.append(frames(path, gi["w"], gi["h"]))
cw = 32
A = Image.new("RGBA", (cw * max(len(a) for a in act), cw * len(act)), (0, 0, 0, 0))
for r, a in enumerate(act):
    for k, f in enumerate(a):
        A.paste(f, (k * cw + (cw - f.width) // 2, r * cw + (cw - f.height)))
A.save(os.path.join(world, "actions.png"))

# field objects
F = Image.new("RGBA", (16 * 4, 16), (0, 0, 0, 0))
for k, g in enumerate(["CUT_TREE", "ROCK_SMASH_ROCK", "PUSHABLE_BOULDER", "ITEM_BALL"]):
    path, gi = sheet_of("OBJ_EVENT_GFX_" + g)
    F.paste(frames(path, gi["w"], gi["h"])[0], (k * 16, 0))
F.save(os.path.join(world, "fieldobj.png"))
print(len(names), "people:", " ".join(names))
