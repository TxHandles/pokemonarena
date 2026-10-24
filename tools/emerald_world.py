#!/usr/bin/env python3
"""Builds walked Hoenn (World/hoenn.txt and World/maps/em_*.png) from the pret/pokeemerald decompilation:
every Hoenn map with its picture, walls, grass, water (and where you can Dive), ledges, doors, connections,
signs, people (and what they say), route trainers and their Emerald teams (rematch teams too, for Match
Call), item balls, hidden items, Cut trees, rocks and boulders, the Pokemon met on the map and the legendaries.

    emerald_world.py <pokeemerald dir> <World dir> <story.txt> [--no-images | --images-only=Route131,...]

World/patches_hoenn.txt (hand-written) is merged in: the story's own battles, gifts, gates and anything the
Emerald scripts do that a line of hoenn.txt can't say. Same syntax as World/patches.txt."""
import json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emerald_render as R

ROOT, WORLD, STORY = sys.argv[1], sys.argv[2], sys.argv[3]
IMAGES = "--no-images" not in sys.argv
ONLY = set()
for a in sys.argv:
    if a.startswith("--images-only="): ONLY = set(a.split("=", 1)[1].split(","))   # (just these maps' pictures)
MAPS = os.path.join(ROOT, "data/maps")

# Not walked: link rooms, Secret Bases, event islands, the Contest stage (the Contest is a screen of its own),
# the Battle Frontier's battle rooms (its facilities run from their lobbies) and Ruby/Sapphire leftovers.
SKIP = re.compile(r"^(BattleColosseum|RecordCorner|TradeCenter|UnionRoom|LinkContestRoom|SecretBase|InsideOfTruck|"
                  r"ContestHall|AlteringCave|"
                  r"BattlePyramidSquare|SSTidalLowerDeck|SSTidalRooms|"
                  r"BattleFrontier_(BattlePike(Room|Corridor|ThreePathRoom)|BattlePyramid(Floor|Top)|BattleArenaBattleRoom|"
                  r"BattleDome(BattleRoom|PreBattleRoom|Corridor)|BattlePalace(BattleRoom|Corridor)|BattleFactory(PreBattleRoom|BattleRoom)|"
                  r"BattleTower(BattleRoom|Elevator|Corridor|MultiPartnerRoom|MultiCorridor|MultiBattleRoom))|"
                  r"Route\d+_?Unused|.*Unused|Route104_Prototype|.*BattleTent(BattleRoom|Corridor))|_PokemonCenter_2F$|^LilycoveCity_ContestHall|^.*_UnusedRubySapphire")

# ------------------------------------------------------------------ names
OVERRIDE = {
    "LittlerootTown_BrendansHouse_1F": "Your Home 1F", "LittlerootTown_BrendansHouse_2F": "Your Home 2F",
    "LittlerootTown_MaysHouse_1F": "May's House 1F", "LittlerootTown_MaysHouse_2F": "May's House 2F",
    "LittlerootTown_ProfessorBirchsLab": "Birch's Lab",
    "EverGrandeCity_SidneysRoom": "Sidney's Room", "EverGrandeCity_PhoebesRoom": "Phoebe's Room",
    "EverGrandeCity_GlaciasRoom": "Glacia's Room", "EverGrandeCity_DrakesRoom": "Drake's Room",
    "EverGrandeCity_ChampionsRoom": "Wallace's Room", "EverGrandeCity_HallOfFame": "Hall of Fame (Hoenn)",
    "EverGrandeCity_PokemonLeague_1F": "Pokemon League (Hoenn)", "EverGrandeCity_HallOfFame": "Hall of Fame (Hoenn)",
    "MtChimney": "Mt. Chimney", "MtChimney_CableCarStation": "Mt. Chimney Cable Car",
    "Route112_CableCarStation": "Route 112 Cable Car", "SSTidalCorridor": "S.S. Tidal",
    "SlateportCity_OceanicMuseum_1F": "Oceanic Museum 1F", "SlateportCity_OceanicMuseum_2F": "Oceanic Museum 2F",
    "MossdeepCity_SpaceCenter_1F": "Mossdeep Space Center 1F", "MossdeepCity_SpaceCenter_2F": "Mossdeep Space Center 2F",
    "Route119_WeatherInstitute_1F": "Weather Institute 1F", "Route119_WeatherInstitute_2F": "Weather Institute 2F",
    "BattleFrontier_OutsideWest": "Battle Frontier West", "BattleFrontier_OutsideEast": "Battle Frontier East",
    "SafariZone_RestHouse": "Safari Zone Rest House (Hoenn)",
}
WORD = {"Pokemon": "Pokemon", "Mart": "Poke Mart", "SS": "S.S.", "Mt": "Mt.", "Mr": "Mr.", "Brineys": "Briney's",
        "Birchs": "Birch's", "Lanettes": "Lanette's", "Cozmos": "Cozmo's", "Wallys": "Wally's", "Stevens": "Steven's",
        "Captains": "Captain's", "Rydels": "Rydel's", "Cycle": "Cycle"}
TOWN_GYM = re.compile(r"^(.+?) (City|Town) Gym( \w+)?$")


def words(s):
    s = re.sub(r"(?<=[a-z])(?=[A-Z0-9])|(?<=[0-9])(?=[A-Z][a-z])|(?<=[A-Z])(?=[A-Z][a-z])", " ", s)
    out = " ".join(WORD.get(w, w) for w in s.split())
    out = re.sub(r"\b([A-Za-z]*[a-rt-z])s (House|Room|Lab|Office|Cabin|Rest Stop|Shipyard|Tunnel)\b", r"\1's \2", out)
    return re.sub(r"(?<=\w) (Of|And|The|To) ", lambda m: " " + m.group(1).lower() + " ", out)


def pretty(folder):
    if folder in OVERRIDE:
        return OVERRIDE[folder]
    out = " ".join(words(p) for p in folder.split("_"))
    out = out.replace("Pokemon Center 1F", "Pokemon Center").replace(" Mart", " Poke Mart").replace("Poke Poke", "Poke")
    out = out.replace("Battle Frontier Battle ", "Battle ").replace("Tunnelers Rest", "Tunnelers' Rest")
    m = TOWN_GYM.match(out)   # "Rustboro City Gym" -> "Rustboro Gym" (as the story calls it)
    if m:
        out = m.group(1) + " Gym" + (m.group(3) or "")
    return out


