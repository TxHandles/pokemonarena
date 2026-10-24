#!/usr/bin/env python3
"""The move tutors of FireRed / LeafGreen and Emerald, the Move Deleters and the Move Relearners.

  - World/tutors.txt: who can learn each tutor move ("Move = Species, ..."), from pret/pokeemerald's
    src/data/pokemon/tutor_learnsets.h (FireRed's tables are the same 15 moves), plus the starters' ultimate moves
    (Cape Brink). "covers = ..." lists the species the tables know about: a newer Pokemon (Gen 4 on) can learn a
    tutor move if it could learn it some other way (level up or TM) - see TutorCanLearn in overworld.cpp.
  - prints patch lines (for World/patches.txt, patches_hoenn.txt and patches_johto.txt) turning the tutors' NPCs
    (found in the decomps' map.json object events) into "tutor|Move|what they say", the Deleters into "deleter",
    the Relearners into "relearner".

    move_tutors.py <pokeemerald> <pokefirered> kanto|hoenn|johto > patch.txt
"""
import json
import os
import re
import sys

EM, FR, REGION = sys.argv[1], sys.argv[2], sys.argv[3]
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORLD = os.path.join(HERE, "World")

MOVE_NAME = {"SOFT_BOILED": "Soft-Boiled", "DOUBLE_EDGE": "Double-Edge", "MUD_SLAP": "Mud-Slap", "DYNAMIC_PUNCH": "Dynamic Punch",
             "THUNDER_PUNCH": "Thunder Punch", "PSYCH_UP": "Psych Up"}
def move_name(c):
    return MOVE_NAME.get(c, " ".join(w.capitalize() for w in c.split("_")))

ULTIMATE = {"Frenzy Plant": ["Venusaur", "Meganium", "Sceptile", "Torterra", "Serperior", "Chesnaught", "Decidueye", "Rillaboom", "Meowscarada"],
            "Blast Burn": ["Charizard", "Typhlosion", "Blaziken", "Infernape", "Emboar", "Delphox", "Incineroar", "Cinderace", "Skeledirge"],
            "Hydro Cannon": ["Blastoise", "Feraligatr", "Swampert", "Empoleon", "Samurott", "Greninja", "Primarina", "Inteleon", "Quaquaval"]}


def our_species():
    by_dex = {}
    for d in os.listdir(os.path.join(HERE, "Pokemon")):
        p = os.path.join(HERE, "Pokemon", d, "pokemon.txt")
        if not os.path.exists(p):
            continue
        t = open(p, encoding="utf-8", errors="replace").read()
        m = re.search(r"^name\s*=\s*(.+?)\s*$", t, re.M); n = re.search(r"^dex\s*=\s*(\d+)", t, re.M)
        if m and n:   # (the plain form, not a Mega or a regional one)
            nm = m.group(1).split("#")[0].strip()
            plain = not re.match(r"(Mega|Alolan|Galarian|Hisuian|Paldean|Primal|Gigantamax) ", nm)
            old = by_dex.get(int(n.group(1)))
            if old is None or (plain and (re.match(r"(Mega|Alolan|Galarian|Hisuian|Paldean|Primal|Gigantamax) ", old) or len(nm) < len(old))):
                by_dex[int(n.group(1))] = nm
    return by_dex


def tutors_txt():
    dex = {}
    k = 0
    for line in open(os.path.join(EM, "include/constants/pokedex.h")):
        m = re.match(r"\s*NATIONAL_DEX_(\w+)", line)
        if m:
            dex[m.group(1)] = k; k += 1
        if "NATIONAL_DEX_COUNT" in line or "HOENN_DEX_NONE" in line:
            break
    names = our_species()
    src = open(os.path.join(EM, "src/data/pokemon/tutor_learnsets.h")).read()
    moves = re.findall(r"\[TUTOR_MOVE_(\w+)\]\s*=\s*MOVE_\w+", src)
    can = {move_name(m): [] for m in moves}
    covers = []
    for sp, body in re.findall(r"\[SPECIES_(\w+)\]\s*=\s*\(([^;]*?)\)\s*,", src, re.S):
        if sp == "NONE" or sp.startswith("OLD_UNOWN") or sp not in dex or dex[sp] not in names:
            continue
        n = names[dex[sp]]
        if n not in covers:
            covers.append(n)
        for m in re.findall(r"TUTOR\(MOVE_(\w+)\)", body):
            if n not in can[move_name(m)]:
                can[move_name(m)].append(n)
    have = set(names.values())
    L = ["# Who the move tutors can teach (made by tools/move_tutors.py from pret/pokeemerald's tutor tables).",
         "# A Pokemon not in \"covers\" (newer than Emerald) can learn a tutor move if it could learn it by level or TM.",
         "covers = " + ", ".join(covers)]
    for mv, sps in can.items():
        L.append("%s = %s" % (mv, ", ".join(sps)))
    for mv, sps in ULTIMATE.items():
        L.append("%s = %s" % (mv, ", ".join(s for s in sps if s in have)))
    open(os.path.join(WORLD, "tutors.txt"), "w").write("\n".join(L) + "\n")


