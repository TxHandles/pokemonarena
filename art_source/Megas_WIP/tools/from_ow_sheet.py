"""The Megas from the "Mega Evolution Overworld Sprites" sheet (by Kidkatt, Larryturbo and Princess-Phoenix -
please give proper credit). Each block on the sheet: 2 walk frames x 4 rows (front, left, right, back); 32 px
frames in the top 4 rows of blocks, 64 px frames in the bottom row (the big legendaries).
The sheet came as a JPG, so the background is keyed out from the block's edges (flood fill with a tolerance)
and the colours are snapped to a small palette per Pokemon, which takes out the compression noise.
    python from_ow_sheet.py [sheet] [Mega name ...]
Writes sprites/<Mega>/ (frames, portrait, shiny.txt) and sprites/<Mega>/pokemon.txt (PokeAPI data)."""
import os, sys, csv
from collections import deque, Counter
from PIL import Image
sys.path.insert(0, '/home/claude/arena-clash/tools')
import shinygen
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SHEET = os.path.join(ROOT, 'ow_sheet', 'Mega_Overworld_Sprites_Kidkatt_Larryturbo_Princess-Phoenix.jpg')
CSV = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/pokeapi/api/data/v2/csv'
OFF = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/pk2/sprites/pokemon'

# (block column, block row) on the sheet -> (Mega name, PokeAPI id, held item)
TOP = [
    [None] * 12,   # row 0: Venusaur ... Gyarados (we have these)
    [None, None, None, ("Mega Ampharos", 10045, "Ampharosite"), ("Mega Scizor", 10046, "Scizorite"),
     ("Mega Heracross", 10047, "Heracronite"), ("Mega Houndoom", 10048, "Houndoominite"),
     ("Mega Tyranitar", 10049, "Tyranitarite"), ("Mega Sceptile", 10065, "Sceptilite"),
     ("Mega Blaziken", 10050, "Blazikenite"), ("Mega Swampert", 10064, "Swampertite"),
     ("Mega Gardevoir", 10051, "Gardevoirite")],
    [("Mega Sableye", 10066, "Sablenite"), ("Mega Mawile", 10052, "Mawilite"), ("Mega Aggron", 10053, "Aggronite"),
     ("Mega Medicham", 10054, "Medichamite"), ("Mega Manectric", 10055, "Manectite"),
     ("Mega Sharpedo", 10070, "Sharpedonite"), ("Mega Camerupt", 10087, "Cameruptite"),
     ("Mega Altaria", 10067, "Altarianite"), ("Mega Banette", 10056, "Banettite"), ("Mega Absol", 10057, "Absolite"),
     ("Mega Glalie", 10074, "Glalitite"), ("Mega Salamence", 10089, "Salamencite")],
    [("Mega Metagross", 10076, "Metagrossite"), ("Mega Lopunny", 10088, "Lopunnite"),
     ("Mega Garchomp", 10058, "Garchompite"), ("Mega Lucario", 10059, "Lucarionite"),
     ("Mega Abomasnow", 10060, "Abomasite"), ("Mega Gallade", 10068, "Galladite"), ("Mega Audino", 10069, "Audinite"),
     None],   # Mega Diancie: cut from the Gen 6 sheet instead (Pokemon/Mega Diancie)
]
BIG = [("Mega Steelix", 10072, "Steelixite"), ("Mega Latias", 10062, "Latiasite"), ("Mega Latios", 10063, "Latiosite"),
       ("Primal Kyogre", 10077, "Blue Orb"), ("Primal Groudon", 10078, "Red Orb"),
       ("Mega Rayquaza", 10079, "move:Dragon Ascent")]
KEY = ['1_mae', '2_hidari', '3_migi', '4_ushiro']


def dist2(a, b):
    return sum((x - y) ** 2 for x, y in zip(a[:3], b[:3]))


