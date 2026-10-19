// Battle animation: the visual layer that makes every move feel big. It watches the battle
// (moves starting, moves going off, Pokemon getting hit or knocked out) and adds what isn't part
// of the battle itself: the user's body language (charging, recoil, lunges, hops), a cast
// flourish themed on the move's type and sized by its power, camera punches toward big hits,
// impact frames, and a short cinematic for the strongest moves.
//
// None of this touches the battle simulation (no battle state, no battle random numbers), so it
// can't change what happens or put an online game out of step.
#include <algorithm>
#include <cmath>
#include <vector>
#include "arena.h"
#include "battle.h"
#include "rlgl.h"
#include "sprites.h"
#include "vec.h"
#include "vfx.h"

namespace {

constexpr float PI_F = 3.14159265f;
constexpr int MAX_SLOTS = 8;

struct SlotAnim {
    int species = -1;
    float lastCd[4] = {0, 0, 0, 0};
    int lastPending = -1;
    float lastHp = -1;
    float relT = -99;            // when a move last went off
    int relKind = 0;             // its MoveKind
    Vector2 relAim{1, 0};
    float relPower = 0;
    int relType = 0;
    float ultUntil = -99;      // a cinematic already running for this Pokemon
    float chargeT = -99;         // when the current wind-up started
    Color chargeCol = WHITE;
    float hitT = -99, hitAmt = 0;
    Vector2 lastPos{0, 0};
    bool havePos = false;
    Vector2 hitFrom{0, 0};
};

enum EvKind { EV_SIGIL, EV_MUZZLE, EV_RINGS, EV_BOLTS, EV_IMPACT, EV_ULTIMATE, EV_KO, EV_SPEED };
struct Ev {
    EvKind kind;
    float t, dur;
    Vector2 wpos;
    float z = 0;
    Vector2 aim{1, 0};
    float power = 0;     // 0..1+
    Color col = WHITE;
    int type = 0;
    uint32_t seed = 1;
    float cam = 0;       // camera punch strength (0 = none)
};

struct VPart {
    PK kind;
    Vector2 wp, wv;
    float z, vz, life, maxLife, size, rot, spin, drag, grow;
    Color col;
    bool grav;
    bool screen;   // already in screen space (wp is a screen point)
};

SlotAnim gSA[MAX_SLOTS];
std::vector<Ev> gEv;
std::vector<VPart> gVP;
float gLastT = -1;
const void* gLastBattle = nullptr;
uint32_t gSeed = 12345;

float Rnd() { gSeed = gSeed * 1664525u + 1013904223u; return (gSeed >> 8) / 16777216.0f; }
float Rnd(float a, float b) { return a + (b - a) * Rnd(); }
Vector2 RndDir() { float a = Rnd(0, 2 * PI_F); return {std::cos(a), std::sin(a)}; }
float Clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
Color Cc(int r, int g, int b) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, 255}; }

VPart& Add(PK k, Vector2 wp, float z, Vector2 wv, float vz, float life, float size, Color c) {
    if (gVP.size() > 900) gVP.erase(gVP.begin(), gVP.begin() + 100);
    VPart p{k, wp, wv, z, vz, life, life, size, Rnd(0, 6.28f), Rnd(-6, 6), 2.5f, 0, c, false, false};
    gVP.push_back(p);
    return gVP.back();
}

bool Ranged(MoveKind k) {
    return k == MoveKind::Projectile || k == MoveKind::Spread || k == MoveKind::Beam || k == MoveKind::Cone || k == MoveKind::Lob ||
           k == MoveKind::Chain || k == MoveKind::Boomerang || k == MoveKind::Orbit;
}
bool Lunge(MoveKind k) { return k == MoveKind::Melee || k == MoveKind::Dash || k == MoveKind::Blink || k == MoveKind::Leap || k == MoveKind::Burrow; }