# ------------------------------------------------------------------ text
def clean_text(s):
    s = s.replace("{POKEBLOCK}S", "Pokeblocks").replace("{POKEBLOCK}", "Pokeblock")
    s = s.replace("{RIVAL}", "May").replace("{PLAYER}", "@P@").replace("{KUN}", "")
    s = re.sub(r"\{[A-Z_0-9 ]+\}", "", s)
    s = s.replace("POKéMON", "Pokemon").replace("POKé", "Poke").replace("Pokémon", "Pokemon").replace("Poké", "Poke")
    s = s.replace("é", "e").replace("…", "...").replace("“", '"').replace("”", '"').replace("’", "'").replace("‘", "'")
    s = s.replace("♂", " M").replace("♀", " F")
    def cap(m):
        w = m.group(0)
        if w in ("TM", "HM", "PP", "HP", "OK", "PC", "TV", "S.S.", "EXP", "ID", "KO", "VS", "UFO", "GO", "PKMN"):
            return w
        return w.capitalize() if len(w) > 1 else w
    s = re.sub(r"\b[A-Z][A-Z']+\b", cap, s)
    s = s.replace("@P@", "{PLAYER}").replace("@p@", "{PLAYER}")
    return s


TEXT = {}
def load_texts():
    files = []
    for d in os.listdir(MAPS):
        for f in ("text.inc", "scripts.inc"):
            p = os.path.join(MAPS, d, f)
            if os.path.exists(p):
                files.append(p)
    for base, _, fs in os.walk(os.path.join(ROOT, "data/text")):
        files += [os.path.join(base, f) for f in fs if f.endswith(".inc")]
    for base, _, fs in os.walk(os.path.join(ROOT, "data/scripts")):
        files += [os.path.join(base, f) for f in fs if f.endswith(".inc")]
    for p in files:
        cur = None
        for line in open(p, encoding="utf-8", errors="replace"):
            m = re.match(r"^(\w+)::?\s*$", line)
            if m:
                cur = m.group(1); TEXT.setdefault(cur, ""); continue
            m = re.match(r'\s*\.(?:string|braille)\s+"(.*)"', line)
            if m and cur:
                TEXT[cur] += m.group(1)
            elif m is None and cur and line.strip() and not line.strip().startswith(".string"):
                pass


def text_of(label, sep="|", sign=False):
    raw = TEXT.get(label)
    if not raw:
        return None
    raw = raw.replace("$", "")
    out = []
    for p in re.split(r"\\p", raw):
        if sign:
            first, _, rest = p.partition("\\n")
            if rest and first.strip() and first.upper() == first:
                out.append(clean_text(first.strip()))
                p = rest
        p = re.sub(r"\\[nl]", " ", p)
        p = re.sub(r"\s+", " ", p).strip()
        if p:
            out.append(clean_text(p))
    return sep.join(t.replace("/", "-").replace("|", "-") for t in out)


# ------------------------------------------------------------------ scripts
SCRIPT = {}
def load_scripts():
    files = [os.path.join(MAPS, d, "scripts.inc") for d in os.listdir(MAPS) if os.path.exists(os.path.join(MAPS, d, "scripts.inc"))]
    files += [os.path.join(ROOT, "data/scripts", f) for f in os.listdir(os.path.join(ROOT, "data/scripts")) if f.endswith(".inc")]
    for p in files:
        cur = None
        for line in open(p, encoding="utf-8", errors="replace"):
            m = re.match(r"^(\w+)::?\s*$", line)
            if m:
                cur = m.group(1); SCRIPT[cur] = []; continue
            if cur and line.strip() and not line.strip().startswith(".string"):
                SCRIPT[cur].append(line.strip())


def script_lines(label, depth=0):
    out = []
    for l in SCRIPT.get(label, []):
        out.append(l)
        m = re.match(r"(?:goto|call)\s+(\w+)$", l) or re.match(r"(?:goto|call)_if_\w+ [^,]+, (?:[^,]+, )?(\w+)$", l)
        if m and depth < 2 and not m.group(1).startswith(("EventScript_", "Common_")):
            out += script_lines(m.group(1), depth + 1)
    return out


# ------------------------------------------------------------------ trainers
TRAINERS, PARTIES, CLASSNAMES = {}, {}, {}
def load_trainers():
    src = open(os.path.join(ROOT, "src/data/trainers.h")).read()
    for m in re.finditer(r"\[(TRAINER_\w+)\]\s*=\s*\{(.*?)\n    \},", src, re.S):
        b = m.group(2)
        cls = re.search(r"\.trainerClass\s*=\s*(\w+)", b)
        name = re.search(r'\.trainerName\s*=\s*_\("(.*?)"\)', b)
        party = re.search(r"\.party\s*=\s*\w+\((\w+)\)", b)
        double = "doubleBattle = TRUE" in b
        TRAINERS[m.group(1)] = (cls.group(1) if cls else "", name.group(1) if name else "", party.group(1) if party else "", double)
    ps = open(os.path.join(ROOT, "src/data/trainer_parties.h")).read()
    for chunk in ps.split("static const struct")[1:]:
        m = re.search(r"(sParty_\w+)\[\]", chunk)
        if not m:
            continue
        mons = re.findall(r"\.lvl\s*=\s*(\d+),\s*\n\s*\.species\s*=\s*SPECIES_(\w+)", chunk)
        PARTIES[m.group(1)] = [(species_name(s), int(l)) for l, s in mons]
    cn = open(os.path.join(ROOT, "src/data/text/trainer_class_names.h")).read()
    for k, v in re.findall(r"\[(TRAINER_CLASS_\w+)\]\s*=\s*_\(\"(.*?)\"\)", cn):
        CLASSNAMES[k] = clean_text(v.replace("{PKMN}", "PKMN")).replace("PKMN ", "Pokemon ").replace("Sr. And Jr.", "Sr. and Jr.")


SPECIAL_SPECIES = {"NIDORAN_F": "Nidoran F", "NIDORAN_M": "Nidoran M", "MR_MIME": "Mr. Mime", "FARFETCHD": "Farfetch'd",
                   "HO_OH": "Ho-Oh", "UNOWN": "Unown", "PORYGON2": "Porygon2"}
def species_name(s):
    if s in SPECIAL_SPECIES:
        return SPECIAL_SPECIES[s]
    return "-".join(p.capitalize() for p in s.split("_")) if "_" in s else s.capitalize()


def trainer_line(tid):
    cls, name, party, double = TRAINERS[tid]
    who = (CLASSNAMES.get(cls, words(cls.replace("TRAINER_CLASS_", "").title().replace("_", ""))) + " " + clean_text(name).title()).strip()
    team = ", ".join(f"{s} {l}" for s, l in PARTIES.get(party, []))
    return who, team


def rematch_teams(tid):
    """Match Call: the trainer's stronger teams (TRAINER_CALVIN_2 .. _5)"""
    m = re.match(r"(TRAINER_\w+?)_1$", tid)
    if not m:
        return []
    out = []
    for k in range(2, 6):
        t = f"{m.group(1)}_{k}"
        if t in TRAINERS:
            out.append(trainer_line(t)[1])
    return out


