#!/usr/bin/env python3
"""Adds the Kalos (Gen 6) and Alola (Gen 7) Pokemon to the wild areas of every region in wild.txt.
There is no Kalos or Alola region, so each newcomer lives where a local Pokemon fills the same niche
(Bunnelby digs where Rattata and Bidoof live, Wishiwashi bites where Magikarp does, Carbink sparkles in the
caves with Nosepass and Sableye...). It shows up with the same method and levels as that local Pokemon;
the newcomers get about a fifth of its weight between them, so the local tables keep their feel.
Starters, evolved forms (they come from evolving), legendaries, Ultra Beasts and mythicals are left out.
Re-running replaces the earlier lines (they follow a '# Kalos and Alola' comment).

    python tools/add_kalos_alola.py [wild.txt]
"""
import re
import sys

# local Pokemon -> the newcomers that share its niche
NEWCOMERS = {
    # rodents and other everyday Normal types
    "Rattata": ["Bunnelby", "Yungoos"], "Sentret": ["Bunnelby", "Yungoos"], "Zigzagoon": ["Bunnelby", "Yungoos"],
    "Bidoof": ["Bunnelby"], "Patrat": ["Yungoos"],
    "Raticate": ["Diggersby", "Gumshoos"], "Furret": ["Diggersby"], "Linoone": ["Gumshoos"], "Bibarel": ["Diggersby"],
    "Watchog": ["Gumshoos"],
    # birds
    "Pidgey": ["Fletchling"], "Starly": ["Fletchling"], "Pidove": ["Fletchling"], "Taillow": ["Fletchling", "Pikipek"],
    "Pidgeotto": ["Fletchinder"], "Staravia": ["Fletchinder"], "Tranquill": ["Fletchinder"],
    "Spearow": ["Pikipek"], "Hoothoot": ["Pikipek"], "Fearow": ["Trumbeak"], "Noctowl": ["Trumbeak"],
    "Wingull": ["Sandygast"],
    # bugs
    "Caterpie": ["Scatterbug", "Grubbin"], "Wurmple": ["Scatterbug", "Grubbin"], "Sewaddle": ["Scatterbug", "Grubbin"],
    "Metapod": ["Spewpa"], "Silcoon": ["Spewpa"], "Cascoon": ["Spewpa"], "Swadloon": ["Spewpa"], "Kakuna": ["Spewpa"],
    "Butterfree": ["Vivillon"], "Beautifly": ["Vivillon"],
    "Ledyba": ["Cutiefly"], "Combee": ["Cutiefly"], "Cottonee": ["Cutiefly", "Flabebe"],
    "Volbeat": ["Oricorio"], "Illumise": ["Oricorio"], "Ledian": ["Oricorio"],
    "Surskit": ["Dewpider"], "Poliwag": ["Dewpider"],
    # grass and flowers
    "Oddish": ["Flabebe", "Bounsweet"], "Budew": ["Flabebe"], "Petilil": ["Flabebe"],
    "Bellossom": ["Comfey"], "Roselia": ["Comfey"],
    "Bellsprout": ["Fomantis"], "Sunkern": ["Fomantis"], "Exeggcute": ["Bounsweet"], "Cherubi": ["Bounsweet"],
    "Paras": ["Morelull"], "Shroomish": ["Morelull"], "Foongus": ["Morelull"],
    # field and mountain
    "Tauros": ["Skiddo"], "Miltank": ["Skiddo"], "Deerling": ["Skiddo"], "Bouffalant": ["Skiddo"],
    "Rhyhorn": ["Mudbray"], "Phanpy": ["Mudbray"], "Hippopotas": ["Mudbray"],
    "Mankey": ["Pancham", "Passimian"], "Machop": ["Pancham"], "Makuhita": ["Pancham"], "Timburr": ["Pancham"],
    "Meditite": ["Pancham"], "Aipom": ["Passimian", "Oranguru"], "Pansage": ["Passimian"], "Pansear": ["Passimian"],
    "Panpour": ["Passimian"], "Primeape": ["Hawlucha"], "Machoke": ["Hawlucha"],
    "Teddiursa": ["Stufful"], "Slakoth": ["Komala"], "Vigoroth": ["Komala"],
    "Growlithe": ["Litleo", "Furfrou"], "Houndour": ["Litleo"], "Poochyena": ["Rockruff"], "Lillipup": ["Rockruff", "Furfrou"],
    "Snubbull": ["Furfrou", "Swirlix"], "Herdier": ["Furfrou"],
    "Meowth": ["Espurr"], "Glameow": ["Espurr"], "Purrloin": ["Espurr"], "Skitty": ["Espurr", "Swirlix"],
    "Jigglypuff": ["Spritzee"], "Audino": ["Swirlix"],
    "Pikachu": ["Dedenne", "Togedemaru"], "Pachirisu": ["Dedenne", "Togedemaru"], "Emolga": ["Dedenne", "Togedemaru"],
    "Plusle": ["Togedemaru"], "Minun": ["Togedemaru"],
    "Sandile": ["Helioptile"], "Trapinch": ["Helioptile"], "Cacnea": ["Helioptile"], "Electrike": ["Helioptile"],
    # ghosts
    "Gastly": ["Pumpkaboo", "Honedge"], "Duskull": ["Honedge", "Pumpkaboo"], "Litwick": ["Honedge", "Pumpkaboo"],
    "Shuppet": ["Phantump"], "Misdreavus": ["Phantump", "Mimikyu"], "Haunter": ["Mimikyu"],
    # caves, rock, steel and ice
    "Zubat": ["Noibat"], "Woobat": ["Noibat"],
    "Nosepass": ["Carbink"], "Sableye": ["Carbink"], "Boldore": ["Carbink"], "Graveler": ["Carbink"],
    "Onix": ["Tyrunt"], "Aerodactyl": ["Tyrunt"], "Snorunt": ["Amaura", "Bergmite"], "Swinub": ["Amaura", "Bergmite"],
    "Vanillite": ["Bergmite"],
    "Clefairy": ["Minior"], "Lunatone": ["Minior"], "Solrock": ["Minior"],
    "Magnemite": ["Klefki"], "Klink": ["Klefki"], "Bronzor": ["Klefki"], "Mawile": ["Klefki"],
    "Slugma": ["Salandit"], "Numel": ["Salandit"], "Torkoal": ["Salandit", "Turtonator"], "Magcargo": ["Turtonator"],
    # dragons
    "Dratini": ["Goomy"], "Bagon": ["Jangmo-o"], "Gible": ["Jangmo-o"], "Axew": ["Goomy", "Jangmo-o"],
    "Altaria": ["Drampa"], "Tropius": ["Drampa"],
    # psychic monkeys and bears
    "Girafarig": ["Oranguru"], "Ursaring": ["Stufful"], "Cubchoo": ["Bergmite", "Stufful"], "Spinda": ["Stufful"],
    # water
    "Tentacool": ["Inkay"], "Frillish": ["Inkay"], "Remoraid": ["Inkay", "Bruxish"], "Qwilfish": ["Bruxish"],
    "Horsea": ["Skrelp"], "Krabby": ["Clauncher", "Crabrawler", "Wimpod"], "Corphish": ["Clauncher", "Crabrawler", "Wimpod"],
    "Shellder": ["Binacle", "Mareanie"], "Clamperl": ["Binacle"], "Corsola": ["Binacle", "Mareanie"],
    "Staryu": ["Pyukumuku"], "Shellos": ["Pyukumuku", "Sandygast"],
    "Magikarp": ["Wishiwashi"], "Goldeen": ["Wishiwashi"], "Basculin": ["Wishiwashi"],
    "Wailmer": ["Dhelmise"], "Relicanth": ["Dhelmise"],
}
# single-stage newcomers as strong as a final evolution: they wait for the region's third badge
STRONG = {"Passimian", "Oranguru", "Hawlucha", "Drampa", "Turtonator", "Dhelmise", "Mimikyu", "Bruxish", "Komala",
          "Togedemaru", "Klefki", "Carbink", "Minior", "Comfey", "Oricorio", "Pyukumuku", "Dedenne"}