// The type's cast flourish around the user: particles themed on the element, scaled by power.
void Flourish(Type t, Vector2 at, float k, Vector2 aim, Color glow) {
    auto ring = [&](PK kind, int n, float spd, float up, float life, float size, Color c, bool grav = false, float drag = 2.5f) {
        for (int i = 0; i < n; i++) {
            Vector2 d = RndDir();
            VPart& p = Add(kind, at + d * Rnd(3, 7), Rnd(2, 10), d * Rnd(spd * 0.5f, spd) * k, up * Rnd(0.6f, 1.2f), Rnd(life * 0.7f, life), size * Rnd(0.8f, 1.2f) * std::sqrt(k), c);
            p.grav = grav;
            p.drag = drag;
        }
    };
    int n = (int)(8 * k + 2);
    switch (t) {
        case Type::Fire:
            ring(PK::Flame, n + 4, 55, 45, 0.5f, 2.4f, Cc(255, 130, 40));
            ring(PK::Ember, n, 80, 70, 0.7f, 1.0f, Cc(255, 200, 90));
            break;
        case Type::Water:
            ring(PK::Drop, n + 4, 80, 70, 0.6f, 1.8f, Cc(110, 180, 255), true);
            ring(PK::Bubble, n / 2, 30, 30, 0.6f, 1.6f, Cc(200, 235, 255));
            break;
        case Type::Electric:
            ring(PK::Spark, n + 6, 190, 20, 0.25f, 1.3f, Cc(255, 235, 90), false, 4);
            break;
        case Type::Grass:
            for (int i = 0; i < n + 2; i++) {   // leaves spiralling up around the user
                float a = i * 6.2832f / (n + 2);
                Vector2 d{std::cos(a), std::sin(a)};
                VPart& p = Add(PK::Leaf, at + d * 8.0f, 2, Vector2{-d.y, d.x} * (40 * k) + d * 10.0f, 40 + Rnd(0, 20), Rnd(0.6f, 0.9f), 2.0f * std::sqrt(k), Cc(120, 210, 80));
                p.drag = 1.5f;
            }
            break;
        case Type::Ice:
            ring(PK::Shard, n, 70, 40, 0.6f, 1.8f, Cc(200, 240, 255), true);
            ring(PK::Snow, n, 40, 20, 0.8f, 1.5f, WHITE);
            break;
        case Type::Fighting:
            ring(PK::Spark, n + 4, 160, 10, 0.25f, 1.4f, Cc(255, 200, 120), false, 4);
            ring(PK::Smoke, 4, 30, 6, 0.6f, 3.0f, Cc(220, 200, 170));
            break;
        case Type::Poison:
            ring(PK::Bubble, n + 2, 30, 45, 0.8f, 1.8f, Cc(190, 100, 230));
            break;
        case Type::Ground:
            ring(PK::Debris, n + 2, 60, 90, 0.7f, 1.8f, Cc(150, 120, 85), true);
            ring(PK::Smoke, 5, 35, 8, 0.8f, 3.5f, Cc(180, 150, 110));
            break;
        case Type::Flying:
            ring(PK::Feather, n / 2 + 2, 50, 30, 0.8f, 2.0f, WHITE);
            ring(PK::Glow, n, 110, 10, 0.3f, 1.0f, Cc(200, 225, 255), false, 4);
            break;
        case Type::Psychic:
            for (int i = 0; i < n; i++) {   // motes orbiting inward
                float a = i * 6.2832f / n;
                Vector2 d{std::cos(a), std::sin(a)};
                VPart& p = Add(PK::Glow, at + d * (18 * k), 10, Vector2{-d.y, d.x} * 60.0f - d * 30.0f, 8, 0.45f, 1.4f, Cc(255, 120, 210));
                p.drag = 0.5f;
            }
            break;
        case Type::Bug:
            ring(PK::Glow, n + 2, 90, 20, 0.4f, 1.0f, Cc(190, 230, 70), false, 3);
            ring(PK::Pixel, n, 120, 30, 0.4f, 1.4f, Cc(150, 190, 40), false, 3);
            break;
        case Type::Rock:
            ring(PK::Debris, n + 2, 70, 100, 0.8f, 2.2f, Cc(170, 150, 120), true);
            break;
        case Type::Ghost:
            ring(PK::Wisp, n, 35, 35, 0.8f, 1.8f, Cc(170, 110, 255));
            break;
        case Type::Dragon:
            ring(PK::Flame, n + 2, 50, 60, 0.55f, 2.2f, Cc(140, 90, 255));
            ring(PK::Glow, n, 90, 50, 0.4f, 1.2f, Cc(200, 180, 255));
            break;
        case Type::Dark:
            ring(PK::Smoke, n, 45, 10, 0.7f, 3.4f, Cc(50, 30, 70));
            ring(PK::Wisp, n / 2, 30, 25, 0.6f, 1.4f, Cc(140, 80, 170));
            break;
        case Type::Steel:
            ring(PK::Spark, n + 4, 200, 20, 0.22f, 1.2f, WHITE, false, 4);
            ring(PK::Ember, n / 2, 60, 30, 0.4f, 1.0f, Cc(200, 220, 240));
            break;
        case Type::Fairy:
            ring(PK::Star, n, 60, 40, 0.7f, 1.9f, Cc(255, 200, 235));
            ring(PK::Heart, n / 3 + 1, 30, 35, 0.8f, 1.6f, Cc(255, 140, 200));
            break;
        default:
            ring(PK::Star, n / 2 + 1, 70, 30, 0.5f, 1.6f, WHITE);
            ring(PK::Glow, n, 100, 15, 0.3f, 1.1f, glow, false, 3);
            break;
    }
    (void)aim;
}