# ------------------------------------------------------------------ items
ITEM_NAMES = {}
GEN3_TMS = ["Focus Punch", "Dragon Claw", "Water Pulse", "Calm Mind", "Roar", "Toxic", "Hail", "Bulk Up", "Bullet Seed", "Hidden Power",
            "Sunny Day", "Taunt", "Ice Beam", "Blizzard", "Hyper Beam", "Light Screen", "Protect", "Rain Dance", "Giga Drain", "Safeguard",
            "Frustration", "Solar Beam", "Iron Tail", "Thunderbolt", "Thunder", "Earthquake", "Return", "Dig", "Psychic", "Shadow Ball",
            "Brick Break", "Double Team", "Reflect", "Shock Wave", "Flamethrower", "Sludge Bomb", "Sandstorm", "Fire Blast", "Rock Tomb", "Aerial Ace",
            "Torment", "Facade", "Secret Power", "Rest", "Attract", "Thief", "Steel Wing", "Skill Swap", "Snatch", "Overheat"]
TM_STANDIN = {"Hail": "Snowscape", "Frustration": "Lash Out", "Return": "Body Slam", "Double Team": "Substitute",
              "Torment": "Spite", "Secret Power": "Swift", "Attract": "Charm", "Steel Wing": "Dual Wingbeat", "Snatch": "Trick",
              "Hidden Power": "Tera Blast", "Iron Tail": "Iron Head"}
EM_HMS = ["HM01 Cut", "HM02 Fly", "HM03 Surf", "HM04 Strength", "HM05 Flash", "HM06 Rock Smash", "HM07 Waterfall", "HM08 Dive"]
OURS = set()
OUR_TM = {}
UNKNOWN_ITEMS = set()
def load_our_items():
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
    it = open(os.path.join(src, "items.cpp")).read()
    OURS.update(re.findall(r'(?:add|med|bx|pu|val)\("([^"]+)"', it))
    OURS.update(re.findall(r'\{"([^"]+)", Type::', it))
    OURS.update(re.findall(r'"(\w+ Berry)"', it))   # (berries added in a loop)
    for no, mv in re.findall(r'\{(\d+), "([^"]+)"', open(os.path.join(src, "tms.inc")).read()):
        OUR_TM[mv] = "TM%03d %s" % (int(no), mv)
    for m in re.finditer(r"\[(ITEM_\w+)\]\s*=\s*\{\s*\.name\s*=\s*_\(\"(.*?)\"\)", open(os.path.join(ROOT, "src/data/items.h")).read()):
        ITEM_NAMES[m.group(1).replace("ITEM_", "")] = m.group(2)


def item_name(it):
    k = it.replace("ITEM_", "")
    if k.endswith("_MAIL"):
        return None   # (Mail: nothing to write here)
    mm = re.match(r"(TM|HM)_(\w+)$", k)   # Emerald names them by move: ITEM_HM_FLASH, ITEM_TM_FOCUS_PUNCH
    if mm:
        key = mm.group(2).replace("_", "").lower()
        lst = EM_HMS if mm.group(1) == "HM" else ["%s%02d %s" % ("TM", i + 1, t) for i, t in enumerate(GEN3_TMS)]
        for i, e in enumerate(lst):
            if e.split(" ", 1)[1].replace(" ", "").replace("-", "").lower() == key:
                k = "%s%02d" % (mm.group(1), i + 1)
                break
    m = re.match(r"(TM|HM)(\d+)", k)
    if m:
        n = int(m.group(2))
        if m.group(1) == "HM":
            return EM_HMS[n - 1]
        mv = GEN3_TMS[n - 1] if n <= len(GEN3_TMS) else ""
        mv = TM_STANDIN.get(mv, mv)
        if mv in OUR_TM:
            return OUR_TM[mv]
        UNKNOWN_ITEMS.add("TM%02d %s" % (n, mv))
        return "TM%02d %s" % (n, mv)
    special = {"POKE_BALL": "Poke Ball", "X_SP_ATK": "X Sp. Atk", "PP_UP": "PP Up", "PP_MAX": "PP Max", "HP_UP": "HP Up",
               "TINY_MUSHROOM": "Tiny Mushroom", "SS_TICKET": "S.S. Ticket", "KINGS_ROCK": "King's Rock",
               "PARALYZE_HEAL": "Paralyze Heal", "MACH_BIKE": "Bicycle", "ACRO_BIKE": "Bicycle", "DEVON_GOODS": "Devon Goods",
               "LETTER": "Letter", "POKEBLOCK_CASE": "Pokeblock Case", "GO_GOGGLES": "Go-Goggles", "DEVON_SCOPE": "Devon Scope",
               "BASEMENT_KEY": "Basement Key", "STORAGE_KEY": "Storage Key", "ROOM_1_KEY": "Room 1 Key", "ROOM_2_KEY": "Room 2 Key",
               "ROOM_4_KEY": "Room 4 Key", "ROOM_6_KEY": "Room 6 Key", "WAILMER_PAIL": "Wailmer Pail", "SOOT_SACK": "Soot Sack",
               "MAGMA_EMBLEM": "Magma Emblem", "METEORITE": "Meteorite", "SCANNER": "Scanner", "COIN_CASE": "Coin Case",
               "ITEMFINDER": "Itemfinder", "EXP_SHARE": "Exp. Share", "SILK_SCARF": "Silk Scarf", "TWISTED_SPOON": "Twisted Spoon",
               "BLACK_GLASSES": "Black Glasses", "NEVER_MELT_ICE": "Never-Melt Ice", "DEEP_SEA_TOOTH": "Deep Sea Tooth",
               "DEEP_SEA_SCALE": "Deep Sea Scale", "UP_GRADE": "Linking Cord", "DRAGON_SCALE": "Linking Cord",
               "THUNDER_STONE": "Thunder Stone", "SUPER_ROD": "Super Rod", "GOOD_ROD": "Good Rod", "OLD_ROD": "Old Rod",
               "CONTEST_PASS": "Contest Pass", "EON_TICKET": "Eon Ticket", "EXP_SHARE": "Exp. All"}
    if k in special:
        name = special[k]
    else:
        e = ITEM_NAMES.get(k, k.replace("_", " "))
        name = " ".join(w.capitalize() if not re.match(r"^[A-Z]{1,2}$", w) or w in ("UP",) else w for w in clean_text(e).replace("é", "e").split())
        name = name.replace("Pokeball", "Poke Ball").replace("Poke ball", "Poke Ball")
    name = {"Thunderstone": "Thunder Stone", "Blackglasses": "Black Glasses", "Nevermeltice": "Never-Melt Ice",
            "Guard Spec": "Guard Spec.", "Brightpowder": "Bright Powder", "Silverpowder": "Silver Powder",
            "Twistedspoon": "Twisted Spoon", "Deepseatooth": "Deep Sea Tooth", "Deepseascale": "Deep Sea Scale",
            "Parlyz Heal": "Paralyze Heal", "Energypowder": "Energy Powder", "Tinymushroom": "Tiny Mushroom",
            "Fluffy Tail": "Poke Doll"}.get(name, name)
    low = {n.lower(): n for n in OURS}
    if name.lower() in low:
        return low[name.lower()]
    UNKNOWN_ITEMS.add(name)
    return name


