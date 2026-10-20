// Online co-op: connecting, the message protocol, and the battle running on both computers.
//
// Each player has their own game: their own menus, shop, healing center and Party & Box.
// Battles start when both pick the same one in the co-op hub. During a battle both computers
// run the same simulation from the same starting state; only button presses travel. Your own
// presses show up straight away: the partner's are predicted (they're usually still doing the
// same thing) and when their real ones arrive, the battle is quietly re-run from the last
// moment both were known ("rollback"). A checksum every second catches any drift, and the host
// then sends its copy of the battle.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "legend.h"
#include "wild.h"
#include "net.h"
#include "net_state.h"
#include "settings.h"
#include "sprites.h"
#include "storymap.h"
#include "ui.h"

#ifdef ARENA_AUTOTEST
extern bool gAutoNetInput;
#endif

namespace {

constexpr int KIND_LEGEND = 200;  // ready kinds 200..204: a legendary encounter (legend.h)
constexpr int KIND_TOWER = 250;   // ready kind: the Battle Tower (1 story, 2 training, 3+N rematch stage N)
constexpr uint32_t PROTOCOL = 17;   // 17: Gen 2 Pokemon 161-251 and their abilities, 16: rolling moves and gym centerpieces, 15: sweeping beams, tethers, grapples and throws (battle state), 14: the legendaries moved to a Map tab (hub items renumbered), 13: new battle effects (particle kinds), 5: dodge and jump, 6: swap back, 7: target switching, 8: gym rematches, reconnect, 9: the League and the Battle Tower, 10: legendary encounters, 11: Gen 2 starters, the rival's two starters, 12: shiny Pokemon
constexpr float TICK = 1.0f / 60.0f;
constexpr int INPUT_DELAY = 2;    // ticks: our own presses take effect this much later (33 ms)
constexpr int MAX_AHEAD = 15;     // ticks we may run ahead of what the partner has confirmed

enum Msg : uint8_t {
    M_HELLO = 1, M_PROFILE, M_COOP, M_READY, M_GO, M_PICKS, M_CANCEL, M_BATTLE, M_INPUT, M_HASH,
    M_RESYNC_REQ, M_RESYNC, M_CATCH, M_LEAVE_BATTLE, M_ACTIVITY, M_BYE, M_TRADE, M_TRADE_GO, M_CURSOR, M_FOSSIL, M_PICKVIEW, M_CATCHSEL, M_THROW
};

// ---- message packing
struct W {
    std::string s;
    explicit W(Msg m) { s.push_back((char)m); }
    W& u8(int v) { s.push_back((char)(uint8_t)v); return *this; }
    W& i32(int32_t v) { s.append((const char*)&v, 4); return *this; }
    W& u64(uint64_t v) { s.append((const char*)&v, 8); return *this; }
    W& f32(float v) { s.append((const char*)&v, 4); return *this; }
    W& str(const std::string& v) { i32((int32_t)v.size()); s += v; return *this; }
    void Send() { net::Send(s); }
};
struct R {
    const std::string& s;
    size_t p = 1;
    bool ok = true;
    explicit R(const std::string& m) : s(m) {}
    template <class T> T get() {
        T v{};
        if (p + sizeof(T) > s.size()) { ok = false; return v; }
        std::memcpy(&v, s.data() + p, sizeof(T));
        p += sizeof(T);
        return v;
    }
    int u8() { return get<uint8_t>(); }
    int32_t i32() { return get<int32_t>(); }
    uint64_t u64() { return get<uint64_t>(); }
    float f32() { return get<float>(); }
    std::string str() {
        int32_t n = i32();
        if (n < 0 || p + (size_t)n > s.size()) { ok = false; return ""; }
        std::string v = s.substr(p, (size_t)n);
        p += (size_t)n;
        return v;
    }
};

void PackInput(W& w, const PlayerInput& in) {
    w.f32(in.move.x).f32(in.move.y);
    int bits = 0;
    for (int i = 0; i < 4; i++) bits |= in.attack[i] ? (1 << i) : 0;
    bits |= in.swap ? 16 : 0;
    bits |= in.dodge ? 32 : 0;
    bits |= in.jump ? 64 : 0;
    bits |= in.swapPrev ? 128 : 0;
    w.u8(bits);
    w.u8(in.target == 0 ? 0 : (in.target > 0 ? 1 : 2));
}
PlayerInput UnpackInput(R& r) {
    PlayerInput in;
    in.move.x = r.f32();
    in.move.y = r.f32();
    int bits = r.u8();
    for (int i = 0; i < 4; i++) in.attack[i] = (bits >> i) & 1;
    in.swap = (bits & 16) != 0;
    in.dodge = (bits & 32) != 0;
    in.jump = (bits & 64) != 0;
    in.swapPrev = (bits & 128) != 0;
    int tg = r.u8();
    in.target = tg == 1 ? 1 : (tg == 2 ? -1 : 0);
    return in;
}
bool SameInput(const PlayerInput& a, const PlayerInput& b) {
    if (a.move.x != b.move.x || a.move.y != b.move.y || a.swap != b.swap || a.dodge != b.dodge || a.jump != b.jump || a.swapPrev != b.swapPrev || a.target != b.target) return false;
    for (int i = 0; i < 4; i++) if (a.attack[i] != b.attack[i]) return false;
    return true;
}
PlayerInput Predict(const PlayerInput& last) {  // they keep moving the same way; presses don't repeat
    PlayerInput p;
    p.move = last.move;
    return p;
}

// Both games must have the same Pokemon, moves and items, or battles would play out differently.
uint64_t DataHash() {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* d, size_t n) {
        const unsigned char* c = (const unsigned char*)d;
        for (size_t i = 0; i < n; i++) { h ^= c[i]; h *= 1099511628211ull; }
    };
    auto ms = [&](const std::string& s) { mix(s.data(), s.size()); };
    auto mi = [&](int v) { mix(&v, sizeof v); };
    auto mf = [&](float v) { mix(&v, sizeof v); };
    for (const auto& sp : AllSpecies()) {
        ms(sp.name); mi((int)sp.type1); mi((int)sp.type2); mi(sp.hp); mi(sp.atk); mi(sp.def); mi(sp.spd); mi(sp.spAtk); mi(sp.spDef);
        mf(sp.size); mf(sp.drawScale); mi(sp.floats); mi(sp.heavy); mi(sp.winged); ms(sp.evolvesTo); mi(sp.evolveLevel);
        for (const auto& e : sp.learnset) { mi(e.level); ms(e.move.name); mi(e.move.power); }
        for (const auto& e : sp.evoAlts) { ms(e.to); ms(e.item); mi(e.friendship); mi(e.trade); }
        ms(sp.ability1); ms(sp.ability2); ms(sp.hiddenAbility);
    }
    for (const auto& m : MoveLibrary()) { ms(m.name); mi(m.power); mi((int)m.kind); mf(m.cooldown); mf(m.range); }
    for (const auto& it : ItemList()) { ms(it.name); mf(it.value); }
    return h;
}

