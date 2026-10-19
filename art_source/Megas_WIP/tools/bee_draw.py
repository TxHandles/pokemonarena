# Mega Beedrill walking frames, drawn part by part (back to front); each part gets its own dark outline.
from PIL import Image, ImageDraw
def hx(s): s=s.lstrip('#'); return tuple(int(s[i:i+2],16) for i in (0,2,4))+(255,)
K=hx('#0a0a0a'); Y=hx('#f8c030'); G=hx('#bc992a'); B=hx('#2e2a26'); W=hx('#fdfdfe'); L=hx('#a7b4a7'); l=hx('#d9dfc6')
S=hx('#eceef4'); s=hx('#a4a8b8'); R=hx('#e0302c'); r=hx('#8a1616'); P=hx('#ffd9d0')
class Part:
    def __init__(p,w,h): p.im=Image.new('RGBA',(w,h),(0,0,0,0)); p.d=ImageDraw.Draw(p.im)
def outline(im, col=K):
    px=im.load(); w,h=im.size; out=im.copy(); o=out.load()
    for y in range(h):
        for x in range(w):
            if px[x,y][3]: continue
            for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
                X,Y_=x+dx,y+dy
                if 0<=X<w and 0<=Y_<h and px[X,Y_][3]: o[x,y]=col; break
    return out
def shade_right(p, n=1):
    im=p.im; w,h=im.size
    for y in range(h):
        xs=[x for x in range(w) if im.getpixel((x,y))[3] and im.getpixel((x,y))==Y]
        for x in sorted(xs)[-n:]: im.putpixel((x,y),G)
def comp(w,h,parts):
    c=Image.new('RGBA',(w,h),(0,0,0,0))
    for p in parts: c.alpha_composite(outline(p.im))
    return c
def ant(im,pts):
    for p in pts:
        if not im.getpixel(p)[3]: im.putpixel(p,K)
    return im
def P_(w,h): return Part(w,h)
def lance(w,h,base,tip,wd=2):
    """a long silver lance from base (arm) to tip, wide at the base"""
    p=P_(w,h); (x0,y0),(x1,y1)=base,tip
    import math
    dx,dy=x1-x0,y1-y0; n=math.hypot(dx,dy); nx,ny=-dy/n,dx/n
    p.d.polygon([(x0+nx*wd,y0+ny*wd),(x0-nx*wd,y0-ny*wd),(x1,y1)],fill=S)
    p.d.line([(x0+nx*wd*0.6,y0+ny*wd*0.6),(x1,y1)],fill=s)      # shaded edge
    return p
def abdomen(w,h,cx,top,bot,rx,tilt=0):
    p=P_(w,h)
    p.d.ellipse([cx-rx,top,cx+rx,bot],fill=Y)
    for y in range(top+2,bot+1,3):                 # black bands
        for x in range(w):
            if p.im.getpixel((x,y))[3] and p.im.getpixel((x,min(h-1,y)))[3]: p.im.putpixel((x,y),B)
    # stinger
    sx=cx+tilt; p.d.polygon([(sx-1.5,bot-1),(sx+1.5,bot-1),(sx+tilt,bot+5)],fill=S)
    p.d.line([(sx+0.5,bot),(sx+tilt,bot+4)],fill=s)
    return p
def wing(w,h,pts,shade):
    p=P_(w,h); p.d.polygon(pts,fill=W); p.d.line(shade,fill=l); return p
