# Mega Slowbro (sketch), drawn from scratch to match the user's own pixel chart (charts/Mega Slowbro.png):
# the user's Slowbro head in the chart's colours sitting in a grey-green spiral Shellder shell (a wide
# toothed rim tapering to a point), pink arms out the sides, pink tail tip at the bottom.
# The portrait is the user's chart itself.
# Build: python3 m_slowb.py   -> out/Mega Slowbro/, p_m_slowb.png, g_slowb.png
import os
from PIL import Image, ImageOps
NAME='Mega Slowbro'; BASE='Slowbro'; DEX='080'; PID=10071
POK='/home/claude/arena-clash/Pokemon/Slowbro/'
HERE=os.path.dirname(os.path.abspath(__file__))
CHART=os.path.join(HERE,'charts','Mega Slowbro.png')
def hx(s): s=s.lstrip('#'); return tuple(int(s[i:i+2],16) for i in (0,2,4))+(255,)
# the chart's palette (+ a darker yellow for muzzle shading)
PAL={'A':'#333232','B':'#cd877d','C':'#feada4','D':'#525752','E':'#fefefe','F':'#f6e69b','G':'#90937a','H':'#bccaba','f':'#d4c27a'}
C={k:hx(v) for k,v in PAL.items()}
T0=(0,0,0,0)
# base Slowbro colours -> chart colours (nearest match)
HEADMAP=[('#efa9b3','C'),('#f0a8b3','C'),('#cf7781','B'),('#d27882','B'),('#b15f6b','B'),('#b16069','B'),('#702f37','A'),('#6f3037','A'),
         ('#0a0a0a','A'),('#e8e9f6','E'),('#d7c159','f'),('#dbc158','f'),('#eef1a7','F'),('#c2c8cd','H'),('#8f763a','A'),('#d86f63','B'),
         ('#cd7467','B'),('#494f58','D'),('#999999','G'),('#cd5852','B'),('#907837','A')]

def blank(w,h): return Image.new('RGBA',(w,h),T0)
def put(im,x,y,col):
    if 0<=x<im.width and 0<=y<im.height: im.putpixel((x,y),C[col])
def rows(im,x0,y0,txt):
    for dy,row in enumerate(txt.strip('\n').split('\n')):
        for dx,ch in enumerate(row):
            if ch in '. ': continue
            put(im,x0+dx,y0+dy,ch)
def recolour(im):
    im=im.convert('RGBA'); px=im.load()
    refs=[(hx(a),C[b]) for a,b in HEADMAP]
    for y in range(im.height):
        for x in range(im.width):
            c=px[x,y]
            if c[3]<128: px[x,y]=T0; continue
            best=min(refs,key=lambda r:sum((c[i]-r[0][i])**2 for i in range(3)))
            px[x,y]=best[1]
    return im
def head(path, box):
    return recolour(Image.open(path).crop(box))

# The shell, shared by every view: the rim lip and a cone of spiral bands tapering to the tail tip.
CONE="""
AHHHHHHHHHHHHHHHHHHHHHGA
.AGHHHHHHHHHHHHHHHHHHGA.
..AGDDDDDDDDDHHHHHHHGA..
..AHHHHHHHDDDDDDDDDDGA..
...AHHHHHHHHHHHHHHHGA...
...AGDDDDDDDHHHHHHGGA...
....AHHHHHHDDDDDDDGA....
....AGHHHHHHHHHHHGGA....
.....AGDDDDDHHHHGGA.....
......AGHHHDDDDDGA......
.......AGHHHHHGGA.......
........AADDDDAA........
..........ACBA..........
..........ABFA..........
...........AA...........
"""

def front(frame):
    im=blank(28,29)
    hd=head(POK+f'080_1_mae_{frame}.png', (2,0,16,12) if frame==1 else (3,0,17,12))
    im.alpha_composite(hd,(7,0))
    # rim sides rising beside the head, the dark inside of the shell, and Shellder's teeth
    rows(im,2,8,"""
..A..................A..
.AHA................AGA.
.AHHA..............AHGA.
.AHHDEDDEDDEDDEDDEDDHGA.
AHHHDEEDEEDEEDEEDEEDHHGA
""")
    rows(im,2,13,CONE)
    l = 0 if frame==1 else 1     # the arms swing a little between frames
    rows(im,0,10+l,"""
.AAA.
ACCBA
AECBA
.AAA.
""")
    rows(im,23,11+(1-l),"""
.AAA.
ABCCA
ABCEA
.AAA.
""")
    return im