int Other(const Game& g) { return 1 - g.on.me; }

// Which copy of the shared save is newer. Battles fought only ever go up (the story stage can go
// back when a League run is lost), so they decide first.
bool MoreProgress(const CoopSave& a, const CoopSave& b) {
    int fa = a.wins + a.losses + a.trainingWins, fb = b.wins + b.losses + b.trainingWins;
    if (fa != fb) return fa > fb;
    if (a.storyStage != b.storyStage) return a.storyStage > b.storyStage;
    return a.badges.size() > b.badges.size();
}

void SendCoop(const CoopSave& c) {
    W w(M_COOP);
    w.i32(c.storyStage).i32(c.wins).i32(c.losses).i32(c.trainingWins).i32((int)c.badges.size());
    for (const auto& b : c.badges) w.str(b);
    w.i32(c.clock);
    for (int p = 0; p < 2; p++) w.i32(c.run[p].kos).i32(c.run[p].damage).i32(c.run[p].caught).i32(c.run[p].mvp).i32(c.run[p].faints).i32(c.run[p].battles);
    w.str(c.trainArea);
    w.i32(c.towerFloor).i32(c.towerBest).i32(c.towerRuns);
    w.i32(c.legends);
    for (int i = 0; i < LEGEND_COUNT; i++) w.i32(c.legendWho[i]);
    w.str(c.rival[0]).str(c.rival[1]);
    w.Send();
}

std::string Compress(const std::string& in) {
    int n = 0;
    unsigned char* c = CompressData((const unsigned char*)in.data(), (int)in.size(), &n);
    std::string out((const char*)c, (size_t)n);
    MemFree(c);
    return out;
}
std::string Decompress(const std::string& in) {
    int n = 0;
    unsigned char* d = DecompressData((const unsigned char*)in.data(), (int)in.size(), &n);
    if (!d) return "";
    std::string out((const char*)d, (size_t)n);
    MemFree(d);
    return out;
}

const char* ActivityName(Screen s) {
    switch (s) {
        case Screen::CoopHub: return "in the hub";
        case Screen::Shop: return "shopping";
        case Screen::Heal: return "at the Healing Center";
        case Screen::MyCreatures: return "looking at their Party & Box";
        case Screen::Map: return "looking at the map";
        case Screen::Select: return "choosing their team";
        case Screen::Battle: return "in battle";
        case Screen::StoryResults: case Screen::BattleStats: case Screen::Evolve: case Screen::LearnMove: case Screen::Reward:
        case Screen::Fossil: case Screen::Gift: case Screen::Revive: return "after the battle";
        case Screen::Slots: return "at the Game Corner";
        case Screen::Practice: return "in the practice arena";
        case Screen::StageIntro: return "getting ready for the battle";
        case Screen::Settings: case Screen::Controls: return "in the settings";
        case Screen::CoopStarter: return "choosing a starter";
        case Screen::Catch: return "catching";
        case Screen::Trade: return "at the trade machine";
        case Screen::Pokedex: return "reading their Pokedex";
        case Screen::Legends: return "reading about Legendary Pokemon";
        case Screen::Bag: return "looking in their Bag";
        case Screen::LegendCatch: return "catching a legendary";
        default: return "busy";
    }
}

// ---- battle (rollback)

void BeginOnlineBattle(Game& g) {
    OnlineSession& o = g.on;
    o.inBattle = true;
    o.partnerThrows.clear();
    o.partnerCatchSel = 0;
    gLocalInput = o.me;
    o.tick = o.confirmed = 0;
    o.confirmedState = g.battle;
    for (auto& m : o.inputs) m.clear();
    o.predicted.clear();
    for (int t = 0; t < INPUT_DELAY; t++) { o.inputs[0][t] = PlayerInput(); o.inputs[1][t] = PlayerInput(); }
    o.lastRemote = o.pending = PlayerInput();
    o.acc = 0;
    o.resim = false;
    o.firstWrong = -1;
    o.myHash.clear();
    o.theirHash.clear();
    o.leaveAsk = false;
    o.stallT = 0;
    g.paused = false;
    g.screen = Screen::Battle;
    // The guest's battle arrives as saved state: build the arena look (gym stands, crowd) here too
    if (IsWindowReady()) SetBattleArena(g.battle.Setup().arena, g.battle.GetField());
}

PlayerInput InputFor(OnlineSession& o, int who, int t, bool remember) {
    auto it = o.inputs[who].find(t);
    if (it != o.inputs[who].end()) return it->second;
    PlayerInput p = Predict(o.lastRemote);
    if (remember) o.predicted[t] = p;
    return p;
}

void StepConfirmed(Game& g) {
    OnlineSession& o = g.on;
    bool was = gSfxMuted;
    gSfxMuted = true;
    while (o.inputs[0].count(o.confirmed) && o.inputs[1].count(o.confirmed) && !o.confirmedState.Finished()) {
        PlayerInput in[2] = {o.inputs[0][o.confirmed], o.inputs[1][o.confirmed]};
        o.confirmedState.Update(TICK, in);
        o.confirmed++;
        if (o.confirmed % 60 == 0) {
            uint64_t h = BattleHash(o.confirmedState);
            o.myHash[o.confirmed] = h;
            W(M_HASH).i32(o.confirmed).u64(h).Send();
        }
        o.predicted.erase(o.confirmed - 1);
    }
    gSfxMuted = was;
}

void Resimulate(Game& g) {
    OnlineSession& o = g.on;
    bool was = gSfxMuted;
    gSfxMuted = true;
    g.battle = o.confirmedState;
    for (int t = o.confirmed; t < o.tick && !g.battle.Finished(); t++) {
        PlayerInput in[2];
        in[o.me] = InputFor(o, o.me, t, false);
        in[Other(g)] = InputFor(o, Other(g), t, true);
        g.battle.Update(TICK, in);
    }
    gSfxMuted = was;
    o.rollbacks++;
}

} // namespace

bool OnlineMine(const Game& g, int who) { return !g.on.active || who == g.on.me; }

void OnlineSendProfile(Game& g) {
    if (!g.on.active) return;
    W(M_PROFILE).str(ProfileToText(Prof(g, g.on.me))).Send();
}

void OnlineCancelSelect(Game& g) { if (g.on.active) W(M_CANCEL).Send(); }

void OnlineSendCoop(Game& g) {
    if (!g.on.active) return;
    SaveCoop(g.coopSave);
    SendCoop(g.coopSave);
}

