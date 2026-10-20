from PIL import Image
import numpy as np

def shrink(img, k):
    """Smaller, keeping only the sprite's own colours (each pixel snaps to the closest one)."""
    pal = [c[:3] for n, c in img.getcolors(1 << 16) if c[3] >= 128]
    w, h = max(1, round(img.width * k)), max(1, round(img.height * k))
    sm = np.array(img.resize((w, h), Image.LANCZOS)).astype(int)
    P = np.array(pal)
    out = np.zeros_like(sm)
    for y in range(h):
        for x in range(w):
            p = sm[y, x]
            if p[3] < 110: continue
            i = np.argmin(((P - p[:3]) ** 2).sum(1))
            out[y, x, :3] = P[i]; out[y, x, 3] = 255
    return Image.fromarray(out.astype(np.uint8), 'RGBA')

