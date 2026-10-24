#!/usr/bin/env python3
"""Emerald's Easy Chat words (pret/pokeemerald's src/data/easy_chat/): World/easychat.txt, a "[Group]" line then its
words, one line, comma-separated. The word picker (screens_easychat.cpp) offers them for the Bard's song, Dewford's
trendy saying, the Lilycove Lady's quiz and Giddy's chatter.

    emerald_easychat.py <pokeemerald>
"""
import os
import re
import sys

ROOT = sys.argv[1]
D = os.path.join(ROOT, "src/data/easy_chat")
WORLD = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "World")
ORDER = [("Pokemon", ["pokemon", "pokemon2"]), ("Trainer", ["trainer"]), ("Status", ["status"]), ("Battle", ["battle"]),
         ("Greetings", ["greetings"]), ("People", ["people"]), ("Voices", ["voices"]), ("Speech", ["speech"]),
         ("Endings", ["endings"]), ("Feelings", ["feelings"]), ("Conditions", ["conditions"]), ("Actions", ["actions"]),
         ("Lifestyle", ["lifestyle"]), ("Hobbies", ["hobbies"]), ("Time", ["time"]), ("Misc", ["misc"]),
         ("Adjectives", ["adjectives"]), ("Events", ["events"]), ("Moves", ["move_1", "move_2"]), ("Trendy Saying", ["trendy_saying"])]
SPECIES = {"MR_MIME": "Mr. Mime", "NIDORAN_F": "Nidoran F", "NIDORAN_M": "Nidoran M", "FARFETCHD": "Farfetch'd", "HO_OH": "Ho-Oh"}
SKIP = {"RED", "GREEN", "FLAME", "GOLD", "LEAF", "SILVER"}


def word(w):
    w = w.replace("POKéMON", "Pokemon").replace("é", "e").replace("…", "...").replace("{", "").replace("}", "")
    return " ".join(p.capitalize() if p.isupper() and len(p) > 1 and p not in ("TV", "PC", "OK", "HP", "PP") else p for p in w.split(" "))


def main():
    out = ["# Emerald's Easy Chat words (made by tools/emerald_easychat.py from pret/pokeemerald)."]
    for name, files in ORDER:
        words = []
        for f in files:
            t = open(os.path.join(D, "easy_chat_group_%s.h" % f)).read()
            if "SPECIES_" in t:
                words += [SPECIES.get(s, "-".join(p.capitalize() for p in s.split("_"))) for s in re.findall(r"SPECIES_(\w+)", t)]
            elif "MOVE_" in t:
                words += [" ".join(p.capitalize() for p in s.split("_")) for s in re.findall(r"MOVE_(\w+)", t)]
            else:
                words += [word(w) for w in re.findall(r'_\("([^"]*)"\)', t) if w not in SKIP]
        seen, uniq = set(), []
        for w in words:
            if w and w not in seen:
                seen.add(w); uniq.append(w.replace(",", ""))
        out += ["[%s]" % name, ", ".join(sorted(uniq, key=str.lower) if name in ("Pokemon", "Moves") else uniq)]
    open(os.path.join(WORLD, "easychat.txt"), "w").write("\n".join(out) + "\n")
    print(len(out) // 2, "groups")


if __name__ == "__main__":
    main()
