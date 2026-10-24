#!/usr/bin/env python3
"""HeartGold / SoulSilver's Apricorn trees (pret/pokeheartgold: the std_apricorn_tree objects in
files/fielddata/eventdata/zone_event, their colours in src/apricorn_tree_sys.c). Prints World/patches_johto.txt
lines that stand them on our maps: "npc = x y apricorn:Color down 0 apricorn|tree|Color" (one Apricorn a day;
Kurt in Azalea turns them into Poke Balls).

    hgss_apricorns.py <pokeheartgold dir> [World/johto.txt]    > patch lines"""
import json, os, re, sys

HG = sys.argv[1]
WORLD = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "World", "johto.txt")
COLOR = {"RED": "Red", "YLW": "Yellow", "BLU": "Blue", "GRN": "Green", "PNK": "Pink", "WHT": "White", "BLK": "Black"}
src = open(os.path.join(HG, "src/apricorn_tree_sys.c")).read()
colors = [COLOR[c] for c in re.findall(r"APRICORN_(\w+),", src.split("sTreeApricorns")[1].split("};")[0])]
maps = {}
for b in open(WORLD).read().split("[map]"):
    m = re.search(r"^name = (.+)$", b, re.M)
    img = re.search(r"^image = maps/hg_(\w+)\.png$", b, re.M)
    pos = re.search(r"^pos = (\d+) (\d+)$", b, re.M)
    if m and img and pos:
        maps[img.group(1)] = (m.group(1), int(pos.group(1)), int(pos.group(2)))
out = ["# ------------------------------------------------------------------ Apricorn trees (tools/hgss_apricorns.py)"]
d = os.path.join(HG, "files/fielddata/eventdata/zone_event")
for f in sorted(os.listdir(d)):
    code = f.split("_", 1)[1].rsplit(".", 1)[0] if "_" in f else ""
    try:
        j = json.load(open(os.path.join(d, f)))
    except ValueError:
        continue
    for o in j.get("objects", []):
        if o.get("scriptId") != "std_apricorn_tree" or code not in maps:
            continue
        name, px, py = maps[code]
        t = o["param0"]
        out.append("[patch %s]" % name)
        out.append("npc = %d %d apricorn:%s down 0 apricorn|%d|%s" % (o["x"] - px, o["z"] - py, colors[t], t, colors[t]))
print("\n".join(out))
