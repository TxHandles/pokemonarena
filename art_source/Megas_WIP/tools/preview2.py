import sys, os, glob
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, '/home/claude/arena-clash/tools')
from sheet2frames import SHEETS, COMPARE, frames
import shinygen
from PIL import Image, ImageDraw, ImageFont
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
F = "/home/claude/arena-clash/fonts/PixelifySans-Medium.ttf"
font, small = ImageFont.truetype(F, 22), ImageFont.truetype(F, 16)
S, CELL, LW = 3, 120, 250
dirs = ["1_mae", "2_hidari", "3_migi", "4_ushiro"]
names = sorted(os.listdir(os.path.join(root, "sprites")))
def shinymap(folder):
    p = os.path.join(folder, "shiny.txt"); m = {}
    if os.path.exists(p):
        for l in open(p):
            if l.startswith("shiny ="):
                a, b = l.split("=")[1].split(">")
                m[tuple(int(a.strip()[i:i+2], 16) for i in (0, 2, 4))] = tuple(int(b.strip()[i:i+2], 16) for i in (0, 2, 4))
    return m
H = 70 + (len(names) + len(COMPARE)) * (CELL + 14); W = LW + 10 * CELL + 40
img = Image.new("RGBA", (W, H), (26, 24, 38, 255)); d = ImageDraw.Draw(img)
d.text((16, 14), "Mega sprites - updated (game size, x3)   teal = from your sheets   orange = new options to pick", font=font, fill=(255, 210, 90))
for i, c in enumerate(["front", "left", "right", "back", "portrait", "shiny front", "shiny left", "shiny right", "shiny back", "shiny portrait"]):
    d.text((LW + i * CELL + 6, 46), c, font=small, fill=(160, 150, 190))
def put(fr, x, y):
    fr = fr.resize((fr.width * S, fr.height * S), Image.NEAREST)
    img.alpha_composite(fr, (x + (CELL - fr.width) // 2, y + CELL - fr.height - 4))
y = 70
for n in names:
    folder = os.path.join(root, "sprites", n)
    new = n in SHEETS or n == "Mega Beedrill"
    d.rectangle((8, y, W - 8, y + CELL + 6), fill=(34, 46, 54, 255) if new else (40, 38, 58, 255))
    d.text((16, y + 10), n.replace("Mega ", "Mega\n"), font=font, fill=(235, 235, 245))
    tag = "from your sheet" + (" (shrunk 2/3)" if n == "Mega Gyarados" else "") if new else "round 4"
    d.text((16, y + CELL - 22), tag, font=small, fill=(120, 220, 200) if new else (150, 150, 170))
    m = shinymap(folder)
    for i, k in enumerate(dirs):
        g = sorted(glob.glob(os.path.join(folder, "*_%s_1.png" % k)))
        if not g: continue
        fr = Image.open(g[0]).convert("RGBA")
        put(fr, LW + i * CELL, y)
        if m: put(shinygen.recolour(fr.copy(), m), LW + (5 + i) * CELL, y)
    g = glob.glob(os.path.join(folder, "Pokemon-*.png"))
    if g:
        po = Image.open(g[0]).convert("RGBA")
        put(po, LW + 4 * CELL, y)
        if m: put(shinygen.recolour(po.copy(), m), LW + 9 * CELL, y)
    y += CELL + 14
    if n in COMPARE:   # the user's new sheet, to compare with round 4
        d.rectangle((8, y, W - 8, y + CELL + 6), fill=(60, 44, 30, 255))
        d.text((16, y + 10), n.replace("Mega ", "Mega\n"), font=font, fill=(235, 235, 245))
        d.text((16, y + CELL - 22), "OPTION: your new sheet", font=small, fill=(255, 190, 110))
        fr, _ = frames(root, n, compare=True)
        for i, dn in enumerate(["front", "left", "right", "back"]):
            put(fr[(dn, 0)], LW + i * CELL, y)
            put(fr[(dn, 4)], LW + (5 + i) * CELL, y)
        y += CELL + 14
img.save(os.path.join(root, "megas_preview.png"))
# Gyarados close-up: old round 4, the sheet at full size, and the shrunk frames (x6)
rows = [("round 4", os.path.join(root, "sprites_round4", "Mega Gyarados")), ("shrunk to match", os.path.join(root, "sprites", "Mega Gyarados"))]
G = Image.new("RGBA", (1900, 560), (26, 24, 38, 255)); gd = ImageDraw.Draw(G)
gd.text((16, 12), "Mega Gyarados - revised (x6): your sheet shrunk to 2/3, only its own colours", font=font, fill=(255, 210, 90))
m = shinymap(rows[1][1])
yy = 60
for label, folder in rows:
    gd.text((16, yy + 100), label, font=font, fill=(235, 235, 245))
    for i, k in enumerate(dirs):
        for j, shiny in enumerate((False, True)):
            if shiny and label == "round 4": continue
            f = sorted(glob.glob(os.path.join(folder, "*_%s_1.png" % k)))[0]
            fr = Image.open(f).convert("RGBA")
            if shiny: fr = shinygen.recolour(fr, m)
            fr = fr.resize((fr.width * 6, fr.height * 6), Image.NEAREST)
            G.alpha_composite(fr, (230 + (i + 4 * j) * 200 + (190 - fr.width) // 2, yy + 230 - fr.height))
    yy += 250
G = G.crop((0, 0, 230 + 8 * 200, 560))
G.save(os.path.join(root, "gyarados_revised.png"))
print("ok")
