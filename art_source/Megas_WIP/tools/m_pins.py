NAME='Mega Pinsir'; BASE='Pinsir'; DEX='127'; PID=10040
# Colours from the user's Mega Pinsir chart: white spiky horns, brown/tan body, yellow and orange wings.
RULES=[
 ('#c2c0cf','#fefefe'), ('#c0c0d0','#fefefe'), ('#c2c0d1','#fefefe'), ('#909090','#c0b5a2'), ('#919190','#c0b5a2'),
 ('#68676a','#8a8682'), ('#686868','#8a8682'), ('#696969','#8a8682'),
 ('#988061','#a78758'), ('#978061','#a78758'), ('#988060','#a78758'), ('#776852','#846342'), ('#786850','#846342'),
 ('#615038','#66492f'), ('#5e523a','#66492f'), ('#4f4133','#4a3422'), ('#504033','#4a3422'), ('#594934','#4a3422'),
 ('#5b4b35','#4a3422'), ('#5f503c','#66492f'), ('#5f5138','#66492f'),
 ('#0a0a0a',None), ('#333333',None), ('#000000',None), ('#eae8f6','#fedd31'), ('#e8e8f8','#fedd31'),
]
RULES_portrait=[('#fafafa','#f8f0d8'), ('#c6c2bd','#e8d8b0'), ('#859194','#b89c70'),
 ('#ccb297','#a88464'), ('#edd6b0','#c8a47c'), ('#9a887f','#7c5c44'), ('#68554d','#5a3e2c'), ('#43413f',None)]
THRESH=12
UNDER='yorWQgG'
LEG={'A':'#0a0a0a','h':'#fefefe','H':'#c0b5a2','y':'#fedd31','o':'#fe8c39','r':'#e06a28','R':'#e06a28','W':'#0a0a0a','Q':'#fedd31',
     'Y':'#fedd31','O':'#fe8c39','E':'#e06a28','K':'#0a0a0a','L':'#fff09a','b':'#2a1c14','B':'#5a3a26','g':'#4b4b4b','G':'#6e6e6e'}
PAD={'front':(6,1,6,0),'left':(0,3,11,0),'back':(6,1,6,0),'portrait':(0,0,0,0)}
def mirror(rows): return [r[::-1] for r in rows]
WING=["..WWWWWWW....",
      ".WyyyyyyyWW..",
      "WyyyyyyyyyyW.",
      "WrrrryyyyyyyW",
      ".WooorrryyyyW",
      "..WWooooorryW",
      "....WWWWWoooW",
      ".........WWW."]
WING2=["WWWWWW.......",
       "WyyyyyyWWW...",
       "WyyyyyyyyyyW.",
       ".WrrrryyyyyyW",
       "..WooorrryyyW",
       "...WWooooorrW",
       ".....WWWWWooW",
       "..........WW."]
# horns: extra spikes (front view, padded by 6 left / 1 top)
SPK_F=[(6+2,1+1,["h"]),(6+2,1+2,["A"]),(6+1,1+3,["Ah"]),(6+7,1+1,["h"]),(6+8,1+2,["A"]),
       (6+22,1+1,["h"]),(6+22,1+2,["A"]),(6+22,1+3,["hA"]),(6+1,1+5,["hA"]),(6+22,1+5,["Ah"]),(6+17,1+1,["h"]),(6+16,1+2,["A"])]
EYES=[(14,10,['Y']),(22,10,['Y']),(14,11,['O']),(22,11,['O'])]
GLINT=[(14,10,['H']),(22,10,['H']),(14,11,['H']),(22,11,['H'])]   # these were light pixels at the horn roots, not eyes
FRONT1=[(0,5,WING),(23,5,mirror(WING))]+SPK_F+GLINT
FRONT2=[(0,4,WING2),(23,4,mirror(WING2))]+SPK_F+GLINT
# side view: the far wing rises behind the back (fills empty pixels only), the near wing sweeps back
# over the rear of the body
FAR_W=["......WWWWW...",
       "...WWWyyyyyW..",
       ".WWyyyyyyyyyW.",
       "WyyyyyyyyrrrW.",
       "WrrryyyyrooW..",
       ".WoooorroooW..",
       "..WWWWoooWW...",
       "......WWW....."]
FAR_W2=["....WWWWWW....",
        "..WWyyyyyyWW..",
        "WWyyyyyyyyyyW.",
        "WyyyyyyyyyrrW.",
        ".WrryyyyrrooW.",
        "..WoooorroooW.",
        "...WWWWoooWW..",
        ".......WWW...."]
NEAR_W=["KKKK..........",
        "KLYYKKKK......",
        "KYYYYYYYKKK...",
        ".KEEYYYYYYYKK.",
        "..KOOEEEYYYYYK",
        "...KKOOOOEEYYK",
        ".....KKKKKOOOK",
        "..........KKK."]
NEAR_W2=["KKKK..........",
         "KLYYKKKKK.....",
         "KYYYYYYYYKKK..",
         ".KEEYYYYYYYYKK",
         "..KOOEEEYYYYYK",
         "...KKOOOOOEEK.",
         ".....KKKKKKK..",
         ".............."]
LEFT1=[(11,2,FAR_W),(10,9,NEAR_W),(0,3+0,[".h"]),(1,3+1,["hA"])]
LEFT2=[(11,1,FAR_W2),(10,9,NEAR_W2),(0,3+0,[".h"]),(1,3+1,["hA"])]
BACK_STRIPE=[(6+8,1+11,["R.......R"]),(6+8,1+12,["R.R...R.R"]),(6+9,1+13,["R.....R"])]
# back: the red ridge plates behind the horns, leaning outward (like the reference's back spikes)
PLATES=[]
for (x,y,ch) in [(13,11,'E'),(13,10,'E'),(12,9,'E'),(12,8,'O'),(14,11,'K'),(14,10,'K'),(13,9,'K'),(13,8,'K'),(12,7,'K'),
                 (15,11,'E'),(15,10,'E'),(15,9,'O'),(16,11,'K'),(16,10,'K'),(16,9,'K'),(15,8,'K')]:
    PLATES.append((x,y,[ch])); PLATES.append((35-x,y,[ch]))
BACK1=[(0,4,WING),(23,4,mirror(WING))]+SPK_F+PLATES
BACK2=[(0,3,WING2),(23,3,mirror(WING2))]+SPK_F+PLATES
BIGW=["......WWWWWWWW.....",
      "...WWWyyyyyyyyWW...",
      ".WWyyyyyyyyyyyyyW..",
      "WyyyyyyyyyyyyyyyyW.",
      "WrrrrryyyyyyyyyyyW.",
      ".WoooorrrryyyyyyyyW",
      "..WWoooooorrryyyyyW",
      "....WWWooooooorryyW",
      ".......WWWWWoooooyW",
      "............WWWWWW."]
# orange-tipped back spikes rising behind the head (the user's chart), seen from the front
BSPK=[".W.","WyW","WoW","WoW","WrW","WgW"]
BSPK_S=[".W.","WyW","WoW","WrW","WgW"]
BACKSPIKES=[(17,0,BSPK),(14,1,BSPK_S),(20,1,BSPK_S),(11,3,BSPK_S),(23,3,BSPK_S)]
FRONT1=BACKSPIKES+FRONT1; FRONT2=BACKSPIKES+FRONT2
PATCH={'front1':FRONT1,'front2':FRONT2,'left1':LEFT1,'left2':LEFT2,'back1':BACK1,'back2':BACK2,
       'portrait':[(2,2,BIGW),(27,0,mirror(BIGW))]}
