# Mega Blastoise, drawn to match the user's chart (charts/Mega Blastoise.png).
# Build:  python3 m_blast.py      (writes out/Mega Blastoise/ and p_m_blast.png, g_blast.png)
# The walking frames start from the user's base Blastoise (recoloured to the chart's palette, shoulder
# cannons removed) with one big diagonal cannon rising from the shell and a cannon on each forearm.
import os
from PIL import Image, ImageOps
NAME = 'Mega Blastoise'; BASE = 'Blastoise'; DEX = '009'; PID = 10036
POK = '/home/claude/arena-clash/Pokemon'
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'out')
SP = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad'

def hx(s): s = s.lstrip('#'); return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
# the chart's palette (+ two in-between shades)
PAL = {'k': '#313232', 'W': '#fdfdfd', 'g': '#bdbba9', 'G': '#4f4f4f', 'S': '#7b5732', 's': '#5b3f22',
       'T': '#b8954a', 'B': '#75a5e3', 'b': '#4e7199', 'd': '#35517a', 'C': '#f5e592', 'c': '#d6c27a',
       'r': '#c84a3a', 'm': '#8e8c80'}
PAL = {k: hx(v) for k, v in PAL.items()}
# base Blastoise colours -> chart colours
RECOL = {'#9088e8': 'B', '#9088e5': 'B', '#9289eb': 'B', '#6060a8': 'b', '#6261a5': 'b', '#6261a4': 'b',
         '#303858': 'd', '#2d3954': 'd', '#2d3856': 'd',
         '#0a0a0a': 'k', '#09090b': 'k',
         '#f0c850': 'C', '#ebc65c': 'C', '#ebc94a': 'C', '#c8a850': 'c', '#988850': 'T', '#948652': 'T', '#998a53': 'T',
         '#805008': 'S', '#7c5104': 'S', '#834e0b': 'S', '#b07018': 'T', '#ad7115': 'T', '#b26d1b': 'T',
         '#44250b': 's', '#43290b': 's',
         '#e8e8f8': 'W', '#e6e9f2': 'W', '#e9eaf3': 'W', '#b8b8b8': 'g', '#b9b9b9': 'g', '#888888': 'm', '#888989': 'm',
         '#898989': 'm', '#505050': 'G', '#4e4e4e': 'G'}

def base(name):
    im = Image.open(f'{POK}/{BASE}/{DEX}_{name}.png').convert('RGBA')
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            p = px[x, y]
            if p[3] < 128: continue
            h = '#%02x%02x%02x' % p[:3]
            if h in RECOL: px[x, y] = PAL[RECOL[h]]
            else: print('unmapped', name, h)
    return im

def canvas(w, h): return Image.new('RGBA', (w, h), (0, 0, 0, 0))
def paste(cv, im, x, y): cv.alpha_composite(im, (x, y))
def rows(cv, x0, y0, rs, under=False):
    px = cv.load()
    for dy, r in enumerate(rs):
        for dx, ch in enumerate(r):
            if ch in '. ': continue
            x, y = x0 + dx, y0 + dy
            if not (0 <= x < cv.width and 0 <= y < cv.height): continue
            if ch == '_': px[x, y] = (0, 0, 0, 0); continue
            if under and px[x, y][3] >= 128: continue
            px[x, y] = PAL[ch]
