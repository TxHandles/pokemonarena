#!/usr/bin/env python3
"""Builds the walked world (World/world.txt and World/maps/*.png) from the pret/pokefirered decompilation:
every Kanto and Sevii Islands map with its picture, walls, grass, water, ledges, doors, connections, signs,
people (and what they say), route trainers and their FireRed teams, item balls, hidden items, Cut trees,
rocks and boulders, the Pokemon met on the map and the legendaries.

    frlg_world.py <pokefirered dir> <World dir> <story.txt> [--no-images]

World/patches.txt (hand-written) is merged in: the story's own battles, gifts, gates and anything the
FireRed scripts do that a line of world.txt can't say. See that file's header."""
import json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frlg_render

ROOT, WORLD, STORY = sys.argv[1], sys.argv[2], sys.argv[3]
IMAGES = "--no-images" not in sys.argv
MAPS = os.path.join(ROOT, "data/maps")

SKIP = re.compile(r"^(BattleColosseum|RecordCorner|TradeCenter|UnionRoom|Prototype|NavelRock|BirthIsland)|_PokemonCenter_2F$|Unused")

# ------------------------------------------------------------------ names
ISLANDS = ["OneIsland", "TwoIsland", "ThreeIsland", "FourIsland", "FiveIsland", "SixIsland", "SevenIsland"]
KEEP_PREFIX = re.compile(r"^(House\d*|Mart|PokemonCenter_1F|Harbor|Gym|PokemonDayCare|LoreleisHouse|ResortGorgeous_House|House_Room\d)$")
OVERRIDE = {
    "PalletTown_PlayersHouse_1F": "Your House 1F", "PalletTown_PlayersHouse_2F": "Your House 2F",
    "PalletTown_RivalsHouse": "Blue's House", "PalletTown_ProfessorOaksLab": "Oak's Lab",
    "IndigoPlateau_Exterior": "Indigo Plateau", "IndigoPlateau_PokemonCenter_1F": "Indigo Plateau Pokemon Center",
    "ThreeIsland_Port": "Three Isle Port", "FiveIsland_Meadow": "Five Isle Meadow", "SSAnne_Exterior": "S.S. Anne",
    "MtMoon_1F": "Mt. Moon 1F", "MtMoon_B1F": "Mt. Moon B1F", "MtMoon_B2F": "Mt. Moon B2F",
    "Route21_North": "Route 21 North", "Route21_South": "Route 21 South",
    "SaffronCity_Connection": "Saffron City Gates", "SevenIsland_TrainerTower": "Trainer Tower Grounds",
    "TrainerTower_Lobby": "Trainer Tower", "PokemonLeague_LoreleisRoom": "Lorelei's Room",
    "PokemonLeague_BrunosRoom": "Bruno's Room", "PokemonLeague_AgathasRoom": "Agatha's Room",
    "PokemonLeague_LancesRoom": "Lance's Room", "PokemonLeague_ChampionsRoom": "Champion's Room",
    "PokemonLeague_HallOfFame": "Hall of Fame",
}
WORD = {"Pokemon": "Pokemon", "Mart": "Poke Mart", "SS": "S.S.", "Mt": "Mt.", "Digletts": "Diglett's", "Wardens": "Warden's",
        "Copycats": "Copycat's", "Loreleis": "Lorelei's", "Bills": "Bill's"}


def words(s):
    s = re.sub(r"(?<=[a-z])(?=[A-Z0-9])|(?<=[0-9])(?=[A-Z][a-z])|(?<=[A-Z])(?=[A-Z][a-z])", " ", s)
    return " ".join(WORD.get(w, w) for w in s.split())


def pretty(folder):
    if folder in OVERRIDE:
        return OVERRIDE[folder]
    parts = folder.split("_")
    if parts[0] in ISLANDS and len(parts) > 1 and not KEEP_PREFIX.match("_".join(parts[1:])):
        parts = parts[1:]
    out = " ".join(words(p) for p in parts)
    out = out.replace("Pokemon Center 1F", "Pokemon Center").replace(" Mart", " Poke Mart").replace("Poke Poke", "Poke")
    out = re.sub(r"Route (\d+)", r"Route \1", out)
    return out


# ------------------------------------------------------------------ text
def clean_text(s):
    s = s.replace("{RIVAL}", "Blue").replace("{PLAYER}", "@P@")
    s = re.sub(r"\{[A-Z_0-9]+\}", "", s)
    s = s.replace("POKéMON", "Pokemon").replace("POKé", "Poke").replace("Pokémon", "Pokemon").replace("Poké", "Poke")
    s = s.replace("é", "e").replace("…", "...").replace("“", '"').replace("”", '"').replace("’", "'").replace("‘", "'")
    s = s.replace("♂", " M").replace("♀", " F")
    def cap(m):
        w = m.group(0)
        if w in ("TM", "HM", "PP", "HP", "OK", "PC", "TV", "S.S.", "EXP", "ID", "KO", "VS", "UFO", "GO"):
            return w
        return w.capitalize() if len(w) > 1 else w
    s = re.sub(r"\b[A-Z][A-Z']+\b", cap, s)
    s = s.replace("@P@", "{PLAYER}").replace("@p@", "{PLAYER}")
    return s


