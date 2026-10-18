#include "types.h"
#include <initializer_list>

namespace {

const char* kNames[TYPE_COUNT] = {
    "Normal", "Fire", "Water", "Grass", "Electric", "Ice", "Fighting", "Poison", "Ground",
    "Flying", "Psychic", "Bug", "Rock", "Ghost", "Dragon", "Dark", "Steel", "Fairy"
};

const Color kColors[TYPE_COUNT] = {
    {168, 168, 120, 255}, // Normal
    {240, 128,  48, 255}, // Fire
    {104, 144, 240, 255}, // Water
    {120, 200,  80, 255}, // Grass
    {248, 208,  48, 255}, // Electric
    {152, 216, 216, 255}, // Ice
    {192,  48,  40, 255}, // Fighting
    {160,  64, 160, 255}, // Poison
    {224, 192, 104, 255}, // Ground
    {168, 144, 240, 255}, // Flying
    {248,  88, 136, 255}, // Psychic
    {168, 184,  32, 255}, // Bug
    {184, 160,  56, 255}, // Rock
    {112,  88, 152, 255}, // Ghost
    {112,  56, 248, 255}, // Dragon
    {112,  88,  72, 255}, // Dark
    {184, 184, 208, 255}, // Steel
    {238, 153, 172, 255}, // Fairy
};

struct Chart {
    float m[TYPE_COUNT][TYPE_COUNT];

    void Set(Type a, std::initializer_list<Type> defenders, float v) {
        for (Type d : defenders) m[(int)a][(int)d] = v;
    }

    Chart() {
        for (int i = 0; i < TYPE_COUNT; i++)
            for (int j = 0; j < TYPE_COUNT; j++) m[i][j] = 1.0f;

        using T = Type;
        Set(T::Normal,   {T::Rock, T::Steel}, 0.5f);
        Set(T::Normal,   {T::Ghost}, 0.0f);

        Set(T::Fire,     {T::Grass, T::Ice, T::Bug, T::Steel}, 2.0f);
        Set(T::Fire,     {T::Fire, T::Water, T::Rock, T::Dragon}, 0.5f);

        Set(T::Water,    {T::Fire, T::Ground, T::Rock}, 2.0f);
        Set(T::Water,    {T::Water, T::Grass, T::Dragon}, 0.5f);

        Set(T::Electric, {T::Water, T::Flying}, 2.0f);
        Set(T::Electric, {T::Electric, T::Grass, T::Dragon}, 0.5f);
        Set(T::Electric, {T::Ground}, 0.0f);

        Set(T::Grass,    {T::Water, T::Ground, T::Rock}, 2.0f);
        Set(T::Grass,    {T::Fire, T::Grass, T::Poison, T::Flying, T::Bug, T::Dragon, T::Steel}, 0.5f);

        Set(T::Ice,      {T::Grass, T::Ground, T::Flying, T::Dragon}, 2.0f);
        Set(T::Ice,      {T::Fire, T::Water, T::Ice, T::Steel}, 0.5f);

        Set(T::Fighting, {T::Normal, T::Ice, T::Rock, T::Dark, T::Steel}, 2.0f);
        Set(T::Fighting, {T::Poison, T::Flying, T::Psychic, T::Bug, T::Fairy}, 0.5f);
        Set(T::Fighting, {T::Ghost}, 0.0f);

        Set(T::Poison,   {T::Grass, T::Fairy}, 2.0f);
        Set(T::Poison,   {T::Poison, T::Ground, T::Rock, T::Ghost}, 0.5f);
        Set(T::Poison,   {T::Steel}, 0.0f);

        Set(T::Ground,   {T::Fire, T::Electric, T::Poison, T::Rock, T::Steel}, 2.0f);
        Set(T::Ground,   {T::Grass, T::Bug}, 0.5f);
        Set(T::Ground,   {T::Flying}, 0.0f);

        Set(T::Flying,   {T::Grass, T::Fighting, T::Bug}, 2.0f);
        Set(T::Flying,   {T::Electric, T::Rock, T::Steel}, 0.5f);

        Set(T::Psychic,  {T::Fighting, T::Poison}, 2.0f);
        Set(T::Psychic,  {T::Psychic, T::Steel}, 0.5f);
        Set(T::Psychic,  {T::Dark}, 0.0f);

        Set(T::Bug,      {T::Grass, T::Psychic, T::Dark}, 2.0f);
        Set(T::Bug,      {T::Fire, T::Fighting, T::Poison, T::Flying, T::Ghost, T::Steel, T::Fairy}, 0.5f);

        Set(T::Rock,     {T::Fire, T::Ice, T::Flying, T::Bug}, 2.0f);
        Set(T::Rock,     {T::Fighting, T::Ground, T::Steel}, 0.5f);

        Set(T::Ghost,    {T::Psychic, T::Ghost}, 2.0f);
        Set(T::Ghost,    {T::Dark}, 0.5f);
        Set(T::Ghost,    {T::Normal}, 0.0f);

        Set(T::Dragon,   {T::Dragon}, 2.0f);
        Set(T::Dragon,   {T::Steel}, 0.5f);
        Set(T::Dragon,   {T::Fairy}, 0.0f);

        Set(T::Dark,     {T::Psychic, T::Ghost}, 2.0f);
        Set(T::Dark,     {T::Fighting, T::Dark, T::Fairy}, 0.5f);

        Set(T::Steel,    {T::Ice, T::Rock, T::Fairy}, 2.0f);
        Set(T::Steel,    {T::Fire, T::Water, T::Electric, T::Steel}, 0.5f);

        Set(T::Fairy,    {T::Fighting, T::Dragon, T::Dark}, 2.0f);
        Set(T::Fairy,    {T::Fire, T::Poison, T::Steel}, 0.5f);
    }
};

const Chart& GetChart() {
    static Chart chart;
    return chart;
}

bool Valid(Type t) { return (int)t >= 0 && (int)t < TYPE_COUNT; }

} // namespace

const char* TypeName(Type t) { return Valid(t) ? kNames[(int)t] : ""; }

Color TypeColor(Type t) { return Valid(t) ? kColors[(int)t] : Color{128, 128, 128, 255}; }

float TypeEffectiveness(Type attack, Type defend) {
    if (!Valid(attack) || !Valid(defend)) return 1.0f;
    return GetChart().m[(int)attack][(int)defend];
}

float TypeEffectiveness(Type attack, Type def1, Type def2) {
    float m = TypeEffectiveness(attack, def1);
    if (Valid(def2) && def2 != def1) m *= TypeEffectiveness(attack, def2);
    return m;
}
