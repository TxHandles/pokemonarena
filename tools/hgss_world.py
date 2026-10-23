#!/usr/bin/env python3
"""Johto, walked: builds World/johto.txt and World/maps/hg_*.png from

  - the HeartGold / SoulSilver location models (The Models Resource, "DS / DSi - Pokemon HeartGold /
    SoulSilver - Locations"), rendered top-down by tools/hgss_render.py, and
  - the pret/pokeheartgold decompilation: where the doors, people, signs and items are (zone_event),
    what they say (scripts + messages) and the trainers' teams.

Everyday rooms (houses, Pokemon Centers, Marts, gates) reuse FireRed's interiors from World/world.txt with
HeartGold's people and lines in them; the special rooms (gyms, Elm's lab, the towers) come later.

    python3 tools/hgss_world.py <pokeheartgold> <models dir with the zips> <cache dir> [--only T20,R29] [--force]
"""
import glob
import json
import os
import re
import sys
import zipfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hgss_render  # noqa: E402
import crystal_rooms  # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORLD = os.path.join(HERE, "World")
args = [a for a in sys.argv[1:] if not a.startswith("--")]
HG, MODELS, CACHE = args[0], args[1], args[2]
FORCE = "--force" in sys.argv
REGRID = "--regrid" in sys.argv
ONLY = None
for a in sys.argv:
    if a.startswith("--only="):
        ONLY = set(a[7:].split(","))
CRYSTAL = os.path.join(os.path.dirname(HG.rstrip("/")), "cr")   # pret/pokecrystal next to pokeheartgold
HB = 16          # the ground's usual height in the models: people there stand where the tile is drawn
PAD = 64         # (tools/hgss_render.py) the room left above the top row of each picture

# ------------------------------------------------------------------ the region: model, map code, our name
# (code: the decomp's map code - T20 New Bark Town, R29 Route 29 ...)
EXTERIORS = [
    ("T20", "New Bark Town", "New Bark Town", "New Bark Town"),
    ("R29", "Route 29", "Route 29", None),
    ("T21", "Cherrygrove City", "Cherrygrove City", "Cherrygrove City"),
    ("R30", "Route 30", "Route 30", None),
    ("R31", "Route 31", "Route 31", None),
    ("T22", "Violet City", "Violet City", "Violet City"),
    ("R32", "Route 32", "Route 32", None),
    ("D24R0101", "Ruins of Alph", "Ruins of Alph", None),
    ("R33", "Route 33", "Route 33", None),
    ("T23", "Azalea Town", "Azalea Town", "Azalea Town"),
    ("R34", "Route 34", "Route 34", None),
    ("T25", "Goldenrod City", "Goldenrod City", "Goldenrod City"),
    ("R35", "Route 35", "Route 35", None),
    ("R36", "Route 36", "Route 36", None),
    ("R37", "Route 37", "Route 37", None),
    ("T27", "Ecruteak City", "Ecruteak City", "Ecruteak City"),
    ("R38", "Route 38", "Route 38", None),
    ("R39", "Route 39", "Route 39", None),
    ("T26", "Olivine City", "Olivine City", "Olivine City"),
    ("W40", "Route 40", "Route 40", None),
    ("W41", "Route 41", "Route 41", None),
    ("T24", "Cianwood City", "Cianwood City", "Cianwood City"),
    ("R42", "Route 42", "Route 42", None),
    ("T28", "Mahogani Town", "Mahogany Town", "Mahogany Town"),
    ("R43", "Route 43", "Route 43", None),
    ("T29", "Lake of Rage", "Lake of Rage", "Lake of Rage"),
    ("R44", "Route 44", "Route 44", None),
    ("T30", "Blackthorn City", "Blackthorn City", "Blackthorn City"),
    ("R45", "Route 45", "Route 45", None),
    ("R46", "Route 46", "Route 46", None),
    ("R26", "Route 26", "Route 26", None),
    ("R27", "Route 27", "Route 27", None),
    ("R47", "Route 47", "Route 47", None),
    ("R48", "Route 48", "Route 48", None),
    ("R28", "Route 28", "Route 28", None),
    ("T31", "Mt. Silver", "Mt. Silver", "Mt. Silver"),
]
# the town you'd be in before each Johto story battle (a save from the Story menu days starts at its Pokemon Center)
STORY_TOWN = [("Cherrygrove City", "Cherrygrove City"), ("Route 30", "Cherrygrove City"), ("Sprout Tower", "Violet City"),
              ("Violet Gym", "Violet City"), ("Union Cave", "Route 32"), ("Slowpoke Well", "Azalea Town"), ("Azalea Gym", "Azalea Town"),
              ("Azalea Town", "Azalea Town"), ("Ilex Forest", "Azalea Town"), ("Route 34", "Goldenrod City"),
              ("Goldenrod Gym", "Goldenrod City"), ("Route 36", "Goldenrod City"), ("Burned Tower", "Ecruteak City"),
              ("Ecruteak Gym", "Ecruteak City"), ("Olivine Lighthouse", "Olivine City"), ("Route 41", "Olivine City"),
              ("Cianwood Gym", "Cianwood City"), ("Olivine Gym", "Olivine City"), ("Lake of Rage", "Mahogany Town"),
              ("Rocket Hideout", "Mahogany Town"), ("Rocket Hideout B3", "Mahogany Town"), ("Mahogany Gym", "Mahogany Town"),
              ("Goldenrod Underground", "Goldenrod City"), ("Radio Tower 3F", "Goldenrod City"), ("Radio Tower 4F", "Goldenrod City"),
              ("Radio Tower 5F", "Goldenrod City"), ("Ecruteak Dance Theater", "Ecruteak City"), ("Ice Path", "Mahogany Town"),
              ("Blackthorn Gym", "Blackthorn City"), ("Dragon's Den", "Blackthorn City"), ("Route 27", "New Bark Town"),
              ("Victory Road", "New Bark Town"), ("Will's Chamber", "New Bark Town"), ("Koga's Chamber", "New Bark Town"),
              ("Bruno's Ring", "New Bark Town"), ("Karen's Chamber", "New Bark Town"), ("Champion's Room", "New Bark Town"),
              ("Mt. Silver", "Mt. Silver")]
