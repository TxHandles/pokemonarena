NAME='Mega Aerodactyl'; BASE='Aerodactyl'; DEX='142'; PID=10042
# Colours from the user's Mega Aerodactyl chart: charcoal / warm-grey rocky body, blue-violet wing membranes,
# green eyes, orange-red mouth; big rock spikes (jaw, chin, crest, back, wing joints, tail).
RULES=[
 # overworld body (light / mid / dark / outline)
 ('#b2afde','#9a8a88'), ('#b1b0de','#9a8a88'), ('#b1b0e0','#9a8a88'),
 ('#9287c6','#726664'), ('#9088c5','#726664'), ('#9088c6','#726664'),
 ('#7e72af','#4f4a4a'), ('#8172b0','#4f4a4a'), ('#7f73b0','#4f4a4a'), ('#7e72b1','#4f4a4a'),
 ('#464160','#262426'), ('#49405e','#262426'), ('#47405e','#262426'),
 # wings
 ('#b889bf','#7d6ab8'), ('#b889bd','#7d6ab8'), ('#815881','#55458e'), ('#86567d','#55458e'),
 # mouth
 ('#5c1111','#b8593c'), ('#5f100d','#b8593c'), ('#5b130d','#b8593c'), ('#5f100f','#b8593c'),
 # portrait
 ('#c3c1cc','#9a8a88'), ('#95948c','#726664'), ('#715f58','#4f4a4a'), ('#43413f','#262426'),
 ('#936594','#55458e'), ('#d58fc9','#7d6ab8'), ('#c45023','#c8663f'),
]
THRESH=10
LEG={'k':'#161416','r':'#3a3838','R':'#5c5656','h':'#bcbcbc','e':'#52c27a'}
PAD={'front':(0,0,0,3),'left':(1,0,0,3),'portrait':(0,0,0,2)}
def st(x0,y0,rows): return [(x0+dx,y0+dy,ch) for dy,r in enumerate(rows) for dx,ch in enumerate(r) if ch not in '. ']
def pts(p): return [(x,y,[c]) for x,y,c in p]
def mir(p,w): return [(w-1-x,y,c) for x,y,c in p]
def both(p,w): return pts(p+mir(p,w))
def sh(p,dx,dy): return [(x+dx,y+dy,c) for x,y,c in p]
BLADE=['...kk','..kRk','.kRhk','kRhRk','kRRk.','kk...']   # a rock blade jutting down-left from the jaw
CHIN=['kRRhRRk','.kRhRk.','..khk..','...k...']            # one big rock spike under the chin
KNOB=['.k.','khk','kRk','kRk']                                   # an upright rock spike
CREST=['.k.','khk','kRk']
# front (wings up / wings down)
F1=st(4,18,BLADE)+st(9,0,CREST)+st(4,1,['kk','hR','kr'])+st(0,0,['kk','kh'])
F1C=st(11,21,CHIN)+st(13,4,['.k.','khk'])
F2=st(6,15,BLADE)+st(8,0,['k.','hk','rk'])+st(3,11,['.k','kh','kr'])
F2C=st(11,18,CHIN)+st(13,1,['.k.','khk'])
# back: dorsal rock spikes down the spine
B1=st(9,0,CREST)+st(4,1,['kk','hR','kr'])
B1C=st(13,4,KNOB)+st(13,8,KNOB)+st(13,12,KNOB)+st(13,16,KNOB)
B2=st(8,0,['k.','hk','rk'])+st(3,11,['.k','kh','kr'])
B2C=st(13,2,KNOB)+st(13,6,KNOB)+st(13,10,KNOB)+st(13,14,KNOB)
# sides (+1 for the left pad)
L1=sh(st(2,16,['kRRRk','.kRhk','..khk','...k.'])   # one rock spike under the chin
      +st(8,11,['kh','Rk'])                     # cheek spike
      +st(11,0,['kk','hk'])                     # crest crag
      +st(15,12,['.k','kh'])+st(18,12,['.k','kh'])+st(21,13,['.k','kh'])   # tail spikes
      +st(24,1,['k','h','R'])                   # wing joint
      +st(5,10,['e']),1,0)
L2=sh(st(2,15,['kRRRk','.kRhk','..khk','...k.'])
      +st(10,0,['k','h'])
      +st(19,12,['.k','kh'])
      +st(11,17,['k','h'])+st(12,20,['k','h'])
      +st(5,8,['e']),1,0)
P=(st(9,29,['kRhRhRk','.kRkRk.','..k.k..'])          # rock spikes under the jaw
   +st(18,22,['k..','Rk.','hRk','.kk'])                 # cheek spike
   +st(12,5,CREST)+st(20,5,CREST)                       # crest crags
   +st(23,9,KNOB)+st(27,10,KNOB)                        # back spikes
   +st(35,19,['k','h'])+st(38,17,['k','h'])+st(41,16,['k','h'])
   +st(30,4,['k','h']))
PATCH={
 'front1':both(F1,29)+pts(F1C), 'front2':both(F2,29)+pts(F2C),
 'back1':both(B1,29)+pts(B1C), 'back2':both(B2,29)+pts(B2C),
 'left1':pts(L1), 'left2':pts(L2), 'portrait':pts(P),
}
