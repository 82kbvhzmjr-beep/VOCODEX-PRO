#include "../Source/Dsp.h"
#include <cstdio>
#include <cmath>

static float measureFreq (const std::vector<float>& x, double fs, int from, int to)
{
    int crossings = 0; int first = -1, last = -1;
    for (int i = from + 1; i < to; ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { if (first < 0) first = i; last = i; ++crossings; }
    if (crossings < 2) return 0;
    return (float) (fs * (crossings - 1) / (last - first));
}

int main()
{
    const double fs = 48000;
    vx::Engine e; e.prepare (fs);
    printf ("latencia: %d muestras (%.1f ms)\n", e.getLatencySamples(), 1000.0 * e.getLatencySamples() / fs);

    // 1) Afinacion: A3 +30 cents -> debe ir a 220 Hz
    {
        vx::Params p; p.tuneOn = true; p.tuneSpeed = 10; p.tuneAmount = 100; p.tuneTol = 5; p.revMix = 0; p.dlyMix = 0;
        const float f = 220.0f * std::pow (2.0f, 30.0f / 1200.0f);
        const int N = (int) (fs * 3);
        std::vector<float> in (N), out (N);
        for (int i = 0; i < N; ++i)
            in[i] = 0.3f * (std::sin (2 * vx::kPi * f * i / fs) + 0.3f * std::sin (4 * vx::kPi * f * i / fs));
        out = in;
        for (int s = 0; s < N; s += 256) e.process (out.data() + s, nullptr, std::min (256, N - s), 120.0, p);
        printf ("entrada %.2f Hz -> salida %.2f Hz (objetivo 220.00), nota detectada midi=%.2f\n",
                measureFreq (in, fs, N / 2, N), measureFreq (out, fs, N / 2, N), e.detectedMidi.load());
    }
    // 2) Escala: F# mayor? Key C Major, entrada 277.18 (C#4) -> mas cercana C4 o D4
    {
        e.reset();
        vx::Params p; p.tuneOn = true; p.key = 0; p.scale = 1; p.tuneSpeed = 5; p.tuneAmount = 100; p.tuneTol = 5;
        const float f = 277.18f;
        const int N = (int) (fs * 3);
        std::vector<float> out (N);
        for (int i = 0; i < N; ++i) out[i] = 0.3f * (std::sin (2 * vx::kPi * f * i / fs) + 0.3f * std::sin (4 * vx::kPi * f * i / fs));
        for (int s = 0; s < N; s += 512) e.process (out.data() + s, nullptr, std::min (512, N - s), 120.0, p);
        printf ("C#4 en C mayor -> %.2f Hz (C4=261.63 / D4=293.66)\n", measureFreq (out, fs, N / 2, N));
    }
    // 3) Cadena completa estereo con todo activado: sin NaN, niveles razonables
    {
        e.reset();
        vx::Params p; p.tuneOn = true; p.comp = 58; p.compMode = 2; p.deess = 45; p.toneType = 2; p.toneInt = 40; p.magic = 50;
        p.midAir = 40; p.highAir = 50; p.revMix = 30; p.revType = 2; p.dlyMix = 25; p.dlyFb = 60; p.lowCut = 80; p.eqMid = -2.5f;
        for (int mic = 0; mic < 8; ++mic)
        {
            p.mic = mic;
            const int N = (int) (fs * 2);
            std::vector<float> L (N), R (N);
            for (int i = 0; i < N; ++i) { float v = 0.25f * std::sin (2 * vx::kPi * 200.f * i / fs); L[i] = v; R[i] = v; }
            double sumIn = 0, sumOut = 0; bool bad = false; float peak = 0;
            for (int s = 0; s < N; s += 128) e.process (L.data() + s, R.data() + s, 128, 140.0, p);
            for (int i = N / 2; i < N; ++i)
            { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true; sumOut += L[i] * L[i]; peak = std::max (peak, std::fabs (L[i])); sumIn += 0.25 * 0.25 * 0.5; }
            printf ("mic %d: RMS in %.3f out %.3f  pico %.3f  %s\n", mic, std::sqrt (sumIn / (N / 2)), std::sqrt (sumOut / (N / 2)), peak, bad ? "NaN!" : "ok");
        }
    }
    // 4) Reverb sola: cola y nivel
    {
        e.reset();
        vx::Params p; p.revMix = 30; p.revType = 0; p.mic = 2;
        const int N = (int) (fs * 3);
        std::vector<float> L (N, 0.f), R (N, 0.f);
        for (int i = 0; i < (int) (fs * 0.5); ++i) { L[i] = 0.25f * std::sin (2 * vx::kPi * 300.f * i / fs); R[i] = L[i]; }
        for (int s = 0; s < N; s += 256) e.process (L.data() + s, R.data() + s, std::min (256, N - s), 120.0, p);
        auto rms = [&] (int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += L[i] * L[i]; return std::sqrt (s / (b - a)); };
        printf ("reverb: rms senal %.3f, rms cola 0.6-0.8s %.4f, 2.0-2.2s %.4f\n", rms (1000, 20000), rms ((int) (fs * 0.6), (int) (fs * 0.8)), rms ((int) (fs * 2.0), (int) (fs * 2.2)));
    }
    return 0;
}
