#pragma once
#include <string>

// Sound effects: FireRed / LeafGreen's own (Sounds/<name>.ogg, made by tools/frlg_sounds.py), with small
// synthesized ones for anything that has no file (and as a fallback if the Sounds folder is missing).
enum class Sfx { Shoot, Zap, Fire, Water, Thud, Hit, HitSuper, HitWeak, Crit, Faint, LevelUp, Evolve, Status,
                 Heal, Shield, Buff, MenuMove, MenuOk, MenuBack, Win,
                 BallThrow, BallShake, BallOpen, Caught, Flee, Door, Bump, Ledge, Save, Shiny, EggHatch, Repel, Shop,
                 ItemGet, CenterHeal, Count };
const char* SfxName(Sfx s);   // its file name in Sounds/ ("menu_move")
// A move's own sound (HeartGold / SoulSilver's, Sounds/Moves/<move>.ogg, loaded the first time it's used);
// false if it has none (play the type's sound instead).
bool PlayMoveSfx(const char* move);

void SetSfxSet(const std::string& set);   // a region's own sounds (Sounds/<set>/, e.g. "Hoenn"); "" the usual ones
void InitSfx();                                   // after InitAudioDevice
void UnloadSfx();
void SetSfxVolume(float master, float effects);   // 0..1 each
void PlaySfx(Sfx s);
extern bool gSfxMuted;   // online: replaying battle ticks quietly