# ------------------------------------------------------------------ sprites
def load_sprite_names():
    p = os.path.join(WORLD, "chars.txt")
    return open(p).read().split() if os.path.exists(p) else []


MON_GFX = {"WINGULL": "Wingull", "KECLEON": "Kecleon", "POOCHYENA": "Poochyena", "ZIGZAGOON_1": "Zigzagoon", "ZIGZAGOON_2": "Zigzagoon",
           "SKITTY": "Skitty", "AZURILL": "Azurill", "AZUMARILL": "Azumarill", "PIKACHU": "Pikachu", "KIRLIA": "Kirlia",
           "DUSCLOPS": "Dusclops", "SUDOWOODO": "Sudowoodo", "MEW": "Mew", "LUGIA": "Lugia", "HOOH": "Ho-Oh",
           "RAYQUAZA": "Rayquaza", "RAYQUAZA_STILL": "Rayquaza", "KYOGRE_FRONT": "Kyogre", "KYOGRE_SIDE": "Kyogre",
           "KYOGRE_ASLEEP": "Kyogre", "GROUDON_FRONT": "Groudon", "GROUDON_SIDE": "Groudon", "GROUDON_ASLEEP": "Groudon",
           "REGIROCK": "Regirock", "REGICE": "Regice", "REGISTEEL": "Registeel", "DEOXYS": "Deoxys",
           "VIGOROTH_CARRYING_BOX": "Vigoroth", "VIGOROTH_FACING_AWAY": "Vigoroth"}
LEGENDS = {"Rayquaza", "Kyogre", "Groudon", "Regirock", "Regice", "Registeel"}
SKIP_GFX = {"OBJ_EVENT_GFX_TRUCK", "OBJ_EVENT_GFX_MOVING_BOX", "OBJ_EVENT_GFX_SS_TIDAL", "OBJ_EVENT_GFX_MR_BRINEYS_BOAT",
            "OBJ_EVENT_GFX_CABLE_CAR", "OBJ_EVENT_GFX_SUBMARINE_SHADOW", "OBJ_EVENT_GFX_KECLEON_BRIDGE_SHADOW",
            "OBJ_EVENT_GFX_FOSSIL", "OBJ_EVENT_GFX_DEOXYS_TRIANGLE",
            "OBJ_EVENT_GFX_BIRCHS_BAG", "OBJ_EVENT_GFX_BERRY_TREE", "0",
            "OBJ_EVENT_GFX_TREECKO_DOLL", "OBJ_EVENT_GFX_TORCHIC_DOLL", "OBJ_EVENT_GFX_MUDKIP_DOLL", "OBJ_EVENT_GFX_SWABLU_DOLL",
            "OBJ_EVENT_GFX_SKITTY_DOLL", "OBJ_EVENT_GFX_PIKACHU_DOLL", "OBJ_EVENT_GFX_PICHU_DOLL"}
SPRITE_ALIAS = {"rival_may_normal": "em_may", "rival_brendan_normal": "em_brendan", "prof_birch": "em_prof_birch",
                "mart_employee": "clerk"}


def gift_of(lines):
    ITEMLINE = r"(?:giveitem|checkitemspace|additem|giveitem_msg \w+,) (ITEM_\w+)(?:, (\d+))?"
    item = next((re.match(ITEMLINE, l) for l in lines if re.match(ITEMLINE, l)), None)
    setf = next((re.match(r"setflag (FLAG_\w+)", l).group(1) for l in lines if re.match(r"setflag FLAG_(GOT|RECEIVED)\w*", l)), None)
    if not item or not setf or "removeitem" in " ".join(lines):
        return None
    flag = next((re.match(r"goto_if_set (\w+), (\w+)", l) for l in lines if re.match(r"goto_if_set " + setf + r", (\w+)", l)), None)
    already = set(SCRIPT.get(flag.group(2), [])) if flag else set()
    intro = None
    for l in lines:
        m = re.match(r"msgbox (\w+)", l)
        if m and text_of(m.group(1)) and l not in already:
            intro = text_of(m.group(1), "/"); break
        if re.match(ITEMLINE, l):
            break
    after = None
    for l in SCRIPT.get(flag.group(2), []) if flag else []:
        m = re.match(r"msgbox (\w+)", l)
        if m and text_of(m.group(1)):
            after = text_of(m.group(1), "/"); break
    if not after:
        seen = False
        for l in lines:
            if re.match(ITEMLINE, l): seen = True; continue
            m = re.match(r"msgbox (\w+)", l)
            if seen and m and text_of(m.group(1)):
                after = text_of(m.group(1), "/"); break
    name = item_name(item.group(1))
    if not name:
        return None
    return f"give|{intro or 'Here, take this!'}|{name}|{item.group(2) or 1}|got:{setf}|{after or 'Take care!'}"


def face_of(mt):
    mt = mt.replace("MOVEMENT_TYPE_", "")
    if "WANDER" in mt or mt in ("WALK_UP_AND_DOWN", "WALK_LEFT_AND_RIGHT"):
        return "wander"
    for k, v in (("FACE_UP", "up"), ("FACE_LEFT", "left"), ("FACE_RIGHT", "right"), ("WALK_IN_PLACE_UP", "up"),
                 ("WALK_IN_PLACE_LEFT", "left"), ("WALK_IN_PLACE_RIGHT", "right"), ("FACE_DOWN_AND_UP", "down"),
                 ("FACE_UP_AND_LEFT", "up"), ("FACE_LEFT_AND_RIGHT", "left"), ("FACE_UP_AND_RIGHT", "up")):
        if mt.startswith(k):
            return v
    return "down"


# ------------------------------------------------------------------ berry trees
TREE_ID, TREE_BERRY = {}, {}
def load_berry_trees():
    for n, v in re.findall(r"#define (BERRY_TREE_\w+)\s+(\d+)", open(os.path.join(ROOT, "include/constants/berry.h")).read()):
        TREE_ID[n] = int(v)
    for n, b in re.findall(r"setberrytree (BERRY_TREE_\w+),\s*ITEM_TO_BERRY\(ITEM_(\w+)_BERRY\)", open(os.path.join(ROOT, "data/scripts/new_game.inc")).read()):
        TREE_BERRY[n] = b.title()


# ------------------------------------------------------------------ the grid
MB = {}
def load_behaviors():
    src = open(os.path.join(ROOT, "include/constants/metatile_behaviors.h")).read()
    body = src[src.index("enum"):]
    for k, name in enumerate(re.findall(r"^\s*(MB_\w+)", body, re.M)):
        MB.setdefault(name, k)


