#!/usr/bin/env python3
"""FireRed's weather sprites (pret/pokefirered graphics/weather) -> World/weather_*.png (RGBA).
usage: frlg_weather.py <pokefirered> <World dir>"""
import os, sys
from PIL import Image

def main():
    root, out = sys.argv[1], sys.argv[2]
    for n in ["rain", "snow0", "snow1", "fog_horizontal", "fog_diagonal", "sandstorm"]:
        im = Image.open(os.path.join(root, "graphics/weather", n + ".png"))
        pal = im.getpalette()
        o = Image.new("RGBA", im.size)
        px = o.load()
        for y in range(im.size[1]):
            for x in range(im.size[0]):
                i = im.getpixel((x, y))
                if i:   # index 0 is see-through
                    px[x, y] = (pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], 255)
        o.save(os.path.join(out, "weather_" + n + ".png"), optimize=True)
        print("wrote", n, im.size)

main()
