#!/usr/bin/env python3
"""Render a HeartGold/SoulSilver location model (the Models Resource's .dae + textures) as a 2D map in the
walked world's style: seen from the south and above, so the ground lines up with the 16-pixel walking grid
while house fronts and trees stand up the way FireRed draws them.

    hgss_render.py <location .dae> <out.png> [--tilt 1.0] [--info out.json]

The model's ground pieces are named after the HGSS map matrix cells they fill ("map14_07c" = column 14,
row 7, 32x32 tiles each, 16 units a tile), so every pixel can be placed in HGSS's world grid - the same
grid its event data (doors, people, signs) uses. --info writes that origin, and per tile the ground's
height and what the top surface there is textured with (for the walls / water / grass pass).
"""
import json, math, os, re, sys
import xml.etree.ElementTree as ET
import numpy as np
from PIL import Image

NS = {"c": "http://www.collada.org/2005/11/COLLADASchema"}
TILE = 16.0          # model units per tile
CHUNK = 32           # tiles per matrix cell


def floats(t):
    return np.array([float(x) for x in t.split()], dtype=np.float64) if t and t.strip() else np.zeros(0)


# the ground pieces, named after their matrix cells: "map14_07c" outdoors, "m_dun2503_00_01c" / "d36_00_01" in a cave
ISMAP = re.compile(r"^(?:map|m_dun\d+_|d\d+_)(\d+)_(\d+)")
# things the game places itself (we draw them as sprites, or they go away): item balls, smashable rocks, Cut trees
SKIP_NODE = re.compile(r"^(Pokeball[ _]Item|Breack[ _]Rock|Tree)([._]\d+)?$")


def load(dae):
    root = ET.parse(dae).getroot()
    base = os.path.dirname(dae)
    images = {}
    for im in root.findall(".//c:library_images/c:image", NS):
        f = im.find("c:init_from", NS).text
        images[im.get("id")] = os.path.join(base, f)
    effects = {}
    for ef in root.findall(".//c:library_effects/c:effect", NS):
        surf = {}
        for p in ef.findall(".//c:newparam", NS):
            s = p.find("c:surface/c:init_from", NS)
            if s is not None: surf[p.get("sid")] = s.text
            sm = p.find("c:sampler2D/c:source", NS)
            if sm is not None: surf[p.get("sid")] = surf.get(sm.text, sm.text)
        tex = ef.find(".//c:diffuse/c:texture", NS)
        img = None
        if tex is not None:
            img = surf.get(tex.get("texture"), tex.get("texture"))
        alpha = ef.find(".//c:transparent/c:color", NS)
        a = 1.0
        if alpha is not None:
            v = floats(alpha.text)
            if len(v) == 4: a = v[3]
        effects[ef.get("id")] = (img, a)
    materials = {}
    for m in root.findall(".//c:library_materials/c:material", NS):
        ie = m.find("c:instance_effect", NS)
        materials[m.get("id")] = effects.get(ie.get("url")[1:], (None, 1.0))
    geoms = {}
    for g in root.findall(".//c:library_geometries/c:geometry", NS):
        mesh = g.find("c:mesh", NS)
        if mesh is None: continue
        src = {}
        for s in mesh.findall("c:source", NS):
            fa = s.find("c:float_array", NS)
            acc = s.find(".//c:accessor", NS)
            stride = int(acc.get("stride")) if acc is not None else 1
            src[s.get("id")] = floats(fa.text).reshape(-1, stride) if fa is not None else None
        verts = {}
        for v in mesh.findall("c:vertices", NS):
            for i in v.findall("c:input", NS):
                if i.get("semantic") == "POSITION": verts[v.get("id")] = i.get("source")[1:]
        parts = []
        for tri in mesh.findall("c:triangles", NS) + mesh.findall("c:polylist", NS):
            ins = tri.findall("c:input", NS)
            n = max(int(i.get("offset")) for i in ins) + 1
            p = np.array([int(x) for x in tri.find("c:p", NS).text.split()], dtype=np.int64).reshape(-1, n)
            if tri.tag.endswith("polylist"):
                vc = [int(x) for x in tri.find("c:vcount", NS).text.split()]
                rows, k = [], 0
                for c in vc:   # fan them into triangles
                    for j in range(1, c - 1): rows += [p[k], p[k + j], p[k + j + 1]]
                    k += c
                p = np.array(rows)
            pos = uv = col = None
            for i in ins:
                sem, off, s = i.get("semantic"), int(i.get("offset")), i.get("source")[1:]
                if sem == "VERTEX": pos = src[verts[s]][p[:, off]]
                elif sem == "TEXCOORD" and i.get("set", "0") == "0": uv = src[s][p[:, off]]
                elif sem == "COLOR": col = src[s][p[:, off]]
            if pos is None: continue
            parts.append((tri.get("material"), pos, uv, col))
        geoms[g.get("id")] = parts
    # the scene: nodes with matrices (nested), each with its geometry and its materials
    out = []
    centres = {}
    def walk(node, M):
        mt = node.find("c:matrix", NS)
        if mt is not None: M = M @ floats(mt.text).reshape(4, 4)
        name = node.get("name") or ""
        if SKIP_NODE.match(name): return
        for ig in node.findall("c:instance_geometry", NS):
            bind = {}
            for im in ig.findall(".//c:instance_material", NS):
                bind[im.get("symbol")] = materials.get(im.get("target")[1:], (None, 1.0))
            for mat, pos, uv, col in geoms.get(ig.get("url")[1:], []):
                P = np.c_[pos, np.ones(len(pos))] @ M.T
                out.append((name, bind.get(mat, (None, 1.0)), P[:, :3], uv, col))
                if ISMAP.match(name): centres[name] = (M[0, 3], M[1, 3])
        for ch in node.findall("c:node", NS): walk(ch, M)
    for n in root.findall(".//c:library_visual_scenes/c:visual_scene/c:node", NS): walk(n, np.eye(4))
    load.centres = centres
    return out, images