def erase(cv, x0, y0, x1, y1):
    px = cv.load()
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if 0 <= x < cv.width and 0 <= y < cv.height: px[x, y] = (0, 0, 0, 0)
def barrel(cv, p0, p1, r=2.0, muzzle=True):
    """A thick grey cannon barrel from p0 (its base) to p1 (the muzzle), lit from above."""
    import math
    px = cv.load()
    (x0, y0), (x1, y1) = p0, p1
    dx, dy = x1 - x0, y1 - y0; L = math.hypot(dx, dy); ux, uy = dx / L, dy / L
    nx, ny = (uy, -ux) if uy * 0 - ux < 0 else (-uy, ux)   # the normal that points up
    if ny > 0: nx, ny = -nx, -ny
    for y in range(int(min(y0, y1) - r - 3), int(max(y0, y1) + r + 4)):
        for x in range(int(min(x0, x1) - r - 3), int(max(x0, x1) + r + 4)):
            if not (0 <= x < cv.width and 0 <= y < cv.height): continue
            t = max(0.0, min(L, (x + 0.5 - x0) * ux + (y + 0.5 - y0) * uy))
            cx, cy = x0 + ux * t, y0 + uy * t
            ox, oy = x + 0.5 - cx, y + 0.5 - cy
            if math.hypot(ox, oy) > r: continue
            off = ox * nx + oy * ny
            px[x, y] = PAL['W'] if off > r - 1.1 else PAL['m'] if off < -(r - 1.1) else PAL['g']
    if muzzle:   # the open end: a white ring around a dark bore
        for y in range(int(y1 - r - 2), int(y1 + r + 3)):
            for x in range(int(x1 - r - 2), int(x1 + r + 3)):
                d = math.hypot(x + 0.5 - x1, y + 0.5 - y1)
                if not (0 <= x < cv.width and 0 <= y < cv.height): continue
                if d <= r - 0.4: px[x, y] = PAL['G']
                elif d <= r + 1.0: px[x, y] = PAL['W']

def outline(cv):
    """Give every opaque pixel that touches transparency on the outside a dark outline neighbour."""
    px = cv.load(); W, H = cv.size; add = []
    for y in range(H):
        for x in range(W):
            if px[x, y][3] >= 128: continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                X, Y = x + dx, y + dy
                if 0 <= X < W and 0 <= Y < H and px[X, Y][3] >= 128 and px[X, Y][:3] != PAL['k'][:3]:
                    add.append((x, y)); break
    for x, y in add: px[x, y] = PAL['k']

# ------------------------------------------------------------------ front
# the big cannon, seen from the front: it rises from the shell behind the head toward the upper left
CANNON_F = [
    '..kkkkk.........',
    '.kWWWWWk........',
    'kWGGGGGWk.......',
    'kWGkkkGWk.......',
    'kWGGGGGWgk......',
    '.kWWWWWgggk.....',
    '..kkggggggmk....',
    '....kgggggmmk...',
    '.....kkgggmmWk..',
    '......kWWggWWk..',
    '.......kWWWWWk..',
    '........kkkkk...',
]
# the big brown shell behind the head, that the cannon is mounted on
SHELL_F = [
    '....kkkkkk....',
    '..kkSSSSSSkk..',
    '.kSSTTTTSSSSk.',
    'kSSTTSSSSSSssk',
    'kSTTSSSSSSSssk',
    'kSTSSSSSSSSssk',
    'kSSSSSSSSSSssk',
    'kSSSSSSSSSsssk',
    'kSSSSSSSSSsssk',
    'kSSSSSSSSSsssk',
    'kSSSSSSSSsssk.',
    'kSSSSSSSSsssk.',
    'kSSSSSSSSsssk.',
    'kSSSSSSSsssk..',
    '.kSSSSSSsssk..',
    '.kSSSSSsssk...',
    '..kSSSsssk....',
    '...kkkkkk.....',
]
ARMCAN_L = ['.kkkk.', 'kgWWgk', 'kWGGWk', 'kgWWgk', '.kkkk.']   # a forearm cannon seen end-on
def front(frame):
    b = base('1_mae_%d' % frame)
    erase(b, 0, 3, 5, 9); erase(b, 20, 3, 25, 9)   # the base's shoulder cannons go
    cv = canvas(33, 33)
    # behind: the shell rim showing past the shoulders, the cannon
    rows(cv, 17, 8, SHELL_F)
    rows(cv, 3, 14, ['..kkkk', '.kSSSS', 'kSTSSS', 'kSSSSS', 'kSSSSS', 'kSSSSS', '.kSSSS', '..kkkk'])
    rows(cv, 6, 0, CANNON_F)
    paste(cv, b, 1, 9)
    # the base's shoulder cannons go (x 0-5 / 20-25, y 3-9 of the base -> +1, +9)
    # the shell's white rim and brown edge behind the shoulders
    for x, y in ((10, 16), (10, 17), (17, 16), (17, 17)): rows(cv, x, y, ['G'])   # dark eyes, as in the chart
    # forearm cannons on the hands
    dy = 0 if frame == 1 else -1
    rows(cv, 2, 21 + dy, ARMCAN_L)
    rows(cv, 20, 21 - dy, ARMCAN_L)
    return cv.crop(cv.getbbox())

