NAME='Mega Venusaur'; BASE='Venusaur'; DEX='003'; PID=10033
RULES=[
 ('#c84880','#e85a6a'), ('#f078a0','#ff9c9c'), ('#e8c070','#fff0dc'),   # petals: salmon with pale tips
 ('#68d8c0','#4cbcae'), ('#409860','#2c7c74'),                         # body: deeper teal
 ('#60a810','#3c9a40'), ('#88e020','#78d060'), ('#406030','#24502c'), ('#3f6030','#24502c'),  # fern leaves
]
THRESH=20
RULES_portrait=[('#e66b73','#ff5c78'),('#ffb5a5','#ffc4cc'),('#a5424a','#b0304a'),
 ('#73b55a','#3c9a40'),('#63945a','#2c7a38'),('#4a734a','#24502c'),
 ('#6bbda5','#4cbcae'),('#429484','#2c7c74'),('#297b6b','#1c5c56')]
PAD={'front':(2,0,2,0),'back':(2,0,2,0),'portrait':(0,2,0,0)}
LEG={'A':'#682028','B':'#e85a6a','D':'#ff9c9c','W':'#fff0dc','Y':'#ffd000','y':'#d89000','o':'#313131','p':'#ff5c78','q':'#ffc4cc'}
def blk(x,y,rows): return (x,y,rows)
def wide(xl, xr, rows):   # petals one pixel wider on each side
    out=[]
    for y in rows: out += [blk(xl,y,['AB']), blk(xr,y,['BA'])]
    return out
HEADF=['.ABBA.','ABDDBA','BDYYDB','ABYyBA','.ABBA.']   # the flower on its head
HEADP=['.AB.BA.','ABDBDBA','BDDYDDB','ABYyYBA','.BDYDB.','..ABA..']
F=wide(4,23,(3,4,5))+[blk(12,13,HEADF)]
B=wide(5,24,(3,4,5))
HEADS=['.ABA.','ABYBA','.AAA.']   # side view: a small flower seated on the forehead
L=[blk(1,9,HEADS)]+[blk(5,y,['AB']) for y in (3,4,5)]+[blk(24,y,['BA']) for y in (3,4,5)]+[blk(6,2,['A']),blk(24,2,['A']),blk(6,6,['A']),blk(24,6,['A'])]
Pp=[blk(13,0,['oooooooo']),
    blk(12,1,['opqqppqqpo']),
    blk(11,2,['oppqpppqppo']),
    blk(8,4,['op']),blk(8,5,['op']),blk(9,3,['o']),blk(24,5,['po']),blk(24,6,['po']),blk(25,4,['o']),   # wider petals
    blk(5,12,HEADP)]                                                   # head flower
PATCH={'portrait':Pp,'front1':F,'front2':F,'back1':B,'back2':B,'left1':L,'left2':L}
REGION=[('portrait',9,1,27,8,[('#525252','#b0304a')])]   # petal shading inside the flower
