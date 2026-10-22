#!/usr/bin/env python3
"""Adds "cousins" to the Kanto wild areas of wild.txt: Pokemon from the later regions that fill the same
niche (Zigzagoon, Sentret, Bidoof and Patrat where Rattata lives...). Each cousin appears with the same
method and levels as its Kanto counterpart; the cousins get a third of its weight between them
(so about a quarter of its encounters go to them).
Re-running replaces the earlier cousin lines (they follow a '# cousins of' comment)."""
import re, sys

COUSINS = {
    "Rattata": ["Sentret", "Zigzagoon", "Bidoof", "Patrat"],
    "Raticate": ["Furret", "Linoone", "Bibarel", "Watchog"],
    "Pidgey": ["Starly", "Pidove"],
    "Pidgeotto": ["Staravia", "Tranquill"],
    "Spearow": ["Taillow"],
    "Fearow": ["Swellow"],
    "Caterpie": ["Wurmple", "Sewaddle"],
    "Weedle": ["Kricketot", "Venipede"],
    "Metapod": ["Silcoon"],
    "Kakuna": ["Cascoon"],
    "Pikachu": ["Plusle", "Minun", "Pachirisu", "Emolga"],
    "Ekans": ["Seviper"],
    "Sandshrew": ["Drilbur"],
    "Mankey": ["Aipom"],
    "Jigglypuff": ["Skitty"],
    "Clefairy": ["Lunatone", "Munna"],
    "Zubat": ["Woobat"],
    "Golbat": ["Swoobat"],
    "Geodude": ["Roggenrola"],
    "Graveler": ["Boldore"],
    "Onix": ["Nosepass"],
    "Paras": ["Shroomish", "Foongus"],
    "Oddish": ["Budew", "Petilil"],
    "Bellsprout": ["Sunkern", "Cherubi"],
    "Meowth": ["Glameow", "Purrloin"],
    "Psyduck": ["Wooper", "Ducklett"],
    "Poliwag": ["Tympole"],
    "Tentacool": ["Frillish", "Finneon"],
    "Magikarp": ["Basculin"],
    "Goldeen": ["Barboach"],
    "Horsea": ["Remoraid"],
    "Krabby": ["Corphish"],
    "Shellder": ["Clamperl"],
    "Staryu": ["Corsola"],
    "Slowpoke": ["Lotad"],
    "Diglett": ["Trapinch"],
    "Voltorb": ["Shinx"],
    "Magnemite": ["Klink"],
    "Electabuzz": ["Electrike"],
    "Grimer": ["Trubbish", "Gulpin"],
    "Koffing": ["Stunky"],
    "Growlithe": ["Houndour"],
    "Ponyta": ["Numel"],
    "Vulpix": ["Darumaka"],
    "Magmar": ["Slugma"],
    "Drowzee": ["Spoink"],
    "Abra": ["Ralts"],
    "Gastly": ["Misdreavus", "Shuppet", "Litwick"],
    "Haunter": ["Duskull"],
    "Cubone": ["Yamask"],
    "Venonat": ["Yanma"],
    "Machop": ["Makuhita", "Timburr"],
    "Tauros": ["Miltank", "Bouffalant"],
    "Scyther": ["Heracross"],
    "Pinsir": ["Heracross"],
    "Chansey": ["Audino"],
    "Exeggcute": ["Seedot"],
    "Rhyhorn": ["Phanpy"],
    "Seel": ["Spheal"],
    "Jynx": ["Snorunt"],
    "Dratini": ["Bagon", "Axew"],
    "Ditto": ["Kecleon"],
    "Doduo": ["Girafarig"],
}

def main(path):
    lines = open(path, encoding="utf-8").read().split("\n")
    out, i, in_kanto = [], 0, True
    # drop earlier cousin blocks
    cleaned = []
    skip = False
    for ln in lines:
        if ln.startswith("# cousins of the Kanto Pokemon here"):
            skip = True
            continue
        if skip and ln.startswith("pokemon =") and ln.rstrip().endswith(", cousin"):
            continue
        skip = False
        cleaned.append(ln)
    lines = cleaned
    area_entries = []
    def flush():
        if not area_entries: return
        merged = {}
        for method, name, rate, lv in area_entries:
            cs = COUSINS.get(name)
            if not cs: continue
            share = max(len(cs), round(rate / 3))
            for c in cs:
                key = (method, c, lv)
                merged[key] = merged.get(key, 0) + max(1, share // len(cs))
        extra = [f"pokemon = {m}, {c}, {r}, {lv}, cousin" for (m, c, lv), r in merged.items()]
        if extra:
            while out and out[-1] == "": out.pop()
            out.append("# cousins of the Kanto Pokemon here (other regions' Pokemon in the same niche)")
            out.extend(extra)
            out.append("")
        area_entries.clear()
    for ln in lines:
        if in_kanto and (ln.startswith("[") or ln.startswith("# ====")):
            flush()
        if ln.startswith("#  JOHTO"):
            in_kanto = False
        if in_kanto:
            m = re.match(r"pokemon\s*=\s*([^,]+),\s*([^,]+),\s*(\d+),\s*([\d-]+)(.*)$", ln)
            if m and "rare" not in m.group(5):
                area_entries.append((m.group(1).strip(), m.group(2).strip(), int(m.group(3)), m.group(4).strip()))
        out.append(ln)
    open(path, "w", encoding="utf-8").write("\n".join(out))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "wild.txt")
