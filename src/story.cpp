#include "story.h"
#include "wild.h"
#include "embedded.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include "data.h"
#include "raylib.h"

namespace {

StoryDef gStory;

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\""), b = s.find_last_not_of(" \t\r\n\"");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::vector<std::string> SplitList(const std::string& v) {
    std::vector<std::string> out;
    std::string part;
    std::istringstream ss(v);
    while (std::getline(ss, part, ',')) {
        part = Trim(part);
        if (!part.empty()) out.push_back(part);
    }
    return out;
}

bool IsNo(std::string v) {
    for (char& c : v) c = (char)std::tolower((unsigned char)c);
    return v == "no" || v == "0" || v == "none" || v == "false" || v == "off";
}

AILevel ParseLevel(const std::string& v) {
    std::string l = v;
    for (char& c : l) c = (char)std::tolower((unsigned char)c);
    if (l == "easy") return AILevel::Easy;
    if (l == "hard") return AILevel::Hard;
    return AILevel::Normal;
}

const char* kDefaultStory = R"STORY(

start_level = 5
rare_every = 5
training_max_foes = 3

map_image = map.png
town = Pallet Town, 261, 642
town = Viridian City, 261, 500
town = Pewter City, 261, 309
town = Cerulean City, 737, 261
town = Saffron City, 737, 404
town = Celadon City, 594, 404
town = Lavender Town, 928, 404
town = Vermilion City, 737, 547
town = Fuchsia City, 642, 690
town = Cinnabar Island, 261, 785
town = Indigo Plateau, 167, 261

[stage]
name = Route 1
map = 261, 571
trainer = Youngster Ty
say = I just caught this one! Let's battle!
team = Rattata, Pidgey -1
level = -2
cpu = easy

[stage]
name = Viridian Forest
map = 261, 410
trainer = Bug Catcher Rick
say = Bugs are the best, and I'll prove it!
team = Weedle, Caterpie, Weedle +1
level = -2
cpu = easy

[stage]
name = Pewter Outskirts
map = 261, 355
trainer = Camper Liam
say = I trained in Brock's shadow. Let me show you!
team = Geodude -1, Sandshrew
level = -1
cpu = easy

[stage]
name = Pewter Gym
map = 261, 309
trainer = Leader Brock
say = My rock-hard will cannot be broken! The pillars here stop any shot - if you can't break them.
team = Geodude -1, Onix +1
level = 1
cpu = normal
arena = rock
badge = Boulder

[stage]
name = Mt. Moon
map = 430, 309
trainer = Hiker Dale
say = Down here in the dark you have to be tough!
team = Geodude, Zubat -1, Onix +1
level = -1
cpu = easy

[stage]
name = Nugget Bridge
map = 737, 195
trainer = Lass Mia
say = Nobody crosses this bridge without a battle!
team = Pidgey, Nidoran F +1
level = 0
cpu = normal

[stage]
name = Route 24
map = 800, 160
trainer = Swimmer Kai
say = Ready to get soaked?
team = Horsea, Shellder, Goldeen +1
level = 0
cpu = normal

[stage]
name = Cerulean Gym
map = 737, 261
trainer = Leader Misty
say = My pool is my stage! Water types fly through it - everyone else sinks.
team = Staryu -1, Starmie +2
level = 1
cpu = normal
arena = water
badge = Cascade

[stage]
name = Route 6
map = 737, 480
trainer = Engineer Volt
say = Careful, I'm fully charged!
team = Magnemite, Voltorb +1
level = 0
cpu = normal

[stage]
name = S.S. Anne
map = 690, 600
trainer = Sailor Bruno
say = Sailors don't back down from a fight!
team = Machop, Shellder, Tentacool
level = 0
cpu = normal

[stage]
name = Diglett's Cave
map = 830, 547
trainer = Hiker Rocco
say = Ground beats Electric, everyone knows that. Let's warm you up!
team = Diglett, Geodude, Dugtrio +2
level = 0
cpu = normal

