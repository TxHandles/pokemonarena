// Abilities in battle. Each one is the Pokemon games' ability translated to real-time fights;
// abilities.cpp has the list and descriptions.
#include <algorithm>
#include <cmath>
#include "rng.h"
#include "audio.h"
#include "battle.h"
#include "data.h"
#include "items.h"
#include "ui.h"
#include "vec.h"

namespace {

float Frand(float a, float b) { return a + (b - a) * (float)SimRand(0, 10000) / 10000.0f; }
bool Chance(float p) { return p > 0 && Frand(0, 1) < p; }
bool HasT(const SpeciesDef* sp, Type t) { return sp->type1 == t || sp->type2 == t; }
const char* SName(StatId s) { return s == StatId::Atk ? "ATK" : (s == StatId::Def ? "DEF" : "SPD"); }
const Color kAbCol{255, 225, 130, 255};
bool Here(const Fighter& f) { return (f.state == FState::Active || f.state == FState::Entering) && f.hp > 0; }

void Buff(Fighter& f, StatId s, float mul, float t) {
    f.buffMul[(int)s] = std::max(f.buffMul[(int)s], mul);
    f.buffT[(int)s] = std::max(f.buffT[(int)s], t);
}

} // namespace

void Battle::InitAbility(Fighter& f, int slot) {
    if (slot < 0) {
        bool hidden = !AllSpecies()[f.species].hiddenAbility.empty() && SimRand(1, 20) == 1;
        slot = hidden ? 2 : (AbilitySlotCount(f.species) > 1 ? SimRand(0, 1) : 0);
    }
    f.abSlot = slot;
    f.ab = AbilityByName(SpeciesAbilityName(f.species, slot));
}

Ab Battle::AbOf(const Fighter& f) const {
    if (gasOn && f.ab != Ab::NeutralizingGas) return Ab::None;
    return f.ab;
}

bool Battle::AbOnField(Ab a, int except, int foesOf) const {
    for (int k = 0; k < N(); k++) {
        if (k == except || (foesOf >= 0 && !Foes(foesOf, k))) continue;
        const Fighter& f = sides[k].Cur();
        if (Here(f) && AbOf(f) == a) return true;
    }
    return false;
}

void Battle::AbRefresh() {
    gasOn = false;
    for (int k = 0; k < N(); k++) {
        const Fighter& f = sides[k].Cur();
        if (Here(f) && f.ab == Ab::NeutralizingGas) gasOn = true;
    }
    cloudNine = AbOnField(Ab::CloudNine);
}

void Battle::AbPopup(int s, const std::string& text) {
    const Fighter& f = sides[s].Cur();
    AddText(f.pos, "[" + std::string(AbilityName(f.ab)) + "] " + text, kAbCol, 50);
}

// ---------------------------------------------------------------- Entering and leaving