void OnlineSendTrade(Game& g) {
    if (!g.on.active) return;
    int me = g.on.me;
    bool here = g.screen == Screen::Trade;
    int offer = here ? g.tradeOffer[me] : -1;
    const Profile& P = Prof(g, me);
    std::string text = offer >= 0 && offer < (int)P.mons.size() ? MonToText(P.mons[offer]) : std::string();
    W(M_TRADE).u8(here).i32(offer).str(text).u8(here && g.tradeReady[me]).Send();
}

void OnlineTradeUpdate(Game& g) {
    OnlineSession& o = g.on;
    if (!o.active || o.me != 0 || g.screen != Screen::Trade || g.tradePhase != 0) return;
    int mine = g.tradeOffer[0];
    if (!g.tradeReady[0] || !o.partnerTradeReady || !o.partnerInTrade || mine < 0 || o.partnerOffer < 0) return;
    const Profile& P = Prof(g, 0);
    OwnedMon theirs;
    if (mine >= (int)P.mons.size() || !MonFromText(o.partnerOfferMon, theirs)) return;
    int idx[2] = {mine, o.partnerOffer};
    OwnedMon mons[2] = {P.mons[mine], theirs};
    W(M_TRADE_GO).i32(idx[0]).str(MonToText(mons[0])).i32(idx[1]).str(MonToText(mons[1])).Send();
    o.partnerTradeReady = false;
    ApplyTrade(g, idx, mons);
}

void OnlineSendCatch(int poolPick) { W(M_CATCH).i32(poolPick).Send(); }
void OnlineSendThrow(int species, int level, bool ok, int shakes, bool fled) {
    W(M_THROW).i32(species).i32(level).u8(ok).u8((uint8_t)shakes).u8(fled).Send();
}
void OnlineSendFossil(const std::string& choice) { W(M_FOSSIL).str(choice).Send(); }
void OnlineSendCursor(int sel) { W(M_CURSOR).u8((uint8_t)sel).Send(); }

void OnlineSetReady(Game& g, int kind) {
    if (!g.on.active || g.on.myReady == kind) return;
    g.on.myReady = kind;
    W(M_READY).u8(kind).Send();
}

void OnlineLeave(Game& g, const std::string& why) {
    if (g.on.active || net::GetState() != net::State::Off) {
        if (net::GetState() == net::State::Connected) { W(M_BYE).Send(); }
        net::Poll();
        net::Close();
    }
    if (g.coop && g.on.active) {
        if (g.coopActive >= 0) { g.coopP[g.coopActive] = g.profile; g.coopActive = -1; }
        SaveProfile(g.coopP[g.on.me]);
        SaveCoop(g.coopSave);
    }
    g.on = OnlineSession();
    gSfxMuted = false;
    gLocalInput = -1;
    GoTitle(g);
    if (!why.empty()) Toast(g, why);
}

// Host: both picked the same battle in the hub -> everyone to team select.
static void HostMaybeGo(Game& g) {
    OnlineSession& o = g.on;
    if (o.me != 0 || o.myReady == 0 || o.myReady != o.partnerReady || g.screen != Screen::CoopHub) return;
    int kind = o.myReady;
    int stages = (int)Story().stages.size();
    bool legend = kind >= KIND_LEGEND && kind < KIND_LEGEND + LEGEND_COUNT;   // 200+id: a legendary encounter
    if (legend && !LegendOpen(g, kind - KIND_LEGEND)) { OnlineSetReady(g, 0); return; }
    int stageIdx = kind == 1 ? std::min(g.coopSave.storyStage, stages - 1) : (kind >= 3 && !legend && kind != KIND_TOWER) ? std::min(kind - 3, stages - 1) : -1;   // 3+: a gym rematch
    bool rare = false;
    int rareSpecies = -1;
    if (kind == KIND_TOWER) {   // the Battle Tower: the floor and its trainer's seed
        stageIdx = g.coopSave.towerFloor + 1;
        rareSpecies = (int)TowerSeedFor(g);
    }
    if (kind == 2) {
        int every = std::max(1, Story().rareEvery);
        rare = (g.coopSave.trainingWins + 1) % every == 0;
        SetTrainingWild(g);
        if (rare) {
            std::vector<std::string> owned;
            int sum = 0, n = 0;
            for (int p = 0; p < 2; p++)
                for (const auto& m : Prof(g, p).mons) {
                    owned.push_back(m.species);
                    if (!m.boxed) { sum += m.level; n++; }
                }
            rareSpecies = PickRareSpecies(owned, n ? sum / n : 5);
        }
    }
    W(M_GO).u8(kind).i32(stageIdx).u8(rare).i32(rareSpecies).Send();
    g.stageIdx = stageIdx;
    g.mode = kind != 2 ? Mode::CoopStory : Mode::CoopTraining;
    g.rareBattle = rare;
    g.rareSpecies = rareSpecies;
    g.tower = false;
    g.legend = -1;
    if (kind == KIND_TOWER) BeginTowerFloor(g, stageIdx, (uint32_t)rareSpecies);
    if (legend) BeginLegend(g, kind - KIND_LEGEND);
    o.myReady = o.partnerReady = 0;
    o.picksSent = o.partnerPicksIn = false;
    o.partnerPickView.clear();
    o.partnerSelCursor = -1;
    if (kind != 2) g.screen = Screen::StageIntro;   // the trainer / gym intro first, like offline
    else BeginTeamSelect(g);
}

static void HostMaybeStartBattle(Game& g) {
    OnlineSession& o = g.on;
    if (o.me != 0 || !o.picksSent || !o.partnerPicksIn || g.screen != Screen::Select) return;
    g.roster[1] = ProfileRoster(g.coopP[1], true);
    g.picks[1].clear();
    for (int i : o.partnerPicks)
        if (i >= 0 && i < (int)g.roster[1].size()) g.picks[1].push_back(i);
    StartBattleFromSelect(g);   // builds the 2v2 battle (both players' slots are human)
    W(M_BATTLE).i32(g.stageIdx).u8((int)g.mode).u8(g.rareBattle).i32(g.rareSpecies).str(Compress(SaveBattle(g.battle))).i32(g.legend).Send();
    BeginOnlineBattle(g);
}

