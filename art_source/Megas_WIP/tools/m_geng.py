# Mega Gengar, drawn from scratch (not a recolour): hunched low on all fours, arms planted in front,
# a spiky crown, third eye, red eyes, a wide grin and a crimson ghostly lower body.
# Walking frames: hand-drawn pixel rows in geng_draw.py (pass 5). Build:  python3 m_geng.py      (writes out/Mega Gengar/ and p_m_geng.png)
import os
from PIL import Image, ImageDraw, ImageOps
NAME = 'Mega Gengar'; BASE = 'Gengar'; DEX = '094'; PID = 10038
OUT = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/mega/out'

PAL = {   # the user's Mega Gengar chart (charts/Mega Gengar.png): one purple, grey shading, salmon/mauve ghost fire
    'k': '#2c2c2c',                                   # outline
    '1': '#7f67b4', '2': '#745ca9', '3': '#4f4f4f',   # purple body: light / mid / grey shade
    'f': '#d0716f', 'e': '#c56362', 'd': '#a36189',   # ghost fire: light / salmon / mauve
    'r': '#c56362', 'R': '#a36189',                   # eyes
    'y': '#f6ce37', 'w': '#fefefe', 'g': '#d0abaa', 'm': '#2c2c2c',
}
BODY, CRIM, ARM = 1, 2, 3   # fill classes (an arm gets an outline where it crosses the body)

def hx(s): s = s.lstrip('#'); return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)

class Canvas:
    def __init__(s, w, h):
        s.w, s.h = w, h
        s.cls = Image.new('L', (w, h), 0)     # 0 empty, 1 body, 2 crimson
        s.d = ImageDraw.Draw(s.cls)
        s.det = {}                            # hand-placed detail pixels drawn after shading
    def poly(s, pts, c=BODY): s.d.polygon(pts, fill=c)
    def ell(s, box, c=BODY): s.d.ellipse(box, fill=c)
    def line(s, a, b, wdt, c=BODY): s.d.line([a, b], fill=c, width=wdt)
    def rect(s, box, c=BODY): s.d.rectangle(box, fill=c)
    def px(s, x, y, ch): s.det[(x, y)] = ch
    def rows(s, x0, y0, rows):
        for dy, r in enumerate(rows):
            for dx, ch in enumerate(r):
                if ch not in '. ': s.det[(x0 + dx, y0 + dy)] = ch
    def erase(s, pts): s.d.polygon(pts, fill=0)
    def render(s):
        c = s.cls.load()
        out = Image.new('RGBA', (s.w, s.h), (0, 0, 0, 0)); o = out.load()
        get = lambda x, y: c[x, y] if 0 <= x < s.w and 0 <= y < s.h else 0
        for y in range(s.h):
            for x in range(s.w):
                v = get(x, y)
                if v == 0:
                    if any(get(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))): o[x, y] = hx(PAL['k'])
                    continue
                up, lf, dn, rt = get(x, y - 1) == 0, get(x - 1, y) == 0, get(x, y + 1) == 0, get(x + 1, y) == 0
                up2 = get(x, y - 2) == 0
                if v == ARM:
                    if any(get(x + dx, y + dy) not in (0, ARM) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))): o[x, y] = hx(PAL['k']); continue
                    v = BODY
                if v == BODY:
                    ch = '1' if (up or (lf and not dn) or (up2 and not dn)) else '3' if (dn or rt) else '2'
                    if get(x, y + 1) == CRIM: ch = '3'
                else:
                    ch = 'f' if (up or get(x, y - 1) == BODY) else 'd' if (dn or rt) else 'e'
                o[x, y] = hx(PAL[ch])
        for (x, y), ch in s.det.items():
            if 0 <= x < s.w and 0 <= y < s.h: o[x, y] = hx(PAL[ch]) if ch != '_' else (0, 0, 0, 0)
        return out

def hand(cv, x, y, left, c=CRIM, lift=0, big=False):
    """A crimson ghostly hand on the ground at (x, y): a pad with flame-like claws licking upward."""
    y -= lift
    s = -1 if left else 1
    r = 3 if big else 2
    cv.ell((x - r, y - 1, x + r, y + 1), c)
    h = 4 if big else 3
    for dx, hh in (((-2, h - 1), (0, h), (2, h - 1)) if big else ((-1, h - 1), (1, h))):
        cv.poly([(x + dx * s, y - 1), (x + dx * s + s, y - 1), (x + dx * s + s * 2, y - 1 - hh)], c)