void Battle::AbOnEnter(int s) {
    AbRefresh();
    Fighter& f = sides[s].Cur();
    Ab a = AbOf(f);
    std::vector<int> foes = FoeSlots(s);
    switch (a) {
        case Ab::Intimidate: {
            AbPopup(s, f.sp->name + " intimidates its foes!");
            for (int ts : foes) {
                Fighter& t = sides[ts].Cur();
                if (!Here(t) || AbBlocksLower(ts, StatId::Atk, s, "Intimidate")) continue;
                t.dropMul[0] = 0.67f;
                t.dropT[0] = 8;
                AddText(t.pos, "ATK fell!", UI_DIM, 36);
                AbLowered(ts, s);
            }
        } break;
        case Ab::Drought:
            weather = Weather::Sun;
            weatherT = 10;
            weatherTick = 1.0f;
            AbPopup(s, "The sunlight turned harsh!");
            break;
        case Ab::Drizzle:
            weather = Weather::Rain;
            weatherT = 10;
            weatherTick = 1.0f;
            AbPopup(s, "It started to rain!");
            break;
        case Ab::SandStream:
            weather = Weather::Sand;
            weatherT = 10;
            weatherTick = 1.0f;
            AbPopup(s, "A sandstorm kicked up!");
            break;
        case Ab::ShadowTag: AbPopup(s, "Its foes are caught in its shadow!"); break;
        case Ab::Download:
            Buff(f, StatId::Atk, 1.4f, 10);
            AbPopup(s, "ATK rose!");
            break;
        case Ab::Trace: {
            for (int ts : foes) {
                const Fighter& t = sides[ts].Cur();
                Ab ta = t.ab;
                if (!Here(t) || ta == Ab::None || ta == Ab::Trace || ta == Ab::Imposter || ta == Ab::NeutralizingGas) continue;
                f.ab = ta;
                AddText(f.pos, f.sp->name + " traced " + t.sp->name + "'s " + AbilityName(ta) + "!", kAbCol, 50);
                AbOnEnter(s);
                break;
            }
        } break;
        case Ab::Imposter: {
            int ts = sides[s].target >= 0 ? sides[s].target : AnyFoe(s);
            if (ts >= 0 && ts < N() && Here(sides[ts].Cur()) && f.transformFrom < 0) DoTransform(s, ts, true);
        } break;
        case Ab::Frisk:
            for (int ts : foes) {
                const Fighter& t = sides[ts].Cur();
                if (Here(t) && t.item) AbPopup(s, t.sp->name + " is holding " + t.item->name + ".");
            }
            break;
        case Ab::Forewarn: {
            const MoveDef* best = nullptr;
            for (int ts : foes) {
                const Fighter& t = sides[ts].Cur();
                if (!Here(t)) continue;
                for (int i = 0; i < t.nMoves; i++)
                    if (!best || t.moves[i].power > best->power) best = &t.moves[i];
            }
            if (best && best->power > 0) AbPopup(s, "It senses " + best->name + "!");
        } break;
        case Ab::Anticipation: {
            bool danger = false;
            for (int ts : foes) {
                const Fighter& t = sides[ts].Cur();
                if (!Here(t)) continue;
                for (int i = 0; i < t.nMoves; i++)
                    danger |= t.moves[i].power > 0 && TypeEffectiveness(t.moves[i].type, f.sp->type1, f.sp->type2) >= 2;
            }
            if (danger) AbPopup(s, f.sp->name + " shuddered!");
        } break;
        case Ab::Pressure: AbPopup(s, f.sp->name + " is exerting its Pressure!"); break;
        case Ab::Unnerve: AbPopup(s, "Its foes are too nervous to snack!"); break;
        case Ab::NeutralizingGas: AbPopup(s, "Neutralizing gas filled the area!"); break;
        case Ab::CloudNine: AbPopup(s, "The effects of weather disappeared."); break;
        case Ab::MoldBreaker: AbPopup(s, f.sp->name + " breaks the mold!"); break;
        case Ab::Illuminate: AbPopup(s, "The arena lit up!"); break;
        default: break;
    }
}

void Battle::AbOnLeave(int s) {
    Fighter& f = sides[s].Cur();
    Ab a = AbOf(f);
    if (a == Ab::NaturalCure && f.st.major != Status::None) f.st.major = Status::None;
    if (a == Ab::Regenerator && f.hp > 0) f.hp = std::min((float)f.maxHp, f.hp + f.maxHp / 3.0f);
}

// ---------------------------------------------------------------- Every frame