# Maps whose scripts swap in another layout as you arrive (setmaplayoutindex): Route 131's is the one with the
# channel up to the Sky Pillar.
LAYOUT_OVERRIDE = {"Route131": "LAYOUT_ROUTE131_SKY_PILLAR"}


def grid_of(cells, W, H, land, underwater):
    b = MB
    water = {b[n] for n in ("MB_POND_WATER", "MB_INTERIOR_DEEP_WATER", "MB_DEEP_WATER", "MB_SOOTOPOLIS_DEEP_WATER", "MB_OCEAN_WATER",
                            "MB_UNUSED_SOOTOPOLIS_DEEP_WATER", "MB_UNUSED_SOOTOPOLIS_DEEP_WATER_2", "MB_EASTWARD_CURRENT",
                            "MB_WESTWARD_CURRENT", "MB_NORTHWARD_CURRENT", "MB_SOUTHWARD_CURRENT", "MB_NO_SURFACING",
                            "MB_SEAWEED", "MB_SEAWEED_NO_SURFACING")}
    dive = {b["MB_INTERIOR_DEEP_WATER"], b["MB_DEEP_WATER"], b["MB_SOOTOPOLIS_DEEP_WATER"]}
    no_emerge = {b["MB_NO_SURFACING"], b["MB_SEAWEED_NO_SURFACING"], b["MB_UNUSED_SOOTOPOLIS_DEEP_WATER_2"]}
    grass = {b["MB_TALL_GRASS"], b["MB_LONG_GRASS"], b["MB_ASHGRASS"]}
    cave = {b["MB_CAVE"], b["MB_INDOOR_ENCOUNTER"], b["MB_DEEP_SAND"], b["MB_FOOTPRINTS"], b["MB_UNUSED_05"]}
    ledge = {b["MB_JUMP_SOUTH"]: "v", b["MB_JUMP_EAST"]: ">", b["MB_JUMP_WEST"]: "<"}
    spin = {b["MB_WALK_NORTH"]: "1", b["MB_WALK_SOUTH"]: "2", b["MB_WALK_WEST"]: "3", b["MB_WALK_EAST"]: "4",
            b["MB_SLIDE_NORTH"]: "1", b["MB_SLIDE_SOUTH"]: "2", b["MB_SLIDE_WEST"]: "3", b["MB_SLIDE_EAST"]: "4"}
    g = []
    for (mid, coll, elev, beh) in cells:
        c = "#" if coll else "."
        if underwater:
            if not coll:
                c = "g" if beh == b["MB_SEAWEED_NO_SURFACING"] or beh == b["MB_SEAWEED"] else ("." if beh in no_emerge else "u")
        elif beh == b["MB_WATERFALL"]:
            c = "W"
        elif beh in water and not coll:
            c = "d" if beh in dive else "w"
        elif beh in ledge:
            c = ledge[beh]
        elif beh == b["MB_JUMP_NORTH"]:
            c = "#"
        elif beh in spin and not coll:
            c = spin[beh]
        elif beh == b["MB_TRICK_HOUSE_PUZZLE_8_FLOOR"] and not coll:
            c = "i"   # (slippery: you slide on to the next obstacle)
        elif not coll and beh in grass:
            c = "g"
        elif not coll and land and beh in cave:
            c = "c"
        g.append(c)
    return g


# ------------------------------------------------------------------ the wild areas (wild.txt's Hoenn areas)
ROUTE_AREA = [((101, 104), "Routes 101 to 103"), ((105, 109), "Routes 105 to 109"), ((110, 110), "Route 110 and New Mauville"),
              ((111, 111), "Route 111 Desert"), ((112, 112), "Fiery Path and Route 112"), ((113, 115), "Routes 113 and 114"),
              ((116, 116), "Route 116 and Rusturf Tunnel"), ((117, 117), "Route 117 and Verdanturf"),
              ((118, 119), "Routes 118 and 119"), ((120, 121), "Routes 120 and 121"), ((122, 123), "Mt. Pyre Slopes"),
              ((124, 134), "Routes 124 to 128")]
PLACE_AREA = [("Underwater", "Underwater (Hoenn)"), ("PetalburgWoods", "Petalburg Woods"), ("RusturfTunnel", "Route 116 and Rusturf Tunnel"),
              ("GraniteCave", "Granite Cave"), ("NewMauville", "Route 110 and New Mauville"), ("FieryPath", "Fiery Path and Route 112"),
              ("JaggedPass", "Fiery Path and Route 112"), ("MagmaHideout", "Fiery Path and Route 112"), ("MeteorFalls", "Meteor Falls Caves"),
              ("SafariZone", "Safari Zone"), ("MtPyre", "Mt. Pyre Slopes"), ("ShoalCave", "Shoal Cave"), ("SkyPillar", "Sky Pillar Tower"),
              ("VictoryRoad", "Victory Road (Hoenn)"), ("DesertUnderpass", "Desert Underpass and Mirage Island"),
              ("MirageTower", "Desert Underpass and Mirage Island"), ("DesertRuins", "Route 111 Desert"), ("ArtisanCave", "Fiery Path and Route 112"),
              ("ScorchedSlab", "Routes 120 and 121"), ("AncientTomb", "Routes 120 and 121"), ("IslandCave", "Routes 105 to 109"),
              ("SealedChamber", "Routes 124 to 128"), ("CaveOfOrigin", "Routes 124 to 128"), ("SeafloorCavern", "Routes 124 to 128"),
              ("MarineCave", "Routes 124 to 128"), ("TerraCave", "Route 111 Desert"), ("AbandonedShip", "Routes 105 to 109"),
              ("PetalburgCity", "Routes 101 to 103"), ("LittlerootTown", "Routes 101 to 103"), ("DewfordTown", "Routes 105 to 109"),
              ("SlateportCity", "Routes 105 to 109"), ("LilycoveCity", "Routes 120 and 121"), ("MossdeepCity", "Routes 124 to 128"),
              ("SootopolisCity", "Routes 124 to 128"), ("EverGrandeCity", "Routes 124 to 128"), ("PacifidlogTown", "Routes 124 to 128"),
              ("FortreeCity", "Routes 118 and 119"), ("MauvilleCity", "Route 110 and New Mauville"), ("BattleFrontier", "Routes 124 to 128")]


def wild_area(folder):
    m = re.match(r"(?:Underwater_)?Route(\d+)", folder)
    if folder.startswith("Underwater"):
        return "Underwater (Hoenn)"
    if m:
        n = int(m.group(1))
        for (lo, hi), a in ROUTE_AREA:
            if lo <= n <= hi:
                return a
    for pre, a in PLACE_AREA:
        if folder.startswith(pre):
            return a
    return ""


