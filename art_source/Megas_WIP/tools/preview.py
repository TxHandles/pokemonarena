import sys, os, glob
sys.path.insert(0, os.path.dirname(__file__))
from sheet2frames import SHEETS, frames
from PIL import Image, ImageDraw, ImageFont
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
font = ImageFont.truetype("/home/claude/arena-clash/fonts/PixelifySans-Medium.ttf", 22)
small = ImageFont.truetype("/home/claude/arena-clash/fonts/PixelifySans-Medium.ttf", 16)
S = 3            # scale
CELL = 40 * S    # one frame slot
names = sorted(os.listdir(os.path.join(root, "sprites")))
dirs = [("1_mae", "front"), ("2_hidari", "left"), ("3_migi", "right"), ("4_ushiro", "back")]
LABEL_W = 250
rows = []
for n in names:
    rows.append((n, "now"))
    if n in SHEETS: rows.append((n, "sheet"))
H = 70 + len(rows) * (CELL + 14)
W = LABEL_W + 10 * CELL + 40
img = Image.new("RGBA", (W, H), (26, 24, 38, 255))
d = ImageDraw.Draw(img)
d.text((16, 14), "Mega sprites: what we have now (in the game's size, x3)", font=font, fill=(255, 210, 90))
cols = ["front", "left", "right", "back", "", "shiny front", "shiny left", "shiny right", "shiny back"]
for i, c in enumerate(cols):
    d.text((LABEL_W + i * CELL + 6, 46), c, font=small, fill=(160, 150, 190))
y = 70
def put(fr, x, y):
    if fr is None: return
    fr = fr.resize((fr.width * S, fr.height * S), Image.NEAREST)
    img.alpha_composite(fr, (x + (CELL - fr.width) // 2, y + CELL - fr.height - 4))
for n, kind in rows:
    shade = (40, 38, 58, 255) if kind == "now" else (34, 46, 54, 255)
    d.rectangle((8, y, W - 8, y + CELL + 6), fill=shade)
    d.text((16, y + 10), n.replace("Mega ", "Mega\n"), font=font, fill=(235, 235, 245))
    d.text((16, y + CELL - 22), "current (review 4)" if kind == "now" else "from your sheet", font=small,
           fill=(150, 150, 170) if kind == "now" else (120, 220, 200))
    if kind == "now":
        folder = os.path.join(root, "sprites", n)
        for i, (k, _) in enumerate(dirs):
            g = glob.glob(os.path.join(folder, "*_%s_1.png" % k))
            put(Image.open(g[0]).convert("RGBA") if g else None, LABEL_W + i * CELL, y)
    else:
        fr, _ = frames(root, n)
        for i, (_, dn) in enumerate(dirs):
            put(fr.get((dn, 0)), LABEL_W + i * CELL, y)
            put(fr.get((dn, 4)), LABEL_W + (5 + i) * CELL, y)
    y += CELL + 14
out = os.path.join(root, "megas_preview.png")
img.save(out)
print(out, img.size)