TEXT = {}
def load_texts():
    files = []
    for d in os.listdir(MAPS):
        p = os.path.join(MAPS, d, "text.inc")
        if os.path.exists(p):
            files.append(p)
    for base, _, fs in os.walk(os.path.join(ROOT, "data/text")):
        files += [os.path.join(base, f) for f in fs if f.endswith(".inc")]
    for p in files:
        cur = None
        for line in open(p, encoding="utf-8", errors="replace"):
            m = re.match(r"^(\w+)::", line)
            if m:
                cur = m.group(1); TEXT[cur] = ""; continue
            m = re.match(r'\s*\.string\s+"(.*)"', line)
            if m and cur:
                TEXT[cur] += m.group(1)


def text_of(label, sep="|", sign=False):
    """a message's pages joined by sep ("/" inside an action whose fields are split by "|");
    a sign's title line (PALLET TOWN) is a page of its own"""
    raw = TEXT.get(label)
    if raw is None:
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
            m = re.match(r"^(\w+)::", line)
            if m:
                cur = m.group(1); SCRIPT[cur] = []; continue
            if cur and line.strip():
                SCRIPT[cur].append(line.strip())


def script_lines(label, depth=0):
    """the script's lines, following goto/call into its own map's helpers a little"""
    out = []
    for l in SCRIPT.get(label, []):
        out.append(l)
        m = re.match(r"(?:goto|call)\s+(\w+)$", l) or re.match(r"(?:goto|call)_if_\w+ [^,]+, (?:[^,]+, )?(\w+)$", l)
        if m and depth < 2 and not m.group(1).startswith("EventScript_"):
            out += script_lines(m.group(1), depth + 1)
        if l in ("end", "return", "releaseall", "release") and depth == 0:
            pass
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
    for chunk in ps.split("static const struct")[1:]:   # one party each
        m = re.search(r"(sParty_\w+)\[\]", chunk)
        if not m:
            continue
        mons = re.findall(r"\.lvl\s*=\s*(\d+),\s*\n\s*\.species\s*=\s*SPECIES_(\w+)", chunk)
        PARTIES[m.group(1)] = [(species_name(s), int(l)) for l, s in mons]
    cn = open(os.path.join(ROOT, "src/data/text/trainer_class_names.h")).read()
    for k, v in re.findall(r"\[(TRAINER_CLASS_\w+)\]\s*=\s*_\(\"(.*?)\"\)", cn):
        CLASSNAMES[k] = clean_text(v.replace("{PKMN}", "PKMN"))


SPECIAL_SPECIES = {"NIDORAN_F": "Nidoran F", "NIDORAN_M": "Nidoran M", "MR_MIME": "Mr. Mime", "FARFETCHD": "Farfetch'd",
                   "HO_OH": "Ho-Oh", "MIME_JR": "Mime Jr.", "UNOWN": "Unown", "PORYGON2": "Porygon2"}
def species_name(s):
    if s in SPECIAL_SPECIES:
        return SPECIAL_SPECIES[s]
    return "-".join(p.capitalize() for p in s.split("_")) if "_" in s else s.capitalize()


def trainer_line(tid):
    cls, name, party, double = TRAINERS[tid]
    who = (CLASSNAMES.get(cls, words(cls.replace("TRAINER_CLASS_", "").title().replace("_", ""))) + " " + clean_text(name).title()).strip()
    team = ", ".join(f"{s} {l}" for s, l in PARTIES.get(party, []))
    return who, team


# ------------------------------------------------------------------ items
ITEM_NAMES = {}
FR_TMS = ["Focus Punch", "Dragon Claw", "Water Pulse", "Calm Mind", "Roar", "Toxic", "Hail", "Bulk Up", "Bullet Seed", "Hidden Power",
          "Sunny Day", "Taunt", "Ice Beam", "Blizzard", "Hyper Beam", "Light Screen", "Protect", "Rain Dance", "Giga Drain", "Safeguard",
          "Frustration", "Solar Beam", "Iron Tail", "Thunderbolt", "Thunder", "Earthquake", "Return", "Dig", "Psychic", "Shadow Ball",
          "Brick Break", "Double Team", "Reflect", "Shock Wave", "Flamethrower", "Sludge Bomb", "Sandstorm", "Fire Blast", "Rock Tomb", "Aerial Ace",
          "Torment", "Facade", "Secret Power", "Rest", "Attract", "Thief", "Steel Wing", "Skill Swap", "Snatch", "Overheat"]