void Battle::AbTick(int s, float dt) {
    Fighter& f = sides[s].Cur();
    f.sinceAct += dt;
    f.abSpd = f.abAtk = f.abDef = 1;
    Ab a = AbOf(f);
    if (a == Ab::None) return;
    Weather w = Wx();
    bool status = f.st.major != Status::None;
    switch (a) {
        case Ab::Chlorophyll: if (w == Weather::Sun) f.abSpd = 1.5f; break;
        case Ab::SwiftSwim: if (w == Weather::Rain) f.abSpd = 1.5f; break;
        case Ab::SandRush: if (w == Weather::Sand) f.abSpd = 1.5f; break;
        case Ab::QuickFeet: if (status) f.abSpd = f.st.major == Status::Paralyze ? 1.5f / 0.55f : 1.5f; break;
        case Ab::Unburden: if (f.focusUsed) f.abSpd = 1.6f; break;
        case Ab::Guts: if (status) f.abAtk = f.st.major == Status::Burn ? 1.5f / 0.75f : 1.5f; break;
        case Ab::Hustle: f.abAtk = 1.5f; break;
        case Ab::HugePower: f.abAtk = 2.0f; break;
        case Ab::LightMetal: f.abSpd = 1.1f; break;
        case Ab::MarvelScale: if (status) f.abDef = 1.5f; break;
        case Ab::EarlyBird: if (f.st.major == Status::Sleep) f.st.majorT -= dt; break;
        default: break;
    }
    auto heal = [&](float frac) {
        float amt = std::min((float)f.maxHp - f.hp, f.maxHp * frac);
        if (amt >= 1) {
            f.hp += amt;
            AddText(f.pos, "+" + std::to_string((int)amt), Color{120, 240, 130, 255}, 24);
        }
    };
    // Snacks (Gluttony, Harvest) once HP drops below half.
    if ((a == Ab::Gluttony || a == Ab::Harvest) && !f.snackUsed && f.hp < f.maxHp * 0.5f && !AbOnField(Ab::Unnerve, -1, s)) {
        f.snackUsed = true;
        AbPopup(s, f.sp->name + " ate a berry!");
        heal(a == Ab::Gluttony ? 0.2f : 0.15f);
    }
    // Pinch abilities announce themselves once
    if ((a == Ab::Blaze || a == Ab::Torrent || a == Ab::Overgrow || a == Ab::Swarm) && !f.pinchShown && f.hp > 0 && f.hp <= f.maxHp / 3.0f) {
        f.pinchShown = true;
        AbPopup(s, "Its power surges!");
        Burst(f.pos, AbilityInfo(a).name[0] == 'B' ? Color{255, 120, 40, 255} : (a == Ab::Torrent ? Color{80, 150, 255, 255} : (a == Ab::Swarm ? Color{170, 210, 60, 255} : Color{90, 210, 80, 255})), 18, 60, 8);
    }
    f.abT += dt;
    if (f.abT < 1.0f) return;
    f.abT -= 1.0f;
    int os = f.lastHitBy >= 0 && f.lastHitBy < N() && Foes(s, f.lastHitBy) ? f.lastHitBy : AnyFoe(s);
    switch (a) {
        case Ab::RainDish: if (w == Weather::Rain) heal(0.04f); break;
        case Ab::IceBody: if (w == Weather::Snow) heal(0.04f); break;
        case Ab::DrySkin:
            if (w == Weather::Rain) heal(0.04f);
            else if (w == Weather::Sun) DirectDamage(s, f.maxHp * 0.03f, Color{255, 160, 80, 255}, os);
            break;
        case Ab::SolarPower: if (w == Weather::Sun) DirectDamage(s, f.maxHp * 0.025f, Color{255, 160, 80, 255}, os); break;
        case Ab::Hydration:
            if (w == Weather::Rain && status) { f.st.major = Status::None; AbPopup(s, "The rain washed its status away!"); }
            break;
        case Ab::ShedSkin:
            if (status && Chance(0.25f)) { f.st.major = Status::None; AbPopup(s, f.sp->name + " shed its skin!"); }
            break;
        case Ab::Healer:
            for (int k = 0; k < N(); k++) {
                if (k == s || sides[k].teamId != sides[s].teamId) continue;
                Fighter& ally = sides[k].Cur();
                if (Here(ally) && ally.st.major != Status::None && Chance(0.3f)) {
                    ally.st.major = Status::None;
                    AbPopup(s, "It healed " + ally.sp->name + "!");
                }
            }
            break;
        case Ab::SpeedBoost:
            if (Chance(0.25f) && f.buffMul[2] < 2.0f) {
                f.buffMul[2] = std::min(2.0f, std::max(1.0f, f.buffMul[2]) + 0.15f);
                f.buffT[2] = std::max(f.buffT[2], 30.0f);
                AbPopup(s, "SPD rose!");
            }
            break;
        case Ab::Moody:
            if (Chance(0.2f)) {
                int up = SimRand(0, 2), down = (up + SimRand(1, 2)) % 3;
                Buff(f, (StatId)up, 1.6f, 8);
                f.dropMul[down] = std::min(f.dropMul[down], 0.75f);
                f.dropT[down] = std::max(f.dropT[down], 8.0f);
                AbPopup(s, std::string(SName((StatId)up)) + " sharply rose, " + SName((StatId)down) + " fell!");
            }
            break;
        case Ab::Harvest:
            if (f.snackUsed && Chance(w == Weather::Sun ? 1.0f : 0.15f)) { f.snackUsed = false; AbPopup(s, "It grew a new berry!"); }
            break;
        default: break;
    }
}

