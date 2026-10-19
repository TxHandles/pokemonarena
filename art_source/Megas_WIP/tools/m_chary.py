NAME='Mega Charizard Y'; BASE='Charizard'; DEX='006'; PID=10035
RULES=[]
THRESH=20
PAD={'front':(3,3,3,0),'back':(3,3,3,0),'left':(0,3,0,0),'portrait':(0,3,0,0)}
LEG={'Q':'#313131','P':'#f79429','O':'#603000','B':'#f87838','E':'#c85828','D':'#904020','Y':'#f0a040','R':'#c05a28','K':'#0a0a0a','U':'#284858','f':'#b5423a','h':'#d66342','T':'#316b5a','t':'#4a8473','d':'#f79429'}
PATCH={}
def blk(x,y,rows): return (x,y,rows)
def horns(xs, top=0): return [blk(x-1,top,['.O.','OBO','OBO','.B.']) for x in xs]
def mir(b, W):   # the same block on the other side (W = width - 1)
    x,y,rows=b; n=max(len(r) for r in rows)
    return (W-x-n+1, y, [r.ljust(n,'.')[::-1] for r in rows])
def both(b, W): return [b, mir(b, W)]
F1=horns([13,20])+both(blk(4,4,['..O....','..OO...','.OEO...','.OEEO..','OEEEO..','OEEEEO.','OEDEEO.','.OE....']),33)
F2=horns([13,20])+both(blk(3,3,['..O...','.OEO..','.OEEO.','OEEEEO']),33)
B1=horns([13,20])+both(blk(3,9,['..O...','.OBO..','.OBBO.','OBBBEO','OBBEEO','OBBEE.']),33)
B2=horns([13,20])+both(blk(3,10,['..O...','.OBO..','.OBBO.','OBBBBO','OBBBEO','OBBEE.','OBEE..','OBE...']),29)
L=[blk(14,0,['..O','.OB','OBO']), blk(16,2,['..O.','.OBO','.OBO','OBBO']),
   blk(19,4,['...O','..OO','.OBO','OBEO','OEUO','OEUO','.OUO','..O.'])]
PORT=[
 blk(10,1,['Q...........Q',
           'QQ....Q....QQ',
           'QdQ..QdQ..QdQ',
           'QddQQdddQQddQ',
           'QdhddhdhddhdQ',
           '.QdhTTTTThdQ.',
           '..QdTtttTTdQ.',
           '...QdTttTdQ..',
           '....QQdTdQ...']),
 # long horns swept back
 blk(3,5,['.....QQ','....QdQ','...QdQ.','..QdQ..','.QdQ...','QddQ...']),
 blk(9,9,['..QQ','.QdQ','QdQ.']),
]
PATCH={'portrait':PORT,'front1':F1,'front2':F2,'back1':B1,'back2':B2,'left1':L,'left2':L}

# ---- user note: "Looks like a regular Charizard, try to make the sharper spikey wings and the sharp point
# on his head more pronounced."  Big jagged wings (three sharp points along the trailing edge, a sharp tip on
# top) that spread well past the body, and long sharp horns.
PAD={'front':(7,6,7,0),'back':(7,6,7,0),'left':(0,7,4,0),'portrait':(0,3,0,0)}
WING=[".O..............",   # left wing, front view (membrane teal); the right wing is its mirror
      ".OO.............",
      ".OBO............",
      "..OBO...........",
      "..OTBO..........",
      "..OtTBO.........",
      "..OttTBO........",
      "..OttTTBO.......",
      ".OtttTTTBO......",
      ".OttttTTTBO.....",
      ".OttttTTTTBO....",
      "OtttttTTTTTBOO..",
      "OtttttTTTTTTBBO.",
      "OttttttTTTTTEBBO",
      "OtttOttTTTTUUEBO",
      "OttO.OtTTOTTUUEO",
      "OtO..OtTO.OTUUEO",
      "OO...OTO...OUUO.",
      "O....OO....OUO..",
      ".....O......O...",
      "............O..."]
def flip(rows): return [r[::-1] for r in rows]
def wingpair(dy, W=41, rows=WING):
    return [(0,dy,rows), (W-len(rows[0])+1,dy,flip(rows))]
ERASE_F1=[(7,12,['________']*9), (27,12,['________']*9)]
ERASE_F2=[(7,10,['________']*10), (27,10,['________']*10)]
HORN=['.O.','.B.','OBO','OBO','OBO','OEO']          # long sharp horns (front/back)
def hornpair(x1,x2,top): return [(x1-1,top,HORN),(x2-1,top,HORN)]
F1=ERASE_F1+wingpair(1)+hornpair(17,24,0)
F2=ERASE_F2+wingpair(0)+hornpair(17,24,0)
WING=[r[:7].replace('t','E')+r[7:] for r in WING]   # orange outer membrane, teal near the body (like the chart)
BACKW=[r.replace('t','B').replace('T','E').replace('U','D') for r in WING]   # the back of the wing is orange
ERASE_B=[(7,14,['_________']*9),(30,14,['_________']*9)]
B1=ERASE_B+wingpair(3,W=37,rows=BACKW)+hornpair(17,24,4)
B2=ERASE_B+wingpair(2,W=37,rows=BACKW)+hornpair(17,24,4)
# back frames differ in width (28 and 24): each gets its own erase/mirror/horns
ERASE_B1=[(7,14,['________']*9),(27,14,['________']*9)]
ERASE_B2=[(7,16,['_______']*11),(24,16,['_______']*11)]
B1=ERASE_B1+wingpair(3,W=41,rows=BACKW)+hornpair(17,24,4)
B2=ERASE_B2+wingpair(4,W=37,rows=BACKW)+hornpair(15,22,4)
PATCH.update({'front1':F1,'front2':F2,'back1':B1,'back2':B2})
# side view: a tall pointed wing rising from the back (three points on the trailing edge) and a long horn swept back
SIDEW=["...........O.....",
       "..........OBO....",
       "..........OBEO...",
       ".........OBEEO...",
       ".........OBEEEO..",
       "........OBEETEEO.",
       "........OBEETEEEO",
       ".......OBEETTEOO.",
       ".......OBEETTEEO.",
       "......OBEETTTEEEO",
       "......OBEETTTEOO.",
       ".....OBEETTTTEEO.",
       ".....OBEETTTTEEEO",
       "....OBEETTTUUEOO.",
       "....OBEETTUUUEO..",
       "...OBEETTUUUEO...",
       "...OBEEUUUEOO....",
       "....OOOOOOO......"]
SIDEH=[".......OO",
       "......OBO",
       ".....OBO.",
       "....OBO..",
       "...OBO...",
       "..OBEO...",
       ".OBEO....",
       "OBEO....."]
L=[(14,0,SIDEW),(11,1,SIDEH)]
PATCH.update({'left1':L,'left2':L})
# rebuild the front frames with the orange-and-teal wing
F1=ERASE_F1+wingpair(1,rows=WING)+hornpair(17,24,0)
F2=ERASE_F2+wingpair(0,rows=WING)+hornpair(17,24,0)
PATCH.update({'front1':F1,'front2':F2})
