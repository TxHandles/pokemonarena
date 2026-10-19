import sys
from PIL import Image
LET='.ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'
def dump(path, pal=None):
    im=Image.open(path).convert('RGBA'); W,H=im.size; px=im.load()
    if pal is None: pal={}
    rows=[]
    for y in range(H):
        r=''
        for x in range(W):
            c=px[x,y]
            if c[3]<128: r+='.'; continue
            c=c[:3]
            if c not in pal: pal[c]=LET[len(pal)+1]
            r+=pal[c]
        rows.append(r)
    return rows,pal
if __name__=='__main__':
    pal={}
    for p in sys.argv[1:]:
        rows,pal=dump(p,pal)
        print(p.split('/')[-1], len(rows[0]),'x',len(rows))
        print('   '+''.join(str(i%10) for i in range(len(rows[0]))))
        for i,r in enumerate(rows): print(f'{i:2d} '+r)
    print({v:'#%02x%02x%02x'%k for k,v in pal.items()})