def front(frame):
    w,h=27,28; cx=13; up=0 if frame==1 else 1
    parts=[]
    # wings: four, raised up and out behind the shoulders
    for sgn in (-1,1):
        X=lambda x: cx+sgn*x
        parts.append(wing(w,h,[(X(2),9-up),(X(9),0+up),(X(11),2+up),(X(5),10-up)],[(X(3),9-up),(X(9),2+up)]))
        parts.append(wing(w,h,[(X(3),10),(X(12),6+up),(X(12),8+up),(X(5),12)],[(X(4),11),(X(11),7+up)]))
    # abdomen hanging down
    parts.append(abdomen(w,h,cx,12,21,3))
    # thorax
    th=P_(w,h); th.d.ellipse([cx-3,8,cx+3,13],fill=Y); th.d.line([(cx-2,12),(cx+2,12)],fill=G); parts.append(th)
    # lances: from the shoulders, long and pointing down and out
    a=1 if frame==1 else 0
    parts.append(lance(w,h,(cx-4,10),(1,24-a)))
    parts.append(lance(w,h,(cx+4,10),(w-2,24-a)))
    # head with big red eyes and antennae
    hd=P_(w,h); hd.d.ellipse([cx-3,3,cx+3,9],fill=Y); shade_right(hd)
    for ex in (cx-3,cx+2):
        hd.d.rectangle([ex,5,ex+1,6],fill=R); hd.im.putpixel((ex,6) if ex<cx else (ex+1,6),r)
    hd.im.putpixel((cx,8),G)
    parts.append(hd)
    return ant(comp(w,h,parts),[(cx-2,2),(cx-3,1),(cx-4,0),(cx+2,2),(cx+3,1),(cx+4,0)])
def back(frame):
    w,h=27,28; cx=13; up=0 if frame==1 else 1; parts=[]
    a=1 if frame==1 else 0
    parts.append(lance(w,h,(cx-4,10),(1,24-a))); parts.append(lance(w,h,(cx+4,10),(w-2,24-a)))
    parts.append(abdomen(w,h,cx,11,21,3))
    th=P_(w,h); th.d.ellipse([cx-3,8,cx+3,13],fill=Y); th.d.line([(cx,9),(cx,12)],fill=G); parts.append(th)
    hd=P_(w,h); hd.d.ellipse([cx-3,3,cx+3,9],fill=Y); shade_right(hd); parts.append(hd)
    for sgn in (-1,1):   # wings in front (we see its back)
        X=lambda x: cx+sgn*x
        parts.append(wing(w,h,[(X(1),9-up),(X(8),0+up),(X(10),2+up),(X(4),10-up)],[(X(2),9-up),(X(8),2+up)]))
        parts.append(wing(w,h,[(X(2),10),(X(11),6+up),(X(11),8+up),(X(4),12)],[(X(3),11),(X(10),7+up)]))
    return ant(comp(w,h,parts),[(cx-2,2),(cx-3,1),(cx-4,0),(cx+2,2),(cx+3,1),(cx+4,0)])
def left(frame):
    """facing left: head at the left, abdomen angled down and back, one lance forward, one back"""
    w,h=24,27; up=0 if frame==1 else 1; parts=[]
    # far wings (behind)
    parts.append(wing(w,h,[(11,8),(17,0+up),(19,1+up),(14,9)],[(12,8),(17,2+up)]))
    # far lance pointing back-down
    parts.append(lance(w,h,(12,11),(21,22-up)))
    # abdomen angled back-down
    ab=abdomen(w,h,14,12,21,3,tilt=1); parts.append(ab)
    th=P_(w,h); th.d.ellipse([8,7,14,13],fill=Y); th.d.line([(9,12),(13,12)],fill=G); parts.append(th)
    hd=P_(w,h); hd.d.ellipse([3,3,9,9],fill=Y); shade_right(hd); hd.d.rectangle([3,5,5,7],fill=R); hd.im.putpixel((3,7),r); hd.im.putpixel((4,7),r); parts.append(hd)
    # near wing (in front, raised)
    parts.append(wing(w,h,[(10,9),(18,2+up),(21,4+up),(13,11)],[(11,10),(19,4+up)]))
    # near lance pointing forward-down
    parts.append(lance(w,h,(9,11),(0,21-up)))
    return ant(comp(w,h,parts),[(5,2),(4,1),(3,0),(7,2),(8,1),(8,0)])
