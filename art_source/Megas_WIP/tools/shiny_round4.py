import sys, os
sys.path.insert(0, '/home/claude/arena-clash/tools')
import shinygen
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OFF = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/pk2/sprites/pokemon'
DEX = {"Mega Beedrill": 10090, "Mega Mewtwo X": 10043,
       "Mega Pidgeot": 10073}
for n, dex in DEX.items():
    folder = os.path.join(root, 'sprites', n)
    m = shinygen.match(shinygen.our_colours(folder), shinygen.official_map(OFF, dex), shinygen.spatial_votes(folder, OFF, dex))
    shinygen.write(folder, m, 'mega', n)
    print(n, len(m))
