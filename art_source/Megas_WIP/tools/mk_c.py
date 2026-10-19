# Mega sprite maker: recolour a base Pokemon's sprites by colour roles, then apply pixel patches.
import os, glob, sys, importlib
from PIL import Image, ImageOps
POK='/home/claude/arena-clash/Pokemon'
OUT='/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/mega/out'
def hx(s): s=s.lstrip('#'); return tuple(int(s[i:i+2],16) for i in (0,2,4))
def files(base):
    fs=sorted(glob.glob(f'{POK}/{base}/*.png'))
    d={}
    for f in fs:
        n=os.path.basename(f)
        if n.startswith('Pokemon-'): d['portrait']=f
        elif '_1_' in n: d['front'+n[-5]]=f
        elif '_2_' in n: d['left'+n[-5]]=f
        elif '_4_' in n: d['back'+n[-5]]=f
    return d
def recolour(im, rules, thresh=60):
    im=im.convert('RGBA'); px=im.load()
    refs=[(hx(a),hx(b) if b else None) for a,b in rules]
    for y in range(im.height):
        for x in range(im.width):
            c=px[x,y]
            if c[3]<128: continue
            best=None;bd=1e9
            for a,b in refs:
                dd=sum((c[i]-a[i])**2 for i in range(3))
                if dd<bd: bd=dd;best=(a,b)
            if bd<=thresh*thresh and best[1] is not None:
                px[x,y]=best[1]+(255,)
    return im
def pad(im, l=0,t=0,r=0,b=0):
    n=Image.new('RGBA',(im.width+l+r,im.height+t+b),(0,0,0,0)); n.paste(im,(l,t)); return n
UNDER=''
def patch(im, x0, y0, rows, legend):
    px=im.load()
    for dy,row in enumerate(rows):
        for dx,ch in enumerate(row):
            if ch in '. ': continue
            x,y=x0+dx,y0+dy
            if not(0<=x<im.width and 0<=y<im.height): continue
            if ch in UNDER and px[x,y][3]>=128: continue   # 'under' colours only fill empty pixels
            if ch=='_': px[x,y]=(0,0,0,0)
            else: px[x,y]=hx(legend[ch])+(255,)
    return im
def build(spec):
    global UNDER
    UNDER=getattr(spec,'UNDER','')
    d=files(spec.BASE)
    res={}
    for k,f in d.items():
        im=recolour(Image.open(f), spec.RULES + getattr(spec,'RULES_'+k.rstrip('12'),[]), getattr(spec,'THRESH',60))
        for (rk,x0,y0,x1,y1,rr) in getattr(spec,'REGION',[]):   # recolour only inside a box
            if rk!=k and rk!=k.rstrip('12'): continue
            sub=recolour(im.crop((x0,y0,x1,y1)), rr, getattr(spec,'THRESH',60))
            im.paste(sub,(x0,y0))
        p=getattr(spec,'PAD',{}).get(k) or getattr(spec,'PAD',{}).get(k.rstrip('12'))
        if p: im=pad(im,*p)
        for (x,y,rows) in spec.PATCH.get(k,[]):
            im=patch(im,x,y,rows,spec.LEG)
        res[k]=im
    os.makedirs(f'{OUT}/{spec.NAME}',exist_ok=True)
    dex=spec.DEX
    names={'front1':f'{dex}_1_mae_1','front2':f'{dex}_1_mae_2','left1':f'{dex}_2_hidari_1','left2':f'{dex}_2_hidari_2',
           'back1':f'{dex}_4_ushiro_1','back2':f'{dex}_4_ushiro_2'}
    for k,n in names.items(): res[k].save(f'{OUT}/{spec.NAME}/{n}.png')
    for k,n in [('left1',f'{dex}_3_migi_1'),('left2',f'{dex}_3_migi_2')]: ImageOps.mirror(res[k]).save(f'{OUT}/{spec.NAME}/{n}.png')
    res['portrait'].save(f'{OUT}/{spec.NAME}/Pokemon-{dex}-{spec.NAME.replace(" ","-")}.png')
    return res
def preview(spec, res, out, S=8):
    from PIL import ImageDraw
    SP='/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad'
    d=files(spec.BASE)
    order=['front1','front2','left1','left2','back1','back2','portrait']
    rows=[[Image.open(d[k]).convert('RGBA') for k in order],[res[k] for k in order]]
    refs=[Image.open(f'{SP}/pas/sprites/pokemon/{spec.PID}.png').convert('RGBA'),Image.open(f'{SP}/pas/sprites/pokemon/back/{spec.PID}.png').convert('RGBA')]
    refs=[r.crop(r.getbbox()) for r in refs]
    cw=max(i.width for r in rows for i in r)*S+10
    rh=max(i.height for r in rows for i in r)*S+10
    W=cw*7+sum(r.width*3+10 for r in refs)+10
    H=max(rh*2, max(r.height*3 for r in refs))+10
    c=Image.new('RGBA',(W,H),(70,74,92,255))
    for ri,r in enumerate(rows):
        for ci,i in enumerate(r):
            c.alpha_composite(i.resize((i.width*S,i.height*S),Image.NEAREST),(10+ci*cw,5+ri*rh))
    x=10+cw*7
    for r in refs: c.alpha_composite(r.resize((r.width*3,r.height*3),Image.NEAREST),(x,5)); x+=r.width*3+10
    c.save(out)
if __name__=='__main__':
    m=importlib.import_module(sys.argv[1])
    r=build(m); preview(m,r,f'p_{sys.argv[1]}.png', int(sys.argv[2]) if len(sys.argv)>2 else 8)