# where the story's Hoenn battles are fought: the story stage -> the town whose Pokemon Center you return to
STORY_TOWN = [("Route 103", "Oldale Town"), ("Petalburg Woods", "Petalburg City"), ("Rustboro City", "Rustboro City"),
              ("Rustboro Gym", "Rustboro City"), ("Rusturf Tunnel", "Rustboro City"), ("Dewford Gym", "Dewford Town"),
              ("Oceanic Museum", "Slateport City"), ("Route 110", "Slateport City"), ("Cycling Road", "Mauville City"),
              ("Mauville City", "Mauville City"), ("Mauville Gym", "Mauville City"), ("Mt. Chimney", "Lavaridge Town"),
              ("Mt. Chimney Summit", "Lavaridge Town"), ("Lavaridge Gym", "Lavaridge Town"), ("Petalburg Gym", "Petalburg City"),
              ("Weather Institute", "Fortree City"), ("Route 119", "Fortree City"), ("Route 120", "Fortree City"),
              ("Fortree Gym", "Fortree City"), ("Lilycove City", "Lilycove City"), ("Mt. Pyre", "Lilycove City"),
              ("Magma Hideout", "Lavaridge Town"), ("Magma Hideout Depths", "Lavaridge Town"), ("Aqua Hideout", "Lilycove City"),
              ("Aqua Hideout Storeroom", "Lilycove City"), ("Mossdeep Space Center", "Mossdeep City"), ("Mossdeep Gym", "Mossdeep City"),
              ("Seafloor Cavern", "Mossdeep City"), ("Seafloor Cavern Depths", "Mossdeep City"), ("Sootopolis Gym", "Sootopolis City"),
              ("Victory Road B2F", "Ever Grande City"), ("Victory Road", "Ever Grande City"), ("Sidney's Room", "Ever Grande City"),
              ("Phoebe's Room", "Ever Grande City"), ("Glacia's Room", "Ever Grande City"), ("Drake's Room", "Ever Grande City"),
              ("Champion's Room", "Ever Grande City")]

# Emerald's fixed dive spots (map scripts' setdivewarp): diving / surfacing anywhere on that map goes there
FIXED_DIVE = {"Route134": ("dive", "Underwater_Route134", 8, 6), "Underwater_Route134": ("emerge", "Route134", 60, 31),
              "SootopolisCity": ("dive", "Underwater_SootopolisCity", 9, 6), "Underwater_SootopolisCity": ("emerge", "SootopolisCity", 29, 53),
              "SeafloorCavern_Entrance": ("dive", "Underwater_SeafloorCavern", 6, 5),
              "Underwater_SeafloorCavern": ("emerge", "SeafloorCavern_Entrance", 10, 17),
              "SealedChamber_OuterRoom": ("dive", "Underwater_SealedChamber", 12, 44),
              "Underwater_SealedChamber": ("emerge", "Route134", 60, 31),
              "AbandonedShip_HiddenFloorCorridors": ("dive", "AbandonedShip_Underwater1", 5, 4),
              "AbandonedShip_Corridors_B1F": ("dive", "AbandonedShip_Underwater1", 5, 4),
              "AbandonedShip_Underwater1": ("emerge", "AbandonedShip_HiddenFloorCorridors", 0, 10),
              "AbandonedShip_Rooms_B1F": ("dive", "AbandonedShip_Underwater2", 17, 4),
              "AbandonedShip_Underwater2": ("emerge", "AbandonedShip_Rooms_B1F", 13, 7)}


# ------------------------------------------------------------------ main
def existing_names():
    out = set()
    for f in ("world.txt", "johto.txt"):
        p = os.path.join(WORLD, f)
        if os.path.exists(p):
            out |= set(re.findall(r"^name = (.+)$", open(p, encoding="utf-8").read(), re.M))
    return out


