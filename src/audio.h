#pragma once

// Small synthesized sound effects (no audio files needed).
enum class Sfx { Shoot, Zap, Fire, Water, Thud, Hit, HitSuper, HitWeak, Crit, Faint, LevelUp, Evolve, Status,
                 Heal, Shield, Buff, MenuMove, MenuOk, MenuBack, Win, Count };

void InitSfx();                                   // after InitAudioDevice
void UnloadSfx();
void SetSfxVolume(float master, float effects);   // 0..1 each
void PlaySfx(Sfx s);
extern bool gSfxMuted;   // online: replaying battle ticks quietly