# ------------------------------------------------------------------ front
def front(frame):
    cv = Canvas(36, 24)
    b = 1 if frame == 2 else 0          # body bob
    la = 0 if frame == 1 else 1; ra = 1 - la
    o = 4                               # x offset
    P = lambda pts: [(x + o, y + b) for x, y in pts]
    # arms first (behind the head): from the shoulders down and out to the ground
    cv.poly(P([(4, 11), (8, 14), (2, 20 - la), (-1, 19 - la)]), ARM)
    cv.poly(P([(19, 14), (23, 11), (28, 19 - ra), (25, 20 - ra)]), ARM)
    # crimson lower body sinking into the ground
    cv.poly([(8 + o, 14 + b), (19 + o, 14 + b), (18 + o, 19), (15 + o, 23), (12 + o, 23), (9 + o, 19)], CRIM)
    # head: wide and low between the shoulders, big swept ears, a crown of spikes
    cv.poly(P([(6, 7), (21, 7), (24, 12), (20, 16), (7, 16), (3, 12)]))
    for pts in ([(4, 10), (9, 7), (0, 1)], [(18, 7), (23, 10), (27, 1)],                 # ears
                [(10, 7), (13, 7), (11, 2)], [(13, 7), (16, 7), (15, 0)], [(16, 7), (18, 7), (18, 4)]):   # crown
        cv.poly(P(pts))
    hand(cv, 0 + o, 21 - la, True)
    hand(cv, 27 + o, 21 - ra, False)
    y0 = 8 + b
    cv.rows(12 + o, y0 - 2, ['.k.', 'kyk', 'yky', 'kyk', '.k.'])   # the big third eye
    cv.rows(7 + o, y0 + 2, ['kk..', 'rrrk', '.Rrr'])      # angry eyes
    cv.rows(17 + o, y0 + 2, ['..kk', 'krrr', 'rrR.'])
    cv.rows(8 + o, y0 + 5, ['.mmmmmmmmmm.',
                            'mwwwwwwwwwwm',
                            'mwgwwgwwgwgm',
                            '.mmmmmmmmmm.'])
    return cv.render()

# ------------------------------------------------------------------ left (faces left)
def left(frame):
    """A small version of the user's chart: head forward and low, crown spike up, third eye, grin,
    the near hand reaching forward, the far hand trailing behind, the ghostly lower body sinking down."""
    cv = Canvas(32, 26)
    b = 1 if frame == 2 else 0
    n = 1 if frame == 2 else 0          # the near hand reaches a little further on frame 2
    P = lambda pts: [(x, y + b + 1) for x, y in pts]
    # far arm trailing behind: purple with ghost fire along its underside, and the back spikes
    cv.poly(P([(19, 13), (26, 13), (29, 15), (25, 17), (20, 16)]), CRIM)
    cv.poly(P([(18, 8), (25, 7), (28, 4), (29, 9), (27, 14), (19, 14)]))
    cv.poly(P([(17, 6), (21, 2), (23, 4), (20, 9)]))
    # the ghostly lower body sinking down to a point under the head
    cv.poly([(10, 16 + b), (19, 15 + b), (18, 20), (16, 24), (14, 24), (11, 20)], CRIM)
    # head and crown spike
    cv.poly(P([(7, 6), (12, 4), (18, 6), (20, 11), (17, 16), (9, 16), (6, 12)]))
    cv.poly(P([(9, 6), (13, 5), (12, -1)]))
    cv.poly(P([(12, 4), (15, 5), (14, 2)]))
    # near arm reaching forward to a ghost-fire hand
    cv.poly(P([(9, 10), (11, 13), (5, 15), (4, 12)]), ARM)
    cv.poly(P([(1, 13), (5, 13), (5, 16), (2, 17)]), CRIM)
    cv.poly(P([(1, 14), (1, 11 - n), (3, 14)]), CRIM)
    cv.poly(P([(3, 13), (4, 10 - n), (5, 13)]), CRIM)
    # face
    y0 = 8 + b
    cv.rows(8, y0 - 1, ['.k.', 'kyk', 'yky', 'yky', 'kyk', '.k.'])   # the big third eye with its slit
    cv.rows(11, y0 + 2, ['k.', 'rk'])                              # red eye glint
    cv.rows(7, y0 + 5, ['.kk.....kk.',
                        'kwwkk.kkwwk',
                        'kwwwwwwwwgk',
                        '.kgwwwwwgk.',
                        '..kkggggk..'])
    # grey shading on the body like the chart
    for x, y in ((13, 6), (14, 7), (18, 7), (19, 8), (22, 9), (23, 10), (26, 11), (17, 12)):
        cv.px(x, y + b + 1, '3')
    return cv.render()

