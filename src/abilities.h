#pragma once
#include <cstdint>
#include <string>
#include "types.h"

// Pokemon abilities (every ability used by the first 721), adapted to real-time battles.
enum class Ab : uint8_t {
    None,
    Adaptability,
    Aftermath,
    Analytic,
    AngerPoint,
    Anticipation,
    ArenaTrap,
    BattleArmor,
    BigPecks,
    Blaze,
    Chlorophyll,
    ClearBody,
    CloudNine,
    Competitive,
    CompoundEyes,
    CursedBody,
    CuteCharm,
    Damp,
    Defiant,
    Download,
    Drought,
    DrySkin,
    EarlyBird,
    EffectSpore,
    Filter,
    FlameBody,
    FlashFire,
    Forewarn,
    FriendGuard,
    Frisk,
    Gluttony,
    Guts,
    Harvest,
    Healer,
    Hustle,
    Hydration,
    HyperCutter,
    IceBody,
    Illuminate,
    Immunity,
    Imposter,
    Infiltrator,
    InnerFocus,
    Insomnia,
    Intimidate,
    IronFist,
    Justified,
    KeenEye,
    LeafGuard,
    Levitate,
    LightningRod,
    Limber,
    LiquidOoze,
    MagicGuard,
    MagnetPull,
    MarvelScale,
    MoldBreaker,
    Moxie,
    Multiscale,
    NaturalCure,
    NeutralizingGas,
    NoGuard,
    Oblivious,
    Overcoat,
    Overgrow,
    OwnTempo,
    Pickup,
    PoisonPoint,
    PoisonTouch,
    Pressure,
    QuickFeet,
    RainDish,
    Rattled,
    Reckless,
    Regenerator,
    Rivalry,
    RockHead,
    RunAway,
    SandForce,
    SandRush,
    SandVeil,
    Scrappy,
    SereneGrace,
    ShedSkin,
    SheerForce,
    ShellArmor,
    ShieldDust,
    SkillLink,
    Sniper,
    SnowCloak,
    SolarPower,
    Soundproof,
    Static,
    Steadfast,
    Stench,
    StickyHold,
    Sturdy,
    Swarm,
    SwiftSwim,
    Synchronize,
    TangledFeet,
    Technician,
    ThickFat,
    TintedLens,
    Torrent,
    Trace,
    Unaware,
    Unburden,
    Unnerve,
    VitalSpirit,
    VoltAbsorb,
    WaterAbsorb,
    WaterVeil,
    WeakArmor,
    WonderSkin,
    // Gen 2 additions (kept after the Gen 1 set so older values stay put)
    Contrary,
    Drizzle,
    HoneyGather,
    HugePower,
    LightMetal,
    MagicBounce,
    MagmaArmor,
    Moody,
    Pickpocket,
    Plus,
    Prankster,
    SandStream,
    SapSipper,
    ShadowTag,
    SpeedBoost,
    SuctionCups,
    SuperLuck,
    Telepathy,
    // Gen 3
    HeavyMetal, Minus, Normalize, PoisonHeal, Protean, Simple, SolidRock, Stall, StormDrain, ToxicBoost, WindRider,
    AirLock, ColorChange, Forecast, PurePower, RoughSkin, Truant, WhiteSmoke, WonderGuard,
    BadDreams, FlareBoost, FlowerGift, Heatproof, Klutz, MotorDrive, Multitype, Sharpness, SlowStart, SnowWarning,   // Gen 4
    Defeatist, Illusion, IronBarbs, Mummy, SlushRush, Teravolt, Turboblaze, VictoryStar, ZenMode,   // Gen 5
    AromaVeil, AuraBreak, Bulletproof, CheekPouch, DarkAura, FairyAura, FlowerVeil, FurCoat, GaleWings, Gooey, GrassPelt,
    Magician, MegaLauncher, Pixilate, Refrigerate, StanceChange, StrongJaw, SweetVeil, Symbiosis, ToughClaws,   // Gen 6
    Aerilate, ParentalBond, InnardsOut,   // Mega Evolutions (Mega Pinsir, Mega Kangaskhan, Mega Victreebel)
    PrimordialSea, DesolateLand, DeltaStream,   // Primal Kyogre, Primal Groudon, Mega Rayquaza
    Count
};

struct AbilityDef {
    const char* name;
    const char* desc;
};
const AbilityDef& AbilityInfo(Ab a);
Ab AbilityByName(const std::string& name);   // Ab::None if unknown
const char* AbilityName(Ab a);

// Move-name groups some abilities care about.
bool IsSoundMoveName(const std::string& name);
bool IsWindMoveName(const std::string& name);   // Wind Rider
bool IsPunchMoveName(const std::string& name);
bool IsPowderMoveName(const std::string& name);
bool IsAccuracyMoveName(const std::string& name);   // Sand Attack, Smokescreen... (Keen Eye)
// 0 if this ability makes the holder immune to (or absorb) moves of type t, else 1. For the CPU's move choice.
float AbilityTypeMul(Ab a, Type t);