# caves and forests: their own 3D models, a floor each (model zip, the floor's .dae, our name, dark)
DUNGEONS = [
    ("D25R0101", "Union Cave", "1F/", "Union Cave 1F", False),
    ("D25R0102", "Union Cave", "B1F/", "Union Cave B1F", False),
    ("D25R0103", "Union Cave", "B2F/", "Union Cave B2F", False),
    ("D36R0101", "Ilex Forest", "Ilex Forest.dae", "Ilex Forest", False),
    ("D22R0101", "National Park", "National Park.dae", "National Park", False),
    ("D38R0101", "Mt. Mortar", "1F_/", "Mt. Mortar 1F", False),
    ("D38R0102", "Mt. Mortar", "1F - Back_/1F/", "Mt. Mortar 1F (Back)", False),
    ("D38R0103", "Mt. Mortar", "1F - Back_/2F/", "Mt. Mortar 2F", False),
    ("D38R0104", "Mt. Mortar", "B1F_/", "Mt. Mortar B1F", False),
    ("D39R0101", "Ice Path", "1F/", "Ice Path 1F", False),
    ("D39R0102", "Ice Path", "B1F/", "Ice Path B1F", False),
    ("D39R0103", "Ice Path", "B2F/", "Ice Path B2F", False),
    ("D39R0104", "Ice Path", "B3F/", "Ice Path B3F", False),
    ("D42R0101", "Dark Cave", "Route 45 Side/", "Dark Cave (Route 45)", True),
    ("D42R0102", "Dark Cave", "Route 31 Side/", "Dark Cave", True),
    ("D44R0101", "Dragon's Den", "1F/", "Dragon's Den", False),
    ("D44R0102", "Dragon's Den", "B1F/", "Dragon's Den B1F", False),
    ("D45R0101", "Tohjo Falls", "Entrance/", "Tohjo Falls", False),
    ("D45R0102", "Tohjo Falls", "Hidden Room/", "Tohjo Falls (Hidden Room)", False),
    ("D50R0101", "Cliff Edge", "Cliff Edge.dae", "Cliff Cave", False),
    ("D43R0101", "Victory Road", "1F/", "Johto Victory Road 1F", False),
    ("D43R0102", "Victory Road", "2F/", "Johto Victory Road 2F", False),
    ("D43R0103", "Victory Road", "3F/", "Johto Victory Road 3F", False),
]
# the special rooms, from Pokemon Crystal: (Crystal map, the HeartGold map its outside door leads to, our name)
SPECIAL = [
    ("ElmsLab", "MAP_NEW_BARK_ELMS_LAB_1F", "Elm's Lab"),
    ("VioletGym", "MAP_VIOLET_GYM", "Violet Gym"),
    ("SproutTower1F", "MAP_SPROUT_TOWER_1F", "Sprout Tower 1F"),
    ("SproutTower2F", None, "Sprout Tower 2F"),
    ("SproutTower3F", None, "Sprout Tower 3F"),
    ("AzaleaGym", "MAP_AZALEA_GYM_ENTRANCE", "Azalea Gym"),
    ("SlowpokeWellB1F", "MAP_SLOWPOKE_WELL_ENTRANCE", "Slowpoke Well B1F"),
    ("SlowpokeWellB2F", None, "Slowpoke Well B2F"),
    ("GoldenrodGym", "MAP_GOLDENROD_GYM", "Goldenrod Gym"),
    ("GoldenrodGameCorner", "MAP_GOLDENROD_GAME_CORNER", "Goldenrod Game Corner"),
    ("RadioTower1F", "MAP_GOLDENROD_RADIO_TOWER_1F", "Radio Tower 1F"),
    ("RadioTower2F", None, "Radio Tower 2F"),
    ("RadioTower3F", None, "Radio Tower 3F"),
    ("RadioTower4F", None, "Radio Tower 4F"),
    ("RadioTower5F", None, "Radio Tower 5F"),
    ("GoldenrodDeptStore1F", "MAP_GOLDENROD_DEPARTMENT_STORE_1F", "Goldenrod Dept. Store 1F"),
    ("GoldenrodDeptStore2F", None, "Goldenrod Dept. Store 2F"),
    ("GoldenrodDeptStore3F", None, "Goldenrod Dept. Store 3F"),
    ("GoldenrodDeptStore4F", None, "Goldenrod Dept. Store 4F"),
    ("GoldenrodDeptStore5F", None, "Goldenrod Dept. Store 5F"),
    ("GoldenrodDeptStore6F", None, "Goldenrod Dept. Store 6F"),
    ("GoldenrodDeptStoreRoof", None, "Goldenrod Dept. Store Roof"),
    ("EcruteakGym", "MAP_ECRUTEAK_GYM", "Ecruteak Gym"),
    ("BurnedTower1F", "MAP_BURNED_TOWER_1F", "Burned Tower 1F"),
    ("BurnedTowerB1F", None, "Burned Tower B1F"),
    ("OlivineGym", "MAP_OLIVINE_GYM", "Olivine Gym"),
    ("OlivineLighthouse1F", "MAP_OLIVINE_LIGHTHOUSE_1F", "Olivine Lighthouse 1F"),
    ("OlivineLighthouse2F", None, "Olivine Lighthouse 2F"),
    ("OlivineLighthouse3F", None, "Olivine Lighthouse 3F"),
    ("OlivineLighthouse4F", None, "Olivine Lighthouse 4F"),
    ("OlivineLighthouse5F", None, "Olivine Lighthouse 5F"),
    ("OlivineLighthouse6F", None, "Olivine Lighthouse 6F"),
    ("CianwoodGym", "MAP_CIANWOOD_GYM", "Cianwood Gym"),
    ("WhirlIslandNW", "MAP_WHIRL_ISLANDS_1F", "Whirl Islands (Northwest)"),
    ("WhirlIslandNE", "MAP_WHIRL_ISLANDS_1F", "Whirl Islands (Northeast)"),
    ("WhirlIslandSW", "MAP_WHIRL_ISLANDS_1F", "Whirl Islands (Southwest)"),
    ("WhirlIslandSE", "MAP_WHIRL_ISLANDS_1F", "Whirl Islands (Southeast)"),
    ("WhirlIslandCave", None, "Whirl Islands Cave"),
    ("WhirlIslandB1F", None, "Whirl Islands B1F"),
    ("WhirlIslandB2F", None, "Whirl Islands B2F"),
    ("WhirlIslandLugiaChamber", None, "Lugia's Chamber"),
    ("MahoganyGym", "MAP_MAHOGANY_GYM_ROOM_1", "Mahogany Gym"),
    ("MahoganyMart1F", "MAP_MAHOGANY_SOUVENIR_SHOP", "Mahogany Souvenir Shop"),
    ("TeamRocketBaseB1F", None, "Team Rocket HQ B1F"),
    ("TeamRocketBaseB2F", None, "Team Rocket HQ B2F"),
    ("TeamRocketBaseB3F", None, "Team Rocket HQ B3F"),
    ("BlackthornGym1F", "MAP_BLACKTHORN_GYM", "Blackthorn Gym"),
    ("BlackthornGym2F", None, "Blackthorn Gym 2F"),
    ("DragonShrine", "MAP_DRAGONS_DEN_SHRINE", "Dragon Shrine"),
    ("RuinsOfAlphResearchCenter", "MAP_RUINS_OF_ALPH_RESEARCH_CENTER", "Ruins of Alph Research Center"),
    ("DanceTheater", "MAP_ECRUTEAK_DANCE_THEATER", "Ecruteak Dance Theater"),
    ("IndigoPlateauPokecenter1F", None, "Indigo Plateau (Johto League)"),
    ("WillsRoom", None, "Will's Chamber"),
    ("KogasRoom", None, "Koga's Chamber"),
    ("BrunosRoom", None, "Bruno's Ring"),
    ("KarensRoom", None, "Karen's Chamber"),
    ("LancesRoom", None, "Lance's Chamber"),
]
SPECIAL_HG = {s[1] for s in SPECIAL if s[1]}
# Crystal's outdoor maps -> ours (HeartGold's codes)
CR_OUT = {"NEW_BARK_TOWN": "T20", "CHERRYGROVE_CITY": "T21", "VIOLET_CITY": "T22", "AZALEA_TOWN": "T23", "CIANWOOD_CITY": "T24",
          "GOLDENROD_CITY": "T25", "OLIVINE_CITY": "T26", "ECRUTEAK_CITY": "T27", "MAHOGANY_TOWN": "T28", "LAKE_OF_RAGE": "T29",
          "BLACKTHORN_CITY": "T30", "ROUTE_40": "W40", "ROUTE_41": "W41", "DRAGONS_DEN_B1F": "D44R0102",
          "RUINS_OF_ALPH_OUTSIDE": "D24R0101"}
for _n in range(26, 49):
    CR_OUT.setdefault("ROUTE_%d" % _n, "R%d" % _n)
# Crystal's people -> our sprites (until HeartGold's own)
CR_SPRITE = {"YOUNGSTER": "youngster", "POKEFAN_M": "fat_man", "LASS": "lass", "FISHER": "fisher", "SUPER_NERD": "poke_maniac",
             "TEACHER": "woman_3", "COOLTRAINER_F": "cooltrainer_f", "ROCKET": "rocket_m", "COOLTRAINER_M": "cooltrainer_m",
             "GRAMPS": "old_man_1", "OFFICER": "policeman", "GENTLEMAN": "gentleman", "CLERK": "clerk", "SAILOR": "sailor",
             "GYM_GUIDE": "gym_guy", "SAGE": "channeler", "POKEFAN_F": "woman_2", "NURSE": "nurse", "BUG_CATCHER": "bug_catcher",
             "TWIN": "little_girl", "GRANNY": "old_woman", "BLACK_BELT": "black_belt", "SCIENTIST": "scientist",
             "RECEPTIONIST": "union_room_receptionist", "FISHING_GURU": "old_man_2", "ROCKER": "rocker", "SWIMMER_GIRL": "swimmer_f_land",
             "BEAUTY": "beauty", "PHARMACIST": "old_man_1", "RIVAL": "rival", "GAMEBOY_KID": "gba_kid", "STANDING_YOUNGSTER": "youngster",
             "ROCKET_GIRL": "rocket_f", "BIKER": "rocker", "SWIMMER_GUY": "swimmer_m_land", "LANCE": "lance", "KIMONO_GIRL": "beauty",
             "MOM": "mom", "KURT": "old_man_2", "ELDER": "old_man_1", "OAK": "prof_oak", "ELM": "scientist", "BILL": "bill",
             "KOGA": "koga", "BRUNO": "bruno", "MISTY": "misty", "BROCK": "brock", "BLAINE": "blaine", "ERIKA": "erika",
             "SABRINA": "sabrina", "SURGE": "lt_surge", "BLUE": "blue", "RED": "player", "DAISY": "daisy", "CAPTAIN": "captain",
             "FALKNER": "hg_falkner", "BUGSY": "hg_bugsy", "WHITNEY": "hg_whitney", "MORTY": "hg_morty", "CHUCK": "hg_chuck",
             "JASMINE": "hg_jasmine", "PRYCE": "hg_pryce", "CLAIR": "hg_clair", "WILL": "hg_will", "KAREN": "hg_karen",
             "AZALEA_ROCKET": "hg_rocket_m", "OLIVINE_RIVAL": "hg_silver", "CHRIS": "player", "COPYCAT": "lass",
             "LINK_RECEPTIONIST": "union_room_receptionist"}
CR_SPRITE.update({"ROCKET": "hg_rocket_m", "ROCKET_GIRL": "hg_rocket_f", "RIVAL": "hg_silver", "LANCE": "hg_lance", "KOGA": "hg_koga",
                  "BRUNO": "hg_bruno", "MISTY": "hg_misty", "BROCK": "hg_brock", "BLAINE": "hg_blaine", "ERIKA": "hg_erika",
                  "SABRINA": "hg_sabrina", "SURGE": "hg_surge", "BLUE": "hg_blue", "RED": "hg_red", "ELM": "hg_elm", "KURT": "hg_kurt",
                  "MOM": "hg_mom", "OFFICER": "hg_officer", "SAGE": "hg_monk", "ELDER": "hg_sage", "KIMONO_GIRL": "hg_kimono_girl",
                  "CAPTAIN": "hg_captain", "JANINE": "hg_janine", "GENTLEMAN": "gentleman"})
CR_MON = {"SLOWPOKE": "Slowpoke", "GROWLITHE": "Growlithe", "VOLTORB": "Voltorb", "SUICUNE": "Suicune", "MOLTRES": "Moltres",
          "TAUROS": "Tauros", "RHYDON": "Rhydon", "RAIKOU": "Raikou", "ENTEI": "Entei", "MACHOP": "Machop", "JIGGLYPUFF": "Jigglypuff",
          "CLEFAIRY": "Clefairy", "ZUBAT": "Zubat", "POLIWAG": "Poliwag", "ODDISH": "Oddish", "LUGIA": "Lugia", "JYNX": "Jynx",
          "HO_OH": "Ho-Oh", "GYARADOS": "Gyarados", "EKANS": "Ekans", "DIGLETT": "Diglett", "BUTTERFREE": "Butterfree",
          "MONSTER": "Ampharos", "DRAGON": "Dragonite", "BIRD": "Pidgeot"}
LEADERS = {"CHAMPION": "Champion's Room", "FALKNER": "Violet Gym", "BUGSY": "Azalea Gym", "WHITNEY": "Goldenrod Gym", "MORTY": "Ecruteak Gym",
           "CHUCK": "Cianwood Gym", "JASMINE": "Olivine Gym", "PRYCE": "Mahogany Gym", "CLAIR": "Blackthorn Gym",
           "WILL": "Will's Chamber", "KOGA": "Koga's Chamber", "BRUNO": "Bruno's Ring", "KAREN": "Karen's Chamber", "LANCE": "Champion's Room"}
