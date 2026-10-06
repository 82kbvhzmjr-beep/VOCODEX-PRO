// VOCODEX PRO - motor de audio (C++ puro, sin JUCE)
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace vx
{
constexpr float kPi = 3.14159265358979323846f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

//==============================================================================
// Biquad (RBJ cookbook, forma transpuesta II)
//==============================================================================
enum class FType { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf };

struct BiquadCoef
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

inline BiquadCoef makeBiquad (FType type, double fs, double f, double q, double gainDb)
{
    f = std::min (std::max (f, 10.0), fs * 0.45);
    q = std::max (q, 0.05);
    const double w0 = 2.0 * 3.14159265358979323846 * f / fs;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / (2.0 * q);
    const double A = std::pow (10.0, gainDb / 40.0);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

    switch (type)
    {
        case FType::LowPass:
            b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case FType::HighPass:
            b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case FType::BandPass:
            b0 = alpha; b1 = 0; b2 = -alpha;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case FType::Peak:
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
        case FType::LowShelf:
        {
            const double sA = std::sqrt (A);
            b0 = A * ((A + 1) - (A - 1) * cw + 2 * sA * alpha);
            b1 = 2 * A * ((A - 1) - (A + 1) * cw);
            b2 = A * ((A + 1) - (A - 1) * cw - 2 * sA * alpha);
            a0 = (A + 1) + (A - 1) * cw + 2 * sA * alpha;
            a1 = -2 * ((A - 1) + (A + 1) * cw);
            a2 = (A + 1) + (A - 1) * cw - 2 * sA * alpha; break;
        }
        case FType::HighShelf:
        {
            const double sA = std::sqrt (A);
            b0 = A * ((A + 1) + (A - 1) * cw + 2 * sA * alpha);
            b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            b2 = A * ((A + 1) + (A - 1) * cw - 2 * sA * alpha);
            a0 = (A + 1) - (A - 1) * cw + 2 * sA * alpha;
            a1 = 2 * ((A - 1) - (A + 1) * cw);
            a2 = (A + 1) - (A - 1) * cw - 2 * sA * alpha; break;
        }
    }

    BiquadCoef c;
    c.b0 = (float) (b0 / a0); c.b1 = (float) (b1 / a0); c.b2 = (float) (b2 / a0);
    c.a1 = (float) (a1 / a0); c.a2 = (float) (a2 / a0);
    return c;
}

struct Biquad
{
    BiquadCoef c;
    float z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float x)
    {
        const float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// Dos canales con los mismos coeficientes
struct BiquadSt
{
    Biquad ch[2];
    void set (const BiquadCoef& c) { ch[0].c = c; ch[1].c = c; }
    void reset() { ch[0].reset(); ch[1].reset(); }
    inline float process (int c, float x) { return ch[c].process (x); }
};

struct DcBlocker
{
    float x1 = 0, y1 = 0;
    inline float process (float x)
    {
        const float y = x - x1 + 0.995f * y1;
        x1 = x; y1 = y; return y;
    }
    void reset() { x1 = y1 = 0; }
};

//==============================================================================
// Detector de tono (YIN) sobre audio decimado x2
//==============================================================================
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        fsd = (float) (sampleRate * 0.5);
        tauMin = std::max (2, (int) (fsd / 1000.0f));
        tauMax = (int) (fsd / 70.0f) + 2;
        W = std::max (1024, tauMax);
        ringSize = 1;
        while (ringSize < W + tauMax + 8) ringSize <<= 1;
        ring.assign ((size_t) ringSize, 0.0f);
        lin.assign ((size_t) (W + tauMax + 2), 0.0f);
        d.assign ((size_t) (tauMax + 2), 0.0f);
        cm.assign ((size_t) (tauMax + 2), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        wr = 0; hop = 0; toggle = false; acc = 0.0f;
        midi = -1.0f; hangover = 0;
    }

    // Devuelve true cuando hay un nuevo resultado en getMidi()
    inline bool push (float x)
    {
        // decimacion x2 con media de 2 muestras
        if (! toggle) { acc = x; toggle = true; return false; }
        toggle = false;
        ring[(size_t) wr] = 0.5f * (acc + x);
        wr = (wr + 1) & (ringSize - 1);
        if (++hop >= 256) { hop = 0; analyse(); return true; }
        return false;
    }

    // midi fraccionario o -1 si no hay voz
    float getMidi() const { return midi; }

private:
    void analyse()
    {
        const int total = W + tauMax;
        for (int i = 0; i < total; ++i)
            lin[(size_t) i] = ring[(size_t) ((wr - total + i) & (ringSize - 1))];

        // puerta de energia
        float e = 0;
        for (int i = 0; i < W; ++i) e += lin[(size_t) (tauMax + i)] * lin[(size_t) (tauMax + i)];
        e = std::sqrt (e / (float) W);
        if (e < 0.004f)
        {
            if (++hangover > 3) midi = -1.0f;
            return;
        }

        // YIN: funcion de diferencia
        const float* x = lin.data() + tauMax;   // ventana mas reciente
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0;
            const float* a = x;
            const float* b = x - tau;
            for (int j = 0; j < W; ++j) { const float df = a[j] - b[j]; s += df * df; }
            d[(size_t) tau] = s;
        }
        cm[0] = 1.0f;
        float run = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            run += d[(size_t) tau];
            cm[(size_t) tau] = run > 1e-12f ? d[(size_t) tau] * (float) tau / run : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cm[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && cm[(size_t) (tau + 1)] < cm[(size_t) tau]) ++tau;
                best = tau; break;
            }
        }
        if (best < 0)
        {
            if (++hangover > 3) midi = -1.0f;
            return;
        }

        float tauF = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = cm[(size_t) (best - 1)], s1 = cm[(size_t) best], s2 = cm[(size_t) (best + 1)];
            const float den = s0 + s2 - 2.0f * s1;
            if (std::fabs (den) > 1e-9f) tauF += 0.5f * (s0 - s2) / den;
        }
        const float f0 = fsd / tauF;
        if (f0 < 60.0f || f0 > 1100.0f) { if (++hangover > 3) midi = -1.0f; return; }

        hangover = 0;
        midi = 69.0f + 12.0f * std::log2 (f0 / 440.0f);
    }

    float fsd = 24000.0f;
    int tauMin = 24, tauMax = 342, W = 1024, ringSize = 2048, wr = 0, hop = 0, hangover = 0;
    bool toggle = false;
    float acc = 0.0f, midi = -1.0f;
    std::vector<float> ring, lin, d, cm;
};

