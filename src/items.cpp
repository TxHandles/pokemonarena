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
    add("Thunder Stone", ItemFx::Stone, 0, 300, 1, "Evolves Pikachu, Eevee (into Jolteon), Magneton and Nosepass.", Type::Electric);
    add("Leaf Stone", ItemFx::Stone, 0, 300, 2, "Evolves Gloom, Weepinbell, Exeggcute, Nuzleaf and Eevee (into Leafeon).", Type::Grass);
    add("Sun Stone", ItemFx::Stone, 0, 300, 2, "Evolves Gloom (into Bellossom) and Sunkern.", Type::Fire);
    add("Moon Stone", ItemFx::Stone, 0, 400, 2, "Evolves Nidorina, Nidorino, Clefairy, Jigglypuff and Skitty.", Type::Fairy);
    add("Deep Sea Tooth", ItemFx::Stone, 0, 600, 3, "A fang that gleams a sharp silver. Evolves Clamperl into Huntail.", Type::Water);
    add("Deep Sea Scale", ItemFx::Stone, 0, 600, 3, "A scale that shines a faint pink. Evolves Clamperl into Gorebyss.", Type::Water);
    add("Prism Scale", ItemFx::Stone, 0, 800, 3, "A mysterious scale that shines in all the colors of the rainbow. Evolves Feebas into Milotic.", Type::Water);
    add("Linking Cord", ItemFx::Stone, 0, 800, 3, "Evolves the Pokemon that evolve by trading: Kadabra, Machoke, Graveler, Haunter, Onix, Scyther, Seadra, Porygon, Slowpoke (Slowking), Poliwhirl (Politoed), and from Sinnoh Rhydon, Electabuzz, Magmar, Porygon2 and Dusclops.", Type::Psychic);
    add("Shiny Stone", ItemFx::Stone, 0, 500, 3, "Evolves Togetic and Roselia.", Type::Fairy);
    add("Dusk Stone", ItemFx::Stone, 0, 500, 3, "Evolves Misdreavus and Murkrow.", Type::Dark);
    add("Dawn Stone", ItemFx::Stone, 0, 500, 3, "Evolves Kirlia (into Gallade) and Snorunt (into Froslass).", Type::Psychic);
    add("Ice Stone", ItemFx::Stone, 0, 400, 3, "Evolves Eevee into Glaceon.", Type::Ice);
    add("Oval Stone", ItemFx::Stone, 0, 400, 2, "Evolves Happiny into Chansey.", Type::Normal);
    add("Razor Claw", ItemFx::Stone, 0, 600, 3, "A sharp claw. Evolves Sneasel into Weavile.", Type::Dark);
    add("Razor Fang", ItemFx::Stone, 0, 600, 3, "A sharp fang. Evolves Gligar into Gliscor.", Type::Dark);

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
    add("Exp. All", ItemFx::ExpAll, 0.5f, 5000, 0, "Co-op: when you beat wild Pokemon on your own while walking around, your partner's party shares in it - half your XP, split between them. Stays in your Bag.");
    // Medicine: carried in the Bag, used in battle from the four item hotkeys (30 s cooldown per slot)
    auto med = [&](const char* name, float heal, int cure, int price, int badges, const char* desc, Type t) {
        add(name, ItemFx::Medicine, heal, price, badges, desc, t);
        v.back().cure = cure;
    };
    using I = ItemDef;
    med("Potion", 0.20f, 0, 200, 0, "Restores 20% of a Pokemon's max HP.", Type::Normal);
    med("Super Potion", 0.35f, 0, 700, 2, "Restores 35% of a Pokemon's max HP.", Type::Normal);
    med("Hyper Potion", 0.55f, 0, 1500, 4, "Restores 55% of a Pokemon's max HP.", Type::Normal);
    med("Max Potion", 1.0f, 0, 2500, 6, "Fully restores a Pokemon's HP.", Type::Normal);
    med("Full Restore", 1.0f, I::CURE_ALL, 3000, 8, "Fully restores HP and cures every status condition and confusion.", Type::Normal);
    med("Antidote", 0, I::CURE_POISON, 100, 0, "Cures poison.", Type::Poison);
    med("Paralyze Heal", 0, I::CURE_PARALYZE, 200, 0, "Cures paralysis.", Type::Electric);
    med("Awakening", 0, I::CURE_SLEEP, 100, 1, "Wakes up a sleeping Pokemon.", Type::Psychic);
    med("Burn Heal", 0, I::CURE_BURN, 200, 1, "Heals a burn.", Type::Fire);
    med("Ice Heal", 0, I::CURE_FREEZE, 100, 1, "Thaws a frozen Pokemon.", Type::Ice);
    med("Full Heal", 0, I::CURE_ALL, 400, 3, "Cures every status condition and confusion.", Type::Normal);
    med("Oran Berry", 0.12f, 0, 80, 0, "A berry that restores 12% of max HP.", Type::Grass);
    med("Sitrus Berry", 0.25f, 0, 300, 3, "A berry that restores 25% of max HP.", Type::Grass);
    med("Pecha Berry", 0, I::CURE_POISON, 60, 0, "A sweet berry that cures poison.", Type::Grass);
    med("Cheri Berry", 0, I::CURE_PARALYZE, 80, 0, "A spicy berry that cures paralysis.", Type::Grass);
    med("Chesto Berry", 0, I::CURE_SLEEP, 60, 1, "A dry berry that wakes up a sleeping Pokemon.", Type::Grass);
    med("Rawst Berry", 0, I::CURE_BURN, 80, 1, "A bitter berry that heals a burn.", Type::Grass);
    med("Aspear Berry", 0, I::CURE_FREEZE, 60, 1, "A sour berry that thaws a frozen Pokemon.", Type::Grass);
    med("Persim Berry", 0, I::CURE_CONFUSE, 100, 2, "A berry that snaps a Pokemon out of confusion.", Type::Grass);
    med("Lum Berry", 0, I::CURE_ALL, 500, 5, "A berry that cures every status condition and confusion.", Type::Grass);
    // Mega Stones: held by their Pokemon, they let it Mega Evolve once per battle (sold after your first Champion)
    add("Venusaurite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Venusaur: in battle, press Mega to Mega Evolve it into Mega Venusaur (once per battle).", Type::Grass);
    v.back().move = "Venusaur";
    add("Charizardite X", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Charizard: in battle, press Mega to Mega Evolve it into Mega Charizard X (once per battle).", Type::Fire);
    v.back().move = "Charizard";
    add("Charizardite Y", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Charizard: in battle, press Mega to Mega Evolve it into Mega Charizard Y (once per battle).", Type::Fire);
    v.back().move = "Charizard";
    add("Blastoisinite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Blastoise: in battle, press Mega to Mega Evolve it into Mega Blastoise (once per battle).", Type::Water);
    v.back().move = "Blastoise";
    add("Beedrillite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Beedrill: in battle, press Mega to Mega Evolve it into Mega Beedrill (once per battle).", Type::Bug);
    v.back().move = "Beedrill";
    add("Pidgeotite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Pidgeot: in battle, press Mega to Mega Evolve it into Mega Pidgeot (once per battle).", Type::Normal);
    v.back().move = "Pidgeot";
    add("Alakazite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Alakazam: in battle, press Mega to Mega Evolve it into Mega Alakazam (once per battle).", Type::Psychic);
    v.back().move = "Alakazam";
    add("Slowbronite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Slowbro: in battle, press Mega to Mega Evolve it into Mega Slowbro (once per battle).", Type::Water);
    v.back().move = "Slowbro";
    add("Gengarite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Gengar: in battle, press Mega to Mega Evolve it into Mega Gengar (once per battle).", Type::Ghost);
    v.back().move = "Gengar";
    add("Kangaskhanite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Kangaskhan: in battle, press Mega to Mega Evolve it into Mega Kangaskhan (once per battle).", Type::Normal);
    v.back().move = "Kangaskhan";
    add("Pinsirite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Pinsir: in battle, press Mega to Mega Evolve it into Mega Pinsir (once per battle).", Type::Bug);
    v.back().move = "Pinsir";
    add("Gyaradosite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Gyarados: in battle, press Mega to Mega Evolve it into Mega Gyarados (once per battle).", Type::Water);
    v.back().move = "Gyarados";
    add("Aerodactylite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Aerodactyl: in battle, press Mega to Mega Evolve it into Mega Aerodactyl (once per battle).", Type::Rock);
    v.back().move = "Aerodactyl";
    add("Mewtwonite X", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Mewtwo: in battle, press Mega to Mega Evolve it into Mega Mewtwo X (once per battle).", Type::Psychic);
    v.back().move = "Mewtwo";
    add("Mewtwonite Y", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Mewtwo: in battle, press Mega to Mega Evolve it into Mega Mewtwo Y (once per battle).", Type::Psychic);
    v.back().move = "Mewtwo";
    add("Clefablite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Clefable: in battle, press Mega to Mega Evolve it into Mega Clefable (once per battle).", Type::Fairy);
    v.back().move = "Clefable";
    add("Victreebelite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Victreebel: in battle, press Mega to Mega Evolve it into Mega Victreebel (once per battle).", Type::Grass);
    v.back().move = "Victreebel";
    add("Starminite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Starmie: in battle, press Mega to Mega Evolve it into Mega Starmie (once per battle).", Type::Water);
    v.back().move = "Starmie";
    add("Dragoninite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Dragonite: in battle, press Mega to Mega Evolve it into Mega Dragonite (once per battle).", Type::Dragon);
    v.back().move = "Dragonite";
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
bool IsHoldable(const ItemDef& d) { return d.fx != ItemFx::Ball && d.fx != ItemFx::Stone && d.fx != ItemFx::TM && d.fx != ItemFx::KeyItem && d.fx != ItemFx::ExpAll && d.fx != ItemFx::Medicine; }
bool IsTM(const ItemDef& d) { return d.fx == ItemFx::TM; }
bool IsMegaStone(const ItemDef& d) { return d.fx == ItemFx::MegaStone; }
bool IsMedicine(const ItemDef& d) { return d.fx == ItemFx::Medicine; }