DUNGEON_CODES = {d[0] for d in DUNGEONS}
CODE_OF_EXT = {e[0] for e in EXTERIORS}
WILD_AREA = {"R29": "Route 29", "R30": "Route 30", "R31": "Route 30", "R32": "Route 32", "R33": "Route 32", "R34": "Route 34",
             "R35": "Route 34", "R36": "Route 36", "R37": "Route 36", "R38": "Route 36", "R39": "Route 36", "R41": "Route 41",
             "R42": "Route 43", "R43": "Route 43", "R44": "Route 44", "R45": "Route 44", "R46": "Route 29", "R26": "Route 27",
             "R27": "Route 27", "D25R0101": "Union Cave", "D25R0102": "Union Cave", "D25R0103": "Union Cave",
             "D36R0101": "Ilex Forest", "D22R0101": "National Park", "D38R0101": "Mt. Mortar", "D38R0102": "Mt. Mortar",
             "D38R0103": "Mt. Mortar", "D38R0104": "Mt. Mortar", "D39R0101": "Ice Path", "D39R0102": "Ice Path", "D39R0103": "Ice Path",
             "D39R0104": "Ice Path", "D44R0101": "Dragon's Den", "D44R0102": "Dragon's Den", "D41R0101": "Mt. Silver",
             "D42R0101": "Route 44", "D42R0102": "Route 44", "D45R0101": "Route 27"}

# ------------------------------------------------------------------ decomp tables
MAPNO = {}       # MAP_NEW_BARK -> T20
CODE2MAP = {}
for line in open(os.path.join(HG, "include/constants/maps.h")):
    m = re.match(r"#define (MAP_\w+)\s+\d+\s*//\s*MAP_(\w+)", line)
    if m:
        MAPNO[m.group(1)] = m.group(2)
        CODE2MAP[m.group(2)] = m.group(1)
HEADERS = {}     # MAP_* -> its banks
src = open(os.path.join(HG, "src/data/map_headers.h")).read()
for m in re.finditer(r"\[(MAP_\w+)\]\s*=\s*\{(.*?)\n\s*\},", src, re.S):
    body = m.group(2)
    f = dict(re.findall(r"\.(\w+)\s*=\s*([^,\n]+)", body))
    HEADERS[m.group(1)] = f
EVENTS = {os.path.basename(p)[4:-5]: p for p in glob.glob(os.path.join(HG, "files/fielddata/eventdata/zone_event/*.json"))}


def events_of(code):
    p = EVENTS.get(code)
    return json.load(open(p)) if p else {"warps": [], "objects": [], "bgs": [], "coords": []}


# ------------------------------------------------------------------ text
MSG = {}
def load_msgs(bank):
    if bank in MSG:
        return MSG[bank]
    p = os.path.join(HG, "files/msgdata/msg", bank + ".json")
    out = {}
    if os.path.exists(p):
        for e in json.load(open(p))["messages"]:
            t = e.get("en_US", "")
            out[e["id"]] = "".join(t) if isinstance(t, list) else t
    MSG[bank] = out
    return out


def clean(s, sep="|"):
    """a message as our pages: \\r and \\f end a page, \\n is a space"""
    s = re.sub(r"\{STRVAR_1 3, 0, 0\}", "@P@", s)       # the player's name
    s = re.sub(r"\{STRVAR_1 0, 1, 0\}", "your Pokemon", s)
    s = re.sub(r"\{[^}]*\}", "", s)
    s = s.replace("Pokémon", "Pokemon").replace("POKéMON", "Pokemon").replace("Poké", "Poke").replace("é", "e")
    s = s.replace("…", "...").replace("“", '"').replace("”", '"').replace("’", "'").replace("‘", "'").replace("♂", " M").replace("♀", " F")
    pages = []
    for p in re.split(r"\\[rf]|[\r\f]", s):
        p = re.sub(r"\\n|\n", " ", p)
        p = re.sub(r"\s+", " ", p).strip()
        if p:
            pages.append(p.replace("/", "-").replace("|", "-").replace("@P@", "{PLAYER}"))
    return sep.join(pages)


SCRIPTS = {}
def load_script(code):
    """label -> its lines, for the map's script file"""
    if code in SCRIPTS:
        return SCRIPTS[code]
    out, msgs = {}, None
    ps = glob.glob(os.path.join(HG, "files/fielddata/script/scr_seq", "scr_seq_*_%s.s" % code))
    cur = None
    if ps:
        for line in open(ps[0], encoding="utf-8", errors="replace"):
            m = re.match(r'#include "msgdata/msg/(msg_\w+)\.h"', line)
            if m:
                msgs = m.group(1)
            m = re.match(r"^(\w+):\s*$", line)
            if m:
                cur = m.group(1); out[cur] = []; continue
            if cur and line.strip() and not line.strip().startswith("."):
                out[cur].append(line.strip())
    SCRIPTS[code] = (out, msgs)
    return SCRIPTS[code]


def script_msgs(code, label, limit=3):
    """the lines someone says: follow the script (straight on past the ifs) and collect its messages"""
    lines, bank = load_script(code)
    if not bank:
        return []
    msgs = load_msgs(bank)
    got, seen, stack = [], set(), [label]
    while stack and len(got) < limit:
        lab = stack.pop()
        if lab in seen or lab not in lines:
            continue
        seen.add(lab)
        body = lines[lab]
        nxt = list(lines.keys())
        for ins in body:
            op = ins.split()[0]
            m = re.search(r"(msg_\w+_\d{5})", ins)
            if m and not op.startswith(("Buffer", "GoTo", "Call", "Compare", "Set", "Check")):
                t = msgs.get(m.group(1))
                if t:
                    got.append(clean(t))
                if len(got) >= limit:
                    break
            if op == "GoTo":
                stack.append(ins.split()[1]); break
            if op in ("End", "ReleaseAll", "Return"):
                if op != "ReleaseAll":
                    break
        else:   # (ran off the end of the label: on into the next one)
            i = nxt.index(lab)
            if i + 1 < len(nxt) and not nxt[i + 1].startswith("scr_seq_"):
                stack.append(nxt[i + 1])
    return got


def script_index(sid):
    m = re.match(r"_EV_scr_seq_(\w+?)_(\d+) \+ 1", str(sid))
    return (m.group(1), int(m.group(2))) if m else None


def say_of(code, sid, sep="|"):
    s = script_index(sid)
    if not s:
        return ""
    t = script_msgs(s[0], "scr_seq_%s_%03d" % (s[0], s[1]))
    return sep.join(t)


# ------------------------------------------------------------------ trainers
TR_IDS = {}
for line in open(os.path.join(HG, "include/constants/trainers.h")):
    m = re.match(r"#define (TRAINER_\w+)\s+(\d+)", line)
    if m:
        TR_IDS[m.group(1)] = int(m.group(2))
TRAINERS = json.load(open(os.path.join(HG, "files/poketool/trainer/trainers.json")))["trainers"]
SPECIAL_SPECIES = {"NIDORAN_F": "Nidoran F", "NIDORAN_M": "Nidoran M", "MR_MIME": "Mr. Mime", "FARFETCHD": "Farfetch'd",
                   "HO_OH": "Ho-Oh", "MIME_JR": "Mime Jr.", "PORYGON2": "Porygon2", "PORYGON_Z": "Porygon-Z"}


def species_name(s):
    s = s.replace("SPECIES_", "")
    return SPECIAL_SPECIES.get(s, s.capitalize())


def class_name(c):
    c = c.replace("TRAINERCLASS_", "")
    c = re.sub(r"_GS$", "", c)
    fix = {"BUG_CATCHER": "Bug Catcher", "COOLTRAINER_M": "Cooltrainer", "COOLTRAINER_F": "Cooltrainer", "SAGE": "Sage",
           "PKMN_TRAINER_ETHAN": "", "RIVAL": "", "SCHOOL_KID_M": "Schoolboy", "SCHOOL_KID_F": "Schoolgirl", "BIRD_KEEPER": "Bird Keeper",
           "POKEFAN_M": "Pokefan", "POKEFAN_F": "Pokefan", "SWIMMER_M": "Swimmer", "SWIMMER_F": "Swimmer", "GRUNT_M": "Rocket Grunt",
           "GRUNT_F": "Rocket Grunt", "PSYCHIC_M": "Psychic", "PSYCHIC_F": "Psychic", "TWINS": "Twins", "KIMONO_GIRL": "Kimono Girl",
           "BLACK_BELT": "Black Belt", "SUPER_NERD": "Super Nerd", "POKE_MANIAC": "Poke Maniac", "GUITARIST": "Guitarist",
           "LEADER": "Leader", "ELITE_FOUR": "Elite Four", "CHAMPION": "Champion"}
    return fix.get(c, " ".join(w.capitalize() for w in c.split("_")))


def trainer_of(const):
    i = TR_IDS.get(const)
    if i is None or i >= len(TRAINERS):
        return None
    t = TRAINERS[i]
    name = clean(re.sub(r"\{TRNAME\}", "", t["name"]))
    who = (class_name(t["class"]) + " " + name).strip()
    team = ", ".join("%s %d" % (species_name(p["species"]), p["level"]) for p in t["party"])
    msgs = {m["type"]: clean(m["message"].replace("\\n", "\n").replace("\\r", "\r"), "/") for m in t["messages"]}
    before = msgs.get("TRMSG_INTRO", msgs.get("TRMSG_DBL_INTRO_1", ""))
    after = msgs.get("TRMSG_AFTER", msgs.get("TRMSG_DBL_AFTER_1", msgs.get("TRMSG_LOSE", "")))
    return who, team, before, after


# ------------------------------------------------------------------ items
def item_of(std):
    """std_itemball_r29_potion / std_hiddenitem_t20_potion -> Potion"""
    m = re.match(r"std_(?:itemball|hiddenitem)_(\w+)$", std)
    if not m:
        return None
    k = m.group(1)
    while re.match(r"^[a-z]\d+[a-z]?\d*_", k):   # (the map codes: r29_, t21_, d22r0101_)
        k = k.split("_", 1)[1]
    k = re.sub(r"_\d+$", "", k)
    special = {"poke_ball": "Poke Ball", "pp_up": "PP Up", "hp_up": "HP Up", "x_sp_atk": "X Sp. Atk", "kings_rock": "King's Rock",
               "paralyze_heal": "Paralyze Heal", "tinymushroom": "Tiny Mushroom", "thunderstone": "Thunder Stone"}
    if k in special:
        return special[k]
    m = re.match(r"tm(\d+)", k)
    if m:
        return None   # (TMs: their own pass later)
    return " ".join(w.capitalize() for w in k.split("_"))


