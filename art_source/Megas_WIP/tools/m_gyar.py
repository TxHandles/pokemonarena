NAME='Mega Gyarados'; BASE='Gyarados'; DEX='130'; PID=10041
# Colours from the user's Mega Gyarados chart: bright blue, dark-grey belly with red marks, yellow/tan fins and crest.
RULES=[
 ('#4aafe1','#5a96e0'), ('#48b0df','#5a96e0'), ('#4aafdf','#5a96e0'),
 ('#407995','#4278c4'), ('#3f7796','#4278c4'), ('#3b7c93','#4278c4'), ('#297f7e','#4278c4'),
 ('#215e7e','#2c5596'), ('#205f81','#2c5596'), ('#225f81','#2c5596'),
 ('#32374e','#3c3d3e'), ('#303750','#3c3d3e'),
 ('#b18930','#505150'), ('#af8930','#505150'), ('#886121','#bb4737'), ('#82602c','#bb4737'), ('#866022','#bb4737'),
 ('#c1c1cf','#ebd375'), ('#c1c1d1','#ebd375'), ('#c1c1cd','#ebd375'), ('#899090','#bbac84'),
 ('#0a0a0a',None), ('#eae8f6',None), ('#e8e9f6',None), ('#880019',None), ('#890019',None),
 ('#f7c96b',None), ('#f8c86b',None), ('#af476a',None), ('#d57296',None), ('#d97196',None), ('#b94069',None), ('#b94167',None),
]
THRESH=10
LEG={'A':'#0a0a0a','c':'#ebd375','C':'#bbac84','r':'#bb4737'}
PATCH={}
RULES_portrait=[('#31a1e6','#5a96e0'),('#2e7fab','#4278c4'),('#4a4d4e','#3c3d3e'),('#9b846b','#bb4737'),
 ('#c2c4c3','#d2b474'),('#f1c797','#505150'),('#000000',None),('#f64439',None),('#ff9183',None),('#fafafa','#f4e4bc')]
UNDER='fFo'
LEG.update({'f':'#ebd375','F':'#bbac84','o':'#0a0a0a'})
PAD={'front':(3,2,0,6),'left':(2,4,1,4),'back':(0,0,0,0)}
FIN_L=["o.....",
       "ofo...",
       "offo..",
       ".offo.",
       ".oFffo",
       "..oFfo",
       "...oo."]
CREST=["....o.....o..",
       "...ofo...ofo.",
       "...ofFo.oFfo.",
       "....oFooFo..."]
def mirror(rows): return [r[::-1] for r in rows]
FRONT=[(3,6,FIN_L),(18,2,mirror(FIN_L)),(8,0,CREST)]
SIDE_FIN=["......oo",
          "....oofo",
          "..offffo",
          ".offfFfo",
          "offfFFo.",
          "oFFFFo..",
          ".oooo..."]
SPK=["...o","..oo",".ofo","ofFo"]
LEFT=[(6,2,SPK),(9,2,SPK),(12,2,SPK),(15,3,SPK),(5,1,["..o.","..fo",".ofo",".oFo"])]
BACK=[(6,0,CREST)]
PATCH={'front1':FRONT,'front2':FRONT,'left1':LEFT,'left2':LEFT,'back1':BACK,'back2':BACK}

# Long white whisker strands trailing from the jaw (user's chart): a white line with a dark outline
LEG.update({'w':'#f4f4f4'})
def strand(pts):
    out=[]
    for x,y in pts:
        for dx in (-1,0,1):
            for dy in (-1,0,1):
                out.append((x+dx,y+dy,['o']))   # 'o' is an UNDER colour: only fills empty pixels
    return out+[(x,y,['w']) for x,y in pts]
SL=[(8,27),(7,28),(6,29),(5,30),(5,31),(5,32),(6,33),(7,34),(8,34)]
SR=[(16,28),(16,29),(16,30),(17,31),(18,32),(19,33),(20,33)]
FRONT_S=strand(SL)+strand(SR)
SIDE=[(4,21),(3,22),(2,23),(1,24),(1,25),(2,26),(3,27),(4,28),(5,28)]   # +2 for the left pad
LEFT2=[(x+2,y,r) for x,y,r in LEFT]
PATCH={'front1':FRONT+FRONT_S,'front2':FRONT+FRONT_S,'left1':LEFT2+strand(SIDE),'left2':LEFT2+strand(SIDE),'back1':[],'back2':[]}
