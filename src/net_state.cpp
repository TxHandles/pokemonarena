// Saving and loading a whole battle as bytes (online co-op: the host sends the battle when it
// starts, and again if the two games ever drift apart), plus a checksum to notice drift.
#include "net_state.h"
#include <cstring>
#include <type_traits>
#include "battle.h"

namespace {

struct Ar {
    bool writing = true;
    std::string* out = nullptr;
    const std::string* in = nullptr;
    size_t pos = 0;
    bool ok = true;

    void bytes(void* p, size_t n) {
        if (writing) { out->append((const char*)p, n); return; }
        if (!ok || pos + n > in->size()) { ok = false; std::memset(p, 0, n); return; }
        std::memcpy(p, in->data() + pos, n);
        pos += n;
    }
    template <class T>
    typename std::enable_if<std::is_arithmetic<T>::value || std::is_enum<T>::value>::type v(T& x) { bytes(&x, sizeof x); }
    void v(Vector2& x) { v(x.x); v(x.y); }
    void v(Color& c) { v(c.r); v(c.g); v(c.b); v(c.a); }
    void v(std::string& s) {
        uint32_t n = (uint32_t)s.size();
        v(n);
        if (writing) { out->append(s); return; }
        if (!ok || n > in->size() - pos) { ok = false; s.clear(); return; }
        s.assign(in->data() + pos, n);
        pos += n;
    }
    template <class T> void v(std::vector<T>& vec) {
        uint32_t n = (uint32_t)vec.size();
        v(n);
        if (!writing) {
            if (!ok || n > 100000) { ok = false; return; }
            vec.assign(n, T());
        }
        for (auto& e : vec) v(e);
    }
    template <class T, size_t N> void v(std::array<T, N>& a) { for (auto& e : a) v(e); }
    template <class T, size_t N> void v(T (&a)[N]) { for (auto& e : a) v(e); }