# ------------------------------------------------------------------ back
def back(frame):
    cv = Canvas(36, 24)
    b = 1 if frame == 2 else 0
    la = 0 if frame == 1 else 1; ra = 1 - la
    o = 4
    P = lambda pts: [(x + o, y + b) for x, y in pts]
    cv.poly(P([(4, 11), (8, 14), (2, 20 - la), (-1, 19 - la)]), ARM)
    cv.poly(P([(19, 14), (23, 11), (28, 19 - ra), (25, 20 - ra)]), ARM)
    cv.poly([(8 + o, 14 + b), (19 + o, 14 + b), (18 + o, 19), (15 + o, 23), (12 + o, 23), (9 + o, 19)], CRIM)
    # the wide spiky back, seen from behind
    cv.poly(P([(6, 7), (21, 7), (24, 12), (20, 17), (7, 17), (3, 12)]))
    for pts in ([(4, 10), (9, 7), (0, 1)], [(18, 7), (23, 10), (27, 1)],
                [(10, 7), (13, 7), (11, 2)], [(13, 7), (16, 7), (15, 0)], [(16, 7), (18, 7), (18, 4)]):
        cv.poly(P(pts))
    hand(cv, 0 + o, 21 - la, True)
    hand(cv, 27 + o, 21 - ra, False)
    # grey shading strokes down the spiky back, like the chart
    for (x, y) in ((11, 9), (12, 10), (16, 9), (15, 10), (13, 13), (14, 14), (9, 12), (18, 12), (6, 9), (21, 9)):
        cv.px(x + o, y + b, '3')
    return cv.render()

# ------------------------------------------------------------------ portrait (3/4, facing left, bigger)
def portrait():
    cv = Canvas(46, 34)
    # arms reaching down and out to the ground (behind the head)
    cv.poly([(9, 17), (14, 20), (6, 29), (1, 28)], ARM)
    cv.poly([(31, 19), (36, 16), (43, 27), (38, 29)], ARM)
    # crimson lower body sinking into the ground
    cv.ell((11, 20, 34, 33), CRIM)
    # head (3/4 view, turned a little to the left)
    cv.poly([(9, 9), (31, 9), (37, 16), (32, 25), (12, 25), (5, 17)])
    for pts in ([(6, 14), (13, 9), (1, 0)], [(28, 9), (36, 14), (44, 1)],                    # ears
                [(14, 9), (19, 9), (16, 2)], [(19, 9), (24, 9), (23, 0)], [(24, 9), (28, 9), (28, 4)],   # crown
                [(35, 15), (38, 19), (43, 15)]):                                        # cheek spike
        cv.poly(pts)
    hand(cv, 3, 31, True, big=True)
    hand(cv, 41, 31, False, big=True)
    cv.rows(18, 11, ['.kk.', 'kyyk', 'kykk', '.kk.'])        # third eye
    cv.rows(9, 15, ['kk...', 'rrrrk', '.RRrr'])              # eyes, angry and slanted
    cv.rows(25, 15, ['...kk', 'krrrr', 'rrRR.'])
    cv.rows(11, 19, ['.mmmmmmmmmmmmmmmmmmm.',
                     'mwwwmwwwwmwwwwmwwwwmm',
                     'mwwwgwwwwgwwwwgwwwgm.',
                     '.mwwwgwwwgwwwwgwwwm..',
                     '..mmmmmmmmmmmmmmmm...'])
    return cv.render()


# ------------------------------------------------------------------ game-size frames (same pixel size as the
# other walking sprites: about 24 px wide, 20 tall). The big versions above are kept for reference.
def front_s(frame, face=True):
    cv = Canvas(26, 21)
    b = 1 if frame == 2 else 0
    la = 0 if frame == 1 else 1; ra = 1 - la
    P = lambda pts: [(x, y + b) for x, y in pts]
    cv.poly(P([(4, 8), (7, 10), (3, 15 - la), (1, 14 - la)]), ARM)
    cv.poly(P([(18, 10), (21, 8), (24, 14 - ra), (22, 15 - ra)]), ARM)
    cv.poly([(8, 11 + b), (17, 11 + b), (16, 15), (14, 19), (11, 19), (9, 15)], CRIM)
    cv.poly(P([(6, 5), (19, 5), (22, 8), (19, 12), (6, 12), (3, 8)]))
    for pts in ([(4, 7), (8, 5), (1, 0)], [(17, 5), (21, 7), (24, 0)],
                [(9, 5), (11, 5), (10, 1)], [(11, 5), (14, 5), (13, 0)], [(14, 5), (16, 5), (16, 2)]):
        cv.poly(P(pts))
    for x, y, l in ((1, 17 - la, True), (24, 17 - ra, False)):
        cv.ell((x - 2, y - 1, x + 2, y), CRIM)
        s = -1 if l else 1
        cv.poly([(x, y - 1), (x + s, y - 1), (x + 2 * s, y - 3)], CRIM)
        cv.poly([(x - s, y - 1), (x, y - 1), (x, y - 3)], CRIM)
    if face:
        cv.rows(11, 5 + b, ['.y.', 'yky', '.y.'])
        cv.rows(6, 8 + b, ['kk.', 'rrk'])
        cv.rows(17, 8 + b, ['.kk', 'krr'])
        cv.rows(7, 10 + b, ['mwwwwwwwwwwm', '.mgwwgwwgwm.', '..mmmmmmmm..'])
    else:
        for x, y in ((9, 7), (10, 8), (15, 7), (14, 8), (12, 10), (7, 9), (18, 9)):
            cv.px(x, y + b, '3')
    return cv.render()