//==============================================================================
// Cambiador de tono (dos taps con crossfade) con latencia fija
//==============================================================================
class PitchShifter
{
public:
    void prepare (double fs)
    {
        N = std::floor ((float) (0.030 * fs));
        size = (int) N * 2 + 8;
        buf.assign ((size_t) size, 0.0f);
        w = 0; phase = 0.0f;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; phase = 0.0f; }
    int latencySamples() const { return (int) (N * 0.5f); }

    inline float process (float x, float ratio, float mix)
    {
        buf[(size_t) w] = x;
        w = (w + 1) % size;

        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        const float w1 = 0.5f - 0.5f * std::cos (2.0f * kPi * phase);
        const float w2 = 0.5f - 0.5f * std::cos (2.0f * kPi * p2);
        const float shifted = w1 * tap (phase * N) + w2 * tap (p2 * N);
        const float dry = tap (N * 0.5f);

        phase += (1.0f - ratio) / N;
        phase -= std::floor (phase);

        return dry * (1.0f - mix) + shifted * mix;
    }

private:
    inline float tap (float delay) const
    {
        float r = (float) (w - 1) - delay;
        while (r < 0.0f) r += (float) size;
        int i0 = (int) r;
        const float fr = r - (float) i0;
        if (i0 >= size) i0 -= size;
        int i1 = i0 + 1;
        if (i1 >= size) i1 -= size;
        return buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr;
    }

    std::vector<float> buf;
    int size = 8, w = 0;
    float N = 1.0f, phase = 0.0f;
};