TM_STANDIN = {"Hail": "Snowscape", "Frustration": "Lash Out", "Return": "Body Slam", "Double Team": "Substitute",
              "Torment": "Spite", "Secret Power": "Swift", "Attract": "Charm", "Steel Wing": "Dual Wingbeat", "Snatch": "Trick"}
FR_HMS = ["HM01 Cut", "HM02 Fly", "HM03 Surf", "HM04 Strength", "HM05 Flash", "HM06 Rock Smash", "HM07 Waterfall"]
OURS = set()
OUR_TM = {}
UNKNOWN_ITEMS = set()
def load_our_items():
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
    it = open(os.path.join(src, "items.cpp")).read()
    OURS.update(re.findall(r'(?:add|med|bx|pu|val)\("([^"]+)"', it))
    OURS.update(re.findall(r'\{"([^"]+)", Type::', it))   # the type boosters
    OURS.update(["Razz Berry", "Bluk Berry", "Nanab Berry", "Wepear Berry", "Pinap Berry"])
    for no, mv in re.findall(r'\{(\d+), "([^"]+)"', open(os.path.join(src, "tms.inc")).read()):
        OUR_TM[mv] = "TM%03d %s" % (int(no), mv)
    fr = json.load(open(os.path.join(ROOT, "src/data/items.json")))["items"]
    for x in fr:
        ITEM_NAMES[x["itemId"].replace("ITEM_", "")] = x["english"]


def item_name(it):
    k = it.replace("ITEM_", "")
    m = re.match(r"(TM|HM)(\d+)", k)
    if m:
        n = int(m.group(2))
        if m.group(1) == "HM":
            return FR_HMS[n - 1]
        mv = FR_TMS[n - 1] if n <= len(FR_TMS) else ""
        mv = TM_STANDIN.get(mv, mv)   # FireRed TMs that aren't TMs any more: the nearest one that is
        if mv in OUR_TM:
            return OUR_TM[mv]
        UNKNOWN_ITEMS.add("TM%02d %s" % (n, mv))
        return "TM%02d %s" % (n, mv)
    special = {"POKE_BALL": "Poke Ball", "X_SP_ATK": "X Sp. Atk", "PP_UP": "PP Up", "PP_MAX": "PP Max", "HP_UP": "HP Up",
               "TINY_MUSHROOM": "Tiny Mushroom", "SS_TICKET": "S.S. Ticket", "TM_CASE": "TM Case", "EXP_SHARE": "Exp. All",
               "KINGS_ROCK": "King's Rock", "OAKS_PARCEL": "Oak's Parcel", "POKE_FLUTE": "Poke Flute", "SILPH_SCOPE": "Silph Scope",
               "PARALYZE_HEAL": "Paralyze Heal", "TRI_PASS": "Tri-Pass", "VS_SEEKER": "VS Seeker", "TEACHY_TV": "Teachy TV",
               "BICYCLE": "Bicycle", "BIKE_VOUCHER": "Bike Voucher", "GOLD_TEETH": "Gold Teeth"}
    if k in special:
        name = special[k]
    else:
        e = ITEM_NAMES.get(k, k.replace("_", " "))
        name = " ".join(w.capitalize() if not re.match(r"^[A-Z]{1,2}$", w) or w in ("UP",) else w for w in clean_text(e).replace("é", "e").split())
        name = name.replace("Pokeball", "Poke Ball").replace("Poke ball", "Poke Ball")
    name = {"Thunderstone": "Thunder Stone", "Blackglasses": "Black Glasses", "Nevermeltice": "Never-Melt Ice",
            "Up-grade": "Up-Grade", "Guard Spec": "Guard Spec."}.get(name, name)
    low = {n.lower(): n for n in OURS}
    if name.lower() in low:
        return low[name.lower()]
    UNKNOWN_ITEMS.add(name)
    return name


# ------------------------------------------------------------------ sprites
def load_sprite_names():
    p = os.path.join(WORLD, "chars.txt")
    return open(p).read().split() if os.path.exists(p) else []


MON_GFX = {"SNORLAX", "VOLTORB", "MOLTRES", "ZAPDOS", "ARTICUNO", "MEWTWO", "LUGIA", "HO_OH", "PIKACHU", "CLEFAIRY", "DODUO",
           "PSYDUCK", "NIDORAN_F", "NIDORAN_M", "JIGGLYPUFF", "FEAROW", "PIDGEOT", "PIDGEY", "SEEL", "SLOWBRO", "SLOWPOKE",
           "MEOWTH", "WIGGLYTUFF", "CHANSEY", "POLIWRATH", "MACHOP", "LAPRAS", "KANGASKHAN", "NIDORINO", "CUBONE", "MACHOKE",
           "SPEAROW", "DEOXYS", "MEW"}
