"""A new Mega from a user sheet: frames into sprites/<Mega>/, shiny.txt from the official shiny sprite (or
the sheet's shiny columns), and a portrait (the official Gen 7 icon if there is one, else the front frame).
    python tools/add_mega.py "Mega Victreebel" 071 10279"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, '/home/claude/arena-clash/tools')
from sheet2frames import frames, SHEETS
import shinygen
from collections import Counter, defaultdict
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OFF = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/pk2/sprites/pokemon'
name, pre, pid = sys.argv[1], sys.argv[2], int(sys.argv[3])
scale = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0   # e.g. 0.5: shrink (each pixel snaps to the sheet's own colours)
folder = os.path.join(root, 'sprites', name)
os.makedirs(folder, exist_ok=True)
fr, _ = frames(root, name)
key = {'front': '1_mae', 'left': '2_hidari', 'right': '3_migi', 'back': '4_ushiro'}
for d, k in key.items():
    for c in (0, 1):
        img = fr[(d, c)]
        if scale != 1.0:
            from apply_sheets_shrink import shrink
            img = shrink(img, scale)
        img.save(os.path.join(folder, f'{pre}_{k}_{c + 1}.png'))
icon = os.path.join(OFF, 'versions/generation-vii/icons', f'{pid}.png')
from PIL import Image
chart = os.path.join(root, 'charts', f'{name} portrait.png')   # the user's pixel chart, read by chart_pair.py
po = Image.open(chart).convert('RGBA') if os.path.exists(chart) else Image.open(icon).convert('RGBA') if os.path.exists(icon) else Image.open(os.path.join(folder, f'{pre}_1_mae_1.png')).convert('RGBA')
po.crop(po.getbbox()).save(os.path.join(folder, f'Pokemon-{pre}-{name.replace(" ", "-")}.png'))
m = {}
if SHEETS[name][5]:
    votes = defaultdict(Counter)
    for d in key:
        for c in (0, 1):
            a, b = fr[(d, c)].load(), fr[(d, c + 4)].load()
            for y in range(fr[(d, c)].height):
                for x in range(fr[(d, c)].width):
                    p, q = a[x, y], b[x, y]
                    if p[3] >= 128 and q[3] >= 128: votes[p[:3]][q[:3]] += 1
    m = {c: v.most_common(1)[0][0] for c, v in votes.items() if v.most_common(1)[0][0] != c}
rest = Counter({c: n for c, n in shinygen.our_colours(folder).items() if c not in m})
m.update(shinygen.match(rest, shinygen.official_map(OFF, pid), shinygen.spatial_votes(folder, OFF, pid)))
shinygen.write(folder, m, 'mega', name)
print(name, 'shiny colours', len(m), 'portrait', 'chart' if os.path.exists(chart) else 'icon' if os.path.exists(icon) else 'front frame')