[stage]
name = Vermilion Gym
map = 737, 547
trainer = Lt. Surge
say = Hey kid! My tesla coils fry anyone who gets close - except my Electric troops, they love it!
team = Voltorb -2, Pikachu -5, Raichu +1
level = 2
cpu = normal
arena = electric
badge = Thunder

[stage]
name = Rock Tunnel
map = 928, 300
trainer = Picnicker Jo
say = It's so dark in here... let's battle to pass the time!
team = Pidgey, Rattata, Oddish
level = 0
cpu = normal

[stage]
name = Route 8
map = 830, 404
trainer = Gambler Sid
say = I'll bet my coins on this one!
team = Growlithe, Vulpix, Meowth +1
level = 0
cpu = normal

[stage]
name = Celadon Park
map = 500, 404
trainer = Beauty Rosa
say = My flowers bloom brightest in battle.
team = Oddish, Bellsprout, Bulbasaur +1
level = 1
cpu = normal

[stage]
name = Celadon Gym
map = 594, 404
trainer = Leader Erika
say = Welcome to my greenhouse. Mind the trees... and please don't set them on fire.
team = Victreebel +1, Tangela -4, Vileplume +1
level = 2
cpu = normal
arena = grass
badge = Rainbow

[stage]
name = Cycling Road
map = 405, 560
trainer = Biker Rex
say = Outta my way, slowpoke!
team = Koffing, Grimer, Weezing +2
level = 1
cpu = normal

[stage]
name = Safari Zone
map = 642, 630
trainer = Ranger Ana
say = The wild ones here are tougher than you think.
team = Exeggcute, Rhyhorn, Venonat, Kangaskhan +1
level = 1
cpu = normal

[stage]
name = Fuchsia Dojo
map = 740, 690
trainer = Black Belt Ken
say = Hi-yah! My fists are my creed!
team = random Fighting, random Fighting, random
level = 1
cpu = normal

[stage]
name = Fuchsia Gym
map = 642, 690
trainer = Leader Koga
say = Fwahaha! My swamp shrinks the field - the spikes and pools only spare my poison.
team = Koffing, Muk +2, Koffing, Weezing +6
level = 2
cpu = hard
arena = poison
badge = Soul

[stage]
name = Silph Co.
map = 737, 355
trainer = Scientist Mel
say = My research says you won't win.
team = Magnemite, Voltorb, Magneton +1, Electrode +2
level = 1
cpu = normal

[stage]
name = Pokemon Tower
map = 928, 404
trainer = Channeler Ivy
say = The spirits are restless tonight...
team = Gastly -1, Gastly -1, Haunter +1
level = 1
cpu = normal

[stage]
name = Saffron Streets
map = 670, 404
trainer = Psychic Tyron
say = I saw this battle in a vision. You lose.
team = Slowpoke, Drowzee, Kadabra +1, Mr. Mime +2
level = 2
cpu = hard

[stage]
name = Saffron Gym
map = 737, 404
trainer = Leader Sabrina
say = Up is down here. My portals move as they please - and so will you.
team = Kadabra -3, Mr. Mime -4, Venomoth -3, Alakazam +2
level = 3
cpu = hard
arena = psychic
badge = Marsh

[stage]
name = Seafoam Islands
map = 436, 787
trainer = Skier Lena
say = It's freezing in here! Let's warm up with a battle.
team = Seel, Shellder, Dewgong +1, Cloyster +2
level = 2
cpu = normal

[stage]
name = Cinnabar Mansion
map = 310, 800
trainer = Burglar Vic
say = Heh, you didn't see me here.
team = Koffing, Grimer, Muk +1, Weezing +2
level = 2
cpu = hard

[stage]
name = Cinnabar Volcano
map = 225, 760
trainer = Firebreather Ash
say = Feel the heat!
team = Ponyta, Magmar, Charmeleon, Rapidash +2
level = 2
cpu = hard

[stage]
name = Cinnabar Gym
map = 261, 785
trainer = Leader Blaine
say = Hah! Welcome to my volcano! Watch the lava - it bites back!
team = Growlithe -3, Ponyta -5, Rapidash -3, Arcanine +2
level = 3
cpu = hard
arena = fire
badge = Volcano

