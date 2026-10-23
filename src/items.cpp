#include <cstdio>
#include "items.h"
#include "moves.h"
#include "berries.h"

namespace {

const char* kHmMovesPub[9] = {"", "Cut", "Fly", "Surf", "Strength", "Flash", "Rock Smash", "Waterfall", "Dive"};
std::string TextFormatHm(int n) { char b[40]; std::snprintf(b, sizeof b, "HM%02d %s", n, kHmMovesPub[n]); return b; }

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
    add("Poke Ball", ItemFx::Ball, 1.0f, 200, 0, "A ball for catching weakened Pokemon.");
    add("Great Ball", ItemFx::Ball, 1.5f, 600, 2, "Catch rate x1.5.");
    add("Ultra Ball", ItemFx::Ball, 2.0f, 1200, 5, "Catch rate x2.");
    add("Master Ball", ItemFx::Ball, 0.0f, 2500, 8, "Never fails.");
    add("Net Ball", ItemFx::Ball, 1.0f, 1000, 99, "Catch rate x3 on Water- and Bug-type Pokemon.");
    add("Nest Ball", ItemFx::Ball, 1.0f, 1000, 99, "Works better the lower the wild Pokemon's level (up to x3).");
    add("Dive Ball", ItemFx::Ball, 1.0f, 1000, 99, "Catch rate x3.5 on Pokemon met on (or under) the water - surfing, diving or fishing.");
    add("Repeat Ball", ItemFx::Ball, 1.0f, 1000, 99, "Catch rate x3.5 on a species you've already caught.");
    add("Luxury Ball", ItemFx::Ball, 1.0f, 3000, 99, "A comfortable ball: a Pokemon caught in it grows friendly twice as fast.");
    add("Premier Ball", ItemFx::Ball, 1.0f, 200, 99, "A rare ball made in commemoration of some event. Works like a Poke Ball.");
    add("Timer Ball", ItemFx::Ball, 1.0f, 1000, 99, "Works better the longer the battle goes on (up to x4).");

