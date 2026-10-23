"""Pokemon Crystal's rooms (the pret/pokecrystal decompilation) as pictures + walking grids + their events:
the Johto gyms, Elm's lab, the towers, the Radio Tower, the Department Store and the like, which have no
HeartGold / SoulSilver interior models. Used by tools/hgss_world.py.

    room = CrystalRooms(path_to_pokecrystal).room("VioletGym")
    room.image (PIL RGBA, 16 px per step), room.grid (rows of . # w g v < > ^), room.warps / bgs / objects
"""
import os
import re

from PIL import Image

PALS = ["GRAY", "RED", "GREEN", "WATER", "YELLOW", "BROWN", "ROOF", "TEXT"]
COLL = {   # Crystal's collision -> our grid
    "FLOOR": ".", "WALL": "#", "TALL_GRASS": "g", "LONG_GRASS": "g", "WATER": "w", "SEA": "w", "WATER_2": "w",
    "WHIRLPOOL": "w", "WATERFALL": "W", "ICE": ".", "PIT": ".", "PIT_2": ".", "DOOR": ".", "DOOR_2": ".",
    "STAIRCASE": ".", "LADDER": ".", "CAVE": ".", "WARP_PANEL": ".", "WARP_CARPET_DOWN": ".", "WARP_CARPET_UP": ".",
    "WARP_CARPET_LEFT": ".", "WARP_CARPET_RIGHT": ".", "LEDGE_DOWN": "v", "LEDGE_LEFT": "<", "LEDGE_RIGHT": ">",
    "HOP_DOWN": "v", "HOP_LEFT": "<", "HOP_RIGHT": ">", "HOP_UP": "#", "WARP_PANEL_2": ".", "STAIRCASE_2": ".",
    "BRIDGE": ".", "PIT_ICE": ".", "SPINNER": ".",
}


def snake(n):
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", n).lower()


def norm(n):
    return n.replace("_", "").upper()


class Room:
    pass


