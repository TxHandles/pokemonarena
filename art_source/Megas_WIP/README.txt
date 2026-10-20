Mega Evolution sprites - parked work in progress (not used by the game yet).

sprites/<Mega name>/  the game's file layout: walking frames (front, left, right, back x2), the menu portrait
                      (Pokemon-NNN-Mega-X.png) and shiny.txt (normal colour > shiny colour).
sprites_round4/       the earlier hand-made frames and chart portraits (kept for reference).
Mega_*_sheet.*        the user's sprite sheets: rows front / left / right / back, 4 walk frames, then 4 shiny.
megas_preview.png     everything at game size; gyarados_revised.png the shrunk Gyarados.
tools/                sheet2frames.py cuts a sheet (2x sheets halved, nearest-neighbour);
                      apply_sheets.py writes the frames and shiny.txt (from the sheet's shiny columns, plus
                      the portrait's colours from the official shiny sprite); portraits.py puts in the official
                      Gen 7 menu icons (the same source as the Gen 4-6 portraits); shiny_round4.py makes
                      shiny.txt for the round-4 Megas from the official shiny sprites; preview2.py the previews.

From the user's sheets: Venusaur, Charizard X, Charizard Y, Blastoise, Alakazam, Slowbro, Gengar (its second sheet), Gyarados
(shrunk to 2/3), Kangaskhan, Mewtwo Y, Pinsir, Beedrill
(its small sheet: back, front, left; right is the left view mirrored).  Aerodactyl.  Round 4 (approved): Mewtwo X, Pidgeot.
All 15 final (Oct 20).