LEGENDS = {"MOLTRES", "ZAPDOS", "ARTICUNO", "MEWTWO", "LUGIA", "HO_OH"}
SKIP_GFX = {"OBJ_EVENT_GFX_SEAGALLOP", "OBJ_EVENT_GFX_SS_ANNE", "OBJ_EVENT_GFX_TOWN_MAP", "OBJ_EVENT_GFX_POKEDEX",
            "OBJ_EVENT_GFX_CLIPBOARD", "OBJ_EVENT_GFX_FOSSIL", "OBJ_EVENT_GFX_METEORITE", "OBJ_EVENT_GFX_OLD_AMBER",
            "OBJ_EVENT_GFX_LAPRAS_DOLL", "OBJ_EVENT_GFX_RUBY", "OBJ_EVENT_GFX_SAPPHIRE", "0"}


def gift_of(lines):
    """someone who hands you something once: (goto_if_set FLAG, Already;) msgbox intro; giveitem ITEM; setflag FLAG"""
    ITEMLINE = r"(?:giveitem|checkitemspace|additem|giveitem_msg \w+,) (ITEM_\w+)(?:, (\d+))?"
    item = next((re.match(ITEMLINE, l) for l in lines if re.match(ITEMLINE, l)), None)
    setf = next((re.match(r"setflag (FLAG_\w+)", l).group(1) for l in lines if re.match(r"setflag FLAG_(GOT|RECEIVED)\w*", l)), None)
    if not item or not setf or "removeitem" in " ".join(lines):
        return None
    flag = next((re.match(r"goto_if_set (\w+), (\w+)", l) for l in lines if re.match(r"goto_if_set " + setf + r", (\w+)", l)), None)
    already = set(SCRIPT.get(flag.group(2), [])) if flag else set()
    already |= {l for l in lines if re.match(r"msgbox \w+Text_(No|Oh|Too|Come|DidntCome|HowAre)", l)}
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
    if not after:   # what they say after handing it over
        seen = False
        for l in lines:
            if re.match(ITEMLINE, l): seen = True; continue
            m = re.match(r"msgbox (\w+)", l)
            if seen and m and text_of(m.group(1)):
                after = text_of(m.group(1), "/"); break
    name = item_name(item.group(1))
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


# ------------------------------------------------------------------ the grid
WATER = {0x10, 0x11, 0x12, 0x14, 0x15, 0x1B, 0x50, 0x51, 0x52, 0x53}
SPIN = {0x56: "1", 0x57: "2", 0x55: "3", 0x54: "4", 0x58: "0"}
LEDGE = {0x3B: "v", 0x38: ">", 0x39: "<"}
ARROW = {0x65: (0, 1), 0x64: (0, -1), 0x62: (1, 0), 0x63: (-1, 0), 0x6C: (1, 0), 0x6E: (1, 0), 0x6D: (-1, 0), 0x6F: (-1, 0)}


def grid_of(cells, W, H, attrs_enc, land):
    g = []
    for k, (mid, coll, elev, beh) in enumerate(cells):
        c = "#" if coll else "."
        if beh == 0x13:
            c = "W"
        elif beh in WATER and not coll:
            c = "w"
        elif beh in LEDGE:
            c = LEDGE[beh]
        elif beh == 0x3A:
            c = "#"
        elif beh in SPIN and not coll:
            c = SPIN[beh]
        elif not coll and (beh in (0x02, 0xD1)):
            c = "g"
        elif not coll and land and attrs_enc[k] == 1:
            c = "c"
        g.append(c)
    return g


