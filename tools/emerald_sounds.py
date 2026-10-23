#!/usr/bin/env python3
"""Emerald's sound effects into Sounds/Hoenn/ (short mono OGGs). Out in Hoenn the game plays these over its
FireRed ones (audio.cpp's sound sets); anywhere else nothing changes. The source: The Sounds Resource's
"Pokemon Emerald - Sound Effects" (emerald_XXXX.wav, XXXX = the sound's number in hex - pret/pokeemerald's
include/constants/songs.h).
    python tools/emerald_sounds.py <folder with the emerald_XXXX.wav files> <pokeemerald dir>
"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC, EM = sys.argv[1], sys.argv[2]
OUT = os.path.join(ROOT, "Sounds", "Hoenn")
SONGS = {n: int(v) for n, v in re.findall(r"#define (SE_\w+)\s+(\d+)", open(os.path.join(EM, "include/constants/songs.h")).read())}
# our sound -> Emerald's (the same choices as tools/frlg_sounds.py, by name)
MAP = {
    "shoot": "SE_M_SWIFT", "zap": "SE_M_THUNDERBOLT", "fire": "SE_M_EMBER", "water": "SE_M_BUBBLE", "thud": "SE_M_HEADBUTT",
    "hit": "SE_EFFECTIVE", "hit_super": "SE_SUPER_EFFECTIVE", "hit_weak": "SE_NOT_EFFECTIVE", "crit": "SE_M_SCRATCH",
    "faint": "SE_FAINT", "level_up": "SE_EXP_MAX", "heal": "SE_USE_ITEM", "shield": "SE_M_DETECT", "buff": "SE_M_STAT_INCREASE",
    "menu_move": "SE_SELECT", "menu_ok": "SE_SELECT", "menu_back": "SE_SELECT", "ball_throw": "SE_BALL_THROW",
    "ball_shake": "SE_BALL", "ball_open": "SE_BALL_OPEN", "caught": "SE_BALL_CLICK", "flee": "SE_FLEE", "door": "SE_DOOR",
    "bump": "SE_WALL_HIT", "ledge": "SE_LEDGE", "save": "SE_SAVE", "shiny": "SE_SHINY", "egg_hatch": "SE_EGG_HATCH",
    "repel": "SE_REPEL", "shop": "SE_SHOP",
}
os.makedirs(OUT, exist_ok=True)
for name, se in MAP.items():
    num = SONGS.get(se)
    src = os.path.join(SRC, "emerald_%04X.wav" % num) if num else ""
    if not num or not os.path.exists(src):
        print("missing", name, se, num)
        continue
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-ac", "1", "-ar", "22050",
                    "-af", "areverse,silenceremove=start_periods=1:start_threshold=-60dB,areverse",
                    "-c:a", "libvorbis", "-q:a", "3", os.path.join(OUT, name + ".ogg")], check=True)
total = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT))
print(len(os.listdir(OUT)), "Hoenn sounds,", total // 1024, "KB")
