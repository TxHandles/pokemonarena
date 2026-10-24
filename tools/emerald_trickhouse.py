#!/usr/bin/env python3
"""The Trick House on Route 110 (pret/pokeemerald), on top of what tools/emerald_world.py makes of its rooms.

Prints World/patches_hoenn.txt lines (and draws the pictures they use):
  - the Trick Master's flow: the entrance's back door takes you to your puzzle ("trickgo": puzzle 2 once you have the
    Dynamo Badge ... as Emerald), each puzzle's scroll ("trickscroll|N") unlocks its door, the Trick Master at the end
    gives that puzzle's prize ("trickreward")
  - puzzle 2: the four buttons, each covering a hole (World/maps/em_Route110_TrickHousePuzzle2_on.png: the second
    picture, "swap" squares from it, "gate"s on the holes)
  - puzzle 3: the red and blue doors that swap over at every button ("alt" picture, gates both ways)
  - puzzle 5: the Mechadolls' quizzes ("quiz3": Emerald's questions; a wrong answer and you're back at the start)
  - puzzle 6 and Fortree Gym: the rotating gates ("rgate", World/rgates/*.png)
(Puzzle 1's Cut trees, 4's boulders, 7's warp panels and 8's slippery floor come from emerald_world.py itself.)

    emerald_trickhouse.py <pokeemerald> > patch.txt
"""
import json
import os
import re
import struct
import sys
import tempfile

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emerald_render as R  # noqa: E402

ROOT = sys.argv[1]
WORLD = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "World")
LABELS = dict((k, int(v, 16)) for k, v in re.findall(r"#define (METATILE_\w+)\s+(0x[0-9A-Fa-f]+)", open(os.path.join(ROOT, "include/constants/metatile_labels.h")).read()))
FOUND = R.find_tilesets(ROOT)
LAYOUTS = {l["id"]: l for l in json.load(open(os.path.join(ROOT, "data/layouts/layouts.json")))["layouts"] if "width" in l}
SHAPES = {"GATE_SHAPE_L1": 0, "GATE_SHAPE_L2": 1, "GATE_SHAPE_L3": 2, "GATE_SHAPE_L4": 3, "GATE_SHAPE_T1": 4, "GATE_SHAPE_T2": 5, "GATE_SHAPE_T3": 6, "GATE_SHAPE_T4": 7}
ORIENT = {"GATE_ORIENTATION_0": 0, "GATE_ORIENTATION_90": 1, "GATE_ORIENTATION_180": 2, "GATE_ORIENTATION_270": 3}
P = "Route 110 Trick House Puzzle %d"


def script(n):
    return open(os.path.join(ROOT, "data/maps/Route110_TrickHousePuzzle%d/scripts.inc" % n)).read()


def mapjson(name):
    return json.load(open(os.path.join(ROOT, "data/maps", name, "map.json")))


def sets(src, label):
    body = src.split(label + "::")[1].split("return")[0]
    return [(int(x), int(y), LABELS[m], c == "TRUE") for x, y, m, c in re.findall(r"setmetatile\s+(\d+),\s*(\d+),\s*(\w+),\s*(TRUE|FALSE)", body)]


def layout(n):
    lay = dict(LAYOUTS["LAYOUT_ROUTE110_TRICK_HOUSE_PUZZLE%d" % n])
    W, H = lay["width"], lay["height"]
    raw = open(os.path.join(ROOT, lay["blockdata_filepath"]), "rb").read()
    return lay, W, H, list(struct.unpack("<%dH" % (W * H), raw[:W * H * 2]))


def apply(blocks, W, changes):
    b = list(blocks)
    for x, y, mt, solid in changes:
        b[y * W + x] = (b[y * W + x] & 0xF000) | ((3 if solid else 0) << 10) | mt
    return b


def render(lay, W, H, blocks, out):
    tmp = tempfile.NamedTemporaryFile(suffix=".bin", delete=False)
    tmp.write(struct.pack("<%dH" % (W * H), *blocks)); tmp.close()
    l2 = dict(lay); l2["blockdata_filepath"] = tmp.name
    img, over, _ = R.render_layout(ROOT, l2, FOUND)
    os.unlink(tmp.name)
    img.save(os.path.join(WORLD, "maps", out), optimize=True)


