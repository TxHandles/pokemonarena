#!/usr/bin/env python3
"""Shoal Cave's tides (Emerald: pret/pokeemerald's data/maps/ShoalCave_*/scripts.inc, src/field_specials.c).

In Emerald the cave's Entrance Room and Inner Room swap their layouts as you walk in: high tide from 3:00 to 9:00 and
from 15:00 to 21:00, low tide the rest of the day. Here they're the "High Tide" and "Low Tide" maps, and every way in
picks the one for the time on the story clock ("if = hightide"). The High Tide rooms get the Low Tide rooms' doors,
people and signs (its items stay in the Low Tide rooms). The Shoal Salt and Shoal Shells come back each day ("daily"), and the old man in the Entrance Room
makes a Shell Bell from four of each ("shellbell").

    emerald_shoal.py World/hoenn.txt > patch.txt     (reads the generated maps; prints World/patches_hoenn.txt lines)
"""
import re
import sys

LOW = {"Shoal Cave Low Tide Entrance Room": "Shoal Cave High Tide Entrance Room",
       "Shoal Cave Low Tide Inner Room": "Shoal Cave High Tide Inner Room"}


GRID = {}
def blocks(text):
    out = {}
    for b in re.split(r"\n(?=\[map\])", text):
        m = re.search(r"^name = (.+)$", b, re.M)
        if m:
            out[m.group(1)] = b.split("grid:")[0].split("\n")
            GRID[m.group(1)] = b.split("grid:\n")[1].strip().split("\n") if "grid:\n" in b else []
    return out


def under_water(name, line):   # (at high tide: a hole down, a Shoal Salt spot - under the sea)
    x, y = map(int, line.split()[2:4])
    g = GRID.get(name, [])
    return 0 <= y < len(g) and 0 <= x < len(g[y]) and g[y][x] in "wWd"


def entries(lines):
    """Its doors, people, items and signs, each with any if / unless lines after it."""
    out, cur = [], None
    for l in lines:
        if re.match(r"(door|npc|item|sign|spot) = ", l):
            cur = [l]; out.append(cur)
        elif cur is not None and re.match(r"(if|unless|fightopts|rematch) = ", l):
            cur.append(l)
        else:
            cur = None
    return out


def to_high(l):
    for lo, hi in LOW.items():
        l = l.replace("warp|%s|" % lo, "warp|%s|" % hi)
    return l


def tide_door(e):
    """A way into the Entrance or Inner Room: the High Tide one first while the tide's in."""
    if not any("warp|%s|" % lo in e[0] for lo in LOW):
        return None
    return [to_high(e[0]), "if = hightide"] + e[1:], e


def salt(e, n):
    """A Shoal Salt / Shell spot (Emerald: a sign once it's been taken): it's back every day."""
    m = re.match(r"sign = (\d+) (\d+) There was (?:some |a )(Shoal Salt|Shoal Shell) here", e[0])
    if not m:
        return None
    x, y, it = m.groups()
    return ["drop sign %s %s" % (x, y), "spot = %s %s daily|%s|shoal%d|There was %s here. But there's nothing here now - come back tomorrow." %
            (x, y, it, n, "some Shoal Salt" if it == "Shoal Salt" else "a Shoal Shell")]


def main():
    B = blocks(open(sys.argv[1]).read())
    L = ["", "# ------------------------------------------------------------------ Shoal Cave's tides (tools/emerald_shoal.py)"]
    n = 0
    shoal = sorted(k for k in B if k.startswith("Shoal Cave Low Tide") or k == "Route 125")
    salts = {}
    for name in shoal:
        P = []
        for e in entries(B[name]):
            if e[0].startswith("door"):
                t = tide_door(e)
                if t:
                    x, y = e[0].split()[2:4]
                    P += ["drop door %s %s" % (x, y)] + t[0] + t[1]
            elif e[0].startswith("sign"):
                s = salt(e, n)
                if s:
                    salts.setdefault(name, []).append((e, n)); n += 1
                    P += s
            elif e[0].startswith("npc") and "Shoal Salt and Shoal Shells" in e[0]:
                x, y = e[0].split()[2:4]
                P += ["drop npc %s %s" % (x, y), " ".join(e[0].split()[:7]) + " shellbell"]
        if P:
            L += ["[patch %s]" % name] + P
    for lo, hi in LOW.items():   # the High Tide rooms: the same doors, people and things
        L += ["[patch %s]" % hi, "wild = Shoal Cave"]
        for e in entries(B[lo]):
            if re.match(r"(door|sign) = ", e[0]) and under_water(hi, e[0]):
                continue
            if e[0].startswith("door"):
                L += [to_high(e[0])] + e[1:]
            elif e[0].startswith("sign"):
                k = [k for s, k in salts.get(lo, []) if s[0] == e[0]]
                if k:
                    L += salt(e, k[0])[1:]
                else:
                    L += e
            elif e[0].startswith("npc") and "Shoal Salt and Shoal Shells" in e[0]:
                L += [" ".join(e[0].split()[:7]) + " shellbell"]
            elif not e[0].startswith("item"):   # (an item: just the one, in the Low Tide room)
                L += e
    print("\n".join(L))


if __name__ == "__main__":
    main()
