#!/usr/bin/env python3
"""HeartGold / SoulSilver's trainers on the overworld sheet (run after tools/frlg_sprites.py).

    hgss_sprites.py <trainers sheet png> [World dir]

The sheet: The Spriters Resource's "Pokemon HeartGold / SoulSilver - Trainers (Overworld)", ripped by Dragoon
(credit wanted). Each person is a 96x128 block on its own background colour, 32x32 frames.

World/chars.png becomes 32x32 cells (FireRed's 16x32 people sit in the middle), one row per person:
down step/stand/step, up x3, left x3 (right = left mirrored); the HeartGold people go after FireRed's, named
hg_* in World/chars.txt (and so in world.txt's "sprites =" line, which tools/frlg_world.py copies from it).
"""
import os
import sys
from collections import Counter

from PIL import Image

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = sys.argv[1]
WORLD = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "World")

# the people on the sheet, left to right, top to bottom
NAMES = ["elm", "silver", "steven", "kurt", "goggles_man", "little_boy", "maizie", "cap_boy", "straw_hat", "mom",
         "pigtails", "young_man", "boy", "blond_boy", "spiky_man", "girl", "jasmine", "bun_woman", "bald_man", "woman",
         "pryce", "granny", "aunt", "falkner", "mr_pokemon", "swimmer_m", "swimmer_f", "glasses_man", "blue_hair", "bugsy",
         "whitney", "morty", "bruno", "erika", "old_white", "clair", "will", "koga", "chuck", "karen",
         "lance", "twintails", "surge", "sabrina", "misty", "headband_girl", "janine", "brock", "blaine", "blue",
         "red", "officer", "monk", "rocket_f", "rocket_m", "ariana", "archer", "cook", "flame_man", "cyclist",
         "kimono_girl", "lass", "captain", "dark_girl", "spiky_punk", "eusine", "elder", "tank_man", "pink_girl", "green_girl",
         "navy_girl", "cap_girl", "hat_girl", "grandpa", "giovanni", "bearded_man", "sage", "petrel", "proton", "worker"]
# the sheet's frame order (3 x 4 grid) -> ours: down step/stand/step, up x3, left x3
#   up: stand 0, steps 2, 10   down: stand 5, steps 8, 11   left: stand 3, steps 1, 4
ORDER = [8, 5, 11, 2, 0, 10, 1, 3, 4]


def cut(sheet):
    im = Image.open(sheet).convert("RGB")
    px = im.load()
    W, H = im.size
    people = []
    for r in range(8):
        y0 = r * 128
        row = [px[x, y0 + 1] for x in range(W)]
        xs = [0] + [x for x in range(1, W) if row[x] != row[x - 1]] + [W]
        segs = []
        for a, b in zip(xs, xs[1:]):
            if b - a < 20:
                if segs:
                    segs[-1] = (segs[-1][0], b)
                continue
            n = max(1, round((b - a) / 96))
            for k in range(n):
                segs.append((a + (b - a) * k // n, a + (b - a) * (k + 1) // n))
        for a, b in segs:
            frames = []
            for fr in range(4):
                for fc in range(3):
                    fx, fy = a + (b - a) * fc // 3, y0 + 32 * fr
                    f = im.crop((fx, fy, fx + 32, fy + 32)).convert("RGBA")
                    d = f.load()
                    border = [d[i, 0] for i in range(32)] + [d[0, i] for i in range(32)] + [d[31, i] for i in range(32)]
                    bg = Counter(border).most_common(1)[0][0]
                    for yy in range(32):
                        for xx in range(32):
                            c = d[xx, yy]
                            if abs(c[0] - bg[0]) + abs(c[1] - bg[1]) + abs(c[2] - bg[2]) < 12:
                                d[xx, yy] = (0, 0, 0, 0)
                    frames.append(f)
            people.append(frames)
    return people


def main():
    chars = Image.open(os.path.join(WORLD, "chars.png")).convert("RGBA")
    names = open(os.path.join(WORLD, "chars.txt")).read().split()
    keep = [n for n in names if not n.startswith("hg_")]   # (run again: FireRed's rows only)
    if chars.width == 144:   # FireRed's 16x32 cells -> 32x32
        wide = Image.new("RGBA", (288, chars.height))
        for i in range(9):
            wide.paste(chars.crop((i * 16, 0, i * 16 + 16, chars.height)), (i * 32 + 8, 0))
        chars = wide
    chars = chars.crop((0, 0, 288, len(keep) * 32))
    people = cut(SRC)
    out = Image.new("RGBA", (288, (len(keep) + len(people)) * 32))
    out.paste(chars, (0, 0))
    for k, frames in enumerate(people):
        for i, f in enumerate(ORDER):
            out.paste(frames[f], (i * 32, (len(keep) + k) * 32))
    out.save(os.path.join(WORLD, "chars.png"), optimize=True)
    open(os.path.join(WORLD, "chars.txt"), "w").write(" ".join(keep + ["hg_" + n for n in NAMES[:len(people)]]) + "\n")
    print(len(keep), "FireRed people +", len(people), "HeartGold people")


if __name__ == "__main__":
    main()
