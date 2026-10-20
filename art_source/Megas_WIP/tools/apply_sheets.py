"""Put the user's sheets into sprites/<Mega>/ (the game's file layout) and write each one's shiny.txt.
The old round-4 frames are kept in sprites_round4/."""
import sys, os, glob, shutil
from collections import Counter, defaultdict
sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, '/home/claude/arena-clash/tools')
from sheet2frames import SHEETS, frames
import shinygen
from PIL import Image
import numpy as np

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OFFICIAL = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/pk2/sprites/pokemon'
DEX = {"Mega Gengar": 10038, "Mega Charizard X": 10034}
SHRINK = {"Mega Gyarados": 2 / 3}
names = {"front": "1_mae", "left": "2_hidari", "right": "3_migi", "back": "4_ushiro"}

def shrink(img, k):
    """Smaller, keeping only the sprite's own colours (each pixel snaps to the closest one)."""
    pal = [c[:3] for n, c in img.getcolors(1 << 16) if c[3] >= 128]
    w, h = max(1, round(img.width * k)), max(1, round(img.height * k))
    sm = np.array(img.resize((w, h), Image.LANCZOS)).astype(int)
    P = np.array(pal)
    out = np.zeros_like(sm)
    for y in range(h):
        for x in range(w):
            p = sm[y, x]
            if p[3] < 110: continue
            i = np.argmin(((P - p[:3]) ** 2).sum(1))
            out[y, x, :3] = P[i]; out[y, x, 3] = 255
    return Image.fromarray(out.astype(np.uint8), 'RGBA')

def pair_map(a, b):
    votes = defaultdict(Counter)
    pa, pb = a.load(), b.load()
    for y in range(min(a.height, b.height)):
        for x in range(min(a.width, b.width)):
            p, q = pa[x, y], pb[x, y]
            if p[3] >= 128 and q[3] >= 128: votes[p[:3]][q[:3]] += 1
    return votes

for mega in SHEETS:
    folder = os.path.join(root, 'sprites', mega)
    keep = os.path.join(root, 'sprites_round4', mega)
    if not os.path.exists(keep): shutil.copytree(folder, keep)
    pre = os.path.basename(glob.glob(os.path.join(keep, '*_1_mae_1.png'))[0]).split('_')[0]
    fr, _ = frames(root, mega)
    shiny_cols = SHEETS[mega][5]
    so = SHEETS[mega][6] if len(SHEETS[mega]) > 6 else 4
    votes = defaultdict(Counter)
    for d, key in names.items():
        for k in (0, 1):
            img = fr[(d, k)]
            if shiny_cols:   # colour pairs between the normal frame and its shiny twin (same pixels)
                for c, cnt in pair_map(fr[(d, k)], fr[(d, k + 4)]).items(): votes[c].update(cnt)
            if mega in SHRINK: img = shrink(img, SHRINK[mega])
            img.save(os.path.join(folder, f'{pre}_{key}_{k + 1}.png'))
    if shiny_cols:
        mapping = {c: v.most_common(1)[0][0] for c, v in votes.items() if v.most_common(1)[0][0] != c}
        # the portrait (round 4) gets the same colours where they match
        for f in glob.glob(os.path.join(folder, 'Pokemon-*.png')):
            ours = shinygen.our_colours(folder)
    else:
        dex = DEX[mega]
        mapping = shinygen.match(shinygen.our_colours(folder), shinygen.official_map(OFFICIAL, dex), shinygen.spatial_votes(folder, OFFICIAL, dex))
    shinygen.write(folder, mapping, 'mega', mega)
    print(mega, pre, len(mapping), 'shiny colours')

# The menu portrait (the official icon) has its own colours: they get theirs from the official shiny
# sprite (shinygen's match), added to the colours taken from the sheet.
DEXALL = {"Mega Venusaur": 10033, "Mega Charizard X": 10034, "Mega Charizard Y": 10035, "Mega Blastoise": 10036,
          "Mega Alakazam": 10037, "Mega Gengar": 10038, "Mega Kangaskhan": 10039, "Mega Gyarados": 10041,
          "Mega Mewtwo Y": 10044, "Mega Slowbro": 10071, "Mega Pinsir": 10040, "Mega Aerodactyl": 10042}
from collections import Counter as _C
for mega in SHEETS:
    folder = os.path.join(root, 'sprites', mega)
    path = os.path.join(folder, 'shiny.txt')
    m = {}
    for line in open(path):
        if line.startswith('shiny ='):
            a, b = line.split('=')[1].split('>')
            m[tuple(int(a.strip()[i:i + 2], 16) for i in (0, 2, 4))] = tuple(int(b.strip()[i:i + 2], 16) for i in (0, 2, 4))
    pc = _C()
    for f in glob.glob(os.path.join(folder, 'Pokemon-*.png')):
        for n, c in Image.open(f).convert('RGBA').getcolors(1 << 16):
            if c[3] >= 128 and c[:3] not in m: pc[c[:3]] += n
    dex = DEXALL[mega]
    pm = shinygen.match(pc, shinygen.official_map(OFFICIAL, dex))
    m.update(pm)
    shinygen.write(folder, m, 'mega', mega)
    print(mega, 'portrait colours', len(pm))