# ------------------------------------------------------------------ main
def main():
    load_texts(); load_scripts(); load_trainers(); load_our_items()
    sprite_names = load_sprite_names()
    found = frlg_render.find_tilesets(ROOT)
    lays = {l["id"]: l for l in json.load(open(os.path.join(ROOT, "data/layouts/layouts.json")))["layouts"] if "width" in l}
    maps = {}
    for d in sorted(os.listdir(MAPS)):
        p = os.path.join(MAPS, d, "map.json")
        if os.path.exists(p) and not SKIP.search(d):
            j = json.load(open(p))
            j["folder"] = d
            maps[j["id"]] = j
    names = {mid: pretty(j["folder"]) for mid, j in maps.items()}
    dup = {}
    for mid, n in names.items():
        dup.setdefault(n, []).append(mid)
    for n, ids in dup.items():
        if len(ids) > 1:
            for mid in ids:
                names[mid] = words(maps[mid]["folder"].replace("_", " "))
    # wild areas: wild.txt's areas by name; maps with their own tables that no area matches are listed
    wild = json.load(open(os.path.join(ROOT, "src/data/wild_encounters.json")))["wild_encounter_groups"][0]["encounters"]
    wild_maps = {}
    for e in wild:
        wild_maps.setdefault(e["map"], e)
    wtxt = open(os.path.join(os.path.dirname(STORY), "wild.txt")).read()
    wtxt = wtxt.split("# ---- generated by frlg_world.py")[0]   # (not the areas this tool wrote last time)
    wild_areas = re.findall(r"^\[(.+)\]$", wtxt, re.M)
    # where Fly lands (in front of each town's Pokemon Center: the heal locations)
    fly = {}
    hp = os.path.join(ROOT, "src/data/heal_locations.json")
    if os.path.exists(hp):
        for h in json.load(open(hp))["heal_locations"]:
            fly.setdefault(h["map"], (h["x"], h["y"]))
    hide_at_start = set(re.findall(r"setflag (FLAG_HIDE_\w+)", "\n".join(SCRIPT.get("EventScript_ResetAllMapFlags", []))))
    os.makedirs(os.path.join(WORLD, "maps"), exist_ok=True)
    out = []
    unmatched_wild = []
    for mid, j in maps.items():
        folder, name = j["folder"], names[mid]
        lay = lays.get(j["layout"])
        if not lay or "NULL" in (lay.get("primary_tileset"), lay.get("secondary_tileset")):
            continue
        img, over, cells = frlg_render.render_layout(ROOT, lay, found)
        W, H = lay["width"], lay["height"]
        if IMAGES:
            img.save(os.path.join(WORLD, "maps", folder + ".png"), optimize=True)
            if over.getbbox():
                over.save(os.path.join(WORLD, "maps", folder + "_over.png"), optimize=True)
        prim = frlg_render.tileset(ROOT, lay["primary_tileset"], found)
        sec = frlg_render.tileset(ROOT, lay["secondary_tileset"], found)
        enc = []
        for (m_, c_, e_, b_) in cells:
            _, attr = frlg_render.metatile_info(prim, sec, m_)
            enc.append(attr >> 24 & 7)
        we = wild_maps.get(mid)
        land = bool(we and "land_mons" in we)
        grid = grid_of(cells, W, H, enc, land)
        L = []   # the map's lines
        L.append("[map]")
        L.append(f"name = {name}")
        L.append(f"image = maps/{folder}.png")
        if over.getbbox():
            L.append(f"over = maps/{folder}_over.png")
        L.append("objects = sprites")
        mt = j.get("map_type", "")
        indoor = mt in ("MAP_TYPE_INDOOR", "MAP_TYPE_UNDERGROUND", "MAP_TYPE_SECRET_BASE")
        if indoor:
            L.append("indoor = yes")
            if j.get("allow_cycling"):
                L.append("cycling = yes")   # (a cave you can ride a Bicycle in)
        if j.get("requires_flash"):
            L.append("dark = yes")
        wth = {"WEATHER_RAIN": "rain", "WEATHER_SNOW": "snow", "WEATHER_FOG_HORIZONTAL": "fog", "WEATHER_FOG_DIAGONAL": "fog", "WEATHER_SANDSTORM": "sand"}.get(j.get("weather", ""))
        if wth:
            L.append(f"weather = {wth}")
        area = ""
        if we:
            base = re.sub(r"( \d?B?\d+F)?( Stairs)?$", "", re.sub(r" (Entrance|Back|Room ?\d+|North|South|East|West|Center)$", "", name))
            alias = {"Icefall Cave Entrance": "Icefall Cave (Entrance)", "Icefall Cave 1F": "Icefall Cave (Inner)",
                     "Icefall Cave B1F": "Icefall Cave (Inner)", "Icefall Cave Back": "Icefall Cave (Back)"}
            for cand in (alias.get(name, ""), name, base, base.replace("Sea ", ""), "Sea " + name, "Sea " + base):
                if cand in wild_areas:
                    area = cand; break
            if not area:
                area = base
                unmatched_wild.append((base, mid))
        L.append(f"wild = {area}")
        if mt in ("MAP_TYPE_TOWN", "MAP_TYPE_CITY"):
            L.append(f"town = {name}")
        if mid in fly:
            L.append(f"fly = {fly[mid][0]} {fly[mid][1]}")
        if folder == "Route17":
            L.append("bike = yes")
        dirs = {"up": "north", "down": "south", "left": "west", "right": "east"}
        for c in j.get("connections") or []:
            if c["direction"] in dirs and c["map"] in names:
                L.append(f"connect = {dirs[c['direction']]} {names[c['map']]} {-c['offset']}")
        # doors and warps
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
            beh = cells[y * W + x][3] if 0 <= x < W and 0 <= y < H else 0
            if beh in ARROW:   # a mat or stairs you step off: the door is the next tile that way
                dx, dy = ARROW[beh]
                x, y = x + dx, y + dy
            else:
                grid[wv["y"] * W + wv["x"]] = "D" if grid[wv["y"] * W + wv["x"]] in ".#" else grid[wv["y"] * W + wv["x"]]
            dl = lays.get(dj["layout"], {})
            dbeh = 0
            if dl:
                try:
                    dcells_beh = None
                except Exception:
                    pass
            face = "down" if dj.get("map_type") not in ("MAP_TYPE_INDOOR", "MAP_TYPE_UNDERGROUND") else "up"
            L.append(f"door = {x} {y} warp|{names[dest]}|{dw['x']}|{dw['y']}|{face}")
        # signs and hidden items
        for b in j.get("bg_events") or []:
            if b["type"] == "sign":
                t = None
                for l in script_lines(b["script"]):
                    m = re.match(r"msgbox (\w+)", l)
                    if m:
                        t = text_of(m.group(1), sign=True); break
                if t:
                    L.append(f"sign = {b['x']} {b['y']} {t}")
            elif b["type"] == "hidden_item" and b["item"] != "ITEM_NONE":
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
            if gfx == "OBJ_EVENT_GFX_CUT_TREE":
                grid[y * W + x] = "T"; continue
            if gfx == "OBJ_EVENT_GFX_ROCK_SMASH_ROCK":
                grid[y * W + x] = "R"; continue
            if gfx == "OBJ_EVENT_GFX_PUSHABLE_BOULDER":
                grid[y * W + x] = "B"; continue
            if gfx in ("OBJ_EVENT_GFX_RUBY", "OBJ_EVENT_GFX_SAPPHIRE"):   # the Network Machine's gems, lying on the floor
                L.append(f"item = {x} {y} {'Ruby' if 'RUBY' in gfx else 'Sapphire'} 1")
                continue
            if gfx == "OBJ_EVENT_GFX_ITEM_BALL":
                for l in lines:
                    m = re.match(r"finditem (ITEM_\w+)(?:,\s*(\d+))?", l)
                    if m and m.group(1) != "ITEM_NONE":
                        L.append(f"item = {x} {y} {item_name(m.group(1))} {m.group(2) or 1}")
                        L += cond
                        break
                continue
            if gfx in SKIP_GFX or short.startswith("VAR_"):
                continue
            face = face_of(o.get("movement_type", ""))
            sight = int(o.get("trainer_sight_or_berry_tree_id", "0") or 0) if o.get("trainer_type") == "TRAINER_TYPE_NORMAL" else 0
            if short in MON_GFX:
                sprite = "mon:" + species_name(short)
                act = None
                if short in LEGENDS:
                    act = f"legend|{species_name(short)}|{species_name(short)}: ...!"
                for l in lines:
                    m = re.match(r"setwildbattle SPECIES_(\w+), (\d+)", l)
                    if m and not act:
                        act = f"encounter|{species_name(m.group(1))}|{m.group(2)}|met:{folder}:{x},{y}|"
                if not act:
                    t = next((text_of(re.match(r"msgbox (\w+)", l).group(1)) for l in lines if re.match(r"msgbox (\w+)", l)), None)
                    act = "talk|" + (t or species_name(short) + ": ...!")
                L.append(f"npc = {x} {y} {sprite} {face if face != 'wander' else 'down'} 0 {act}")
                L += cond
                continue
            sprite = short.lower()
            if sprite not in sprite_names:
                sprite = "youngster" if "youngster" in sprite_names else (sprite_names[0] if sprite_names else "player")
            act = None
            tb = next((l for l in lines if l.startswith("trainerbattle")), None)
            if tb:
                m = re.match(r"trainerbattle_\w+ (TRAINER_\w+), (\w+)(?:, (\w+))?", tb)
                if m and m.group(1) in TRAINERS:
                    who, team = trainer_line(m.group(1))
                    intro = text_of(m.group(2), "/") or "Let's battle!"
                    post = None
                    after = False
                    for l in lines:
                        if l.startswith("trainerbattle"):
                            after = True; continue
                        mm = re.match(r"msgbox (\w+)", l)
                        if after and mm:
                            post = text_of(mm.group(1), "/"); break
                    post = post or text_of(m.group(3) or "", "/") or "You're strong!"
                    act = f"fight|{who}|{team}|{intro}|{post}|{m.group(1)}"
                    if sight <= 0:
                        sight = 1
            if not act and any("EventScript_PkmnCenterNurse" in l for l in lines):
                act = "heal|Welcome to our Pokemon Center!|We restore your tired Pokemon to full health."
            if not act and any(l.startswith("pokemart") for l in lines):
                act = "shop|Hi there! May I help you?"
            if not act:
                act = gift_of(lines)
            if not act:
                t = None
                for l in lines:
                    mm = re.match(r"msgbox (\w+)", l) or re.match(r"message (\w+)", l)
                    if mm and text_of(mm.group(1)):
                        t = text_of(mm.group(1)); break
                act = "talk|" + (t or "...")
            if act.startswith("fight|"):
                parts = act.split("|")
                act = "|".join(parts[:5])   # name|team|before|after (the flag comes from the name)
            L.append(f"npc = {x} {y} {sprite} {face} {sight} {act}")
            L += cond
        if mid == "MAP_PALLET_TOWN_PLAYERS_HOUSE_2F":
            L.append("start = 6 6")
        L.append("grid:")
        for yy in range(H):
            L.append("".join(grid[yy * W:(yy + 1) * W]))
        out.append("\n".join(L))
    head = [
        "# The walked world: Kanto and the Sevii Islands, made by tools/frlg_world.py from the pret/pokefirered",
        "# decompilation (do not edit by hand - edit World/patches.txt and run the tool again).",
        "# Grid: . walk  # solid  g tall grass  c cave floor (wild Pokemon)  v < > ledges  w water (Surf)  W waterfall",
        "#       T tree to Cut  R rock to smash  B boulder (Strength)  1-4 spin tiles (up, down, left, right)  0 spin stop  D door",
        "# door = x y action (walk into it)   sign/spot = x y (face it, press Enter)",
        "# npc = x y sprite facing sight action   (sight > 0: a trainer who walks up when you pass)",
        "#   actions: talk|text  trainer|Story stage|before|after  fight|Name|Pidgey 9, Rattata 8|before|after",
        "#            give|text/more text|Item|count|flag|after  heal|text  shop|text",
        "#   legend|Species|cry text (npc sprite \"mon:Species\": gone once caught)  raid (the Raid Den)",
        "#   tower (the Battle Tower)  trashcan|flag  quiz|question|yes or no|flag|trainer  exchange|give item|get item|text",
        "#   encounter|Species|level|flag|text|item needed|text without it  ferry|text",
        "# item = x y Name [count] [hidden]      gate = x y flag (solid until the flag is set)",
        "# if = flag / unless = flag   (on the line after an npc, door, spot or item: only while...)",
        "#   flags: anything set by \"flag|name\" or a gift, \"badge:Cascade\", \"stage:Cerulean Gym\", \"item:Bicycle\", \"!\" = not",
        "# map keys: indoor  dark (Flash)  bike (Cycling Road)  town  fly = x y  weather  over (drawn over people)  objects = sprites",
        "sprites = " + " ".join(sprite_names),
    ]
    text = "\n".join(head) + "\n\n" + "\n\n".join(out) + "\n"
    patch = os.path.join(WORLD, "patches.txt")
    if os.path.exists(patch):
        text = apply_patches(text, open(patch).read())
    open(os.path.join(WORLD, "world.txt"), "w").write(text)
    write_new_wild(unmatched_wild, wild)
    print(len(out), "maps;", "wild tables not in wild.txt (now generated):", len(unmatched_wild))
    print("items the game doesn't have yet:", ", ".join(sorted(UNKNOWN_ITEMS)))
    for n, m in unmatched_wild:
        print("   ", n, m)