[stage]
name = Rocket Hideout
map = 300, 470
trainer = Rocket Grunt Jessie
say = Prepare for trouble! The boss doesn't want visitors.
team = Ekans, Lickitung, Arbok +2
level = 2
cpu = hard

[stage]
name = Rocket Hideout B2
map = 167, 450
trainer = Rocket Grunt James
say = Make it double! You won't get any closer.
team = Koffing, Victreebel, Weezing +2
level = 2
cpu = hard

[stage]
name = Rocket Hideout B4
map = 200, 500
trainer = Boss Giovanni
say = So, you've come all the way down here. Team Rocket's boss doesn't lose twice.
team = Onix -1, Rhyhorn -1, Kangaskhan +2
level = 3
cpu = hard

[stage]
name = Viridian Gym
map = 261, 500
trainer = Boss Giovanni
say = So you've found my lair. My cannons fire every ten seconds. Let's see you dodge them.
team = Rhyhorn -3, Dugtrio -6, Nidoqueen -4, Nidoking -3, Rhydon +2
level = 4
cpu = hard
arena = ground
badge = Earth

[stage]
name = Victory Road
map = 182, 340
trainer = Cooltrainer Caroline
say = Only trainers with all eight badges make it this far. Let's see if you deserve the League!
team = Golbat -2, Machoke -1, Marowak -1, Onix +2
lv = 50
cpu = hard
venue = cave

# ---------------------------------------------------------------- The Pokemon League (Indigo Plateau)
# The Elite Four and the Champion are fought in a row: lose any of them and the challenge
# starts over from Lorelei. (league = 1-4 the Elite Four in order, 5 the Champion.)
[stage]
name = Lorelei's Chamber
map = 140, 236
trainer = Elite Four Lorelei
say = Welcome to the Pokemon League. I am Lorelei of the Elite Four. My icy Pokemon will freeze you right where you stand!
team = Dewgong -2, Cloyster -3, Slowbro -2, Jynx, Lapras
lv = 54
cpu = hard
arena = ice
league = 1

[stage]
name = Bruno's Ring
map = 156, 222
trainer = Elite Four Bruno
say = I am Bruno of the Elite Four! My Pokemon and I have trained our bodies to the limit. Hoo hah! Prepare to be crushed!
team = Onix -5, Hitmonchan -3, Hitmonlee -3, Onix -2, Machamp
lv = 56
cpu = hard
arena = fighting
league = 2

[stage]
name = Agatha's Hall
map = 178, 222
trainer = Elite Four Agatha
say = I'm Agatha of the Elite Four. Oak's taken a shine to you, has he? That old duff once was tough. Let me show you how a real trainer fights!
team = Gengar -4, Golbat -4, Haunter -5, Arbok -2, Gengar
lv = 58
cpu = hard
arena = ghost
league = 3

[stage]
name = Lance's Den
map = 194, 236
trainer = Elite Four Lance
say = I am Lance, the last of the Elite Four - and a Dragon master. Dragons are mythical Pokemon: hard to catch and harder to beat!
team = Gyarados -4, Dragonair -6, Dragonair -6, Aerodactyl -2, Dragonite
lv = 60
cpu = hard
arena = dragon
league = 4

[stage]
name = Champion's Stadium
map = 167, 252
trainer = Champion Blue
say = Hey! I was looking forward to seeing you. While you were working your way up, I became the Pokemon League Champion! You know what that means? I'm the most powerful trainer in the world!
team = Pidgeot -4, Alakazam -6, Rhydon -4, rival_trio1 -2, rival_trio2 -2, rival
lv = 63
cpu = hard
catch = no
rival = yes
arena = champion
league = 5
)STORY";

