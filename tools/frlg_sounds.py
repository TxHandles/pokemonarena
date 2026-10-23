#!/usr/bin/env python3
"""FireRed / LeafGreen's sound effects into the game's Sounds/ folder (short mono OGGs the game loads over its
synthesized ones). The source: The Sounds Resource's "Pokemon FireRed / LeafGreen - Miscellaneous - Sound Effects"
(firered_XXXX.wav, XXXX = the sound's number in the game: pret/pokefirered's include/constants/songs.h). Sound effects
only - no music.
    python tools/frlg_sounds.py <folder with the firered_XXXX.wav files>
"""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = sys.argv[1]
OUT = os.path.join(ROOT, "Sounds")
# our sound -> FireRed's (songs.h)
MAP = {
    "shoot": 199,       # SE_M_SWIFT
    "zap": 111,         # SE_M_THUNDERBOLT
    "fire": 144,        # SE_M_EMBER
    "water": 117,       # SE_M_BUBBLE
    "thud": 155,        # SE_M_HEADBUTT
    "hit": 13,          # SE_EFFECTIVE
    "hit_super": 14,    # SE_SUPER_EFFECTIVE
    "hit_weak": 12,     # SE_NOT_EFFECTIVE
    "crit": 148,        # SE_M_SCRATCH
    "faint": 16,        # SE_FAINT
    "level_up": 84,     # SE_EXP_MAX
    "heal": 1,          # SE_USE_ITEM
    "shield": 202,      # SE_M_DETECT
    "buff": 232,        # SE_M_STAT_INCREASE
    "menu_move": 245,   # SE_BAG_CURSOR
    "menu_ok": 5,       # SE_SELECT
    "menu_back": 5,     # SE_SELECT
    "ball_throw": 54,   # SE_BALL_THROW
    "ball_shake": 23,   # SE_BALL
    "ball_open": 15,    # SE_BALL_OPEN
    "caught": 247,      # SE_BALL_CLICK
    "flee": 17,         # SE_FLEE
    "door": 241,        # SE_DOOR
    "bump": 7,          # SE_WALL_HIT
    "ledge": 10,        # SE_LEDGE
    "save": 48,         # SE_SAVE
    "shiny": 95,        # SE_SHINY
    "egg_hatch": 106,   # SE_EGG_HATCH
    "repel": 41,        # SE_REPEL
    "shop": 248,        # SE_SHOP
}
os.makedirs(OUT, exist_ok=True)
for name, num in MAP.items():
    src = os.path.join(SRC, "firered_%04X.wav" % num)
    if not os.path.exists(src):
        print("missing", name, num)
        continue
    dst = os.path.join(OUT, name + ".ogg")
    # mono, 22 kHz, the trailing silence cut off
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-ac", "1", "-ar", "22050",
                    "-af", "areverse,silenceremove=start_periods=1:start_threshold=-60dB,areverse",
                    "-c:a", "libvorbis", "-q:a", "3", dst], check=True)
total = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT))
print(len(os.listdir(OUT)), "sounds,", total // 1024, "KB")
