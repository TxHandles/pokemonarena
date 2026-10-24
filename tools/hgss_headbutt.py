#!/usr/bin/env python3
"""Johto's Headbutt trees (HeartGold / SoulSilver: pret/pokeheartgold's files/arc/headbutt.json).

Every tree on Johto's walked maps (World/johto.txt) becomes "spot = x y headbutt|<Map> (Headbutt)" on its trunk squares,
and each map's tree Pokemon go into wild.txt as their own area ("pokemon = headbutt, ...": HeartGold's common slots,
the rare ones less often), at the levels of that map's grass (Johto's levels in this game, not HeartGold's).
Headbutt itself: the man in Ilex Forest teaches it ("tutor|Headbutt", World/patches_johto.txt).
Run it again any time: it replaces what it made before.

    python3 tools/hgss_headbutt.py <headbutt.json> <hgss_world.py's cache dir>
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORLD = os.path.join(HERE, "World")
WILD = os.path.join(HERE, "wild.txt")
TABLES = json.load(open(sys.argv[1]))["tables"]
CACHE = sys.argv[2]
BEGIN, END = "# ---- Headbutt trees (made by tools/hgss_headbutt.py)", "# ---- end of the Headbutt trees"
COMMON_RATES = [50, 15, 15, 10, 5, 5]    # HeartGold's six slots
LEVEL_SHIFT = 53                          # (Johto's wild levels here vs HeartGold's, where a map has no grass to go by)
UNLOCK = {"New Bark Town": "Cherrygrove City", "Cherrygrove City": "Cherrygrove City", "Violet City": "Sprout Tower",
          "Azalea Town": "Azalea Town", "Ecruteak City": "Burned Tower", "Route 28": "Mt. Silver", "Route 47": "Mt. Silver",
          "Route 48": "Mt. Silver"}   # (towns and routes with no grass of their own: when their trees open)
NAMES = {"NIDORAN_F": "Nidoran F", "NIDORAN_M": "Nidoran M", "MR_MIME": "Mr. Mime", "FARFETCHD": "Farfetch'd"}


def species(s):
    if isinstance(s, dict):
        s = s.get("gold") or list(s.values())[0]
    s = s.replace("SPECIES_", "")
    return NAMES.get(s, s.replace("_", " ").title())


def main():
    johto = open(os.path.join(WORLD, "johto.txt"), encoding="utf-8").read()
    wild = open(WILD, encoding="utf-8").read()
    if BEGIN in wild:
        wild = wild[:wild.index(BEGIN)].rstrip("\n") + "\n" + wild[wild.index(END) + len(END):].lstrip("\n")
    # the wild areas: their unlock and grass levels
    areas = {}
    for m in re.finditer(r"^\[(.+)\]\n((?:(?!\[).*\n?)*)", wild, re.M):
        body = m.group(2)
        unlock = re.search(r"^unlock = (.+)$", body, re.M)
        lv = [tuple(map(int, x)) for x in re.findall(r"^pokemon = walk, [^,]+, \d+, (\d+)-(\d+)", body, re.M)]
        areas[m.group(1)] = (unlock.group(1) if unlock else "", min(a for a, b in lv) if lv else 0, max(b for a, b in lv) if lv else 0)
    blocks = re.split(r"\n(?=\[map\])", johto)
    out_areas, spots = [], 0
    for t in TABLES:
        if not t["Trees"] and not t["SecretTrees"]:
            continue
        code = t["Map"]
        bi = [k for k, b in enumerate(blocks) if "image = maps/hg_%s.png" % code in b]
        cache = os.path.join(CACHE, "hg_%s.json" % code)
        if not bi or not os.path.exists(cache):
            continue   # (Kanto's trees: FireRed's maps there)
        k = bi[0]
        b = blocks[k]
        name = re.search(r"^name = (.+)$", b, re.M).group(1)
        info = json.load(open(cache))
        gx0, gy0 = info["gx0"], info["gy0"]
        head, rest = b.split("grid:\n", 1)
        grid = rest.split("heights:")[0].strip().split("\n")
        area = name + " (Headbutt)"
        lines = [l for l in head.split("\n") if not re.match(r"spot = \d+ \d+ headbutt\|", l)]
        taken = set()
        for tree in t["Trees"] + t["SecretTrees"]:
            for p in tree:
                x, y = p["x"] - gx0, p["y"] - gy0
                if 0 <= y < len(grid) and 0 <= x < len(grid[0]) and grid[y][x] not in ".gcsD^" and (x, y) not in taken:
                    taken.add((x, y))
                    lines.append("spot = %d %d headbutt|%s" % (x, y, area))
                    spots += 1
        blocks[k] = "\n".join(lines) + "\ngrid:\n" + rest
        # its Pokemon: the common slots, the rare trees' half as often, the secret trees' rarer still
        mapwild = re.search(r"^wild = (.*)$", b, re.M)
        key = mapwild.group(1).strip() if mapwild and mapwild.group(1).strip() else name   # (no wild line: the area named after it)
        unlock, lo, hi = areas.get(key, ("", 0, 0))
        unlock = unlock or UNLOCK.get(name, "Cherrygrove City")
        mons = {}
        for lst, scale in ((t["CommonMons"], 1.0), (t["RareMons"], 0.5), (t["SecretMons"], 0.25)):
            for slot, m in enumerate(lst):
                sp = species(m["species"])
                r = COMMON_RATES[slot] if slot < len(COMMON_RATES) else 5
                a, z = (lo, hi) if lo else (m["minLevel"] + LEVEL_SHIFT, m["maxLevel"] + LEVEL_SHIFT)
                e = mons.setdefault(sp, [0, a, z])
                e[0] += r * scale
        if not mons:
            continue
        out_areas.append("[%s]\nunlock = %s\n" % (area, unlock) +
                         "".join("pokemon = headbutt, %s, %d, %d-%d\n" % (sp, max(1, round(r)), a, z) for sp, (r, a, z) in mons.items()))
    johto = "\n".join(blocks)
    open(os.path.join(WORLD, "johto.txt"), "w", encoding="utf-8").write(johto)
    wild = wild.rstrip("\n") + "\n\n" + BEGIN + "\n\n" + "\n".join(out_areas) + "\n" + END + "\n"
    open(WILD, "w", encoding="utf-8").write(wild)
    print("Headbutt trees: %d squares on %d maps" % (spots, len(out_areas)))


if __name__ == "__main__":
    main()
