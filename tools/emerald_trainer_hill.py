#!/usr/bin/env python3
"""Emerald's Trainer Hill (Route 111): pret/pokeemerald's src/data/battle_frontier/trainer_hill.h and src/trainer_hill.c.

The game builds each floor at run time: the top five rows (the stairs) from the 1F-4F maps, then a 16 x 16 floor of the
mode you chose (Normal, Variety, Unique or Expert) - its metatiles, its walls, its two Trainers. Here every mode's floor is
its own map, "Trainer Hill 1F (Normal)" and so on:

  - expand(root, maps, lays): called by tools/emerald_world.py - swaps the four floor maps for sixteen, each with its
    floor's blocks written in (and the stairs between floors of the same mode)
  - run as a script: prints World/patches_hoenn.txt lines - the counter ("thill": pick a mode, the clock starts), the
    stairs up from the lobby ("thup"), each floor's two Trainers (their Emerald teams, levelled to your party), the stairs
    up shut until both are beaten, the owner on the roof ("thfinish": your time, a prize), the elevator back down

    emerald_trainer_hill.py <pokeemerald> > patch.txt
"""
import copy
import os
import re
import struct
import sys
import tempfile

MODES = ["Normal", "Variety", "Unique", "Expert"]
FLOORS = ["1F", "2F", "3F", "4F"]
MARGIN = 5   # HILL_FLOOR_HEIGHT_MARGIN
NUM_PRIMARY = 512


def floor_blocks(root, mode, n, lay):
    """The floor map's block data: the first five rows of the layout, then the mode's floor (metatiles, collision)."""
    W, H = lay["width"], lay["height"]
    base = list(struct.unpack("<%dH" % (W * H), open(os.path.join(root, lay["blockdata_filepath"]), "rb").read()[:W * H * 2]))
    d = os.path.join(root, "graphics/trainer_hill/maps_%s/floor_%d" % (mode.lower(), n))
    mt = open(os.path.join(d, "metatiles.bin"), "rb").read()
    coll = struct.unpack("<16H", open(os.path.join(d, "collision.bin"), "rb").read()[:32])
    for y in range(16):
        for x in range(16):
            imp = (coll[y] >> (15 - x)) & 1
            base[(y + MARGIN) * W + x] = (imp << 10) | (3 << 12) | (mt[y * 16 + x] + NUM_PRIMARY)
    return base


def expand(root, maps, lays):
    floors = {mid: j for mid, j in maps.items() if re.match(r"TrainerHill_[1-4]F$", j["folder"])}
    if not floors:
        return
    for mid in floors:
        del maps[mid]
    for mode in MODES:
        for mid, j in floors.items():
            n = FLOORS.index(j["folder"][-2:])
            v = copy.deepcopy(j)
            v["id"] = mid + "__" + mode.upper()
            v["folder"] = "%s_%s" % (j["folder"], mode)
            v["_name"] = "Trainer Hill %s (%s)" % (FLOORS[n], mode)
            lay = dict(lays[j["layout"]])
            blocks = floor_blocks(root, mode, n, lay)
            tmp = tempfile.NamedTemporaryFile(suffix=".bin", delete=False)
            tmp.write(struct.pack("<%dH" % len(blocks), *blocks)); tmp.close()
            lay["blockdata_filepath"] = tmp.name
            lay["id"] = v["layout"] = j["layout"] + "__" + mode.upper()
            lays[lay["id"]] = lay
            for wv in v.get("warp_events") or []:   # the stairs: to the floors of the same mode
                if wv["dest_map"] in floors:
                    wv["dest_map"] += "__" + mode.upper()
            maps[v["id"]] = v


# ------------------------------------------------------------------ the Trainers (the patch)
SPECIES = {"MR_MIME": "Mr. Mime", "NIDORAN_F": "Nidoran F", "NIDORAN_M": "Nidoran M", "FARFETCHD": "Farfetch'd", "HO_OH": "Ho-Oh"}
CLASS = {"PKMN_BREEDER_M": "Pokemon Breeder", "PKMN_BREEDER_F": "Pokemon Breeder", "PKMN_RANGER_M": "Pokemon Ranger",
         "PKMN_RANGER_F": "Pokemon Ranger", "POKEFAN_M": "Pokefan", "POKEFAN_F": "Pokefan", "POKEMANIAC": "Poke Maniac",
         "SR_AND_JR": "Sr. and Jr.", "SIS_AND_BRO": "Sis and Bro", "OLD_COUPLE": "Old Couple", "YOUNG_COUPLE": "Young Couple"}
DIRS = {1: "down", 2: "up", 3: "left", 4: "right"}   # DIR_SOUTH, DIR_NORTH, DIR_WEST, DIR_EAST


def species(s):
    return SPECIES.get(s, "-".join(p.capitalize() for p in s.split("_")))


def klass(c):
    if c in CLASS:
        return CLASS[c]
    c = re.sub(r"_(M|F)$", "", c)
    return " ".join(p.capitalize() for p in c.split("_"))