    // Structs
    void v(MoveDef& m) {
        v(m.name); v(m.type); v(m.kind); v(m.power); v(m.cooldown); v(m.range); v(m.radius); v(m.speed);
        v(m.windup); v(m.duration); v(m.stat); v(m.amount); v(m.maxUses); v(m.status); v(m.statusChance);
        v(m.lowers); v(m.lowerStat); v(m.lowerChance); v(m.selfDrop); v(m.selfStat); v(m.drain); v(m.recoil);
        v(m.knock); v(m.homing); v(m.hits); v(m.highCrit); v(m.hex); v(m.quick); v(m.recharge); v(m.count);
        v(m.blast); v(m.patchPower); v(m.patchTime); v(m.patchRadius); v(m.bounces); v(m.weather); v(m.stat2);
        v(m.fixedDmg); v(m.powerMod); v(m.forceSwap); v(m.ohko); v(m.selfKO); v(m.counter); v(m.endure);
        v(m.falseSwipe); v(m.haze); v(m.cure); v(m.metronome); v(m.copycat); v(m.transform); v(m.payDay);
        v(m.original); v(m.cat); v(m.abFx); v(m.scope);
        v(m.arc); v(m.tether); v(m.sleepOnly); v(m.antiAir); v(m.pin); v(m.throwKind); v(m.clears); v(m.trail);
    }
    void v(HitSpec& h) {
        v(h.side); v(h.fighter); v(h.name); v(h.type); v(h.power); v(h.light); v(h.atk); v(h.lvlMul); v(h.t1); v(h.t2);
        v(h.status); v(h.statusChance); v(h.lowers); v(h.lowerStat); v(h.lowerChance); v(h.drain); v(h.recoil);
        v(h.knock); v(h.highCrit); v(h.hex); v(h.heavy); v(h.dmgMul); v(h.superMul); v(h.flinchChance); v(h.scope);
        v(h.contact); v(h.special); v(h.falseSwipe); v(h.forceSwap); v(h.ohko); v(h.brine); v(h.fixedDmg);
        v(h.attLevel); v(h.patchPower); v(h.patchTime); v(h.patchRadius); v(h.look); v(h.ab); v(h.moveIdx); v(h.atkStage); v(h.effects);
    }
    void v(CombatantSpec& c) { v(c.species); v(c.level); v(c.xp); v(c.moves); v(c.ownedId); v(c.hpFrac); v(c.item); v(c.owner); v(c.abilitySlot); v(c.nature); v(c.boss); v(c.shiny); v(c.vit); v(c.iv); v(c.ev); v(c.nick); v(c.flee); }
    void v(SideSetup& s) { v(s.team); v(s.cpu); v(s.level); v(s.earnXp); v(s.label); v(s.team_); v(s.input); v(s.tag); v(s.hot); v(s.hotN); v(s.squad); }
    void v(BattleSetup& s) {
        v(s.side); v(s.twoPlayer); v(s.autoAim); v(s.trainerBattle); v(s.screenShake); v(s.damageNumbers); v(s.arena); v(s.catchable); v(s.night); v(s.daylight); v(s.weather); v(s.venue); v(s.season); v(s.practice); v(s.leader); v(s.raid); v(s.judgeTime);
    }
    void v(StatusState& s) {
        v(s.major); v(s.majorT); v(s.tickT); v(s.paraT); v(s.paraStunT); v(s.toxicN); v(s.confuseT); v(s.confuseAngle);
        v(s.confuseSwitchT); v(s.flinchT); v(s.seedT); v(s.seedTickT); v(s.seedSide);
    }
    void v(AIBrain& b) {
        v(b.actT); v(b.strafeT); v(b.swapThinkT); v(b.dodgeT); v(b.restT); v(b.guardT); v(b.targetT); v(b.dodging); v(b.strafing); v(b.strafeSign);
    }
    void v(Fighter& f) {
        v(f.species);
        if (!writing) f.sp = (f.species >= 0 && f.species < (int)AllSpecies().size()) ? &AllSpecies()[f.species] : &AllSpecies()[0];
        v(f.level); v(f.xp); v(f.ownedId); v(f.moves); v(f.transformFrom); v(f.megaFrom); v(f.megaTo); v(f.origMoves); v(f.origN); v(f.nMoves);
        v(f.maxHp); v(f.hp); v(f.displayHp); v(f.pos); v(f.knock); v(f.moveVel); v(f.aim); v(f.dir); v(f.state); v(f.stateT);
        v(f.cd); v(f.uses); v(f.hitstun); v(f.flash); v(f.animT); v(f.moving); v(f.pendingMove); v(f.windupT); v(f.pendingAim);
        v(f.dashing); v(f.dashT); v(f.dashVel); v(f.dashMove); v(f.dashHit); v(f.buffMul); v(f.buffT); v(f.dropMul); v(f.dropT);
        v(f.st); v(f.shieldT); v(f.rechargeT); v(f.burrowed); v(f.leaping); v(f.burrowT); v(f.burrowMove); v(f.burrowTarget);
        v(f.volleyMove); v(f.volleyLeft); v(f.volleyT); v(f.evolveTo); v(f.envSpeed); v(f.envAtk); v(f.envTickT); v(f.portalCd); v(f.envDef);
        v(f.regenAcc); v(f.dashObsHit); v(f.weakened); v(f.dazeT); v(f.retired); v(f.counterT); v(f.endureT); v(f.lastHitBy);
        v(f.lent); v(f.owner); v(f.boss); v(f.shiny); v(f.vit); v(f.critT); v(f.iv); v(f.ev); v(f.evGain); v(f.nick); v(f.fleeT); v(f.fled);
        std::string item = f.item ? f.item->name : std::string();
        v(item);
        if (!writing) f.item = FindItem(item);
        v(f.focusUsed); v(f.leftAcc); v(f.sentOut); v(f.dmgDealt); v(f.dmgTaken); v(f.hurtT); v(f.hitFresh); v(f.startLevel); v(f.startSpecies); v(f.xpGained); v(f.kos);
        v(f.learnedNow); v(f.pendingLearn); v(f.evolvedNames);
        v(f.ab); v(f.abSlot); v(f.nature); v(f.abSpd); v(f.abAtk); v(f.abDef); v(f.abT); v(f.sinceAct); v(f.flashFire); v(f.snackUsed); v(f.pinchShown); v(f.shiftT);
        v(f.onIce); v(f.slid); v(f.slide); v(f.safeT); v(f.mistT); v(f.chantT); v(f.followT);
        v(f.dodgeT); v(f.dodgeCd); v(f.dodgeVel); v(f.jumpT); v(f.jumpDur); v(f.jumpVel);
        v(f.sweepT); v(f.sweepDur); v(f.sweepA0); v(f.sweepA1); v(f.sweepMove); v(f.sweepMask); v(f.heldT); v(f.heldZ); v(f.pinT);
        v(f.rollT); v(f.rollBoost); v(f.rollDist); v(f.rollTrailT); v(f.rollMove); v(f.rollMask); v(f.rollBounces); v(f.rollVel);
        v(f.inBall); v(f.caught); v(f.caughtBy); v(f.caughtBall);
    }
    void v(BallThrow& t) { v(t.from); v(t.to); v(t.ball); v(t.t); v(t.shakes); v(t.ok); v(t.done); v(t.start); }
    void v(Side& s) {
        v(s.team); v(s.active); v(s.teamId); v(s.input); v(s.target); v(s.tag); v(s.damageDealt); v(s.cpu); v(s.earnXp);
        v(s.level); v(s.label); v(s.swapCd); v(s.megaUsed); v(s.hot); v(s.hotN); v(s.hotStart); v(s.hotCd); v(s.squad); v(s.swapTo); v(s.manualT); v(s.dummy); v(s.sendT); v(s.lastPos); v(s.ai);
    }
    void v(Obstacle& o) {
        v(o.kind); v(o.pos); v(o.radius); v(o.height); v(o.alive); v(o.burnT); v(o.fallT); v(o.fallDir); v(o.respawnT);
        v(o.life); v(o.maxLife); v(o.wallType); v(o.hp); v(o.maxHp); v(o.grow); v(o.seed);
    }
    void v(Puddle& p) { v(p.pos); v(p.r); v(p.maxR); v(p.ice); v(p.shockCd); v(p.shockT); v(p.sheet); v(p.seed); v(p.rink); v(p.thawT); }
    void v(Wisp& w) { v(w.pos); v(w.vel); v(w.life); v(w.respawnT); v(w.seed); }
    void v(Zone& z) { v(z.kind); v(z.pos); v(z.rx); v(z.ry); v(z.seed); }
    void v(Portal& p) { v(p.pos); v(p.age); }
    void v(Tile& t) { v(t.pos); v(t.state); v(t.standT); v(t.t); }
    void v(Turret& t) { v(t.pos); v(t.dir); v(t.warnT); v(t.aimAt); }
    void v(Centerpiece& c) {
        v(c.kind); v(c.pos); v(c.home); v(c.vel); v(c.radius); v(c.phase); v(c.t); v(c.charge); v(c.spin); v(c.spinRate);
        v(c.owner); v(c.fireT); v(c.mask); v(c.bounces); v(c.hitCd); v(c.flash); v(c.marks);
    }
    void v(Field& f) {
        v(f.kind); v(f.obs); v(f.zones); v(f.portals); v(f.turrets); v(f.portalT); v(f.turretT); v(f.lavaT); v(f.tickT);
        v(f.electrifyCd); v(f.waterFlash); v(f.spikeBand); v(f.puddles); v(f.puddleT); v(f.moundT); v(f.venue); v(f.season);
        v(f.wisps); v(f.spot); v(f.spotTo); v(f.spotT); v(f.strikeT); v(f.pulseT); v(f.blinkT);
        v(f.windDir); v(f.windT); v(f.windOn); v(f.lights); v(f.lightTo); v(f.lightT);
        v(f.center);
        v(f.tiles); v(f.phaseT); v(f.phase); v(f.warnT); v(f.jamT); v(f.rainOwner); v(f.rainT); v(f.surgeT); v(f.surgeDir);
        v(f.scaleOwner); v(f.scaleT); v(f.glowOwner); v(f.glowT);
        v(f.xT); v(f.xI); v(f.xMarks); v(f.sideT); v(f.sideFrom); v(f.sideTo);
    }
    void v(Projectile& p) {
        v(p.hit); v(p.pos); v(p.vel); v(p.traveled); v(p.maxDist); v(p.radius); v(p.homing); v(p.lob); v(p.drop); v(p.start);
        v(p.target); v(p.t); v(p.dur); v(p.z); v(p.spin); v(p.alive); v(p.bounces); v(p.boomer); v(p.returning); v(p.hitMask);
        v(p.thrown); v(p.portalCd); v(p.hook); v(p.hookPull); v(p.hookPin); v(p.hookFrom);
    }
    void v(Hazard& h) {
        v(h.hit); v(h.pos); v(h.radius); v(h.life); v(h.maxLife); v(h.tickT); v(h.patch); v(h.orbit); v(h.orbitAngle);
        v(h.orbitDist); v(h.orbitSpeed); v(h.hitCd); v(h.trap); v(h.armT); v(h.pull);
    }
    void v(Tether& t) {
        v(t.hit); v(t.side); v(t.target); v(t.ufi); v(t.tfi); v(t.kind); v(t.life); v(t.maxLife); v(t.range); v(t.leash);
        v(t.tickT); v(t.tickPower); v(t.age); v(t.sleepOnly); v(t.alive);
    }
    void v(Grab& g) {
        v(g.hit); v(g.by); v(g.target); v(g.tfi); v(g.kind); v(g.from); v(g.to); v(g.t); v(g.dur); v(g.height); v(g.bonus); v(g.slam); v(g.alive);
    }
    void v(ShockWave& w) { v(w.hit); v(w.origin); v(w.r); v(w.maxR); v(w.speed); v(w.done); v(w.hitMask); }
    void v(Fx& f) { v(f.kind); v(f.pos); v(f.dir); v(f.life); v(f.maxLife); v(f.radius); v(f.len); v(f.col); v(f.pts); v(f.look); v(f.seed); v(f.z); v(f.type); v(f.sub); }
    void v(Particle& p) { v(p.pos); v(p.vel); v(p.z); v(p.vz); v(p.life); v(p.maxLife); v(p.size); v(p.col); v(p.grav); v(p.kind); v(p.rot); v(p.spin); v(p.grow); v(p.drag); }
    void v(FloatText& t) { v(t.pos); v(t.z); v(t.life); v(t.maxLife); v(t.text); v(t.col); }
};

} // namespace