// ---------------------------------------------------------------- Hits

bool Battle::AbBlocksHit(const HitSpec& h, int ts) {
    Fighter& t = sides[ts].Cur();
    Ab ta = h.ab == Ab::MoldBreaker ? Ab::None : AbOf(t);
    auto unaffected = [&](const std::string& why) {
        if (!h.light) AddText(t.pos, "[" + std::string(AbilityName(t.ab)) + "] " + why, kAbCol, 40);
        t.flash = 0.05f;
        return true;
    };
    auto absorb = [&](float frac) {
        float amt = std::min((float)t.maxHp - t.hp, t.maxHp * (h.light ? frac * 0.15f : frac));
        if (amt >= 1) { t.hp += amt; AddText(t.pos, "+" + std::to_string((int)amt), Color{120, 240, 130, 255}, 24); }
        if (!h.light) AddText(t.pos, "[" + std::string(AbilityName(t.ab)) + "] absorbed it!", kAbCol, 40);
        return true;
    };
    switch (ta) {
        case Ab::Levitate: if (h.type == Type::Ground && h.name != "Sand Attack") return unaffected("It floats above it!"); break;
        case Ab::VoltAbsorb: if (h.type == Type::Electric) return absorb(0.25f); break;
        case Ab::WaterAbsorb: case Ab::DrySkin: if (h.type == Type::Water) return absorb(0.25f); break;
        case Ab::LightningRod:
            if (h.type == Type::Electric) {
                if (!h.light) Buff(t, StatId::Atk, 1.5f, 8);
                return unaffected("It drew in the electricity! ATK rose!");
            }
            break;
        case Ab::FlashFire:
            if (h.type == Type::Fire) {
                bool was = t.flashFire;
                t.flashFire = true;
                return unaffected(was ? "It absorbed the fire!" : "Its Fire moves powered up!");
            }
            break;
        case Ab::SapSipper:
            if (h.type == Type::Grass) {
                if (!h.light) Buff(t, StatId::Atk, 1.5f, 8);
                return unaffected("It ate the leaves! ATK rose!");
            }
            break;
        case Ab::MagicBounce:
            if (h.power <= 0 && h.fixedDmg == 0 && !h.light && h.side >= 0 && h.side < N() && h.side != ts && Foes(h.side, ts)) {
                if (h.status != Status::None && sides[h.side].active == h.fighter) TryStatus(h.side, h.status, ts);
                return unaffected("It bounced the move back!");
            }
            break;
        case Ab::Telepathy:
            if (h.side >= 0 && h.side < N() && h.side != ts && !Foes(h.side, ts)) return unaffected("It saw its partner's attack coming!");
            break;
        case Ab::Soundproof: if (IsSoundMoveName(h.name)) return unaffected("It can't hear it!"); break;
        case Ab::Overcoat: if (IsPowderMoveName(h.name)) return unaffected("The powder can't get through!"); break;
        case Ab::WonderSkin: if (h.power <= 0 && h.fixedDmg == 0 && Chance(0.5f)) return unaffected("It wasn't affected!"); break;
        case Ab::Sturdy: if (h.ohko) return unaffected("It can't be knocked out in one hit!"); break;
        case Ab::SandVeil: case Ab::SnowCloak:
            if (!h.light && h.ab != Ab::NoGuard && h.ab != Ab::KeenEye && Wx() == (ta == Ab::SandVeil ? Weather::Sand : Weather::Snow) && Chance(0.2f)) {
                AddText(t.pos, "Missed!", UI_DIM, 30);
                return true;
            }
            break;
        case Ab::TangledFeet:
            if (!h.light && t.st.confuseT > 0 && h.ab != Ab::NoGuard && Chance(0.35f)) { AddText(t.pos, "Missed!", UI_DIM, 30); return true; }
            break;
        default: break;
    }
    if (h.ab == Ab::Hustle && !h.light && !h.special && h.power > 0 && ta != Ab::NoGuard && Chance(0.12f)) {
        AddText(t.pos, "Missed!", UI_DIM, 30);
        return true;
    }
    return false;
}

