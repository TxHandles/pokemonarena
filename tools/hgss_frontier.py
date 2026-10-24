#!/usr/bin/env python3
"""HeartGold / SoulSilver's Battle Frontier, past Route 40: adds to World/johto.txt

  - Frontier Access (D32R0102: the Pokemon Center, the Mart, the beach) and the Battle Frontier itself (D32), both
    rendered top-down from The Models Resource's location models by tools/hgss_render.py into World/maps/hg_D32*.png
  - the five facilities' lobbies (Johto Battle Tower, Johto Battle Factory, Battle Arcade, Battle Castle, Battle Hall)
    in Emerald's Frontier lobby rooms, each with its receptionist ("frontier|8" .. "frontier|12" - screens_story.cpp)
  - the Frontier Access Pokemon Center and Poke Mart (FireRed's rooms, like the rest of Johto's)
The doors come from the pret/pokeheartgold decompilation (files/fielddata/eventdata/zone_event). Run it again any
time: it replaces the maps it made before. (Route 40's gate gets its doors from World/patches_johto.txt.)

    python3 tools/hgss_frontier.py <pokeheartgold> <dir with "Battle Frontier__*.zip" and "Frontier Access__*.zip">
"""
import glob
import json
import os
import re
import sys
import tempfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hgss_render  # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORLD = os.path.join(HERE, "World")
HG, MODELS = sys.argv[1], sys.argv[2]
HB, PAD = 16, 64   # (as tools/hgss_world.py)

FRONTIER, ACCESS = "Battle Frontier (Johto)", "Frontier Access"
# the facilities: HeartGold's map, our lobby's name, the Emerald lobby it borrows, its frontier number
LOBBIES = [
    ("MAP_BATTLE_TOWER", "Johto Battle Tower", "Battle Tower Lobby", 8),
    ("MAP_BATTLE_FRONTIER_BATTLE_FACTORY", "Johto Battle Factory", "Battle Factory Lobby", 9),
    ("MAP_BATTLE_FRONTIER_BATTLE_ARCADE", "Battle Arcade", "Battle Arena Lobby", 10),
    ("MAP_BATTLE_FRONTIER_BATTLE_CASTLE", "Battle Castle", "Battle Dome Lobby", 11),
    ("MAP_BATTLE_FRONTIER_BATTLE_HALL", "Battle Hall", "Battle Palace Lobby", 12),
]
SIGNS = {(47, 19): "Battle Tower|Prove your skill, battle after battle!",
         (72, 32): "Battle Factory|Rental Pokemon - your knowledge is your weapon!",
         (25, 32): "Battle Hall|One Pokemon against one, every type in turn!",
         (35, 57): "Battle Castle|Win with what you have left - and spend your Castle Points well!",
         (62, 57): "Battle Arcade|Spin the board - luck is part of battling too!"}
PEOPLE = {   # a few of HeartGold's Frontier visitors (their places from the decomp; the lines are ours)
    FRONTIER: [(50, 19, "youngster", "down", "The Battle Tower's Tycoon is Palmer. They say he's tougher than any Champion!"),
               (38, 23, "black_belt", "wander", "My Pokemon and I came all the way from Sinnoh for the Battle Castle!"),
               (60, 29, "gentleman", "wander", "The Battle Hall's Matron, Argenta, has raised Pokemon of every type."),
               (66, 50, "lass", "wander", "In the Arcade, the board decides the weather, your HP, even the levels... I love it!"),
               (44, 61, "woman_1", "wander", "Thorton at the Factory remembers every rental Pokemon there is."),
               (49, 64, "old_woman", "down", "Such young Trainers... it does my heart good to watch them battle."),
               (48, 64, "old_man", "down", "We come here every Sunday to cheer on the challengers.")],
    ACCESS: [(13, 16, "gentleman", "wander", "Up ahead is the Battle Frontier! Only Trainers who've beaten Johto's League may challenge it."),
             (9, 22, "lass", "wander", "Go west along the beach and the cliffs go on and on..."),
             (2, 20, "fisher", "left", "The fish here have seen so many battles, they're not scared of anything.")],
}


def model(name):
    want = os.path.join(tempfile.gettempdir(), "hgss_frontier", re.sub(r"\W+", "_", name))
    dae = glob.glob(os.path.join(want, "**", "*.dae"), recursive=True)
    if not dae:
        z = glob.glob(os.path.join(MODELS, name + "__*.zip"))[0]
        with zipfile.ZipFile(z) as zf:
            for n in zf.namelist():
                if ".." not in n and not n.startswith("/"):
                    zf.extract(n, want)
        dae = glob.glob(os.path.join(want, "**", "*.dae"), recursive=True)
    return sorted(dae)[0]