    // Evolution stones (used from Party & Box)
    add("Fire Stone", ItemFx::Stone, 0, 2100, 1, "Evolves Vulpix, Growlithe and Eevee (into Flareon).", Type::Fire);
    add("Water Stone", ItemFx::Stone, 0, 2100, 1, "Evolves Poliwhirl, Shellder, Staryu, Lombre and Eevee (into Vaporeon).", Type::Water);
    add("Thunder Stone", ItemFx::Stone, 0, 2100, 1, "Evolves Pikachu, Eevee (into Jolteon), Magneton and Nosepass.", Type::Electric);
    add("Leaf Stone", ItemFx::Stone, 0, 2100, 2, "Evolves Gloom, Weepinbell, Exeggcute, Nuzleaf and Eevee (into Leafeon).", Type::Grass);
    add("Sun Stone", ItemFx::Stone, 0, 1500, 2, "Evolves Gloom (into Bellossom) and Sunkern.", Type::Fire);
    add("Moon Stone", ItemFx::Stone, 0, 2100, 2, "Evolves Nidorina, Nidorino, Clefairy, Jigglypuff and Skitty.", Type::Fairy);
    add("Deep Sea Tooth", ItemFx::Stone, 0, 3000, 3, "A fang that gleams a sharp silver. Evolves Clamperl into Huntail.", Type::Water);
    add("Deep Sea Scale", ItemFx::Stone, 0, 3000, 3, "A scale that shines a faint pink. Evolves Clamperl into Gorebyss.", Type::Water);
    add("Prism Scale", ItemFx::Stone, 0, 4000, 3, "A mysterious scale that shines in all the colors of the rainbow. Evolves Feebas into Milotic.", Type::Water);
    add("Linking Cord", ItemFx::Stone, 0, 4000, 3, "Evolves the Pokemon that evolve by trading: Kadabra, Machoke, Graveler, Haunter, Onix, Scyther, Seadra, Porygon, Slowpoke (Slowking), Poliwhirl (Politoed), and from Sinnoh Rhydon, Electabuzz, Magmar, Porygon2 and Dusclops.", Type::Psychic);
    add("Shiny Stone", ItemFx::Stone, 0, 2500, 3, "Evolves Togetic and Roselia.", Type::Fairy);
    add("Dusk Stone", ItemFx::Stone, 0, 2500, 3, "Evolves Misdreavus and Murkrow.", Type::Dark);
    add("Dawn Stone", ItemFx::Stone, 0, 2500, 3, "Evolves Kirlia (into Gallade) and Snorunt (into Froslass).", Type::Psychic);
    add("Ice Stone", ItemFx::Stone, 0, 2000, 3, "Evolves Eevee into Glaceon.", Type::Ice);
    add("Oval Stone", ItemFx::Stone, 0, 2000, 2, "Evolves Happiny into Chansey.", Type::Normal);
    add("Razor Claw", ItemFx::Stone, 0, 3000, 3, "A sharp claw. Evolves Sneasel into Weavile.", Type::Dark);
    add("Razor Fang", ItemFx::Stone, 0, 3000, 3, "A sharp fang. Evolves Gligar into Gliscor.", Type::Dark);

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
        d.price = 1200;
        d.badges = t.badges;
        d.desc = desc;
        v.push_back(d);
    }

    add("Swift Wing", ItemFx::SpeedMul, 1.12f, 1500, 0, "Moves around 12% faster.");
    add("Quick Claw", ItemFx::CooldownMul, 0.88f, 2500, 1, "Move cooldowns 12% shorter.");
    add("Lucky Egg", ItemFx::LuckyEgg, 1.5f, 3000, 1, "Earns 50% more XP (Story Mode).");
    add("Amulet Coin", ItemFx::AmuletCoin, 1.5f, 3000, 2, "50% more coins if it takes part in a win.");
    add("Leftovers", ItemFx::Leftovers, 0.006f, 4000, 2, "Slowly restores HP during battle.");
    add("Muscle Band", ItemFx::AtkMul, 1.1f, 3000, 3, "ATK +10%.");
    add("Scope Lens", ItemFx::ScopeLens, 1.0f, 3000, 3, "Critical hits much more likely.");
    add("Shell Bell", ItemFx::ShellBell, 0.12f, 4000, 4, "Heals 12% of the damage it deals.");
    add("Rocky Helmet", ItemFx::RockyHelmet, 0.08f, 4000, 4, "Melee and dash attackers take 8% of their max HP.");
    add("Expert Belt", ItemFx::ExpertBelt, 1.2f, 5000, 5, "Super-effective hits deal 20% more.");
    add("King's Rock", ItemFx::KingsRock, 0.12f, 3500, 5, "Damaging hits may make the target flinch.");
    add("Focus Band", ItemFx::FocusBand, 1.0f, 4500, 5, "Once per battle, hangs on at 1 HP instead of fainting.");
    add("Choice Scarf", ItemFx::ChoiceScarf, 1.3f, 6000, 6, "Moves 30% faster, but cooldowns 15% longer.");
    add("Eviolite", ItemFx::DefIfEvolves, 1.3f, 5000, 6, "DEF +30% while it can still evolve.");
    add("Assault Vest", ItemFx::DefMul, 1.25f, 5000, 6, "DEF +25%.");
    add("Life Orb", ItemFx::LifeOrb, 1.3f, 7000, 7, "Damage +30%, but each attack costs a little HP.");
    add("Choice Band", ItemFx::ChoiceBand, 1.35f, 7000, 7, "ATK +35%, but cooldowns 15% longer.");
    add("Exp. All", ItemFx::ExpAll, 0.5f, 5000, 0, "Co-op: when you beat wild Pokemon on your own while walking around, your partner's party shares in it - half your XP, split between them. Stays in your Bag.");
    // Medicine: carried in the Bag, used in battle from the four item hotkeys (30 s cooldown per slot)
    auto med = [&](const char* name, float heal, int cure, int price, int badges, const char* desc, Type t) {
        add(name, ItemFx::Medicine, heal, price, badges, desc, t);
        v.back().cure = cure;
    };
    using I = ItemDef;
    med("Potion", 0.20f, 0, 300, 0, "Restores 20% of a Pokemon's max HP.", Type::Normal);
    med("Super Potion", 0.35f, 0, 700, 2, "Restores 35% of a Pokemon's max HP.", Type::Normal);
    med("Hyper Potion", 0.55f, 0, 1200, 4, "Restores 55% of a Pokemon's max HP.", Type::Normal);
    med("Max Potion", 1.0f, 0, 2500, 6, "Fully restores a Pokemon's HP.", Type::Normal);
    med("Full Restore", 1.0f, I::CURE_ALL, 3000, 8, "Fully restores HP and cures every status condition and confusion.", Type::Normal);
    med("Antidote", 0, I::CURE_POISON, 100, 0, "Cures poison.", Type::Poison);
    med("Paralyze Heal", 0, I::CURE_PARALYZE, 200, 0, "Cures paralysis.", Type::Electric);
    med("Awakening", 0, I::CURE_SLEEP, 250, 1, "Wakes up a sleeping Pokemon.", Type::Psychic);
    med("Burn Heal", 0, I::CURE_BURN, 250, 1, "Heals a burn.", Type::Fire);
    med("Ice Heal", 0, I::CURE_FREEZE, 250, 1, "Thaws a frozen Pokemon.", Type::Ice);
    med("Full Heal", 0, I::CURE_ALL, 600, 3, "Cures every status condition and confusion.", Type::Normal);
    med("Oran Berry", 0.12f, 0, 150, 0, "A berry that restores 12% of max HP.", Type::Grass);
    med("Sitrus Berry", 0.25f, 0, 500, 3, "A berry that restores 25% of max HP.", Type::Grass);
    med("Pecha Berry", 0, I::CURE_POISON, 120, 0, "A sweet berry that cures poison.", Type::Grass);
    med("Cheri Berry", 0, I::CURE_PARALYZE, 120, 0, "A spicy berry that cures paralysis.", Type::Grass);
    med("Chesto Berry", 0, I::CURE_SLEEP, 120, 1, "A dry berry that wakes up a sleeping Pokemon.", Type::Grass);
    med("Rawst Berry", 0, I::CURE_BURN, 120, 1, "A bitter berry that heals a burn.", Type::Grass);
    med("Aspear Berry", 0, I::CURE_FREEZE, 120, 1, "A sour berry that thaws a frozen Pokemon.", Type::Grass);
    med("Persim Berry", 0, I::CURE_CONFUSE, 150, 2, "A berry that snaps a Pokemon out of confusion.", Type::Grass);
    med("Lum Berry", 0, I::CURE_ALL, 800, 5, "A berry that cures every status condition and confusion.", Type::Grass);
    // FireRed's battle items: on a hotkey like Medicine, they boost the Pokemon out for a while
    auto bx = [&](const char* name, int boost, int price, int badges, const char* desc, Type t) {
        add(name, ItemFx::Medicine, 0, price, badges, desc, t);
        v.back().boost = boost;
    };
    bx("X Attack", I::BOOST_ATK, 500, 1, "Raises attacks (ATK and Sp. Atk) by half for 30 s in battle.", Type::Fighting);
    bx("X Special", I::BOOST_ATK, 350, 1, "Raises attacks (ATK and Sp. Atk) by half for 30 s in battle.", Type::Psychic);
    bx("X Defend", I::BOOST_DEF, 550, 1, "Raises DEF by half for 30 s in battle.", Type::Steel);
    bx("X Speed", I::BOOST_SPD, 350, 1, "Raises speed by half for 30 s in battle.", Type::Electric);
    bx("X Accuracy", I::BOOST_CRIT, 950, 2, "Sharpens its aim: critical hits are much more likely for 60 s.", Type::Normal);
    bx("Dire Hit", I::BOOST_CRIT, 650, 2, "Critical hits are much more likely for 60 s in battle.", Type::Normal);
    bx("Guard Spec.", I::BOOST_GUARD, 700, 2, "Undoes its lowered stats and confusion, and keeps its stats from being lowered for 30 s.", Type::Normal);
    bx("Ether", I::BOOST_COOLDOWN, 1200, 99, "Restores its moves: every move's cooldown is cut in half.", Type::Water);
    bx("Max Ether", I::BOOST_COOLDOWN, 2000, 99, "Fully restores its moves: every move is ready at once.", Type::Water);
    bx("Elixir", I::BOOST_COOLDOWN, 3000, 99, "Fully restores its moves: every move is ready at once.", Type::Water);
    bx("Max Elixir", I::BOOST_COOLDOWN, 4500, 99, "Fully restores its moves: every move is ready at once.", Type::Water);
    bx("Leppa Berry", I::BOOST_COOLDOWN, 200, 99, "A berry that restores its moves: every move is ready at once.", Type::Grass);
    med("Energy Powder", 0.35f, 0, 500, 99, "A bitter powder that restores 35% of a Pokemon's max HP. It's very bitter, so it's cheap.", Type::Grass);
    med("Energy Root", 0.55f, 0, 800, 99, "A bitter root that restores 55% of a Pokemon's max HP.", Type::Grass);
    med("Heal Powder", 0, I::CURE_ALL, 450, 99, "A bitter powder that cures every status condition and confusion.", Type::Grass);
    med("Fresh Water", 0.20f, 0, 200, 99, "Water with high mineral content: restores 20% of a Pokemon's max HP.", Type::Water);
    med("Soda Pop", 0.25f, 0, 300, 99, "A fizzy soda drink: restores 25% of a Pokemon's max HP.", Type::Water);
    med("Lemonade", 0.30f, 0, 350, 99, "A very sweet drink: restores 30% of a Pokemon's max HP.", Type::Water);
    med("Moomoo Milk", 0.40f, 0, 500, 99, "Highly nutritious milk: restores 40% of a Pokemon's max HP.", Type::Normal);
    med("Mental Herb", 0, I::CURE_CONFUSE, 100, 99, "A herb that snaps a Pokemon out of confusion and infatuation.", Type::Grass);
    bx("White Herb", I::BOOST_GUARD, 100, 99, "A herb that undoes lowered stats, and keeps them from being lowered for 30 s.", Type::Grass);
    // Hoenn's berries (Emerald): the five that restore HP, and the rare ones that raise a stat in battle
    for (const char* b : {"Figy Berry", "Wiki Berry", "Mago Berry", "Aguav Berry", "Iapapa Berry"})
        med(b, 0.125f, 0, 200, 99, "A berry that restores 1/8 of a Pokemon's max HP. Plant it in soft soil to grow more.", Type::Grass);
    bx("Liechi Berry", I::BOOST_ATK, 1000, 99, "A rare berry: raises attacks (ATK and Sp. Atk) by half for 30 s in battle.", Type::Grass);
    bx("Petaya Berry", I::BOOST_ATK, 1000, 99, "A rare berry: raises attacks by half for 30 s in battle.", Type::Grass);
    bx("Ganlon Berry", I::BOOST_DEF, 1000, 99, "A rare berry: raises DEF by half for 30 s in battle.", Type::Grass);
    bx("Apicot Berry", I::BOOST_DEF, 1000, 99, "A rare berry: raises defenses by half for 30 s in battle.", Type::Grass);
    bx("Salac Berry", I::BOOST_SPD, 1000, 99, "A rare berry: raises speed by half for 30 s in battle.", Type::Grass);
    bx("Lansat Berry", I::BOOST_CRIT, 1000, 99, "A rare berry: critical hits are much more likely for 60 s in battle.", Type::Grass);
    bx("Starf Berry", I::BOOST_ATK, 1000, 99, "A rare berry: sharply raises its attacks for 30 s in battle.", Type::Grass);
    med("Lava Cookie", 0, I::CURE_ALL, 200, 99, "Lavaridge Town's local specialty. Cures every status condition and confusion.", Type::Fire);
    // Used on a Pokemon from Party & Box
    auto pu = [&](const char* name, float value, int boost, int price, int badges, const char* desc) {
        add(name, ItemFx::PartyUse, value, price, badges, desc);
        v.back().boost = boost;
    };
    pu("Revive", 0.5f, -1, 1500, 3, "Revives a fainted Pokemon with half its HP. Use it from Party & Box.");
    pu("Max Revive", 1.0f, -1, 4000, 99, "Revives a fainted Pokemon with all its HP. Use it from Party & Box.");
    pu("Revival Herb", 1.0f, -1, 2800, 99, "A very bitter herb that revives a fainted Pokemon with all its HP. Use it from Party & Box.");
    pu("Rare Candy", 0, -1, 4800, 99, "Raises a Pokemon's level by one. Use it from Party & Box.");
    // vitamins: +10 EVs in one stat (up to 252, as in Sword and Shield on)
    pu("HP Up", 0, 0, 9800, 4, "A nutritious drink: +10 HP EVs (up to 252). EVs raise a stat as it levels. Use it from Party & Box.");
    pu("Protein", 0, 1, 9800, 4, "A nutritious drink: +10 Attack EVs (up to 252). Use it from Party & Box.");
    pu("Iron", 0, 2, 9800, 4, "A nutritious drink: +10 Defense EVs (up to 252). Use it from Party & Box.");
    pu("Calcium", 0, 3, 9800, 4, "A nutritious drink: +10 Sp. Atk EVs (up to 252). Use it from Party & Box.");
    pu("Zinc", 0, 4, 9800, 4, "A nutritious drink: +10 Sp. Def EVs (up to 252). Use it from Party & Box.");
    pu("Carbos", 0, 5, 9800, 4, "A nutritious drink: +10 Speed EVs (up to 252). Use it from Party & Box.");
    // berries that lower EVs (and make it friendlier)
    pu("Pomeg Berry", 0, 10, 300, 4, "Lowers a Pokemon's HP EVs by 10 and makes it friendlier. Use it from Party & Box.");
    pu("Kelpsy Berry", 0, 11, 300, 4, "Lowers a Pokemon's Attack EVs by 10 and makes it friendlier. Use it from Party & Box.");
    pu("Qualot Berry", 0, 12, 300, 4, "Lowers a Pokemon's Defense EVs by 10 and makes it friendlier. Use it from Party & Box.");
    pu("Hondew Berry", 0, 13, 300, 4, "Lowers a Pokemon's Sp. Atk EVs by 10 and makes it friendlier. Use it from Party & Box.");
    pu("Grepa Berry", 0, 14, 300, 4, "Lowers a Pokemon's Sp. Def EVs by 10 and makes it friendlier. Use it from Party & Box.");
    pu("Tamato Berry", 0, 15, 300, 4, "Lowers a Pokemon's Speed EVs by 10 and makes it friendlier. Use it from Party & Box.");
    // Pokeblocks from Lilycove's Berry Blender (one per berry): Contest condition, from Party & Box
    auto flavorText = [](int i) {
        static const char* kCond[5] = {"Cool", "Beauty", "Cute", "Smart", "Tough"};
        std::string t;
        for (int k = 0; k < 5; k++) if (BerryAt(i).flavor[k] > 0) t += (t.empty() ? "" : ", ") + std::string(kCond[k]);
        return t.empty() ? std::string("sheen") : t;
    };
    for (int i = 0; i < BerryCount(); i++) {
        std::string b = BerryAt(i).name;
        std::string nm = "Pokeblock (" + b.substr(0, b.find(' ')) + ")";
        add(nm.c_str(), ItemFx::PartyUse, 0, 0, 99, ("A Pokeblock blended from a " + b + ". Feed it to a Pokemon from Party & Box to raise its Contest condition (" + flavorText(i) + ").").c_str());
        v.back().boost = 100 + i;
    }
    // Used from the Bag out in the world
    add("Repel", ItemFx::Field, 100, 350, 1, "Keeps wild Pokemon away for 100 steps. Use it from the Bag.");
    add("Super Repel", ItemFx::Field, 200, 500, 3, "Keeps wild Pokemon away for 200 steps. Use it from the Bag.");
    add("Max Repel", ItemFx::Field, 250, 700, 5, "Keeps wild Pokemon away for 250 steps. Use it from the Bag.");
    add("Escape Rope", ItemFx::Field, 0, 550, 1, "Climb out of a cave or building full of wild Pokemon, back to the last Pokemon Center's town.");
    // Only worth selling (from the Bag: the Mart pays half)
    auto val = [&](const char* name, int price, const char* desc) { add(name, ItemFx::Valuable, 0, price, 99, desc); };
    val("Nugget", 10000, "A nugget of pure gold. Sell it from the Bag for 5000 coins.");
    val("Big Pearl", 7500, "A big, beautiful pearl. Sell it from the Bag for 3750 coins.");
    val("Pearl", 1400, "A small, pretty pearl. Sell it from the Bag for 700 coins.");
    val("Star Piece", 9800, "A shard of a pretty red gem. Sell it from the Bag for 4900 coins.");
    val("Stardust", 2000, "Lovely, red-colored sand. Sell it from the Bag for 1000 coins.");
    val("Big Mushroom", 5000, "A big, rare mushroom. Sell it from the Bag for 2500 coins.");
    val("Tiny Mushroom", 500, "A small, rare mushroom. Sell it from the Bag for 250 coins.");
    val("Heart Scale", 200, "A pretty, heart-shaped scale. Sell it from the Bag for 100 coins.");
    val("Red Shard", 400, "A small red shard from an ancient item. A man on Route 124 trades a Fire Stone for 4... or sell it for 200.");
    val("Blue Shard", 400, "A small blue shard from an ancient item. A man on Route 124 trades a Water Stone for 4... or sell it for 200.");
    val("Yellow Shard", 400, "A small yellow shard from an ancient item. A man on Route 124 trades a Thunder Stone for 4... or sell it for 200.");
    val("Green Shard", 400, "A small green shard from an ancient item. A man on Route 124 trades a Leaf Stone for 4... or sell it for 200.");
    val("Shoal Salt", 40, "Pure salt from deep inside Shoal Cave. Sell it for 20... or bring 4 with 4 Shoal Shells to the old man there.");
    val("Shoal Shell", 40, "A pretty seashell from Shoal Cave. Sell it for 20... or bring 4 with 4 Shoal Salts to the old man there.");
    val("PP Up", 9800, "A drink that powers up a move. Its moves don't run out here, so sell it from the Bag for 4900 coins.");
    val("PP Max", 9800, "A drink that fully powers up a move. Its moves don't run out here, so sell it from the Bag for 4900 coins.");
    for (const char* b : {"Razz Berry", "Bluk Berry", "Nanab Berry", "Wepear Berry", "Pinap Berry", "Cornn Berry", "Magost Berry",
                          "Rabuta Berry", "Nomel Berry", "Spelon Berry", "Pamtre Berry", "Watmel Berry", "Durin Berry", "Belue Berry", "Enigma Berry"})
        val(b, 40, "A berry for making Pokeblocks in a Berry Blender (Hoenn). Plant it in soft soil to grow more, or sell it for 20 coins.");
    // FireRed's other held items
    add("Everstone", ItemFx::Everstone, 0, 2000, 1, "A strange stone: the Pokemon holding it doesn't evolve.");
    add("Soothe Bell", ItemFx::None, 2, 3000, 2, "A bell with a comforting chime: the Pokemon holding it grows fond of you twice as fast.");
    add("Cleanse Tag", ItemFx::None, 0, 2000, 2, "Held by the first Pokemon in your party, wild Pokemon show up a third less often.");
    add("Lax Incense", ItemFx::DefMul, 1.08f, 2000, 3, "A light incense that wafts around it: DEF +8%.");
    add("Sea Incense", ItemFx::TypeBoost, 1.1f, 2000, 3, "A watery incense: Water-type moves deal 10% more damage.", Type::Water);
    add("Lucky Punch", ItemFx::ScopeLens, 1.0f, 2500, 3, "A glove that brings luck: held by Chansey, its critical hits are much more likely.");
    add("Macho Brace", ItemFx::LuckyEgg, 1.25f, 3000, 2, "Heavy training gear: 25% more XP and double EVs from the Pokemon it beats, but it moves 10% slower.");
    // the Power items: +8 EVs in one stat for every Pokemon it beats (and, at the Day Care, that IV is passed down)
    auto pw = [&](const char* name, int stat, const char* what) {
        add(name, ItemFx::PowerItem, 0, 3000, 4, (std::string("Heavy gear: +8 ") + what + " EVs for every Pokemon it beats, but it moves 10% slower. At the Day Care, its " + what + " IV is passed down.").c_str());
        v.back().boost = stat;
    };
    pw("Power Weight", 0, "HP");
    pw("Power Bracer", 1, "Attack");
    pw("Power Belt", 2, "Defense");
    pw("Power Lens", 3, "Sp. Atk");
    pw("Power Band", 4, "Sp. Def");
    pw("Power Anklet", 5, "Speed");
    add("Destiny Knot", ItemFx::Breeding, 0, 5000, 4, "A long, thin, bright-red string. Held at the Day Care, it passes five of the parents' IVs down to an Egg instead of three.");
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
    // From the "Mega Evolution Overworld Sprites" sheet (Kidkatt, Larryturbo, Princess-Phoenix); Mega Rayquaza needs no
    // stone: it Mega Evolves when it knows Dragon Ascent.
    add("Ampharosite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Ampharos: in battle, press Mega to Mega Evolve it into Mega Ampharos (once per battle).", Type::Electric);
    v.back().move = "Ampharos";
    add("Scizorite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Scizor: in battle, press Mega to Mega Evolve it into Mega Scizor (once per battle).", Type::Bug);
    v.back().move = "Scizor";
    add("Heracronite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Heracross: in battle, press Mega to Mega Evolve it into Mega Heracross (once per battle).", Type::Bug);
    v.back().move = "Heracross";
    add("Houndoominite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Houndoom: in battle, press Mega to Mega Evolve it into Mega Houndoom (once per battle).", Type::Dark);
    v.back().move = "Houndoom";
    add("Tyranitarite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Tyranitar: in battle, press Mega to Mega Evolve it into Mega Tyranitar (once per battle).", Type::Rock);
    v.back().move = "Tyranitar";
    add("Sceptilite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Sceptile: in battle, press Mega to Mega Evolve it into Mega Sceptile (once per battle).", Type::Grass);
    v.back().move = "Sceptile";
    add("Blazikenite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Blaziken: in battle, press Mega to Mega Evolve it into Mega Blaziken (once per battle).", Type::Fire);
    v.back().move = "Blaziken";
    add("Swampertite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Swampert: in battle, press Mega to Mega Evolve it into Mega Swampert (once per battle).", Type::Water);
    v.back().move = "Swampert";
    add("Gardevoirite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Gardevoir: in battle, press Mega to Mega Evolve it into Mega Gardevoir (once per battle).", Type::Psychic);
    v.back().move = "Gardevoir";
    add("Sablenite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Sableye: in battle, press Mega to Mega Evolve it into Mega Sableye (once per battle).", Type::Dark);
    v.back().move = "Sableye";
    add("Mawilite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Mawile: in battle, press Mega to Mega Evolve it into Mega Mawile (once per battle).", Type::Steel);
    v.back().move = "Mawile";
    add("Aggronite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Aggron: in battle, press Mega to Mega Evolve it into Mega Aggron (once per battle).", Type::Steel);
    v.back().move = "Aggron";
    add("Medichamite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Medicham: in battle, press Mega to Mega Evolve it into Mega Medicham (once per battle).", Type::Fighting);
    v.back().move = "Medicham";
    add("Manectite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Manectric: in battle, press Mega to Mega Evolve it into Mega Manectric (once per battle).", Type::Electric);
    v.back().move = "Manectric";
    add("Sharpedonite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Sharpedo: in battle, press Mega to Mega Evolve it into Mega Sharpedo (once per battle).", Type::Water);
    v.back().move = "Sharpedo";
    add("Cameruptite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Camerupt: in battle, press Mega to Mega Evolve it into Mega Camerupt (once per battle).", Type::Fire);
    v.back().move = "Camerupt";
    add("Altarianite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Altaria: in battle, press Mega to Mega Evolve it into Mega Altaria (once per battle).", Type::Dragon);
    v.back().move = "Altaria";
    add("Banettite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Banette: in battle, press Mega to Mega Evolve it into Mega Banette (once per battle).", Type::Ghost);
    v.back().move = "Banette";
    add("Absolite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Absol: in battle, press Mega to Mega Evolve it into Mega Absol (once per battle).", Type::Dark);
    v.back().move = "Absol";
    add("Glalitite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Glalie: in battle, press Mega to Mega Evolve it into Mega Glalie (once per battle).", Type::Ice);
    v.back().move = "Glalie";
    add("Salamencite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Salamence: in battle, press Mega to Mega Evolve it into Mega Salamence (once per battle).", Type::Dragon);
    v.back().move = "Salamence";
    add("Metagrossite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Metagross: in battle, press Mega to Mega Evolve it into Mega Metagross (once per battle).", Type::Steel);
    v.back().move = "Metagross";
    add("Lopunnite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Lopunny: in battle, press Mega to Mega Evolve it into Mega Lopunny (once per battle).", Type::Normal);
    v.back().move = "Lopunny";
    add("Garchompite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Garchomp: in battle, press Mega to Mega Evolve it into Mega Garchomp (once per battle).", Type::Dragon);
    v.back().move = "Garchomp";
    add("Lucarionite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Lucario: in battle, press Mega to Mega Evolve it into Mega Lucario (once per battle).", Type::Fighting);
    v.back().move = "Lucario";
    add("Abomasite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Abomasnow: in battle, press Mega to Mega Evolve it into Mega Abomasnow (once per battle).", Type::Grass);
    v.back().move = "Abomasnow";
    add("Galladite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Gallade: in battle, press Mega to Mega Evolve it into Mega Gallade (once per battle).", Type::Psychic);
    v.back().move = "Gallade";
    add("Audinite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Audino: in battle, press Mega to Mega Evolve it into Mega Audino (once per battle).", Type::Normal);
    v.back().move = "Audino";
    add("Diancite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Diancie: in battle, press Mega to Mega Evolve it into Mega Diancie (once per battle).", Type::Rock);
    v.back().move = "Diancie";
    add("Steelixite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Steelix: in battle, press Mega to Mega Evolve it into Mega Steelix (once per battle).", Type::Steel);
    v.back().move = "Steelix";
    add("Latiasite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Latias: in battle, press Mega to Mega Evolve it into Mega Latias (once per battle).", Type::Dragon);
    v.back().move = "Latias";
    add("Latiosite", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Latios: in battle, press Mega to Mega Evolve it into Mega Latios (once per battle).", Type::Dragon);
    v.back().move = "Latios";
    add("Blue Orb", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Kyogre: in battle, press Mega to trigger its Primal Reversion into Primal Kyogre (doesn't use up your Mega Evolution).", Type::Water);
    v.back().move = "Kyogre";
    add("Red Orb", ItemFx::MegaStone, 0, 5000, ItemDef::AFTER_CHAMPION, "Held by Groudon: in battle, press Mega to trigger its Primal Reversion into Primal Groudon (doesn't use up your Mega Evolution).", Type::Ground);
    v.back().move = "Groudon";
    // Fossils from Mt. Moon (revived at the Cinnabar Lab)
    // Key items for the overworld (story gifts, never sold)
    add("Itemfinder", ItemFx::KeyItem, 0, 0, 99, "Use it from the Bag: it beeps toward items hidden on the ground nearby.");
    add("Powder Jar", ItemFx::KeyItem, 0, 0, 99, "A jar for storing berry powder. A keepsake from the Sevii Islands.");
    add("Running Shoes", ItemFx::KeyItem, 0, 0, 99, "Mom's gift: hold the Run button (Shift / B) to dash.");
    add("Bicycle", ItemFx::KeyItem, 0, 0, 99, "A folding bike: much faster than running. Use it from the Bag; some roads (Cycling Road) need it.");
    add("Bike Voucher", ItemFx::KeyItem, 0, 0, 99, "A voucher for a free Bicycle at the Cerulean Bike Shop.");
    add("Old Rod", ItemFx::KeyItem, 0, 0, 99, "An old fishing rod. Face the water and use it to fish.");
    add("Good Rod", ItemFx::KeyItem, 0, 0, 99, "A decent fishing rod for catching better Pokemon.");
    add("Super Rod", ItemFx::KeyItem, 0, 0, 99, "The best fishing rod, for the rarest water Pokemon.");
    add("Oak's Parcel", ItemFx::KeyItem, 0, 0, 99, "A parcel for Professor Oak, from the Viridian Mart.");
    add("Town Map", ItemFx::KeyItem, 0, 0, 99, "A map of the region. Fly from it once you can.");
    add("S.S. Ticket", ItemFx::KeyItem, 0, 0, 99, "A ticket for the S.S. Anne, docked at Vermilion City.");
    add("Silph Scope", ItemFx::KeyItem, 0, 0, 99, "A scope that makes the ghosts of Pokemon Tower visible.");
    add("Poke Flute", ItemFx::KeyItem, 0, 0, 99, "A flute whose tune wakes sleeping Pokemon - like the Snorlax on the road.");
    add("Card Key", ItemFx::KeyItem, 0, 0, 99, "Opens the locked doors of Silph Co.");
    add("Lift Key", ItemFx::KeyItem, 0, 0, 99, "Runs the elevator in Team Rocket's hideout.");
    add("Secret Key", ItemFx::KeyItem, 0, 0, 99, "Opens the door of the Cinnabar Gym.");
    add("Gold Teeth", ItemFx::KeyItem, 0, 0, 99, "The Safari Zone Warden's lost dentures.");
    add("Tea", ItemFx::KeyItem, 0, 0, 99, "A soothing tea. Thirsty guards might let you through for it.");
    add("VS Seeker", ItemFx::KeyItem, 0, 0, 99, "Lets trainers you've beaten know you're up for a rematch.");
    add("Tri-Pass", ItemFx::KeyItem, 0, 0, 99, "A pass for the ferry to One, Two and Three Island.");
    add("Rainbow Pass", ItemFx::KeyItem, 0, 0, 99, "A pass for the ferry to all of the Sevii Islands.");
    add("Ruby", ItemFx::KeyItem, 0, 0, 99, "A gem from Mt. Ember - Celio needs it for his network machine.");
    add("Sapphire", ItemFx::KeyItem, 0, 0, 99, "A gem from the Dotted Hole - Celio needs it too.");
    add("Meteorite", ItemFx::KeyItem, 0, 0, 99, "A meteorite that fell on One Island.");
    // Hoenn's key items (Emerald)
    add("Mach Bike", ItemFx::KeyItem, 0, 0, 99, "Rydel's folding bike that goes really fast - up the sandy slopes, across the cracked floors.");
    add("Acro Bike", ItemFx::KeyItem, 0, 0, 99, "Rydel's bike for tricks: hop along the white rails of Route 119 and the Sky Pillar.");
    add("Devon Goods", ItemFx::KeyItem, 0, 0, 99, "A package of Devon parts - Mr. Stone wants it back in Rustboro.");
    add("Letter", ItemFx::KeyItem, 0, 0, 99, "Mr. Stone's letter for his son Steven, who's in Dewford's Granite Cave.");
    add("Pokenav", ItemFx::KeyItem, 0, 0, 99, "Devon's Pokemon Navigator: its Match Call keeps in touch with Trainers you've battled. Use it from the Bag.");
    add("Wailmer Pail", ItemFx::KeyItem, 0, 0, 99, "A watering can shaped like Wailmer - it wakes up the strange tree on Route 120.");
    add("Devon Scope", ItemFx::KeyItem, 0, 0, 99, "A Devon device that shows Pokemon that can't be seen - like the Kecleon on Route 120.");
    add("Go-Goggles", ItemFx::KeyItem, 0, 0, 99, "Goggles that keep out the sandstorm of the desert on Route 111.");
    add("Soot Sack", ItemFx::KeyItem, 0, 0, 99, "A sack for gathering volcanic ash on Route 113. The Glass Workshop makes flutes from it.");
    add("Magma Emblem", ItemFx::KeyItem, 0, 0, 99, "A medal-like item in the shape of Team Magma's emblem. It opens their hideout in Jagged Pass.");
    add("Basement Key", ItemFx::KeyItem, 0, 0, 99, "The key to New Mauville, under Mauville City.");
    add("Storage Key", ItemFx::KeyItem, 0, 0, 99, "The key to the storeroom of the Abandoned Ship.");
    add("Room 1 Key", ItemFx::KeyItem, 0, 0, 99, "A key to a cabin of the Abandoned Ship.");
    add("Room 2 Key", ItemFx::KeyItem, 0, 0, 99, "A key to a cabin of the Abandoned Ship.");
    add("Room 4 Key", ItemFx::KeyItem, 0, 0, 99, "A key to a cabin of the Abandoned Ship.");
    add("Room 6 Key", ItemFx::KeyItem, 0, 0, 99, "A key to a cabin of the Abandoned Ship.");
    add("Scanner", ItemFx::KeyItem, 0, 0, 99, "A device found in the Abandoned Ship. Captain Stern is looking for it.");
    add("Contest Pass", ItemFx::KeyItem, 0, 0, 99, "A pass for entering Pokemon Contests in Hoenn's Contest Hall.");
    add("Pokeblock Case", ItemFx::KeyItem, 0, 0, 99, "A case for Pokeblocks made in a Berry Blender. Feed them to a Pokemon to make it shine in Contests.");
    add("Eon Ticket", ItemFx::KeyItem, 0, 0, 99, "A ticket for a ferry to a distant southern island.");
    add("S.S. Tidal Ticket", ItemFx::KeyItem, 0, 0, 99, "A ticket for the S.S. Tidal, sailing between Slateport and Lilycove (and on to the Battle Frontier).");
    add("Teachy TV", ItemFx::KeyItem, 0, 0, 99, "A TV that shows handy tips for new trainers.");
    add("Helix Fossil", ItemFx::KeyItem, 0, 0, 99, "A fossil of an ancient spiral-shelled Pokemon. The Cinnabar Lab can revive it.", Type::Rock);
    add("Dome Fossil", ItemFx::KeyItem, 0, 0, 99, "A fossil of an ancient Pokemon with a domed shell. The Cinnabar Lab can revive it.", Type::Rock);
    add("Old Amber", ItemFx::KeyItem, 0, 0, 99, "A piece of amber holding the genes of an ancient flying Pokemon. The Cinnabar Lab can revive it.", Type::Rock);
    // Celadon Game Corner prizes (bought with casino coins; badges 99 keeps them out of the Poke Mart)
    add("Smoke Ball", ItemFx::SpeedMul, 1.10f, 0, 99, "A Game Corner prize. Puffs of smoke help it slip around: moves 10% faster.");
    add("Yellow Flute", ItemFx::ConfuseGuard, 0.3f, 0, 99, "A Game Corner prize. Its tune snaps the holder out of confusion much sooner.");
    // HMs (FireRed): given in the story, never used up; on the map they clear the way once you have the badge
    for (int n = 1; n <= 8; n++) {
        ItemDef d;
        d.name = TextFormatHm(n);
        d.fx = ItemFx::TM;
        d.move = kHmMovesPub[n];
        d.hmNo = n;
        d.price = 0;
        d.badges = 99;   // not sold
        const MoveDef* m = FindMove(d.move);
        d.type = m ? m->type : Type::Normal;
        d.desc = std::string("Teaches ") + d.move + " to a Pokemon that can learn it. It's never used up, and the move can be forgotten any time. "
                 "Out in the world, " + d.move + " clears the way once you have the right badge.";
        v.push_back(d);
    }
    // TMs (Scarlet/Violet numbering): used up when taught, from Party & Box
    for (const TmInfo& t : kTms) {
        ItemDef d;
        char buf[64];
        std::snprintf(buf, sizeof buf, "TM%03d %s", t.no, t.move);
        d.name = buf;
        d.fx = ItemFx::TM;
        d.move = t.move;
        d.tmNo = t.no;
        d.price = t.price * 5;   // (FireRed money: TMs cost a few thousand)
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
bool IsHoldable(const ItemDef& d) {
    return d.fx != ItemFx::Ball && d.fx != ItemFx::Stone && d.fx != ItemFx::TM && d.fx != ItemFx::KeyItem && d.fx != ItemFx::ExpAll &&
           d.fx != ItemFx::Medicine && d.fx != ItemFx::PartyUse && d.fx != ItemFx::Field && d.fx != ItemFx::Valuable;
}
bool IsPartyUse(const ItemDef& d) { return d.fx == ItemFx::PartyUse; }
int SellPrice(const ItemDef& d) { return d.fx == ItemFx::KeyItem || d.fx == ItemFx::ExpAll || IsHM(d) ? 0 : d.price / 2; }
bool IsTM(const ItemDef& d) { return d.fx == ItemFx::TM; }
bool IsMegaStone(const ItemDef& d) { return d.fx == ItemFx::MegaStone; }
bool IsHM(const ItemDef& d) { return d.fx == ItemFx::TM && d.hmNo > 0; }
namespace {
const char* kHmMoves[9] = {"", "Cut", "Fly", "Surf", "Strength", "Flash", "Rock Smash", "Waterfall", "Dive"};
// FireRed: Flash needs the Boulder Badge, Cut the Cascade, Fly the Thunder, Strength the Rainbow,
// Surf the Soul, Rock Smash the Marsh and Waterfall the Volcano Badge (badges in Kanto order).
const int kHmBadge[9] = {0, 2, 3, 5, 4, 1, 6, 7, 99};   // (Dive: Hoenn only - the Mind Badge)
#include "hm_compat.inc"
}
const char* HmMove(int n) { return n >= 1 && n <= 8 ? kHmMoves[n] : ""; }
int HmBadges(int n) { return n >= 1 && n <= 8 ? kHmBadge[n] : 99; }
bool CanLearnHM(const std::string& species, const std::string& move) {
    for (const auto& e : kHmCompat) {
        if (species != e.species) continue;
        std::string all = std::string("|") + e.moves + "|";
        return all.find("|" + move + "|") != std::string::npos;
    }
    return false;
}
bool IsMedicine(const ItemDef& d) { return d.fx == ItemFx::Medicine; }