HEAD = "# Kalos and Alola Pokemon here (Gen 6 and 7 newcomers in the same niche as the locals)"
LINE = re.compile(r"pokemon\s*=\s*([^,]+),\s*([^,]+),\s*(\d+),\s*([\d-]+)(.*)$")


def main(path):
    lines = open(path, encoding="utf-8").read().split("\n")
    cleaned, skip = [], False
    for ln in lines:   # drop the earlier newcomer blocks
        if ln.startswith(HEAD):
            skip = True
            continue
        if skip and ln.startswith("pokemon =") and ln.rstrip().endswith(", newcomer"):
            continue
        skip = False
        cleaned.append(ln)
    out, entries, placed = [], [], {}

    def flush():
        merged = {}
        for method, name, rate, lv, extra in entries:
            ns = NEWCOMERS.get(name)
            if not ns:
                continue
            share = max(len(ns), round(rate / 5))
            badges = re.search(r"badges\s*\d+", extra)
            for n in ns:
                key = (method, n, lv, badges.group(0) if badges else "")
                merged[key] = merged.get(key, 0) + max(1, share // len(ns))
        here = {e[1] for e in entries}
        extra = []
        for (m, n, lv, b), r in merged.items():
            if n in here:
                continue
            if n in STRONG and not b:
                b = "badges 3"
            extra.append(f"pokemon = {m}, {n}, {r}, {lv}" + (f", {b}" if b else "") + ", newcomer")
            placed[n] = placed.get(n, 0) + 1
        if extra:
            while out and out[-1] == "":
                out.pop()
            out.append(HEAD)
            out.extend(extra)
            out.append("")
        entries.clear()

    for ln in cleaned:
        if ln.startswith("[") or ln.startswith("# ===="):
            flush()
        m = LINE.match(ln)
        if m and "cousin" not in m.group(5):
            entries.append((m.group(1).strip(), m.group(2).strip(), int(m.group(3)), m.group(4).strip(), m.group(5)))
        out.append(ln)
    flush()
    open(path, "w", encoding="utf-8").write("\n".join(out))
    allnew = sorted({n for ns in NEWCOMERS.values() for n in ns})
    print(len(placed), "of", len(allnew), "newcomers placed;", sum(placed.values()), "area entries")
    missing = [n for n in allnew if n not in placed]
    if missing:
        print("not placed:", ", ".join(missing))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "wild.txt")