void AddEv(EvKind k, float t, float dur, Vector2 wpos, float z, Vector2 aim, float power, Color col, int type, float cam = 0) {
    if (gEv.size() > 60) gEv.erase(gEv.begin());
    if (k == EV_ULTIMATE)   // one cinematic at a time: a second one just gets its shock rings
        for (const Ev& e : gEv)
            if (e.kind == EV_ULTIMATE && t - e.t < e.dur) { k = EV_RINGS; dur = 0.4f; break; }
    Ev e{k, t, dur, wpos, z, aim, power, col, type, (uint32_t)(Rnd() * 1e9f) + 1, cam};
    gEv.push_back(e);
}

} // namespace

// ---------------------------------------------------------------- Watching the battle

void Battle::AnimTick() const {
    float now = time;
    bool fresh = gLastBattle != (const void*)this || now < gLastT - 1.0f || gLastT < 0;
    if (fresh) {   // a new battle: start from what's on the field, no events
        for (auto& sa : gSA) sa = SlotAnim();
        gEv.clear();
        gVP.clear();
    }
    float dt = fresh ? 0 : now - gLastT;
    bool rewind = dt < 0;   // online rollback re-ran a little: just re-read the field
    if (rewind) dt = 0;
    dt = std::min(dt, 0.1f);
    gLastBattle = this;
    gLastT = now;
    gSeed ^= (uint32_t)(now * 1000);

    for (int s = 0; s < N() && s < MAX_SLOTS; s++) {
        const Fighter& f = sides[s].Cur();
        SlotAnim& sa = gSA[s];
        bool baseline = fresh || rewind || sa.species != f.species || f.state != FState::Active;
        if (!baseline) {
            for (int i = 0; i < f.nMoves && i < 4; i++) {
                if (f.cd[i] <= sa.lastCd[i] + 0.05f) continue;   // this move just started
                const MoveDef& m = f.moves[i];
                const Look& L = GetLook(LookKey(m.name, m.type, m.kind), m.type);
                if (m.windup >= 0.2f && f.pendingMove == i) {
                    sa.chargeT = now;
                    sa.chargeCol = L.glow;
                    bool ultimate = m.power >= 120 || (m.kind == MoveKind::Beam && m.power >= 100) || L.cast == Cast::Explosion;
                    if (ultimate) {   // the cinematic starts with the wind-up and lands as the move goes off
                        float d = std::min(1.6f, m.windup) + 0.5f;
                        AddEv(EV_ULTIMATE, now, d, f.pos, 9, f.aim, m.power / 100.0f, L.glow, (int)m.type, 0.05f);
                        sa.ultUntil = now + d;
                    }
                }
            }
            bool released = sa.lastPending >= 0 && f.pendingMove < 0 && f.dodgeT <= 0;
            int relIdx = released ? sa.lastPending : -1;
            if (relIdx < 0)   // a move with no wind-up goes off as it starts
                for (int i = 0; i < f.nMoves && i < 4; i++)
                    if (f.cd[i] > sa.lastCd[i] + 0.05f && f.pendingMove != i) relIdx = i;
            if (relIdx >= 0 && relIdx < f.nMoves) {
                const MoveDef& m = f.moves[relIdx];
                const Look& L = GetLook(LookKey(m.name, m.type, m.kind), m.type);
                float pw = m.power > 0 ? m.power / 100.0f : 0.35f;
                float k = 0.6f + std::min(1.6f, pw) * 0.9f;
                sa.relT = now;
                sa.relKind = (int)m.kind;
                sa.relAim = f.aim;
                sa.relPower = pw;
                sa.relType = (int)m.type;
                sa.chargeT = -99;
                Flourish(m.type, f.pos, k, f.aim, L.glow);
                AddEv(EV_SIGIL, now, 0.55f + pw * 0.25f, f.pos, 0, f.aim, pw, L.glow, (int)m.type);
                if (Ranged(m.kind)) AddEv(EV_MUZZLE, now, 0.2f, f.pos + f.aim * 10.0f, 9, f.aim, pw, L.glow, (int)m.type);
                if (m.type == Type::Electric) AddEv(EV_BOLTS, now, 0.35f, f.pos, 9, f.aim, pw, L.glow, (int)m.type);
                if (m.type == Type::Psychic || m.type == Type::Flying || m.type == Type::Fighting || pw >= 0.6f)
                    AddEv(EV_RINGS, now, 0.45f, f.pos, 2, f.aim, pw, L.glow, (int)m.type);
                if (Lunge(m.kind)) AddEv(EV_SPEED, now, 0.25f, f.pos, 9, f.aim, pw, L.glow, (int)m.type);
                bool ultimate = m.power >= 120 || (m.kind == MoveKind::Beam && m.power >= 100) || L.cast == Cast::Explosion;
                if (ultimate && now > sa.ultUntil) AddEv(EV_ULTIMATE, now, 0.85f, f.pos, 9, f.aim, pw, L.glow, (int)m.type, 0.06f);
                else if (ultimate) AddEv(EV_RINGS, now, 0.4f, f.pos, 9, f.aim, pw, L.glow, (int)m.type, 0.06f);
                else if (m.power >= 90) AddEv(EV_RINGS, now, 0.3f, f.pos, 9, f.aim, pw, L.glow, (int)m.type, 0.025f);
            }
            float amt = sa.lastHp > 0 ? (sa.lastHp - f.hp) / std::max(1, f.maxHp) : 0.0f;
            if (amt >= 0.025f || (sa.lastHp > 0 && f.hp <= 0 && amt > 0)) {   // hit (not the little ticks of burn or poison)
                sa.hitT = now;
                sa.hitAmt = std::min(1.0f, amt * 4 + 0.25f);
                int by = f.lastHitBy;
                if (by >= 0 && by < MAX_SLOTS && now - gSA[by].relT < 3.0f) {   // the attacker's element splashes off the target
                    const SlotAnim& at = gSA[by];
                    Flourish((Type)at.relType, f.pos, 0.45f + std::min(1.2f, amt * 5), {1, 0}, WHITE);
                }
                Color c = Cc(255, 245, 220);
                if (f.hp <= 0) AddEv(EV_KO, now, 0.7f, f.pos, 10, {1, 0}, 1, c, 0, 0.08f);
                else if (amt >= 0.16f) AddEv(EV_IMPACT, now, 0.32f, f.pos, 10, {1, 0}, std::min(1.5f, amt * 3), c, 0, 0.035f + std::min(0.03f, amt * 0.05f));
            }
        }
        if (!baseline && sa.havePos && Dist(sa.lastPos, f.pos) > 45 && !f.dashing) {   // teleported: a flash where it left and where it lands
            const Look& L = GetLook(0, f.sp->type1);
            for (int e = 0; e < 2; e++) {
                Vector2 at = e ? f.pos : sa.lastPos;
                AddEv(EV_RINGS, now, 0.35f, at, 6, {1, 0}, 0.4f, L.glow, (int)f.sp->type1);
                for (int i = 0; i < 14; i++) {
                    Vector2 d = RndDir();
                    VPart& p = Add(PK::Glow, at + d * 3.0f, Rnd(2, 16), d * Rnd(30, 70), Rnd(-10, 30), 0.4f, 1.3f, e ? WHITE : L.glow);
                    p.drag = 3;
                }
            }
        }
        sa.lastPos = f.pos;
        sa.havePos = f.state == FState::Active;
        sa.species = f.species;
        for (int i = 0; i < 4; i++) sa.lastCd[i] = f.cd[i];
        sa.lastPending = f.pendingMove;
        sa.lastHp = f.hp;
        // While winding up: energy gathering into the user
        if (!baseline && f.pendingMove >= 0 && f.windupT > 0 && sa.chargeT > -50 && dt > 0) {
            float rate = 40;
            int n = (int)(rate * dt + Rnd());
            for (int i = 0; i < n; i++) {
                Vector2 d = RndDir();
                float r = Rnd(18, 30);
                VPart& p = Add(PK::Glow, f.pos + d * r, Rnd(2, 16), d * -(r * 3.2f), 6, 0.3f, 1.2f, sa.chargeCol);
                p.drag = 0;
            }
        }
    }
    for (auto it = gVP.begin(); it != gVP.end();) {   // the visual particles
        VPart& p = *it;
        p.life -= dt;
        if (p.life <= 0) { it = gVP.erase(it); continue; }
        p.wp += p.wv * dt;
        p.z += p.vz * dt;
        if (p.grav) { p.vz -= 220 * dt; if (p.z < 0) { p.z = 0; p.vz *= -0.3f; p.wv = p.wv * 0.6f; } }
        float dr = std::max(0.0f, 1 - p.drag * dt);
        p.wv = p.wv * dr;
        if (!p.grav) p.vz *= dr;
        p.rot += p.spin * dt;
        p.size += p.grow * dt;
        ++it;
    }
    gEv.erase(std::remove_if(gEv.begin(), gEv.end(), [&](const Ev& e) { return now - e.t > e.dur; }), gEv.end());
}

