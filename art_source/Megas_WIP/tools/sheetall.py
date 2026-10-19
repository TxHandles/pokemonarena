import glob,os
from PIL import Image, ImageDraw
SP='/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad'
order=[('Mega Venusaur','Venusaur',10033),('Mega Charizard X','Charizard',10034),('Mega Charizard Y','Charizard',10035),('Mega Blastoise','Blastoise',10036),('Mega Beedrill','Beedrill',10090),('Mega Pidgeot','Pidgeot',10073),('Mega Alakazam','Alakazam',10037),('Mega Slowbro','Slowbro',10071),('Mega Gengar','Gengar',10038),('Mega Kangaskhan','Kangaskhan',10039),('Mega Pinsir','Pinsir',10040),('Mega Gyarados','Gyarados',10041),('Mega Aerodactyl','Aerodactyl',10042),('Mega Mewtwo X','Mewtwo',10043),('Mega Mewtwo Y','Mewtwo',10044)]
S=5
def views(folder):
    fs=sorted(glob.glob(folder+'/*.png'))
    pick=lambda k:[f for f in fs if k in os.path.basename(f)]
    return [Image.open(f).convert('RGBA') for f in pick('_1_mae_1')+pick('_2_hidari_1')+pick('_3_migi_1')+pick('_4_ushiro_1')+(pick('Pokemon-') if 'arena-clash' in folder else [])]
def rowimg(name,bv,mv,ref):
    H=max(max(i.height for i in mv)*S, max(i.height for i in bv)*3, ref.height*2)+24
    W=4000
    c=Image.new('RGBA',(W,H),(64,120,72,255)); d=ImageDraw.Draw(c)
    d.text((8,4),name,fill=(255,240,160))
    x=8
    for i in bv: c.alpha_composite(i.resize((i.width*3,i.height*3),Image.NEAREST),(x,20)); x+=i.width*3+8
    x+=20
    for i in mv: c.alpha_composite(i.resize((i.width*S,i.height*S),Image.NEAREST),(x,20)); x+=i.width*S+10
    x+=20
    c.alpha_composite(ref.resize((ref.width*2,ref.height*2),Image.NEAREST),(x,20)); x+=ref.width*2+10
    return c.crop((0,0,x,H))
imgs=[]
for name,base,pid in order:
    ref=Image.open(f'{SP}/pas/sprites/pokemon/{pid}.png').convert('RGBA'); ref=ref.crop(ref.getbbox())
    imgs.append(rowimg(name,views(f'/home/claude/arena-clash/Pokemon/{base}'),views(f'out/{name}')+[Image.open(f'charts/{name}.png').convert('RGBA')],ref))
for part,(a,b) in enumerate([(0,5),(5,10),(10,15)]):
    sub=imgs[a:b]; W=max(i.width for i in sub); H=sum(i.height for i in sub)
    c=Image.new('RGBA',(W,H),(40,44,56,255)); y=0
    for i in sub: c.alpha_composite(i,(0,y)); y+=i.height
    c.save(f'{SP}/deliver/megas_v4r_{part+1}.png')
