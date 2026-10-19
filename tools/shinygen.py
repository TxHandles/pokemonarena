"""Shiny colours for every Pokemon: maps each colour of our sprites to the official shiny colour.

For each Pokemon it compares the official normal and shiny sprites (PokeAPI's sprites repo, front
and back), which are the same picture recoloured, so every official colour gives its shiny colour.
Each colour in our own sprites is matched to the closest official normal colour and gets that
colour's shiny version, keeping our shade (lighter / darker) so the shading still reads.
The result goes in Pokemon/<Name>/shiny.txt ("shiny = RRGGBB > RRGGBB"), which the game uses to
recolour the sprites when it loads them.

    python tools/shinygen.py <PokeAPI sprites/pokemon folder> [Name ...] [--sheet out.png]

Run it again after adding a Pokemon (with just its name to do only that one).
"""
import colorsys, glob, math, os, re, sys
from collections import Counter, defaultdict
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Pokemon')
# Hand fixes where two parts share a normal colour but turn different shiny colours:
# {Name: {our RRGGBB: shiny RRGGBB}} (applied after the automatic match).
OVERRIDES = {}


def srgb_to_lab(c):
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(v) for v in c[:3])
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    f = lambda t: t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    fx, fy, fz = f(x), f(y), f(z)
    return (116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz))


def lab_to_srgb(lab):
    L, a, b = lab
    fy = (L + 16) / 116
    fx, fz = fy + a / 500, fy - b / 200
    inv = lambda t: t ** 3 if t ** 3 > 0.008856 else (t - 16 / 116) / 7.787
    x, y, z = inv(fx) * 0.95047, inv(fy), inv(fz) * 1.08883
    r = 3.2406 * x - 1.5372 * y - 0.4986 * z
    g = -0.9689 * x + 1.8758 * y + 0.0415 * z
    bb = 0.0557 * x - 0.2040 * y + 1.0570 * z
    def gam(v):
        v = max(0.0, min(1.0, v))
        return 12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055
    return tuple(int(round(gam(v) * 255)) for v in (r, g, bb))


def official_map(src, dex):
    """{normal RGB: (shiny RGB, pixel count)} from the official front and back sprites."""
    pairs = defaultdict(Counter)
    for sub in ('', 'back/'):
        n = os.path.join(src, sub + f'{dex}.png')
        s = os.path.join(src, sub + f'shiny/{dex}.png')
        if not (os.path.exists(n) and os.path.exists(s)):
            continue
        a = Image.open(n).convert('RGBA')
        b = Image.open(s).convert('RGBA')
        if a.size != b.size:
            continue
        pa, pb = a.load(), b.load()
        for y in range(a.height):
            for x in range(a.width):
                p, q = pa[x, y], pb[x, y]
                if p[3] < 128 or q[3] < 128:
                    continue
                pairs[p[:3]][q[:3]] += 1
    return {n: (c.most_common(1)[0][0], sum(c.values())) for n, c in pairs.items()}


def our_colours(folder):
    cnt = Counter()
    for f in glob.glob(os.path.join(folder, '*.png')):
        im = Image.open(f).convert('RGBA')
        for n, c in im.getcolors(1 << 20) or []:
            if c[3] >= 128:
                cnt[c[:3]] += n
    return cnt


def dist(l1, l2):
    # lightness counts less than hue: our artist's shades differ from the official ones
    return math.sqrt(0.5 * (l1[0] - l2[0]) ** 2 + (l1[1] - l2[1]) ** 2 + (l1[2] - l2[2]) ** 2)


def spatial_votes(folder, src, dex):
    """Lay our front / back views over the official ones (both cropped to the Pokemon and scaled
    to the same box): which official colour sits where each of our colours is. The poses differ,
    but the body is where the body is - it tells the body apart from a same-coloured detail."""
    votes = defaultdict(Counter)
    views = [('*_1_mae_1.png', ''), ('*_1_mae_2.png', ''), ('*_4_ushiro_1.png', 'back/'), ('*_4_ushiro_2.png', 'back/'), ('Pokemon-*.png', '')]
    for pat, sub in views:
        mine = [f for f in glob.glob(os.path.join(folder, pat)) if not f.endswith('-No.png')]
        off = os.path.join(src, sub + f'{dex}.png')
        if not mine or not os.path.exists(off):
            continue
        a = Image.open(mine[0]).convert('RGBA')
        o = Image.open(off).convert('RGBA')
        ba, bo = a.getbbox(), o.getbbox()
        if not ba or not bo:
            continue
        a = a.crop(ba)
        o = o.crop(bo).resize(a.size, Image.NEAREST)
        pa, po = a.load(), o.load()
        for y in range(a.height):
            for x in range(a.width):
                p, q = pa[x, y], po[x, y]
                if p[3] >= 128 and q[3] >= 128:
                    votes[p[:3]][q[:3]] += 1
    return votes