// ---------------------------------------------------------------- Body language

void Battle::AnimPose(int s, Vector2& off, float& sx, float& sy, float& lift) const {
    off = {0, 0};
    lift = 0;
    if (s >= MAX_SLOTS) return;
    const Fighter& f = sides[s].Cur();
    const SlotAnim& sa = gSA[s];
    float now = time;
    if (f.pendingMove >= 0 && f.windupT > 0 && sa.chargeT > -50) {   // crouched, trembling, gathering power
        float c = Clamp01((now - sa.chargeT) / 0.25f);
        float pulse = 0.5f + 0.5f * std::sin(now * 30);
        sx *= 1 + 0.06f * c;
        sy *= 1 - 0.07f * c - 0.02f * pulse * c;
        off.x += std::sin(now * 70) * 0.5f * c;
    }
    float u = (now - sa.relT) / 0.32f;
    if (u >= 0 && u < 1) {
        MoveKind k = (MoveKind)sa.relKind;
        Vector2 a{sa.relAim.x, sa.relAim.y * DEPTH};
        if (Ranged(k)) {   // recoil and a stretch toward the shot
            float r = (1 - u) * (1 - u);
            off = off - a * (3.5f + sa.relPower * 2.5f) * r;
            sx *= 1 + 0.12f * r;
            sy *= 1 - 0.06f * r;
        } else if (Lunge(k)) {   // a lunge
            float l = std::sin(u * PI_F);
            off = off + a * (5.0f + sa.relPower * 3.0f) * l;
            sx *= 1 + 0.14f * l;
            sy *= 1 - 0.08f * l;
        } else {   // a hop and a swell (bursts, waves, weather, stat moves)
            float l = std::sin(u * PI_F);
            lift = 5.0f * l;
            sx *= 1 + 0.1f * l;
            sy *= 1 + 0.12f * l;
        }
    }
    float h = (now - sa.hitT) / 0.28f;   // flinch: squashed and knocked about
    if (h >= 0 && h < 1) {
        float a = (1 - h) * sa.hitAmt;
        sx *= 1 + 0.2f * a * std::cos(h * PI_F * 3);
        sy *= 1 - 0.16f * a * std::cos(h * PI_F * 3);
        off.x += std::sin(h * 40) * 1.6f * a;
    }
}

