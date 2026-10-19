NAME='Mega Alakazam'; BASE='Alakazam'; DEX='065'; PID=10037
W='#f4f4fc'; WS='#b8b8cc'; M='#907383'; MS='#7b4a6b'   # robe colours from the user's Mega Alakazam chart
RULES=[
 ('#8e7a64',M), ('#8e7962',M), ('#907860',M), ('#67513b',MS), ('#675239',MS), ('#685038',MS),
]
RULES_portrait=[('#906e5a',M)]
THRESH=10
LEG={'w':'#ffffff','s':WS,'r':'#f66152','R':'#b83a30','g':'#dcdce8','G':'#9898b0','k':'#2a2a36'}
PAD={'front':(4,6,4,0),'left':(4,6,4,0),'back':(4,6,4,0),'portrait':(0,3,0,0)}
def pts(lst,ox=0,oy=0): return [(x+ox,y+oy,[c]) for x,y,c in lst]
def stamp(x,y,rows):
    out=[]
    for dy,r in enumerate(rows):
        for dx,ch in enumerate(r):
            if ch!='.': out.append((x+dx,y+dy,ch))
    return out
# spoons: an outlined oval bowl and an outlined handle (vertical, or slanting toward the body)
SPOON_V=['.kk.','kwgk','kgGk','.kk.','.kGk','.kGk','..k.']
SPOON_L=['.kk...','kwgk..','kgGk..','.kkGk.','...kGk','....kk']   # handle goes down-right
SPOON_R=[r[::-1] for r in SPOON_L]                                  # handle goes down-left
def spoon(x,y,kind='v'):
    return stamp(x,y,{'v':SPOON_V,'l':SPOON_L,'r':SPOON_R}[kind])
GEM=lambda x,y:[(x,y,'r'),(x+1,y,'R')]
# front (coords before padding; pts adds the pad offset)
FB=[]
for y,(a,b) in zip(range(12,19),[(10,13),(9,14),(9,14),(9,14),(10,13),(10,13),(11,12)]):
    for x in range(a,b+1): FB.append((x,y,'s' if x in (a,b) or y==18 else 'w'))
FB+=[(8,13,'k'),(15,13,'k'),(8,14,'k'),(15,14,'k'),(8,15,'k'),(15,15,'k'),(9,16,'k'),(14,16,'k'),(9,17,'k'),(14,17,'k'),(10,18,'k'),(13,18,'k'),(11,19,'k'),(12,19,'k')]
# a long drooping moustache on both sides of the snout, outlined
MU=[(9,11,'w'),(8,11,'w'),(7,12,'w'),(6,13,'w'),(6,14,'w'),(6,15,'s'),(7,16,'s'),
    (9,10,'k'),(8,10,'k'),(7,11,'k'),(6,12,'k'),(5,13,'k'),(5,14,'k'),(5,15,'k'),(6,16,'k'),(7,17,'k'),(8,16,'k')]+[(7,13,'w'),(7,14,'w'),(7,15,'s'),(8,12,'w'),(8,13,'w'),(8,14,'s'),(9,12,'w')]
MU=MU+[(23-x,y,c) for x,y,c in MU]
FB2=[(x,y,'w') for y in (12,13) for x in range(9,15)]
F=GEM(11,6)+FB+FB2+MU
FS=spoon(-4,5)+spoon(24,5)+spoon(-4,-6,'l')+spoon(22,-6,'r')+spoon(10,-6)
L=[(5,7,'r'),(5,13,'w'),(5,14,'w'),(5,15,'w'),(5,16,'s'),(4,17,'w'),(5,17,'s'),(4,18,'s'),(6,13,'k'),(6,14,'k'),(6,15,'k'),(6,16,'k'),(6,17,'k'),(5,18,'k'),(4,19,'k'),(3,18,'k'),(3,17,'k'),
   (0,11,'w'),(-1,12,'w'),(-1,13,'s'),(-2,11,'k'),(-2,12,'k'),(-2,13,'k'),(-1,14,'k'),(0,13,'k'),(0,12,'k'),(1,12,'w'),(2,13,'w'),(3,13,'w'),(2,14,'s'),(3,14,'w'),(4,14,'w'),(3,15,'s'),(4,15,'w'),(4,16,'s'),(0,12,'k'),(1,13,'k'),(1,14,'k'),(2,15,'k'),(3,16,'k'),(4,17,'k')]
LS=spoon(-4,0,'l')+spoon(17,7)+spoon(16,-6,'r')
WISP=['..kw','.kww','kwws','kwws','kws.','.ks.','..k.']
BH=stamp(2,11,WISP)+stamp(16,11,[r[::-1] for r in WISP])
BS=BH+spoon(-4,5)+spoon(22,5)+spoon(-4,-6,'l')+spoon(20,-6,'r')+spoon(9,-6)
PBEARD=['.wwwww..',
        'swwwwwws',
        'swwwwwws',
        'kswwwwsk',
        '.kswwsk.',
        '..kwwk..',
        '..kssk..',
        '...kk...']
PATCH={'front1':pts(F+FS,4,6),'front2':pts(F+FS,4,6),'left1':pts(L+LS,4,6),'left2':pts(L+LS,4,6),
       'back1':pts(BS,4,6),'back2':pts(BS,4,6),'portrait':pts(spoon(1,-3,'l')+spoon(20,-3,'r')+spoon(31,-3)+[(19,8,'r'),(20,8,'R')]+stamp(15,14,PBEARD),0,3)}