// The connection dropped (or the partner came back on a new one): keep the session, save, and
// drop whatever needs both players (a battle, team select, the trade machine) back to the hub.
static void BeginReconnect(Game& g, bool alreadyBack) {
    OnlineSession& o = g.on;
    o.reconnecting = true;
    o.reconnectT = 0;
    o.retryAt = 1.5f;
    o.helloSent = o.gotHello = false;
    o.myReady = o.partnerReady = 0;
    o.picksSent = o.partnerPicksIn = false;
    o.partnerPickView.clear();
    o.partnerSelCursor = -1;
    o.partnerInTrade = o.partnerTradeReady = false;
    o.partnerOffer = -1;
    o.partnerCursor = -1;
    o.partnerActivity.clear();
    SaveProfile(g.coopActive == o.me ? g.profile : g.coopP[o.me]);
    SaveCoop(g.coopSave);
    const std::string& pn = g.coopP[Other(g)].name;
    if (o.inBattle) {
        o.inBattle = false;
        gSfxMuted = false;
        gShake = {0, 0};
        EnterStoryHub(g);
        Toast(g, "Lost the connection to " + pn + " - the battle was called off (nothing lost).");
    } else if (g.screen == Screen::Select || g.screen == Screen::StageIntro || g.screen == Screen::Trade) {
        EnterStoryHub(g);
        Toast(g, "Lost the connection to " + pn + " - reconnecting...");
    }
    if (!alreadyBack) net::Rearm();
}

static void ReconnectStep(Game& g) {
    OnlineSession& o = g.on;
    o.reconnectT += GetFrameTime();
    if (o.me != 1) {   // host: wait on the same port (Rearm), punching toward the friend's code if we have it
        if (net::GetState() == net::State::Failed) net::Rearm();
        return;
    }
    net::State st = net::GetState();   // guest: knock again every few seconds
    if ((st == net::State::Off || st == net::State::Failed) && o.reconnectT >= o.retryAt && !o.field.empty()) {
        o.retryAt = o.reconnectT + 3.0f;
        std::string err;
        if (!net::HasSocket()) net::Open(false, net::DEFAULT_PORT, err);
        else net::Rearm();
        net::Connect(o.field, err);
    }
}

bool OnlineWaiting(Game& g) {
    if (!g.on.active || !g.on.reconnecting) return false;
    Toast(g, "Waiting for " + g.coopP[Other(g)].name + " to reconnect...");
    return true;
}

static void TypeCode(std::string& s, size_t maxLen);

// The friend restarted their game (new code): type it here instead of leaving and joining again.
bool OnlineCodeEntry(Game& g) {
    OnlineSession& o = g.on;
    if (!o.active || !o.reconnecting) { o.codeEdit = false; return false; }
    if (!o.codeEdit) {
        if (IsKeyPressed(KEY_TAB)) {
            o.codeEdit = true;
            o.newCode.clear();
            while (GetCharPressed()) {}
            PlaySfx(Sfx::MenuOk);
            return true;
        }
        return false;
    }
    bool back = IsKeyPressed(KEY_ESCAPE);
    for (int p = 0; p < 4; p++) if (IsGamepadAvailable(p) && IsGamepadButtonPressed(p, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) back = true;
    if (back) { o.codeEdit = false; PlaySfx(Sfx::MenuBack); return true; }
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_C) && !net::MyCode().empty()) {
        SetClipboardText(net::MyCode().c_str());
        Toast(g, "Your code was copied - send it to your friend.");
    }
    TypeCode(o.newCode, 60);
    bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    for (int p = 0; p < 4; p++) if (IsGamepadAvailable(p) && IsGamepadButtonPressed(p, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) enter = true;
    if (enter && !o.newCode.empty()) {
        std::string c = o.newCode;
        while (!c.empty() && c.back() == ' ') c.pop_back();
        while (!c.empty() && c.front() == ' ') c.erase(c.begin());
        o.field = c;
        o.codeEdit = false;
        if (o.me == 1) {   // guest: knock on the new code right away
            std::string err;
            if (!net::HasSocket()) net::Open(false, net::DEFAULT_PORT, err);
            else net::Rearm();
            if (!net::Connect(o.field, err)) Toast(g, err);
            o.retryAt = o.reconnectT + 3.0f;
        } else if (net::CodeLooksValid(o.field)) {
            net::PunchTo(o.field);   // host: open the path toward the friend's new code
        }
        Toast(g, "Trying " + c + "...");
        PlaySfx(Sfx::MenuOk);
    }
    return true;
}

void DrawOnlineOverlay(const Game& g) {
    const OnlineSession& o = g.on;
    if (!o.active || !o.reconnecting) return;
    const std::string& pn = g.coopP[1 - o.me].name;
    DrawRectangle(0, 0, VW, 27, Fade(UI_INK, 0.92f));
    DrawRectangle(0, 27, VW, 1, Color{255, 170, 90, 255});
    const char* dots = ((int)(g.t * 3) % 3) == 0 ? ".  " : ((int)(g.t * 3) % 3) == 1 ? ".. " : "...";
    std::string a = "Lost the connection to " + pn + " - reconnecting" + dots + TextFormat(" (%ds)", (int)o.reconnectT) +
                     "    Your progress is saved.";
    DrawTextCentered(a.c_str(), VW / 2, 3, 10, Color{255, 190, 120, 255}, false);
    std::string b = "Did " + pn + " restart the game?  Press Tab to type their new code.";
    DrawTextCentered(b.c_str(), VW / 2, 15, 10, UI_DIM, false);
    if (!o.codeEdit) return;
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.55f));
    int w = 440, h = 150, x = (VW - w) / 2, y = (VH - h) / 2;
    DrawPanel(x, y, w, h, UI_PANEL, UI_GOLD);
    DrawTextCentered(("RECONNECT TO " + pn).c_str(), VW / 2, y + 10, 20, UI_GOLD);
    GDrawText((pn + "'s new connection code:").c_str(), x + 16, y + 40, 10, UI_DIM);
    DrawPanel(x + 16, y + 54, w - 32, 24, UI_PANEL_LIGHT, UI_GOLD);
    std::string shown = o.newCode + ((((int)(g.t * 2)) % 2) ? "_" : " ");
    GDrawText(shown.c_str(), x + 26, y + 61, 10, UI_TEXT);
    const std::string& code = net::MyCode();
    GDrawText("Your code:", x + 16, y + 88, 10, UI_DIM);
    GDrawText(code.empty() ? "finding it..." : code.c_str(), x + 80, y + 88, 10, code.empty() ? UI_DIM : UI_GOLD);
    GDrawText("(Ctrl+C copies - send it to them too)", x + 16, y + 101, 10, UI_DIM);
    DrawTextCentered("Type or Ctrl+V to paste    Enter / (A) connect    Esc / (B) cancel", VW / 2, y + h - 18, 10, UI_TEXT, false);
}