# ------------------------------------------------------------------ side (facing left, like the chart)
SHELL_L = [
    '......kkkkkk...',
    '....kkSSSSSSkk.',
    '...kSSTTTTSSSsk',
    '..kSSTTSSSSSssk',
    '.kSSTSSSSSSSssk',
    '.kSTTSSSSSSsssk',
    'kSSTSSSSSSSsssk',
    'kSSSSSSSSSsssk.',
    'kSSSSSSSSSsssk.',
    'kSSSSSSSSssssk.',
    'kSSSSSSSSsssk..',
    'kSSSSSSSsssk...',
    '.kSSSSSSsssk...',
    '.kSSSSSsssk....',
    '..kSSSssssk....',
    '...kkkkkkk.....',
]
ARMCAN_SIDE = ['.kkkkkk', 'kGWWWgk', 'kGgggmk', '.kkkkkk']   # a forearm cannon pointing forward (left)
def left(frame):
    b = base('2_hidari_%d' % frame)
    # the base's shoulder cannon goes (x 13-18, y 1-11): its grey/white/dark-grey pixels
    px = b.load()
    for y in range(0, 12):
        for x in range(13, 20):
            p = px[x, y]
            if p[3] >= 128 and p[:3] in (PAL['g'][:3], PAL['W'][:3], PAL['G'][:3], PAL['m'][:3], PAL['k'][:3]):
                px[x, y] = PAL['S'] if x < 18 else PAL['s']
    for y in range(0, 12):   # what's left of the cannon's outline above the shell goes
        for x in range(13, 20):
            if y < 4 and px[x, y][:3] in (PAL['S'][:3], PAL['s'][:3]): px[x, y] = (0, 0, 0, 0)
    cv = canvas(32, 30)
    ox, oy = 2, 8
    rows(cv, ox + 13, oy - 1, SHELL_L)
    paste(cv, b, ox, oy)
    # tidy the hole left by the shoulder cannon: brown shell, with a white rim next to the body
    barrel(cv, (ox + 19, oy + 2), (ox + 5, 3), 2.3)
    dy = 0 if frame == 1 else 1
    rows(cv, ox + 3, oy + 13 + dy, ARMCAN_SIDE)
    outline(cv)
    return cv.crop(cv.getbbox())

