NAME='Mega Mewtwo Y'; BASE='Mewtwo'; DEX='150'; PID=10044
# Mega Mewtwo Y: pale and white, red eyes, a tube that runs from the back of the skull down to the upper back,
# purple tail (the belly is white from the front)
RULES=[('#0a0a0a',None), ('#404040',None), ('#000100',None), ('#424042',None), ('#5e5e5e',None), ('#fafafa',None)]
PURPLE=[('#b958cd','#a244e2'), ('#89488b','#6a2aa6'), ('#df89e8','#b866ee'), ('#a459a5','#7a34b4')]
RULES_front=[('#b958cd','#e2dcef'), ('#89488b','#b3abcc'), ('#402e54','#7e7898')]
RULES_left=PURPLE; RULES_back=PURPLE; RULES_portrait=PURPLE
THRESH=14
PAD={'front':(0,0,2,0),'left':(0,0,1,0)}
LEG={'k':'#0a0a0a','N':'#f4f2fb','n':'#c4bcd8','m':'#9a92b4','r':'#e02838'}
UNDER='1234'
LEG.update({'1':'#0a0a0a','2':'#9a92b4','3':'#c4bcd8','4':'#f4f2fb'})
def blk(x,y,rows): return (x,y,rows)
def pts(*p): return [(x,y,[c]) for x,y,c in p]
# front: the tube comes over the top of the head (behind it) and hangs down behind the right shoulder
def tube(path, under=True, light='4', shade='3', edge='1'):
    """A rounded 2-px tube along a list of centre points; outlined; returned as one-pixel patches."""
    fill={}
    for i,(x,y) in enumerate(path):
        fill[(x,y)]=light; 
    for i,(x,y) in enumerate(path):   # thicken to the lower/right side with the shade colour
        for (dx,dy) in ((1,0),(0,1)):
            q=(x+dx,y+dy)
            if q not in fill: fill[q]=shade
    out=dict(fill)
    for (x,y) in fill:
        for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
            q=(x+dx,y+dy)
            if q not in fill and q not in out: out[q]=edge
    return [(x,y,[c]) for (x,y),c in out.items()]
TUBE_F=tube([(16,3),(17,2),(18,1),(19,1),(20,2),(20,3),(20,4),(20,5),(20,6),(20,7),(20,8),(20,9),(19,10)])
EYES_F=pts((8,11,'r'),(14,11,'r'))
# left: from the back of the skull it sweeps backwards and curls down onto the upper back
TUBE_L=blk(12,2,['....kkk...',
                 '..kkNNNk..',
                 'kkNNnkkNk.',
                 'NNnkk..kNk',
                 'kkk.....kNk',
                 '........kNk',
                 '........knk',
                 '.......knk.',
                 '......knk..',
                 '......kk...'])
# back: the tube runs straight down from the back of the head and ends in a rounded bulb between the shoulders
TUBE_B=blk(10,6,['.kkk.',
                 'knNnk',
                 'knNnk',
                 'knNnk',
                 'kmNnk',
                 'kmNnk',
                 'kmnnk',
                 'kmnnk',
                 'kmnmk',
                 '.kmk.',
                 '..k..'])
TUBE_P=['.kkk.','kNNNk','NnkkNk','....kNk','....kNk','....knk','....knk','....knk','...knk.','..knk..','..kk...']
def st(x0,y0,rows): return [(x0+dx,y0+dy,ch) for dy,r in enumerate(rows) for dx,ch in enumerate(r) if ch not in '. ']
PATCH={
 'front1':TUBE_F+EYES_F, 'front2':TUBE_F+EYES_F,
 'left1':[TUBE_L]+pts((5,10,'r'),(4,10,'r')), 'left2':[TUBE_L]+pts((5,10,'r'),(4,10,'r')),
 'back1':[TUBE_B], 'back2':[TUBE_B],
 'portrait':pts(*(st(16,2,TUBE_P)+st(10,12,['rr'])))+pts((10,14,'r')),
}
REGION=[('portrait',4,21,16,33,[('#b866ee','#f2f0fa'),('#a244e2','#dcd6ec'),('#7a34b4','#c4bcd8'),('#6a2aa6','#aaa2c4')])]

# ---- user note: the purple comes from the BACK OF THE HEAD (the head tube), not the tail.
# The tube is purple (the chart's colours) and flows from the back of the skull down behind the back;
# the body's own tail becomes pale and short.
LEG.update({'p':'#a679a8','P':'#8d5190','D':'#79376e','o':'#303031','5':'#a679a8','6':'#79376e','7':'#303031'})
UNDER='1234567'
PALE=[('#b958cd','#c4bcd8'), ('#89488b','#9a92b4'), ('#df89e8','#c4bcd8'), ('#a459a5','#9a92b4'), ('#402e54','#5f5f5f'), ('#452e4a','#5f5f5f')]
RULES_left=PALE; RULES_back=PALE
TUBE_F=tube([(16,3),(17,2),(18,1),(19,1),(20,2),(20,3),(20,4),(20,5),(20,6),(20,7),(20,8),(20,9),(20,10),(20,11),(19,12)],
            light='5', shade='6', edge='7')
PAD['front']=(0,0,3,0)
SIDE_ERASE=(14,2,['________']*16)   # the old white tube and the tail's upward curl
SIDE_TUBE=(11,1,['..ooo......',
                 '.opPPoo....',
                 '.oPpPPPo...',
                 '..ooPpPPo..',
                 '....ooPpPo.',
                 '......oPpPo',
                 '......oPpPo',
                 '......oDPpo',
                 '......oDPpo',
                 '......oDPPo',
                 '......oDPPo',
                 '......oDDPo',
                 '.......oDPo',
                 '.......oDo.',
                 '........o..'])
TAILCUT=(16,17,['_____','_____'])  # trim the tail's tip so it stays short
BACK_TUBE=(10,5,['.ooo.',
                 'oPpPo',
                 'oPpPo',
                 'oPpPo',
                 'oDpPo',
                 'oDPPo',
                 'oDPPo',
                 'oDPPo',
                 'oDPDo',
                 'oDDDo',
                 '.oDo.',
                 '..o..'])
for f in ('front1','front2'): PATCH[f]=TUBE_F+EYES_F
for f in ('left1','left2'): PATCH[f]=[SIDE_ERASE,TAILCUT,SIDE_TUBE]+pts((5,10,'r'),(4,10,'r'))
for f in ('back1','back2'): PATCH[f]=[BACK_TUBE]
FIX_L=[(18,19,['__']),(13,16,['o.']),(14,17,['oo']),(16,18,['o'])]   # tidy the trimmed tail
for f in ('left1','left2'): PATCH[f]=PATCH[f]+FIX_L
