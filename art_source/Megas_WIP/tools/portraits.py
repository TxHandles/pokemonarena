"""Menu portraits: the official Gen 7 menu icons (PokeAPI sprites, versions/generation-vii/icons), cropped
- the same source as the Gen 4-6 Pokemon's portraits. The old chart portraits stay in sprites_round4/."""
import os, glob, shutil
from PIL import Image
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICONS = '/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/pk2/sprites/pokemon/versions/generation-vii/icons'
IDS = {"Mega Venusaur": 10033, "Mega Charizard X": 10034, "Mega Charizard Y": 10035, "Mega Blastoise": 10036,
       "Mega Alakazam": 10037, "Mega Gengar": 10038, "Mega Kangaskhan": 10039, "Mega Pinsir": 10040,
       "Mega Gyarados": 10041, "Mega Aerodactyl": 10042, "Mega Mewtwo X": 10043, "Mega Mewtwo Y": 10044,
       "Mega Slowbro": 10071, "Mega Pidgeot": 10073, "Mega Beedrill": 10090}
for mega, i in IDS.items():
    folder = os.path.join(root, 'sprites', mega)
    keep = os.path.join(root, 'sprites_round4', mega)
    if not os.path.exists(keep): shutil.copytree(folder, keep)
    old = glob.glob(os.path.join(folder, 'Pokemon-*.png'))[0]
    im = Image.open(os.path.join(ICONS, f'{i}.png')).convert('RGBA')
    im = im.crop(im.getbbox())
    im.save(old)
    print(mega, im.size)