void Parse(const char* text, StoryDef& st, std::string& log) {
    st = StoryDef();
    std::istringstream ss(text);
    std::string line;
    StageDef* cur = nullptr;
    int ln = 0;
    while (std::getline(ss, line)) {
        ln++;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = Trim(line);
        if (line.empty()) continue;
        if (line == "[stage]") {
            st.stages.push_back(StageDef());
            cur = &st.stages.back();
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        if (!cur) {
            if (k == "starters") st.starters = SplitList(v);
            else if (k == "start_level") st.startLevel = std::max(1, std::min(MAX_LEVEL, std::atoi(v.c_str())));
            else if (k == "rare_every") st.rareEvery = std::max(1, std::atoi(v.c_str()));
            else if (k == "training_max_foes") st.trainingMaxFoes = std::max(1, std::min(MAX_TEAM_SIZE, std::atoi(v.c_str())));
            else if (k == "map_image") st.mapImage = v;
            else if (k == "town") {
                auto parts = SplitList(v);
                if (parts.size() >= 3) {
                    StoryDef::Town t;
                    t.name = parts[0];
                    t.x = (float)std::atof(parts[1].c_str());
                    t.y = (float)std::atof(parts[2].c_str());
                    st.towns.push_back(t);
                }
            }
            else if (k == "story_rev") st.rev = std::max(1, std::atoi(v.c_str()));
            else if (k == "training_reward_every") {}  // old setting, replaced by rare encounters
        } else {
            if (k == "name") cur->name = v;
            else if (k == "trainer") cur->trainer = v;
            else if (k == "say") cur->say = v;
            else if (k == "team") cur->team = SplitList(v);
            else if (k == "level") cur->levelOffset = std::atoi(v.c_str());
            else if (k == "lv") cur->baseLevel = std::max(0, std::atoi(v.c_str()));
            else if (k == "cpu") cur->cpu = ParseLevel(v);
            else if (k == "reward") cur->reward = !IsNo(v);
            else if (k == "arena" || k == "stadium") {
                if (!ParseArena(v, cur->arena)) log += TextFormat("  story.txt line %d: unknown arena '%s'\n", ln, v.c_str());
            } else if (k == "badge") cur->badge = v;
            else if (k == "catch" || k == "catchable") cur->catchMode = IsNo(v) ? 0 : 1;
            else if (k == "rival") cur->rival = !IsNo(v);
            else if (k == "snorlax") cur->snorlax = std::max(0, std::atoi(v.c_str()));
            else if (k == "outdoor") cur->outdoor = !IsNo(v);
            else if (k == "venue") cur->venue = v;
            else if (k == "league") cur->league = std::max(0, std::min(5, std::atoi(v.c_str())));
            else if (k == "coins" || k == "prize") cur->coins = std::max(0, std::atoi(v.c_str()));
            else if (k == "map") {
                auto parts = SplitList(v);
                if (parts.size() >= 2) {
                    cur->mapX = (float)std::atof(parts[0].c_str());
                    cur->mapY = (float)std::atof(parts[1].c_str());
                }
            }
            else log += TextFormat("  story.txt line %d: unknown setting '%s'\n", ln, k.c_str());
        }
    }
    for (auto& s : st.stages) {
        if (s.team.empty()) s.team.push_back("random");
        if (s.trainer.empty()) s.trainer = "Trainer";
    }
}

int RandomBase(Type want, bool anyType) {
    std::vector<int> pool;
    for (int i = 0; i < (int)AllSpecies().size(); i++) {
        if (!IsBaseForm(i) || !InSpeciesPool(i) || AllSpecies()[i].rare) continue;  // legendaries only as rare encounters
        const SpeciesDef& sp = AllSpecies()[i];
        if (anyType || sp.type1 == want || sp.type2 == want) pool.push_back(i);
    }
    if (pool.empty()) {
        for (int i = 0; i < (int)AllSpecies().size(); i++) if (IsBaseForm(i) && InSpeciesPool(i)) pool.push_back(i);
        if (pool.empty()) for (int i = 0; i < (int)AllSpecies().size(); i++) pool.push_back(i);
    }
    return pool[GetRandomValue(0, (int)pool.size() - 1)];
}

} // namespace

const StoryDef& Story() { return gStory; }