void OnlinePoll(Game& g) {
    OnlineSession& o = g.on;
    if (net::GetState() == net::State::Off && !o.active) return;
    net::Poll();
    if (o.active) {
        bool replaced = net::TakeReplaced();
        if (replaced || (!o.reconnecting && net::GetState() == net::State::Failed)) BeginReconnect(g, replaced);
        if (o.reconnecting) ReconnectStep(g);
    }
    // Hello once connected.
    if (net::GetState() == net::State::Connected && !o.helloSent) {
        o.me = net::IsHost() ? 0 : 1;
        if (o.active) o.mine = g.coopActive == o.me ? g.profile : g.coopP[o.me];   // a reconnect: who we are now
        W(M_HELLO).i32((int32_t)PROTOCOL).u64(DataHash()).str(ProfileToText(o.mine)).Send();
        o.helloSent = true;
    }
    std::string m;
    while (net::Receive(m)) {
        if (m.empty()) continue;
        R r(m);
        switch ((Msg)(uint8_t)m[0]) {
            case M_HELLO: {
                uint32_t proto = (uint32_t)r.i32();
                uint64_t dh = r.u64();
                std::string prof = r.str();
                if (proto != PROTOCOL) { OnlineLeave(g, "Your friend has a different version of the game - both need the same ArenaClash.exe."); return; }
                if (dh != DataHash()) { OnlineLeave(g, "Your Pokemon folders are different - both need the same version and Pokemon files."); return; }
                Profile partner;
                ProfileFromText(prof, "", partner);
                if (o.active && o.reconnecting) {   // our partner is back (or someone else knocked)
                    if (partner.name != g.coopP[Other(g)].name) { net::Rearm(); o.helloSent = false; break; }
                    g.coopP[Other(g)] = partner;
                    o.gotHello = true;
                    SendCoop(g.coopSave);
                    break;
                }
                o.gotHello = true;
                // Set up the co-op session: me in my slot, them in theirs.
                g.coop = true;
                g.coopActive = -1;
                g.coopP[o.me] = o.mine;
                g.coopP[Other(g)] = partner;
                std::string a = o.me == 0 ? o.mine.name : partner.name;
                std::string b = o.me == 0 ? partner.name : o.mine.name;
                bool existed = LoadCoop(a, b, g.coopSave);
                g.coopSave.p[0] = a;
                g.coopSave.p[1] = b;
                if (!existed) SaveCoop(g.coopSave);
                SendCoop(g.coopSave);
            } break;
            case M_COOP: {
                CoopSave c = g.coopSave;
                c.storyStage = r.i32();
                c.wins = r.i32();
                c.losses = r.i32();
                c.trainingWins = r.i32();
                int nb = r.i32();
                c.badges.clear();
                for (int i = 0; i < nb && r.ok; i++) c.badges.push_back(r.str());
                int clk = r.i32();
                if (r.ok && clk > c.clock) c.clock = clk;
                bool statsChanged = false;
                for (int p = 0; p < 2 && r.ok; p++) {   // leaderboard counts only grow: keep the larger of each
                    int* mine[6] = {&g.coopSave.run[p].kos, &g.coopSave.run[p].damage, &g.coopSave.run[p].caught, &g.coopSave.run[p].mvp, &g.coopSave.run[p].faints, &g.coopSave.run[p].battles};
                    for (int q = 0; q < 6; q++) {
                        int v = r.i32();
                        if (r.ok && v > *mine[q]) { *mine[q] = v; statsChanged = true; }
                    }
                }
                for (int p = 0; p < 2; p++) c.run[p] = g.coopSave.run[p];
                std::string ta = r.str();   // the host picks the training area
                if (r.ok && g.on.me == 1 && !ta.empty() && ta != g.coopSave.trainArea) { g.coopSave.trainArea = ta; statsChanged = true; }
                c.trainArea = g.coopSave.trainArea;
                c.towerFloor = r.i32();
                c.towerBest = r.i32();
                c.towerRuns = r.i32();
                if (r.ok && c.towerBest > g.coopSave.towerBest) { g.coopSave.towerBest = c.towerBest; statsChanged = true; }
                c.towerBest = std::max(c.towerBest, g.coopSave.towerBest);
                {   // caught legendaries stay caught (whoever caught one keeps it)
                    int lm = r.i32();
                    int who[LEGEND_COUNT];
                    for (int i = 0; i < LEGEND_COUNT; i++) who[i] = r.i32();
                    if (r.ok)
                        for (int i = 0; i < LEGEND_COUNT; i++)
                            if ((lm >> i & 1) && !(g.coopSave.legends >> i & 1)) {
                                g.coopSave.legends |= 1 << i;
                                g.coopSave.legendWho[i] = who[i];
                                statsChanged = true;
                            }
                    c.legends = g.coopSave.legends;
                    for (int i = 0; i < LEGEND_COUNT; i++) c.legendWho[i] = g.coopSave.legendWho[i];
                }
                {   // the rival's starters: picked once (by the host), then they never change
                    std::string r0 = r.str(), r1 = r.str();
                    if (r.ok && g.coopSave.rival[0].empty() && !r0.empty()) { g.coopSave.rival[0] = r0; g.coopSave.rival[1] = r1; statsChanged = true; }
                    c.rival[0] = g.coopSave.rival[0];
                    c.rival[1] = g.coopSave.rival[1];
                }
                if (statsChanged) SaveCoop(g.coopSave);
                if (MoreProgress(c, g.coopSave)) { g.coopSave = c; SaveCoop(g.coopSave); }
                else if (c.clock > g.coopSave.clock) {   // partner waited: follow their clock
                    g.coopSave.clock = c.clock;
                    SaveCoop(g.coopSave);
                }
                if (o.active && o.reconnecting && o.gotHello) {
                    o.reconnecting = false;
                    o.resend = true;
                    Toast(g, "Reconnected to " + g.coopP[Other(g)].name + "!");
                    OnlineSendProfile(g);
                }
                if (!o.active && o.gotHello) {
                    o.active = true;
                    o.myReady = o.partnerReady = 0;
                    g.rareSpecies = -1;
                    Toast(g, "Connected to " + g.coopP[Other(g)].name + "!");
                    if (CoopNeedsStarter(g)) { g.coopStarterSel[0] = g.coopStarterSel[1] = 0; g.screen = Screen::CoopStarter; }
                    else EnterStoryHub(g);
                }
            } break;
            case M_PROFILE: {
                Profile p;
                if (ProfileFromText(r.str(), "", p)) g.coopP[Other(g)] = p;
            } break;
            case M_READY: o.partnerReady = r.u8(); break;
            case M_ACTIVITY: o.partnerActivity = r.str(); break;
            case M_CURSOR: o.partnerCursor = r.u8(); break;
            case M_FOSSIL: o.partnerFossil = r.str(); break;
            case M_PICKVIEW: {   // the partner's team so far, while they pick
                int n = r.u8();
                std::vector<int> v;
                for (int i = 0; i < n && r.ok; i++) v.push_back(r.u8());
                int cur = r.u8();
                if (r.ok) { o.partnerPickView = v; o.partnerSelCursor = cur == 255 ? -1 : cur; }
            } break;
            case M_GO: {
                int kind = r.u8();
                g.stageIdx = r.i32();
                g.rareBattle = r.u8() != 0;
                g.rareSpecies = r.i32();
                g.mode = kind != 2 ? Mode::CoopStory : Mode::CoopTraining;
                o.myReady = o.partnerReady = 0;
                o.picksSent = o.partnerPicksIn = false;
                o.partnerPickView.clear();
                o.partnerSelCursor = -1;
                if (g.coopActive >= 0) EnterStoryHub(g);  // close the shop etc. first
                g.tower = false;
                g.legend = -1;
                if (kind == KIND_TOWER) BeginTowerFloor(g, g.stageIdx, (uint32_t)g.rareSpecies);
                else if (kind >= KIND_LEGEND && kind < KIND_LEGEND + LEGEND_COUNT) BeginLegend(g, kind - KIND_LEGEND);
                else if (kind != 2) g.screen = Screen::StageIntro;   // the trainer / gym intro first (3+: a rematch)
                else BeginTeamSelect(g);
            } break;
            case M_PICKS: {
                int n = r.i32();
                o.partnerPicks.clear();
                for (int i = 0; i < n && r.ok; i++) o.partnerPicks.push_back(r.i32());
                Profile p;
                if (ProfileFromText(r.str(), "", p)) g.coopP[Other(g)] = p;
                o.partnerPicksIn = true;
            } break;
            case M_CANCEL:
                if (g.screen == Screen::Select || g.screen == Screen::StageIntro) { EnterStoryHub(g); Toast(g, g.coopP[Other(g)].name + " went back to the hub."); }
                o.partnerPicksIn = false;
                break;
            case M_BATTLE: {
                g.stageIdx = r.i32();
                g.mode = (Mode)r.u8();
                g.rareBattle = r.u8() != 0;
                g.rareSpecies = r.i32();
                std::string st = Decompress(r.str());
                int lg = r.i32();
                if (r.ok) g.legend = lg;
                if (!LoadBattle(st, g.battle)) { OnlineLeave(g, "Couldn't start the online battle (the games don't match)."); return; }
                BeginOnlineBattle(g);
            } break;
            case M_INPUT: {
                int t = r.i32();
                PlayerInput in = UnpackInput(r);
                int who = Other(g);
                if (!o.inputs[who].count(t)) {
                    o.inputs[who][t] = in;
                    auto it = o.predicted.find(t);
                    if (it != o.predicted.end() && t < o.tick && !SameInput(it->second, in)) {
                        if (o.firstWrong < 0 || t < o.firstWrong) o.firstWrong = t;
                        o.resim = true;
                    }
                }
                // the newest one we have is the best guess for what they're doing now
                int newest = o.inputs[who].empty() ? -1 : o.inputs[who].rbegin()->first;
                if (t == newest) o.lastRemote = in;
            } break;
            case M_HASH: {
                int t = r.i32();
                uint64_t h = r.u64();
                o.theirHash[t] = h;
            } break;
            case M_RESYNC_REQ:
                if (o.me == 0 && o.inBattle)
                    W(M_RESYNC).i32(o.confirmed).str(Compress(SaveBattle(o.confirmedState))).Send();
                break;
            case M_RESYNC: {
                int t = r.i32();
                Battle b;
                if (o.inBattle && LoadBattle(Decompress(r.str()), b)) {
                    o.confirmedState = b;
                    o.confirmed = t;
                    o.tick = std::max(o.tick, t);
                    o.resim = true;
                    o.myHash.clear();
                    o.theirHash.clear();
                }
            } break;
            case M_CATCH: o.partnerCatch.push_back(r.i32()); break;
            case M_CATCHSEL: o.partnerCatchSel = r.u8(); break;
            case M_THROW: {
                OnlineSession::Throw t;
                t.species = r.i32();
                t.level = r.i32();
                t.ok = r.u8() != 0;
                t.shakes = r.u8();
                t.fled = r.u8() != 0;
                if (r.ok) o.partnerThrows.push_back(t);
            } break;
            case M_TRADE: {
                bool here = r.u8() != 0;
                int offer = r.i32();
                std::string text = r.str();
                bool ready = r.u8() != 0;
                if (offer != o.partnerOffer && g.tradeReady[o.me]) {   // the deal changed: look again
                    g.tradeReady[o.me] = false;
                    OnlineSendTrade(g);
                }
                o.partnerInTrade = here;
                o.partnerOffer = offer;
                o.partnerOfferMon = text;
                o.partnerTradeReady = ready;
            } break;
            case M_TRADE_GO: {
                int idx[2];
                OwnedMon mons[2];
                idx[0] = r.i32();
                bool ok = MonFromText(r.str(), mons[0]);
                idx[1] = r.i32();
                ok = MonFromText(r.str(), mons[1]) && ok && r.ok;
                if (ok) {
                    if (g.coopActive >= 0) { g.coopP[g.coopActive] = g.profile; g.coopActive = -1; }
                    g.screen = Screen::Trade;
                    o.partnerTradeReady = false;
                    ApplyTrade(g, idx, mons);
                }
            } break;
            case M_LEAVE_BATTLE:
                if (o.inBattle) {
                    o.inBattle = false;
                    EnterStoryHub(g);
                    Toast(g, g.coopP[Other(g)].name + " left the battle.");
                }
                break;
            case M_BYE:
                OnlineLeave(g, (g.coop ? g.coopP[Other(g)].name : std::string("Your friend")) + " left the game. Your progress is saved.");
                return;
        }
    }
    if (!o.active) return;
    // Tell the partner what we're up to when it changes.
    static Screen lastScreen = Screen::Title;
    static int lastCursor = -1;
    if (o.reconnecting) return;   // nothing to send until they're back
    if (o.resend) { o.resend = false; lastScreen = Screen::Title; lastCursor = -1; }
    if (g.screen != lastScreen) {
        lastScreen = g.screen;
        lastCursor = -1;
        W(M_ACTIVITY).str(ActivityName(g.screen)).Send();
        if (g.screen != Screen::CoopHub && o.myReady != 0 && g.screen != Screen::Select) OnlineSetReady(g, 0);
    }
    // Our highlighted creature in the catch draft, so the partner can watch us choose.
    if (g.screen == Screen::Catch && g.catchWho == o.me && g.catchPhase == 0) {
        static int lastSel = -1;
        if (g.catchSel != lastSel) { lastSel = g.catchSel; W(M_CATCHSEL).u8((uint8_t)std::min(255, g.catchSel)).Send(); }
    }
    // Our highlighted hub item, so the partner sees where we are.
    if (g.screen == Screen::CoopHub && g.hubSel != lastCursor) {
        lastCursor = g.hubSel;
        W(M_CURSOR).u8((uint8_t)g.hubSel).Send();
    }
    // Team select: our picks so far and where our cursor is, so the partner can plan around them.
    if (g.screen == Screen::Select) {
        static std::string lastView;
        std::string view;
        for (int i : g.picks[o.me]) view += (char)(i + 1);
        view += '|';
        view += (char)(g.cursor[o.me] + 1);
        if (view != lastView) {
            lastView = view;
            W w(M_PICKVIEW);
            w.u8((uint8_t)g.picks[o.me].size());
            for (int i : g.picks[o.me]) w.u8((uint8_t)i);
            w.u8((uint8_t)std::min(254, g.cursor[o.me]));
            w.Send();
        }
    }
    // Team select: send our picks once locked in.
    if (g.screen == Screen::Select && g.ready[o.me] && !o.picksSent) {
        W w(M_PICKS);
        w.i32((int)g.picks[o.me].size());
        for (int i : g.picks[o.me]) w.i32(i);
        w.str(ProfileToText(Prof(g, o.me)));
        w.Send();
        o.picksSent = true;
    }
    HostMaybeGo(g);
    HostMaybeStartBattle(g);
}