struct BattleIO {
    static void Visit(Ar& a, Battle& b) {
        uint32_t magic = 0xA7C1A5E1;
        a.v(magic);
        if (magic != 0xA7C1A5E1) { a.ok = false; return; }
        a.v(b.setup); a.v(b.sides); a.v(b.field); a.v(b.projs); a.v(b.hazards); a.v(b.waves); a.v(b.fxs); a.v(b.parts);
        a.v(b.throws);
        a.v(b.texts); a.v(b.time); a.v(b.megaFreezeT); a.v(b.introT); a.v(b.shakeT); a.v(b.sinceDamage); a.v(b.lastHpSum); a.v(b.suddenTick);
        a.v(b.suddenDeath); a.v(b.shakeAmt); a.v(b.winner); a.v(b.overT); a.v(b.envLevel); a.v(b.weather); a.v(b.weatherT);
        a.v(b.weatherTick); a.v(b.lastMove); a.v(b.hasLastMove); a.v(b.payDayCoins); a.v(b.rng.s); a.v(b.gasOn); a.v(b.cloudNine); a.v(b.natural);
        a.v(b.tethers); a.v(b.grabs);
    }
    static uint64_t Hash(const Battle& b) {
        // FNV-1a over the things that matter for the outcome (bit-exact floats).
        uint64_t h = 1469598103934665603ull;
        auto mix = [&](const void* p, size_t n) {
            const unsigned char* c = (const unsigned char*)p;
            for (size_t i = 0; i < n; i++) { h ^= c[i]; h *= 1099511628211ull; }
        };
        auto f = [&](float x) { mix(&x, sizeof x); };
        auto i = [&](int x) { mix(&x, sizeof x); };
        mix(&b.rng.s, sizeof b.rng.s);
        f(b.time); i(b.winner);
        for (const auto& sd : b.sides) {
            i(sd.active);
            for (const auto& fi : sd.team) {
                f(fi.hp); f(fi.pos.x); f(fi.pos.y); i((int)fi.state); i(fi.species); f(fi.cd[0]); f(fi.cd[1]); f(fi.cd[2]); f(fi.cd[3]);
                i(fi.xp); i((int)fi.st.major); f(fi.rollT);
            }
        }
        i((int)b.projs.size());
        for (const auto& p : b.projs) { f(p.pos.x); f(p.pos.y); }
        i((int)b.hazards.size());
        i((int)b.waves.size());
        i((int)b.tethers.size());
        for (const auto& t : b.tethers) f(t.life);
        i((int)b.grabs.size());
        for (const auto& o : b.field.obs) { i(o.alive); f(o.pos.x); f(o.hp); }
        for (const auto& p : b.field.puddles) { f(p.r); f(p.ice); }
        for (const auto& w : b.field.wisps) { f(w.pos.x); f(w.life); }
        f(b.field.spot.x);
        f(b.field.strikeT); f(b.field.blinkT);
        for (const auto& t : b.field.tiles) { i(t.state); f(t.standT); }
        i(b.field.phase); f(b.field.phaseT); f(b.field.rainT); f(b.field.surgeT); f(b.field.scaleT); f(b.field.glowT);
        for (float x : b.field.xT) f(x);
        for (int x : b.field.xI) i(x);
        for (float x : b.field.sideT) f(x);
        i(b.field.center.phase); f(b.field.center.t); f(b.field.center.pos.x); f(b.field.center.charge); i(b.field.center.owner);
        return h;
    }
};

std::string SaveBattle(const Battle& b) {
    std::string s;
    Ar a;
    a.writing = true;
    a.out = &s;
    BattleIO::Visit(a, const_cast<Battle&>(b));
    return s;
}

bool LoadBattle(const std::string& data, Battle& b) {
    Battle tmp;
    Ar a;
    a.writing = false;
    a.in = &data;
    BattleIO::Visit(a, tmp);
    if (!a.ok || a.pos != data.size()) return false;
    b = tmp;
    return true;
}

uint64_t BattleHash(const Battle& b) { return BattleIO::Hash(b); }

std::string SaveCombatants(const std::vector<CombatantSpec>& team) {
    std::string s;
    Ar a;
    a.writing = true;
    a.out = &s;
    auto t = team;
    a.v(t);
    return s;
}

bool LoadCombatants(const std::string& data, std::vector<CombatantSpec>& team) {
    Ar a;
    a.writing = false;
    a.in = &data;
    std::vector<CombatantSpec> t;
    a.v(t);
    if (!a.ok) return false;
    team = t;
    return true;
}