def gfx_table(root):
    t = open(os.path.join(root, "src/battle_tower.c")).read()
    def arr(name):
        body = t.split(name)[1].split("};")[0]
        return re.findall(r"(?:FACILITY_CLASS_|OBJ_EVENT_GFX_)(\w+)", body)
    out = dict(zip(arr("gTowerMaleFacilityClasses[30] ="), arr("gTowerMaleTrainerGfxIds[30] =")))
    out.update(zip(arr("gTowerFemaleFacilityClasses[20] ="), arr("gTowerFemaleTrainerGfxIds[20] =")))
    return out


_T = []
def ec_words(ws):
    if _T:
        return _T[0].words(re.sub(r"EC_(?:MOVE2?|POKEMON2?|POKEMON_NATIONAL)\((\w+)\)", r"EC_WORD_\1", ws))
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    old = sys.argv
    sys.argv = [old[0], os.path.join(os.path.dirname(os.path.abspath(old[1])), "pokefirered")]   # (its sets file is read on import)
    import importlib
    try:
        T = importlib.import_module("frlg_trainer_tower")
    except Exception:
        T = None
    sys.argv = old
    if T:
        _T.append(T)
    ws = re.sub(r"EC_(?:MOVE2?|POKEMON2?|POKEMON_NATIONAL)\((\w+)\)", r"EC_WORD_\1", ws)
    return T.words(ws) if T else ""


def our_sprites():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    t = open(os.path.join(here, "World/hoenn.txt")).read()
    return set(re.findall(r"\b(em_\w+)\b", t))


def main():
    root = sys.argv[1]
    src = open(os.path.join(root, "src/data/battle_frontier/trainer_hill.h")).read()
    gfx = gfx_table(root)
    have = our_sprites()
    L = ["", "# ------------------------------------------------------------------ the Trainer Hill (tools/emerald_trainer_hill.py)",
         "[patch Trainer Hill Entrance]", "drop npc 11 6", "npc = 11 6 em_teala down 0 thill",
         "door = 9 1 thup|2|1", "[patch Trainer Hill Roof]", "drop npc 12 7", "npc = 12 7 em_gentleman down 0 thfinish",
         "door = 9 5 msg|(The stairs down are closed. Please take the elevator.)",
         "[patch Trainer Hill Elevator]", "drop npc 0 6",
         "npc = 0 6 em_teala right 0 warp|Trainer Hill Entrance|9|2|down"]
    for mode in MODES:
        body = src.split("sFloors_%s[] =" % mode)[1].split("\n};")[0]
        fl = body.split(".trainerNum1")[1:]
        for n, f in enumerate(fl[:4]):
            m = re.search(r"\.trainerCoords = \{COORDS_XY\((\d+),\s*(\d+)\), COORDS_XY\((\d+),\s*(\d+)\)\}", f)
            coords = [(int(m.group(1)), int(m.group(2)) + MARGIN), (int(m.group(3)), int(m.group(4)) + MARGIN)]
            m = re.search(r"\.trainerDirections = TRAINER_DIRS\(DIR_(\w+), DIR_(\w+)\)", f)
            dn = {"SOUTH": 1, "NORTH": 2, "WEST": 3, "EAST": 4}
            dirs = [DIRS[dn[m.group(2)]], DIRS[dn[m.group(1)]]]   # (trainer 1 is the low four bits: the second)
            m = re.search(r"\.trainerRanges = TRAINER_RANGE\((\d+), (\d+)\)", f)
            ranges = [int(m.group(2)), int(m.group(1))]
            trs = f.split(".trainers =")[1].split(".map =")[0].split(".name = ")[1:]
            name = "Trainer Hill %s (%s)" % (FLOORS[n], mode)
            L.append("[patch %s]" % name)
            wins = []
            for k, t in enumerate(trs[:2]):
                nm = re.match(r'_\("([^"]*)"\)', t).group(1).capitalize()
                fc = re.search(r"\.facilityClass = FACILITY_CLASS_(\w+)", t).group(1)
                mons = re.findall(r"\.species = SPECIES_(\w+)", t)
                team = ", ".join("%s 50" % species(s) for s in mons[3 * k:3 * k + 3])
                before = ec_words(re.search(r"\.speechBefore = \{([^}]*)\}", t).group(1)).replace("|", "-").replace("/", "-")
                lose = ec_words(re.search(r"\.speechLose = \{([^}]*)\}", t).group(1)).replace("|", "-").replace("/", "-")
                spr = "em_" + gfx.get(fc, "BOY_1").lower()
                if spr not in have:
                    spr = "em_boy_1"
                flag = "th:%s:%s:%d" % (FLOORS[n], mode.lower(), k + 1)
                wins.append(flag)
                x, y = coords[k]
                L += ["npc = %d %d %s %s %d fight|%s %s|%s|%s|%s|%s" % (x, y, spr, dirs[k], ranges[k], klass(fc), nm, team, before or "...", lose or "...", flag),
                      "fightopts = scale"]
            L.append("block = 12 2 %s|The stairs up open once you've beaten both of this floor's Trainers!" % "&".join(wins))
    print("\n".join(L))


if __name__ == "__main__":
    main()
