#!/usr/bin/env python3
"""Emerald's rotating tile puzzles: Mossdeep Gym and Trick House Puzzle 7 (pret/pokeemerald's src/rotating_tile_puzzle.c).

The floors have tracks of coloured arrows (yellow, blue, green, purple, red). Step on a switch of a colour and everyone
standing on that colour's arrows - the Trainers, the stone statues - moves one square the way their arrow points and
turns a quarter to the left. Prints World/patches_hoenn.txt lines: "rtile = x y colour direction" for each arrow
square (from the maps' metatiles) and "step = x y rtile|colour" for each switch (the maps' coord events).

    emerald_rotating.py <pokeemerald> > patch.txt
"""
import json
import os
import re
import struct
import sys

ROOT = sys.argv[1]
LABELS = dict((k, int(v, 16)) for k, v in re.findall(r"#define (METATILE_\w+)\s+(0x[0-9A-Fa-f]+)", open(os.path.join(ROOT, "include/constants/metatile_labels.h")).read()))
PUZZLES = [("MossdeepCity_Gym", "Mossdeep Gym", "METATILE_MossdeepGym_YellowArrow_Right"),
           ("Route110_TrickHousePuzzle7", "Route 110 Trick House Puzzle 7", "METATILE_TrickHousePuzzle_Arrow_YellowOnWhite_Right")]
COLOURS = ["Yellow", "Blue", "Green", "Purple", "Red"]


def main():
    lays = {l["id"]: l for l in json.load(open(os.path.join(ROOT, "data/layouts/layouts.json")))["layouts"] if "width" in l}
    L = ["", "# ------------------------------------------------------------------ the rotating tile puzzles (tools/emerald_rotating.py)"]
    for folder, name, first in PUZZLES:
        j = json.load(open(os.path.join(ROOT, "data/maps", folder, "map.json")))
        lay = lays[j["layout"]]
        W, H = lay["width"], lay["height"]
        blocks = struct.unpack("<%dH" % (W * H), open(os.path.join(ROOT, lay["blockdata_filepath"]), "rb").read()[:W * H * 2])
        start = LABELS[first]
        L.append("[patch %s]" % name)
        for k, b in enumerate(blocks):
            d = (b & 0x3FF) - start
            if 0 <= d and d // 8 < 5 and d % 8 < 4:
                L.append("rtile = %d %d %d %d" % (k % W, k // W, d // 8, d % 8))
        for c in j.get("coord_events") or []:
            m = re.search(r"_EventScript_(\w+?)(?:Floor)?(?:Switch|Button)$", c.get("script", ""))
            if m and m.group(1) in COLOURS:
                L.append("step = %d %d rtile|%d" % (c["x"], c["y"], COLOURS.index(m.group(1))))
    print("\n".join(L))


if __name__ == "__main__":
    main()