# ------------------------------------------------------------------ people
SPRITE = {   # HeartGold's overworld people -> the nearest FireRed one we have (World/chars.txt)
    "GSRIVEL": "rival", "GSWOMAN1": "woman_1", "GSWOMAN2": "woman_2", "GSWOMAN3": "woman_3", "GSWOMAN6": "beauty",
    "GSBIGMAN": "fat_man", "GSMAN1": "man", "GSMAN2": "man", "GSMAN3": "rocker", "GSBOY1": "boy", "GSBOY2": "youngster",
    "GSBOY3": "bug_catcher", "GSGIRL1": "lass", "GSGIRL2": "lass", "GSGIRL3": "picnicker", "GSOLDMAN1": "old_man_1",
    "GSOLDWOMAN1": "old_woman", "GSMIDDLEMAN1": "gentleman", "GSGENTLEMAN": "gentleman", "GSBABYGIRL1": "little_girl",
    "GSBABYBOY1": "little_boy", "GSASSISTANTM": "scientist", "ASSISTANTM": "scientist", "DOCTOR": "scientist",
    "GSMAMA": "mom", "PCWOMAN1": "nurse", "PCWOMAN2": "nurse", "PCWOMAN3": "nurse", "SHOPM1_2": "clerk", "COUNTERM": "clerk",
    "GSFIGHTER": "black_belt", "FIGHTER": "black_belt", "SEAMAN_2": "sailor", "FISHING_2": "fisher", "MOUNT_2": "hiker",
    "CAMPBOY_2": "camper", "PICNICGIRL_2": "picnicker", "MIDDLEWOMAN1_2": "woman_3", "SUNGLASSES": "rocker", "POLICEMAN": "policeman",
    "GSSWIMMERW": "swimmer_f_water", "GSSWIMMERM": "swimmer_m_water", "ROCKETM": "rocket_m", "ROCKETW": "rocket_f",
    "WORKMAN": "worker_m", "MANIA": "poke_maniac", "BOZU": "channeler", "ITAKO": "channeler", "DANCER": "beauty",
    "GSDSBOY": "gba_kid", "BOY1": "boy", "OOKIDO": "prof_oak", "WATARU": "lance", "SAKAKI": "giovanni", "RED": "player",
    "MINAKI": "gentleman", "GANTETSU": "old_man_2", "SUIT": "gentleman", "DELIVERY": "mg_deliveryman", "DELIVERY2": "mg_deliveryman",
    "VAR_1": "lass", "CAPTAIN": "hg_captain",
    # HeartGold's own people (World/chars.png rows from tools/hgss_sprites.py)
    "GSLEADER1": "hg_falkner", "GSLEADER2": "hg_bugsy", "GSLEADER3": "hg_whitney", "GSLEADER4": "hg_morty", "GSLEADER5": "hg_chuck",
    "GSLEADER6": "hg_jasmine", "GSLEADER7": "hg_pryce", "GSLEADER8": "hg_clair", "GSLEADER9": "hg_brock", "GSLEADER10": "hg_misty",
    "GSLEADER11": "hg_surge", "GSLEADER12": "hg_erika", "GSLEADER13": "hg_janine", "GSLEADER14": "hg_sabrina", "GSLEADER15": "hg_blaine",
    "GSLEADER16": "hg_blue", "GSBIGFOUR1": "hg_will", "GSBIGFOUR2": "hg_koga", "GSBIGFOUR3": "hg_bruno", "GSBIGFOUR4": "hg_karen",
    "RKANBUM": "hg_archer", "RKANBUM2": "hg_proton", "RKANBUM3": "hg_petrel", "RKANBUW": "hg_ariana", "DAIGO": "hg_steven",
    "BOZU": "hg_monk", "CHOUROU": "hg_sage", "MAIKO": "hg_kimono_girl", "SUNGLASSES": "rocker",
}
SPRITE.update({"GSRIVEL": "hg_silver", "DOCTOR": "hg_elm", "GSMAMA": "hg_mom", "ROCKETM": "hg_rocket_m", "ROCKETW": "hg_rocket_f",
               "WATARU": "hg_lance", "SAKAKI": "hg_giovanni", "RED": "hg_red", "MINAKI": "hg_eusine", "GANTETSU": "hg_kurt",
               "POLICEMAN": "hg_officer", "SEAMAN_2": "sailor", "DANCER": "hg_kimono_girl"})
SKIP_SPRITES = re.compile(r"^(VAR_\d+|STOP|BONGURI|LEAG_DOOR\d|GATE_\w+|SIGN\w+|POKEWALL|STATUE\w+|\w*TOROPHY|SCROLL|ICE|FIRE|WIFISF|MEDAL|"
                          r"LUG_OBJ\d+|HOU_OBJ\d+|MONO_PIP)$")
WANDER = {3, 4, 5}
FACE = {0: "up", 1: "down", 2: "left", 3: "right"}
MOVE_FACE = {14: "up", 15: "down", 16: "left", 17: "right"}
HIDDEN_STORY = {   # people only there for a scene (we don't play those yet)
    "FLAG_HIDE_NEW_BARK_FRIEND", "FLAG_HIDE_NEW_BARK_FRIEND_2", "FLAG_HIDE_NEW_BARK_MARILL", "FLAG_HIDE_NEW_BARK_MOM",
    "FLAG_HIDE_CAMERON", "FLAG_HIDE_ROUTE_30_BATTLERS", "FLAG_HIDE_ROUTE_30_APRICORN_MAN",
}


def sprite_of(sid):
    s = sid.replace("SPRITE_", "")
    if s.startswith("FOLLOWER_MON_STATIC_"):
        sp = species_name(re.sub(r"_(SPIKY|NOTCHED)$", "", s[len("FOLLOWER_MON_STATIC_"):]))
        return "mon:" + sp if os.path.isdir(os.path.join(HERE, "Pokemon", sp)) else None
    if SKIP_SPRITES.match(s):
        return None
    if s in SPRITE:
        return SPRITE[s]
    if "WOMAN" in s or "GIRL" in s:
        return "woman_1"
    return "man"


def face_of(o):
    mv = o.get("movement", 0)
    if mv in WANDER:
        return "wander"
    if mv in MOVE_FACE:
        return MOVE_FACE[mv]
    return FACE.get(o.get("facingDirection", 1), "down")


# ------------------------------------------------------------------ FireRed's rooms (the templates)
def read_world(path):
    blocks, cur = {}, None
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if line == "[map]":
            cur = []
            continue
        if cur is None:
            continue
        cur.append(line)
        if line.startswith("name = ") and len(cur) == 1:
            blocks[line[7:]] = cur
    return blocks


FR = read_world(os.path.join(WORLD, "world.txt"))


def fr_entry(tname):
    """where you stand coming into a FireRed room: the tile its outside door warps you to"""
    for name, lines in FR.items():
        for l in lines:
            m = re.match(r"door = \d+ \d+ warp\|" + re.escape(tname) + r"\|(\d+)\|(\d+)\|(\w+)", l)
            if m:
                return int(m.group(1)), int(m.group(2)), m.group(3)
    return None


TEMPLATE = {   # kind -> FireRed room
    "pc": "Viridian City Pokemon Center", "mart": "Viridian City Poke Mart", "house1f": "Your House 1F", "house2f": "Your House 2F",
    "rival": "Blue's House", "lab": "Oak's Lab", "gate_ns": "Route 6 North Entrance",
    "house": ["Route 2 House", "Pewter City House 1", "Cerulean City House 1", "Lavender Town House 1", "Fuchsia City House 1",
              "Celadon City House 1", "Pewter City House 2", "Cerulean City House 2"],
}


def kind_of(mapname, code):
    if mapname in SPECIAL_HG:
        return None
    if code in DUNGEON_CODES or code in CODE_OF_EXT:
        return None
    if re.match(r"^D\d", code) and not re.match(r"^D(35|47)R", code):   # towers, wells, caves without a model yet: later
        return None
    if "POKECENTER_1F" in mapname:
        return "pc"
    if "POKECENTER" in mapname:
        return None
    if "POKEMART" in mapname or re.search(r"FS01", code):
        return "mart"
    if mapname.endswith("PLAYER_HOUSE_1F"):
        return "house1f"
    if mapname.endswith("PLAYER_HOUSE_2F"):
        return "house2f"
    if mapname.endswith("ELMS_LAB_1F"):
        return "lab"
    if mapname.endswith("ELMS_LAB_2F"):
        return None
    if "GATEHOUSE" in mapname or "GATE" in mapname:
        return "gate_ns"
    if mapname.endswith("RIVAL_HOUSE_1F"):
        return "rival"
    if re.search(r"R\d\d01$", code) or "HOUSE" in mapname:
        return "house"
    return None


def pretty_room(mapname, town):
    """MAP_GOLDENROD_BIKE_SHOP -> Goldenrod City Bike Shop; MAP_ROUTE_30_MR_POKEMON_HOUSE -> Route 30 Mr. Pokemon House"""
    n = mapname.replace("MAP_", "")
    fixed = {"NEW_BARK_PLAYER_HOUSE_1F": "Your House (New Bark) 1F", "NEW_BARK_PLAYER_HOUSE_2F": "Your House (New Bark) 2F",
             "NEW_BARK_RIVAL_HOUSE_1F": "Lyra's House", "SS_AQUA_OLIVINE_PORT_INTERIOR": "Olivine Port",
             "ROUTE_22_POKEMON_LEAGUE_RECEPTION_GATE": "Pokemon League Reception Gate"}
    if n in fixed:
        return fixed[n]
    for t in ("NEW_BARK", "CHERRYGROVE", "VIOLET", "AZALEA", "GOLDENROD", "ECRUTEAK", "OLIVINE", "CIANWOOD", "MAHOGANY",
              "LAKE_OF_RAGE", "BLACKTHORN", "MOUNT_SILVER"):
        if n.startswith(t + "_"):
            n = n[len(t) + 1:]
            break
    word = {"POKECENTER": "Pokemon Center", "POKEMART": "Poke Mart", "GATEHOUSE": "Gate", "BILLS": "Bill's", "KIRKS": "Kirk's",
            "CAMERONS": "Cameron's", "MR": "Mr.", "POKEATHALON": "Pokeathlon", "OF": "of", "SS": "S.S.", "KURT": "Kurt's",
            "DAYCARE": "Day-Care", "MOOMOO": "MooMoo", "1F": "1F", "2F": "2F"}
    parts = [word.get(w, w.capitalize()) for w in n.split("_")]
    s = " ".join(parts).replace("Pokemon Center 1F", "Pokemon Center")
    if s.startswith("Route "):
        return s
    return (town + " " + s).strip()


