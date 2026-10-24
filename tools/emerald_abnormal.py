#!/usr/bin/env python3
"""Emerald's abnormal weather (pret/pokeemerald's data/scripts/abnormal_weather.inc): after the League, the Weather
Institute reports a drought or a downpour somewhere, and Groudon's Terra Cave or Kyogre's Marine Cave opens there.

Prints World/patches_hoenn.txt lines and draws the pictures they use:
  - each of the 8 drought spots (Routes 114, 115, 116, 118): the cave mouth, shown while "abw:<spot>" is set (the
    route's second picture, World/maps/em_<Route>_abw.png, and "swap" squares), and its door into the Terra Cave
  - each of the 8 downpour spots (Routes 105, 125, 127, 129): the churning sea, a "divespot" down to the Marine Cave
  - the caves' ways back out ("abwback": to the spot you came in by), Marine Cave's surfacing and diving
  - the Weather Institute 2F scientist's report ("abweather", overworld.cpp)

    emerald_abnormal.py <pokeemerald> > patch.txt
"""
import json
import os
import re
import struct
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emerald_render as R  # noqa: E402

ROOT = sys.argv[1]
WORLD = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "World")
LABELS = dict((k, int(v, 16)) for k, v in re.findall(r"#define (METATILE_\w+)\s+(0x[0-9A-Fa-f]+)", open(os.path.join(ROOT, "include/constants/metatile_labels.h")).read()))
SRC = open(os.path.join(ROOT, "data/scripts/abnormal_weather.inc")).read()
FOUND = R.find_tilesets(ROOT)
LAYOUTS = {l["id"]: l for l in json.load(open(os.path.join(ROOT, "data/layouts/layouts.json")))["layouts"] if "width" in l}
SPOTS = [("114n", 114, "North", False), ("114s", 114, "South", False), ("115w", 115, "West", False), ("115e", 115, "East", False),
         ("116n", 116, "North", False), ("116s", 116, "South", False), ("118e", 118, "East", False), ("118w", 118, "West", False),
         ("105n", 105, "North", True), ("105s", 105, "South", True), ("125w", 125, "West", True), ("125e", 125, "East", True),
         ("127n", 127, "North", True), ("127s", 127, "South", True), ("129w", 129, "West", True), ("129e", 129, "East", True)]


def sets(label):
    body = SRC.split(label + "::")[1].split("return")[0]
    return [(int(x), int(y), m, c == "TRUE") for x, y, m, c in re.findall(r"setmetatile\s+(\d+),\s*(\d+),\s*(\w+),\s*(TRUE|FALSE)", body)]


def main():
    L = ["", "# ------------------------------------------------------------------ abnormal weather: Terra Cave and Marine Cave (tools/emerald_abnormal.py)"]
    by_route = {}
    for spot, rt, side, marine in SPOTS:
        by_route.setdefault(rt, []).append((spot, side, marine))
    for rt, spots in by_route.items():
        j = json.load(open(os.path.join(ROOT, "data/maps/Route%d/map.json" % rt)))
        lay = dict(LAYOUTS[j["layout"]])
        W, H = lay["width"], lay["height"]
        blocks = list(struct.unpack("<%dH" % (W * H), open(os.path.join(ROOT, lay["blockdata_filepath"]), "rb").read()[:W * H * 2]))
        L.append("[patch Route %d]" % rt)
        L.append("img2 = maps/em_Route%d_abw.png" % rt)
        for spot, side, marine in spots:
            flag = "abw:" + spot
            ch = sets("AbnormalWeather_EventScript_PlaceTilesRoute%d%s" % (rt, side))
            deep = []
            for x, y, mt, solid in ch:
                blocks[y * W + x] = (blocks[y * W + x] & 0xF000) | ((3 if solid else 0) << 10) | LABELS[mt]
                L.append("swap = %d %d %s" % (x, y, flag))
                if "DeepWater" in mt:
                    deep.append((x, y))
            if marine:
                xs, ys = [x for x, _ in deep], [y for _, y in deep]
                L.append("divespot = %d %d %d %d %s|Underwater Marine Cave|9|7" % (min(xs), min(ys), max(xs), max(ys), flag))
            else:   # the mouth: its bottom half is the way in
                bx, by = [(x, y) for x, y, mt, solid in ch if not solid][0]
                L += ["door = %d %d warp|Terra Cave Entrance|8|17|up" % (bx, by), "if = " + flag]
        tmp = tempfile.NamedTemporaryFile(suffix=".bin", delete=False)
        tmp.write(struct.pack("<%dH" % (W * H), *blocks)); tmp.close()
        l2 = dict(lay); l2["blockdata_filepath"] = tmp.name
        img, over, _ = R.render_layout(ROOT, l2, FOUND)
        os.unlink(tmp.name)
        img.save(os.path.join(WORLD, "maps", "em_Route%d_abw.png" % rt), optimize=True)
    L += ["[patch Terra Cave Entrance]"] + ["door = %d 18 abwback" % x for x in range(7, 11)]
    L += ["[patch Underwater Marine Cave]", "emerge = Marine Cave Entrance|9|17", "door = 9 8 abwback"]
    L += ["[patch Marine Cave Entrance]", "dive = Underwater Marine Cave|9|7"]
    L += ["[patch Weather Institute 2F]", "drop npc 18 6", "npc = 18 6 em_scientist_1 down 0 abweather"]
    print("\n".join(L))


if __name__ == "__main__":
    main()
