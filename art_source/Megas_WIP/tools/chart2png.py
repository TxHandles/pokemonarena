# Reads a pixel chart (22 px grid, 1-based labels) into a one-pixel-per-square sprite.
# plain = the chart without numbers (colours), numbered = the same chart with numbers (which squares are filled:
# a white square that belongs to the Pokemon still has its number printed in it).
import sys
from PIL import Image
def convert(plain, numbered, dst, cols, rows, pitch=22):
    a=Image.open(plain).convert('RGB'); pa=a.load()
    b=Image.open(numbered).convert('RGB'); pb=b.load()
    out=Image.new('RGBA',(cols,rows),(0,0,0,0)); o=out.load()
    for r in range(rows):
        for c in range(cols):
            x0=pitch*(c+1); y0=pitch*(r+1)
            filled=False
            for y in range(y0+4,y0+pitch-3):
                for x in range(x0+4,x0+pitch-3):
                    if min(pb[x,y])<200: filled=True; break
                if filled: break
            if not filled: continue
            o[c,r]=pa[x0+pitch//2,y0+3][:3]+(255,) if False else pa[x0+pitch//2,y0+pitch//2]+(255,)
    bb=out.getbbox(); out=out.crop(bb) if bb else out
    out.save(dst); return out
if __name__=='__main__':
    o=convert(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5])); print(o.size)
