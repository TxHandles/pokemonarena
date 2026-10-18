#include "audio.h"
#include <cmath>
#include <cstdlib>
#include <vector>
#include "raylib.h"

namespace {

constexpr int RATE = 22050;
constexpr int ALIASES = 3;

struct Slot {
    Sound base{};
    Sound alias[ALIASES]{};
    int next = 0;
    float vol = 0.5f;
    double lastPlay = -1;
    bool ok = false;
};

Slot gSlots[(int)Sfx::Count];
float gMaster = 0.8f, gEffects = 0.8f;
bool gReady = false;

enum Osc { SQUARE, TRIANGLE, SINE, NOISE };

// Append a tone that slides from f0 to f1 Hz over 'dur' seconds.
void Tone(std::vector<float>& out, Osc w, float f0, float f1, float dur, float vol = 1.0f, float attack = 0.005f) {
    int n = (int)(dur * RATE);
    float phase = 0;
    float noiseVal = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float f = f0 + (f1 - f0) * t;
        phase += f / RATE;
        phase -= std::floor(phase);
        float s = 0;
        switch (w) {
            case SQUARE: s = phase < 0.5f ? 0.6f : -0.6f; break;
            case TRIANGLE: s = 4 * std::fabs(phase - 0.5f) - 1; break;
            case SINE: s = std::sin(phase * 6.2831853f); break;
            case NOISE:
                if (i % std::max(1, (int)(RATE / std::max(50.0f, f))) == 0) noiseVal = (std::rand() / (float)RAND_MAX) * 2 - 1;
                s = noiseVal;
                break;
        }
        float env = std::min(1.0f, (float)i / (attack * RATE + 1)) * (1.0f - t) * (1.0f - t * 0.3f);
        out.push_back(s * env * vol);
    }
}

void Mix(std::vector<float>& a, const std::vector<float>& b) {
    if (b.size() > a.size()) a.resize(b.size(), 0);
    for (size_t i = 0; i < b.size(); i++) a[i] += b[i];
}

Sound Make(const std::vector<float>& samples) {
    ::Wave w{};
    w.frameCount = (unsigned)samples.size();
    w.sampleRate = RATE;
    w.sampleSize = 16;
    w.channels = 1;
    short* data = (short*)MemAlloc((unsigned)(samples.size() * sizeof(short)));
    for (size_t i = 0; i < samples.size(); i++) {
        float v = samples[i];
        v = v > 1 ? 1 : (v < -1 ? -1 : v);
        data[i] = (short)(v * 32000);
    }
    w.data = data;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

void Build(Sfx id, const std::vector<float>& samples, float vol) {
    Slot& s = gSlots[(int)id];
    s.base = Make(samples);
    for (int i = 0; i < ALIASES; i++) s.alias[i] = LoadSoundAlias(s.base);
    s.vol = vol;
    s.ok = true;
}

} // namespace

