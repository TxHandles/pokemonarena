NAME='Mega Mewtwo X'; BASE='Mewtwo'; DEX='150'; PID=10043
# blue-grey body, deep purple belly and tail (the tail comes over the shoulder), blue eyes
RULES=[
 ('#eae8f6','#cdd1e6'), ('#e8e9f6','#cdd1e6'), ('#d6cad6','#acb2d0'), ('#d7cad7','#acb2d0'),
 ('#af99c0','#8890b8'), ('#ae9ac0','#8890b8'), ('#817999','#5e6590'), ('#7f7898','#5e6590'),
 ('#b958cd','#8a3cd0'), ('#89488b','#5a2a98'), ('#402e54','#2c1a52'), ('#452e4a','#2c1a52'),
 ('#e7e9ed','#cdd1e6'), ('#c7c0d2','#acb2d0'), ('#93928c','#7a80a8'), ('#656467','#525a80'),
 ('#df89e8','#a050e4'), ('#a459a5','#6a30aa'), ('#fafafa',None), ('#0a0a0a',None), ('#404040',None), ('#000100',None), ('#424042',None), ('#5e5e5e',None),
]
THRESH=14
LEG={'p':'#8a3cd0','P':'#5a2a98','e':'#40a8ff','k':'#0a0a0a'}
def pts(*p): return [(x,y,[c]) for x,y,c in p]
def st(x0,y0,rows): return [(x0+dx,y0+dy,ch) for dy,r in enumerate(rows) for dx,ch in enumerate(r) if ch not in '. ']
SCARF=['....kk','...kpPk','..kpPk.','.kpPk..','kpPk...','kPk....']
PATCH={
 'front1':pts(*(st(8,11,['e']) + st(14,11,['e']))),
 'front2':pts(*(st(8,11,['e']) + st(14,11,['e']))),
 'left1':pts(*(st(5,10,['e']) + st(8,11,['.kkk','kpPPk','kPPk.']) + st(11,8,['.kk','kpk','kPk']))),
 'left2':pts(*(st(5,10,['e']) + st(8,11,['.kkk','kpPPk','kPPk.']) + st(11,8,['.kk','kpk','kPk']))),
 'back1':pts(*st(11,8,['.k.','kPk','kpk','kpk','kpk','kpk','kpk','kPk','kpk'])),
 'back2':pts(*st(11,8,['.k.','kPk','kpk','kpk','kpk','kpk','kpk','kPk','kpk'])),
 'portrait':pts(*(st(10,12,['ee']) + st(8,16,['kk....','kpPk...','.kpPk..','..kpPk.','...kpPk','....kpk']))),
}
# ---- polish pass: swept-back horn ears and bulky forearms (front/back), shifted for the 2-px top pad
PAD={'front':(0,2,0,0),'back':(0,2,0,0),'left':(0,2,0,0)}
LEG.update({'W':'#cdd1e6','L':'#acb2d0','M':'#8890b8'})
EAR=['kk....','kWWk..','.kWWLk','.kWLLk','..kLk.']
def mir(rows): return [r[::-1] for r in rows]
ARM=['..kkk.','.kWWLk','kWWLMk','kWLMk.','kLMk..','.kk...']
def shift(lst,dy): return [(x,y+dy,c) for x,y,c in lst]
def front(dyL):
    return (pts(*shift([p for p in st(0,0,[])],0)) + pts(*st(3,0,EAR)) + pts(*st(15,0,mir(EAR)))
            + pts(*st(0,17+dyL,ARM)) + pts(*st(17,17,mir(ARM))))
PATCH['front1']=[(x,y+2,c) for x,y,c in PATCH['front1']] + front(0)
PATCH['front2']=[(x,y+2,c) for x,y,c in PATCH['front2']] + front(-1)
PATCH['back1']=[(x,y+2,c) for x,y,c in PATCH['back1']] + pts(*st(3,0,EAR)) + pts(*st(15,0,mir(EAR)))
PATCH['back2']=[(x,y+2,c) for x,y,c in PATCH['back2']] + pts(*st(3,0,EAR)) + pts(*st(15,0,mir(EAR)))
EARS=['....kk','..kkWk','.kWWLk','kWWLk.','kLLk..']
ARMS=['.kkk.','kWWLk','kWLMk','kLMk.','.kk..']
for f in ('left1','left2'):
    PATCH[f]=[(x,y+2,c) for x,y,c in PATCH[f]] + pts(*st(10,0,EARS)) + pts(*st(1,18,ARMS))
PATCH['portrait']=PATCH['portrait'] + pts(*st(16,0,['...kk','.kkWk','kWWLk','kWLk.'])) + pts(*st(5,0,['kk...','kWkk.','kLWWk','.kLWk']))
PATCH['portrait']=PATCH['portrait']+pts((10,14,'e'))
PATCH['left1']=PATCH['left1']+pts((4,12,'e')); PATCH['left2']=PATCH['left2']+pts((4,12,'e'))

# Match the user's chart (charts/Mega Mewtwo X.png): pale lavender/white, pink-magenta, warm grey. Build with mk_e.py.
POST={'#cdd1e6':'#e7e9ed','#acb2d0':'#c7c0d2','#8890b8':'#93928c','#5e6590':'#4f4e4f','#404040':'#4f4e4f',
      '#5e5e5e':'#4f4e4f','#0a0a0a':'#303131','#8a3cd0':'#df89e8','#5a2a98':'#a459a5','#40a8ff':'#2c84fe','#2c1a52':'#a459a5','#5f5f5f':'#4f4e4f'}

# ---- user note: the pink came too close to the eye; it should run across both shoulders (a V-shaped collar)
COLLAR=['.kpp.........ppk.',
        'kpPpp.......ppPpk',
        '...Pppk...kppP...',
        '......PpppP......']
PATCH['front1']+=pts(*st(3,15,COLLAR)); PATCH['front2']+=pts(*st(3,15,COLLAR))
