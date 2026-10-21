"""Install the Megas into Pokemon/<Mega name>/: the frames, portrait and shiny.txt from sprites/, and a
pokemon.txt with the official Mega stats, types and ability (PokeAPI data), mega_of and mega_stone."""
import os, shutil, csv
ROOT = '/home/claude/arena-clash'
SRC = os.path.join(ROOT, 'art_source/Megas_WIP/sprites')
CSV = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/pokeapi/api/data/v2/csv'
MEGAS = [("Mega Venusaur", "Venusaur", "venusaur-mega", "Venusaurite"), ("Mega Charizard X", "Charizard", "charizard-mega-x", "Charizardite X"),
         ("Mega Charizard Y", "Charizard", "charizard-mega-y", "Charizardite Y"), ("Mega Blastoise", "Blastoise", "blastoise-mega", "Blastoisinite"),
         ("Mega Beedrill", "Beedrill", "beedrill-mega", "Beedrillite"), ("Mega Pidgeot", "Pidgeot", "pidgeot-mega", "Pidgeotite"),
         ("Mega Alakazam", "Alakazam", "alakazam-mega", "Alakazite"), ("Mega Slowbro", "Slowbro", "slowbro-mega", "Slowbronite"),
         ("Mega Gengar", "Gengar", "gengar-mega", "Gengarite"), ("Mega Kangaskhan", "Kangaskhan", "kangaskhan-mega", "Kangaskhanite"),
         ("Mega Pinsir", "Pinsir", "pinsir-mega", "Pinsirite"), ("Mega Gyarados", "Gyarados", "gyarados-mega", "Gyaradosite"),
         ("Mega Aerodactyl", "Aerodactyl", "aerodactyl-mega", "Aerodactylite"), ("Mega Mewtwo X", "Mewtwo", "mewtwo-mega-x", "Mewtwonite X"),
         ("Mega Mewtwo Y", "Mewtwo", "mewtwo-mega-y", "Mewtwonite Y"), ("Mega Clefable", "Clefable", "clefable-mega", "Clefablite"),
         ("Mega Victreebel", "Victreebel", "victreebel-mega", "Victreebelite"), ("Mega Starmie", "Starmie", "starmie-mega", "Starminite"),
         ("Mega Dragonite", "Dragonite", "dragonite-mega", "Dragoninite")]
P = {r['identifier']: r for r in csv.DictReader(open(os.path.join(CSV, 'pokemon.csv')))}
types = {r['id']: r['identifier'] for r in csv.DictReader(open(os.path.join(CSV, 'types.csv')))}
abil = {r['id']: r['identifier'] for r in csv.DictReader(open(os.path.join(CSV, 'abilities.csv')))}
st, ty, ab = {}, {}, {}
for r in csv.DictReader(open(os.path.join(CSV, 'pokemon_stats.csv'))): st.setdefault(r['pokemon_id'], {})[r['stat_id']] = r['base_stat']
for r in csv.DictReader(open(os.path.join(CSV, 'pokemon_types.csv'))): ty.setdefault(r['pokemon_id'], []).append((int(r['slot']), types[r['type_id']]))
for r in csv.DictReader(open(os.path.join(CSV, 'pokemon_abilities.csv'))): ab.setdefault(r['pokemon_id'], []).append((int(r['slot']), abil[r['ability_id']]))
title = lambda s: ' '.join(w.capitalize() for w in s.replace('-', ' ').split())
def base_val(base, key):
    for line in open(os.path.join(ROOT, 'Pokemon', base, 'pokemon.txt'), encoding='utf-8', errors='replace'):
        k = line.split('=')[0].strip()
        if k == key: return line.split('=', 1)[1].split('#')[0].strip()
    return ''
for name, base, ident, stone in MEGAS:
    r = P[ident]; i = r['id']; s = st[i]
    t = [title(x) for _, x in sorted(ty[i])]
    dst = os.path.join(ROOT, 'Pokemon', name)
    if os.path.exists(dst): shutil.rmtree(dst)
    shutil.copytree(os.path.join(SRC, name), dst)
    for f in os.listdir(dst):
        if not (f.endswith('.png') or f == 'shiny.txt') or f == name + '.png': os.remove(os.path.join(dst, f))
    dex = base_val(base, 'dex')
    txt = f"""# {name}: the Mega Evolution of {base} (official Mega stats, types and ability). Not caught or met on its
# own: a {base} holding the {stone} Mega Evolves into it in battle (once per battle).
name = {name}
dex  = {dex}
type = {' / '.join(t)}
mega_of = {base}
mega_stone = {stone}

hp         = {s['1']}
attack     = {s['2']}
defense    = {s['3']}
sp_attack  = {s['4']}
sp_defense = {s['5']}
speed      = {s['6']}

height = {int(r['height']) / 10:.1f}      # Pokedex height in meters
weight = {int(r['weight']) / 10:.1f}      # Pokedex weight in kg
hovers = {base_val(base, 'hovers') or 'no'}
heavy  = {base_val(base, 'heavy') or 'no'}
winged = {base_val(base, 'winged') or 'no'}
ability = {title(sorted(ab[i])[0][1])}
category = {base_val(base, 'category') or ''}
"""
    open(os.path.join(dst, 'pokemon.txt'), 'w').write(txt)
    print(name, t, [s[k] for k in '123456'], title(sorted(ab[i])[0][1]))