def render(dae, png, tilt=1.0, info=None):
    meshes, images = load(dae)
    # the matrix cells: "mapXX_YYc" ground pieces, with their centres
    cells = {}
    for name, _, P, _, _ in meshes:
        m = ISMAP.match(name)
        if m: cells[(int(m.group(1)), int(m.group(2)))] = True
    allp = np.concatenate([P for _, _, P, _, _ in meshes])
    if cells:   # world grid: find the offset from the cells' meshes (each spans 512 units)
        cx0, cy0 = min(c[0] for c in cells), min(c[1] for c in cells)
        cx1, cy1 = max(c[0] for c in cells), max(c[1] for c in cells)
        # model x of the west edge of column cx0, model y of the north edge of row cy0
        # each cell's centre: the middle of its ground (snapped to the 256 grid); the west / north edges from it
        ext = {}
        for n, _, P, _, _ in meshes:
            m = ISMAP.match(n)
            if not m: continue
            k = (int(m.group(1)), int(m.group(2)))
            lo, hi = P[:, :2].min(0), P[:, :2].max(0)
            if k in ext: lo, hi = np.minimum(lo, ext[k][0]), np.maximum(hi, ext[k][1])
            ext[k] = (lo, hi)
        xs, ys = [], []
        for (c, r), (lo, hi) in ext.items():
            xs.append(round((lo[0] + hi[0]) / 2 / 256) * 256 - 256 - (c - cx0) * 512)
            ys.append(round((lo[1] + hi[1]) / 2 / 256) * 256 + 256 + (r - cy0) * 512)
        x0, y1 = float(np.median(xs)), float(np.median(ys))
        wt, ht = (cx1 - cx0 + 1) * CHUNK, (cy1 - cy0 + 1) * CHUNK
        gx0, gy0 = cx0 * CHUNK, cy0 * CHUNK
    else:
        x0, y1 = math.floor(allp[:, 0].min() / TILE) * TILE, math.ceil(allp[:, 1].max() / TILE) * TILE
        wt = int(math.ceil((allp[:, 0].max() - x0) / TILE)); ht = int(math.ceil((y1 - allp[:, 1].min()) / TILE))
        gx0 = gy0 = 0
    W, H = int(wt * TILE), int(ht * TILE)
    pad = 64   # room above the top row for tall things standing on it
    img = np.zeros((H + pad, W, 4), dtype=np.float32)
    zb = np.full((H + pad, W), -1e9, dtype=np.float32)
    top = np.full((ht, wt), -1e9, dtype=np.float32)       # the highest ground per tile
    tex_at = [[None] * wt for _ in range(ht)]               # its texture
    texc = {}
    def tex(path):
        if path not in texc:
            try:
                t = Image.open(path).convert("RGBA")
                texc[path] = np.asarray(t, dtype=np.float32) / 255.0
            except Exception:
                texc[path] = None
        return texc[path]
    def to_screen(P):
        x = P[:, 0] - x0
        sy = (y1 - P[:, 1]) - P[:, 2] * tilt + pad   # south is down; height lifts it up the screen
        depth = -(P[:, 1]) + P[:, 2]                 # nearer the camera: further south, higher up
        return x, sy, depth
    # each building / prop's base: the south edge of its footprint and the ground it stands on (shadows aside)
    nbase = {}
    for name, (imgid, alpha), P, uv, col in meshes:
        if ISMAP.match(name) or "kage" in (imgid or ""): continue
        lo = (P[:, 1].min(), P[:, 2].min())
        if name in nbase: lo = (min(lo[0], nbase[name][0]), min(lo[1], nbase[name][1]))
        nbase[name] = lo
    over = np.zeros((H + pad, W), dtype=bool)   # the pixel belongs to something tall that people walk behind
    order = []
    for name, (imgid, alpha), P, uv, col in meshes:
        if name.startswith("wind"): continue   # the drifting cloud shadows
        if alpha <= 0.05: alpha = 1.0           # (the export's "0" means the texture's own alpha decides)
        order.append((name, imgid, alpha, P, uv, col))
    order.sort(key=lambda o: o[2] < 0.99)       # see-through things (shadows, glass) after everything solid
    for name, imgid, alpha, P, uv, col in order:
        T = tex(images.get(imgid)) if imgid else None
        sx, sy, dp = to_screen(P)
        n = len(P) // 3
        for k in range(n):
            a, b, c = 3 * k, 3 * k + 1, 3 * k + 2
            xs, ys = np.array([sx[a], sx[b], sx[c]]), np.array([sy[a], sy[b], sy[c]])
            minx, maxx = int(max(0, math.floor(xs.min()))), int(min(W - 1, math.ceil(xs.max())))
            miny, maxy = int(max(0, math.floor(ys.min()))), int(min(H + pad - 1, math.ceil(ys.max())))
            if minx > maxx or miny > maxy: continue
            den = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
            if abs(den) < 1e-9: continue
            gx, gy = np.meshgrid(np.arange(minx, maxx + 1) + 0.5, np.arange(miny, maxy + 1) + 0.5)
            w0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / den
            w1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / den
            w2 = 1 - w0 - w1
            inside = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
            if not inside.any(): continue
            d = w0 * dp[a] + w1 * dp[b] + w2 * dp[c]
            ismap = ISMAP.match(name) is not None
            if ismap:   # a tree, a fence, a cliff face: its own south edge, standing on its own lowest point
                tri = P[[a, b, c]]
                upright = tri[:, 2].max() - tri[:, 2].min() > 6
                by, bz = tri[:, 1].min(), tri[:, 2].min()
            else:
                upright = True
                by, bz = nbase.get(name, (P[:, 1].min(), P[:, 2].min()))
            base_sy = (y1 - by) - bz * tilt + pad
            if T is not None and uv is not None:
                u = w0 * uv[a, 0] + w1 * uv[b, 0] + w2 * uv[c, 0]
                v = w0 * uv[a, 1] + w1 * uv[b, 1] + w2 * uv[c, 1]
                th, tw = T.shape[:2]
                ti = ((np.floor((1 - v) * th)).astype(int)) % th
                tj = ((np.floor(u * tw)).astype(int)) % tw
                C = T[ti, tj]
            else:
                C = np.ones(gx.shape + (4,), dtype=np.float32) * np.array([0.6, 0.6, 0.6, 1.0])
            if col is not None and col[[a, b, c], :3].max() > 0.05:   # (all-black vertex colours: unlit billboards, ignore)
                cc = w0[..., None] * col[a, :3] + w1[..., None] * col[b, :3] + w2[..., None] * col[c, :3]
                C = C.copy(); C[..., :3] *= cc
            ok = inside & (C[..., 3] > 0.5)
            yy, xx = gy[ok].astype(int), gx[ok].astype(int)
            dd = d[ok]
            near = dd >= zb[yy, xx] - 1e-3
            yy, xx, dd = yy[near], xx[near], dd[near]
            Cn = C[ok][near]
            if alpha >= 0.99:
                zb[yy, xx] = dd
                over[yy, xx] = upright & (yy < base_sy - 24)
            if alpha < 0.99:   # see-through (window glass, water sheen): blend
                img[yy, xx, :3] = img[yy, xx, :3] * (1 - alpha) + Cn[:, :3] * alpha
                img[yy, xx, 3] = 1
            else:
                img[yy, xx] = np.c_[Cn[:, :3], np.ones(len(Cn))]
            # the ground: the top surface over each tile (flat-ish triangles only)
            if info is not None and ISMAP.match(name):
                nrm = np.cross(P[b] - P[a], P[c] - P[a])
                if abs(nrm[2]) > 0.7 * np.linalg.norm(nrm):
                    # every tile this triangle covers
                    ex = [(p[0] - x0) / TILE for p in (P[a], P[b], P[c])]
                    ey = [(y1 - p[1]) / TILE for p in (P[a], P[b], P[c])]
                    for ty2 in range(max(0, int(min(ey))), min(ht, int(math.ceil(max(ey))))):
                        for tx2 in range(max(0, int(min(ex))), min(wt, int(math.ceil(max(ex))))):
                            px, py = x0 + (tx2 + 0.5) * TILE, y1 - (ty2 + 0.5) * TILE
                            # inside the triangle (in plan)?
                            A, B, Cc = P[a][:2], P[b][:2], P[c][:2]
                            dd2 = (B[1] - Cc[1]) * (A[0] - Cc[0]) + (Cc[0] - B[0]) * (A[1] - Cc[1])
                            if abs(dd2) < 1e-9: continue
                            l0 = ((B[1] - Cc[1]) * (px - Cc[0]) + (Cc[0] - B[0]) * (py - Cc[1])) / dd2
                            l1 = ((Cc[1] - A[1]) * (px - Cc[0]) + (A[0] - Cc[0]) * (py - Cc[1])) / dd2
                            if l0 < -0.01 or l1 < -0.01 or l0 + l1 > 1.01: continue
                            z = l0 * P[a][2] + l1 * P[b][2] + (1 - l0 - l1) * P[c][2]
                            if z > top[ty2, tx2]:
                                top[ty2, tx2] = z
                                tex_at[ty2][tx2] = os.path.basename(images.get(imgid, "") or "")
    out = (np.clip(img, 0, 1) * 255).astype(np.uint8)
    Image.fromarray(out, "RGBA").save(png, optimize=True)
    ov = out.copy()
    ov[~over] = 0
    Image.fromarray(ov, "RGBA").save(png[:-4] + "_over.png", optimize=True)
    if info is not None:
        grid = build_grid(meshes, images, top, tex_at, x0, y1, wt, ht)
        json.dump({"gx0": gx0, "gy0": gy0, "w": wt, "h": ht, "pad": pad, "grid": grid, "x0": float(x0), "y1": float(y1), "v": 2,
                   "height": [[None if v < -1e8 else round(float(v), 1) for v in row] for row in top.tolist()],
                   "tex": tex_at}, open(info, "w"))
    return gx0, gy0, wt, ht