float Battle::AbDamageMul(const HitSpec& h, int ts, float eff, bool crit) const {
    const Fighter& t = sides[ts].Cur();
    bool ignore = h.ab == Ab::MoldBreaker;
    Ab ta = ignore ? Ab::None : AbOf(t);
    float m = 1;
    switch (ta) {
        case Ab::ThickFat: if (h.type == Type::Fire || h.type == Type::Ice) m *= 0.5f; break;
        case Ab::Filter: if (eff > 1) m *= 0.75f; break;
        case Ab::Multiscale: if (t.hp >= t.maxHp) m *= 0.5f; break;
        case Ab::DrySkin: if (h.type == Type::Fire) m *= 1.25f; break;
        case Ab::Unaware: if (h.atkStage > 0) m /= h.atkStage; break;
        default: break;
    }
    if (!ignore)
        for (int k = 0; k < N(); k++)
            if (k != ts && sides[k].teamId == sides[ts].teamId && Here(sides[k].Cur()) && AbOf(sides[k].Cur()) == Ab::FriendGuard) m *= 0.75f;
    switch (h.ab) {
        case Ab::TintedLens: if (eff < 1) m *= 2; break;
        case Ab::Sniper: if (crit) m *= 1.5f; break;
        case Ab::Analytic: if (t.sinceAct < 1.0f) m *= 1.3f; break;
        case Ab::Rivalry:
            if (HasT(t.sp, h.t1) || (h.t2 != Type::None && HasT(t.sp, h.t2))) m *= 1.25f;
            break;
        case Ab::Adaptability: if (h.type == h.t1 || h.type == h.t2) m *= 2.0f / 1.5f; break;
        case Ab::Unaware: m *= t.buffMul[1] * t.dropMul[1]; break;
        default: break;
    }
    return m;
}

