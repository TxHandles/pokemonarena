# Mega Beedrill: walking frames drawn from scratch (bee_draw.py) to the Mega's build -- slim and upright,
# a long black-and-yellow abdomen with a big stinger, huge white lances, four raised white wings, red eyes.
# Colours from the user's chart. Build: python3 m_bee.py  (writes out/Mega Beedrill/; the portrait is the chart)
import os
from PIL import Image, ImageOps
NAME='Mega Beedrill'; BASE='Beedrill'; DEX='015'; PID=10090
OUT='/tmp/claude-0/-home-claude/ac4f08c9-6953-57f3-8701-1eb199b7c9da/scratchpad/mega/out'
RULES=[]; PATCH={}; LEG={}
def build():
    import sys; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import bee_draw as D
    res={'front1':D.front(1),'front2':D.front(2),'left1':D.left(1),'left2':D.left(2),'back1':D.back(1),'back2':D.back(2)}
    chart=os.path.join(os.path.dirname(os.path.abspath(__file__)),'charts','Mega Beedrill.png')
    res['portrait']=Image.open(chart).convert('RGBA')
    os.makedirs(f'{OUT}/{NAME}',exist_ok=True)
    for f in os.listdir(f'{OUT}/{NAME}'): os.remove(f'{OUT}/{NAME}/{f}')
    n={'front1':'1_mae_1','front2':'1_mae_2','left1':'2_hidari_1','left2':'2_hidari_2','back1':'4_ushiro_1','back2':'4_ushiro_2'}
    for k,v in n.items(): res[k].save(f'{OUT}/{NAME}/{DEX}_{v}.png')
    ImageOps.mirror(res['left1']).save(f'{OUT}/{NAME}/{DEX}_3_migi_1.png')
    ImageOps.mirror(res['left2']).save(f'{OUT}/{NAME}/{DEX}_3_migi_2.png')
    res['portrait'].save(f'{OUT}/{NAME}/Pokemon-{DEX}-Mega-Beedrill.png')
    return res
if __name__=='__main__':
    build()
