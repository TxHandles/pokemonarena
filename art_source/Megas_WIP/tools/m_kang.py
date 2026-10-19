NAME='Mega Kangaskhan'; BASE='Kangaskhan'; DEX='115'; PID=10039
# The mother keeps her colours; her pouch is closed up and the baby (a little Kangaskhan in the
# family's colours: grey helmet, tan ears, brown body, white eyes, tan muzzle) stands beside her, fists up.
RULES=[]
THRESH=30
LEG={'A':'#0a0a0a','B':'#8a8a8a','D':'#585858','E':'#f8c878','G':'#d0a860','F':'#a07828',
     'I':'#b87870','M':'#785038','C':'#403020','w':'#f4f4ff','m':'#e8c890','r':'#c02838',
     # portrait palette
     'b':'#000000','f':'#a59aa7','e':'#7d6a65','c':'#fef0ad','h':'#dfa976','j':'#c48061','g':'#fafafa','d':'#434041','i':'#f66d52'}
PAD={'front':(8,0,0,0),'left':(6,0,0,0),'back':(8,0,0,0),'portrait':(12,0,0,0)}
# --- the baby (colours from the user's Mega Kangaskhan chart): lavender body, cream belly and muzzle,
#     brown-grey helmet, tan ears; it stands right up against its mother, fists raised
LEG.update({'L':'#9797cd','l':'#6c6cab','n':'#bcbcec','c':'#f6eeac','h':'#7f6756','H':'#525252','t':'#c9a873'})
BABY_F=["..A.AAA.A..",
        ".AtAhhhAtA.",
        ".AttHHHttA.",
        ".ALwALAwLA.",
        "AnALcrcLAnA",
        "AnALcccLAnA",
        ".AALcccLAA.",
        "..AlA.AlA..",
        "..AA...AA.."]
BABY_F2=BABY_F[:7]+[".AlA...AlA.",".AA.....AA."]
BABY_L=["....A.AA.",
        "...AtAhtA",
        "..AhHHHHA",
        "ALAwLLLLA",
        "ccLLLLLA.",
        "ArLLAnnA.",
        "..AcLlLA.",
        "..AlAAlA.",
        "..AA..AA."]
BABY_L2=BABY_L[:7]+[".AlA.AlA.",".AA..AA.."]
BABY_B=["..A.AAA.A..",
        ".AtAhhhAtA.",
        ".AttHHHttA.",
        "..AHHHHHA..",
        "AnALLLLLAnA",
        "AnALLLLLAnA",
        "..ALLlLLA..",
        "..AlA.AlA..",
        "..AA...AA.."]
BABY_B2=BABY_B[:7]+[".AlA...AlA.",".AA.....AA."]
# --- the mother's pouch, closed (front: base cols 6-15, rows 13-17; +10 for the pad)
POUCH_F=(14,13,["MCCCCCCCCM",
                "MGEEEEEEGM",
                "MFGGGGGGFM",
                "CMFGGGGFMC",
                "CCMFFFFMCC"])
POUCH_L=(8,14,[".ACCCC",
                "AGEEGC",
                "AGGGGC",
                "AFGGGC",
                ".AFGFC",
                "..AFFC"])
POUCH_L2=(8,14,[".ACCCC",
                 "AGEEGC",
                 "AGGGGC",
                 "AFGGGC",
                 ".AFGFC",
                 "..AFFC",
                 "...AAC"])
# --- portrait: the baby out of the pouch, standing to the mother's left, fists up (12 px pad)
BABY_P=["...b....b....",
        "..bcb..bcb...",
        "..bcbbbbcb...",
        ".bffffffffb..",
        ".bfffffeefb..",
        "beeeeeeeeeb..",
        "bjgbjjjjjjb..",
        "hhhjjjjjjjb..",
        "bhhjjjjjjb...",
        ".bijjbbbb....",
        "bhhbjjjjb....",
        "bhhbjjjjjb...",
        ".bbjjjjjjb...",
        "..bjhhhjjb...",
        "..bjhhhjb....",
        ".bjjb.bjjb...",
        ".bbb...bbb..."]
PATCH={'front1':[POUCH_F,(0,12,BABY_F)],'front2':[POUCH_F,(0,12,BABY_F2)],
       'left1':[POUCH_L,(0,14,BABY_L)],'left2':[POUCH_L2,(0,14,BABY_L2)],
       'back1':[(0,12,BABY_B)],'back2':[(0,12,BABY_B2)],
       'portrait':[(0,16,BABY_P)]}
# portrait: the pouch is closed up (the baby's colours there become the mother's belly)
REGION=[('portrait',3,16,19,31,[('#fef0ad','#dfa976'),('#a59aa7','#c48061'),('#7d6a65','#c48061'),('#fafafa','#dfa976'),('#f66d52','#dfa976'),('#434041','#9c5c44')])]