// Returns true when it ran the battle screen this frame.
bool OnlineBattleUpdate(Game& g, float dt) {
    OnlineSession& o = g.on;
    if (!o.active || !o.inBattle) return false;
    // Gather our input every frame; presses stay held until a tick takes them.
    PlayerInput li = ReadInput(0, false);
    li.swapTo = -1;   // (keys 1-6 are solo only: not part of the online input)
#ifdef ARENA_AUTOTEST
    if (gAutoNetInput) {
        uint32_t h = (uint32_t)(o.tick / 18) * 2654435761u ^ (uint32_t)(o.me + 1) * 40503u;
        li = PlayerInput();
        li.move = {((h & 7) / 3.5f) - 1.0f, (((h >> 3) & 7) / 3.5f) - 1.0f};
        int a = (int)((h >> 6) % 6);
        if (a < 4 && o.tick % 9 == 0) li.attack[a] = true;
        if (a == 4 && o.tick % 9 == 0) li.dodge = true;
        if (a == 5 && o.tick % 9 == 0) li.jump = true;
    }
#endif
    o.pending.move = li.move;
    for (int i = 0; i < 4; i++) o.pending.attack[i] = o.pending.attack[i] || li.attack[i];
    o.pending.swap = o.pending.swap || li.swap;
    o.pending.dodge = o.pending.dodge || li.dodge;
    o.pending.jump = o.pending.jump || li.jump;
    o.pending.swapPrev = o.pending.swapPrev || li.swapPrev;
    if (li.target) o.pending.target = li.target;

    // Leaving (the battle doesn't pause online).
    if (li.pause || IsKeyPressed(KEY_ESCAPE)) { o.leaveAsk = !o.leaveAsk; PlaySfx(Sfx::MenuMove); }
    if (o.leaveAsk) {
        PlayerInput mi = MenuInput();
        if (IsKeyPressed(KEY_ENTER) || mi.confirm) {
            W(M_LEAVE_BATTLE).Send();
            o.inBattle = false;
            gSfxMuted = false;
            EnterStoryHub(g);
            Toast(g, "You left the battle - nothing was saved.");
            return true;
        }
    }

    // Catch up with the partner's real inputs.
    StepConfirmed(g);
    if (o.resim) { Resimulate(g); o.resim = false; o.firstWrong = -1; }

    // Checksums: any mismatch -> the guest asks the host for its copy.
    for (auto it = o.myHash.begin(); it != o.myHash.end();) {
        auto th = o.theirHash.find(it->first);
        if (th == o.theirHash.end()) { ++it; continue; }
        if (th->second != it->second) {
            o.desyncs++;
            if (o.me == 1) W(M_RESYNC_REQ).Send();
        }
        o.theirHash.erase(th);
        it = o.myHash.erase(it);
    }

    // Advance our own (predicted) battle in fixed steps.
    (void)dt;
    o.acc = std::min(o.acc + std::min(GetFrameTime(), 0.25f), 0.25f);  // real time, not the capped frame step
    while (o.acc >= TICK) {
        if (o.tick - o.confirmed >= MAX_AHEAD) { o.stallT += TICK; break; }   // wait for the partner
        o.stallT = 0;
        int t = o.tick + INPUT_DELAY;
        o.inputs[o.me][t] = o.pending;
        W w(M_INPUT);
        w.i32(t);
        PackInput(w, o.pending);
        w.Send();
        PlayerInput keep;
        keep.move = o.pending.move;
        o.pending = keep;
        PlayerInput in[2];
        in[o.me] = InputFor(o, o.me, o.tick, false);
        in[Other(g)] = InputFor(o, Other(g), o.tick, true);
        if (!g.battle.Finished()) g.battle.Update(TICK, in);
        o.tick++;
        o.acc -= TICK;
    }
    if (o.acc >= TICK) o.stallT += dt;

    // The real result: once the confirmed battle is over.
    if (o.confirmedState.Finished()) {
        g.battle = o.confirmedState;
        o.inBattle = false;
        gShake = {0, 0};
        ProcessStoryBattleEnd(g);
    }
    return true;
}