//==============================================================================
// Compresor estereo enlazado
//==============================================================================
class Compressor
{
public:
    void prepare (double sr) { fs = (float) sr; grDb = 0.0f; }
    void reset() { grDb = 0.0f; }

    void set (float amount01, int mode)
    {
        amount = amount01;
        float maxRatio = 3.0f, attMs = 15.0f, relMs = 150.0f;
        knee = 12.0f;
        if (mode == 1) { maxRatio = 4.0f; attMs = 30.0f; relMs = 80.0f; knee = 6.0f; }
        if (mode == 2) { maxRatio = 8.0f; attMs = 3.0f; relMs = 60.0f; knee = 3.0f; }
        threshold = -6.0f - 24.0f * amount;
        ratio = 1.0f + amount * (maxRatio - 1.0f);
        att = std::exp (-1.0f / (attMs * 0.001f * fs));
        rel = std::exp (-1.0f / (relMs * 0.001f * fs));
        makeupDb = -threshold * (1.0f - 1.0f / ratio) * 0.45f;
    }

    inline void process (float& l, float& r)
    {
        if (amount < 0.001f) return;
        const float lvl = std::max (std::fabs (l), std::fabs (r));
        const float lvlDb = 20.0f * std::log10 (std::max (lvl, 1e-6f));
        const float over = lvlDb - threshold;
        float outDb;
        if (2.0f * over < -knee) outDb = lvlDb;
        else if (2.0f * std::fabs (over) <= knee)
        {
            const float t = over + knee * 0.5f;
            outDb = lvlDb + (1.0f / ratio - 1.0f) * t * t / (2.0f * knee);
        }
        else outDb = threshold + over / ratio;

        const float target = outDb - lvlDb;           // <= 0
        const float coef = target < grDb ? att : rel;
        grDb = coef * grDb + (1.0f - coef) * target;
        const float g = dbToGain (grDb + makeupDb);
        l *= g; r *= g;
    }

    float getGainReductionDb() const { return grDb; }

private:
    float fs = 48000.0f, amount = 0, threshold = -10, ratio = 1, knee = 6, att = 0, rel = 0, makeupDb = 0, grDb = 0;
};

//==============================================================================
// De-esser (banda dividida)
//==============================================================================
class DeEsser
{
public:
    void prepare (double sr) { fs = sr; reset(); }
    void reset() { det.reset(); band.reset(); env = 0.0f; }

    void set (float amount01, float focusHz)
    {
        amount = amount01;
        const auto c = makeBiquad (FType::BandPass, fs, focusHz, 1.3, 0.0);
        det.set (c);
        band.set (c);
        thresholdDb = -16.0f - 24.0f * amount;
        att = std::exp (-1.0f / (0.001f * (float) fs));
        rel = std::exp (-1.0f / (0.040f * (float) fs));
    }

    inline void process (float& l, float& r)
    {
        if (amount < 0.001f) return;
        const float bl = band.process (0, l);
        const float br = band.process (1, r);
        const float s = det.process (0, 0.5f * (l + r));
        const float a = std::fabs (s);
        env = a > env ? att * env + (1.0f - att) * a : rel * env + (1.0f - rel) * a;
        const float envDb = 20.0f * std::log10 (std::max (env, 1e-6f));
        float redDb = envDb > thresholdDb ? (envDb - thresholdDb) * 0.7f : 0.0f;
        redDb = std::min (redDb, 14.0f);
        const float g = dbToGain (-redDb);
        l -= bl * (1.0f - g);
        r -= br * (1.0f - g);
    }

private:
    double fs = 48000.0;
    BiquadSt det, band;
    float amount = 0, thresholdDb = -20, att = 0, rel = 0, env = 0;
};

//==============================================================================
// Saturacion
//==============================================================================
inline float saturate (float x, float drive)
{
    return std::tanh (x * drive) / drive * (1.0f + (drive - 1.0f) * 0.35f);
}