float Battle::AnimGhost(int s, Vector2& back) const {
    if (s >= MAX_SLOTS) return 0;
    const SlotAnim& sa = gSA[s];
    float u = (time - sa.relT) / 0.32f;
    if (u < 0 || u >= 1 || !Lunge((MoveKind)sa.relKind)) return 0;
    back = Vector2{sa.relAim.x, sa.relAim.y * DEPTH} * -7.0f;
    return std::sin(u * PI_F);
}

// ---------------------------------------------------------------- Camera

float Battle::AnimCamera(Vector2& focus) const {
    if (!setup.screenShake) return 0;
    float now = time, zoom = 0, wsum = 0;
    Vector2 fsum{0, 0};
    for (const Ev& e : gEv) {
        if (e.cam <= 0) continue;
        float u = (now - e.t) / e.dur;
        if (u < 0 || u > 1) continue;
        float env = std::sin(Clamp01(u * 5) * PI_F / 2) * (1 - u) * (1 - u);
        float w = e.cam * env;
        zoom += w;
        fsum = fsum + ToScreen(e.wpos, e.z) * w;
        wsum += w;
    }
    if (wsum <= 0) return 0;
    focus = fsum * (1 / wsum);
    return std::min(0.11f, zoom);
}

// ---------------------------------------------------------------- Drawing