# ------------------------------------------------------------------ the models
def find_model(name, floor=None):
    want = os.path.join(CACHE, "models", re.sub(r"\W+", "_", name))
    def pick(daes):
        if floor:
            daes = [d for d in daes if floor in d.replace(os.sep, "/")]
        return sorted(daes)[0] if daes else None
    dae = glob.glob(os.path.join(want, "**", "*.dae"), recursive=True)
    if dae:
        return pick(dae)
    for z in glob.glob(os.path.join(MODELS, "*", name + "__*.zip")) + glob.glob(os.path.join(MODELS, name + "__*.zip")):
        with zipfile.ZipFile(z) as zf:
            for n in zf.namelist():
                if ".." in n or n.startswith("/"):
                    continue
                zf.extract(n, want)
        dae = glob.glob(os.path.join(want, "**", "*.dae"), recursive=True)
        if dae:
            return pick(dae)
    return None


def mark_slopes(info):
    """slopes and steps (their own textures): 's' - you can walk up and down them though they rise a lot"""
    g = [list(r) for r in info["grid"]]
    for y, row in enumerate(info.get("tex", [])):
        for x, t in enumerate(row):
            if t and ("slope" in t or "stairhole" in t) and g[y][x] in ".g":
                g[y][x] = "s"
    # an overpass (the Magnet Train's line over Route 32...): the ground under it is what you walk on
    H, W = len(g), len(g[0]) if g else 0
    hs = info["height"]
    over = lambda x, y: (info["tex"][y][x] or "").startswith(("overpass", "senro"))
    for x in range(W):
        y = 0
        while y < H:
            if not over(x, y):
                y += 1; continue
            y1 = y
            while y1 < H and over(x, y1):
                y1 += 1
            a, b = y - 1, y1   # the ground either side (past the overpass's own sides: up to 2 rows)
            for _ in range(2):
                if a > 0 and g[a][x] == "#" and g[a - 1][x] in ".gcs": a -= 1
                if b < H - 1 and g[b][x] == "#" and g[b + 1][x] in ".gcs": b += 1
            y = max(0, a + 1)
            y1 = b
            ok = a >= 0 and b < H and g[a][x] in ".gcs" and g[b][x] in ".gcs"
            za = hs[a][x] if a >= 0 and hs[a][x] is not None else None
            zb = hs[b][x] if b < H and hs[b][x] is not None else None
            for yy in range(y, y1):
                g[yy][x] = "." if ok else "#"
                if za is not None or zb is not None:
                    hs[yy][x] = za if zb is None else zb if za is None else (za + zb) / 2
            y = y1
    info["grid"] = ["".join(r) for r in g]
    return info


def render(code, model, floor=None):
    png = os.path.join(WORLD, "maps", "hg_%s.png" % code)
    info = os.path.join(CACHE, "hg_%s.json" % code)
    if FORCE or not (os.path.exists(png) and os.path.exists(info)):
        dae = find_model(model, floor)
        if not dae:
            print("no model for", model)
            return None
        print("rendering", model, "...", flush=True)
        hgss_render.render(dae, png, 1.0, info)
    elif REGRID:   # the walking grid again (the walls' rules changed), from the saved heights
        dae = find_model(model, floor)
        if dae:
            hgss_render.regrid(dae, info)
    return mark_slopes(json.load(open(info)))


# ------------------------------------------------------------------ writing
def heights_of(info):
    rows = []
    for row in info["height"]:
        r = ""
        for v in row:
            z = HB if v is None else v
            r += chr(max(33, min(126, 48 + int(round((z - HB) / 4)))))
        rows.append(r)
    return rows


def door_at(grid, x, y, prefer=None):
    """HeartGold's warp tile is the doorway in the wall; a big building's footprint (or a gatehouse's steps) can stand in
    front of it. Our door is the last solid tile you walk into, so: (door x, y) and (where you come out x, y, facing)"""
    H, W = len(grid), len(grid[0])
    ok = lambda tx, ty: 0 <= tx < W and 0 <= ty < H and grid[ty][tx] in ".gcs"
    dirs = [(0, 1, "down"), (0, -1, "up"), (-1, 0, "left"), (1, 0, "right")]
    if prefer:
        dirs.sort(key=lambda d: d[2] != prefer)
    for dx, dy, face in dirs:
        tx, ty = x, y
        for _ in range(10):
            if ok(tx + dx, ty + dy):
                return (tx, ty), (tx + dx, ty + dy, face)
            if not (0 <= tx + dx < W and 0 <= ty + dy < H) or grid[ty + dy][tx + dx] not in "#":
                break
            tx, ty = tx + dx, ty + dy
    return (x, y), (x, y + 1, "down")


def exit_tile(grid, x, y):
    return door_at(grid, x, y)[1]


OUT = []
EXT_NAME = {}     # code -> our name (exteriors)
ROOM_NAME = {}    # MAP_* -> our name (interiors)
MADE_ROOMS = set()


def arrival(target, anchor, src=None):
    """where a warp to that map (its warp number anchor) puts you: (map name, x, y, facing)"""
    if src in CR_PAIR:   # a Crystal room
        nm, k = CR_PAIR[src]
        _, arr = cr_door(CR_ROOM[nm], k)
        return (CR_NAME[nm],) + arr
    tcode = MAPNO.get(target, "")
    if tcode in EXT_INFO:
        info = EXT_INFO[tcode]
        ws = events_of(tcode).get("warps", [])
        if not ws:
            return None
        tw = ws[anchor] if anchor < len(ws) else ws[0]
        x, y = tw["x"] - info["gx0"], tw["z"] - info["gy0"]
        g = info["grid"]
        if tcode not in CODE_OF_EXT and 0 <= y < len(g) and 0 <= x < len(g[0]) and g[y][x] in ".gcs":
            return EXT_NAME[tcode], x, y, "down"
        _, (ex, ey, face) = door_at(g, x, y)
        return EXT_NAME[tcode], ex, ey, face
    if target in ROOM_NAME and src and gate_entry(target, src[0]):
        e = gate_entry(target, src[0])
        return ROOM_NAME[target], e[0], e[1], e[2]
    if target in ROOM_NAME and ROOM_ENTRY.get(target):
        e = ROOM_ENTRY[target]
        return ROOM_NAME[target], e[0], e[1], e[2]
    return None


def emit_exterior(code, model, name, town, dungeon=False, dark=False):
    info = EXT_INFO.get(code)
    if info is None:
        return
    ev = events_of(code)
    gx0, gy0 = info["gx0"], info["gy0"]
    grid = [list(r) for r in info["grid"]]
    L = ["[map]", "name = " + name, "image = maps/hg_%s.png" % code, "over = maps/hg_%s_over.png" % code,
         "objects = sprites", "region = Johto", "imgy = %d" % (-(PAD - HB)), "wild = " + WILD_AREA.get(code, "")]
    if not dungeon:
        L.append("pos = %d %d" % (gx0, gy0))
    elif not code.startswith(("D36", "D22")):   # (a forest, the park and the mountainsides stay outdoors)
        L.append("indoor = yes")
    if dark:
        L.append("dark = yes")
    if town:
        L.append("town = " + town)
    for w in ev.get("warps", []):
        x, y = w["x"] - gx0, w["z"] - gy0
        dest = arrival(w["header"], w.get("anchor", 0), (code, w["x"], w["z"]))
        if not dest or not (0 <= y < len(grid) and 0 <= x < len(grid[0])):
            continue
        if grid[y][x] not in ".gcs" or not dungeon:   # (outdoors a warp is a doorway: our door is the wall you walk into)
            (x, y), _ = door_at(info["grid"], x, y)
        L.append("door = %d %d warp|%s|%d|%d|%s" % ((x, y) + tuple(dest)))
        if 0 <= y < len(grid) and 0 <= x < len(grid[0]):
            grid[y][x] = "D" if grid[y][x] in ".gcs" else grid[y][x]
    inside = lambda x, y: 0 <= y < len(grid) and 0 <= x < len(grid[0])
    for b in ev.get("bgs", []):
        x, y = b["x"] - gx0, b["z"] - gy0
        if not inside(x, y):
            continue
        if b["type"] == 2:
            it = item_of(str(b["scriptId"]))
            if it:
                L.append("item = %d %d %s hidden" % (x, y, it))
        else:
            t = say_of(code, b["scriptId"])
            if t:
                L.append("sign = %d %d %s" % (x, y, t))
    fly = None
    for o in ev.get("objects", []):
        x, y = o["x"] - gx0, o["z"] - gy0
        if not inside(x, y):
            continue
        sid = str(o["scriptId"])
        if sid.startswith("std_itemball"):
            it = item_of(sid)
            if it:
                L.append("item = %d %d %s" % (x, y, it))
            continue
        sp0 = o["spriteId"].replace("SPRITE_", "")
        if sp0 in ("ROCK", "BREAKROCK", "TREE") and inside(x, y):   # Strength boulders, rocks to smash, trees to cut
            grid[y][x] = {"ROCK": "B", "BREAKROCK": "R", "TREE": "T"}[sp0]
            continue
        if sid == "std_field_cut":
            if 0 <= y < len(grid) and 0 <= x < len(grid[0]):
                grid[y][x] = "T"
            continue
        if sid == "std_field_rocksmash":
            if 0 <= y < len(grid) and 0 <= x < len(grid[0]):
                grid[y][x] = "R"
            continue
        if sp0 in ("USOKKY", "RGYARADOSU") and inside(x, y):   # the odd tree / the red Gyarados: the way is open once they're gone
            grid[y][x] = "w" if grid[y][x] == "w" else "."
        if o["eventFlag"] in HIDDEN_STORY:
            continue
        spr = sprite_of(o["spriteId"])
        if not spr:
            continue
        m = re.match(r"std_trainer\((TRAINER_\w+)\)", sid)
        if m:
            t = trainer_of(m.group(1))
            if t:
                who, team, before, after = t
                L.append("npc = %d %d %s %s %d fight|%s|%s|%s|%s" % (x, y, spr, face_of(o), max(1, o.get("param0", 3)), who, team, before, after))
            continue
        t = say_of(code, sid) if sid != "0" else ""
        if not t and sp0 not in SPRITE and not spr.startswith("mon:"):
            continue   # (someone only there for a scene)
        if not t:
            if spr.startswith("mon:"):
                t = spr[4:] + ": ..."
            elif sid == "0":
                continue
            else:
                t = "..."
        L.append("npc = %d %d %s %s 0 talk|%s" % (x, y, spr, face_of(o), t))
    # (the Pokemon Center's door: where Fly lands)
    for w in ev.get("warps", []):
        if "POKECENTER_1F" in w["header"]:
            _, (fx, fy, _) = door_at(info["grid"], w["x"] - gx0, w["z"] - gy0)
            fly = (fx, fy)
    if town and not fly:   # no Pokemon Center (New Bark Town): outside the first door
        for w in ev.get("warps", []):
            _, (fx, fy, _) = door_at(info["grid"], w["x"] - gx0, w["z"] - gy0)
            fly = (fx, fy)
            break
    if town and fly:
        L.append("fly = %d %d" % fly)
    L.append("grid:")
    L += ["".join(r) for r in grid]
    L.append("")
    L.append("heights:")
    L += heights_of(info)
    L.append("")
    OUT.extend(L)


