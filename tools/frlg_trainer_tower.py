#!/usr/bin/env python3
"""The Trainer Tower on Seven Island (FireRed / LeafGreen: pret/pokefirered's src/trainer_tower_sets.c).

Prints the World/patches.txt lines that make it work:
  - the lobby's receptionist ("ttower": Single, Double, Knockout or Mixed challenge, a clock from that moment) and the
    door up to 1F, open while a challenge is on
  - each floor's trainers for each challenge (FireRed's sets: a Single floor's trainer with three Pokemon, a Double
    floor's pair as one double battle, a Knockout floor's three trainers' lead Pokemon one after another), at FireRed's
    places, levelled to your party ("fightopts = scale [double]"), only there during their challenge
  - the stairs up, shut until that floor's trainer for your challenge is beaten (block ... tt:1F:single,tt:1F:double,...: any of them)
  - the owner on the roof ("ttfinish"): your time, the record, the prize; the elevator back down

    frlg_trainer_tower.py <pokefirered> > patch.txt
"""
import re
import sys

SRC = open(sys.argv[1].rstrip("/") + "/src/trainer_tower_sets.c").read()
TYPES = ["single", "double", "knockout", "mixed"]
WORDS = {"EXCL": "!", "EXCL_EXCL": "!!", "QUES": "?", "QUES_EXCL": "?!", "ELLIPSIS": "...", "ELLIPSIS_EXCL": "...!",
         "ELLIPSIS_ELLIPSIS_ELLIPSIS": "...", "DASH": "-", "DASH_DASH_DASH": "---", "UH_OH": "uh-oh", "YOU_RE": "you're",
         "I_M": "I'm", "IT_S": "it's", "DON_T": "don't", "CAN_T": "can't", "THAT_S": "that's", "WON_T": "won't",
         "I": "I", "POKEMON": "Pokemon", "POKEMON2": "Pokemon", "TRAINER": "Trainer", "I_VE": "I've", "I_LL": "I'll",
         "LET_S": "let's", "DIDN_T": "didn't", "AREN_T": "aren't", "WASN_T": "wasn't", "ISN_T": "isn't", "HE_S": "he's",
         "SHE_S": "she's", "WE_RE": "we're", "THEY_RE": "they're", "THERE_S": "there's", "WHAT_S": "what's"}
CLASS = {"PKMN_BREEDER": "Pokemon Breeder", "PSYCHIC_F": "Psychic", "PSYCHIC_M": "Psychic", "COOLTRAINER_F": "Cooltrainer",
         "COOLTRAINER_M": "Cooltrainer", "PKMN_RANGER_F": "Pokemon Ranger", "PKMN_RANGER_M": "Pokemon Ranger",
         "SIS_AND_BRO": "Sis and Bro", "CRUSH_KIN": "Crush Kin", "SWIMMER_M": "Swimmer", "SWIMMER_F": "Swimmer"}
SPRITE = {"YOUNGSTER": "youngster", "BURGLAR": "man", "PSYCHIC_F": "woman_2", "PSYCHIC_M": "man", "COOL_COUPLE": "cooltrainer_m",
          "YOUNG_COUPLE": "boy", "CRUSH_KIN": "crush_girl", "TWINS": "em_twin", "SIS_AND_BRO": "lass", "BIKER": "rocker",
          "CUE_BALL": "rocker", "HIKER": "hiker", "CRUSH_GIRL": "crush_girl", "BLACK_BELT": "black_belt", "CAMPER": "camper",
          "PKMN_BREEDER": "woman_1", "BUG_CATCHER": "bug_catcher", "PKMN_RANGER_F": "cooltrainer_f", "PKMN_RANGER_M": "cooltrainer_m",
          "COOLTRAINER_M": "cooltrainer_m", "COOLTRAINER_F": "cooltrainer_f", "PICNICKER": "picnicker", "AROMA_LADY": "woman_3",
          "SWIMMER_M": "swimmer_m_land", "SWIMMER_F": "swimmer_f_land", "SAILOR": "sailor", "TUBER": "tuber_f", "LADY": "woman_3",
          "FISHERMAN": "fisher", "GENTLEMAN": "gentleman", "JUGGLER": "man", "SUPER_NERD": "poke_maniac", "ROCKER": "rocker",
          "ENGINEER": "worker_m", "SCIENTIST": "scientist", "BEAUTY": "beauty", "LASS": "lass"}
PLACE = {"CHALLENGE_TYPE_SINGLE": (15, 13, "left"), "CHALLENGE_TYPE_DOUBLE": (10, 10, "down"), "CHALLENGE_TYPE_KNOCKOUT": (10, 16, "up")}
SPECIES = {"MR_MIME": "Mr. Mime", "NIDORAN_F": "Nidoran F", "NIDORAN_M": "Nidoran M", "FARFETCHD": "Farfetch'd"}