void Battle::DrawAnimUnder() const {   // on the ground: casting circles
    float now = time;
    for (const Ev& e : gEv) {
        if (e.kind != EV_SIGIL) continue;
        float u = (now - e.t) / e.dur;
        if (u < 0 || u > 1) continue;
        Vector2 g = ToScreen(e.wpos);
        float grow = std::sin(Clamp01(u * 3) * PI_F / 2);
        float a = (1 - u) * (u < 0.15f ? u / 0.15f : 1);
        float R = (14 + e.power * 10) * (0.6f + 0.4f * grow);
        float spin = now * 2.2f + e.seed % 7;
        VNorm();
        DrawEllipse((int)g.x, (int)g.y, R * 1.05f, R * DEPTH * 1.05f, Fade(e.col, 0.10f * a));
        VAdd();
        VRingE(g, R, R * DEPTH, 1.3f, Fade(e.col, 0.9f * a), 36);
        VRingE(g, R * 0.72f, R * 0.72f * DEPTH, 0.8f, Fade(WHITE, 0.6f * a), 30);
        int spokes = 3 + e.type % 4;   // each type its own pattern
        for (int k = 0; k < spokes * 2; k++) {
            float an = spin + k * PI_F / spokes;
            float r0 = k % 2 ? R * 0.72f : R * 0.35f;
            Vector2 p0{g.x + std::cos(an) * r0, g.y + std::sin(an) * r0 * DEPTH};
            Vector2 p1{g.x + std::cos(an + PI_F / spokes) * R * 0.72f, g.y + std::sin(an + PI_F / spokes) * R * 0.72f * DEPTH};
            DrawLineEx(p0, p1, 0.8f, Fade(e.col, 0.75f * a));
        }
        for (int k = 0; k < 8; k++) {   // runes around the edge
            float an = -spin * 0.7f + k * PI_F / 4;
            Vector2 p{g.x + std::cos(an) * R * 0.86f, g.y + std::sin(an) * R * 0.86f * DEPTH};
            DrawRectangleV({p.x - 0.6f, p.y - 0.6f}, {1.3f, 1.3f}, Fade(WHITE, 0.8f * a));
        }
        VGlow(g, R * 0.8f, e.col, 0.25f * a);
        VNorm();
    }
    for (int s = 0; s < N() && s < MAX_SLOTS; s++) {   // a glow on the ground under a Pokemon gathering power
        const Fighter& f = sides[s].Cur();
        const SlotAnim& sa = gSA[s];
        if (!(f.pendingMove >= 0 && f.windupT > 0 && sa.chargeT > -50)) continue;
        float c = Clamp01((time - sa.chargeT) / 0.4f);
        Vector2 g = ToScreen(f.pos);
        VAdd();
        VGlow(g, 16 + 6 * c + std::sin(time * 25) * 1.5f, sa.chargeCol, 0.35f * c);
        VRingE(g, 18 - 10 * std::fmod(time * 2.5f, 1.0f), (18 - 10 * std::fmod(time * 2.5f, 1.0f)) * DEPTH, 1.0f, Fade(sa.chargeCol, 0.7f * c), 28);
        VNorm();
    }
}

