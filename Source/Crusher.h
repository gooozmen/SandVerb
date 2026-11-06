#pragma once
#include <cmath>
#include <algorithm>
#include "DspUtil.h"

// Биткрашер (по мотивам TAL-Bitcrusher, алгоритмы свои).
//
//   компандер (μ-law, сжатие) -> выборка (Rate) -> квантование (Bits)
//   -> расширение (неполное: тихое поднимается) -> шум -> полки -> Mix
//
//   Bits      1…16, дробные биты допустимы.
//   Rate      целевая частота дискретизации; выборка с дробной фазой, при Rate >= sr не прореживает.
//   Companding тихие части поднимаются, в них больше шума квантования и «искр».
//   Noise     шум, умноженный на огибающую входа: на тишине его нет.
//   Low/High  полки 200 Гц и 4 кГц.
struct Crusher {
    void prepare(float sampleRate, uint32_t seed) {
        sr  = sampleRate;
        rng = XorShift(seed);
        envK = onePoleK(1.f / (6.28318530718f * 0.005f), sr);     // ~5 мс
        env = 0.f;
        ph = 1.f; hold = 0.f;
        lo.reset(); hi.reset();
        setParams(8.f, 11025.f, 0.3f, 0.2f, 0.f, 0.f, 1.f);
    }

    // control-rate
    void setParams(float bits, float rateHz, float compand, float noise, float lowDb, float highDb, float mixAmt) {
        levels = std::exp2(std::clamp(bits, 1.f, 16.f) - 1.f);
        inv    = 1.f / levels;
        ratio  = std::clamp(rateHz / sr, 1e-4f, 1.f);

        mu  = 254.f * std::clamp(compand, 0.f, 1.f);
        muE = mu * (1.f - 0.5f * std::clamp(compand, 0.f, 1.f));
        lMu  = std::log1p(mu);
        lMuE = std::log1p(muE);

        nz  = 0.25f * std::clamp(noise, 0.f, 1.f);              // до −12 дБ относительно сигнала
        if (std::fabs(lowDb)  > 0.05f) lo.lowShelf (200.f,  sr, lowDb);  else lo = Biquad();
        if (std::fabs(highDb) > 0.05f) hi.highShelf(4000.f, sr, highDb); else hi = Biquad();
        mixW = std::clamp(mixAmt, 0.f, 1.f);
    }

    float process(float x) {
        const float dry = x;
        env += envK * (std::fabs(x) - env);

        // часть за пределами ±1 идёт мимо компандера и квантователя
        const float c  = std::clamp(x, -1.f, 1.f);
        const float ex = x - c;
        float a = std::fabs(c);
        const float s = c < 0.f ? -1.f : 1.f;

        if (mu > 1e-3f) a = std::log1p(mu * a) / lMu;           // сжатие

        ph += ratio;
        if (ph >= 1.f) { ph -= std::floor(ph); hold = s * a; }
        float q = std::round(hold * levels) * inv;              // квантование (знак внутри hold)
        a = std::fabs(q);
        const float sq = q < 0.f ? -1.f : 1.f;

        if (mu > 1e-3f) a = std::expm1(std::min(a, 1.f) * lMuE) / muE;   // неполное расширение
        float y = sq * a + ex;

        if (nz > 0.f) y += nz * env * (rng.next01() * 2.f - 1.f) * 1.7320508f;
        y = hi.process(lo.process(y));
        return mixW * y + (1.f - mixW) * dry;          // при Mix = 100 % ровно y, без шума округления
    }

private:
    float sr = 48000.f;
    XorShift rng;
    float levels = 128.f, inv = 1.f / 128.f, ratio = 1.f;
    float mu = 0.f, muE = 0.f, lMu = 1.f, lMuE = 1.f;
    float nz = 0.f, mixW = 1.f;
    float env = 0.f, envK = 0.01f, ph = 1.f, hold = 0.f;
    Biquad lo, hi;
};