//==============================================================================
// Reverb tipo Freeverb
//==============================================================================
class Reverb
{
public:
    void prepare (double sr)
    {
        const double sc = sr / 44100.0;
        static const int ct[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int at[4] = { 556, 441, 341, 225 };
        for (int i = 0; i < 8; ++i)
        {
            combL[i].init ((int) (ct[i] * sc));
            combR[i].init ((int) ((ct[i] + 23) * sc));
        }
        for (int i = 0; i < 4; ++i)
        {
            apL[i].init ((int) (at[i] * sc));
            apR[i].init ((int) ((at[i] + 23) * sc));
        }
        preSize = (int) (0.06 * sr) + 4;
        pre.assign ((size_t) preSize, 0.0f);
        pw = 0; fs = sr;
        lcut.reset(); hcut.reset();
    }

    void reset()
    {
        for (int i = 0; i < 8; ++i) { combL[i].clear(); combR[i].clear(); }
        for (int i = 0; i < 4; ++i) { apL[i].clear(); apR[i].clear(); }
        std::fill (pre.begin(), pre.end(), 0.0f);
        lcut.reset(); hcut.reset();
    }

    void set (int type, float mix01, float lowCutHz, float highCutHz)
    {
        float size = 0.80f, d = 0.2f, preMs = 0.0f;
        if (type == 1) { size = 0.50f; d = 0.45f; preMs = 8.0f; }
        if (type == 2) { size = 0.93f; d = 0.35f; preMs = 25.0f; }
        feedback = 0.70f + 0.28f * size;
        damp = d * 0.4f;
        preDelay = std::min ((int) (preMs * 0.001f * (float) fs), preSize - 2);
        mix = mix01;
        lcut.set (makeBiquad (FType::HighPass, fs, lowCutHz, 0.707, 0.0));
        hcut.set (makeBiquad (FType::LowPass, fs, highCutHz, 0.707, 0.0));
    }

    inline void process (float inL, float inR, float& wetL, float& wetR)
    {
        if (mix < 0.001f) { wetL = wetR = 0.0f; return; }
        pre[(size_t) pw] = (inL + inR) * 0.015f;
        int rp = pw - preDelay; if (rp < 0) rp += preSize;
        const float in = pre[(size_t) rp];
        pw = (pw + 1) % preSize;

        float oL = 0, oR = 0;
        for (int i = 0; i < 8; ++i)
        {
            oL += combL[i].process (in, feedback, damp);
            oR += combR[i].process (in, feedback, damp);
        }
        for (int i = 0; i < 4; ++i)
        {
            oL = apL[i].process (oL);
            oR = apR[i].process (oR);
        }
        oL *= 1.6f; oR *= 1.6f;
        oL = hcut.process (0, lcut.process (0, oL));
        oR = hcut.process (1, lcut.process (1, oR));
        wetL = oL * mix; wetR = oR * mix;
    }

private:
    struct Comb
    {
        std::vector<float> b; int i = 0; float store = 0;
        void init (int n) { b.assign ((size_t) std::max (n, 1), 0.0f); i = 0; store = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); i = 0; store = 0; }
        inline float process (float in, float fb, float dp)
        {
            const float o = b[(size_t) i];
            store = o * (1.0f - dp) + store * dp;
            b[(size_t) i] = in + store * fb;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };
    struct Allpass
    {
        std::vector<float> b; int i = 0;
        void init (int n) { b.assign ((size_t) std::max (n, 1), 0.0f); i = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); i = 0; }
        inline float process (float in)
        {
            const float bo = b[(size_t) i];
            const float o = -in + bo;
            b[(size_t) i] = in + bo * 0.5f;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };

    Comb combL[8], combR[8];
    Allpass apL[4], apR[4];
    std::vector<float> pre;
    int preSize = 8, pw = 0, preDelay = 0;
    double fs = 48000.0;
    float feedback = 0.8f, damp = 0.1f, mix = 0.0f;
    BiquadSt lcut, hcut;
};

//==============================================================================
// Delay estereo sincronizado
//==============================================================================
class StereoDelay
{
public:
    void prepare (double sr)
    {
        fs = sr;
        size = (int) (3.0 * sr) + 8;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        w = 0; cur[0] = cur[1] = (float) (0.25 * sr);
        lc.reset(); hc.reset();
    }
    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        w = 0; lc.reset(); hc.reset();
    }