EXT_INFO = {}
ROOM_ENTRY = {}   # MAP_* -> (x, y, face) where you come in
ROOM_PLAN = {}    # MAP_* -> (template, kind, outside code, door x, y)


def plan_rooms():
    """every room an exterior's doors lead to: its name, its FireRed template and where you come in"""
    used = {}
    towns = {e[0]: e[3] for e in EXTERIORS}
    for code in EXT_INFO:
        name, town = EXT_NAME[code], towns.get(code)
        ev = events_of(code)
        for w in ev.get("warps", []):
            tgt = w["header"]
            tcode = MAPNO.get(tgt, "")
            kind = kind_of(tgt, tcode)
            if not kind or tgt in ROOM_PLAN:
                continue
            tpl = TEMPLATE[kind]
            if isinstance(tpl, list):
                k = used.get("house", 0)
                tpl = tpl[(zlib.crc32(tgt.encode()) + k) % len(tpl)]
                used["house"] = k + 1
            ent = fr_entry(tpl)
            if not ent or tpl not in FR:
                print("no template entry for", tgt, tpl)
                continue
            ROOM_PLAN[tgt] = (tpl, kind, code, w["x"], w["z"])
            ROOM_NAME[tgt] = pretty_room(tgt, town or name)
            ROOM_ENTRY[tgt] = ent
            if kind == "house1f":   # upstairs too
                up = tgt.replace("_1F", "_2F")
                ROOM_PLAN[up] = (TEMPLATE["house2f"], "house2f", code, None, None)
                ROOM_NAME[up] = pretty_room(up, town or name)
                ROOM_ENTRY[up] = fr_entry(TEMPLATE["house2f"]) or (6, 6, "down")


def free_tiles(grid, taken):
    H, W = len(grid), len(grid[0])
    out = []
    for y in range(2, H - 1):
        for x in range(1, W - 1):
            if grid[y][x] == "." and (x, y) not in taken:
                out.append((x, y))
    return out