def render(code, name):
    png = os.path.join(WORLD, "maps", "hg_%s.png" % code)
    info = os.path.join(tempfile.gettempdir(), "hgss_frontier", "hg_%s.json" % code)
    if not (os.path.exists(png) and os.path.exists(info)):
        print("rendering", name, "...", flush=True)
        hgss_render.render(model(name), png, 1.0, info)
    return json.load(open(info))


def events(code):
    f = glob.glob(os.path.join(HG, "files/fielddata/eventdata/zone_event", "*_%s.json" % code))[0]
    return json.load(open(f))


def door_at(grid, x, y, prefer=None):   # (tools/hgss_world.py) the wall you walk into, and where you come out
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


def heights(info):
    rows = []
    for row in info["height"]:
        rows.append("".join(chr(max(33, min(126, 48 + int(round(((HB if v is None else v) - HB) / 4))))) for v in row))
    return rows


def doormats(grid, lines):
    """a door on open ground (a gatehouse's steps) is a door square: walked onto, whatever the rise"""
    g = [list(r) for r in grid]
    for l in lines:
        m = re.match(r"door = (\d+) (\d+) ", l)
        if m and g[int(m.group(2))][int(m.group(1))] != "#":
            g[int(m.group(2))][int(m.group(1))] = "D"
    return ["".join(r) for r in g]


def blocks_of(text):
    return re.split(r"\n(?=\[map\])", text)


def block_named(text, name):
    for b in blocks_of(text):
        if re.search(r"^name = %s$" % re.escape(name), b, re.M):
            return b
    raise SystemExit("no map named " + name)


def lobby_door(block):
    """an Emerald lobby's way out: its door lines (x, y) and the square inside it you arrive on"""
    doors = [tuple(map(int, m)) for m in re.findall(r"^door = (\d+) (\d+) ", block, re.M)]
    grid = block.split("grid:\n")[1].split("\n\n")[0].split("\n")
    x, y = doors[0]
    ay = y - 1
    while ay > 0 and grid[ay][x] not in ".":
        ay -= 1
    return doors, (x, ay)