    void set (int timeIdx, double bpm, float mix01, float fb01, float lowCutHz, float highCutHz)
    {
        static const float beats[3] = { 0.25f, 0.5f, 1.0f };
        if (bpm < 20.0) bpm = 120.0;
        const double sec = (60.0 / bpm) * beats[std::max (0, std::min (2, timeIdx))];
        target = (float) std::min (sec * fs, (double) (size - 4));
        mix = mix01;
        fb = std::min (fb01, 0.9f);
        lc.set (makeBiquad (FType::HighPass, fs, lowCutHz, 0.707, 0.0));
        hc.set (makeBiquad (FType::LowPass, fs, highCutHz, 0.707, 0.0));
    }

    inline void process (float inL, float inR, float& wetL, float& wetR)
    {
        if (mix < 0.001f)
        {
            // mantenemos el buffer corriendo para que el eco no suene cortado al activarlo
            buf[0][(size_t) w] = 0.0f; buf[1][(size_t) w] = 0.0f;
            w = (w + 1) % size;
            wetL = wetR = 0.0f; return;
        }
        float out[2];
        const float in[2] = { inL, inR };
        for (int c = 0; c < 2; ++c)
        {
            cur[c] += (target - cur[c]) * 0.0005f;
            float r = (float) w - cur[c];
            while (r < 0.0f) r += (float) size;
            int i0 = (int) r; const float fr = r - (float) i0;
            if (i0 >= size) i0 -= size;
            int i1 = i0 + 1; if (i1 >= size) i1 = 0;
            const float d = buf[c][(size_t) i0] * (1.0f - fr) + buf[c][(size_t) i1] * fr;
            out[c] = d;
        }
        // ping-pong suave: la realimentacion cruza de canal
        const float f0 = hc.process (0, lc.process (0, out[1])) * fb;
        const float f1 = hc.process (1, lc.process (1, out[0])) * fb;
        buf[0][(size_t) w] = in[0] + f0;
        buf[1][(size_t) w] = in[1] + f1;
        w = (w + 1) % size;
        wetL = out[0] * mix; wetR = out[1] * mix;
    }

private:
    std::vector<float> buf[2];
    int size = 8, w = 0;
    double fs = 48000.0;
    float cur[2] = { 0, 0 }, target = 1000.0f, mix = 0, fb = 0.3f;
    BiquadSt lc, hc;
};

//==============================================================================
// Perfiles de microfono (caracter, no clones 1:1)
//==============================================================================
struct MicProfile
{
    float lsDb, lsF;
    float p1Db, p1F, p1Q;
    float p2Db, p2F, p2Q;
    float hsDb, hsF;
    float sat;
};

inline const MicProfile& micProfile (int i)
{
    static const MicProfile t[8] = {
        //  ls          peak1              peak2               hs         sat
        { 1.5f, 110.f, -1.0f, 400.f, 1.0f, 2.0f, 5000.f, 0.8f, 1.5f, 12000.f, 0.30f },  // Manley-Style Tube
        { 1.0f, 100.f,  0.0f, 300.f, 1.0f, 2.5f, 7500.f, 0.9f, 2.0f, 14000.f, 0.10f },  // Neumann TLM 103-Style
        { 0.0f, 100.f, -1.5f, 250.f, 1.0f, 2.0f, 5000.f, 0.8f, 1.5f, 10000.f, 0.05f },  // AKG C414 XLII-Style
        { 0.0f, 100.f, -1.0f, 300.f, 1.0f, 2.5f, 9000.f, 0.9f, 1.0f, 12000.f, 0.05f },  // RODE NT1A-Style
        { 0.0f, 100.f,  1.0f, 150.f, 1.0f, 3.0f, 6000.f, 1.0f, 0.0f, 12000.f, 0.05f },  // Audio-Technica AT2020-Style
        { 1.0f, 100.f, -0.5f, 500.f, 1.0f, 1.5f, 4000.f, 0.9f, 1.0f, 11000.f, 0.05f },  // AKG P220-Style
        { -1.0f, 80.f,  1.0f, 200.f, 1.0f, 3.0f, 4500.f, 1.2f, -3.0f, 12000.f, 0.15f }, // Sennheiser e835-Style
        { 0.0f, 100.f,  1.5f, 200.f, 1.0f, 1.0f, 3000.f, 1.0f, -1.5f, 10000.f, 0.05f }  // Blue Yeti-Style
    };
    return t[std::max (0, std::min (7, i))];
}

