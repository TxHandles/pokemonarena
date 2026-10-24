#!/usr/bin/env python3
"""FireRed's Fame Checker (pret/pokefirered): World/fame.txt - the sixteen people, the six things you can learn about
each (data/text/fame_checker.inc) and where you learn them (every script with "famechecker PERSON, n", followed back
to the person, sign or map that runs it).

    person = KEY|Name|chars.txt sprite
    fact = KEY|n|question|answer|where (the origin's place)|who (the origin's person or thing)
    at = Our Map Name|x|y|KEY|n          talking to whoever stands there / reading the sign there
    enter = Our Map Name|KEY|n           arriving on that map (a scene there)

    fame_checker.py <pokefirered>
"""
import json
import os
import re
import sys

ROOT = sys.argv[1]
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PEOPLE = [("OAK", "ProfOak", "Prof. Oak", "prof_oak"), ("DAISY", "Daisy", "Daisy", "daisy"), ("BROCK", "Brock", "Brock", "brock"),
          ("MISTY", "Misty", "Misty", "misty"), ("LTSURGE", "LtSurge", "Lt. Surge", "lt_surge"), ("ERIKA", "Erika", "Erika", "erika"),
          ("KOGA", "Koga", "Koga", "koga"), ("SABRINA", "Sabrina", "Sabrina", "sabrina"), ("BLAINE", "Blaine", "Blaine", "blaine"),
          ("LORELEI", "Lorelei", "Lorelei", "lorelei"), ("BRUNO", "Bruno", "Bruno", "bruno"), ("AGATHA", "Agatha", "Agatha", "agatha"),
          ("LANCE", "Lance", "Lance", "lance"), ("BILL", "Bill", "Bill", "bill"), ("MRFUJI", "MrFuji", "Mr. Fuji", "mr_fuji"),
          ("GIOVANNI", "Giovanni", "Giovanni", "giovanni")]


def strings(path):
    out, cur = {}, None
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"^(\w+)::?", line)
        if m:
            cur = m.group(1); out[cur] = ""; continue
        m = re.match(r'\s*\.string\s+"(.*)"', line)
        if m and cur:
            out[cur] += m.group(1)
    return out


def cap(s):
    s = s.replace("LT. ", "Lt. ").replace("MT. ", "Mt. ").replace("MR. ", "Mr. ").replace("CO.", "Co.").replace("SS ANNE", "S.S. Anne").replace("POKeMANIAC", "PokeManiac").replace("PokeMANIAC", "PokeManiac")
    return re.sub(r"\b[A-Z][A-Z']{2,}\b", lambda m: m.group(0).capitalize(), s)


def clean(s):
    s = s.replace("{RIVAL}", "Blue").replace("{PLAYER}", "{PLAYER}")
    s = re.sub(r"\{(?!PLAYER)[^}]*\}", "", s).replace("$", "").replace("POKéMON", "Pokemon").replace("POKé", "Poke").replace("é", "e").replace("…", "...")
    pages = [re.sub(r"\s+", " ", re.sub(r"\\[nl]", " ", p)).strip() for p in s.split("\\p")]
    fix = lambda t: cap(t).replace("Ss Anne", "S.S. Anne").replace("S.s. Anne", "S.S. Anne").replace("LT. ", "Lt. ").replace("MT. ", "Mt. ").replace("S.S. ANNE", "S.S. Anne")
    return [fix(p) for p in pages if p]


def scripts():
    """label -> its lines, and where each label lives"""
    body, home = {}, {}
    files = []
    for base, _, fs in os.walk(os.path.join(ROOT, "data")):
        files += [os.path.join(base, f) for f in fs if f.endswith(".inc") and ("maps" in base or "scripts" in base)]
    for p in files:
        cur = None
        for line in open(p, encoding="utf-8", errors="replace"):
            m = re.match(r"^(\w+)::?\s*$", line)
            if m:
                cur = m.group(1); body[cur] = []; home[cur] = p; continue
            if cur:
                body[cur].append(line.strip())
    return body, home


