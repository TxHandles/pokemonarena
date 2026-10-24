#!/usr/bin/env python3
"""The Ruins of Alph's chambers (Pokemon Crystal's rooms, pret/pokecrystal) for World/johto.txt:

  - the four puzzle chambers (Kabuto, Omanyte, Aerodactyl, Ho-Oh): the stone panel at the back is the Unown
    puzzle ("alphpuzzle|kabuto": screens_puzzle.cpp). Solved, the floor gives way into the Inner Chamber and the
    back wall opens on the chamber's item room - the picture with the holes and the open wall is the map's
    "alt" (shown once the flag "alph:<kind>" is set), the holes and the wall doors only open then too
  - the Inner Chamber, where the Unown come out once any puzzle's solved ("wildif = alph:any")
  - the four item rooms
  - the puzzles' pictures (World/puzzles/alph_<kind>.png, Crystal's gfx/unown_puzzle in the puzzle's colours)
  - the doors from the Ruins of Alph (HeartGold's map) into them, where HeartGold has its entrances
Run it again any time: it replaces what it made before.

    python3 tools/hgss_alph.py <pokecrystal>
"""
import os
import re
import sys
import tempfile

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from crystal_rooms import CrystalRooms  # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORLD = os.path.join(HERE, "World")
CR = CrystalRooms(sys.argv[1])

# the chambers: our kind, Crystal's name, the Ruins of Alph square HeartGold's entrance is on (its warp - 416, 256)
KINDS = [("kabuto", "Kabuto", (20, 10)), ("hooh", "HoOh", (9, 33)), ("omanyte", "Omanyte", (9, 53)), ("aerodactyl", "Aerodactyl", (20, 53))]
INNER_AT = [(14, 28), (15, 28)]
PRETTY = {"kabuto": "Kabuto", "hooh": "Ho-Oh", "omanyte": "Omanyte", "aerodactyl": "Aerodactyl"}
OUTSIDE = "Ruins of Alph"
INNER = "Ruins of Alph Inner Chamber"
PUZZLE_COLOURS = [(248, 232, 176), (216, 160, 72), (136, 80, 32), (48, 24, 8)]   # (light to dark: sandstone)


def chamber_name(kind):
    return "Ruins of Alph %s Chamber" % PRETTY[kind]


def item_name(kind):
    return "Ruins of Alph %s Item Room" % PRETTY[kind]


def room_with(name, changes):
    """a Crystal room with some of its blocks changed: {(block x, block y): block}"""
    if not changes:
        return CR.room(name)
    from crystal_rooms import norm
    path = os.path.join(CR.root, CR.blk[name])
    blk = bytearray(open(path, "rb").read())
    w, _ = CR.dims[norm(name)]
    for (bx, by), b in changes.items():
        blk[by * w + bx] = b
    tmp = tempfile.NamedTemporaryFile(suffix=".blk", delete=False)
    tmp.write(bytes(blk)); tmp.close()
    old = CR.blk[name]
    CR.blk[name] = tmp.name   # (an absolute path joins over the root)
    try:
        return CR.room(name)
    finally:
        CR.blk[name] = old
        os.unlink(tmp.name)


def grid_of(text, name):
    b = [x for x in re.split(r"\n(?=\[map\])", text) if re.search(r"^name = %s$" % re.escape(name), x, re.M)][0]
    return b, b.split("grid:\n")[1].split("heights:")[0].strip().split("\n")


def outside_door(grid, x, y):
    """HeartGold's doorway square on the Ruins of Alph: our door is the wall you walk into, you come out below it"""
    H, W = len(grid), len(grid[0])
    ok = lambda tx, ty: 0 <= tx < W and 0 <= ty < H and grid[ty][tx] in ".gcs"
    tx, ty = x, y
    for _ in range(6):
        if ok(tx, ty + 1):
            return (tx, ty), (tx, ty + 1)
        ty += 1
    return (x, y), (x, y + 1)


