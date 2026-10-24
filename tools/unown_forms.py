#!/usr/bin/env python3
"""Unown's letters: Pokemon/Unown B ... Unown Z, Unown Exclamation, Unown Question.

Each is a cosmetic form of Unown ("form_of = Unown" in its pokemon.txt): Unown's stats and moves, its own picture, and
all of them share Unown's one Pokedex entry. A wild Unown turns up as any letter. The pictures are Emerald's party icons
for each letter (pret/pokeemerald's graphics/pokemon/unown/<letter>/icon.png: two frames, the letter bobbing).

    unown_forms.py <pokeemerald>
"""
import os
import re
import sys

from PIL import Image

ROOT = sys.argv[1]
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(HERE, "Pokemon", "Unown")
LETTERS = [(c, c.upper()) for c in "bcdefghijklmnopqrstuvwxyz"] + [("exclamation_mark", "Exclamation"), ("question_mark", "Question")]


def frames(path):
    im = Image.open(path)
    pal = im.getpalette()
    out = Image.new("RGBA", im.size)
    src, dst = im.load(), out.load()
    for y in range(im.height):
        for x in range(im.width):
            c = src[x, y]
            dst[x, y] = (0, 0, 0, 0) if c == 0 else (pal[c * 3], pal[c * 3 + 1], pal[c * 3 + 2], 255)
    return [out.crop((0, k * 32, 32, k * 32 + 32)) for k in range(im.height // 32)]


def shade(im, f):   # (its back: the same letter, darker)
    px = im.load()
    out = im.copy()
    po = out.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            po[x, y] = (int(r * f), int(g * f), int(b * f), a)
    return out


def main():
    txt = open(os.path.join(BASE, "pokemon.txt")).read()
    for key, letter in LETTERS:
        name = "Unown " + letter
        d = os.path.join(HERE, "Pokemon", name)
        os.makedirs(d, exist_ok=True)
        fr = frames(os.path.join(ROOT, "graphics/pokemon/unown", key, "icon.png"))
        fr = [f.crop(f.getbbox()) for f in fr]
        for k, f in enumerate(fr[:2]):
            f.save(os.path.join(d, "201_1_mae_%d.png" % (k + 1)))
            f.save(os.path.join(d, "201_2_hidari_%d.png" % (k + 1)))
            f.save(os.path.join(d, "201_3_migi_%d.png" % (k + 1)))
            shade(f, 0.6).save(os.path.join(d, "201_4_ushiro_%d.png" % (k + 1)))
        fr[0].save(os.path.join(d, "Pokemon-201-%s.png" % name.replace(" ", "-")))
        t = re.sub(r"^name\s*=.*$", "name = " + name, txt, count=1, flags=re.M)
        t = re.sub(r"^(dex\s*=.*)$", r"\1\nform_of = Unown      # one of Unown's letters: it shares Unown's Pokedex entry (tools/unown_forms.py)", t, count=1, flags=re.M)
        open(os.path.join(d, "pokemon.txt"), "w").write(t)
        s = os.path.join(BASE, "shiny.txt")
        if os.path.exists(s):
            open(os.path.join(d, "shiny.txt"), "w").write(open(s).read())
    print(len(LETTERS), "Unown letters")


if __name__ == "__main__":
    main()