void Battle::DrawAnimOver() const {
    float now = time;
    // the visual particles (normal-blend bodies first, then additive light)
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) VAdd();
        for (const VPart& p : gVP) {
            bool add = PKAdditive(p.kind);
            Vector2 sp = ToScreen(p.wp, p.z);
            Vector2 sv{p.wv.x, p.wv.y * DEPTH - p.vz};
            float lf = p.life / std::max(0.01f, p.maxLife);
            if (pass == 0 && add) VParticleBody(p.kind, sp, sv, p.size, p.rot, lf, p.col, now);
            if (add != (pass == 1)) continue;
            VParticle(p.kind, sp, sv, p.size, p.rot, lf, p.col, now);
            if (pass == 1) VAdd(); else VNorm();
        }
        VNorm();
    }
    for (const Ev& e : gEv) {
        float u = (now - e.t) / e.dur;
        if (u < 0 || u > 1) continue;
        Vector2 c = ToScreen(e.wpos, e.z);
        Vector2 a{e.aim.x, e.aim.y * DEPTH};
        Vector2 n{-a.y, a.x};
        switch (e.kind) {
            case EV_MUZZLE: {   // a flash at the mouth: a star of rays thrown along the shot
                float k = 1 - u;
                VAdd();
                VGlow(c, (6 + e.power * 6) * (0.6f + u), e.col, 0.9f * k);
                VGlow(c, 3 + e.power * 2, WHITE, k);
                for (int i = 0; i < 5; i++) {
                    float sp = (i - 2) * 0.35f;
                    Vector2 d = Rot(a, sp);
                    float L = (8 + e.power * 10) * (0.4f + u) * (i == 2 ? 1.4f : 1.0f);
                    DrawLineEx(c, c + d * L, 1.3f * k, Fade(i == 2 ? WHITE : e.col, k));
                }
                VNorm();
            } break;
            case EV_RINGS: {   // shock rings bursting from the user
                VAdd();
                for (int k = 0; k < 3; k++) {
                    float v = Clamp01(u * 1.4f - k * 0.18f);
                    if (v <= 0 || v >= 1) continue;
                    float R = (8 + (26 + e.power * 18) * v);
                    VRingE(c, R, R * (e.z > 4 ? 0.75f : DEPTH), 1.8f * (1 - v) + 0.3f, Fade(k == 1 ? WHITE : e.col, (1 - v) * 0.9f), 36);
                }
                VNorm();
            } break;
            case EV_BOLTS: {   // crackling arcs around the user
                uint32_t fr = (uint32_t)(now * 30);
                VAdd();
                for (int k = 0; k < 3; k++) {
                    float a1 = VRand(e.seed + fr, k) * 6.2832f;
                    float r = 10 + VRand(e.seed + fr, k + 5) * (10 + e.power * 8);
                    Vector2 p{c.x + std::cos(a1) * r, c.y + std::sin(a1) * r * 0.8f};
                    VBolt(c, p, VHash(e.seed, fr * 3 + k), 0.7f, e.col, 1 - u, 0);
                }
                VNorm();
            } break;
            case EV_SPEED: {   // speed lines streaming behind a lunge
                VAdd();
                for (int k = 0; k < 7; k++) {
                    float off = (k - 3) * 3.0f;
                    float L = 10 + VRand(e.seed, k) * 14;
                    Vector2 s = c + n * off - a * (6 + u * 18 + VRand(e.seed, k + 9) * 6);
                    DrawLineEx(s, s - a * L, 0.9f, Fade(k % 2 ? WHITE : e.col, 0.8f * (1 - u)));
                }
                VNorm();
            } break;
            case EV_IMPACT: case EV_KO: {   // the impact frame: a flash, a ring and radial speed lines
                bool ko = e.kind == EV_KO;
                float k = 1 - u;
                VAdd();
                if (u < 0.18f) DrawRectangle(0, 0, VW, FLOOR_BOTTOM + 8, Fade(WHITE, (ko ? 0.28f : 0.16f) * (1 - u / 0.18f)));
                float R = 10 + (ko ? 70 : 40) * std::sin(Clamp01(u * 2) * PI_F / 2);
                VRingE(c, R, R * 0.8f, 2.2f * k, Fade(e.col, 0.9f * k), 40);
                if (ko) VRingE(c, R * 0.7f, R * 0.56f, 1.2f * k, Fade(Color{255, 200, 120, 255}, 0.8f * k), 40);
                int lines = ko ? 28 : 18;
                for (int i = 0; i < lines; i++) {
                    float an = VRand(e.seed, i) * 6.2832f;
                    Vector2 d{std::cos(an), std::sin(an) * 0.8f};
                    float r0 = 14 + u * 30 + VRand(e.seed, i + 40) * 10;
                    float L = (ko ? 26 : 16) * k + 4;
                    DrawLineEx(c + d * r0, c + d * (r0 + L), 1.0f, Fade(WHITE, 0.75f * k));
                }
                VGlow(c, (ko ? 30 : 18) * (0.6f + u), e.col, 0.6f * k);
                VNorm();
            } break;
            default: break;
        }
    }
}