void Battle::AbAfterHit(const HitSpec& h, int ts, float dmg, bool crit, float hpBefore) {
    Fighter& t = sides[ts].Cur();
    Ab ta = AbOf(t);
    if (ta == Ab::Sturdy && h.ab != Ab::MoldBreaker && hpBefore >= t.maxHp && t.hp <= 0 && dmg > 0) {
        t.hp = 1;
        AbPopup(ts, t.sp->name + " endured the hit!");
    }
    if (dmg <= 0 || h.light) return;
    Fighter* a = nullptr;
    if (h.side >= 0 && h.side < N() && sides[h.side].active == h.fighter && sides[h.side].Cur().state == FState::Active)
        a = &sides[h.side].Cur();
    if (h.contact && a) {
        switch (ta) {
            case Ab::Static: if (a->st.major == Status::None && Chance(0.3f)) { AbPopup(ts, "Zap!"); TryStatus(h.side, Status::Paralyze, ts); } break;
            case Ab::PoisonPoint: if (a->st.major == Status::None && Chance(0.3f)) { AbPopup(ts, "Ouch!"); TryStatus(h.side, Status::Poison, ts); } break;
            case Ab::FlameBody: if (a->st.major == Status::None && Chance(0.3f)) { AbPopup(ts, "Hot!"); TryStatus(h.side, Status::Burn, ts); } break;
            case Ab::EffectSpore:
                if (a->st.major == Status::None && Chance(0.3f) && !HasT(a->sp, Type::Grass) && AbOf(*a) != Ab::Overcoat) {
                    int r = SimRand(0, 2);
                    AbPopup(ts, "Spores!");
                    TryStatus(h.side, r == 0 ? Status::Poison : (r == 1 ? Status::Paralyze : Status::Sleep), ts);
                }
                break;
            case Ab::CuteCharm: if (a->st.confuseT <= 0 && Chance(0.3f)) { AbPopup(ts, "So cute..."); TryStatus(h.side, Status::Confuse, ts); } break;
            case Ab::StickyHold:
                a->dropMul[2] = 0.6f;
                a->dropT[2] = std::max(a->dropT[2], 2.0f);
                break;
            case Ab::Aftermath:
                if (t.hp <= 0 && !AbOnField(Ab::Damp)) {
                    AbPopup(ts, "It exploded as it fainted!");
                    DirectDamage(h.side, a->maxHp * 0.25f, Color{255, 160, 90, 255}, ts);
                }
                break;
            case Ab::Pickpocket:
                if (t.hp > 0 && !t.item && a->item && !IsBall(*a->item)) {
                    t.item = a->item;
                    a->item = nullptr;
                    AbPopup(ts, "It stole " + a->sp->name + "'s " + t.item->name + "!");
                }
                break;
            case Ab::CursedBody:
                if (Chance(0.3f) && h.moveIdx >= 0 && h.moveIdx < 4) {
                    a->cd[h.moveIdx] = std::max(a->cd[h.moveIdx], 4.0f);
                    AbPopup(ts, a->moves[h.moveIdx].name + " was disabled!");
                }
                break;
            default: break;
        }
        a = (h.side >= 0 && sides[h.side].active == h.fighter && sides[h.side].Cur().state == FState::Active) ? &sides[h.side].Cur() : nullptr;
    }
    if (h.contact && h.ab == Ab::PoisonTouch && t.hp > 0 && t.st.major == Status::None && Chance(0.3f)) {
        if (h.side >= 0 && h.side < N() && sides[h.side].active == h.fighter) AbPopup(h.side, "Poison!");
        TryStatus(ts, Status::Poison, h.side);
    }
    if (t.hp > 0) {
        switch (ta) {
            case Ab::AngerPoint: if (crit) { Buff(t, StatId::Atk, 2.0f, 10); AbPopup(ts, "Its ATK maxed out!"); } break;
            case Ab::Justified: if (h.type == Type::Dark) { Buff(t, StatId::Atk, 1.5f, 8); AbPopup(ts, "ATK rose!"); } break;
            case Ab::Rattled:
                if (h.type == Type::Bug || h.type == Type::Dark || h.type == Type::Ghost) { Buff(t, StatId::Spd, 1.5f, 6); AbPopup(ts, "SPD rose!"); }
                break;
            case Ab::WeakArmor:
                if (!h.special) {
                    t.dropMul[1] = 0.67f;
                    t.dropT[1] = 6;
                    Buff(t, StatId::Spd, 1.6f, 6);
                    AbPopup(ts, "DEF fell, SPD rose!");
                }
                break;
            default: break;
        }
    }
}

// ---------------------------------------------------------------- Status and stat changes

bool Battle::AbBlocksStatus(int ts, Status st, int src) {
    Fighter& t = sides[ts].Cur();
    Ab a = AbOf(t);
    if (src >= 0 && src < N() && Foes(src, ts) && AbOf(sides[src].Cur()) == Ab::MoldBreaker) a = Ab::None;
    bool block = false;
    switch (a) {
        case Ab::Insomnia: case Ab::VitalSpirit: block = st == Status::Sleep; break;
        case Ab::Limber: block = st == Status::Paralyze; break;
        case Ab::Immunity: block = st == Status::Poison || st == Status::Toxic; break;
        case Ab::WaterVeil: block = st == Status::Burn; break;
        case Ab::MagmaArmor: block = st == Status::Freeze; break;
        case Ab::OwnTempo: case Ab::Oblivious: block = st == Status::Confuse; break;
        case Ab::InnerFocus: block = st == Status::Flinch; break;
        case Ab::LeafGuard: block = IsMajorStatus(st) && Wx() == Weather::Sun; break;
        default: break;
    }
    if (block && st != Status::Flinch) AddText(t.pos, "[" + std::string(AbilityName(t.ab)) + "] It's protected!", kAbCol, 40);
    return block;
}

