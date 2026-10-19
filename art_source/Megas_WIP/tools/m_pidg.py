from PIL import Image
NAME='Mega Pidgeot'; BASE='Pidgeot'; DEX='018'; PID=10073
RED='#dd4272'; DRED='#a3454a'; BLUE='#00a6d8'; DBLUE='#00709a'   # from the user's Mega Pidgeot chart
RULES_portrait=[('#ff6241',RED),('#9c4141',DRED),('#fff67b','#f4e387'),('#c5ac5a','#e8a030')]
RULES=[
 ('#e68900',RED), ('#e88800',RED), ('#ca481f',DRED), ('#c84820',DRED), ('#c7491f',DRED), ('#c94923',DRED),   # crest orange -> red
 ('#dd7070','#e8608c'), ('#e07070','#e8608c'), ('#e0706f','#e8608c'),        # crest streaks / tail feathers
 ('#fbc030','#f4e387'), ('#f8c030','#f4e387'), ('#f6c12f','#f4e387'), ('#f3c137','#f4e387'),
]
THRESH=12
LEG={'r':RED,'d':DRED,'b':BLUE,'B':DBLUE,'y':'#f4e387','o':'#3a1a10'}
UNDER='o'
PAD={'left':(0,0,12,0),'back':(0,0,0,4),'portrait':(0,1,3,0)}
def pts(*p): return [(x,y,[c]) for x,y,c in p]
def rib(lst):   # a ribbon two pixels thick: red on top, dark red under it
    out=[(x,y,'r') for x,y,c in lst]
    have={(x,y) for x,y,c in lst}
    out+=[(x,y+1,'d') for x,y,c in lst if (x,y+1) not in have]
    return out
def ribo(lst):   # the ribbon with a dark outline (outline only fills empty pixels)
    out=rib(lst)
    have={(x,y) for x,y,c in out}
    for x,y,c in list(out):
        for dx,dy in ((0,-1),(0,1),(-1,0),(1,0)):
            if (x+dx,y+dy) not in have: out.append((x+dx,y+dy,'o')); have.add((x+dx,y+dy))
    return out
def mir(lst,w): return [(w-1-x,y,c) for x,y,c in lst]
F1=[(2,4,'b'),(2,5,'b'),(2,6,'B'),(3,6,'b'),(3,7,'b'),(4,7,'B')]
F1R=[(15,1,'r'),(16,1,'r'),(17,2,'r'),(18,3,'r'),(19,4,'r'),(19,5,'r'),(20,6,'r'),(20,7,'r'),(21,8,'r'),(21,9,'r'),(11,1,'r'),(10,2,'r'),(9,3,'r'),(8,4,'r'),(8,5,'r'),(7,6,'r')]
F2=[(1,25,'b'),(2,25,'b'),(1,26,'B'),(2,26,'b'),(2,27,'b'),(2,28,'B')]
F2R=[(11,0,'r'),(12,1,'r'),(13,2,'r'),(14,3,'d'),(14,4,'r'),(15,5,'r'),(15,6,'d'),(16,7,'r'),(16,8,'r'),(17,9,'d')]
LR=[(19,0,'r'),(20,0,'r'),(21,0,'r'),(22,0,'r'),(23,1,'r'),(24,1,'r'),(25,2,'r'),(26,2,'r'),(27,3,'r'),(28,4,'r'),(29,5,'r'),(30,6,'r'),(31,7,'r'),(31,8,'r'),(32,9,'r'),(32,10,'r'),(33,11,'r'),(33,12,'r'),(34,13,'r'),(34,14,'r'),(35,15,'r'),(35,16,'r'),(36,17,'r')]
L1=[(23,17,'b'),(22,17,'B'),(23,18,'b'),(24,19,'b'),(23,19,'B'),(24,20,'b')]
L2=[(10,26,'b'),(11,26,'b'),(12,26,'B'),(10,27,'b'),(11,27,'b'),(11,25,'B'),(12,25,'B')]
B1=[(2,1,'b'),(2,2,'b'),(3,2,'B'),(2,3,'b')]
B1T=[(11,22,'b'),(17,22,'b'),(12,23,'b'),(13,23,'B'),(15,23,'B'),(16,23,'b')]
B1R=[(14,y,'r') for y in range(19,27)]+[(16,0,'r'),(17,0,'r'),(18,1,'r'),(19,1,'d'),(12,0,'r'),(11,0,'r'),(10,1,'r'),(9,1,'d')]
B2=[(2,19,'b'),(2,20,'b'),(2,21,'B'),(2,22,'b'),(2,23,'b')]
B2T=[(7,21,'b'),(13,21,'b'),(8,22,'B'),(9,22,'b'),(11,22,'b'),(12,22,'B')]
B2R=[(10,y,'r') for y in range(19,27)]+[(12,0,'r'),(13,0,'r'),(14,1,'r'),(15,1,'d'),(8,0,'r'),(7,0,'r'),(6,1,'r'),(5,1,'d')]
# portrait: blue tips on the tail feathers (the red-orange ones at the far right), a red streamer off the crest
P=[]
im=Image.open('/home/claude/arena-clash/Pokemon/Pidgeot/Pokemon-018-Pidgeot.png').convert('RGBA'); px=im.load()
for y in range(im.height):
    for x in range(31,im.width):
        c=px[x,y]
        if c[3]>128 and abs(c[0]-255)<20 and abs(c[1]-98)<25: P.append((x,y+1,'b'))
        elif c[3]>128 and abs(c[0]-156)<15 and abs(c[1]-65)<15: P.append((x,y+1,'B'))
PR=[(23,1,'r'),(24,1,'r'),(25,1,'r'),(26,1,'r'),(27,2,'r'),(28,2,'r'),(29,2,'r'),(30,3,'r'),(31,3,'r'),(32,4,'r'),(33,5,'r'),(34,6,'r'),(35,7,'r'),(36,8,'r'),(36,9,'r')]
PATCH={'front1':pts(*F1,*mir(F1,30),*ribo(F1R)),'front2':pts(*F2,*mir(F2,22),*ribo(F2R)),
       'left1':pts(*ribo(LR),*L1),'left2':pts(*ribo(LR),*L2),
       'back1':pts(*B1,*mir(B1,29),*B1T,*ribo(B1R)),'back2':pts(*B2,*mir(B2,21),*B2T,*ribo(B2R)),
       'portrait':pts(*P,*ribo(PR))}