void LoadStory(std::string& log) {
    std::string path = DataDir() + "story.txt";
    char* text = LoadFileText(path.c_str());
    if (text) {
        Parse(text, gStory, log);
        UnloadFileText(text);
        log += TextFormat("Story: %d stages from story.txt\n", (int)gStory.stages.size());
    } else {
        Parse(kDefaultStory, gStory, log);
        log += "Story: built-in ladder (no story.txt found)\n";
    }
    std::vector<std::string> ok;
    // Starters: every Pokemon with "starter = yes" in its pokemon.txt (Pokedex order).
    for (const auto& sp : AllSpecies())
        if (sp.starter) ok.push_back(sp.name);
    if (!ok.empty()) gStory.starters.clear();  // tags win over a starters = line
    for (const auto& s : gStory.starters) {
        if (SpeciesIndex(s) >= 0) ok.push_back(AllSpecies()[SpeciesIndex(s)].name);
        else log += "  starter '" + s + "' is not a known creature, skipped\n";
    }
    if (ok.empty()) {
        for (int i = 0; i < (int)AllSpecies().size() && ok.size() < 3; i++)
            if (IsBaseForm(i)) ok.push_back(AllSpecies()[i].name);
    }
    gStory.starters = ok;
    if (gStory.stages.empty()) Parse(kDefaultStory, gStory, log);
}

namespace {
int gRivalStarter = -1;
bool gRivalCoop = false;
}

void SetRivalContext(int rivalStarter, bool coop) {
    gRivalStarter = rivalStarter;
    gRivalCoop = coop;
}

int RivalStarterFor(const std::vector<int>& mine) {
    std::vector<int> starters;
    for (const auto& n : Story().starters) { int i = SpeciesIndex(n); if (i >= 0) starters.push_back(i); }
    if (starters.empty()) return -1;
    auto beats = [](int a, int b) { return TypeEffectiveness(AllSpecies()[a].type1, AllSpecies()[b].type1, AllSpecies()[b].type2) > 1; };
    if (mine.size() >= 2) {   // co-op: the one nobody took
        for (int s : starters)
            if (std::find(mine.begin(), mine.end(), s) == mine.end()) return s;
    }
    int me = mine.empty() ? starters[0] : mine[0];
    for (int s : starters)
        if (s != me && beats(s, me)) return s;
    for (int s : starters) if (s != me) return s;
    return starters[0];
}

const char* SeasonName(int s) {
    static const char* n[4] = {"Spring", "Summer", "Fall", "Winter"};
    return n[((s % 4) + 4) % 4];
}

Weather RollSeasonWeather(int season) {
    int r = GetRandomValue(1, 100);
    switch (((season % 4) + 4) % 4) {
        case 0: return r <= 18 ? Weather::Sun : (r <= 36 ? Weather::Rain : Weather::None);   // spring: sun or showers
        case 1: return r <= 40 ? Weather::Sun : Weather::None;                               // summer: harsh sunlight
        case 2: return r <= 40 ? Weather::Rain : Weather::None;                              // fall: rain
        default: return r <= 45 ? Weather::Snow : Weather::None;                             // winter: snow
    }
}

int StoryStageForRev(int stage, int fromRev) {
    if (fromRev >= Story().rev || Story().rev < 2) return stage;
    if (fromRev < 2) {   // story_rev 2 added rival battles before these stages of the old ladder
        static const int kInserted[] = {0, 1, 5, 10, 21, 23, 32};
        int add = 0;
        for (int p : kInserted) if (p < stage) add++;
        stage += add;
    }
    if (Story().rev >= 3) {   // story_rev 3: Snorlax on Routes 16 and 12, the Dojo moved to Saffron
        if (stage >= 28) stage += 3;
        else if (stage >= 27) stage += 2;
        else if (stage >= 20) stage += 1;
    }
    return std::min(stage, (int)Story().stages.size());
}

