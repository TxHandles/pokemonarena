#!/usr/bin/env python3
"""The in-game trades of FireRed / LeafGreen, HeartGold / SoulSilver (Johto) and Emerald.

  - World/trades.txt: each trade - the Pokemon they want, the one you get (its nickname, original trainer, IVs, held
    item) and what they say. FireRed's and Emerald's come from pret/pokefirered's src/data/ingame_trades.h and
    pret/pokeemerald's src/data/trade.h with the trade scripts' lines; HeartGold's four Johto trades from
    pret/pokeheartgold's scripts and messages (their Pokemon's data is in a binary archive, so their IVs are even).
  - prints World/patches*.txt lines turning each trader into "trade|<key>" (overworld.cpp; the trade animation is
    screens_npctrade.cpp).

    ingame_trades.py <pokefirered> <pokeemerald> <hgss scripts dir> <hgss models> <hgss cache> kanto|johto|hoenn > patch.txt
    (the table is written by every run)
"""
import json
import os
import re
import sys

FR, EM, HGS, HGM, HGC, REGION = sys.argv[1:7]
HERE = os.path.dirname(os.path.abspath(__file__))
WORLD = os.path.join(os.path.dirname(HERE), "World")
SPECIES = {"MR_MIME": "Mr. Mime", "NIDORAN_F": "Nidoran F", "NIDORAN_M": "Nidoran M", "FARFETCHD": "Farfetch'd"}
ITEMS = {"STICK": "Stick", "TINY_MUSHROOM": "Tiny Mushroom", "STARDUST": "Stardust", "CHESTO_BERRY": "Chesto Berry"}   # (mail: dropped)


def species(s):
    return SPECIES.get(s, "-".join(p.capitalize() for p in s.split("_")))


def clean(s):
    s = s.replace("$", "").replace("POKéMON", "Pokemon").replace("POKé", "Poke").replace("é", "e").replace("…", "...")
    s = re.sub(r"\\p", "|", s)
    s = re.sub(r"\\[nl]", " ", s)
    s = re.sub(r"\b[A-Z][A-Z']{2,}\b", lambda m: m.group(0).capitalize(), s)
    s = s.replace("{Str_Var_1}", "{GIVE}").replace("{Str_Var_2}", "{GET}").replace("{STR_VAR_1}", "{GIVE}").replace("{STR_VAR_2}", "{GET}")
    s = s.replace("{PLAYER}", "{PLAYER}")
    s = re.sub(r"\{(?!GIVE|GET|PLAYER)[^}]*\}", "", s)
    return "|".join(re.sub(r"\s+", " ", p).strip() for p in s.split("|") if p.strip())


def texts(root):
    T = {}
    for base, _, fs in os.walk(os.path.join(root, "data")):
        for f in fs:
            if not f.endswith(".inc"):
                continue
            cur = None
            for line in open(os.path.join(base, f), encoding="utf-8", errors="replace"):
                m = re.match(r"^(\w+)::?\s*$", line)
                if m:
                    cur = m.group(1); T.setdefault(cur, ""); continue
                m = re.match(r'\s*\.string\s+"(.*)"', line)
                if m and cur:
                    T[cur] += m.group(1)
    return T


def trades_h(path):
    src = open(path).read()
    out = {}
    for key, body in re.findall(r"\[INGAME_TRADE_(\w+)\]\s*=\s*\{(.*?)\n    \}", src, re.S):
        body = body.split("#elif")[0] + ",\n"   # (FireRed's side)
        g = lambda k: (re.search(r"\." + k + r"\s*=\s*(.+?),?\n", body) or [None, ""])[1]
        out[key] = {"nick": re.search(r'\.nickname = _\("([^"]*)"\)', body).group(1),
                    "get": species(g("species").replace("SPECIES_", "")),
                    "give": species(g("requestedSpecies").replace("SPECIES_", "")),
                    "ot": re.search(r'\.otName = _\("([^"]*)"\)', body).group(1).capitalize(),
                    "ivs": " ".join(re.search(r"\.ivs = \{([^}]*)\}", body).group(1).replace(",", " ").split()),
                    "item": ITEMS.get(g("heldItem").replace("ITEM_", ""), "")}
    return out


def body_of(scripts, label):
    return ("\n" + scripts).split("\n" + label + "::")[1].split("::")[0] if ("\n" + label + "::") in ("\n" + scripts) else ""


