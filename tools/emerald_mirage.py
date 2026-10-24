#!/usr/bin/env python3
"""Mirage Island off Route 130 (Emerald: pret/pokeemerald's data/maps/Route130/scripts.inc, LAYOUT_ROUTE130_MIRAGE_ISLAND).

On the days it's there ("mirage", overworld.cpp's HasFlag) Route 130 shows the island: its squares come from a second
picture of the route (World/maps/em_Route130_mirage.png, "img2" and "swap = x y mirage <grid>") and walk like the island,
its grass has Wynaut ("wildat"), and its Liechi Berry tree grows. The watcher in Pacifidlog says whether you can see it.

    emerald_mirage.py <pokeemerald> > patch.txt      (prints World/patches_hoenn.txt lines, draws the picture)
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WORLD = os.path.join(os.path.dirname(HERE), "World")
ROOT = sys.argv[1]
sys.path.insert(0, HERE)
sys.argv = [sys.argv[0], ROOT, WORLD, os.path.join(os.path.dirname(HERE), "story.txt")]
import emerald_render as R  # noqa: E402
import emerald_world as EW  # noqa: E402


def main():
    EW.load_behaviors()
    found = R.find_tilesets(ROOT)
    lays = {l["id"]: l for l in json.load(open(os.path.join(ROOT, "data/layouts/layouts.json")))["layouts"] if "width" in l}
    a, b = lays["LAYOUT_ROUTE130"], lays["LAYOUT_ROUTE130_MIRAGE_ISLAND"]
    W, H = a["width"], a["height"]
    _, _, ca = R.render_layout(ROOT, a, found)
    img, _, cb = R.render_layout(ROOT, b, found)
    img.save(os.path.join(WORLD, "maps", "em_Route130_mirage.png"), optimize=True)
    ga, gb = EW.grid_of(ca, W, H, False, False), EW.grid_of(cb, W, H, True, False)
    L = ["", "# ------------------------------------------------------------------ Mirage Island (tools/emerald_mirage.py)",
         "[patch Route 130]", "img2 = maps/em_Route130_mirage.png"]
    land = []
    for k in range(W * H):
        if ca[k][0] != cb[k][0] or ga[k] != gb[k]:
            L.append("swap = %d %d mirage %s" % (k % W, k // W, gb[k]))
            if gb[k] not in "wWd":
                land.append((k % W, k // W))
    xs, ys = [x for x, _ in land], [y for _, y in land]
    L += ["wildat = %d %d %d %d Mirage Island" % (min(xs), min(ys), max(xs), max(ys)),
          "drop berry 52 9", "berry = 52 9 82 Liechi", "if = mirage",
          "[patch Pacifidlog Town House 5]", "drop npc 9 4", "npc = 9 4 em_expert_m right 0 mirageman"]
    print("\n".join(L))


if __name__ == "__main__":
    main()
