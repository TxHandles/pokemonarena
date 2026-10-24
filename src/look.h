// Your trainer's look: built at the start of Story Mode from FireRed's people - one person's head and hair,
// another's outfit, with your own colours. Out in the world you walk as it (on the Bicycle, surfing and fishing
// you're Red's or Leaf's poses in your colours).
#pragma once
#include <string>
#include <vector>
#include "raylib.h"

struct TrainerLook {
    int type = 0;            // 0 boy (Red's / Ethan's / Brendan's bike, surf and fishing poses), 1 girl (Leaf's / Lyra's / May's)
    int poses = -1;          // whose poses: 0 FireRed's, 1 HeartGold's, 2 Emerald's (-1: the trainer's own game)
    std::string head, body;  // people on World/chars.txt: whose head and hair, whose outfit
    int hair = 0, cloth = 0, skin = 0;   // colour presets (0 = as drawn)
    bool Empty() const { return head.empty() && body.empty(); }
};

TrainerLook ParseLook(const std::string& s);
int LookPoses(const TrainerLook& l);                  // 0 FireRed, 1 HeartGold, 2 Emerald (an unset one: the trainer's game)
const char* PoseBase(int poses, int type);           // the player whose poses they are: player, leaf, hg_ethan...          // "type=0;head=player;body=lass;hair=2;cloth=5;skin=0"
std::string LookText(const TrainerLook& l);
const std::vector<std::string>& LookHeads();   // the heads and outfits you can pick from (on the people sheet)
const std::vector<std::string>& LookBodies();
const std::vector<std::string>& LookPeople();   // whole trainers (the creator offers only these)
const std::vector<const char*>& LookColourNames();   // the colour presets ("As drawn", "Red", "Brown"...)
const std::vector<const char*>& LookSkinNames();
// The walking sheet (one 144x32 row: down x3, up x3, left x3) and the action sheet (rows: bike, surf, fish; 32x32
// cells), made from World/chars.png and World/actions.png. Cached by look; unloaded with UnloadLooks.
Texture2D LookChars(const TrainerLook& l);
Texture2D LookActions(const TrainerLook& l);
void UnloadLooks();
struct Game;
TrainerLook CreatorLookFor(const Game& g);   // the look on the creator screen right now
