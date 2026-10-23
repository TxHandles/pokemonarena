#!/usr/bin/env python3
"""HeartGold / SoulSilver's sounds into the game's Sounds/ folder (short mono OGGs):
  - every move's own sound (The Sounds Resource's "Pokemon HeartGold / SoulSilver - Attack Move Sound Effects", the
    Gen 4 rip): Sounds/Moves/<move>.ogg, the name squashed ("Thunderbolt" -> thunderbolt.ogg, "U-turn" -> uturn.ogg).
    A move in two parts (Thunderbolt_part_1 + _part_2) is joined; a multi-hit move uses its one-hit sound. Each is
    cut to 2.5 seconds at most (a real-time battle uses moves often).
  - from "Miscellaneous Sound Effects": the Pokemon Center's healing jingle and the item-get jingle.
    python tools/hgss_sounds.py <attack move sounds folder> <miscellaneous sound effects folder>
"""
import os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOVES, MISC = sys.argv[1], sys.argv[2]
OUT = os.path.join(ROOT, "Sounds")
os.makedirs(os.path.join(OUT, "Moves"), exist_ok=True)
squash = lambda s: re.sub(r"[^a-z0-9]", "", s.lower())


def ogg(srcs, dst, cap=None):
    """srcs joined, mono 22 kHz, silence trimmed at both ends, at most `cap` seconds (faded out)"""
    inputs = []
    for s in srcs: inputs += ["-i", s]
    chain = "".join(f"[{i}:a]" for i in range(len(srcs))) + f"concat=n={len(srcs)}:v=0:a=1[c];[c]"
    chain += "silenceremove=start_periods=1:start_threshold=-55dB,areverse,silenceremove=start_periods=1:start_threshold=-55dB,areverse"
    if cap: chain += f",atrim=0:{cap},afade=t=out:st={cap - 0.3}:d=0.3"
    chain += "[o]"
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error"] + inputs + ["-filter_complex", chain, "-map", "[o]", "-ac", "1", "-ar", "22050",
                    "-c:a", "libvorbis", "-q:a", "2", dst], check=True)


# the moves: group the files by move
groups = {}
for f in sorted(os.listdir(MOVES)):
    if not f.lower().endswith(".mp3"): continue
    base = f[:-4]
    m = re.match(r"(.+?)_(part_(\d)|1hit|2hits|turn_damage)$", base)
    name, part = (m.group(1), m.group(2)) if m else (base, "")
    groups.setdefault(squash(name), {})[part] = os.path.join(MOVES, f)
n = 0
for key, parts in sorted(groups.items()):
    if "" in parts: srcs = [parts[""]]
    elif "part_1" in parts: srcs = [parts[k] for k in sorted(p for p in parts if p.startswith("part_"))]
    elif "1hit" in parts: srcs = [parts["1hit"]]
    else: srcs = [next(iter(parts.values()))]
    ogg(srcs, os.path.join(OUT, "Moves", key + ".ogg"), cap=2.5)
    n += 1
# the jingles
for src, name in [("recovery.wav", "center_heal"), ("item_get.wav", "item_get")]:
    if os.path.exists(os.path.join(MISC, src)): ogg([os.path.join(MISC, src)], os.path.join(OUT, name + ".ogg"))
size = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(OUT) for f in fs)
print(n, "move sounds;", size // 1024, "KB in Sounds/")