StageDef RematchStage(const StageDef& stage) {
    StageDef r = stage;
    int top = -99;
    for (const auto& e0 : stage.team) {   // the highest "+N / -N" on the team is its ace
        std::string e = Trim(e0);
        int extra = 0;
        size_t sp = e.find_last_of(' ');
        if (sp != std::string::npos && sp + 1 < e.size() && (e[sp + 1] == '+' || e[sp + 1] == '-') && std::isdigit((unsigned char)e.back()))
            extra = std::atoi(e.c_str() + sp + 1);
        top = std::max(top, extra);
    }
    if (top == -99) top = 0;
    r.baseLevel = 0;            // no fixed level: follow the challenger
    r.levelOffset = 3 - top;    // the ace lands 3-4 levels above your team's average
    r.cpu = AILevel::Hard;
    return r;
}

int LeagueStartStage() {
    for (int i = 0; i < (int)Story().stages.size(); i++)
        if (Story().stages[i].league == 1) return i;
    return -1;
}

int TowerTeamSize(int floor) { return floor < 10 ? 3 : floor < 20 ? 4 : floor < 30 ? 5 : 6; }

StageDef TowerStage(int floor, uint32_t seed) {
    floor = std::max(1, floor);
    uint32_t h = seed ^ ((uint32_t)floor * 2654435761u);
    auto pick = [&](int n) {
        h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
        return n <= 1 ? 0 : (int)(h % (uint32_t)n);
    };
    pick(1);
    int climb = std::min(30, (floor - 5) / 4);   // levels above your team's average: -1 at first, +1 every 4 floors
    if (floor % 10 == 0) {   // a boss floor: a gym leader, Elite Four member or the Champion, in their own arena
        std::vector<int> bosses;
        for (int i = 0; i < (int)Story().stages.size(); i++)
            if (Story().stages[i].IsBoss()) bosses.push_back(i);
        if (!bosses.empty()) {
            const StageDef& b = Story().stages[bosses[pick((int)bosses.size())]];
            StageDef r = RematchStage(b);   // their own team, at your level, at full strength
            r.levelOffset += std::max(0, climb - 2);
            r.towerFloor = floor;
            r.name = TextFormat("Battle Tower - Floor %d", floor);
            r.badge.clear();
            r.league = 0;
            r.coins = -1;
            r.venue = "stadium";
            r.trainer = "Tower Boss " + b.trainer;
            // their team fills out to the floor's size with more of their favourite type
            Type fav = Type::Normal;
            for (const auto& e0 : b.team) {
                std::string e = Trim(e0);
                size_t sp = e.find_last_of(' ');
                if (sp != std::string::npos && (e[sp + 1] == '+' || e[sp + 1] == '-')) e = Trim(e.substr(0, sp));
                int idx = SpeciesIndex(e);
                if (idx >= 0) fav = AllSpecies()[idx].type1;
            }
            while ((int)r.team.size() < TowerTeamSize(floor)) r.team.insert(r.team.begin(), std::string("random ") + TypeName(fav));
            return r;
        }
    }
    struct Cls { const char* name; const char* type; };
    static const Cls kCls[] = {{"Ace Trainer", ""}, {"Black Belt", "Fighting"}, {"Bird Keeper", "Flying"}, {"Swimmer", "Water"},
                               {"Psychic", "Psychic"}, {"Hiker", "Rock"}, {"Bug Maniac", "Bug"}, {"Channeler", "Ghost"},
                               {"Burglar", "Fire"}, {"Rocker", "Electric"}, {"Beauty", "Grass"}, {"Juggler", "Poison"},
                               {"Ruin Maniac", "Ground"}, {"Gentleman", "Normal"}, {"Ice Skater", "Ice"}, {"Cooltrainer", ""},
                               {"Veteran", ""}, {"Tamer", ""}};
    static const char* kNames[] = {"Alex", "Sam", "Riley", "Jordan", "Casey", "Morgan", "Taylor", "Quinn", "Avery", "Drew",
                                   "Kai", "Rowan", "Sky", "Jamie", "Reese", "Blake", "Harper", "Logan", "Mika", "Nova"};
    static const char* kSay[] = {"Only the strongest make it this high. Let's see if you're one of them!",
                                 "I've been waiting on this floor all day. Don't disappoint me!",
                                 "Every floor gets harder. This is where your streak ends!",
                                 "The Tower doesn't care about badges - only wins.",
                                 "My team was built for this. Show me yours!",
                                 "You're still climbing? Impressive. Not for long!"};
    const Cls& c = kCls[pick((int)(sizeof kCls / sizeof kCls[0]))];
    StageDef st;
    st.towerFloor = floor;
    st.name = TextFormat("Battle Tower - Floor %d", floor);
    st.trainer = std::string(c.name) + " " + kNames[pick((int)(sizeof kNames / sizeof kNames[0]))];
    st.say = kSay[pick((int)(sizeof kSay / sizeof kSay[0]))];
    st.venue = "stadium";
    st.levelOffset = climb;
    st.cpu = floor < 5 ? AILevel::Normal : AILevel::Hard;
    st.catchMode = 0;
    st.reward = false;
    int size = TowerTeamSize(floor);
    for (int i = 0; i < size; i++) {
        std::string e = (c.type[0] && pick(2) == 0) ? std::string("random ") + c.type : std::string("random");
        if (i == size - 1) e += " +2";   // their ace
        st.team.push_back(e);
    }
    if (floor % 5 == 0) {   // every fifth floor is fought in one of the gym arenas
        static const ArenaKind kGyms[] = {ArenaKind::Rock, ArenaKind::Water, ArenaKind::Electric, ArenaKind::Grass,
                                          ArenaKind::Poison, ArenaKind::Psychic, ArenaKind::Fire, ArenaKind::Ground};
        st.arena = kGyms[pick(8)];
    }
    return st;
}

