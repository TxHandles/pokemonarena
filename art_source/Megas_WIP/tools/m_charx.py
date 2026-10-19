NAME='Mega Charizard X'; BASE='Charizard'; DEX='006'; PID=10034
RULES=[
 ('#603000','#101018'), ('#0a0a0a',None), ('#313131','#101018'),  # outlines
 ('#f87838','#5c5c70'), ('#c85828','#444456'), ('#904020','#2e2e3c'),   # body
 ('#f79429','#5c5c70'), ('#d66342','#444456'), ('#b5423a','#2e2e3c'),
 ('#d8a830','#4aa0e0'), ('#f0d070','#8ed8ff'), ('#f7d673','#8ed8ff'), ('#d6b563','#4aa0e0'), # belly -> light blue
 ('#408080','#2d6fd0'), ('#284858','#1d3f8c'), ('#316b5a','#1d3f8c'), ('#4a8473','#2d6fd0'), # wings
 ('#c01020','#1e7cff'), ('#f8b810','#9ef0ff'), ('#ff5a3a','#1e7cff'), ('#ffe628','#9ef0ff'), # flame
 ('#d8d8f8',None), ('#a0a0a0',None), ('#fafafa',None), ('#bdbdbd',None), ('#525252','#2e2e3c'),
]
THRESH=45
LEG={'b':'#1e7cff','c':'#9ef0ff','r':'#e03030','d':'#2a6ed0','w':'#f0fcff','e':'#5cd4ff'}
def pts(*p):   # (x, y, colour key) -> one-pixel patches
    return [(x,y,[c]) for x,y,c in p]
FLAME_L=[  # a blue flame streaming up and out from the left mouth corner (front view), origin (3,7)
 "d....d",
 "ed..de",
 "ce.dec",
 "wceecd",
 ".ewcc.",
 "..ecce",
 "...ewc",
 "....ec",
]
def mirror(rows, width=28, x0=3):   # the same flame for the right mouth corner
    w=len(rows[0]); return (width-x0-w, [r[::-1] for r in rows])
mx,mrows=mirror(FLAME_L)
FRONT=pts((10,1,'c'),(17,1,'c'),(10,2,'b'),(17,2,'b'),(11,10,'r'),(16,10,'r'))+[(3,7,FLAME_L),(mx,7,mrows)]
LEFT=pts((6,8,'r'),(14,0,'b'),(13,1,'b'))+[(1,10,[  # flames streaming back from the mouth under the eye
 ".........d",
 "ec.....dce",
 ".ewceecwc.",
 "..eccwce..",
 "....eed...",
])]
BACK1=pts((8,4,'b'),(9,3,'c'),(19,4,'b'),(18,3,'c'),(8,5,'b'),(19,5,'b'))
BACK2=pts((6,4,'b'),(7,3,'c'),(17,4,'b'),(16,3,'c'),(6,5,'b'),(17,5,'b'))
PATCH={'back1':BACK1,'back2':BACK2,'front1':FRONT,'front2':FRONT,'left1':LEFT,'left2':LEFT,
       'portrait':pts((8,15,'r'),(8,16,'r'),(5,21,'b'),(6,21,'c'),(4,22,'b'))}

# Match the user's chart (charts/Mega Charizard X.png): mid-grey body, blue wings, light-blue belly and flames. Build with mk_e.py.
POST={'#5c5c70':'#7c7971','#444456':'#5a5a55','#2e2e3c':'#47473f','#101018':'#2f2f2f','#0a0a0a':'#2f2f2f',
      '#1d3f8c':'#395a9c','#2d6fd0':'#4a84bd','#4aa0e0':'#43b2fc','#8ed8ff':'#a3e3fc','#1e7cff':'#43b2fc',
      '#9ef0ff':'#a3e3fc','#e03030':'#c64a39'}