def main():
    johto = open(os.path.join(WORLD, "johto.txt"), encoding="utf-8").read()
    hoenn = open(os.path.join(WORLD, "hoenn.txt"), encoding="utf-8").read()
    ours = [FRONTIER, ACCESS, ACCESS + " Pokemon Center", ACCESS + " Poke Mart"] + [l[1] for l in LOBBIES]
    johto = "\n".join(b for b in blocks_of(johto) if not any(re.search(r"^name = %s$" % re.escape(n), b, re.M) for n in ours))
    johto = johto.rstrip("\n") + "\n"

    fr, ac = render("D32", "Battle Frontier"), render("D32R0102", "Frontier Access")
    ev_fr, ev_ac = events("D32"), events("D32R0102")
    out = []

    def exterior(name, code, info, extra):
        L = ["[map]", "name = " + name, "image = maps/hg_%s.png" % code, "over = maps/hg_%s_over.png" % code,
             "objects = sprites", "region = Johto", "imgy = %d" % (-(PAD - HB)), "wild = "] + extra
        return L

    # where each lobby's door puts you back outside, and where the Frontier's doors lead in
    lob = {}
    for hgmap, name, emer, fac in LOBBIES:
        b = block_named(hoenn, emer)
        doors, arrive = lobby_door(b)
        lob[hgmap] = (name, b, doors, arrive, fac)

    # ---- the Battle Frontier
    g = fr["grid"]
    L = exterior(FRONTIER, "D32", fr, [])
    back = {}   # lobby map -> (x, y, facing) outside its door
    # HeartGold's warps 0-11 are the doorways; 12-15 are where you stand when you come back out of a building
    ws = ev_fr["warps"]
    for k, w in enumerate(ws):
        if k >= 12 and w["header"] in lob:
            back[w["header"]] = (w["x"], w["z"], "right" if w["x"] < 48 else "left")
    for k, w in enumerate(ws[:12]):   # (the gatehouse from Frontier Access is at the plaza's south end: you come out heading north)
        (dx, dy), (ex, ey, face) = door_at(g, w["x"], w["z"], "up" if w["header"].endswith("ACCESS_GATEHOUSE") else "down")
        if w["header"] in lob:
            name, b, doors, arrive, fac = lob[w["header"]]
            L.append("door = %d %d warp|%s|%d|%d|up" % (dx, dy, name, arrive[0], arrive[1]))
            back.setdefault(w["header"], (ex, ey, face))
        elif w["header"] == "MAP_BATTLE_FRONTIER_FRONTIER_ACCESS_GATEHOUSE":
            L.append("door = %d %d warp|%s|14|8|down" % (dx, dy, ACCESS))
            back.setdefault("gate", (ex, ey, face))
    for (x, y), t in SIGNS.items():
        L.append("sign = %d %d %s" % (x, y, t))
    for x, y, spr, face, say in PEOPLE[FRONTIER]:
        L.append("npc = %d %d %s %s 0 talk|%s" % (x, y, spr, face, say))
    out.append("\n".join(L + ["grid:"] + doormats(g, L) + ["heights:"] + heights(fr)))

    # ---- Frontier Access
    g = [list(r) for r in ac["grid"]]
    for y in range(34, 44):   # the Route 40 gatehouse's roof (its north doorway is HeartGold's warp at 14-15, 34)
        for x in range(12, 18):
            g[y][x] = "#"
    g = ["".join(r) for r in g]
    L = exterior(ACCESS, "D32R0102", ac, ["town = Battle Frontier"])
    gx, gy, gface = back["gate"]
    for w in ev_ac["warps"]:
        (dx, dy), (ex, ey, face) = door_at(g, w["x"], w["z"], "up" if w["z"] < 10 else None)
        h = w["header"]
        if h == "MAP_ROUTE_40_BATTLE_FRONTIER_GATEHOUSE":
            (dx, dy), (ex, ey, face) = (w["x"], w["z"]), (w["x"], w["z"] - 1, "up")
        if h == "MAP_ROUTE_40_BATTLE_FRONTIER_GATEHOUSE":
            L.append("door = %d %d warp|Route 40 Battle Frontier Gate|4|2|down" % (dx, dy))
        elif h == "MAP_BATTLE_FRONTIER_FRONTIER_ACCESS_GATEHOUSE":
            L.append("door = %d %d warp|%s|%d|%d|%s" % (dx, dy, FRONTIER, gx, gy, gface))
        elif h == "MAP_FRONTIER_ACCESS_POKECENTER_1F":
            L.append("door = %d %d warp|%s Pokemon Center|7|8|up" % (dx, dy, ACCESS))
            pc_out = (ex, ey, face)
            L.append("fly = %d %d" % (ex, ey))
        elif h == "MAP_FRONTIER_ACCESS_POKEMART":
            L.append("door = %d %d warp|%s Poke Mart|4|7|up" % (dx, dy, ACCESS))
            mart_out = (ex, ey, face)
    for b in ev_ac.get("bgs", []):
        if b["type"] == 2:
            L.append("item = %d %d Rare Candy hidden" % (b["x"], b["z"]))
        elif b["type"] == 1:
            L.append("sign = %d %d Frontier Access|The Battle Frontier is straight ahead!" % (b["x"], b["z"]))
    for x, y, spr, face, say in PEOPLE[ACCESS]:
        L.append("npc = %d %d %s %s 0 talk|%s" % (x, y, spr, face, say))
    out.append("\n".join(L + ["grid:"] + doormats(g, L) + ["heights:"] + heights(ac)))

    # ---- its Pokemon Center and Mart (FireRed's rooms)
    for src, nm, (ex, ey, face), first in [("Cianwood City Pokemon Center", ACCESS + " Pokemon Center", pc_out, "npc = 7 2 nurse down 0 heal|Welcome to our Pokemon Center!"),
                                           ("Cherrygrove City Poke Mart", ACCESS + " Poke Mart", mart_out, "npc = 2 3 clerk right 0 shop|Welcome! How may I help you?")]:
        b = block_named(johto, src)
        head, grid = b.split("grid:\n")
        lines = [l for l in head.split("\n") if not l.startswith(("npc = ", "if = ", "unless = ", "name = ")) and l]
        lines = [re.sub(r"warp\|[^|]+\|\d+\|\d+\|\w+", "warp|%s|%d|%d|%s" % (ACCESS, ex, ey, face), l) for l in lines]
        lines.insert(1, "name = " + nm)
        lines.append(first)
        if "Pokemon Center" in nm:
            lines.append("npc = 12 5 gentleman wander 0 talk|The Frontier's challenges wear your Pokemon out. Rest here as often as you like!")
        out.append("\n".join(lines) + "\ngrid:\n" + grid.strip("\n"))

    # ---- the lobbies (Emerald's rooms): their receptionists, the way back out
    for hgmap, (name, b, doors, arrive, fac) in lob.items():
        head, grid = b.split("grid:\n")
        ex, ey, face = back[hgmap]
        lines = ["[map]", "name = " + name, "region = Johto"]
        for l in head.split("\n"):
            if l.startswith(("image = ", "over = ", "objects = ", "indoor = ")):
                lines.append(l)
            elif l.startswith("door = "):
                lines.append(re.sub(r"warp\|.*$", "warp|%s|%d|%d|%s" % (FRONTIER, ex, ey, face), l))
            elif re.match(r"npc = .* frontier\|\d+$", l):
                lines.append(re.sub(r"frontier\|\d+$", "frontier|%d" % fac, l))
        out.append("\n".join(lines) + "\ngrid:\n" + grid.strip("\n"))

    johto += "\n" + "\n\n".join(out) + "\n"
    open(os.path.join(WORLD, "johto.txt"), "w", encoding="utf-8").write(johto)
    print("added", len(out), "maps to World/johto.txt")


if __name__ == "__main__":
    main()