// ------------------------------------------------------------------ connect screen

void StartOnlineFlow(Game& g) {
    OnlineLeave(g, "");
    g.on = OnlineSession();
    g.profiles = ListProfiles();
    g.screen = Screen::NetMenu;
}

namespace {
void TypeInto(std::string& s, size_t maxLen) {
    int c;
    while ((c = GetCharPressed()) != 0)
        if (s.size() < maxLen && c >= 32 && c < 127) s += (char)c;
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !s.empty()) s.pop_back();
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V)) {
        const char* clip = GetClipboardText();
        if (clip) for (const char* p = clip; *p && s.size() < maxLen; p++) if (*p >= 32 && *p < 127) s += *p;
    }
}
} // namespace

static void TypeCode(std::string& s, size_t maxLen) { TypeInto(s, maxLen); }

void UpdateNetMenu(Game& g) {
    OnlineSession& o = g.on;
    int n = (int)g.profiles.size();
    if (o.page == 0) {
        PlayerInput in = MenuInput();
        const int N = 5;
        if (in.up) { o.sel = (o.sel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.down) { o.sel = (o.sel + 1) % N; PlaySfx(Sfx::MenuMove); }
        if (o.sel == 0) {   // the profiles, then "+ New trainer"
            if (in.left) { o.profSel = (o.profSel + n) % (n + 1); PlaySfx(Sfx::MenuMove); }
            if (in.right) { o.profSel = (o.profSel + 1) % (n + 1); PlaySfx(Sfx::MenuMove); }
        }
        if (in.back) { GoTitle(g); return; }
        if (!in.confirm) return;
        if (o.sel == 4) { GoTitle(g); return; }
        if (o.sel == 3) { g.settingsSel = 0; g.settingsBack = Screen::NetMenu; g.screen = Screen::Settings; PlaySfx(Sfx::MenuOk); return; }
        if (o.sel == 0 || o.profSel >= n) {   // make a new trainer: type a name (the starter is picked together once connected)
            if (o.profSel < n) return;
            g.netNewTrainer = true;
            g.nameBuf.clear();
            while (GetCharPressed()) {}
            g.screen = Screen::NameEntry;
            PlaySfx(Sfx::MenuOk);
            return;
        }
        if (!LoadProfile(g.profiles[o.profSel], o.mine)) { Toast(g, "Could not load that profile."); return; }
        std::string err;
        bool host = o.sel == 1;
        if (!net::Open(host, net::DEFAULT_PORT, err)) { o.err = err; Toast(g, err); return; }
        o.page = host ? 1 : 2;
        o.field.clear();
        o.err.clear();
        while (GetCharPressed()) {}
        PlaySfx(Sfx::MenuOk);
        return;
    }
    // Hosting / joining: typing a code or address.
    bool padBack = false;
    for (int p = 0; p < 4; p++) if (IsGamepadAvailable(p) && IsGamepadButtonPressed(p, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) padBack = true;
    if (IsKeyPressed(KEY_ESCAPE) || padBack) { net::Close(); o.page = 0; o.helloSent = o.gotHello = false; PlaySfx(Sfx::MenuBack); return; }
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_C) && !net::MyCode().empty()) {
        SetClipboardText(net::MyCode().c_str());
        Toast(g, "Your code was copied - paste it to your friend.");
    }
    TypeInto(o.field, 60);
    if (o.page == 1 && net::CodeLooksValid(o.field)) net::PunchTo(o.field);
    if (o.page == 2 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && !o.field.empty() &&
        net::GetState() != net::State::Connecting && net::GetState() != net::State::Connected) {
        std::string err;
        if (!net::Connect(o.field, err)) { o.err = err; Toast(g, err); }
        else o.err.clear();
    }
}

