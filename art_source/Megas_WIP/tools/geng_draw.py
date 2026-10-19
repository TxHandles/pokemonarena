# Mega Gengar walking frames, hand-drawn pixel rows (game size, same pixel density as the base Gengar).
from PIL import Image
PAL = {'k': '#2c2c2c', 'K': '#141018', 'L': '#8a72c0', 'P': '#745ca9', 'D': '#5c4690', 'G': '#4f4f4f',
       'y': '#f6ce37', 'r': '#e05050', 'R': '#a33040', 'w': '#fefefe', 'p': '#d0abaa',
       'f': '#d0716f', 'e': '#c56362', 'd': '#a36189', 'h': '#e8a0a0'}
SHADE_R = {'L': 'P', 'P': 'D', 'D': 'D', 'f': 'e', 'e': 'd'}   # right half of a mirrored frame is in shade
def hx(s): s = s.lstrip('#'); return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def sym(half, shade=True):
    """half: rows of the left half including the centre column; returns full rows mirrored about the centre."""
    out = []
    for r in half:
        right = ''.join((SHADE_R.get(c, c) if shade else c) for c in reversed(r[:-1]))
        out.append(r + right)
    return out
def img(rows):
    w = max(len(r) for r in rows); h = len(rows)
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0)); px = im.load()
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c not in '. ': px[x, y] = hx(PAL[c])
    return im
def put(rows, x, y, patch):
    rows = [list(r) for r in rows]
    for dy, pr in enumerate(patch):
        for dx, c in enumerate(pr):
            if c != ' ' and 0 <= y + dy < len(rows) and 0 <= x + dx < len(rows[0]): rows[y + dy][x + dx] = c
    return [''.join(r) for r in rows]

FRONT_HALF = [
    '.k..........',
    '.kk........k',
    '.kLk...k..kL',
    '.kPLk.kLkkLL',
    '..kPLkLLLkLL',
    '..kDPLLLLLLy',
    '..kDkkPLLLyK',
    '.kDPkrhkPLLy',
    '.kPDPkrrkLLL',
    'kPkDDkPPPPPP',
    'kPkDDPkkkkkk',
    'kDkDGkwpwpwp',
    'kDkGDDkpwpwp',
    'kfkDGDDkkkkk',
    'kffkDGDDDGDD',
    'kfffkkdeefee',
    'kfhfkdeeeeee',
    'kfkfkddeddkd',
    '.k.k.kdkdk.k',
    '......k..k..',
]
def front(frame):
    rows = sym(FRONT_HALF)
    return rows

LEFT1 = [
    '.......k..............',
    '......kLk.............',
    '......kLPk.....k......',
    '.....kLLPk....kLk..k..',
    '..kkkLLLLPkk.kLPk.kLk.',
    '.kLLyLLLLLPPkkLPPkkLPk',
    'kLLyKyLLLLLPPPPPPPPPDk',
    'kLLLyLkkLLLPPPPPPPPPDk',
    'kLLLLkrrkLPPPPPPPPPDDk',
    'kkkkkkkkPPPPPDDPPPDDk.',
    'kwpwpwkDDPPPDDPPPDDDk.',
    'kpwpwkDDDkPPkDDDDDDDk.',
    '.kkkkDDDkPPkGDDDGDDk..',
    '..kDDGDkPPkdeeeeeedk..',
    '...kkkkPPkdeeffeeeedk.',
    '.....kfffkdeeeeeedkdk.',
    '.....kfhfkkdeedkdk.k..',
    '.....kfkfk.kdk.k......',
    '......k.k...k.........',
]
BACK_HALF = FRONT_HALF[:5] + [
    '..kDPLLLLLLL',
    '..kDPLLLLLLL',
    '.kDPGLLLLLLL',
    '.kPDPGLLLLGL',
    'kPkDDPGLLLLL',
    'kPkDDPPGLLLL',
    'kDkDGPPPGLLG',
    'kDkGDDPPPGLL',
    'kfkDGDDDDDDD',
] + FRONT_HALF[14:]
def shift_block(rows, x0, x1, y0, y1, dx, dy):
    """move the pixels in cols x0..x1, rows y0..y1 by (dx, dy), leaving transparency behind"""
    g = [list(r) for r in rows]; H = len(g); W = len(g[0])
    blk = {(x, y): g[y][x] for y in range(y0, y1 + 1) for x in range(x0, x1 + 1) if g[y][x] != '.'}
    for (x, y) in blk: g[y][x] = '.'
    for (x, y), c in blk.items():
        if 0 <= x + dx < W and 0 <= y + dy < H: g[y + dy][x + dx] = c
    return [''.join(r) for r in g]
def wiggle_fog(rows, y0):
    """frame 2: the ghost-fire tips flicker (bottom rows shifted one pixel sideways)"""
    out = rows[:y0]
    for r in rows[y0:]: out.append('.' + r[:-1])
    return out
def front(frame):
    rows = sym(FRONT_HALF)
    if frame == 2:
        rows = shift_block(rows, 0, 3, 13, 19, 0, -1)          # one hand lifts
        rows = wiggle_fog(rows, 18)
    return rows
def left(frame):
    rows = list(LEFT1)
    if frame == 2:
        rows = shift_block(rows, 5, 9, 15, 18, -1, 0)          # the hand reaches a little further
        rows = wiggle_fog(rows, 16)
    return rows
def back(frame):
    rows = sym(BACK_HALF)
    if frame == 2:
        rows = shift_block(rows, 19, 22, 13, 19, 0, -1)
        rows = wiggle_fog(rows, 18)
    return rows