def main():
    T = strings(os.path.join(ROOT, "data/text/fame_checker.inc"))
    L = ["# FireRed's Fame Checker (made by tools/fame_checker.py from pret/pokefirered)."]
    for key, lab, name, spr in PEOPLE:
        L.append("person = %s|%s|%s" % (key, name, spr))
        for n in range(6):
            pages = clean(T.get("gFameCheckerFlavorText_%s%d" % (lab, n), ""))
            q, a = (pages[0], " ".join(pages[1:])) if len(pages) > 1 else ("", " ".join(pages))
            where = cap(T.get("gFameCheckerFlavorTextOriginLocation_%s%d" % (lab, n), "").replace("$", "")).strip()
            who = cap(T.get("gFameCheckerFlavorTextOriginObjectName_%s%d" % (lab, n), "").replace("$", "")).strip()
            where = re.sub(r"\{[^}]*\}", "", where).replace("Pokemon", "Pokemon").replace("POKéMON", "Pokemon")
            who = re.sub(r"\{[^}]*\}", "", who).replace("POKéMON", "Pokemon")
            L.append("fact = %s|%d|%s|%s|%s|%s" % (key, n, q, a, where, who))
    # where each is learned: the script, followed back to whatever runs it
    body, home = scripts()
    callers = {}
    for lab, lines in body.items():
        for ln in lines:
            for tok in re.findall(r"\b(\w+)\b", ln):
                if tok in body and tok != lab:
                    callers.setdefault(tok, set()).add(lab)
    ours = {}
    for b in re.split(r"\n(?=\[map\])", open(os.path.join(HERE, "World/world.txt")).read()):
        m, im = re.search(r"^name = (.+)$", b, re.M), re.search(r"^image = maps/(\S+)\.png$", b, re.M)
        if m and im:
            ours[im.group(1)] = m.group(1)
    roots = {}   # label -> [(map dir, kind, x, y)]
    for d in sorted(os.listdir(os.path.join(ROOT, "data/maps"))):
        p = os.path.join(ROOT, "data/maps", d, "map.json")
        if not os.path.exists(p):
            continue
        j = json.load(open(p))
        for o in j.get("object_events", []):
            roots.setdefault(o.get("script", ""), []).append((d, "at", o["x"], o["y"]))
        for o in j.get("bg_events", []):
            if o.get("script"):
                roots.setdefault(o["script"], []).append((d, "at", o["x"], o["y"]))
        for o in j.get("coord_events", []):
            if o.get("script"):
                roots.setdefault(o["script"], []).append((d, "enter", 0, 0))
        roots.setdefault(d + "_MapScripts", []).append((d, "enter", 0, 0))
    got = set()
    missing = 0
    for lab, lines in body.items():
        for ln in lines:
            m = re.match(r"famechecker FAMECHECKER_(\w+), (\d)", ln)
            if not m:
                continue
            seen, stack, found = set(), [lab], []
            while stack:
                x = stack.pop()
                if x in seen:
                    continue
                seen.add(x)
                if x in roots:
                    found += roots[x]
                # map-script tables: a label named after its map's on-frame / on-transition scripts
                mm = re.match(r"^([A-Za-z0-9]+_[A-Za-z0-9_]*?)_(?:On\w+|EventScript_\w+)$", x)
                if mm and not found and x not in callers:
                    dd = os.path.basename(os.path.dirname(home[x]))
                    if os.path.exists(os.path.join(ROOT, "data/maps", dd, "map.json")):
                        found.append((dd, "enter", 0, 0))
                stack += list(callers.get(x, ()))
            if not found:
                missing += 1
                sys.stderr.write("no place for %s %s (%s)\n" % (m.group(1), m.group(2), lab))
            for d, kind, x, y in found:
                if d not in ours:
                    continue
                line = ("at = %s|%d|%d|%s|%s" % (ours[d], x, y, m.group(1), m.group(2))) if kind == "at" else \
                       ("enter = %s|%s|%s" % (ours[d], m.group(1), m.group(2)))
                if line not in got:
                    got.add(line); L.append(line)
    open(os.path.join(HERE, "World/fame.txt"), "w").write("\n".join(L) + "\n")
    print(len(got), "places,", missing, "without one")


if __name__ == "__main__":
    main()