bool Battle::AbBlocksLower(int ts, StatId stat, int src, const std::string& moveName) {
    if (src < 0 || src >= N() || !Foes(src, ts)) return false;
    Fighter& t = sides[ts].Cur();
    Ab a = AbOf(sides[src].Cur()) == Ab::MoldBreaker && moveName != "Intimidate" ? Ab::None : AbOf(t);
    if (a == Ab::Contrary && moveName != "Intimidate") {   // the drop becomes a boost
        Buff(t, stat, 1.5f, 8);
        AddText(t.pos, "[Contrary] " + std::string(SName(stat)) + " rose instead!", kAbCol, 40);
        return true;
    }
    bool block = false;
    switch (a) {
        case Ab::ClearBody: block = true; break;
        case Ab::HyperCutter: block = stat == StatId::Atk; break;
        case Ab::BigPecks: block = stat == StatId::Def; break;
        case Ab::KeenEye: block = IsAccuracyMoveName(moveName); break;
        case Ab::InnerFocus: case Ab::Oblivious: case Ab::OwnTempo: case Ab::Scrappy: block = moveName == "Intimidate"; break;
        default: break;
    }
    if (block) AddText(t.pos, "[" + std::string(AbilityName(t.ab)) + "] " + SName(stat) + " can't be lowered!", kAbCol, 40);
    return block;
}

void Battle::AbLowered(int ts, int src) {
    if (src < 0 || src >= N() || !Foes(src, ts)) return;
    Fighter& t = sides[ts].Cur();
    Ab a = AbOf(t);
    if (a == Ab::Defiant || a == Ab::Competitive) {
        Buff(t, StatId::Atk, 1.6f, 8);
        AbPopup(ts, "ATK sharply rose!");
    }
}

// ---------------------------------------------------------------- Using moves

void Battle::AbSpec(HitSpec& h, const Fighter& f, const MoveDef& m) const {
    Ab a = AbOf(f);
    h.ab = a;
    h.atkStage = f.buffMul[0] * f.dropMul[0];
    if (a == Ab::None) return;
    float mul = 1;
    float frac = f.hp / std::max(1.0f, (float)f.maxHp);
    bool pinch = frac <= 1.0f / 3.0f;
    switch (a) {
        case Ab::Overgrow: if (pinch && m.type == Type::Grass) mul = 1.5f; break;
        case Ab::Blaze: if (pinch && m.type == Type::Fire) mul = 1.5f; break;
        case Ab::Torrent: if (pinch && m.type == Type::Water) mul = 1.5f; break;
        case Ab::Swarm: if (pinch && m.type == Type::Bug) mul = 1.5f; break;
        case Ab::Technician: if (m.power > 0 && m.power <= 60) mul = 1.5f; break;
        case Ab::IronFist: if (IsPunchMoveName(m.name)) mul = 1.2f; break;
        case Ab::Reckless: if (m.recoil > 0) mul = 1.2f; break;
        case Ab::RockHead: h.recoil = 0; break;
        case Ab::FlashFire: if (f.flashFire && m.type == Type::Fire) mul = 1.5f; break;
        case Ab::SandForce: if (Wx() == Weather::Sand && (m.type == Type::Rock || m.type == Type::Ground || m.type == Type::Steel)) mul = 1.3f; break;
        case Ab::SolarPower: if (Wx() == Weather::Sun && h.special) mul = 1.5f; break;
        case Ab::SheerForce:
            if (m.power > 0 && ((m.status != Status::None && m.statusChance < 1) || (m.lowers && m.lowerChance < 1))) {
                mul = 1.3f;
                h.statusChance = 0;
                h.lowers = false;
                h.flinchChance = 0;
                h.effects = false;
            }
            break;
        case Ab::SereneGrace:
            h.statusChance = std::min(1.0f, h.statusChance * 2);
            h.lowerChance = std::min(1.0f, h.lowerChance * 2);
            h.flinchChance = std::min(1.0f, h.flinchChance * 2);
            break;
        case Ab::Stench: if (m.power > 0) h.flinchChance = std::max(h.flinchChance, 0.1f); break;
        case Ab::Plus:
            if (h.special && m.power > 0) {
                int me = -1;
                for (int k = 0; k < N(); k++) if (&sides[k].Cur() == &f) me = k;
                for (int k = 0; me >= 0 && k < N(); k++)
                    if (k != me && sides[k].teamId == sides[me].teamId && Here(sides[k].Cur()) && AbOf(sides[k].Cur()) == Ab::Plus) mul = 1.5f;
            }
            break;
        default: break;
    }
    h.dmgMul *= mul;
}

