"""Mega Beedrill from the user's small sheet (green background, 1x): row 1 back, row 2 front, row 3 facing
left, two frames each. Facing right is the left view mirrored."""
import os, sys, glob, shutil
sys.path.insert(0, '/home/claude/arena-clash/tools')
import shinygen
from PIL import Image, ImageOps
import numpy as np
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OFF = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/pk2/sprites/pokemon'
im = Image.open(os.path.join(root, 'Mega_Beedrill_sheet.png')).convert('RGBA')
a = np.array(im)
bg = (a[:, :, 0] == 0) & (a[:, :, 1] == 168) & (a[:, :, 2] == 4)
a[bg, 3] = 0
im = Image.fromarray(a, 'RGBA')
boxes = {'back': [(17, 30, 52, 62), (52, 30, 87, 62)], 'front': [(17, 63, 53, 96), (53, 63, 88, 96)],
         'left': [(23, 93, 51, 127), (55, 93, 85, 127)]}
folder = os.path.join(root, 'sprites', 'Mega Beedrill')
keep = os.path.join(root, 'sprites_round4', 'Mega Beedrill')
if not os.path.exists(keep): shutil.copytree(folder, keep)
out = {}
for d, bl in boxes.items():
    out[d] = []
    for b in bl:
        f = im.crop(b)
        out[d].append(f.crop(f.getbbox()))
out['right'] = [ImageOps.mirror(f) for f in out['left']]
key = {'front': '1_mae', 'left': '2_hidari', 'right': '3_migi', 'back': '4_ushiro'}
for d, fs in out.items():
    for k, f in enumerate(fs):
        f.save(os.path.join(folder, f'015_{key[d]}_{k + 1}.png'))
        print(d, k, f.size)
m = shinygen.match(shinygen.our_colours(folder), shinygen.official_map(OFF, 10090), shinygen.spatial_votes(folder, OFF, 10090))
shinygen.write(folder, m, 'mega', 'Mega Beedrill')
print('shiny colours', len(m))