class CrystalRooms:
    def __init__(self, root):
        self.root = root
        r = lambda p: open(os.path.join(root, p), encoding="utf-8", errors="replace").read()
        self.dims = {}
        for m in re.finditer(r"map_const (\w+),\s*(\d+),\s*(\d+)", r("constants/map_constants.asm")):
            self.dims[norm(m.group(1))] = (int(m.group(2)), int(m.group(3)))
        self.const = {norm(k): k for k in re.findall(r"map_const (\w+),", r("constants/map_constants.asm"))}
        self.info = {}
        for m in re.finditer(r"^\tmap (\w+), (TILESET_\w+), (\w+),", r("data/maps/maps.asm"), re.M):
            self.info[m.group(1)] = (m.group(2), m.group(3))
        self.blk = {}
        cur = []
        for line in r("data/maps/blocks.asm").splitlines():
            m = re.match(r"^(\w+)_Blocks:", line)
            if m:
                cur.append(m.group(1)); continue
            m = re.match(r'\s*INCBIN "(maps/\w+\.blk)"', line)
            if m:
                for c in cur:
                    self.blk[c] = m.group(1)
                cur = []
        self.camel = {norm(k): k for k in self.info}
        self._pals = None

    def name_of(self, const):
        """VIOLET_CITY -> VioletCity"""
        return self.camel.get(norm(const))

    # ---------------------------------------------------------------- palettes
    def palettes(self, tileset, env):
        def read(p):
            cols = []
            for line in open(p).read().splitlines():
                line = line.split(";")[0]
                if "RGB" not in line:
                    continue
                nums = [int(x) for x in re.findall(r"\d+", line.split("RGB", 1)[1])]
                for i in range(0, len(nums) - 2, 3):
                    cols.append(tuple(min(255, c * 8 + c // 4) for c in nums[i:i + 3]))
            return [cols[i:i + 4] for i in range(0, len(cols) - 3, 4)]
        base = os.path.join(self.root, "gfx/tilesets")
        special = {"TILESET_HOUSE": "house.pal", "TILESET_RADIO_TOWER": "radio_tower.pal", "TILESET_POKECOM_CENTER": "pokecom_center.pal",
                   "TILESET_ICE_PATH": "ice_path.pal", "TILESET_BATTLE_TOWER_INSIDE": "battle_tower_inside.pal"}
        if tileset in special and os.path.exists(os.path.join(base, special[tileset])):
            p = read(os.path.join(base, special[tileset]))
            if len(p) >= 8:
                return p[:8]
        bg = read(os.path.join(base, "bg_tiles.pal"))
        if env in ("INDOOR", "GATE"):
            idx = [0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x07]
        else:   # day colours (towns, routes, caves, dungeons)
            idx = [0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f]
        return [bg[i] if i < len(bg) else bg[0] for i in idx]

    # ---------------------------------------------------------------- the picture and the grid
    def room(self, name):
        tileset, env = self.info[name]
        ts = tileset[len("TILESET_"):].lower()
        base = os.path.join(self.root, "gfx/tilesets")
        dat = os.path.join(self.root, "data/tilesets")
        gfx = Image.open(os.path.join(base, ts + ".png")).convert("L")
        ntiles = (gfx.width // 8) * (gfx.height // 8)
        tiles = []
        for t in range(ntiles):
            x, y = (t % (gfx.width // 8)) * 8, (t // (gfx.width // 8)) * 8
            tiles.append([[3 - gfx.getpixel((x + i, y + j)) // 85 for i in range(8)] for j in range(8)])
        meta = open(os.path.join(dat, ts + "_metatiles.bin"), "rb").read()
        coll = []
        for m in re.finditer(r"tilecoll (\w+), (\w+), (\w+), (\w+)", open(os.path.join(dat, ts + "_collision.asm")).read()):
            coll.append([m.group(k) for k in range(1, 5)])
        palmap = []
        for m in re.finditer(r"tilepal (\d), (.*)", open(os.path.join(base, ts + "_palette_map.asm")).read()):
            for p in m.group(2).split(","):
                p = p.strip().replace("PRIORITY_", "")
                palmap.append(PALS.index(p) if p in PALS else 0)
        pals = self.palettes(tileset, env)
        w, h = self.dims[norm(name)]
        blk = open(os.path.join(self.root, self.blk[name]), "rb").read()
        img = Image.new("RGBA", (w * 32, h * 32))
        px = img.load()
        grid = [["#"] * (w * 2) for _ in range(h * 2)]
        for by in range(h):
            for bx in range(w):
                b = blk[by * w + bx] if by * w + bx < len(blk) else 0
                for ty in range(4):
                    for tx in range(4):
                        tid = meta[b * 16 + ty * 4 + tx] if b * 16 + ty * 4 + tx < len(meta) else 0
                        if tid < 0x60:
                            gi, pi = tid, tid
                        elif tid >= 0x80:
                            gi, pi = tid - 0x20, tid - 0x80 + 0x60
                        else:
                            continue
                        if gi >= len(tiles):
                            continue
                        pal = pals[palmap[pi] if pi < len(palmap) else 0]
                        t = tiles[gi]
                        for j in range(8):
                            for i in range(8):
                                c = pal[t[j][i]]
                                px[bx * 32 + tx * 8 + i, by * 32 + ty * 8 + j] = c + (255,)
                if b < len(coll):
                    for q in range(4):
                        grid[by * 2 + q // 2][bx * 2 + q % 2] = COLL.get(coll[b][q], "#")
        room = Room()
        room.name, room.image, room.grid, room.w, room.h = name, img, ["".join(r) for r in grid], w * 2, h * 2
        room.coll = [[None] * (w * 2) for _ in range(h * 2)]
        for by in range(h):
            for bx in range(w):
                b = blk[by * w + bx] if by * w + bx < len(blk) else 0
                if b < len(coll):
                    for q in range(4):
                        room.coll[by * 2 + q // 2][bx * 2 + q % 2] = coll[b][q]
        room.indoor = env in ("INDOOR", "GATE", "CAVE", "DUNGEON")
        room.env = env
        self.events(room)
        return room

    # ---------------------------------------------------------------- events and words
    def events(self, room):
        src = open(os.path.join(self.root, "maps", room.name + ".asm"), encoding="utf-8", errors="replace").read()
        room.src = src
        room.warps = [(int(a), int(b), c, int(d)) for a, b, c, d in re.findall(r"warp_event\s+(\d+),\s*(\d+),\s*(\w+),\s*(\d+)", src)]
        room.bgs = [(int(a), int(b), c, d) for a, b, c, d in re.findall(r"bg_event\s+(\d+),\s*(\d+),\s*(\w+),\s*(\w+)", src)]
        room.objects = []
        for m in re.finditer(r"object_event\s+(.*)", src):
            f = [x.strip() for x in m.group(1).split(";")[0].split(",")]
            if len(f) >= 13:
                room.objects.append({"x": int(f[0]), "y": int(f[1]), "sprite": f[2], "move": f[3], "type": f[9], "sight": int(f[10]) if f[10].isdigit() else 0,
                                     "script": f[11], "flag": f[12]})
        room.coords = re.findall(r"coord_event\s+(\d+),\s*(\d+),\s*(\w+),\s*(\w+)", src)
        # labels -> lines
        lab, cur = {}, None
        for line in src.splitlines():
            m = re.match(r"^(\.?\w+):+", line)
            if m:
                cur = m.group(1); lab[cur] = []; continue
            if cur and line.strip():
                lab[cur].append(line.strip())
        room.labels = lab

    def text(self, room, label):
        """a text label's pages (our "|" between them)"""
        lines = room.labels.get(label)
        if lines is None:
            return ""
        pages, cur = [], ""
        for l in lines:
            m = re.match(r'(text|line|cont|para|next|page)\s+"(.*)"', l)
            if not m:
                if l.startswith(("done", "prompt", "text_end")):
                    break
                if l.startswith("text_ram") or l.startswith("text_decimal"):
                    cur += " ..."
                continue
            kind, t = m.groups()
            if kind in ("para", "page") and cur:
                pages.append(cur); cur = ""
            cur = (cur + " " + t).strip()
        if cur:
            pages.append(cur)
        return "|".join(clean_crystal(p) for p in pages)

    def script_text(self, room, label, depth=0):
        """what someone says: the first text their script shows"""
        lines = room.labels.get(label, [])
        for l in lines:
            m = re.match(r"(jumptextfaceplayer|jumptext|writetext|farwritetext|jumpstd)\s+(\w+)", l)
            if m:
                if m.group(1) == "jumpstd":
                    return ""
                return self.text(room, m.group(2))
            m = re.match(r"(sjump|scall)\s+(\.?\w+)", l)
            if m and depth < 3:
                return self.script_text(room, m.group(2), depth + 1)
        return ""

    def trainer_of(self, room, label):
        """an OBJECTTYPE_TRAINER's script: trainer CLASS, NAME, event, seen text, beaten text, ..."""
        for l in room.labels.get(label, []):
            m = re.match(r"trainer\s+(\w+),\s*(\w+),\s*\w+,\s*(\w+),\s*(\w+)", l)
            if m:
                cls, who, seen, beaten = m.groups()
                after = ""
                for l2 in room.labels.get(".Script", []) + room.labels.get(label + ".Script", []):
                    m2 = re.match(r"writetext\s+(\w+)", l2)
                    if m2:
                        after = self.text(room, m2.group(1)); break
                # the after-battle text is usually named after the trainer
                k = re.sub(r"SeenText$", "AfterBattleText", seen)
                if k in room.labels:
                    after = self.text(room, k)
                return cls, who, self.text(room, seen), after
        return None


def clean_crystal(s):
    s = s.replace("#MON", "Pokemon").replace("#", "Poke").replace("<PLAYER>", "@P@").replace("<PLAY_G>", "@P@")
    s = s.replace("<RIVAL>", "Silver").replace("<USER>", "").replace("<TARGET>", "").replace("…", "...").replace("@", "")
    s = s.replace("<PK><MN>", "Pokemon").replace("<PKMN>", "Pokemon").replace("'", "'").replace("é", "e")
    s = re.sub(r"<[^>]*>", "", s)
    def cap(m):
        w = m.group(0)
        if w in ("TM", "HM", "PP", "HP", "OK", "PC", "TV", "S.S.", "EXP", "ID", "KO", "VS", "UFO", "PA"):
            return w
        return w.capitalize() if len(w) > 1 else w
    s = re.sub(r"\b[A-Z][A-Z']+\b", cap, s)
    s = re.sub(r"\s+", " ", s).strip()
    s = re.sub(r"(\w)- (\w)", r"\1\2", s).replace("GoldenRod", "Goldenrod").replace("@P@", "{PLAYER}")
    return s.replace("/", "-").replace("|", "-")


CLASS_NAME = {"GRUNTM": "Rocket Grunt", "GRUNTF": "Rocket Grunt", "EXECUTIVEM": "Rocket Executive", "EXECUTIVEF": "Rocket Executive",
              "COOLTRAINERM": "Ace Trainer", "COOLTRAINERF": "Ace Trainer", "POKEFANM": "Pokefan", "POKEFANF": "Pokefan",
              "SWIMMERM": "Swimmer", "SWIMMERF": "Swimmer", "PSYCHIC_T": "Psychic", "SCHOOLBOY": "Schoolboy", "SUPER_NERD": "Super Nerd",
              "BIRD_KEEPER": "Bird Keeper", "BUG_CATCHER": "Bug Catcher", "KIMONO_GIRL": "Kimono Girl", "BLACKBELT_T": "Black Belt",
              "GENTLEMAN": "Gentleman", "SCIENTIST": "Scientist", "BURGLAR": "Burglar", "BOARDER": "Boarder", "SKIER": "Skier"}


def crystal_parties(root):
    """(CLASS, n) -> (trainer name, [(species, level), ...]) from data/trainers/parties.asm"""
    out = {}
    p = os.path.join(root, "data/trainers/parties.asm")
    if not os.path.exists(p):
        return out
    cur, name, team = None, None, []
    for line in open(p, encoding="utf-8", errors="replace"):
        m = re.match(r"\s*; (\w+) \((\d+)\)", line)
        if m:
            cur = (m.group(1), int(m.group(2))); team = []; continue
        m = re.match(r'\s*db "([^@"]*)@?", TRAINERTYPE', line)
        if m and cur:
            name = m.group(1); continue
        m = re.match(r"\s*db\s+(\d+),\s*(\w+)", line)
        if m and cur:
            team.append((m.group(2), int(m.group(1)))); continue
        if re.match(r"\s*db -1", line) and cur:
            out[cur] = (name, team); cur = None
    return out


def crystal_trainer_index(root):
    """GRUNTM_3 -> (GRUNTM, 3)"""
    out, cls, n = {}, None, 0
    for line in open(os.path.join(root, "constants/trainer_constants.asm"), encoding="utf-8", errors="replace"):
        m = re.match(r"\s*trainerclass (\w+)", line)
        if m:
            cls, n = m.group(1), 0; continue
        m = re.match(r"\s*const (\w+)", line)
        if m and cls:
            n += 1; out[m.group(1)] = (cls, n)
    return out