std::vector<CombatantSpec> MakeCpuTeam(const StageDef* stage, int avg, int teamSize) {
    if (!stage && WildActive()) {   // training: the Pokemon that live in the chosen area
        auto t = MakeWildTeam(teamSize, avg);
        if (!t.empty()) return t;
    }
    std::vector<std::string> entries;
    int offset = 0;
    if (stage) {
        entries = stage->team;
        offset = stage->levelOffset;
    } else {
        for (int i = 0; i < teamSize; i++) entries.push_back("random");
        offset = GetRandomValue(-1, 1);
    }
    if ((int)entries.size() > MAX_TEAM_SIZE) entries.resize(MAX_TEAM_SIZE);
    std::vector<CombatantSpec> team;
    std::vector<int> used;
    for (const auto& e0 : entries) {
        // A trailing "+N" / "-N" moves this one's level.
        std::string e = Trim(e0);
        int extra = 0;
        size_t sp = e.find_last_of(' ');
        if (sp != std::string::npos && sp + 1 < e.size() && (e[sp + 1] == '+' || e[sp + 1] == '-') &&
            std::isdigit((unsigned char)e.back())) {
            extra = std::atoi(e.c_str() + sp + 1);
            e = Trim(e.substr(0, sp));
        }
        int level = std::max(2, std::min(MAX_LEVEL, avg + offset + extra + GetRandomValue(0, 1)));
        if (stage && stage->baseLevel > 0) {   // built for a level, like the games; your level only nudges it
            int nudge = std::max(-2, std::min(3, (int)std::lround((avg - stage->baseLevel) * 0.25f)));
            level = std::max(2, std::min(MAX_LEVEL, stage->baseLevel + nudge + extra + GetRandomValue(0, 1)));
        }
        int idx = -1;
        std::string l = e;
        for (char& c : l) c = (char)std::tolower((unsigned char)c);
        if (l.rfind("rival", 0) == 0) {
            int st = gRivalStarter >= 0 ? gRivalStarter : SpeciesIndex(Story().starters.empty() ? "Pikachu" : Story().starters[0]);
            if (l == "rival_partner") {
                if (!gRivalCoop) continue;
                idx = SpeciesIndex("Pikachu");
            } else if (l == "rival_trio1" || l == "rival_trio2") {
                // Red/Blue: the rival's other two of Growlithe, Exeggcute and Gyarados don't share his starter's type.
                std::vector<std::string> trio;
                Type t = st >= 0 ? AllSpecies()[st].type1 : Type::Normal;
                if (t != Type::Fire) trio.push_back("Growlithe");
                if (t != Type::Grass) trio.push_back("Exeggcute");
                if (t != Type::Water) trio.push_back("Gyarados");
                idx = SpeciesIndex(trio[std::min(trio.size() - 1, (size_t)(l == "rival_trio1" ? 0 : 1))]);
            } else {
                idx = st;
            }
            if (idx < 0) idx = RandomBase(Type::Normal, true);
            idx = EvolvedFormAt(idx, level);
        } else if (l.rfind("random", 0) == 0) {
            std::string ty = Trim(e.substr(6));
            Type t = Type::Normal;
            bool any = ty.empty() || !ParseType(ty, t) || t == Type::None;
            for (int tries = 0; tries < 8; tries++) {
                idx = RandomBase(t, any);
                if (std::find(used.begin(), used.end(), idx) == used.end()) break;
            }
            idx = EvolvedFormAt(idx, level);
        } else {
            idx = SpeciesIndex(e);
            if (idx < 0) idx = RandomBase(Type::Normal, true);
        }
        used.push_back(idx);
        CombatantSpec c;
        c.species = idx;
        c.level = level;
        c.moves = MovesAtLevel(AllSpecies()[idx], level);
        team.push_back(c);
    }
    return team;
}