def decomp_trades(root, table, image_prefix, world_file, T):
    blocks = {}
    for b in re.split(r"\n(?=\[map\])", open(os.path.join(WORLD, world_file)).read()):
        m, im = re.search(r"^name = (.+)$", b, re.M), re.search(r"^image = maps/(\S+)\.png$", b, re.M)
        if m and im:
            blocks[im.group(1)] = (m.group(1), b)
    out = []
    for d in sorted(os.listdir(os.path.join(root, "data/maps"))):
        sp = os.path.join(root, "data/maps", d, "scripts.inc")
        if not os.path.exists(sp) or "INGAME_TRADE_" not in open(sp).read():
            continue
        scripts = open(sp).read()
        for o in json.load(open(os.path.join(root, "data/maps", d, "map.json"))).get("object_events", []):
            body = body_of(scripts, o.get("script", ""))
            m = re.search(r"INGAME_TRADE_(\w+)", body)
            if not m or m.group(1) not in table:
                continue
            key = m.group(1)
            lab = lambda pat: (re.search(pat, body) or [None, ""])[1]
            msg_of = lambda l: (re.search(r"msgbox (\w+)", body_of(scripts, l)) or [None, ""])[1] if l else ""
            t = dict(table[key])
            t["ask"] = clean(T.get(lab(r"msgbox (\w+), MSGBOX_YESNO"), ""))
            t["thanks"] = clean(T.get(lab(r"DoInGameTrade\w*[^\n]*\n(?:[^\n]*\n)*?\s*msgbox (\w+)"), ""))
            t["decline"] = clean(T.get(msg_of(lab(r"goto_if_eq VAR_RESULT, NO, (\w+)")), ""))
            t["wrong"] = clean(T.get(msg_of(lab(r"goto_if_ne VAR_RESULT, VAR_0x8009, (\w+)")), ""))
            t["after"] = clean(T.get(msg_of(lab(r"goto_if_set FLAG_\w+, (\w+)")), ""))
            name, _ = blocks.get(image_prefix + d, (None, None))
            if not name:
                sys.stderr.write("no map for %s\n" % d); continue
            out.append((key, t, name, o["x"], o["y"]))
    return out


def hgss_trades():
    sys.argv = [sys.argv[0], HGS, HGM, HGC]
    sys.path.insert(0, HERE)
    import hgss_world as H
    out = []
    for key, code, give, get, nick, ot, mp, x, y, ids in [
            ("ROCKY", "T22R0601", "Bellsprout", "Onix", "ROCKY", "Kyle", "Violet City Bellsprout Onyx Trade House", 7, 2, (1, 2, 3, 4, 5)),
            ("MUSCLE", "T25R1005", "Drowzee", "Machop", "MUSCLE", "Mike", "Goldenrod Dept. Store 5F", 13, 5, (9, 10, 11, 12, 13)),
            ("BILLY", "T26R0301", "Krabby", "Voltorb", "BILLY", "Tim", "Olivine City North House", 5, 2, (0, 1, 3, 2, 4)),
            ("DORIS", "T30R0301", "Dragonair", "Dodrio", "DORIS", "Emy", "Blackthorn City East House", 1, 2, (0, 1, 2, 3, 4))]:
        _, bank = H.load_script(code)
        M = H.load_msgs(bank)
        msg = lambda n: H.clean(M.get("%s_%05d" % (bank, n), "")).replace("㊛", "").replace("㊚", "")
        sw = lambda s: s.replace(give, "{GIVE}").replace(get, "{GET}").replace(give.upper(), "{GIVE}").replace(get.upper(), "{GET}")
        ask, thanks, wrong, decline, after = [sw(msg(n)) for n in ids]
        out.append((key, {"give": give, "get": get, "nick": nick, "ot": ot, "ivs": "20 20 20 20 20 20", "item": "",
                          "ask": ask, "thanks": thanks, "decline": decline, "wrong": wrong, "after": after}, mp, x, y))
    return out


def main():
    all_trades = []
    for root, img, wf, reg in [(FR, "", "world.txt", "kanto"), (EM, "em_", "hoenn.txt", "hoenn")]:
        T = texts(root)
        table = trades_h(os.path.join(root, "src/data/ingame_trades.h" if root == FR else "src/data/trade.h"))
        all_trades += [(reg,) + t for t in decomp_trades(root, table, img, wf, T)]
    all_trades += [("johto",) + t for t in hgss_trades()]
    L = ["# The in-game trades (made by tools/ingame_trades.py). {GIVE} / {GET}: the Pokemon they want / the one they give.",
         "# Each line: key | give | get | nickname | original trainer | IVs | held item | ask | thanks | decline | not that one | after"]
    for reg, key, t, *_ in all_trades:
        L.append(" ~ ".join([key, t["give"], t["get"], t["nick"], t["ot"], t["ivs"], t["item"], t["ask"], t["thanks"], t["decline"], t["wrong"], t["after"]]))
    open(os.path.join(WORLD, "trades.txt"), "w").write("\n".join(L) + "\n")
    P = ["", "# ------------------------------------------------------------------ in-game trades (tools/ingame_trades.py)"]
    cur = None
    for reg, key, t, mp, x, y in sorted((a for a in all_trades if a[0] == REGION), key=lambda a: a[3]):
        blocks = open(os.path.join(WORLD, {"kanto": "world.txt", "hoenn": "hoenn.txt", "johto": "johto.txt"}[reg])).read()
        b = [b for b in re.split(r"\n(?=\[map\])", blocks) if re.search(r"^name = %s$" % re.escape(mp), b, re.M)][0]
        npc = [l for l in b.split("\n") if l.startswith("npc = %d %d " % (x, y))]
        if not npc:
            sys.stderr.write("no trader at %s %d,%d\n" % (mp, x, y)); continue
        if mp != cur:
            P.append("[patch %s]" % mp); cur = mp
        P += ["drop npc %d %d" % (x, y), " ".join(npc[0].split()[:7]) + " trade|" + key]
    print("\n".join(P))


if __name__ == "__main__":
    main()