def solid(v):
    return (v >> 10) & 3 != 0


def main():
    L = ["", "# ------------------------------------------------------------------ the Trick House (tools/emerald_trickhouse.py)",
         "[patch Route 110 Trick House Entrance]", "drop door 5 2", "door = 5 2 trickgo",
         "drop npc 6 2", "npc = 6 2 em_man_1 left 0 talk|Trick Master: I am the Trick Master! Hahaha!|Behind me is my Trick House - a new trick for every Badge you earn!|Find the scroll in each room, open the door with its secret code, and I'll be waiting with a reward!",
         "[patch Route 110 Trick House End]", "drop door 10 1", "drop npc 4 5", "npc = 4 5 em_man_1 right 0 trickreward"]
    for n in range(1, 9):   # every puzzle: its scroll, the locked door at the top
        j = mapjson("Route110_TrickHousePuzzle%d" % n)
        L.append("[patch %s]" % (P % n))
        for b in j.get("bg_events", []):
            if "Scroll" in (b.get("script") or ""):
                L += ["drop sign %d %d" % (b["x"], b["y"]), "spot = %d %d trickscroll|%d" % (b["x"], b["y"], n)]
        L += ["drop door 13 1", "door = 13 1 warp|Route 110 Trick House End|10|2|down"]
        L.append("block = 13 1 trick:scroll%d|The door is locked. It needs a secret code...|(The Trick Master hides one on a scroll somewhere in this room.)" % n)

    # ---- puzzle 2: four buttons, each covering a hole
    src = script(2)
    lay, W, H, base = layout(2)
    on = list(base)
    L.append("[patch %s]" % (P % 2))
    L.append("img2 = maps/em_Route110_TrickHousePuzzle2_on.png")
    for b, ev in enumerate([c for c in mapjson("Route110_TrickHousePuzzle2")["coord_events"] if "Button" in c["script"]], 1):
        k = int(re.search(r"Button(\d)", ev["script"]).group(1))
        flag = "thp:2:%d" % k
        L.append("step = %d %d press|%s" % (ev["x"], ev["y"], flag))
        for x, y, mt, sol in sets(src, "Route110_TrickHousePuzzle2_EventScript_PressButton%d" % k):
            on[y * W + x] = (on[y * W + x] & 0xF000) | ((3 if sol else 0) << 10) | mt
            L.append("swap = %d %d %s" % (x, y, flag))
            if solid(base[y * W + x]) and not sol:
                L += ["tile %d %d ." % (x, y), "gate = %d %d %s" % (x, y, flag)]
    render(lay, W, H, on, "em_Route110_TrickHousePuzzle2_on.png")

    # ---- puzzle 3: every button swaps the red doors and the blue ones over
    src = script(3)
    lay, W, H, blk = layout(3)
    s0 = apply(blk, W, sets(src, "Route110_TrickHousePuzzle3_EventScript_SetDoorsState0"))
    s1 = apply(blk, W, sets(src, "Route110_TrickHousePuzzle3_EventScript_SetDoorsState1"))
    render(lay, W, H, s0, "em_Route110_TrickHousePuzzle3_s0.png")
    render(lay, W, H, s1, "em_Route110_TrickHousePuzzle3_alt.png")
    L += ["[patch %s]" % (P % 3), "set image = maps/em_Route110_TrickHousePuzzle3_s0.png", "alt = thp:3:doors|maps/em_Route110_TrickHousePuzzle3_alt.png|"]
    for ev in mapjson("Route110_TrickHousePuzzle3")["coord_events"]:
        if "Button" in ev["script"]:
            L.append("step = %d %d press|thp:3:doors|toggle" % (ev["x"], ev["y"]))
    for k in range(W * H):
        a, b = solid(s0[k]), solid(s1[k])
        if a == b:
            if s0[k] != blk[k] and solid(blk[k]) != a:   # (as the room starts, not as its blocks are stored)
                L.append("tile %d %d %s" % (k % W, k // W, "#" if a else "."))
            continue
        x, y = k % W, k // W
        L.append("tile %d %d ." % (x, y))
        L.append("gate = %d %d %s" % (x, y, "thp:3:doors" if a else "!thp:3:doors"))

    # ---- puzzle 5: the Mechadolls' quizzes
    src = script(5)
    names = dict(re.findall(r'(gTrickHouse_Mechadoll_\w+)::\s*\.string "([^"]*)\$"', open(os.path.join(ROOT, "data/text/trick_house_mechadolls.inc")).read()))
    menu = open(os.path.join(ROOT, "src/data/script_menu.h")).read()

    def text(label):
        body = src.split(label + ":")[1]
        t = ""
        for l in re.findall(r'\.string "([^"]*)"', body)[:12]:
            t += l
            if l.endswith("$"):
                break
        t = t.rstrip("$").replace("\\p", "/").replace("\\n", " ").replace("\\l", " ")
        t = t.replace("POKéMON", "Pokemon").replace("POKé", "Poke").replace("é", "e")
        return re.sub(r"\b([A-Z]{2,})\b", lambda m: m.group(1).capitalize() if m.group(1) not in ("HP", "PP", "TM") else m.group(1), t)

    L.append("[patch %s]" % (P % 5))
    for d in range(1, 6):
        qs = []
        for q in range(1, 4):
            body = src.split("Route110_TrickHousePuzzle5_EventScript_Mechadoll%dQuiz%d::" % (d, q))[1].split("end")[0]
            mid = re.search(r"multichoice \d+, \d+, (\w+)", body).group(1)
            right = int(re.search(r"case (\d+), Route110_TrickHousePuzzle5_EventScript_CorrectAnswer", body).group(1))
            lst = re.search(r"\[%s\]\s*= MULTICHOICE\((\w+)\)" % mid, menu).group(1)
            opts = re.findall(r"\{(gTrickHouse_Mechadoll_\w+)\}", menu.split("MenuAction %s[]" % lst)[1].split("};")[0])
            choice = [names[o].replace("POKé", "POKE").title().replace("Poke", "Poke") for o in opts]
            qs.append("^".join([str(right), text("Route110_TrickHousePuzzle5_Text_Mechadoll%dQuiz%d" % (d, q))] + choice))
        for ev in mapjson("Route110_TrickHousePuzzle5")["coord_events"]:
            if re.search(r"Mechadoll%dTrigger" % d, ev["script"]):
                L.append("step = %d %d quiz3|thp:5:%d|0,21|%s" % (ev["x"], ev["y"], d, "~".join(qs)))
                L.append("unless = thp:5:%d" % d)
    # (the Mechadolls themselves: what they say once you've passed)
    for o in mapjson("Route110_TrickHousePuzzle5")["object_events"]:
        L += ["drop npc %d %d" % (o["x"], o["y"]), "npc = %d %d em_man_1 left 0 talk|Mechadoll: Beep. Correct answers only. Go through, please." % (o["x"], o["y"])]

    # ---- the rotating gates: puzzle 6 and Fortree Gym
    rg = open(os.path.join(ROOT, "src/rotating_gate.c")).read()
    for cfg, mapname in (("sRotatingGate_TrickHousePuzzleConfig", P % 6), ("sRotatingGate_FortreePuzzleConfig", "Fortree Gym")):
        body = rg.split(cfg + "[] =")[1].split("};")[0]
        L.append("[patch %s]" % mapname)
        for x, y, sh, orient in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(GATE_SHAPE_\w+),\s*(GATE_ORIENTATION_\w+)\}", body):
            L.append("rgate = %s %s %d %d" % (x, y, SHAPES[sh], ORIENT[orient]))
    os.makedirs(os.path.join(WORLD, "rgates"), exist_ok=True)
    for nm in ("l1", "l2", "l3", "l4", "t1", "t2", "t3", "t4"):
        im = Image.open(os.path.join(ROOT, "graphics/rotating_gates/%s.png" % nm))
        pal = im.getpalette()
        rgba = Image.new("RGBA", im.size)
        for yy in range(im.height):
            for xx in range(im.width):
                c = im.getpixel((xx, yy))
                rgba.putpixel((xx, yy), (0, 0, 0, 0) if c == 0 else tuple(pal[c * 3:c * 3 + 3]) + (255,))
        rgba.save(os.path.join(WORLD, "rgates", nm + ".png"), optimize=True)
    print("\n".join(L))


if __name__ == "__main__":
    main()
