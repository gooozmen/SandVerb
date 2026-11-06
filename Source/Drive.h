#pragma once
#include <cmath>
#include <algorithm>
#include "DspUtil.h"

// Педаль подогрева перед крашером. Три типа, плавный переход между ними:
//   Overdrive — усиливается полоса выше ~720 Гц (горб на середине), мягкий tanh.
//   Tube      — клип со смещением рабочей точки: чётные гармоники, тёплый звук.
//   Fuzz      — ФВЧ 100 Гц, большое усиление, почти жёсткий клип.
// Дальше Tone (ФНЧ 1…12 кГц), ФВЧ 30 Гц (он же убирает DC), компенсация громкости и Level.
// Компенсация держит громкость примерно постоянной на всём диапазоне Drive:
// ручка Drive меняет характер, а не уровень. Уровень задаёт Level.
struct Drive {
    static constexpr int kTypes = 3;
    static constexpr int kCompN = 9;                  // узлы таблицы: Drive 0, 5, … 40 дБ

    void prepare(float sampleRate) {
        sr = sampleRate;
        odK = onePoleK(720.f, sr);
        fzK = onePoleK(100.f, sr);
        hpK = onePoleK(30.f,  sr);
        odLp = fzLp = hpLp = toneLp = 0.f;
        for (int i = 0; i < kTypes; ++i) w[i] = wTgt[i] = (i == 0 ? 1.f : 0.f);
        setParams(0, 12.f, 0.5f, 0.f);
        for (int i = 0; i < kTypes; ++i) w[i] = wTgt[i];
    }

    // control-rate
    void setParams(int type, float driveDb, float tone, float levelDb) {
        type = std::clamp(type, 0, kTypes - 1);
        for (int i = 0; i < kTypes; ++i) wTgt[i] = (i == type) ? 1.f : 0.f;
        for (int i = 0; i < kTypes; ++i) w[i] += 0.03f * (wTgt[i] - w[i]);

        g      = std::pow(10.f, driveDb / 20.f);
        toneK  = onePoleK(1000.f * std::pow(12.f, std::clamp(tone, 0.f, 1.f)), sr);
        lvl    = std::pow(10.f, levelDb / 20.f);
        for (int i = 0; i < kTypes; ++i) comp[i] = std::pow(10.f, compDb(i, driveDb) / 20.f);
    }

    float process(float x) {
        // ФВЧ-ветки обновляются всегда, чтобы при переключении типа состояние было свежим
        odLp += odK * (x - odLp);  const float odHp = x - odLp;
        fzLp += fzK * (x - fzLp);  const float fzHp = x - fzLp;

        float y = 0.f;
        if (w[0] > 1e-4f) y += w[0] * comp[0] * std::tanh(x + (g - 1.f) * odHp);
        if (w[1] > 1e-4f) y += w[1] * comp[1] * (std::tanh(g * x + kBias) - kBiasT);
        if (w[2] > 1e-4f) {
            const float u = 4.f * g * fzHp, u2 = u * u;
            y += w[2] * comp[2] * (u / std::sqrt(std::sqrt(1.f + u2 * u2)));
        }

        toneLp += toneK * (y - toneLp);
        hpLp   += hpK * (toneLp - hpLp);
        return (toneLp - hpLp) * lvl;
    }

private:
    static constexpr float kBias  = 0.3f;
    static inline const float kBiasT = std::tanh(0.3f);

    // Поправка громкости в дБ по типу и Drive (замер на розовом шуме −18 dBFS, Tone 70 %)
    static float compDb(int type, float driveDb) {
        static const float tab[kTypes][kCompN] = {
            {  2.4f, -0.1f, -3.7f,  -7.5f, -10.7f, -12.8f, -13.9f, -14.5f, -14.7f },   // Overdrive
            {  3.1f, -1.7f, -6.2f,  -9.9f, -12.5f, -13.9f, -14.6f, -14.9f, -15.1f },   // Tube
            { -8.0f, -11.6f, -13.7f, -14.7f, -15.2f, -15.5f, -15.6f, -15.6f, -15.7f } };  // Fuzz
        const float p = std::clamp(driveDb, 0.f, 40.f) / 5.f;
        const int   i = std::min((int)p, kCompN - 2);
        const float t = p - (float)i;
        return tab[type][i] + t * (tab[type][i + 1] - tab[type][i]);
    }

    float sr = 48000.f;
    float odK = 0.f, fzK = 0.f, hpK = 0.f, toneK = 0.5f;
    float odLp = 0.f, fzLp = 0.f, hpLp = 0.f, toneLp = 0.f;
    float g = 4.f, lvl = 1.f;
    float w[kTypes] = {}, wTgt[kTypes] = {}, comp[kTypes] = { 1.f, 1.f, 1.f };
};