def emit_room(tgt):
    tpl, kind, outcode, dx, dy = ROOM_PLAN[tgt]
    lines = FR[tpl]
    name = ROOM_NAME[tgt]
    tcode = MAPNO.get(tgt, "")
    ev = events_of(tcode)
    out = ["[map]", "name = " + name, "region = Johto"]
    grid = []
    if "grid:" in lines:
        for l in lines[lines.index("grid:") + 1:]:
            if l == "":
                break
            grid.append(l)
    keep_npcs = []
    for l in lines[1:]:
        if l == "grid:":
            break
        k = l.split(" = ")[0]
        if k in ("image", "over", "objects", "indoor"):
            out.append(l)
        elif k == "door":
            m = re.match(r"door = (\d+) (\d+) warp\|([^|]+)\|", l)
            if not m:
                continue
            x, y, to = int(m.group(1)), int(m.group(2)), m.group(3)
            if kind == "house1f" and to.endswith("2F"):
                up = tgt.replace("_1F", "_2F")
                e = ROOM_ENTRY[up]
                out.append("door = %d %d warp|%s|%d|%d|%s" % (x, y, ROOM_NAME[up], e[0], e[1], e[2]))
            elif kind == "house2f":
                down = tgt.replace("_2F", "_1F")
                out.append("door = %d %d warp|%s|%d|%d|up" % (x, y, ROOM_NAME[down], 10, 2))
            elif kind == "gate_ns":
                # the north door(s) lead to the map north of the gate, the south ones back where you came from
                north = y < len(grid) // 2 if grid else False
                ext = gate_side(tgt, north)
                if ext:
                    out.append("door = %d %d warp|%s|%d|%d|%s" % ((x, y) + ext))
            elif dx is not None:
                info = EXT_INFO[outcode]
                _, (ex, ey, face) = door_at(info["grid"], dx - info["gx0"], dy - info["gy0"])
                out.append("door = %d %d warp|%s|%d|%d|%s" % (x, y, EXT_NAME[outcode], ex, ey, face))
        elif k == "sign":
            if kind in ("house1f", "house2f"):
                out.append(l)
        elif k == "npc":
            m = re.match(r"npc = (\d+) (\d+) (\S+) (\S+)", l)
            if m:
                keep_npcs.append((int(m.group(1)), int(m.group(2)), m.group(3), m.group(4)))
        elif k == "block":
            pass
    # the people: the nurse and the clerk where FireRed's stood, everyone else on FireRed's other spots
    spots = [(x, y, f) for x, y, s, f in keep_npcs if s not in ("nurse", "clerk", "item_ball")]
    nurse = [(x, y, f) for x, y, s, f in keep_npcs if s == "nurse"]
    clerk = [(x, y, f) for x, y, s, f in keep_npcs if s == "clerk"]
    taken = {(x, y) for x, y, _, _ in keep_npcs}
    free = free_tiles(grid, taken)
    people = []
    for o in ev.get("objects", []):
        if o["eventFlag"] in HIDDEN_STORY:
            continue
        spr = sprite_of(o["spriteId"])
        if not spr:
            continue
        people.append((spr, o))
    for spr, o in people:
        sid = str(o["scriptId"])
        t = say_of(tcode, sid) if sid != "0" else ""
        if spr == "nurse" and nurse:
            x, y, f = nurse.pop(0)
            out.append("npc = %d %d nurse %s 0 heal|%s" % (x, y, f, t or "Welcome to our Pokemon Center!"))
            continue
        if spr == "clerk" and clerk:
            x, y, f = clerk.pop(0)
            out.append("npc = %d %d clerk %s 0 shop|%s" % (x, y, f, "Welcome! How may I help you?"))
            continue
        if spr in ("nurse", "clerk"):
            continue
        if not t:
            continue
        if spots:
            x, y, f = spots.pop(0)
        elif free:
            x, y = free.pop(len(free) // 2); f = "down"
        else:
            continue
        face = face_of(o) if face_of(o) == "wander" else (f if f in ("up", "down", "left", "right", "wander") else "down")
        out.append("npc = %d %d %s %s 0 talk|%s" % (x, y, spr, face, t))
    for x, y, f in nurse:
        out.append("npc = %d %d nurse %s 0 heal|Welcome to our Pokemon Center!" % (x, y, f))
    for x, y, f in clerk:
        out.append("npc = %d %d clerk %s 0 shop|Welcome! How may I help you?" % (x, y, f))
    out.append("grid:")
    out += grid
    out.append("")
    OUT.extend(out)


GATE_SIDES = {}   # gatehouse MAP_* -> {"top": (code, our name, exit x, y, face), "bottom": ...}


def plan_gates():
    """each gatehouse's two outsides: the one further north (or west, for a gate you cross east-west) gets the
    FireRed room's top door, the other its bottom door"""
    for tgt, (tpl, kind, code, dx, dy) in ROOM_PLAN.items():
        if kind != "gate_ns":
            continue
        sides = []
        for c, info in EXT_INFO.items():
            for ow in events_of(c).get("warps", []):
                if ow["header"] == tgt:
                    x, y = ow["x"] - info["gx0"], ow["z"] - info["gy0"]
                    _, (ex, ey, face) = door_at(info["grid"], x, y)
                    gx, gy = (ow["x"], ow["z"]) if "pos" not in EXT_KIND.get(c, "pos") else (ow["x"], ow["z"])
                    sides.append((c, gx, gy, (EXT_NAME[c], ex, ey, face)))
                    break
        if not sides:
            continue
        if len(sides) == 1:
            GATE_SIDES[tgt] = {"bottom": sides[0]}
            continue
        a, b = sides[0], sides[1]
        ns = abs(a[2] - b[2]) >= abs(a[1] - b[1])
        top, bot = (a, b) if ((a[2] < b[2]) if ns else (a[1] < b[1])) else (b, a)
        def redo(side, prefer):   # out of the gate, away from it: north from the top door, south from the bottom
            c = side[0]; info = EXT_INFO[c]
            _, (ex, ey, face) = door_at(info["grid"], side[1] - info["gx0"], side[2] - info["gy0"], prefer)
            return (c, side[1], side[2], (EXT_NAME[c], ex, ey, face))
        GATE_SIDES[tgt] = {"top": redo(top, ("up" if ns else "left")), "bottom": redo(bot, ("down" if ns else "right"))}


EXT_KIND = {}


def gate_side(tgt, north):
    """a gatehouse's door to the outside: (our map, x, y, face) on the map that way"""
    side = GATE_SIDES.get(tgt, {}).get("top" if north else "bottom")
    return side[3] if side else None


def gate_entry(tgt, code):
    """coming into a gatehouse from that outside: the FireRed room's top or bottom door"""
    s = GATE_SIDES.get(tgt, {})
    if "top" in s and s["top"][0] == code:
        return 4, 2, "down"
    return None


def msg_ref(m):
    """{msg:T20R0101:12}: that map's message 12 (pages split by "|", or "/" inside a give/fight)"""
    code, n = m.group(1), int(m.group(2))
    lines, bank = load_script(code)
    if not bank:
        return ""
    t = load_msgs(bank).get("%s_%05d" % (bank, n), "")
    return clean(t, m.group(3) or "|")


def apply_patches(text, patch):
    """World/patches_johto.txt: "[patch Map Name]" blocks add lines to that map; "drop npc|sign|door|item|block x y" and
    "set key = value" change it; "at x y: line" adds a line after the person standing there; "tile x y C" changes one
    grid square."""
    patch = re.sub(r"\{msg:(\w+):(\d+)(?::(.))?\}", msg_ref, patch)
    blocks = re.split(r"\n(?=\[map\])", text)
    cur, edits = None, {}
    for line in patch.split("\n"):
        s = line.strip()
        m = re.match(r"^\[patch (.+)\]$", s)
        if m:
            cur = m.group(1); edits.setdefault(cur, []); continue
        if cur and s and not s.startswith("#"):
            edits[cur].append(s)
    out = []
    for b in blocks:
        m = re.search(r"^name = (.+)$", b, re.M)
        if m and m.group(1) in edits:
            lines = b.split("\n")
            gi = lines.index("grid:")
            head, rest = lines[:gi], lines[gi:]
            for e in edits.pop(m.group(1)):
                sto = re.match(r"story (.+?) = (.+)$", e)
                if sto:   # a route trainer who is a story battle: "story Youngster Joey = Route 30"
                    for k, l in enumerate(head):
                        mm = re.match(r"(npc = \d+ \d+ \S+ \S+ \d+) fight\|" + re.escape(sto.group(1)) + r"\|[^|]*\|([^|]*)\|([^|]*)$", l)
                        if mm:
                            head[k] = "%s trainer|%s|%s|%s" % (mm.group(1), sto.group(2), mm.group(2) or "Let's battle!", mm.group(3) or "You're strong!")
                            break
                    else:
                        print("patch: no", sto.group(1), "in", m.group(1))
                    continue
                at = re.match(r"at (\d+) (\d+): (.+)$", e)
                d = re.match(r"drop (npc|sign|door|item|spot|block) (\d+) (\d+)$", e)
                st = re.match(r"set (\w+) = (.*)$", e)
                tl = re.match(r"tile (\d+) (\d+) (\S)$", e)
                if tl:   # "tile x y ." : one grid square changed (a gap the 3D model's props closed)
                    tx, ty = int(tl.group(1)), int(tl.group(2))
                    row = rest[1 + ty]
                    rest[1 + ty] = row[:tx] + tl.group(3) + row[tx + 1:]
                elif at:
                    for k, l in enumerate(head):
                        if re.match(r"npc = %s %s " % (at.group(1), at.group(2)), l):
                            head.insert(k + 1, at.group(3)); break
                    else:
                        print("patch: nobody at", at.group(1), at.group(2), "in", m.group(1))
                elif d:
                    keep, skipif = [], False
                    for l in head:
                        if skipif and re.match(r"(if|unless) = ", l):
                            continue
                        skipif = False
                        if re.match(r"%s = %s %s " % (d.group(1), d.group(2), d.group(3)), l):
                            skipif = True; continue
                        keep.append(l)
                    head = keep
                elif st:
                    head = [l for l in head if not l.startswith(st.group(1) + " = ")] + ["%s = %s" % (st.group(1), st.group(2))]
                else:
                    head.append(e)
            b = "\n".join(head + rest)
        out.append(b)
    for k in edits:
        print("patch: no map named", k)
    return "\n".join(out)


# ------------------------------------------------------------------ Crystal's special rooms
CR = None
CR_ROOM = {}     # Crystal map -> its room
CR_NAME = {}     # Crystal map -> our name
CR_PAIR = {}     # (our outdoor code, its warp x, y) -> (Crystal map, warp number)  (HeartGold's door <-> Crystal's)
CR_BACK = {}     # (Crystal map, warp number) -> (our map, x, y, facing)  (Crystal's way out <-> HeartGold's door)


def cr_door(room, k):
    """a Crystal warp: where our door goes (a mat at the edge: the step off it) and where you arrive"""
    x, y = room.warps[k][0], room.warps[k][1]
    c = room.coll[y][x] if 0 <= y < room.h and 0 <= x < room.w else None
    walk = lambda tx, ty: 0 <= ty < room.h and 0 <= tx < room.w and room.grid[ty][tx] in ".gcw"
    if c == "WARP_CARPET_DOWN":
        return (x, y + 1), (x, y, "up")
    if c not in ("WARP_CARPET_UP", "WARP_CARPET_LEFT", "WARP_CARPET_RIGHT") and not walk(x, y):
        # a doorway in the wall (the Elite Four's): our door is the wall, you arrive on the floor in front of it
        for dx, dy, f in ((0, 1, "down"), (0, -1, "up"), (1, 0, "right"), (-1, 0, "left")):
            if walk(x + dx, y + dy):
                return (x, y), (x + dx, y + dy, f)
    if c == "WARP_CARPET_UP":
        return (x, y - 1), (x, y, "down")
    if c == "WARP_CARPET_LEFT":
        return (x - 1, y), (x, y, "right")
    if c == "WARP_CARPET_RIGHT":
        return (x + 1, y), (x, y, "left")
    return (x, y), (x, y, "down")


def cr_item(const):
    k = const.replace("ITEM_", "")
    special = {"PARLYZ_HEAL": "Paralyze Heal", "POKE_BALL": "Poke Ball", "PP_UP": "PP Up", "HP_UP": "HP Up", "X_SPEED": "X Speed",
               "X_ACCURACY": "X Accuracy", "X_SPECIAL": "X Sp. Atk", "TINYMUSHROOM": "Tiny Mushroom", "BIG_MUSHROOM": "Big Mushroom",
               "THUNDERSTONE": "Thunder Stone", "SILVERPOWDER": "Silver Powder", "BLACKGLASSES": "Black Glasses",
               "NEVERMELTICE": "Never-Melt Ice", "KINGS_ROCK": "King's Rock", "TWISTEDSPOON": "Twisted Spoon",
               "BRIGHTPOWDER": "Bright Powder", "MYSTERYBERRY": "Leppa Berry", "MIRACLEBERRY": "Lum Berry", "ICE_BERRY": "Rawst Berry",
               "BURNT_BERRY": "Aspear Berry", "MINT_BERRY": "Chesto Berry", "PRZCUREBERRY": "Cheri Berry", "PSNCUREBERRY": "Pecha Berry",
               "BITTER_BERRY": "Persim Berry", "GOLD_BERRY": "Sitrus Berry", "BERRY": "Oran Berry"}
    if k in special:
        return special[k]
    if k.startswith(("TM_", "HM_")):
        return None
    return " ".join(w.capitalize() for w in k.split("_"))


def cr_trainer(cls, who):
    for c in ("TRAINER_%s_%s" % (cls, who), "TRAINER_%s_GS_%s" % (cls, who)):
        if c in TR_IDS:
            return trainer_of(c)
    for c in TR_IDS:   # (the classes are named a little differently)
        if c.endswith("_" + who) and cls.split("_")[0] in c:
            return trainer_of(c)
    return None


CR_PARTIES, CR_TIDX = {}, {}


def cr_party(cls, who, seen, after):
    """no HeartGold trainer by that name (Team Rocket's): Crystal's own team"""
    if not CR_PARTIES:
        CR_PARTIES.update(crystal_rooms.crystal_parties(CRYSTAL))
        CR_TIDX.update(crystal_rooms.crystal_trainer_index(CRYSTAL))
    k = CR_TIDX.get(who)
    if not k or k not in CR_PARTIES:
        return None
    name, team = CR_PARTIES[k]
    cname = crystal_rooms.CLASS_NAME.get(cls, cls.replace("_", " ").title())
    nm = cname if name.upper() in ("GRUNT", "EXECUTIVE") else cname + " " + name.title()
    return (nm, ", ".join("%s %d" % (species_name(sp), lv) for sp, lv in team), seen.replace("|", "/"), after.replace("|", "/"))


def cr_leader(room, label):
    for l in room.labels.get(label, []):
        m = re.match(r"loadtrainer\s+(\w+),", l)
        if m:
            return m.group(1)
    return None


def plan_crystal():
    global CR
    if not os.path.isdir(CRYSTAL):
        print("no pokecrystal at", CRYSTAL)
        return
    CR = crystal_rooms.CrystalRooms(CRYSTAL)
    for name, hg, our in SPECIAL:
        if name not in CR.info:
            print("no Crystal map", name); continue
        CR_ROOM[name] = CR.room(name)
        CR_NAME[name] = our
    # HeartGold's doors <-> Crystal's: the doors into one HeartGold map, left to right, are Crystal's, left to right
    groups = {}
    for name, hg, our in SPECIAL:
        if hg and name in CR_ROOM:
            groups.setdefault(hg, []).append(name)
    outside_cache = {}
    def outside_warps(const):
        cam = CR.name_of(const)
        if cam not in outside_cache:
            r = crystal_rooms.Room(); r.name = cam
            try:
                CR.events(r)
            except Exception:
                r.warps = []
            outside_cache[cam] = r
        return outside_cache[cam].warps
    for hg, names in groups.items():
        hgw = sorted([(w["x"], w["z"], code) for code in EXT_INFO for w in events_of(code).get("warps", []) if w["header"] == hg])
        # Crystal: each room's warps that lead outside, by the outside warp they use
        keys = {}
        for nm in names:
            for k, (x, y, const, idx) in enumerate(CR_ROOM[nm].warps):
                if const in CR_OUT or const in ("ROUTE_23", "INDIGO_PLATEAU"):
                    ow = outside_warps(const)
                    pos = ow[idx - 1][:2] if 0 < idx <= len(ow) else (x, y)
                    keys.setdefault((const, idx), {"pos": pos, "list": []})["list"].append((nm, k))
        crw = sorted(keys.values(), key=lambda v: v["pos"])
        if not hgw or not crw:
            continue
        for i, v in enumerate(crw):
            gx, gy, code = hgw[min(i, len(hgw) - 1)]
            info = EXT_INFO[code]
            CR_PAIR[(code, gx, gy)] = v["list"][0]
            lx, ly = gx - info["gx0"], gy - info["gy0"]
            g = info["grid"]
            if 0 <= ly < len(g) and 0 <= lx < len(g[0]) and g[ly][lx] in ".gcs":
                back = (EXT_NAME[code], lx, ly, "down")
            else:
                _, (ex, ey, face) = door_at(g, lx, ly)
                back = (EXT_NAME[code], ex, ey, face)
            for nk in v["list"]:
                CR_BACK[nk] = back
        for i in range(len(crw), len(hgw)):   # more HeartGold doors than Crystal ones: they all lead to the first
            gx, gy, code = hgw[i]
            CR_PAIR[(code, gx, gy)] = crw[0]["list"][0]
    # the Johto League: Victory Road's top comes out at Indigo Plateau, straight into Crystal's Pokemon Center
    if "IndigoPlateauPokecenter1F" in CR_ROOM and "D43R0103" in EXT_INFO:
        info = EXT_INFO["D43R0103"]
        for w in events_of("D43R0103").get("warps", []):
            if w["header"] == "MAP_INDIGO_PLATEAU":
                CR_PAIR[("D43R0103", w["x"], w["z"])] = ("IndigoPlateauPokecenter1F", 0)
                lx, ly = w["x"] - info["gx0"], w["z"] - info["gy0"]
                _, (ex, ey, face) = door_at(info["grid"], lx, ly)
                for k in (0, 1):
                    CR_BACK[("IndigoPlateauPokecenter1F", k)] = (EXT_NAME["D43R0103"], ex, ey, face)


def emit_crystal(name):
    room = CR_ROOM[name]
    png = os.path.join(WORLD, "maps", "cr_%s.png" % name)
    room.image.save(png, optimize=True)
    out = ["[map]", "name = " + CR_NAME[name], "region = Johto", "image = maps/cr_%s.png" % name, "objects = sprites"]
    if room.indoor:
        out.append("indoor = yes")
    grid = [list(r) for r in room.grid]
    for k, (x, y, const, idx) in enumerate(room.warps):
        (dx, dy), _ = cr_door(room, k)
        dest = None
        cam = CR.name_of(const)
        if cam in CR_ROOM:
            t = CR_ROOM[cam]
            if 0 < idx <= len(t.warps):
                _, arr = cr_door(t, idx - 1)
                dest = (CR_NAME[cam],) + arr
        elif (name, k) in CR_BACK:
            dest = CR_BACK[(name, k)]
        if dest:
            out.append("door = %d %d warp|%s|%d|%d|%s" % ((dx, dy) + tuple(dest)))
    for x, y, kind, script in room.bgs:
        if kind == "BGEVENT_ITEM":
            for l in room.labels.get(script, []):
                m = re.match(r"hiddenitem\s+(\w+)", l)
                if m and cr_item(m.group(1)):
                    out.append("item = %d %d %s hidden" % (x, y, cr_item(m.group(1))))
            continue
        t = CR.script_text(room, script)
        if t:
            out.append("sign = %d %d %s" % (x, y, t))
    for o in room.objects:
        x, y, spr, typ = o["x"], o["y"], o["sprite"].replace("SPRITE_", ""), o["type"]
        face = "wander" if "WANDER" in o["move"] or "SPINRANDOM" in o["move"] else \
            ("up" if "UP" in o["move"] else "left" if "LEFT" in o["move"] else "right" if "RIGHT" in o["move"] else "down")
        if typ == "OBJECTTYPE_ITEMBALL" or spr == "POKE_BALL":
            for l in room.labels.get(o["script"], []):
                m = re.match(r"itemball\s+(\w+)", l)
                if m and cr_item(m.group(1)):
                    out.append("item = %d %d %s" % (x, y, cr_item(m.group(1))))
            continue
        if spr in ("BOULDER", "ROCK", "FRUIT_TREE", "WEIRD_TREE", "SURF", "POKEDEX", "CONSOLE", "BIG_DOLL", "DOLL_1", "DOLL_2") or spr.startswith("FUCHSIA_GYM"):
            continue
        sprite = ("mon:" + CR_MON[spr]) if spr in CR_MON else CR_SPRITE.get(spr, "man")
        if typ == "OBJECTTYPE_TRAINER":
            tr = CR.trainer_of(room, o["script"])
            if not tr:
                continue
            cls, who, seen, after = tr
            hg = cr_trainer(cls, who) or cr_party(cls, who, seen, after)
            if not hg:
                continue
            nm, team, before, after2 = hg
            out.append("npc = %d %d %s %s %d fight|%s|%s|%s|%s" % (x, y, sprite, face, max(1, o["sight"]), nm, team,
                                                                   before or seen.replace("|", "/"), after2 or after.replace("|", "/")))
            continue
        leader = cr_leader(room, o["script"])
        if leader in LEADERS:
            t = CR.script_text(room, o["script"]).replace("|", "/")
            out.append("npc = %d %d %s %s 0 trainer|%s|%s|You're strong!" % (x, y, sprite, face, LEADERS[leader], t))
            continue
        if spr == "NURSE":
            out.append("npc = %d %d nurse %s 0 heal|Welcome to our Pokemon Center!" % (x, y, face)); continue
        if spr == "CLERK":
            out.append("npc = %d %d clerk %s 0 shop|Welcome! How may I help you?" % (x, y, face)); continue
        t = CR.script_text(room, o["script"])
        if not t:
            continue
        out.append("npc = %d %d %s %s 0 talk|%s" % (x, y, sprite, face, t))
    out.append("grid:")
    out += ["".join(r) for r in grid]
    out.append("")
    OUT.extend(out)


def carve(g, x, y, reach=4):
    """a cave's stairs / ladder walled in by its own model: open the shortest way (up to 4 tiles) to the floor"""
    from collections import deque
    H, W = len(g), len(g[0])
    if any(0 <= x + dx < W and 0 <= y + dy < H and g[y + dy][x + dx] in ".gc" for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
        return
    prev = {(x, y): None}
    q = deque([(x, y, 0)])
    while q:
        cx, cy, d = q.popleft()
        if (cx, cy) != (x, y) and g[cy][cx] in ".gc":
            p = prev[(cx, cy)]
            while p and p != (x, y):
                g[p[1]][p[0]] = "s"
                p = prev[p]
            return
        if d >= reach:
            continue
        for dx, dy in ((0, 1), (0, -1), (-1, 0), (1, 0)):
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < W and 0 <= ny < H and (nx, ny) not in prev and g[ny][nx] in "#.gcs":
                prev[(nx, ny)] = (cx, cy)
                q.append((nx, ny, d + 1))


def main():
    todo = [e for e in EXTERIORS if not ONLY or e[0] in ONLY]
    for code, model, name, town in EXTERIORS:
        EXT_NAME[code] = name
    for code, model, floor, name, dark in DUNGEONS:
        EXT_NAME[code] = name
    for code, model, name, town in todo:   # the pictures (and the ground's shape) first: doors need both ends
        info = render(code, model)
        if info:
            EXT_INFO[code] = info
    for code, model, floor, name, dark in DUNGEONS:
        if ONLY and code not in ONLY:
            continue
        info = render(code, model, floor)
        if info and code.startswith("D36"):   # Ilex Forest: the paths are its ground; the trees' trunks aren't walls of their own
            info["grid"] = ["".join("#" if (t is None or "tree" in t) else ("g" if c == "g" else "." if c == "#" else c)
                                    for t, c in zip(trow, grow)) for trow, grow in zip(info["tex"], info["grid"])]
        if info:   # (a cave's stairs and ladders are models of their own: you stand on them)
            g = [list(r) for r in info["grid"]]
            for w in events_of(code).get("warps", []):
                x, y = w["x"] - info["gx0"], w["z"] - info["gy0"]
                if 0 <= y < len(g) and 0 <= x < len(g[0]):
                    if g[y][x] in "#.":
                        g[y][x] = "s"   # (stairs / a ladder: its height can be anything)
                    carve(g, x, y)
            info["grid"] = ["".join(r) for r in g]
            EXT_INFO[code] = info
    plan_rooms()
    plan_gates()
    plan_crystal()
    OUT.extend(["# Johto, walked: made by tools/hgss_world.py from the HeartGold / SoulSilver location models",
                "# (The Models Resource) and the pret/pokeheartgold decompilation - do not edit by hand.", "",
                "walked = Johto"] + ["story_town = %s | %s" % st for st in STORY_TOWN] + [""])
    for code, model, name, town in todo:
        emit_exterior(code, model, name, town)
    for code, model, floor, name, dark in DUNGEONS:
        emit_exterior(code, model, name, None, True, dark)
    for tgt in ROOM_PLAN:
        if ROOM_PLAN[tgt][2] in EXT_INFO:
            emit_room(tgt)
    for name in CR_ROOM:
        emit_crystal(name)
    text = "\n".join(OUT) + "\n"
    pp = os.path.join(WORLD, "patches_johto.txt")
    if os.path.exists(pp):
        text = apply_patches(text, open(pp, encoding="utf-8").read())
    open(os.path.join(WORLD, "johto.txt"), "w", encoding="utf-8").write(text)
    print("wrote World/johto.txt:", sum(1 for l in OUT if l == "[map]"), "maps")


if __name__ == "__main__":
    main()
