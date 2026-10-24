#!/usr/bin/env python3
"""The Lilycove Lady's quizzes and favors (pret/pokeemerald's src/data/lilycove_lady.h): World/lilycove_lady.txt.

    quiz ~ question ~ choice ~ choice ~ choice ~ answer ~ prize
    favor ~ the kind of thing she wants ~ the items that count ~ her prize

    emerald_lilycove_lady.py <pokeemerald>
"""
import os
import re
import sys

ROOT = sys.argv[1]
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = open(os.path.join(ROOT, "src/data/lilycove_lady.h")).read()
WORDS = {}
for f in os.listdir(os.path.join(ROOT, "src/data/easy_chat")):
    for k, v in re.findall(r'gEasyChatWord_(\w+)\[\] = _\("([^"]*)"\)', open(os.path.join(ROOT, "src/data/easy_chat", f)).read()):
        WORDS[k.upper()] = v


def ours():
    it = open(os.path.join(HERE, "src/items.cpp")).read()
    return set(re.findall(r'(?:add|med|bx|pu|val)\("([^"]+)"', it)) | set(re.findall(r'"(\w+ Berry)"', it))


OURS = ours()


def cap(s):
    return " ".join(p.capitalize() if len(p) > 2 or p not in ("TV", "PC") else p for p in s.replace("POKéMON", "Pokemon").replace("POKé", "Poke").split())


def ec(tok):
    m = re.match(r"EC_MOVE2?\((\w+)\)", tok)
    if m:
        return cap(m.group(1).replace("_", " "))
    if tok == "EC_EMPTY_WORD":
        return ""
    k = tok.replace("EC_WORD_", "")
    if k == "QUES":
        return "?"
    key = k.replace("_", "")
    for w, v in WORDS.items():
        if w.replace("_", "") == key:
            return cap(v)
    return cap(k.replace("_", " "))


def item(tok):
    n = cap(tok.replace("ITEM_", "").replace("_", " ")).replace("Pp", "PP").replace("Tm Hyper Beam", "TM163 Hyper Beam")
    n = {"Poke Ball": "Poke Ball", "X Defend": "X Defense", "Energy Powder": "Energy Powder"}.get(n, n)
    return n if n in OURS or n.startswith("TM") else ""


def main():
    L = ["# The Lilycove Lady (made by tools/emerald_lilycove_lady.py from pret/pokeemerald's src/data/lilycove_lady.h)."]
    qs = [re.findall(r"(EC_\w+(?:\(\w+\))?)", b) for b in re.findall(r"sQuizLadyQuestion\d+\[QUIZ_QUESTION_LEN\] =\s*\{([^}]*)\}", SRC)]
    ans = re.findall(r"(EC_\w+(?:\(\w+\))?)", SRC.split("sQuizLadyQuizAnswers[] =")[1].split("};")[0])
    prizes = re.findall(r"ITEM_\w+", SRC.split("sQuizLadyPrizes[] =")[1].split("};")[0])
    for q, a, p in zip(qs, ans, prizes):
        words = [ec(t) for t in q]
        question = " ".join(w for w in words[:6] if w).replace(" ?", "?")
        L.append(" ~ ".join(["quiz", question] + words[6:9] + [ec(a), item(p) or "Rare Candy"]))
    reqs = re.findall(r"gText_FavorLady_(\w+)", SRC.split("sFavorLadyRequests[] =")[1].split("};")[0])
    fprizes = re.findall(r"ITEM_\w+", SRC.split("sFavorLadyPrizes[] =")[1].split("};")[0])
    for k, r in enumerate(reqs):
        lst = re.findall(r"ITEM_\w+", SRC.split("sFavorLadyAcceptedItems_%s[] =" % r)[1].split("};")[0])
        names = [n for n in (item(t) for t in lst if t != "ITEM_NONE") if n]
        L.append(" ~ ".join(["favor", {"Whamish": "wham-ish"}.get(r, r.lower()), ", ".join(names), item(fprizes[k % len(fprizes)]) or "Rare Candy"]))
    open(os.path.join(HERE, "World", "lilycove_lady.txt"), "w").write("\n".join(L) + "\n")
    print("\n".join(L))


if __name__ == "__main__":
    main()