WATER_TEX = ("sea", "pond", "water", "river", "umi", "ike", "mizu", "taki", "lake")
def build_grid(meshes, images, top, tex_at, x0, y1, wt, ht):
    """The walking rules per tile: '#' solid, '.' walkable, 'g' tall grass, 'w' water, 'v' '<' '>' ledges."""
    g = [["." for _ in range(wt)] for _ in range(ht)]
    tile = lambda x, y: (int((x - x0) // TILE), int((y1 - y) // TILE))
    base = lambda n: os.path.basename(images.get(n, "") or "") if n else ""
    for ty in range(ht):
        for tx in range(wt):
            t = tex_at[ty][tx] or ""
            if top[ty, tx] < -1e8: g[ty][tx] = "#"
            elif "tree" in t: g[ty][tx] = "#"                       # (the ground under a tree)
            elif any(w in t for w in WATER_TEX) and "line" not in t: g[ty][tx] = "w"
            elif "egrass" in t: g[ty][tx] = "g"                     # encounter grass
            elif "slope" in t or "stairhole" in t: g[ty][tx] = "s"   # a slope / steps: walk up and down it
    nodes = {}
    for name, (imgid, alpha), P, uv, col in meshes:
        t = base(imgid)
        if name.startswith("wind") or "kage" in t: continue
        if ISMAP.match(name):
            for k in range(0, len(P) - 2, 3):
                tri = P[k:k + 3]
                if "allpeak" in t and uv is not None:   # a ledge: you hop the way the painted lip faces
                    A = np.array([[tri[1][0] - tri[0][0], tri[2][0] - tri[0][0]], [tri[1][1] - tri[0][1], tri[2][1] - tri[0][1]]])
                    U = np.array([[uv[k + 1][0] - uv[k][0], uv[k + 2][0] - uv[k][0]], [uv[k + 1][1] - uv[k][1], uv[k + 2][1] - uv[k][1]]])
                    if abs(np.linalg.det(U)) < 1e-9: continue
                    d = -(A @ np.linalg.inv(U))[:, 1]
                    ch = "v" if abs(d[1]) >= abs(d[0]) and d[1] < 0 else ">" if abs(d[0]) > abs(d[1]) and d[0] > 0 else "<" if abs(d[0]) > abs(d[1]) else "#"
                    cx, cy = tri[:, 0].mean(), tri[:, 1].mean()
                    tx, ty = tile(cx, cy)
                    if 0 <= tx < wt and 0 <= ty < ht and g[ty][tx] != "#": g[ty][tx] = ch
                    continue
                if tri[:, 2].max() - tri[:, 2].min() <= 6: continue          # flat
                if "tree" in t: continue   # (a tree's own triangles: the ground under it says where it stands)
                # standing up (a fence, a tree, a post): the tiles along its foot - not a cliff's edge between two tiles
                lo = np.argsort(tri[:, 2])[:2]
                pa, pb = tri[lo[0]], tri[lo[1]]
                if pb[2] - pa[2] > 6: pb = pa
                for u in np.linspace(0, 1, 9):
                    px, py = pa[0] + (pb[0] - pa[0]) * u, pa[1] + (pb[1] - pa[1]) * u
                    fx, fy = (px - x0) / TILE, (y1 - py) / TILE
                    along_y = abs(pb[1] - pa[1]) < 1e-3   # an east-west edge: it sits on a tile's row line?
                    if along_y and abs(fy - round(fy)) < 0.15: continue
                    if not along_y and abs(pb[0] - pa[0]) < 1e-3 and abs(fx - round(fx)) < 0.15: continue
                    tx, ty = int(fx), int(fy)
                    if 0 <= tx < wt and 0 <= ty < ht and g[ty][tx] in ".g": g[ty][tx] = "#"
        else:
            nodes.setdefault(name, []).append(P)
    for name, Ps in nodes.items():   # buildings, signs, lamps: solid where their walls / posts meet the ground
        P = np.concatenate(Ps)            # (an archway or a gate you walk through only stands on its pillars)
        z0 = P[:, 2].min()
        marked = 0
        for k in range(0, len(P) - 2, 3):
            tri = P[k:k + 3]
            if tri[:, 2].max() - tri[:, 2].min() <= 2:
                continue
            foot = tri[tri[:, 2] < z0 + 3]
            if len(foot) == 0:
                continue
            pa, pb = foot[0], foot[-1]
            for u in np.linspace(0, 1, 9):
                px, py = pa[0] + (pb[0] - pa[0]) * u, pa[1] + (pb[1] - pa[1]) * u
                tx, ty = tile(px, py)
                if 0 <= tx < wt and 0 <= ty < ht:
                    g[ty][tx] = "#"; marked += 1
        if marked == 0:   # (nothing reaching the ground: the old way, its whole footprint)
            P = P[P[:, 2] > P[:, 2].min() + 1.5] if (P[:, 2] > P[:, 2].min() + 1.5).any() else P
            ax, bx = P[:, 0].min() + 2, P[:, 0].max() - 2
            ay, by = P[:, 1].min() + 2, P[:, 1].max() - 2
            if bx <= ax: ax = bx = (ax + bx) / 2
            if by <= ay: ay = by = (ay + by) / 2
            tx0, ty1 = tile(ax, ay)
            tx1, ty0 = tile(bx, by)
            for ty in range(max(0, ty0), min(ht, ty1 + 1)):
                for tx in range(max(0, tx0), min(wt, tx1 + 1)):
                    g[ty][tx] = "#"
    return ["".join(r) for r in g]


def regrid(dae, info_path):
    """the walking grid again, from a render's saved heights and textures (no drawing)"""
    meshes, images = load(dae)
    d = json.load(open(info_path))
    top = np.array([[-1e9 if v is None else v for v in row] for row in d["height"]], dtype=np.float32)
    # the same origin the render used
    x0 = d.get("x0"); y1 = d.get("y1")
    if x0 is None:
        return d
    d["grid"] = build_grid(meshes, images, top, d["tex"], x0, y1, d["w"], d["h"])
    d["v"] = 2
    json.dump(d, open(info_path, "w"))
    return d


if __name__ == "__main__":
    args = sys.argv[1:]
    tilt = 1.0
    info = None
    if "--tilt" in args: i = args.index("--tilt"); tilt = float(args[i + 1]); del args[i:i + 2]
    if "--info" in args: i = args.index("--info"); info = args[i + 1]; del args[i:i + 2]
    print(render(args[0], args[1], tilt, info))