def main():
    johto = open(os.path.join(WORLD, "johto.txt"), encoding="utf-8").read()
    ours = [INNER] + [chamber_name(k) for k, _, _ in KINDS] + [item_name(k) for k, _, _ in KINDS]
    blocks = re.split(r"\n(?=\[map\])", johto)
    blocks = [b for b in blocks if not any(re.search(r"^name = %s$" % re.escape(n), b, re.M) for n in ours)]
    johto = "\n".join(blocks).rstrip("\n") + "\n"

    # ---- the Ruins of Alph: its doors into the chambers (any made before go first)
    ob, og = grid_of(johto, OUTSIDE)
    lines = [l for l in ob.split("grid:\n")[0].split("\n") if not re.match(r"door = .*warp\|Ruins of Alph (\w|-)+ Chamber\|", l)]
    back = {}
    for kind, crn, (x, y) in KINDS:
        (dx, dy), (ex, ey) = outside_door(og, x, y)
        lines.append("door = %d %d warp|%s|3|8|up" % (dx, dy, chamber_name(kind)))
        back[kind] = (ex, ey)
    for x, y in INNER_AT:
        (dx, dy), (ex, ey) = outside_door(og, x, y)
        lines.append("door = %d %d warp|%s|10|12|up" % (dx, dy, INNER))
        back["inner"] = (ex, ey)
    nb = "\n".join(lines).rstrip("\n") + "\ngrid:\n" + ob.split("grid:\n", 1)[1]
    johto = johto.replace(ob, nb)

    out = []
    os.makedirs(os.path.join(WORLD, "puzzles"), exist_ok=True)
    for kind, crn, _ in KINDS:
        name = "RuinsOfAlph%sChamber" % crn
        # as it starts: the back wall shut, the floor whole; solved: the wall open, two holes in the floor
        shut = room_with(name, {(2, 0): 0x2e, (1, 1): 0x01, (2, 1): 0x02})
        opened = room_with(name, {(2, 0): 0x30, (1, 1): 0x18, (2, 1): 0x19})
        shut.image.save(os.path.join(WORLD, "maps", "cr_%s.png" % name), optimize=True)
        opened.image.save(os.path.join(WORLD, "maps", "cr_%s_alt.png" % name), optimize=True)
        flag = "alph:" + kind
        L = ["[map]", "name = " + chamber_name(kind), "region = Johto", "image = maps/cr_%s.png" % name, "objects = sprites", "indoor = yes",
             "alt = %s|maps/cr_%s_alt.png|" % (flag, name)]
        ex, ey = back[kind]
        L.append("door = 3 9 warp|%s|%d|%d|down" % (OUTSIDE, ex, ey))
        L.append("door = 4 9 warp|%s|%d|%d|down" % (OUTSIDE, ex, ey))
        # the holes (into the Inner Chamber, by its stairs up to this chamber) and the opened wall
        ins = {"kabuto": (15, 4), "hooh": (3, 16), "omanyte": (3, 22), "aerodactyl": (15, 25)}[kind]
        for hx in (3, 4):
            L += ["door = %d 3 warp|%s|%d|%d|down" % (hx, INNER, ins[0] + hx - 3, ins[1]), "if = " + flag]
        L += ["door = 4 0 warp|%s|3|8|up" % item_name(kind), "if = " + flag]
        L.append("spot = 3 2 alphpuzzle|%s" % kind)
        desc = CR.script_text(shut, "RuinsOfAlph%sChamberDescriptionSign" % crn)
        if desc:
            L.append("sign = 4 2 " + desc)
        for x in (2, 5):
            t = CR.script_text(shut, "RuinsOfAlph%sChamberAncientReplica" % crn)
            if t:
                L.append("sign = %d 3 %s" % (x, t))
        sci = CR.text(shut, "RuinsOfAlph%sChamberScientistCrypticText" % crn) if "RuinsOfAlph%sChamberScientistCrypticText" % crn in shut.labels else ""
        L.append("npc = 3 1 scientist up 0 talk|%s" % (sci or "These panels... Arrange the stones to form the picture, and something might happen."))
        L.append("unless = " + flag)
        L.append("npc = 3 1 scientist up 0 talk|Whoa! That tremor... The wall at the back opened up! There's a room past it!")
        L.append("if = " + flag)
        g = [list(r) for r in shut.grid]
        g[0][4] = "#"   # (the wall, until it opens: its door only goes once the puzzle's solved)
        L.append("grid:")
        L += ["".join(r) for r in g]
        out.append("\n".join(L))
        # the item room: four items, the way back out (the opened wall)
        iname = "RuinsOfAlph%sItemRoom" % crn
        r = CR.room(iname)
        r.image.save(os.path.join(WORLD, "maps", "cr_%s.png" % iname), optimize=True)
        L = ["[map]", "name = " + item_name(kind), "region = Johto", "image = maps/cr_%s.png" % iname, "objects = sprites", "indoor = yes",
             "door = 3 9 warp|%s|4|1|down" % chamber_name(kind), "door = 4 9 warp|%s|4|1|down" % chamber_name(kind)]
        conv = {"BERRY": "Oran Berry", "PSNCUREBERRY": "Pecha Berry", "HEAL_POWDER": "Heal Powder", "ENERGYPOWDER": "Energy Powder",
                "MYSTERYBERRY": "Leppa Berry", "MYSTIC_WATER": "Mystic Water", "STARDUST": "Stardust", "STAR_PIECE": "Star Piece",
                "GOLD_BERRY": "Sitrus Berry", "MOON_STONE": "Moon Stone", "PARLYZ_HEAL": "Paralyze Heal", "CHARCOAL": "Charcoal",
                "BITTER_BERRY": "Persim Berry", "ICE_BERRY": "Aspear Berry", "MINT_BERRY": "Chesto Berry", "MIRACLEBERRY": "Lum Berry",
                "BURNT_BERRY": "Rawst Berry", "PRZCUREBERRY": "Cheri Berry"}
        for o in r.objects:
            for l in r.labels.get(o["script"], []):
                m = re.match(r"itemball\s+(\w+)", l)
                if m:
                    L.append("item = %d %d %s" % (o["x"], o["y"], conv.get(m.group(1), m.group(1).replace("_", " ").title())))
        L.append("grid:")
        L += r.grid
        out.append("\n".join(L))
        # the puzzle's picture
        src = Image.open(os.path.join(CR.root, "gfx/unown_puzzle", kind + ".png")).convert("L")
        pic = Image.new("RGB", src.size)
        for y in range(src.height):
            for x in range(src.width):
                pic.putpixel((x, y), PUZZLE_COLOURS[min(3, (255 - src.getpixel((x, y))) // 64)])
        pic.save(os.path.join(WORLD, "puzzles", "alph_%s.png" % kind), optimize=True)

    # ---- the Inner Chamber: the way out, the stairs up to each chamber, the Unown
    r = CR.room("RuinsOfAlphInnerChamber")
    r.image.save(os.path.join(WORLD, "maps", "cr_RuinsOfAlphInnerChamber.png"), optimize=True)
    ex, ey = back["inner"]
    L = ["[map]", "name = " + INNER, "region = Johto", "image = maps/cr_RuinsOfAlphInnerChamber.png", "objects = sprites", "indoor = yes",
         "wild = Ruins of Alph (Inner Chamber)", "wildif = alph:any", "door = 10 13 warp|%s|%d|%d|down" % (OUTSIDE, ex, ey)]
    for x, y, const, idx in r.warps:
        nm = CR.name_of(const)
        for kind, crn, _ in KINDS:
            if nm == "RuinsOfAlph%sChamber" % crn:
                L.append("door = %d %d warp|%s|%d|4|down" % (x, y, chamber_name(kind), 3 if x % 2 else 4))
                L.append("if = alph:" + kind)
    for x, y, kind, script in r.bgs:
        t = CR.script_text(r, script)
        if t:
            L.append("sign = %d %d %s" % (x, y, t))
    L.append("npc = 10 17 scientist wander 0 talk|The Unown... they appeared when the puzzle was solved! There are so many kinds - like letters!")
    L.append("if = alph:any")
    L.append("grid:")
    L += ["".join("c" if ch == "." else ch for ch in row) for row in r.grid]
    out.append("\n".join(L))

    johto += "\n" + "\n\n".join(out).replace("PokeMon", "Pokemon") + "\n"
    open(os.path.join(WORLD, "johto.txt"), "w", encoding="utf-8").write(johto)
    print("Ruins of Alph: %d rooms" % len(out))


if __name__ == "__main__":
    main()
