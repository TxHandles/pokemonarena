#!/usr/bin/env python3
"""HeartGold / SoulSilver's phone rematches (pret/pokeheartgold src/overlay_26_022598C0.c's sTrainerRematchSets):
each route trainer who trades numbers with you has three stronger teams. Prints World/patches_johto.txt lines that
put them on the trainers in World/johto.txt ("at x y: rematch = team / team / team"); the Pokegear's phone calls
work like Emerald's Match Call (src/overworld.cpp).

    hgss_rematches.py <pokeheartgold dir> [World/johto.txt]    > patch lines"""
import json, os, re, sys

HG = sys.argv[1]
WORLD = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "World", "johto.txt")
ids = {n: int(v) for n, v in re.findall(r"#define (TRAINER_\w+)\s+(\d+)", open(os.path.join(HG, "include/constants/trainers.h")).read())}
T = json.load(open(os.path.join(HG, "files/poketool/trainer/trainers.json")))["trainers"]
src = open(os.path.join(HG, "src/overlay_26_022598C0.c")).read().split("sTrainerRematchSets")[1].split("};")[0]
SPECIAL = {"MR_MIME": "Mr. Mime", "HO_OH": "Ho-Oh", "NIDORAN_F": "Nidoran F", "NIDORAN_M": "Nidoran M", "FARFETCHD": "Farfetch'd", "PORYGON2": "Porygon2"}


def team(tid):
    t = T[ids[tid]]
    out = []
    for p in t["party"]:
        sp = p["species"].replace("SPECIES_", "")
        out.append("%s %d" % (SPECIAL.get(sp, sp.replace("_", " ").title()), p["level"]))
    return ", ".join(out)


world = open(WORLD).read()
lines = ["# ------------------------------------------------------------------ phone rematches (tools/hgss_rematches.py)"]
found = 0
for row in re.findall(r"\{([^}]*)\}", src):
    ts = [x.strip() for x in row.split(",") if x.strip()]
    if len(ts) < 3 or "LEADER" in ts[0]:
        continue
    tiers = [x for x in ts[2:] if x != "TRAINER_NONE" and x in ids]
    if not tiers:
        continue
    base = T[ids[ts[1]]]
    who = base["name"].replace("{TRNAME}", "")
    # our trainer: a route fight with that person's name (and the same first team, give or take levels)
    for b in world.split("[map]"):
        m = re.search(r"^name = (.+)$", b, re.M)
        if not m:
            continue
        for x, y, nm in re.findall(r"^npc = (\d+) (\d+) \S+ \S+ \d+ fight\|([^|]+)\|", b, re.M):
            cls = base["class"].replace("TRAINERCLASS_", "").replace("_GS", "").replace("_M", "").replace("_F", "").replace("_", " ").lower()
            if nm.lower().endswith(" " + who.lower()) and nm.lower().replace("&", "and").startswith(cls.split(" ")[0]):
                lines.append("[patch %s]" % m.group(1))
                lines.append("at %s %s: rematch = %s" % (x, y, " / ".join(team(t) for t in tiers)))
                found += 1
print("\n".join(lines))
print("# %d trainers" % found, file=sys.stderr)
