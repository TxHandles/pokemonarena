#!/usr/bin/env python3
"""The Pokemon Mansion's secret switches (pret/pokefirered's data/scripts/pokemon_mansion.inc): each statue swaps
which barriers stand, on every floor at once. For World/world.txt this makes:
  - World/maps/PokemonMansion_<floor>_alt.png (+ _over): the floor with the switch pressed (the barriers moved)
  - lines for World/patches.txt (printed): "alt = mansion_switch|picture|over" (the engine shows those while the
    flag's set), a gate on every square the switch changes ("gate = x y mansion_switch" - shut until it's pressed;
    "!mansion_switch" - shut once it's pressed) and the statues as switches that toggle it.

    frlg_mansion.py <pokefirered dir> [World dir]    > patch lines
Run World/patches.txt's Mansion block through tools/patchlib.py (or frlg_world.py) after."""
import json, os, re, struct, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frlg_render

ROOT = sys.argv[1]
WORLD = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "World")
FLOORS = {"1F": "Pokemon Mansion 1F", "2F": "Pokemon Mansion 2F", "3F": "Pokemon Mansion 3F", "B1F": "Pokemon Mansion B1F"}
FLAG = "mansion_switch"

labels = dict((k, int(v, 16)) for k, v in re.findall(r"#define (METATILE_\w+)\s+(0x[0-9A-Fa-f]+)", open(os.path.join(ROOT, "include/constants/metatile_labels.h")).read()))
src = open(os.path.join(ROOT, "data/scripts/pokemon_mansion.inc")).read()


def sets(label):
    body = src.split(label + "::")[1].split("return")[0]
    return [(int(x), int(y), labels[m], int(c)) for x, y, m, c in re.findall(r"setmetatile\s+(\d+),\s*(\d+),\s*(\w+),\s*(\d)", body)]


found = frlg_render.find_tilesets(ROOT)
lays = {l["id"]: l for l in json.load(open(os.path.join(ROOT, "data/layouts/layouts.json")))["layouts"] if "width" in l}
world = open(os.path.join(WORLD, "world.txt")).read()
out = ["# ------------------------------------------------------------------ the Pokemon Mansion's secret switches",
       "# (made by tools/frlg_mansion.py: the barriers each statue moves, on every floor)"]
for floor, name in FLOORS.items():
    folder = "PokemonMansion_" + floor
    j = json.load(open(os.path.join(ROOT, "data/maps", folder, "map.json")))
    lay = dict(lays[j["layout"]])
    W, H = lay["width"], lay["height"]
    raw = bytearray(open(os.path.join(ROOT, lay["blockdata_filepath"]), "rb").read())
    blocks = list(struct.unpack("<%dH" % (W * H), bytes(raw[:W * H * 2])))
    press = sets("PokemonMansion_EventScript_PressSwitch_" + floor)
    reset = sets("PokemonMansion_EventScript_ResetSwitch_" + floor)
    # the map as it starts is the reset state; pressed: the press list over it
    for x, y, mt, c in reset:
        blocks[y * W + x] = (blocks[y * W + x] & 0xF000) | (c << 10) | mt
    base = list(blocks)
    for x, y, mt, c in press:
        blocks[y * W + x] = (blocks[y * W + x] & 0xF000) | (c << 10) | mt
    tmp = tempfile.NamedTemporaryFile(suffix=".bin", delete=False)
    tmp.write(struct.pack("<%dH" % (W * H), *blocks)); tmp.close()
    lay["blockdata_filepath"] = tmp.name
    img, over, _ = frlg_render.render_layout(ROOT, lay, found)   # (an absolute path joins over the root)
    os.unlink(tmp.name)
    img.save(os.path.join(WORLD, "maps", folder + "_alt.png"), optimize=True)
    over_name = ""
    if over.getbbox():
        over.save(os.path.join(WORLD, "maps", folder + "_alt_over.png"), optimize=True)
        over_name = "maps/" + folder + "_alt_over.png"
    out.append("[patch %s]" % name)
    out.append("alt = %s|maps/%s_alt.png|%s" % (FLAG, folder, over_name))
    for k in range(W * H):
        a, b = base[k] >> 10 & 3, blocks[k] >> 10 & 3
        if a == b:
            continue
        x, y = k % W, k // W
        if a and not b:      # a barrier that goes away: walkable in the grid, shut until pressed
            out.append("tile %d %d ." % (x, y))
            out.append("gate = %d %d %s" % (x, y, FLAG))
        else:                # floor that a barrier closes
            out.append("gate = %d %d !%s" % (x, y, FLAG))
    # the statues: "A secret switch! Press it?"
    blk = [b for b in world.split("[map]") if re.search(r"^name = %s$" % re.escape(name), b, re.M)][0]
    for x, y in re.findall(r"^sign = (\d+) (\d+) A secret switch!", blk, re.M):
        out.append("drop sign %s %s" % (x, y))
        out.append("spot = %s %s switch|%s|A secret switch!|Press it?" % (x, y, FLAG))
print("\n".join(out))