def words(ws):
    out = ""
    for w in re.findall(r"EC_WORD_(\w+)", ws):
        tail = ""
        for suf, p in (("_QUES_EXCL", "?!"), ("_EXCL_EXCL", "!!"), ("_QUES", "?"), ("_EXCL", "!"), ("_ELLIPSIS", "...")):
            if w.endswith(suf) and w != suf[1:]:
                w, tail = w[:-len(suf)], p
                break
        if w in WORDS:
            t = WORDS[w]
        else:
            t = re.sub(r"N_T(?=_|$)", "N'T", w)
            t = re.sub(r"_(S|RE|VE|LL|M|D)(?=_|$)", r"'\1", t).replace("_", " ").lower()
            t = re.sub(r"\bi\b", "I", t)
        t += tail
        out += t if t in ("!", "!!", "?", "?!", "...", "...!") else (" " if out else "") + t
    out = out.strip()
    return (out[0].upper() + out[1:]) if out else ""


def floors():
    out = {}
    for name, body in re.findall(r"static const struct TrainerTowerFloor (\w+) = \{(.*?)\n\};", SRC, re.S):
        ct = re.search(r"challengeType = (\w+)", body).group(1)
        trs = []
        for n, cls, rest, mons in re.findall(r'\.name = _\("([^"]*)"\),\s*\.facilityClass = FACILITY_CLASS_(\w+)(.*?)\.mons = \{(.*?)\n\t\t\t\}', body, re.S):
            sp = [SPECIES.get(s, s.replace("_", " ").title()) for s in re.findall(r"\.species = SPECIES_(\w+)", mons)]
            say = re.search(r"\.speechBefore = \{([^}]*)\}", rest)
            win = re.search(r"\.speechLose = \{([^}]*)\}", rest)   # (what they say when they lose)
            trs.append((n.title().replace("&", " & ").replace("  ", " "), cls, sp, words(say.group(1)) if say else "", words(win.group(1)) if win else ""))
        out[name] = (ct, trs)
    table = re.search(r"gTrainerTowerFloors\[.*?\] = \{(.*?)\n\};", SRC, re.S).group(1)
    sets = [re.findall(r"&(\w+)", b) for b in re.findall(r"\] = \{(.*?)\}", table, re.S)]
    return out, sets


def main():
    fl, sets = floors()
    P = ["", "# ------------------------------------------------------------------ the Trainer Tower (tools/frlg_trainer_tower.py)",
         "[patch Trainer Tower]", "drop npc 11 7", "npc = 11 7 union_room_receptionist left 0 ttower",
         "drop door 17 8", "tile 17 8 #",
         "door = 9 2 warp|Trainer Tower 1F|4|7|down", "block = 9 2 tt:on|Please register for a challenge at the counter first."]
    for n in range(1, 9):
        P.append("[patch Trainer Tower %dF]" % n)
        if n > 1:
            P.append("drop door 13 5")
        beats = []
        for t, ty in enumerate(TYPES):
            ct, trs = fl[sets[t][n - 1]]
            x, y, face = PLACE[ct]
            cls = CLASS.get(trs[0][1], trs[0][1].replace("_", " ").title())
            if ct == "CHALLENGE_TYPE_SINGLE":
                nm, team, say, lose = cls + " " + trs[0][0], trs[0][2][:3], trs[0][3], trs[0][4]
            elif ct == "CHALLENGE_TYPE_DOUBLE":
                nm, team, say, lose = cls + " " + trs[0][0], trs[0][2][:2] + trs[1][2][:2], trs[0][3], trs[1][4] or trs[0][4]
            else:
                nm = ", ".join(tr[0] for tr in trs[:-1]) + " & " + trs[-1][0]
                team, say, lose = [tr[2][0] for tr in trs], trs[0][3], trs[-1][4]
            nm = nm.replace("|", "-")
            flag = "tt:%dF:%s" % (n, ty)   # (its own win flag: the names have "&" and "," in them, which conditions read)
            P.append("npc = %d %d %s %s 0 fight|%s|%s|%s|%s|%s" % (x, y, SPRITE.get(trs[0][1], "cooltrainer_m"), face, nm,
                                                                ", ".join("%s 50" % s for s in team), say or "Let's battle!", lose or "You're strong!", flag))
            P.append("fightopts = scale" + (" double" if ct == "CHALLENGE_TYPE_DOUBLE" else ""))
            P.append("if = tt:" + ty)
            beats.append(flag)
        P.append("block = 14 6 %s|The way up opens once you've beaten this floor's Trainers!" % ",".join(beats))
    P += ["[patch Trainer Tower Roof]", "drop npc 9 7", "npc = 9 7 trainer_tower_dude down 0 ttfinish",
          "drop door 13 5", "door = 13 5 warp|Trainer Tower|17|9|down"]
    print("\n".join(P))


if __name__ == "__main__":
    main()