# ------------------------------------------------------------------ the decomps' text
TEXT = {}
def load_texts(root):
    for base, _, fs in os.walk(os.path.join(root, "data")):
        for f in fs:
            if not f.endswith(".inc"):
                continue
            cur = None
            for line in open(os.path.join(base, f), encoding="utf-8", errors="replace"):
                m = re.match(r"^(\w+)::?\s*$", line)
                if m:
                    cur = m.group(1); TEXT.setdefault(cur, ""); continue
                m = re.match(r'\s*\.string\s+"(.*)"', line)
                if m and cur:
                    TEXT[cur] += m.group(1)


def clean(s):
    s = s.replace("{PLAYER}", "@P@")
    s = re.sub(r"\{[A-Z_0-9 ]+\}", "", s)
    s = s.replace("POKéMON", "Pokemon").replace("POKé", "Poke").replace("é", "e").replace("…", "...")
    s = re.sub(r"\b[A-Z][A-Z'\-]+\b", lambda m: m.group(0) if m.group(0) in ("TM", "HM", "PP", "OK") else m.group(0).capitalize(), s)
    return s.replace("@P@", "{PLAYER}").replace("@p@", "{PLAYER}")


def text_of(label):
    raw = TEXT.get(label, "").replace("$", "")
    out = []
    for p in re.split(r"\\p", raw):
        p = re.sub(r"\s+", " ", re.sub(r"\\[nl]", " ", p)).strip()
        if p:
            out.append(clean(p).replace("|", "-"))
    return "|".join(out)


def script_body(root, label):
    for base, _, fs in os.walk(os.path.join(root, "data")):
        for f in fs:
            if f.endswith(".inc"):
                t = open(os.path.join(base, f), encoding="utf-8", errors="replace").read()
                if "\n" + label + "::" in "\n" + t:
                    return ("\n" + t).split("\n" + label + "::")[1].split("::")[0]
    return ""


# ------------------------------------------------------------------ the NPCs
EM_MAPS = {"MossdeepCity": "Mossdeep City", "LavaridgeTown_House": "Lavaridge Town House", "PacifidlogTown_PokemonCenter_1F": "Pacifidlog Town Pokemon Center",
           "FortreeCity_House2": "Fortree City House 2", "LilycoveCity_DepartmentStoreRooftop": "Lilycove City Department Store Rooftop",
           "SlateportCity_PokemonFanClub": "Slateport City Pokemon Fan Club", "LilycoveCity_MoveDeletersHouse": "Lilycove City Move Deleter's House",
           "FallarborTown_MoveRelearnersHouse": "Fallarbor Town Move Relearner's House", "FallarborTown_Mart": "Fallarbor Town Poke Mart",
           "SootopolisCity_PokemonCenter_1F": "Sootopolis City Pokemon Center", "VerdanturfTown_PokemonCenter_1F": "Verdanturf Town Pokemon Center",
           "MauvilleCity": "Mauville City", "BattleFrontier_Lounge7": "Battle Frontier Lounge 7"}
FR_MAPS = {"VictoryRoad_2F": "Victory Road 2F", "CeladonCity": "Celadon City", "FourIsland_House1": "Four Island House 1",
           "CeladonCity_DepartmentStore_3F": "Celadon City Department Store 3F", "FuchsiaCity": "Fuchsia City", "SevenIsland": "Seven Island",
           "SilphCo_2F": "Silph Co 2F", "TwoIsland_CapeBrink_House": "Cape Brink House", "ViridianCity": "Viridian City",
           "PewterCity_Museum_1F": "Pewter City Museum 1F", "MtEmber_Exterior": "Mt. Ember Exterior",
           "CinnabarIsland_PokemonLab_ResearchRoom": "Cinnabar Island Pokemon Lab Research Room", "Route4": "Route 4", "RockTunnel_B1F": "Rock Tunnel B1F",
           "FuchsiaCity_House3": "Fuchsia City House 3", "TwoIsland_House": "Two Island House"}