# ------------------------------------------------------------------ back
SHELL_B = [
    '......kkkkkkkkkkkk......',
    '...kkkSSSTTTTTTSSSkkk...',
    '..kSSSSTTTTTTTTTSSSSsk..',
    '.kSSSTTTsssssssTTSSSssk.',
    '.kSSTTTsSSSSSSSsTSSSssk.',
    'kSSSSSsSSTTTTSSSsSSSsssk',
    'kssssssSSTTSSSSSsssssssk',
    'kSSSSSsSSSSSSSSSsSSSsssk',
    'kSSSSSSsSSSSSSSsSSSSsssk',
    'kSSSSSSSsssssssSSSSSsssk',
    '.kSSSSSsSSSSSSSsSSSSssk.',
    '.kWSSSsSSSSSSSSSsSSSsWk.',
    '..kWWSSSSSSSSSSSSSsWWk..',
    '...kkWWWWWWWWWWWWWWkk...',
    '.....kkkkkkkkkkkkkk.....',
]
def back(frame):
    b = base('4_ushiro_%d' % frame)
    cv = canvas(30, 32)
    ox, oy = 2, 8
    paste(cv, b, ox, oy)
    # the base's shoulder cannons go (x 0-5 / 20-25, y 4-10)
    erase(cv, ox + 0, oy + 4, ox + 5, oy + 10); erase(cv, ox + 20, oy + 4, ox + 25, oy + 10)
    # the big brown shell covers the back, with its plates and a white rim at the bottom
    rows(cv, ox + 1, oy + 6, SHELL_B)
    # the big cannon stands on the top of the shell and points up and forward, over the head
    barrel(cv, (ox + 12, oy + 7), (ox + 18, oy - 4), 2.6)
    rows(cv, ox + 8, oy + 6, ['.kkkkkkkk.', 'kWWWWWWWWk', 'kmggggggmk', '.kkkkkkkk.'])   # its mount
    # small cannons on the forearms, poking out to the sides
    dy = 0 if frame == 1 else 1
    rows(cv, ox - 2, oy + 14 + dy, ['.kkk', 'kgWk', 'kGmk', '.kkk'])
    rows(cv, ox + 24, oy + 14 + (1 - dy), ['kkk.', 'kWgk', 'kmGk', 'kkk.'])
    outline(cv)
    return cv.crop(cv.getbbox())

def build():
    res = {'front1': front(1), 'front2': front(2), 'left1': left(1), 'left2': left(2), 'back1': back(1), 'back2': back(2)}
    os.makedirs(f'{OUT}/{NAME}', exist_ok=True)
    names = {'front1': '1_mae_1', 'front2': '1_mae_2', 'left1': '2_hidari_1', 'left2': '2_hidari_2',
             'back1': '4_ushiro_1', 'back2': '4_ushiro_2'}
    for k, n in names.items(): res[k].save(f'{OUT}/{NAME}/{DEX}_{n}.png')
    ImageOps.mirror(res['left1']).save(f'{OUT}/{NAME}/{DEX}_3_migi_1.png')
    ImageOps.mirror(res['left2']).save(f'{OUT}/{NAME}/{DEX}_3_migi_2.png')
    Image.open(f'{HERE}/charts/{NAME}.png').convert('RGBA').save(f'{OUT}/{NAME}/Pokemon-{DEX}-Mega-Blastoise.png')   # the user's chart
    return res

def preview(res, out='p_m_blast.png', S=8):
    order = [k for k in ['front1', 'front2', 'left1', 'left2', 'back1', 'back2'] if k in res]
    chart = Image.open(f'{HERE}/charts/{NAME}.png').convert('RGBA')
    ims = [res[k] for k in order] + [chart]
    W = sum(i.width * S + 12 for i in ims) + 12; H = max(i.height for i in ims) * S + 20
    c = Image.new('RGBA', (W, H), (64, 120, 72, 255)); x = 10
    for i in ims: c.alpha_composite(i.resize((i.width * S, i.height * S), Image.NEAREST), (x, 10)); x += i.width * S + 12
    c.save(f'{HERE}/{out}')
    # game-size check: 1x and 3x
    ims2 = [res[k] for k in order]
    W = sum(i.width * 4 + 8 for i in ims2) + sum(i.width + 6 for i in ims2) + 20; H = max(i.height for i in ims2) * 3 + 20
    g = Image.new('RGBA', (W, H), (64, 120, 72, 255)); x = 6
    for i in ims2: g.alpha_composite(i, (x, 6)); x += i.width + 6
    for i in ims2: g.alpha_composite(i.resize((i.width * 3, i.height * 3), Image.NEAREST), (x, 6)); x += i.width * 3 + 8
    g.save(f'{HERE}/g_blast.png')

if __name__ == '__main__':
    r = build(); preview(r)