std::vector<int> RewardChoices(const std::vector<std::string>& owned, int n, int storyStage, int badges) {
    std::vector<int> pool, reachable;
    for (int i = 0; i < (int)AllSpecies().size(); i++) {
        if (!IsBaseForm(i) || !InSpeciesPool(i) || AllSpecies()[i].rare) continue;
        if (storyStage >= 0 && !WildAreas().empty() && !WildAvailableNow(i, storyStage, badges)) continue;   // where it lives
        reachable.push_back(i);
        bool have = false;
        for (const auto& o : owned) {
            // Owning any stage of a line counts as owning the line.
            int oi = SpeciesIndex(o);
            if (oi >= 0 && BaseFormOf(oi) == i) have = true;
        }
        if (!have) pool.push_back(i);
    }
    if (pool.empty()) pool = reachable;   // you have every line around here: any of them again
    for (int i = (int)pool.size() - 1; i > 0; i--) std::swap(pool[i], pool[GetRandomValue(0, i)]);
    if ((int)pool.size() > n) pool.resize(n);
    return pool;
}



int WinCoins(const StageDef* stage, int lv) {
    if (stage && stage->coins >= 0) return stage->coins;
    if (!stage) return 15 + 3 * lv;
    if (stage->IsChampion()) return 600 + 25 * lv;
    if (stage->IsLeague()) return 300 + 20 * lv;
    if (stage->towerFloor > 0) return (40 + 7 * lv + std::min(stage->towerFloor, 200) * 4) * (stage->towerFloor % 10 == 0 ? 3 : 1);
    if (stage->IsGym()) return 150 + 15 * lv;
    return 30 + 6 * lv;
}

int PickRareSpecies(const std::vector<std::string>& owned, int teamLevel) {
    (void)teamLevel;
    if (WildActive()) {   // the rarest finds of the training area
        int r = PickRareWild(owned);
        if (r >= 0) return r;
    }
    std::vector<int> fresh, all;
    for (int i = 0; i < (int)AllSpecies().size(); i++) {
        if (!AllSpecies()[i].rare || !InSpeciesPool(i)) continue;
        if (AllSpecies()[i].legendary) continue;   // legendaries live in their own lairs (Legendary Pokemon)
        all.push_back(i);
        bool have = false;
        for (const auto& o : owned) have |= SpeciesIndex(o) == i;
        if (!have) fresh.push_back(i);
    }
    const auto& pool = !fresh.empty() ? fresh : all;
    if (pool.empty()) return GetRandomValue(0, (int)AllSpecies().size() - 1);
    return pool[GetRandomValue(0, (int)pool.size() - 1)];
}