void InitSfx() {
    gReady = IsAudioDeviceReady();
    if (!gReady) return;
    std::vector<float> v;
    auto reset = [&]() { v.clear(); };

    reset(); Tone(v, SQUARE, 700, 320, 0.08f); Build(Sfx::Shoot, v, 0.25f);
    reset(); Tone(v, SQUARE, 1400, 300, 0.12f); { std::vector<float> n; Tone(n, NOISE, 4000, 2000, 0.12f, 0.5f); Mix(v, n); } Build(Sfx::Zap, v, 0.3f);
    reset(); Tone(v, NOISE, 900, 300, 0.18f, 0.8f); Build(Sfx::Fire, v, 0.3f);
    reset(); Tone(v, SINE, 400, 900, 0.10f); Build(Sfx::Water, v, 0.35f);
    reset(); Tone(v, SINE, 160, 55, 0.14f); { std::vector<float> n; Tone(n, NOISE, 600, 200, 0.1f, 0.5f); Mix(v, n); } Build(Sfx::Thud, v, 0.5f);
    reset(); Tone(v, NOISE, 2500, 800, 0.07f, 0.8f); { std::vector<float> n; Tone(n, SQUARE, 220, 140, 0.07f, 0.6f); Mix(v, n); } Build(Sfx::Hit, v, 0.35f);
    reset(); Tone(v, SQUARE, 300, 700, 0.07f); Tone(v, SQUARE, 700, 900, 0.08f); Build(Sfx::HitSuper, v, 0.35f);
    reset(); Tone(v, TRIANGLE, 140, 100, 0.08f); Build(Sfx::HitWeak, v, 0.35f);
    reset(); Tone(v, SQUARE, 900, 1500, 0.06f); Tone(v, SQUARE, 1500, 1500, 0.06f, 0.6f); Build(Sfx::Crit, v, 0.3f);
    reset(); Tone(v, TRIANGLE, 700, 90, 0.55f); Build(Sfx::Faint, v, 0.4f);
    reset(); { const float n[4] = {523, 659, 784, 1046}; for (float f : n) Tone(v, SQUARE, f, f, 0.08f, 0.7f); } Build(Sfx::LevelUp, v, 0.3f);
    reset(); {
        int n = (int)(1.6f * RATE);
        float phase = 0;
        for (int i = 0; i < n; i++) {
            float t = (float)i / n;
            float f = 200 + 1000 * t * t;
            phase += f / RATE;
            phase -= std::floor(phase);
            float trem = 0.6f + 0.4f * std::sin(t * 60);
            v.push_back((phase < 0.5f ? 0.5f : -0.5f) * trem * std::min(1.0f, (1 - t) * 6));
        }
        Tone(v, TRIANGLE, 1046, 1046, 0.25f);
    } Build(Sfx::Evolve, v, 0.3f);
    reset(); Tone(v, SINE, 500, 300, 0.1f); Tone(v, SINE, 300, 500, 0.1f); Build(Sfx::Status, v, 0.35f);
    reset(); { const float n[3] = {600, 800, 1000}; for (float f : n) Tone(v, TRIANGLE, f, f, 0.07f); } Build(Sfx::Heal, v, 0.35f);
    reset(); Tone(v, SINE, 900, 1200, 0.15f); Build(Sfx::Shield, v, 0.3f);
    reset(); Tone(v, SQUARE, 400, 800, 0.14f, 0.7f); Build(Sfx::Buff, v, 0.25f);
    reset(); Tone(v, SQUARE, 880, 880, 0.03f, 0.6f); Build(Sfx::MenuMove, v, 0.2f);
    reset(); Tone(v, SQUARE, 660, 990, 0.07f, 0.7f); Build(Sfx::MenuOk, v, 0.25f);
    reset(); Tone(v, SQUARE, 500, 330, 0.07f, 0.7f); Build(Sfx::MenuBack, v, 0.25f);
    reset(); { const float n[4] = {392, 523, 659, 784}; for (float f : n) Tone(v, SQUARE, f, f, 0.1f, 0.7f); Tone(v, SQUARE, 1046, 1046, 0.35f, 0.7f); } Build(Sfx::Win, v, 0.3f);
}

void UnloadSfx() {
    for (auto& s : gSlots) {
        if (!s.ok) continue;
        for (int i = 0; i < ALIASES; i++) UnloadSoundAlias(s.alias[i]);
        UnloadSound(s.base);
        s.ok = false;
    }
}

void SetSfxVolume(float master, float effects) {
    gMaster = master;
    gEffects = effects;
}

bool gSfxMuted = false;

void PlaySfx(Sfx id) {
    if (!gReady || gSfxMuted) return;
    Slot& s = gSlots[(int)id];
    if (!s.ok) return;
    double now = GetTime();
    if (now - s.lastPlay < 0.03) return;  // don't stack identical sounds in the same instant
    s.lastPlay = now;
    Sound& snd = s.alias[s.next];
    s.next = (s.next + 1) % ALIASES;
    SetSoundVolume(snd, s.vol * gMaster * gEffects);
    PlaySound(snd);
}
