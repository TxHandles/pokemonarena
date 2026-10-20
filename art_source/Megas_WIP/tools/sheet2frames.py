"""Cut the user's 4-row Mega sheets (front, left, right, back; 4 walk frames normal + 4 shiny) into
game-size frames. Sheets drawn at 2x are halved with nearest-neighbour (no redrawing)."""
from PIL import Image
import numpy as np, os

SHEETS = {   # Mega name: (file, origin x, origin y, cell w, cell h, has shiny columns)
    "Mega Blastoise": ("Mega_Blastoise_sheet.png", 0, 0, 64, 64, True),
    "Mega Venusaur": ("Mega_Venusaur_sheet.png", 0, 0, 64, 64, True),
    "Mega Charizard Y": ("Mega_CharizardY_sheet.png", 0, 0, 64, 64, True),
    "Mega Charizard X": ("Mega_CharizardX_sheet_a.webp", 73, 0, 64, 64, False),
    "Mega Gengar": ("Mega_Gengar_sheet2.png", 0, 0, 32, 32, True, 2),
    "Mega Gyarados": ("Mega_Gyarados_sheet.png", 0, 0, 64, 64, True),
    "Mega Kangaskhan": ("Mega_Kangaskhan_sheet.png", 0, 0, 64, 64, True),
    "Mega Mewtwo Y": ("Mega_MewtwoY_sheet.png", 0, 0, 64, 64, True),
    "Mega Alakazam": ("Mega_Alakazam_sheet.png", 0, 0, 64, 64, True),
    "Mega Slowbro": ("Mega_Slowbro_sheet.png", 0, 0, 64, 64, True),
    "Mega Pinsir": ("Mega_Pinsir_sheet.png", 0, 0, 64, 64, True),
    "Mega Aerodactyl": ("Mega_Aerodactyl_sheet.png", 0, 0, 64, 64, True),
    "Mega Clefable": ("Mega_Clefable_sheet.png", 0, 0, 64, 64, False),
    "Mega Victreebel": ("Mega_Victreebel_sheet.png", 0, 0, 128, 128, False),
    "Mega Starmie": ("Mega_Starmie_sheet.png", 0, 0, 64, 64, False),
    "Mega Dragonite": ("Mega_Dragonite_sheet.png", 0, 0, 64, 64, False),
}
COMPARE = {}   # sheets to compare with the round-4 sprites before choosing (not applied yet)

def clean(cell, bg):
    a = np.array(cell.convert("RGBA")).astype(int)
    d = np.abs(a[:, :, :3] - np.array(bg[:3])).sum(2)
    a[d < 10, 3] = 0
    a[a[:, :, 3] < 128, 3] = 0
    a[a[:, :, 3] > 0, 3] = 255
    return a

def halve(a):
    best, bestScore = None, -1
    for oy in (0, 1):
        for ox in (0, 1):
            s = a[oy::2, ox::2]
            up = np.repeat(np.repeat(s, 2, 0), 2, 1)[: a.shape[0] - oy, : a.shape[1] - ox]
            ref = a[oy:oy + up.shape[0], ox:ox + up.shape[1]]
            m = ref[:, :, 3] > 0
            score = (np.abs(up - ref).sum(2) < 30)[m].mean() if m.any() else 0
            if score > bestScore: best, bestScore = s, score
    return best, bestScore

def frames(root, name, compare=False):
    spec = (COMPARE[name] if compare else SHEETS[name])
    f, ox, oy, cw, ch, shiny = spec[:6]
    so = spec[6] if len(spec) > 6 else 4   # where the shiny columns start
    im = Image.open(os.path.join(root, f)).convert("RGBA")
    bg = im.getpixel((0, 0)) if im.getpixel((0, 0))[3] > 0 else (0, 0, 0, 0)
    out = {}
    doubled = None
    for r, d in enumerate(["front", "left", "right", "back"]):
        for c in range(2 * so if shiny else so):
            box = (ox + c * cw, oy + r * ch, ox + (c + 1) * cw, oy + (r + 1) * ch)
            if box[2] > im.width or box[3] > im.height: box = (box[0], box[1], min(box[2], im.width), min(box[3], im.height))
            a = clean(im.crop(box), bg) if bg[3] > 0 else np.array(im.crop(box)).astype(int)
            h, score = halve(a)
            if doubled is None: doubled = score > 0.9
            img = Image.fromarray((h if doubled else a).astype(np.uint8), "RGBA")
            bb = img.getbbox()
            if bb: img = img.crop(bb)
            out[(d, c if c < so else 4 + c - so)] = img
    return out, doubled
