#!/usr/bin/env python3
"""Install one or more Pokemon folders into Pokemon/ and re-bake the exe's data.

    python tools/install_pokemon.py <folder> [<folder> ...]

Each folder holds a pokemon.txt and sprite images named with direction words
(front/mae, back/ushiro, left/hidari, right/migi + _1/_2). WEBP/JPG images are
converted to PNG. The folder name in Pokemon/ is the "name =" from pokemon.txt.
"""
import os, re, shutil, subprocess, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def name_of(txt):
    for line in open(txt, encoding="utf-8", errors="replace"):
        m = re.match(r"\s*name\s*=\s*([^#]+)", line)
        if m:
            return m.group(1).strip()
    return None

for src in sys.argv[1:]:
    txt = os.path.join(src, "pokemon.txt")
    if not os.path.exists(txt):
        txt = os.path.join(src, "creature.txt")
    name = name_of(txt) if os.path.exists(txt) else os.path.basename(os.path.normpath(src))
    dst = os.path.join(ROOT, "Pokemon", name)
    os.makedirs(dst, exist_ok=True)
    n = 0
    for f in sorted(os.listdir(src)):
        p = os.path.join(src, f)
        base, ext = os.path.splitext(f)
        if ext.lower() in (".webp", ".jpg", ".jpeg", ".gif", ".bmp"):
            Image.open(p).convert("RGB").save(os.path.join(dst, base + ".png")); n += 1
        elif ext.lower() == ".png":
            shutil.copy(p, os.path.join(dst, f)); n += 1
        elif f.lower() in ("pokemon.txt", "creature.txt"):
            shutil.copy(p, os.path.join(dst, "pokemon.txt"))
    print(f"{name}: {n} images")

subprocess.run([sys.executable, os.path.join(ROOT, "tools", "embed_files.py")], check=True)