# Battle Frontier Lounge 7's two tutors (Emerald, for Battle Points there - free here)
LOUNGE7 = {"Left": ["Soft-Boiled", "Seismic Toss", "Dream Eater", "Mega Punch", "Mega Kick", "Body Slam", "Rock Slide", "Counter", "Thunder Wave", "Swords Dance"],
           "Right": ["Defense Curl", "Snore", "Mud-Slap", "Swift", "Icy Wind", "Endure", "Psych Up", "Ice Punch", "Thunder Punch", "Fire Punch"]}


def blocks_of(fn):
    out = {}
    for b in re.split(r"\n(?=\[map\])", open(os.path.join(WORLD, fn)).read()):
        m = re.search(r"^name = (.+)$", b, re.M)
        if m:
            out[m.group(1)] = b
    return out


def npc_line(blocks, mp, x, y):
    b = blocks.get(mp, "")
    for l in b.split("\n"):
        if l.startswith("npc = %d %d " % (x, y)):
            return l
    return None


def main():
    if REGION == "tutors":
        tutors_txt(); return
    out = {}
    def put(mp, x, y, act):
        line = npc_line(BLOCKS, mp, x, y)
        if not line:
            sys.stderr.write("no NPC at %s %d,%d\n" % (mp, x, y)); return
        f = line.split(" ")
        out.setdefault(mp, []).append(("drop npc %d %d" % (x, y), " ".join(f[:7]) + " " + act))
    if REGION == "johto":
        BLOCKS = blocks_of("johto.txt")
        put("Blackthorn City Move Tutor House", 4, 5, "deleter")
        put("Blackthorn City Move Tutor House", 7, 2, "relearner|Ha! I'm the Move Relearner. Let me guess, you want me to teach one of your Pokemon a move?")
    else:
        root, maps = (FR, FR_MAPS) if REGION == "kanto" else (EM, EM_MAPS)
        BLOCKS = blocks_of("world.txt" if REGION == "kanto" else "hoenn.txt")
        load_texts(root)
        for d in sorted(os.listdir(os.path.join(root, "data/maps"))):
            p = os.path.join(root, "data/maps", d, "map.json")
            if d not in maps or not os.path.exists(p):
                continue
            for o in json.load(open(p)).get("object_events", []):
                s = o.get("script", "")
                x, y = int(o["x"]), int(o["y"])
                if "MoveDeleter" in s:
                    put(maps[d], x, y, "deleter")
                elif "MoveRelearner" in s or "MoveManiac" in s:
                    pay = "a Heart Scale" if REGION == "hoenn" else "a Tiny Mushroom or a Big Mushroom"
                    put(maps[d], x, y, "relearner|I'm the Move Relearner. I can make Pokemon remember moves they've forgotten.|(In the old days I'd ask for %s.)" % pay)
                elif "Lounge7" in s and "MoveTutor" in s:
                    side = "Left" if "Left" in s else "Right"
                    put(maps[d], x, y, "tutorpick|%s|Buhahaha! I can teach your Pokemon moves - special and %s ones!" % (",".join(LOUNGE7[side]), "cute" if side == "Left" else "tough"))
                elif "StarterTutor" in s:
                    put(maps[d], x, y, "tutorpick|Frenzy Plant,Blast Burn,Hydro Cannon|I have devoted my life to the ultimate moves of the three kinds - grass, fire and water."
                        + "|I will teach one to a Pokemon worthy of it.|(Only the final forms of the starter Pokemon can learn these.)")
                elif s.endswith("Tutor"):
                    body = script_body(root, s)
                    g = re.match(r"\s*goto (\w+)", body)
                    if g:
                        body = script_body(root, g.group(1))
                    m = re.search(r"TUTOR_MOVE_(\w+)|MOVETUTOR_(\w+)", body)
                    t = re.search(r"msgbox (\w+), MSGBOX_YESNO", body)
                    if not m:
                        sys.stderr.write("no move for %s\n" % s); continue
                    mv = move_name(m.group(1) or m.group(2))
                    words = text_of(t.group(1)) if t else ""
                    put(maps[d], x, y, "tutor|%s|%s" % (mv, words))
    L = ["", "# ------------------------------------------------------------------ move tutors, Move Deleters, Move Relearners (tools/move_tutors.py)"]
    for mp, ls in out.items():
        L.append("[patch %s]" % mp)
        for drop, npc in ls:
            L += [drop, npc]
    print("\n".join(L))


if __name__ == "__main__":
    main()