//==============================================================================
// Parametros (valores reales, no normalizados)
//==============================================================================
struct Params
{
    float lowCut = 20, eqLow = 0, eqMid = 0, eqHigh = 0;
    float comp = 0; int compMode = 0; float deess = 0, deessFocus = 7000;
    bool tuneOn = false; int key = 0, scale = 0; float tuneSpeed = 20, tuneAmount = 100, tuneTol = 10;
    int toneType = 0; float toneInt = 0, magic = 0;
    float midAir = 0, highAir = 0;
    int mic = 1; float micBody = 50, micAir = 50;
    int revType = 0; float revMix = 0, revLow = 150, revHigh = 10000;
    int dlyTime = 1; float dlyMix = 0, dlyFb = 30, dlyLow = 150, dlyHigh = 8000;
};

//==============================================================================
// Motor completo
//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        detector.prepare (fs);
        for (auto& s : shifter) s.prepare (fs);
        comp.prepare (fs);
        deess.prepare (fs);
        reverb.prepare (fs);
        delay.prepare (fs);
        reset();
    }

    void reset()
    {
        detector.reset();
        for (auto& s : shifter) s.reset();
        comp.reset(); deess.reset(); reverb.reset(); delay.reset();
        BiquadSt* all[] = { &hp, &eqL, &eqM, &eqH, &micLs, &micP1, &micP2, &micHs, &micBody, &micAir,
                            &magicPres, &magicLow, &exciterHp, &toneLp, &toneHp, &airMid, &airHigh, &airExHp };
        for (auto* b : all) b->reset();
        for (auto& d : dc) d.reset();
        centsState = 0.0f; centsTarget = 0.0f; detectedMidi.store (-1.0f);
    }

    int getLatencySamples() const { return shifter[0].latencySamples(); }

    // El procesador puede leer esto desde el hilo de la interfaz
    std::atomic<float> detectedMidi { -1.0f };

    void process (float* L, float* R, int n, double bpm, const Params& p)
    {
        updateCoefs (p, bpm);
        const bool tuneOn = p.tuneOn;
        const float speedCoef = p.tuneSpeed <= 0.5f ? 0.0f
                                 : std::exp (-1.0f / (p.tuneSpeed * 0.001f * (float) fs));
        const float amount = p.tuneAmount * 0.01f;
        const float tol = p.tuneTol;

        for (int i = 0; i < n; ++i)
        {
            float l = L[i];
            float r = R != nullptr ? R[i] : l;

            //---------------- TUNE ----------------
            if (tuneOn)
            {
                if (detector.push (0.5f * (l + r)))
                {
                    const float m = detector.getMidi();
                    detectedMidi.store (m);
                    if (m < 0.0f) centsTarget = 0.0f;
                    else
                    {
                        const float err = nearestScaleNoteMidi (m, p.key, p.scale) - m;   // semitonos
                        const float errCents = err * 100.0f;
                        centsTarget = std::fabs (errCents) <= tol ? 0.0f
                                                                  : clampf (errCents * amount, -300.0f, 300.0f);
                    }
                }
            }
            else
            {
                centsTarget = 0.0f;
                if (detectedMidi.load() >= 0.0f) detectedMidi.store (-1.0f);
            }
            centsState = speedCoef * centsState + (1.0f - speedCoef) * centsTarget;
            const float ratio = std::exp2 (centsState / 1200.0f);
            const float mixShift = std::min (1.0f, std::fabs (centsState) * 0.5f);
            l = shifter[0].process (l, ratio, mixShift);
            r = shifter[1].process (r, ratio, mixShift);

            //---------------- EQ ----------------
            l = hp.process (0, l);        r = hp.process (1, r);
            l = eqL.process (0, l);       r = eqL.process (1, r);
            l = eqM.process (0, l);       r = eqM.process (1, r);
            l = eqH.process (0, l);       r = eqH.process (1, r);

            //---------------- MIC ----------------
            l = micChain (0, l, micSat);  r = micChain (1, r, micSat);

            //---------------- DINAMICA ----------------
            comp.process (l, r);
            deess.process (l, r);

            //---------------- TONE (+ MAGIC) ----------------
            l = toneProc (0, l, p);       r = toneProc (1, r, p);

            //---------------- AIR ----------------
            l = airProc (0, l);           r = airProc (1, r);

            //---------------- ENVIOS ----------------
            float dl = 0, dr = 0, rl = 0, rr = 0;
            delay.process (l, r, dl, dr);
            reverb.process (l + dl * 0.5f, r + dr * 0.5f, rl, rr);
            l += dl + rl;
            r += dr + rr;

            //---------------- SEGURIDAD ----------------
            l = softClip (l);
            r = softClip (r);

            L[i] = l;
            if (R != nullptr) R[i] = r;
        }
    }