void DrawNetMenu(const Game& g) {
    const OnlineSession& o = g.on;
    DrawMenuBackdrop(Color{20, 22, 36, 255}, (float)GetTime());
    DrawTextCentered("ONLINE CO-OP", VW / 2, 10, 30, UI_GOLD);
    DrawTextCentered("Play Co-op Story with a friend on another computer. Each of you uses your own profile.", VW / 2, 46, 10, UI_TEXT, false);
    if (o.page == 0) {
        int n = (int)g.profiles.size();
        std::string prof = o.profSel < n ? "Your trainer:  < " + g.profiles[o.profSel] + " >" : "Your trainer:  < + New trainer >";
        const char* items[5] = {prof.c_str(), "Host a game", "Join a game", "Settings", "Back"};
        for (int i = 0; i < 5; i++) DrawMenuItem(items[i], VW / 2, 74 + i * 26, 20, o.sel == i);
        DrawPanel(60, 210, 520, 110, UI_PANEL, UI_BORDER);
        int y = 218;
        auto line = [&](const char* t, Color c) { GDrawText(t, 72, y, 10, c); y += 13; };
        line("How to connect", UI_GOLD);
        line("- Internet: the host and the friend each get a connection code. Swap codes (Discord, text),", UI_TEXT);
        line("  the friend types the host's code and the host types the friend's. No router setup needed.", UI_TEXT);
        line("- If that doesn't connect (some phone hotspots), both install Radmin VPN or ZeroTier", UI_TEXT);
        line("  (free) and join the same network, then use the host's address shown in that app.", UI_TEXT);
        line("- Same house: just type the host's address shown on its screen.", UI_TEXT);
        line("Both players need the same ArenaClash.exe and Pokemon files.", UI_DIM);
        DrawTextCentered("Up/Down choose   Left/Right profile   Enter select   Esc back", VW / 2, 340, 10, UI_DIM, false);
        DrawToast(g);
        return;
    }
    bool host = o.page == 1;
    DrawTextCentered(host ? "HOSTING" : "JOINING", VW / 2, 66, 20, UI_TEXT);
    int y = 96;
    const std::string& code = net::MyCode();
    GDrawText("Your connection code:", 60, y, 10, UI_DIM);
    DrawTextShadow(code.empty() ? "finding it..." : code.c_str(), 200, y - 4, 20, code.empty() ? UI_DIM : UI_GOLD);
    y += 22;
    if (!code.empty()) GDrawText("Ctrl+C copies it. Send it to your friend.", 200, y, 10, UI_DIM);
    y += 20;
    if (host) {
        auto addrs = net::LocalAddresses();
        std::string a;
        for (const auto& s : addrs) a += (a.empty() ? "" : "   ") + s;
        GDrawText("Same network / Radmin / ZeroTier address:", 60, y, 10, UI_DIM);
        GDrawText(a.empty() ? "-" : a.c_str(), 300, y, 10, UI_TEXT);
        y += 22;
        GDrawText("Your friend's code (lets them through your router):", 60, y, 10, UI_DIM);
    } else {
        GDrawText("Host's code or address:", 60, y, 10, UI_DIM);
    }
    y += 14;
    DrawPanel(60, y, 520, 24, UI_PANEL_LIGHT, UI_GOLD);
    std::string shown = o.field + ((((int)(g.t * 2)) % 2) ? "_" : " ");
    GDrawText(shown.c_str(), 70, y + 7, 10, UI_TEXT);
    y += 34;
    if (!host) GDrawText("Type it (or Ctrl+V to paste) and press Enter. Also send the host YOUR code above.", 60, y, 10, UI_DIM);
    else if (net::CodeLooksValid(o.field)) GDrawText("Opening a path to your friend...", 60, y, 10, Color{140, 230, 150, 255});
    y += 20;
    std::string st = net::StatusText();
    if (o.gotHello) st = "Connected! Getting ready...";
    if (!st.empty()) DrawTextCentered(st.c_str(), VW / 2, y + 10, 20, net::GetState() == net::State::Failed ? Color{255, 130, 100, 255} : UI_TEXT);
    if (!o.err.empty()) DrawTextCentered(o.err.c_str(), VW / 2, y + 40, 10, Color{255, 130, 100, 255}, false);
    GDrawText(TextFormat("Port %d (UDP)", net::DEFAULT_PORT), 60, 326, 10, UI_DIM);
    DrawTextCentered("Esc / (B) back", VW / 2, 340, 10, UI_DIM, false);
    DrawToast(g);
}
