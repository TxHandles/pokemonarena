#include <cstdio>
#include "items.h"
#include "moves.h"

namespace {

struct TmInfo { int no; const char* move; int badges; int price; };
#include "tms.inc"

std::vector<ItemDef> Build() {
    std::vector<ItemDef> v;
    auto add = [&](const char* name, ItemFx fx, float value, int price, int badges, const char* desc, Type t = Type::None) {
        ItemDef d;
        d.name = name;
        d.fx = fx;
        d.value = value;
        d.price = price;
        d.badges = badges;
        d.desc = desc;
        d.type = t;
        v.push_back(d);
    };
    // Poke Balls
    add("Poke Ball", ItemFx::Ball, 1.0f, 20, 0, "A ball for catching weakened Pokemon.");
    add("Great Ball", ItemFx::Ball, 1.5f, 60, 2, "Catch rate x1.5.");
    add("Ultra Ball", ItemFx::Ball, 2.0f, 150, 5, "Catch rate x2.");
    add("Master Ball", ItemFx::Ball, 0.0f, 2500, 8, "Never fails.");

    // Evolution stones (used from Party & Box)
    add("Fire Stone", ItemFx::Stone, 0, 300, 1, "Evolves Vulpix, Growlithe and Eevee (into Flareon).", Type::Fire);
    add("Water Stone", ItemFx::Stone, 0, 300, 1, "Evolves Poliwhirl, Shellder, Staryu, Lombre and Eevee (into Vaporeon).", Type::Water);
    add("Thunder Stone", ItemFx::Stone, 0, 300, 1, "Evolves Pikachu and Eevee (into Jolteon).", Type::Electric);
    add("Leaf Stone", ItemFx::Stone, 0, 300, 2, "Evolves Gloom, Weepinbell, Exeggcute and Nuzleaf.", Type::Grass);
    add("Sun Stone", ItemFx::Stone, 0, 300, 2, "Evolves Gloom (into Bellossom) and Sunkern.", Type::Fire);
    add("Moon Stone", ItemFx::Stone, 0, 400, 2, "Evolves Nidorina, Nidorino, Clefairy, Jigglypuff and Skitty.", Type::Fairy);
    add("Deep Sea Tooth", ItemFx::Stone, 0, 600, 3, "A fang that gleams a sharp silver. Evolves Clamperl into Huntail.", Type::Water);
    add("Deep Sea Scale", ItemFx::Stone, 0, 600, 3, "A scale that shines a faint pink. Evolves Clamperl into Gorebyss.", Type::Water);
    add("Prism Scale", ItemFx::Stone, 0, 800, 3, "A mysterious scale that shines in all the colors of the rainbow. Evolves Feebas into Milotic.", Type::Water);
    add("Linking Cord", ItemFx::Stone, 0, 800, 3, "Evolves the Pokemon that evolve by trading: Kadabra, Machoke, Graveler, Haunter, Onix, Scyther, Seadra, Porygon, Slowpoke (Slowking) and Poliwhirl (Politoed).", Type::Psychic);

    // Type boosters (+20% damage for one type)
    struct TB { const char* n; Type t; int badges; };
    const TB tbs[] = {
        {"Silk Scarf", Type::Normal, 0}, {"Charcoal", Type::Fire, 0}, {"Mystic Water", Type::Water, 0},
        {"Miracle Seed", Type::Grass, 0}, {"Magnet", Type::Electric, 1}, {"Hard Stone", Type::Rock, 1},
        {"Soft Sand", Type::Ground, 1}, {"Sharp Beak", Type::Flying, 1}, {"Silver Powder", Type::Bug, 1},
        {"Black Belt", Type::Fighting, 1}, {"Never-Melt Ice", Type::Ice, 2}, {"Poison Barb", Type::Poison, 2},
        {"Twisted Spoon", Type::Psychic, 2}, {"Spell Tag", Type::Ghost, 3}, {"Black Glasses", Type::Dark, 3},
        {"Metal Coat", Type::Steel, 3}, {"Dragon Fang", Type::Dragon, 3}, {"Fairy Feather", Type::Fairy, 3},
    };
    for (const auto& t : tbs) {
        std::string desc = std::string(TypeName(t.t)) + "-type moves deal 20% more damage.";
        ItemDef d;
        d.name = t.n;
        d.fx = ItemFx::TypeBoost;
        d.type = t.t;
        d.value = 1.2f;
        d.price = 120;
        d.badges = t.badges;
        d.desc = desc;
        v.push_back(d);
    }

    add("Swift Wing", ItemFx::SpeedMul, 1.12f, 150, 0, "Moves around 12% faster.");
    add("Quick Claw", ItemFx::CooldownMul, 0.88f, 250, 1, "Move cooldowns 12% shorter.");
    add("Lucky Egg", ItemFx::LuckyEgg, 1.5f, 300, 1, "Earns 50% more XP (Story Mode).");
    add("Amulet Coin", ItemFx::AmuletCoin, 1.5f, 300, 2, "50% more coins if it takes part in a win.");
    add("Leftovers", ItemFx::Leftovers, 0.006f, 400, 2, "Slowly restores HP during battle.");
    add("Muscle Band", ItemFx::AtkMul, 1.1f, 300, 3, "ATK +10%.");
    add("Scope Lens", ItemFx::ScopeLens, 1.0f, 300, 3, "Critical hits much more likely.");
    add("Shell Bell", ItemFx::ShellBell, 0.12f, 400, 4, "Heals 12% of the damage it deals.");
    add("Rocky Helmet", ItemFx::RockyHelmet, 0.08f, 400, 4, "Melee and dash attackers take 8% of their max HP.");
    add("Expert Belt", ItemFx::ExpertBelt, 1.2f, 500, 5, "Super-effective hits deal 20% more.");
    add("King's Rock", ItemFx::KingsRock, 0.12f, 350, 5, "Damaging hits may make the target flinch.");
    add("Focus Band", ItemFx::FocusBand, 1.0f, 450, 5, "Once per battle, hangs on at 1 HP instead of fainting.");
    add("Choice Scarf", ItemFx::ChoiceScarf, 1.3f, 600, 6, "Moves 30% faster, but cooldowns 15% longer.");
    add("Eviolite", ItemFx::DefIfEvolves, 1.3f, 500, 6, "DEF +30% while it can still evolve.");
    add("Assault Vest", ItemFx::DefMul, 1.25f, 500, 6, "DEF +25%.");
    add("Life Orb", ItemFx::LifeOrb, 1.3f, 700, 7, "Damage +30%, but each attack costs a little HP.");
    add("Choice Band", ItemFx::ChoiceBand, 1.35f, 700, 7, "ATK +35%, but cooldowns 15% longer.");
    // Fossils from Mt. Moon (revived at the Cinnabar Lab)
    add("Helix Fossil", ItemFx::KeyItem, 0, 0, 99, "A fossil of an ancient spiral-shelled Pokemon. The Cinnabar Lab can revive it.", Type::Rock);
    add("Dome Fossil", ItemFx::KeyItem, 0, 0, 99, "A fossil of an ancient Pokemon with a domed shell. The Cinnabar Lab can revive it.", Type::Rock);
    add("Old Amber", ItemFx::KeyItem, 0, 0, 99, "A piece of amber holding the genes of an ancient flying Pokemon. The Cinnabar Lab can revive it.", Type::Rock);
    // Celadon Game Corner prizes (bought with casino coins; badges 99 keeps them out of the Poke Mart)
    add("Smoke Ball", ItemFx::SpeedMul, 1.10f, 0, 99, "A Game Corner prize. Puffs of smoke help it slip around: moves 10% faster.");
    add("Yellow Flute", ItemFx::ConfuseGuard, 0.3f, 0, 99, "A Game Corner prize. Its tune snaps the holder out of confusion much sooner.");
    // TMs (Scarlet/Violet numbering): used up when taught, from Party & Box
    for (const TmInfo& t : kTms) {
        ItemDef d;
        char buf[64];
        std::snprintf(buf, sizeof buf, "TM%03d %s", t.no, t.move);
        d.name = buf;
        d.fx = ItemFx::TM;
        d.move = t.move;
        d.tmNo = t.no;
        d.price = t.price;
        d.badges = t.badges;
        const MoveDef* m = FindMove(t.move);
        d.type = m ? m->type : Type::Normal;
        d.desc = std::string("Teaches ") + t.move + " to a Pokemon that can learn it. Used up when taught.";
        v.push_back(d);
    }
    return v;
}

} // namespace

const std::vector<ItemDef>& ItemList() {
    static const std::vector<ItemDef> v = Build();
    return v;
}

const ItemDef* FindItem(const std::string& name) {
    if (name.empty()) return nullptr;
    std::string k = Squash(name);
    for (const auto& d : ItemList())
        if (Squash(d.name) == k) return &d;
    return nullptr;
}

bool IsBall(const ItemDef& d) { return d.fx == ItemFx::Ball; }
bool IsStone(const ItemDef& d) { return d.fx == ItemFx::Stone; }
bool IsHoldable(const ItemDef& d) { return d.fx != ItemFx::Ball && d.fx != ItemFx::Stone && d.fx != ItemFx::TM && d.fx != ItemFx::KeyItem; }
bool IsTM(const ItemDef& d) { return d.fx == ItemFx::TM; }