SLOTS = {"land_mons": ("walk", [20, 20, 10, 10, 10, 10, 5, 5, 4, 4, 1, 1]), "water_mons": ("surf", [60, 30, 5, 4, 1]),
         "rock_smash_mons": ("rock-smash", [60, 30, 5, 4, 1])}
FISH = [("old-rod", 70), ("old-rod", 30), ("good-rod", 60), ("good-rod", 20), ("good-rod", 20),
        ("super-rod", 40), ("super-rod", 40), ("super-rod", 15), ("super-rod", 4), ("super-rod", 1)]


def write_new_wild(unmatched, encounters):
    """wild.txt gets the areas FireRed has that it doesn't (the Sevii Islands...), both versions merged,
    between "# ---- generated by frlg_world.py" markers (replaced on every run)"""
    path = os.path.join(os.path.dirname(STORY), "wild.txt")
    txt = open(path).read()
    START, END = "# ---- generated by frlg_world.py: FireRed / LeafGreen areas (Sevii Islands)", "# ---- end of generated areas"
    if START in txt:
        txt = txt[:txt.index(START)].rstrip("\n") + "\n" + txt[txt.index(END) + len(END):].lstrip("\n")
    areas = {}
    by_map = {}
    for e in encounters:
        by_map.setdefault(e["map"], []).append(e)
    for area, mid in unmatched:
        rates = areas.setdefault(area, {})
        versions = by_map.get(mid, [])
        for e in versions:
            share = 1.0 / len(versions)
            for kind, (method, slot) in SLOTS.items():
                for k, mon in enumerate((e.get(kind) or {}).get("mons", [])):
                    key = (method, species_name(mon["species"].replace("SPECIES_", "")))
                    lo, hi, r = rates.get(key, (99, 0, 0))
                    rates[key] = (min(lo, mon["min_level"]), max(hi, mon["max_level"]), r + slot[k] * share)
            for k, mon in enumerate((e.get("fishing_mons") or {}).get("mons", [])):
                method, r0 = FISH[k]
                key = (method, species_name(mon["species"].replace("SPECIES_", "")))
                lo, hi, r = rates.get(key, (99, 0, 0))
                rates[key] = (min(lo, mon["min_level"]), max(hi, mon["max_level"]), r + r0 * share)
    lines = [START, "# The Sevii Islands open once you're the Champion. Rates: FireRed and LeafGreen averaged."]
    for area, rates in areas.items():
        if not rates:
            continue
        lines += ["", f"[{area}]", "unlock = Champion's Stadium"]
        tot = {}
        for (method, sp), (lo, hi, r) in rates.items():
            tot[method] = tot.get(method, 0) + r
        for (method, sp), (lo, hi, r) in sorted(rates.items(), key=lambda kv: (kv[0][0], -kv[1][2])):
            pct = max(1, round(r * 100 / tot[method]))
            lines.append(f"pokemon = {method}, {sp}, {pct}, {lo}-{hi}")
    lines.append(END)
    open(path, "w").write(txt.rstrip("\n") + "\n\n" + "\n".join(lines) + "\n")