// The cinematic for the strongest moves: the world dims around the user, bars close in, and
// lines of energy rush toward it.
void Battle::DrawAnimCinematic() const {
    float now = time;
    for (const Ev& e : gEv) {
        if (e.kind != EV_ULTIMATE) continue;
        float u = (now - e.t) / e.dur;
        if (u < 0 || u > 1) continue;
        float in = Clamp01(u / 0.15f), out = Clamp01((1 - u) / 0.3f);
        float a = in * out;
        Vector2 c = ToScreen(e.wpos, e.z);
        DrawRectangle(0, 0, VW, FLOOR_BOTTOM + 8, Fade(Color{8, 6, 20, 255}, 0.32f * a));
        VAdd();
        VGlow(c, 70, e.col, 0.35f * a);
        VGlow(c, 26, WHITE, 0.3f * a);
        if (u < 0.5f) {   // energy rushing in
            for (int i = 0; i < 26; i++) {
                float an = VRand(e.seed, i) * 6.2832f;
                Vector2 d{std::cos(an), std::sin(an)};
                float ph = std::fmod(u * 3 + VRand(e.seed, i + 30), 1.0f);
                float r0 = 30 + (1 - ph) * 220;
                DrawLineEx(c + d * r0, c + d * (r0 + 18 + ph * 20), 1.0f, Fade(i % 3 ? e.col : WHITE, 0.7f * a * ph));
            }
        }
        VNorm();
        int bar = (int)(16 * a);   // letterbox bars
        DrawRectangle(0, 0, VW, bar + 18, Fade(BLACK, 0.85f * a));
        DrawRectangle(0, FLOOR_BOTTOM + 8 - bar, VW, bar, Fade(BLACK, 0.85f * a));
        DrawRectangle(0, bar + 18, VW, 1, Fade(e.col, 0.8f * a));
        DrawRectangle(0, FLOOR_BOTTOM + 8 - bar - 1, VW, 1, Fade(e.col, 0.8f * a));
    }
}