def side(frame):
    im=blank(28,29)
    hd=head(POK+f'080_2_hidari_{frame}.png', (0,0,16,13))
    im.alpha_composite(hd,(2,0))
    # seen from the side: the rim's back edge rises behind the head, teeth along the front edge
    rows(im,2,7,"""
...................AA...
..................AHGA..
.................AHHGA..
................AHHHGA..
.AEDDEDDEDDEDDAAHHHHGA..
AHEEDEEDEEDEEDHHHHHHHGA.
""")
    rows(im,2,13,CONE)
    l = 0 if frame==1 else 1
    rows(im,0,10+l,"""
.AAA.
ACCBA
AECBA
.AAA.
""")
    return im

def back(frame):
    im=blank(28,29)
    # the back of the head: the front head's round shape with the face painted over in pink
    hd=head(POK+f'080_1_mae_{frame}.png', (2,0,16,12) if frame==1 else (3,0,17,12))
    p=hd.load(); W,H=hd.size
    def solid(x,y): return 0<=x<W and 0<=y<H and p[x,y][3]>0
    for y in range(H):
        for x in range(W):
            if not p[x,y][3]: continue
            edge=not all(solid(x+dx,y+dy) for dx,dy in((1,0),(-1,0),(0,1),(0,-1)))
            if not edge: p[x,y]=C['B'] if y>=8 or x>=W-3 or (y==2 and (x<=2 or x>=W-3)) else C['C']
    im.alpha_composite(hd,(7,0))
    rows(im,2,8,"""
..A..................A..
.AHA................AGA.
.AHHA..............AHGA.
.AHHHAAAAAAAAAAAAAAAHHGA.
AHHHHHHHHHHHHHHHHHHHHHGA
""")
    rows(im,2,13,CONE)
    l = 0 if frame==1 else 1
    rows(im,0,10+l,"""
.AAA.
ABCCA
ABCBA
.AAA.
""")
    rows(im,23,10+(1-l),"""
.AAA.
ACCBA
ABCBA
.AAA.
""")
    return im

def portrait():
    return Image.open(CHART).convert('RGBA')

def build():
    out=os.path.join(HERE,'out',NAME); os.makedirs(out,exist_ok=True)
    res={}
    for f in (1,2):
        res[f'front{f}']=front(f); res[f'left{f}']=side(f); res[f'back{f}']=back(f)
    res['portrait']=portrait()
    names={'front1':'080_1_mae_1','front2':'080_1_mae_2','left1':'080_2_hidari_1','left2':'080_2_hidari_2','back1':'080_4_ushiro_1','back2':'080_4_ushiro_2'}
    for k,n in names.items(): res[k].save(f'{out}/{n}.png')
    for f in (1,2): ImageOps.mirror(res[f'left{f}']).save(f'{out}/080_3_migi_{f}.png')
    res['portrait'].save(f'{out}/Pokemon-080-Mega-Slowbro.png')
    return res

def preview(res, path, S=8):
    basef=[POK+n for n in ('080_1_mae_1.png','080_1_mae_2.png','080_2_hidari_1.png','080_2_hidari_2.png','080_4_ushiro_1.png','080_4_ushiro_2.png','Pokemon-080-Slowbro.png')]
    order=['front1','front2','left1','left2','back1','back2','portrait']
    rws=[[Image.open(f).convert('RGBA') for f in basef],[res[k] for k in order]]
    cw=max(i.width for r in rws for i in r)*S+10
    rh=[max(i.height for i in r)*S+10 for r in rws]
    c=Image.new('RGBA',(cw*7+10,sum(rh)+10),(70,74,92,255))
    y=5
    for ri,r in enumerate(rws):
        for ci,i in enumerate(r): c.alpha_composite(i.resize((i.width*S,i.height*S),Image.NEAREST),(10+ci*cw,y))
        y+=rh[ri]
    c.save(path)

if __name__=='__main__':
    r=build(); preview(r, os.path.join(HERE,'p_m_slowb.png'))
    order=['front1','front2','left1','back1','portrait']
    g=Image.new('RGBA',(560,120),(64,120,72,255)); x=5
    for k in order: g.alpha_composite(r[k],(x,5)); x+=r[k].width+6
    x=5
    for k in order: g.alpha_composite(r[k].resize((r[k].width*3,r[k].height*3),Image.NEAREST),(x,36)); x+=r[k].width*3+6
    g.save(os.path.join(HERE,'g_slowb.png'))