def apply_patches(text, patch):
    """patches.txt: top-level lines go to the top; "[patch Map Name]" blocks add lines to that map, and
    "drop npc x y" / "drop sign x y" / "drop door x y" / "drop item x y" / "set key = value" change it."""
    blocks = re.split(r"\n(?=\[map\])", text)
    top_lines, cur, edits = [], None, {}
    for line in patch.split("\n"):
        s = line.strip()
        if s.startswith("#") and cur is None:
            continue
        m = re.match(r"^\[patch (.+)\]$", s)
        if m:
            cur = m.group(1); edits.setdefault(cur, []); continue
        if cur is None:
            if s:
                top_lines.append(line)
        elif s and not s.startswith("#"):
            edits[cur].append(s)
    out = []
    for b in blocks:
        m = re.search(r"^name = (.+)$", b, re.M)
        if m and m.group(1) in edits:
            lines = b.split("\n")
            gi = lines.index("grid:")
            head, grid = lines[:gi], lines[gi:]
            for e in edits.pop(m.group(1)):
                stg = re.match(r"stage (\d+) (\d+) ([^|]+?)\s*(?:\|(.*))?$", e)
                if stg:   # a story battle: the person standing there fights it (their own words unless given)
                    x, y, stage = stg.group(1), stg.group(2), stg.group(3).strip()
                    extra = [t.strip() for t in (stg.group(4) or "").split("|")]
                    for k, l in enumerate(head):
                        mm = re.match(rf"npc = {x} {y} (\S+) (\S+) (\d+) (\w+)\|?(.*)$", l)
                        if not mm:
                            continue
                        sprite, face, sight, kind, rest = mm.groups()
                        args = rest.split("|")
                        before = args[2] if kind == "fight" and len(args) > 2 else "Let's battle!"
                        after = args[3] if kind == "fight" and len(args) > 3 else "You're strong!"
                        if len(extra) > 0 and extra[0]: before = extra[0]
                        if len(extra) > 1 and extra[1]: after = extra[1]
                        if len(extra) > 2 and extra[2]: sprite = extra[2]
                        if len(extra) > 3 and extra[3]: sight = extra[3]
                        if sprite == "blue": sprite = "rival"
                        head[k] = f"npc = {x} {y} {sprite} {face} {sight} trainer|{stage}|{before}|{after}"
                        break
                    else:
                        print("patch: no npc at", x, y, "in", m.group(1))
                    continue
                at = re.match(r"at (\d+) (\d+): (.+)$", e)
                if at:   # a line for the person standing there (need = ..., if = ...)
                    for k, l in enumerate(head):
                        if re.match(rf"npc = {at.group(1)} {at.group(2)} ", l):
                            head.insert(k + 1, at.group(3)); break
                    else:
                        print("patch: nobody at", at.group(1), at.group(2), "in", m.group(1))
                    continue
                d = re.match(r"drop (npc|sign|door|item|spot) (\d+) (\d+)$", e)
                st = re.match(r"set (\w+) = (.*)$", e)
                if d:
                    kind = d.group(1)
                    keep, skipif = [], False
                    for l in head:
                        if skipif and re.match(r"(if|unless) = ", l):
                            continue
                        skipif = False
                        if re.match(rf"{kind} = {d.group(2)} {d.group(3)} ", l):
                            skipif = True; continue
                        keep.append(l)
                    head = keep
                elif st:
                    head = [l for l in head if not l.startswith(st.group(1) + " = ")] + [f"{st.group(1)} = {st.group(2)}"]
                else:
                    head.append(e)
            b = "\n".join(head + grid)
        out.append(b)
    for k in edits:
        print("patch: no map named", k)
    first = out[0].split("\n")
    si = next((i for i, l in enumerate(first) if l.startswith("sprites = ")), len(first) - 1)
    first[si + 1:si + 1] = top_lines
    out[0] = "\n".join(first)
    return "\n".join(out)


if __name__ == "__main__":
    main()
