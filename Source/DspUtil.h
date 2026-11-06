#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

// Дешёвый ГПСЧ (xorshift32), безопасен для аудиопотока
struct XorShift {
    uint32_t s = 2463534242u;
    explicit XorShift(uint32_t seed = 2463534242u) : s(seed ? seed : 1u) {}
    inline float next01() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float)(s & 0xFFFFFF) / 16777216.f;
    }
};

// Коэффициент однополюсного ФНЧ для частоты fc
inline float onePoleK(float fc, float sr) {
    return 1.f - std::exp(-6.28318530718f * std::min(fc, 0.45f * sr) / sr);
}

// Biquad (TDF-II), коэффициенты по RBJ. Обновляются на control-rate.
struct Biquad {
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f, z1 = 0.f, z2 = 0.f;

    void reset() { z1 = z2 = 0.f; }
    inline float process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    // Баттерворт (Q = 0.707): без подъёма
    void lowpass(float fc, float sr)  { pass(fc, sr, true); }
    void highpass(float fc, float sr) { pass(fc, sr, false); }

    // Полки по низам и верхам, S = 1. Полки с gainDb и -gainDb на одной частоте взаимно обратны.
    void lowShelf(float fc, float sr, float gainDb) {
        const float A = std::pow(10.f, gainDb / 40.f);
        const float w = 6.28318530718f * fc / sr, cw = std::cos(w), sw = std::sin(w);
        const float sq = 2.f * std::sqrt(A) * sw * 0.70710678f;
        const float a0 = (A + 1.f) + (A - 1.f) * cw + sq;
        b0 =  A * ((A + 1.f) - (A - 1.f) * cw + sq) / a0;
        b1 =  2.f * A * ((A - 1.f) - (A + 1.f) * cw) / a0;
        b2 =  A * ((A + 1.f) - (A - 1.f) * cw - sq) / a0;
        a1 = -2.f * ((A - 1.f) + (A + 1.f) * cw) / a0;
        a2 = ((A + 1.f) + (A - 1.f) * cw - sq) / a0;
    }
    void highShelf(float fc, float sr, float gainDb) {
        const float A = std::pow(10.f, gainDb / 40.f);
        const float w = 6.28318530718f * fc / sr, cw = std::cos(w), sw = std::sin(w);
        const float sq = 2.f * std::sqrt(A) * sw * 0.70710678f;
        const float a0 = (A + 1.f) - (A - 1.f) * cw + sq;
        b0 =  A * ((A + 1.f) + (A - 1.f) * cw + sq) / a0;
        b1 = -2.f * A * ((A - 1.f) + (A + 1.f) * cw) / a0;
        b2 =  A * ((A + 1.f) + (A - 1.f) * cw - sq) / a0;
        a1 =  2.f * ((A - 1.f) - (A + 1.f) * cw) / a0;
        a2 = ((A + 1.f) - (A - 1.f) * cw - sq) / a0;
    }

private:
    void pass(float fc, float sr, bool low) {
        fc = std::clamp(fc, 10.f, 0.45f * sr);
        const float w = 6.28318530718f * fc / sr, cw = std::cos(w);
        const float alpha = std::sin(w) * 0.70710678f, a0 = 1.f + alpha;
        const float k = low ? (1.f - cw) : (1.f + cw);
        b0 = 0.5f * k / a0;
        b1 = (low ? k : -k) / a0;
        b2 = b0;
        a1 = -2.f * cw / a0;
        a2 = (1.f - alpha) / a0;
    }
};