def match(ours, offmap, votes=None):
    offs = [(n, s, w, srgb_to_lab(n), srgb_to_lab(s)) for n, (s, w) in offmap.items() if w >= 3]
    total = sum(w for _, _, w, _, _ in offs) or 1
    out = {}
    unsure, kept = [], []
    for c in ours:
        lc = srgb_to_lab(c)
        v = (votes or {}).get(c, Counter())
        vt = sum(v.values())
        # Votes by shiny group: official colours that turn into the same shiny colour count together.
        grp = Counter()
        for n, k in v.items():
            if n in offmap:
                grp[offmap[n][0]] += k
        def score(o):
            sp = grp[o[1]] / vt if vt >= 4 else 0.0
            return 0.5 * dist(lc, o[3]) - 2.0 * math.log(1 + 50 * o[2] / total) - 60.0 * sp
        best = min(offs, key=score)
        n, s, w, ln, ls = best
        if ln[0] < 15 and lc[0] >= 20:
            unsure.append(c)   # a dark shade that lined up with the outline: decide from its neighbours
            continue
        if n == s or dist(ln, ls) < 3:
            kept.append(c)
            continue   # this part keeps its colour
        if lc[0] < 18 and ln[0] < 25 and ls[0] < 25:
            continue   # outlines stay dark
        # the shiny colour, shifted by how much lighter / darker our shade is than the official one
        L = ls[0] + (lc[0] - ln[0])
        k = 0.1 if dist(ln, ls) > 25 else 0.6   # a big change (new hue) keeps little of our tint
        A = ls[1] + (lc[1] - ln[1]) * k
        B = ls[2] + (lc[2] - ln[2]) * k
        out[c] = lab_to_srgb((max(0, min(100, L)), A, B))
    # Near-identical shades follow each other: a colour whose match was unsure (or that kept its
    # colour while a nearly identical shade changed) takes the same shift as its closest changed twin.
    changed = [(c2, srgb_to_lab(c2), srgb_to_lab(s2)) for c2, s2 in out.items()]
    for c in unsure + kept:
        lc = srgb_to_lab(c)
        near = min(changed, key=lambda t: dist(lc, t[1]), default=None)
        if near is None:
            continue
        limit = 14 if c in unsure else 6
        if dist(lc, near[1]) > limit:
            continue
        d = [near[2][i] - near[1][i] for i in range(3)]
        out[c] = lab_to_srgb((max(0, min(100, lc[0] + d[0])), lc[1] + d[1], lc[2] + d[2]))
    return out


def hexc(c):
    return '%02X%02X%02X' % tuple(c[:3])


def write(folder, mapping, dex, name):
    path = os.path.join(folder, 'shiny.txt')
    lines = [f'# {name} - shiny colours (from the official shiny sprite, made by tools/shinygen.py)',
             '# Each line: one of this Pokemon\'s sprite colours > its shiny colour. Edit freely.']
    for c, s in sorted(mapping.items()):
        lines.append(f'shiny = {hexc(c)} > {hexc(s)}')
    if not mapping:
        lines.append('# (its shiny colours are the same as its normal ones)')
    open(path, 'w').write('\n'.join(lines) + '\n')


def recolour(im, mapping):
    im = im.convert('RGBA')
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            p = px[x, y]
            if p[3] and p[:3] in mapping:
                px[x, y] = mapping[p[:3]] + (p[3],)
    return im


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    sheet = None
    if '--sheet' in sys.argv:
        sheet = sys.argv[sys.argv.index('--sheet') + 1]
        args = [a for a in args if a != sheet]
    src, only = args[0], set(args[1:])
    rows = []
    for folder in sorted(glob.glob(os.path.join(ROOT, '*'))):
        name = os.path.basename(folder)
        txt = os.path.join(folder, 'pokemon.txt')
        if name.startswith('_') or not os.path.exists(txt) or (only and name not in only):
            continue
        m = re.search(r'(?m)^dex\s*=\s*(\d+)', open(txt, encoding='utf-8').read())
        if not m:
            continue
        dex = int(m.group(1))
        offmap = official_map(src, dex)
        ours = our_colours(folder)
        if not offmap or not ours:
            print('skipped', name, '(no official sprite)' if not offmap else '(no sprites)')
            continue
        mapping = match(ours, offmap, spatial_votes(folder, src, dex))
        for k, v in OVERRIDES.get(name, {}).items():
            mapping[tuple(int(k[i:i + 2], 16) for i in (0, 2, 4))] = tuple(int(v[i:i + 2], 16) for i in (0, 2, 4))
        write(folder, mapping, dex, name)
        rows.append((dex, name, folder, mapping))
        print(f'{dex:3d} {name:12s} {len(ours):2d} colours, {len(mapping):2d} change')
    if sheet:
        make_sheet(sorted(rows), src, sheet)


def make_sheet(rows, src, out):
    cell, cols = 96, 5   # per Pokemon: our normal, our shiny, official shiny
    w = cols * cell * 3 + cols * 8
    h = math.ceil(len(rows) / cols) * (cell + 14)
    sh = Image.new('RGBA', (w, h), (40, 36, 56, 255))
    from PIL import ImageDraw
    d = ImageDraw.Draw(sh)
    for i, (dex, name, folder, mapping) in enumerate(rows):
        x0 = (i % cols) * (cell * 3 + 8)
        y0 = (i // cols) * (cell + 14)
        front = sorted(glob.glob(os.path.join(folder, '*_1_mae_1.png')))
        if not front:
            continue
        a = Image.open(front[0]).convert('RGBA')
        b = recolour(a.copy(), mapping)
        o = Image.open(os.path.join(src, f'shiny/{dex}.png')).convert('RGBA')
        bb = o.getbbox()
        if bb:
            o = o.crop(bb)
        for k, im in enumerate((a, b, o)):
            s = max(1, min(cell // im.width, cell // im.height)) if k < 2 else min(cell / im.width, cell / im.height)
            im2 = im.resize((max(1, int(im.width * s)), max(1, int(im.height * s))), Image.NEAREST)
            sh.alpha_composite(im2, (x0 + k * cell + (cell - im2.width) // 2, y0 + (cell - im2.height)))
        d.text((x0 + 2, y0 + cell), f'{dex} {name}', fill=(230, 230, 230, 255))
    sh.save(out)
    print('sheet', out)


if __name__ == '__main__':
    main()