void Battle::AbKo(int k) {
    if (k < 0 || k >= N()) return;
    Fighter& f = sides[k].Cur();
    if (!Here(f)) return;
    Ab a = AbOf(f);
    if (a == Ab::Moxie) { Buff(f, StatId::Atk, 1.5f, 10); AbPopup(k, "ATK rose!"); }
    if (a == Ab::HoneyGather && sides[k].teamId == 0) {
        int c = std::max(3, f.level / 2);
        payDayCoins += c;
        AbPopup(k, "It gathered honey worth " + std::to_string(c) + " coins!");
    }
    if (a == Ab::Pickup && sides[k].teamId == 0) {
        int c = std::max(5, f.level);
        payDayCoins += c;
        AbPopup(k, "It picked up " + std::to_string(c) + " coins!");
    }
}

void Battle::DoTransform(int s, int ts, bool announce) {
    Fighter& f = sides[s].Cur();
    const Fighter& t = sides[ts].Cur();
    if (f.transformFrom < 0) {
        f.transformFrom = f.species;
        f.origMoves = f.moves;
        f.origN = f.nMoves;
    }
    float frac = f.maxHp > 0 ? f.hp / f.maxHp : 1.0f;
    std::string from = f.sp->name;
    f.species = t.species;
    f.sp = t.sp;
    f.moves = t.moves;
    f.nMoves = t.nMoves;
    f.ab = t.ab == Ab::Imposter ? Ab::None : t.ab;
    for (int i = 0; i < 4; i++) f.cd[i] = 0.6f;
    f.RecalcHp(false);
    f.hp = std::max(1.0f, f.maxHp * frac);
    f.flash = 0.3f;
    Burst(f.pos, Color{200, 150, 255, 255}, 24, 90, 8);
    AddText(f.pos, (announce ? "[Imposter] " : "") + from + " transformed into " + t.sp->name + "!", Color{200, 150, 255, 255}, 50);
    PlaySfx(Sfx::Status);
}

void Battle::AbilityMove(int s, int ts, int kind) {
    Fighter& f = sides[s].Cur();
    Fighter& t = sides[ts].Cur();
    auto fixed = [](Ab a) { return a == Ab::NeutralizingGas || a == Ab::Imposter; };
    const Color c{255, 225, 130, 255};
    switch (kind) {
        case 1:   // Skill Swap
            if (fixed(f.ab) || fixed(t.ab)) { AddText(f.pos, "But it failed!", UI_DIM, 30); return; }
            std::swap(f.ab, t.ab);
            AddText(f.pos, f.sp->name + " swapped abilities! Now: " + AbilityName(f.ab), c, 50);
            AbOnEnter(s);
            AbOnEnter(ts);
            break;
        case 2:   // Entrainment
            if (fixed(t.ab) || f.ab == Ab::None) { AddText(f.pos, "But it failed!", UI_DIM, 30); return; }
            t.ab = f.ab;
            AddText(t.pos, t.sp->name + " acquired " + AbilityName(t.ab) + "!", c, 50);
            AbOnEnter(ts);
            break;
        case 3:   // Simple Beam: here, the target's ability is switched off
            if (fixed(t.ab)) { AddText(f.pos, "But it failed!", UI_DIM, 30); return; }
            t.ab = Ab::None;
            AddText(t.pos, t.sp->name + "'s ability was suppressed!", c, 50);
            break;
        case 4:   // Doodle
            if (fixed(t.ab) || t.ab == Ab::None) { AddText(f.pos, "But it failed!", UI_DIM, 30); return; }
            f.ab = t.ab;
            AddText(f.pos, f.sp->name + " copied " + AbilityName(f.ab) + "!", c, 50);
            AbOnEnter(s);
            break;
        case 5:   // Psycho Shift
            if (f.st.major == Status::None || t.st.major != Status::None) { AddText(f.pos, "But it failed!", UI_DIM, 30); return; }
            {
                Status st = f.st.major;
                f.st.major = Status::None;
                TryStatus(ts, st, s);
            }
            break;
        default: break;
    }
}
