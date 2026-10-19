Mega Evolution sprites - parked work in progress (not used by the game yet).

sprites/<Mega name>/  walking frames (front, left, right, back x2) in the game's file layout,
                      plus the menu portrait (the user's chart). State: review round 4.
charts/               the user's pixel charts, one pixel per square (exact).
megas_v4_*.png        the review sheets: base sprites, the Mega views, the chart, the official sprite.
tools/                the scripts that built them (Python + Pillow): chart2png.py converts a chart
                      screenshot (plain + numbered) into a sprite; m_*.py are the per-Mega specs.

Approved as-is: Charizard X, Alakazam, Slowbro, Pinsir, Aerodactyl, Mewtwo X.
Still need work (walking views): Charizard Y, Blastoise, Beedrill, Gengar, Kangaskhan (baby's face),
Gyarados (wings), Mewtwo Y (head ring / tube), Venusaur. Pidgeot is close (back ribbon fixed).