def main():
    load_texts(); load_scripts(); load_trainers(); load_our_items(); load_behaviors(); load_berry_trees()
    sprite_names = load_sprite_names()
    found = R.find_tilesets(ROOT)
    lays = {l["id"]: l for l in json.load(open(os.path.join(ROOT, "data/layouts/layouts.json")))["layouts"] if "width" in l}
    maps = {}
    for d in sorted(os.listdir(MAPS)):
        p = os.path.join(MAPS, d, "map.json")
        if os.path.exists(p) and not SKIP.search(d):
            j = json.load(open(p))
            j["folder"] = d
            maps[j["id"]] = j
    import emerald_trainer_hill
    emerald_trainer_hill.expand(ROOT, maps, lays)   # the Trainer Hill's floors: one map for each of its four modes
    by_folder = {j["folder"]: mid for mid, j in maps.items()}
    names = {mid: j.get("_name") or pretty(j["folder"]) for mid, j in maps.items()}
    dup = {}
    for mid, n in names.items():
        dup.setdefault(n, []).append(mid)
    for n, ids in dup.items():
        if len(ids) > 1:
            for mid in ids:
                if maps[mid].get("_name"):
                    continue
                names[mid] = words(maps[mid]["folder"].replace("_", " "))
    taken = existing_names()
    for mid in names:   # Kanto and Johto have a Victory Road 1F and a Safari Zone North too
        if names[mid] in taken:
            names[mid] += " (Hoenn)"
    wild = json.load(open(os.path.join(ROOT, "src/data/wild_encounters.json")))["wild_encounter_groups"][0]["encounters"]
    wild_maps = {}
    for e in wild:
        wild_maps.setdefault(e["map"], e)
    wtxt = open(os.path.join(os.path.dirname(STORY), "wild.txt")).read()
    wild_areas = set(re.findall(r"^\[(.+)\]$", wtxt, re.M))
    fly = {}
    hp = os.path.join(ROOT, "src/data/heal_locations.json")
    for h in json.load(open(hp))["heal_locations"]:
        if "HOUSE_2F" not in h["map"]:
            fly.setdefault(h["map"], (h["x"], h["y"]))
    hide_at_start = set()
    for lab in ("EventScript_ResetAllMapFlags", "EventScript_HideMapNamePopup"):
        hide_at_start |= set(re.findall(r"setflag (FLAG_HIDE_\w+)", "\n".join(SCRIPT.get(lab, []))))
    os.makedirs(os.path.join(WORLD, "maps"), exist_ok=True)
    out, missing_area, trainers_n, rematch_n = [], set(), 0, 0
    for mid, j in maps.items():
        folder, name = j["folder"], names[mid]
        lay = lays.get(LAYOUT_OVERRIDE.get(folder, j["layout"]))
        if not lay or "NULL" in (lay.get("primary_tileset"), lay.get("secondary_tileset")):
            continue
        img, over, cells = R.render_layout(ROOT, lay, found)
        W, H = lay["width"], lay["height"]
        if IMAGES and (not ONLY or folder in ONLY):
            img.save(os.path.join(WORLD, "maps", "em_" + folder + ".png"), optimize=True)
            if over.getbbox():
                over.save(os.path.join(WORLD, "maps", "em_" + folder + "_over.png"), optimize=True)
        we = wild_maps.get(mid)
        land = bool(we and "land_mons" in we)
        underwater = folder.startswith("Underwater") or folder.startswith("AbandonedShip_Underwater")
        grid = grid_of(cells, W, H, land, underwater)
        L = ["[map]", f"name = {name}", "region = Hoenn", f"image = maps/em_{folder}.png"]
        if over.getbbox():
            L.append(f"over = maps/em_{folder}_over.png")
        L.append("objects = sprites")
        mt = j.get("map_type", "")
        if mt in ("MAP_TYPE_INDOOR", "MAP_TYPE_UNDERGROUND", "MAP_TYPE_SECRET_BASE"):
            L.append("indoor = yes")
            if j.get("allow_cycling"):
                L.append("cycling = yes")   # (a cave you can ride a Bicycle in)
        if mt == "MAP_TYPE_UNDERWATER" or underwater:
            L.append("underwater = yes")
        if j.get("requires_flash"):
            L.append("dark = yes")
        wth = {"WEATHER_FOG_HORIZONTAL": "fog", "WEATHER_FOG_DIAGONAL": "fog", "WEATHER_SANDSTORM": "sand",
               "WEATHER_VOLCANIC_ASH": "ash", "WEATHER_RAIN": "rain", "WEATHER_SHADE": "shade",
               "WEATHER_UNDERWATER_BUBBLES": "bubbles"}.get(j.get("weather", ""))
        if wth:
            L.append(f"weather = {wth}")
        area = ""
        if we:
            area = wild_area(folder)
            if area not in wild_areas:
                missing_area.add((area, folder))
        L.append(f"wild = {area}")
        if mt in ("MAP_TYPE_TOWN", "MAP_TYPE_CITY"):
            L.append(f"town = {name}")
        if mid in fly:
            L.append(f"fly = {fly[mid][0]} {fly[mid][1]}")
        if j.get("allow_cycling") is False and mt not in ("MAP_TYPE_INDOOR",):
            pass
        dirs = {"up": "north", "down": "south", "left": "west", "right": "east"}
        for c in j.get("connections") or []:
            if c["map"] not in names:
                continue
            if c["direction"] in dirs:
                L.append(f"connect = {dirs[c['direction']]} {names[c['map']]} {-c['offset']}")
            elif c["direction"] in ("dive", "emerge"):
                L.append(f"{c['direction']} = {names[c['map']]}")
        if folder == "Underwater_SealedChamber":   # its map script: surfacing under the braille's "GO UP HERE" comes up inside
            L.append("emergeat = 12 44 %s|10|19" % names[by_folder["SealedChamber_OuterRoom"]])
        if folder in FIXED_DIVE:
            kind, tgt, tx, ty = FIXED_DIVE[folder]
            if tgt in by_folder:
                L = [l for l in L if not l.startswith(kind + " = ")]
                L.append(f"{kind} = {names[by_folder[tgt]]}|{tx}|{ty}")
        # doors and warps
        arrows = {MB[k]: v for k, v in (("MB_EAST_ARROW_WARP", (1, 0)), ("MB_WEST_ARROW_WARP", (-1, 0)), ("MB_NORTH_ARROW_WARP", (0, -1)),
                                        ("MB_SOUTH_ARROW_WARP", (0, 1)), ("MB_WATER_SOUTH_ARROW_WARP", (0, 1)), ("MB_DEEP_SOUTH_WARP", (0, 1)))}
        for wv in j.get("warp_events") or []:
            dest = wv["dest_map"]
            if dest not in maps:
                continue
            dj = maps[dest]
            try:
                dw = (dj.get("warp_events") or [])[int(wv["dest_warp_id"])]
            except (ValueError, IndexError):
                continue
            x, y = wv["x"], wv["y"]
            if not (0 <= x < W and 0 <= y < H):
                continue
            beh = cells[y * W + x][3]
            if beh in arrows:
                dx, dy = arrows[beh]
                x, y = x + dx, y + dy
            else:
                grid[wv["y"] * W + wv["x"]] = "D" if grid[wv["y"] * W + wv["x"]] in ".#" else grid[wv["y"] * W + wv["x"]]
            face = "down" if dj.get("map_type") not in ("MAP_TYPE_INDOOR", "MAP_TYPE_UNDERGROUND") else "up"
            L.append(f"door = {x} {y} warp|{names[dest]}|{dw['x']}|{dw['y']}|{face}")
        # holes in a cracked floor: down to the floor below, same spot (setholewarp: the Sky Pillar, Granite Cave...)
        hw = re.search(r"setholewarp (MAP_\w+)", open(os.path.join(MAPS, folder, "scripts.inc")).read()) if os.path.exists(os.path.join(MAPS, folder, "scripts.inc")) else None
        if hw and hw.group(1) in maps:
            for k, (_, coll, _, beh) in enumerate(cells):
                if beh == MB["MB_CRACKED_FLOOR_HOLE"]:
                    grid[k] = "."
                    L.append(f"door = {k % W} {k // W} warp|{names[hw.group(1)]}|{k % W}|{k // W}|down")
                elif beh == MB["MB_CRACKED_FLOOR"] and not coll:   # on foot it gives way; a bike's fast enough to cross
                    L.append(f"door = {k % W} {k // W} warp|{names[hw.group(1)]}|{k % W}|{k // W}|down")
                    L.append("unless = biking")
        # signs, hidden items, braille
        for b in j.get("bg_events") or []:
            if b["type"] == "sign":
                t = None
                lines = script_lines(b.get("script", ""))
                for l in lines:
                    m = re.match(r"msgbox (\w+)", l)
                    if m:
                        t = text_of(m.group(1), sign=True); break
                    m = re.match(r"braillemsgbox (\w+)", l)
                    if m:
                        t = "braille:" + (braille_of(m.group(1)) or "?"); break
                if t:
                    L.append(f"sign = {b['x']} {b['y']} {t}")
            elif b["type"] == "hidden_item" and b.get("item", "ITEM_NONE") != "ITEM_NONE" and item_name(b["item"]):
                L.append(f"item = {b['x']} {b['y']} {item_name(b['item'])} {b.get('quantity', 1) or 1} hidden")
        # people and things
        for o in j.get("object_events") or []:
            gfx = o.get("graphics_id", "0")
            x, y = o["x"], o["y"]
            if not (0 <= x < W and 0 <= y < H):
                continue
            flag = o.get("flag", "0")
            cond = []
            if flag.startswith("FLAG_HIDE_"):
                cond.append(f"if = show:{flag}" if flag in hide_at_start else f"unless = gone:{flag}")
            script = o.get("script", "")
            lines = script_lines(script)
            short = gfx.replace("OBJ_EVENT_GFX_", "")
            if gfx == "OBJ_EVENT_GFX_CUTTABLE_TREE":
                grid[y * W + x] = "T"; continue
            if gfx == "OBJ_EVENT_GFX_BREAKABLE_ROCK":
                grid[y * W + x] = "R"; continue
            if gfx == "OBJ_EVENT_GFX_PUSHABLE_BOULDER":
                grid[y * W + x] = "B"; continue
            if gfx == "OBJ_EVENT_GFX_BERRY_TREE":   # a berry tree: "berry = x y tree Berry" (Soil: nothing planted yet)
                t = o.get("trainer_sight_or_berry_tree_id", "")
                if t in TREE_ID:
                    L.append(f"berry = {x} {y} {TREE_ID[t]} {TREE_BERRY.get(t, 'Soil')}")
                    grid[y * W + x] = "#"
                continue
            if gfx == "OBJ_EVENT_GFX_ITEM_BALL":
                for l in lines:
                    m = re.match(r"finditem (ITEM_\w+)(?:,\s*(\d+))?", l)
                    if m and m.group(1) != "ITEM_NONE" and item_name(m.group(1)):
                        L.append(f"item = {x} {y} {item_name(m.group(1))} {m.group(2) or 1}")
                        L += cond
                        break
                continue
            if gfx == "OBJ_EVENT_GFX_TRICK_HOUSE_STATUE":   # a statue (in the way; Mossdeep Gym's and the Trick House's rotating tiles move them)
                L.append(f"npc = {x} {y} em_statue down 0 statue")
                L += cond
                continue
            if gfx in SKIP_GFX or short.startswith("VAR_"):
                continue
            face = face_of(o.get("movement_type", ""))
            sight = int(o.get("trainer_sight_or_berry_tree_id", "0") or 0) if o.get("trainer_type") == "TRAINER_TYPE_NORMAL" else 0
            if short in MON_GFX:
                sp = MON_GFX[short]
                act = f"legend|{sp}|{sp}: ...!" if sp in LEGENDS else None
                for l in lines:
                    m = re.match(r"setwildbattle SPECIES_(\w+), (\d+)", l)
                    if m and not act:
                        act = f"encounter|{species_name(m.group(1))}|{m.group(2)}|met:{folder}:{x},{y}|"
                if not act:
                    t = next((text_of(re.match(r"msgbox (\w+)", l).group(1)) for l in lines if re.match(r"msgbox (\w+)", l)), None)
                    act = "talk|" + (t or sp + ": ...!")
                L.append(f"npc = {x} {y} mon:{sp} {face if face != 'wander' else 'down'} 0 {act}")
                L += cond
                continue
            low = short.lower()
            sprite = SPRITE_ALIAS.get(low, "em_" + low)
            if sprite not in sprite_names:
                sprite = low if low in sprite_names else ("youngster" if "youngster" in sprite_names else (sprite_names[0] if sprite_names else "player"))
            act = None
            tb = next((l for l in lines if l.startswith("trainerbattle")), None)
            if tb:
                m = re.match(r"trainerbattle_\w+ (TRAINER_\w+), (\w+)(?:, (\w+))?", tb)
                if m and m.group(1) in TRAINERS:
                    who, team = trainer_line(m.group(1))
                    intro = text_of(m.group(2), "/") or "Let's battle!"
                    post, after = None, False
                    for l in lines:
                        if l.startswith("trainerbattle"):
                            after = True; continue
                        mm = re.match(r"msgbox (\w+)", l)
                        if after and mm:
                            post = text_of(mm.group(1), "/"); break
                    post = post or text_of(m.group(3) or "", "/") or "You're strong!"
                    act = f"fight|{who}|{team}|{intro}|{post}"
                    trainers_n += 1
                    if sight <= 0:
                        sight = 1
                    re_teams = rematch_teams(m.group(1))
                    if re_teams and team:
                        L.append(f"npc = {x} {y} {sprite} {face} {sight} {act}")
                        L += cond
                        L.append("rematch = " + " / ".join(re_teams))
                        rematch_n += 1
                        continue
            if not act and any("EventScript_PkmnCenterNurse" in l for l in lines):
                act = "heal|Welcome to our Pokemon Center!|We restore your tired Pokemon to full health."
            if not act and any(l.startswith("pokemart") for l in lines):
                act = "shop|Welcome! How may I serve you?"
            if not act:
                act = gift_of(lines)
            if not act:
                t = None
                for l in lines:
                    mm = re.match(r"msgbox (\w+)", l) or re.match(r"message (\w+)", l)
                    if mm and text_of(mm.group(1)):
                        t = text_of(mm.group(1)); break
                act = "talk|" + (t or "...")
            L.append(f"npc = {x} {y} {sprite} {face} {sight} {act}")
            L += cond
        if folder == "LittlerootTown_BrendansHouse_2F":
            L.append("start = 4 2")
        L.append("grid:")
        for yy in range(H):
            L.append("".join(grid[yy * W:(yy + 1) * W]))
        out.append("\n".join(L))
    head = [
        "# Hoenn, walked: made by tools/emerald_world.py from the pret/pokeemerald decompilation",
        "# (do not edit by hand - edit World/patches_hoenn.txt and run the tool again).",
        "# Grid as in world.txt, and: d deep water (Surf; Dive here)  u under the sea, where you can surface",
        "# Map keys: dive = Map (same spot) or Map|x|y   emerge = the same, from under the sea   underwater = yes",
        "# npc lines may be followed by \"rematch = team / team\": Match Call's stronger teams for that trainer",
        "",
        "walked = Hoenn",
    ] + ["story_town = %s | %s" % st for st in STORY_TOWN] + [""]
    text = "\n".join(head) + "\n" + "\n\n".join(out) + "\n"
    patch = os.path.join(WORLD, "patches_hoenn.txt")
    if os.path.exists(patch):
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        text = apply_patches(text, open(patch, encoding="utf-8").read())
    open(os.path.join(WORLD, "hoenn.txt"), "w", encoding="utf-8").write(text)
    print(len(out), "maps;", trainers_n, "trainers,", rematch_n, "with Match Call rematches")
    if missing_area:
        print("wild areas not in wild.txt:", sorted(missing_area))
    if UNKNOWN_ITEMS:
        print("items the game doesn't have yet:", ", ".join(sorted(UNKNOWN_ITEMS)))


# ------------------------------------------------------------------ braille
BRAILLE = {}
def braille_of(label):
    """the braille message as letters (data/text/braille.inc spells them with the braille font's codes)"""
    raw = TEXT.get(label)
    if raw is None:
        return None
    s = re.sub(r"\{[A-Z_0-9 ]+\}", "", raw).replace("\\n", "~").replace("\\p", "~").replace("$", "")
    return re.sub(r" *~ *", "~", re.sub(r"[ \t]+", " ", s)).strip("~ ")


def apply_patches(text, patch):
    import importlib.util
    spec = importlib.util.spec_from_file_location("frlg_patch", os.path.join(os.path.dirname(os.path.abspath(__file__)), "patchlib.py"))
    lib = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(lib)
    return lib.apply_patches(text, patch)


if __name__ == "__main__":
    main()