private:
    static float softClip (float x)
    {
        const float a = std::fabs (x);
        if (a <= 0.9f) return x;
        const float y = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
        return x < 0 ? -y : y;
    }

    static float nearestScaleNoteMidi (float m, int key, int scale)
    {
        static const int masks[4] = {
            0xFFF,                                            // Chromatic
            (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),  // Major
            (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),  // Minor
            (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10)                          // Pent. minor
        };
        const int mask = masks[std::max (0, std::min (3, scale))];
        const int base = (int) std::floor (m + 0.5f);
        float best = (float) base, bestD = 1e9f;
        for (int n = base - 3; n <= base + 3; ++n)
        {
            const int deg = (((n - key) % 12) + 12) % 12;
            if (mask & (1 << deg))
            {
                const float dd = std::fabs ((float) n - m);
                if (dd < bestD) { bestD = dd; best = (float) n; }
            }
        }
        return best;
    }

    inline float micChain (int c, float x, float sat)
    {
        x = micBody.process (c, x);
        x = micLs.process (c, x);
        x = micP1.process (c, x);
        x = micP2.process (c, x);
        x = micHs.process (c, x);
        x = micAir.process (c, x);
        if (sat > 0.001f)
        {
            const float drive = 1.0f + sat * 3.0f;
            x = dcMic[c].process (saturate (x + 0.1f * sat * x * x, drive));
        }
        return x;
    }

    inline float toneProc (int c, float x, const Params& p)
    {
        const float inten = toneAmt;
        if (inten > 0.001f)
        {
            float wet = x;
            switch (p.toneType)
            {
                case 0: // Warm: saturacion asimetrica + paso bajo suave
                {
                    const float d = 1.0f + inten * 2.5f;
                    wet = dc[c].process (saturate (x + 0.25f * x * x * inten, d));
                    wet = toneLp.process (c, wet);
                    break;
                }
                case 1: // Agudo: armonicos en la parte alta
                {
                    const float hi = toneHp.process (c, x);
                    wet = x + std::tanh (hi * (2.0f + inten * 6.0f)) * 0.35f * inten;
                    break;
                }
                default: // Saturacion
                {
                    const float d = 1.0f + inten * 5.0f;
                    wet = saturate (x, d);
                    break;
                }
            }
            x = x * (1.0f - inten * 0.7f) + wet * (inten * 0.7f);
        }

        // MAGIC: presencia + calidez + brillo armonico en paralelo
        if (magicAmt > 0.001f)
        {
            float y = magicLow.process (c, x);
            y = magicPres.process (c, y);
            const float hi = exciterHp.process (c, x);
            y += std::tanh (hi * 4.0f) * 0.12f * magicAmt;
            x = y;
        }
        return x;
    }

    inline float airProc (int c, float x)
    {
        if (airMidAmt > 0.001f) x = airMid.process (c, x);
        if (airHighAmt > 0.001f)
        {
            x = airHigh.process (c, x);
            const float hi = airExHp.process (c, x);
            x += std::tanh (hi * 3.0f) * 0.10f * airHighAmt;
        }
        return x;
    }

    void updateCoefs (const Params& p, double bpm)
    {
        hp.set  (makeBiquad (FType::HighPass, fs, p.lowCut, 0.707, 0.0));
        eqL.set (makeBiquad (FType::LowShelf, fs, 120.0, 0.7, p.eqLow));
        eqM.set (makeBiquad (FType::Peak, fs, 800.0, 0.8, p.eqMid));
        eqH.set (makeBiquad (FType::HighShelf, fs, 8000.0, 0.7, p.eqHigh));

        const auto& m = micProfile (p.mic);
        micLs.set (makeBiquad (FType::LowShelf, fs, m.lsF, 0.7, m.lsDb));
        micP1.set (makeBiquad (FType::Peak, fs, m.p1F, m.p1Q, m.p1Db));
        micP2.set (makeBiquad (FType::Peak, fs, m.p2F, m.p2Q, m.p2Db));
        micHs.set (makeBiquad (FType::HighShelf, fs, m.hsF, 0.7, m.hsDb));
        micBody.set (makeBiquad (FType::LowShelf, fs, 150.0, 0.7, (p.micBody - 50.0f) / 50.0f * 5.0f));
        micAir.set  (makeBiquad (FType::HighShelf, fs, 11000.0, 0.7, (p.micAir - 50.0f) / 50.0f * 5.0f));
        micSat = m.sat;

        comp.set (p.comp * 0.01f, p.compMode);
        deess.set (p.deess * 0.01f, p.deessFocus);

        toneAmt = p.toneInt * 0.01f;
        toneLp.set (makeBiquad (FType::LowPass, fs, 9000.0, 0.707, 0.0));
        toneHp.set (makeBiquad (FType::HighPass, fs, 2500.0, 0.707, 0.0));

        magicAmt = p.magic * 0.01f;
        magicLow.set  (makeBiquad (FType::LowShelf, fs, 200.0, 0.7, 1.5 * magicAmt));
        magicPres.set (makeBiquad (FType::Peak, fs, 3500.0, 0.8, 2.5 * magicAmt));
        exciterHp.set (makeBiquad (FType::HighPass, fs, 5000.0, 0.707, 0.0));

        airMidAmt = p.midAir * 0.01f;
        airHighAmt = p.highAir * 0.01f;
        airMid.set  (makeBiquad (FType::Peak, fs, 4500.0, 0.7, 6.0 * airMidAmt));
        airHigh.set (makeBiquad (FType::HighShelf, fs, 11000.0, 0.7, 8.0 * airHighAmt));
        airExHp.set (makeBiquad (FType::HighPass, fs, 8000.0, 0.707, 0.0));

        reverb.set (p.revType, p.revMix * 0.01f, p.revLow, p.revHigh);
        delay.set (p.dlyTime, bpm, p.dlyMix * 0.01f, p.dlyFb * 0.01f, p.dlyLow, p.dlyHigh);
    }

    double fs = 48000.0;
    PitchDetector detector;
    PitchShifter shifter[2];
    Compressor comp;
    DeEsser deess;
    Reverb reverb;
    StereoDelay delay;

    BiquadSt hp, eqL, eqM, eqH, micLs, micP1, micP2, micHs, micBody, micAir;
    BiquadSt magicPres, magicLow, exciterHp, toneLp, toneHp, airMid, airHigh, airExHp;
    DcBlocker dc[2], dcMic[2];

    float centsState = 0, centsTarget = 0;
    float micSat = 0, toneAmt = 0, magicAmt = 0, airMidAmt = 0, airHighAmt = 0;
};

} // namespace vx