def left_s(frame):
    cv = Canvas(24, 21)
    b = 1 if frame == 2 else 0
    n = 1 if frame == 2 else 0
    P = lambda pts: [(x, y + b + 1) for x, y in pts]
    cv.poly(P([(14, 10), (19, 10), (21, 11), (18, 13), (15, 12)]), CRIM)
    cv.poly(P([(13, 6), (18, 5), (21, 2), (21, 7), (19, 10), (14, 10)]))
    cv.poly(P([(12, 5), (15, 1), (17, 3), (15, 6)]))
    cv.poly([(8, 12 + b), (14, 11 + b), (13, 15), (12, 19), (10, 19), (8, 15)], CRIM)
    cv.poly(P([(5, 4), (9, 3), (13, 4), (15, 8), (13, 12), (6, 12), (4, 9)]))
    cv.poly(P([(6, 4), (9, 3), (8, -1)]))
    cv.poly(P([(9, 3), (12, 4), (11, 1)]))
    cv.poly(P([(6, 8), (8, 10), (4, 12), (3, 10)]), ARM)
    cv.poly(P([(1, 11), (4, 11), (4, 13), (2, 14)]), CRIM)
    cv.poly(P([(1, 11), (1, 9 - n), (2, 11)]), CRIM)
    cv.poly(P([(2, 11), (3, 8 - n), (4, 11)]), CRIM)
    y0 = 5 + b + 1
    cv.rows(6, y0, ['.y.', 'yky', '.y.'])
    cv.rows(9, y0 + 2, ['kr'])
    cv.rows(5, y0 + 4, ['kwwwwwwk', 'kwgwwgwk', '.kkkkkk.'])
    for x, y in ((14, 7), (17, 6), (18, 8), (10, 4)):
        cv.px(x, y + b + 1, '3')
    return cv.render()

def build():
    os.makedirs(f'{OUT}/{NAME}', exist_ok=True)
    chart = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'charts', 'Mega Gengar.png')
    import geng_draw as G   # hand-drawn game-size frames (pass 5)
    res = {'front1': G.img(G.front(1)), 'front2': G.img(G.front(2)), 'left1': G.img(G.left(1)), 'left2': G.img(G.left(2)),
           'back1': G.img(G.back(1)), 'back2': G.img(G.back(2)),
           'portrait': Image.open(chart).convert('RGBA') if os.path.exists(chart) else portrait()}   # the user's own chart
    n = {'front1': '1_mae_1', 'front2': '1_mae_2', 'left1': '2_hidari_1', 'left2': '2_hidari_2', 'back1': '4_ushiro_1', 'back2': '4_ushiro_2'}
    for k, v in n.items(): res[k].save(f'{OUT}/{NAME}/{DEX}_{v}.png')
    ImageOps.mirror(res['left1']).save(f'{OUT}/{NAME}/{DEX}_3_migi_1.png')
    ImageOps.mirror(res['left2']).save(f'{OUT}/{NAME}/{DEX}_3_migi_2.png')
    res['portrait'].save(f'{OUT}/{NAME}/Pokemon-{DEX}-Mega-Gengar.png')
    return res

if __name__ == '__main__':
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import mk
    r = build()
    mk.preview(sys.modules[__name__], r, 'p_m_geng.png', 8)
    # also a game-size check on grass: 1x, 3x
    order = ['front1', 'front2', 'left1', 'left2', 'back1', 'back2', 'portrait']
    W = sum(r[k].width * 4 + 8 for k in order) + 8
    g = Image.new('RGBA', (W, 140 + 40), (92, 150, 70, 255)); x = 8
    for k in order:
        g.alpha_composite(r[k], (x, 4)); g.alpha_composite(r[k].resize((r[k].width * 4, r[k].height * 4), Image.NEAREST), (x, 44)); x += r[k].width * 4 + 8
    g.save('g_geng.png')