def key_out(cell, tol=48):
    """Background -> transparent: flood fill from the edges through pixels close to the background colour."""
    w, h = cell.size
    px = cell.load()
    edge = Counter(px[x, y][:3] for x in range(w) for y in (0, h - 1))
    edge.update(px[x, y][:3] for y in range(h) for x in (0, w - 1))
    bgs = [c for c, _ in edge.most_common(6)]
    bg = tuple(sorted(c[i] for c in bgs)[len(bgs) // 2] for i in range(3))
    near = lambda p: dist2(p, bg) <= tol * tol
    out = cell.convert('RGBA')
    o = out.load()
    seen = set()
    q = deque((x, y) for x in range(w) for y in (0, h - 1))
    q.extend((x, y) for y in range(h) for x in (0, w - 1))
    while q:
        x, y = q.popleft()
        if (x, y) in seen or not (0 <= x < w and 0 <= y < h):
            continue
        seen.add((x, y))
        if not near(px[x, y]):
            continue
        o[x, y] = (0, 0, 0, 0)
        q.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    # pale halo pixels left between the background and the outline (JPG ringing)
    for _ in range(2):
        kill = []
        for y in range(h):
            for x in range(w):
                if o[x, y][3] == 0:
                    continue
                nb = [o[x + dx, y + dy] for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= x + dx < w and 0 <= y + dy < h]
                clear = sum(1 for n in nb if n[3] == 0)
                if clear >= 3 or (clear >= 1 and dist2(o[x, y], bg) <= (tol * 1.6) ** 2):
                    kill.append((x, y))
        for x, y in kill:
            o[x, y] = (0, 0, 0, 0)
    # specks: tiny islands of 1-2 pixels left over from the JPG
    seen = set()
    for y in range(h):
        for x in range(w):
            if o[x, y][3] == 0 or (x, y) in seen:
                continue
            comp, q = [], [(x, y)]
            seen.add((x, y))
            while q:
                a, b = q.pop()
                comp.append((a, b))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        n = (a + dx, b + dy)
                        if 0 <= n[0] < w and 0 <= n[1] < h and n not in seen and o[n][3]:
                            seen.add(n)
                            q.append(n)
            if len(comp) <= 2:
                for c in comp:
                    o[c] = (0, 0, 0, 0)
    return out


def snap(frames, n=22):
    """All 8 frames to one shared palette (median cut over the opaque pixels)."""
    pix = [p[:3] for f in frames for p in (f.get_flattened_data() if hasattr(f, 'get_flattened_data') else f.getdata()) if p[3]]
    strip = Image.new('RGB', (len(pix), 1))
    strip.putdata(pix)
    pal = strip.quantize(n, method=Image.Quantize.MEDIANCUT).getpalette()[:n * 3]
    cols = [tuple(pal[i:i + 3]) for i in range(0, len(pal), 3)]
    out = []
    for f in frames:
        g = f.copy()
        d = g.load()
        for y in range(g.height):
            for x in range(g.width):
                if d[x, y][3]:
                    c = min(cols, key=lambda c: dist2(c, d[x, y]))
                    d[x, y] = c + (255,)
        out.append(g)
    return out


def read_csv(name):
    return list(csv.DictReader(open(os.path.join(CSV, name), encoding='utf-8')))


def names_by(table, idcol, lang='9'):
    return {r[idcol]: r['name'] for r in read_csv(table) if r['local_language_id'] == lang}


def main():
    sheet = Image.open(sys.argv[1] if len(sys.argv) > 1 and os.path.exists(sys.argv[1]) else SHEET).convert('RGB')
    only = [a for a in sys.argv[1:] if not os.path.exists(a)]
    poke = {r['id']: r for r in read_csv('pokemon.csv')}
    stats = {}
    for r in read_csv('pokemon_stats.csv'):
        stats.setdefault(r['pokemon_id'], {})[r['stat_id']] = int(r['base_stat'])
    tnames = names_by('type_names.csv', 'type_id')
    types = {}
    for r in read_csv('pokemon_types.csv'):
        types.setdefault(r['pokemon_id'], []).append((int(r['slot']), tnames[r['type_id']]))
    anames = names_by('ability_names.csv', 'ability_id')
    abil = {}
    for r in read_csv('pokemon_abilities.csv'):
        if r['is_hidden'] == '0' and r['slot'] == '1':
            abil[r['pokemon_id']] = anames[r['ability_id']]
    snames = names_by('pokemon_species_names.csv', 'pokemon_species_id')
    jobs = []
    for br, row in enumerate(TOP):
        for bc, e in enumerate(row):
            if e:
                jobs.append((e, bc * 64, br * 128, 32))
    for bc, e in enumerate(BIG):
        jobs.append((e, bc * 128, 512, 64))
    for (name, pid, item), x0, y0, fs in jobs:
        if only and name not in only:
            continue
        p = poke[str(pid)]
        base = snames[p['species_id']]
        dex = int(p['species_id'])
        pre = f'{dex:03d}'
        raw = []
        for r in range(4):
            for c in range(2):
                raw.append(key_out(sheet.crop((x0 + c * fs, y0 + r * fs, x0 + (c + 1) * fs, y0 + (r + 1) * fs))))
        fr = snap(raw)
        folder = os.path.join(ROOT, 'sprites', name)
        os.makedirs(folder, exist_ok=True)
        for k in range(8):
            im = fr[k]
            bb = im.getbbox()
            im.crop(bb).save(os.path.join(folder, f'{pre}_{KEY[k // 2]}_{k % 2 + 1}.png'))
        icon = os.path.join(OFF, 'versions/generation-vii/icons', f'{pid}.png')
        po = Image.open(icon).convert('RGBA') if os.path.exists(icon) else fr[0]
        po.crop(po.getbbox()).save(os.path.join(folder, f'Pokemon-{pre}-{name.replace(" ", "-")}.png'))
        st = stats[str(pid)]
        ty = [t for _, t in sorted(types[str(pid)])]
        primal = name.startswith('Primal')
        how = (f"a {base} that knows Dragon Ascent Mega Evolves into it in battle (once per battle)." if item.startswith('move:')
               else f"a {base} holding the {item} returns to it in battle (Primal Reversion, once per battle)." if primal
               else f"a {base} holding the {item} Mega Evolves into it in battle (once per battle).")
        txt = f"""# {name}: the {'Primal form' if primal else 'Mega Evolution'} of {base} (official stats, types and ability). Not caught or met on its
# own: {how}
# Sprite: "Mega Evolution Overworld Sprites" by Kidkatt, Larryturbo and Princess-Phoenix.
name = {name}
dex  = {dex}
type = {' / '.join(ty)}
mega_of = {base}
mega_stone = {item}

hp         = {st['1']}
attack     = {st['2']}
defense    = {st['3']}
sp_attack  = {st['4']}
sp_defense = {st['5']}
speed      = {st['6']}

height = {int(p['height']) / 10:g}      # Pokedex height in meters
weight = {int(p['weight']) / 10:g}      # Pokedex weight in kg
hovers = {'yes' if name in ('Mega Latias', 'Mega Latios', 'Mega Rayquaza') else 'no'}
heavy  = {'yes' if int(p['weight']) >= 2000 else 'no'}
winged = {'yes' if name in ('Mega Latias', 'Mega Latios', 'Mega Rayquaza', 'Mega Salamence', 'Mega Altaria', 'Mega Charizard Y') else 'no'}
ability = {abil[str(pid)]}
category = 
"""
        open(os.path.join(folder, 'pokemon.txt'), 'w').write(txt)
        src = pid if any(w >= 3 for _, w in shinygen.official_map(OFF, pid).values()) else dex   # no official shiny sprite of the Mega: its base Pokemon's colours
        m = shinygen.match(shinygen.our_colours(folder), shinygen.official_map(OFF, src), shinygen.spatial_votes(folder, OFF, src))
        shinygen.write(folder, m, 'mega', name)
        print(f'{name:16s} #{pre} {"/".join(ty):16s} {abil[str(pid)]:16s} {item:18s} {fr[0].getbbox()} shiny {len(m)}')


if __name__ == '__main__':
    main()
