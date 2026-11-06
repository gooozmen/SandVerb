#pragma once
#include <cmath>
#include <algorithm>

// Стерео-линкованный компрессор прямой связи (peak, мягкое колено).
// Стоит внутри петли, поэтому только снижает уровень: усиления выше 0 дБ не даёт.
struct Compressor {
    void prepare(float sampleRate) {
        atk = std::exp(-1.f / (0.004f * sampleRate));   // атака 4 мс
        rel = std::exp(-1.f / (0.150f * sampleRate));   // релиз 150 мс
        envDb = -120.f;
        grDb  = 0.f;
    }

    void setParams(float thresholdDb, float ratio) {
        thr   = thresholdDb;
        slope = 1.f - 1.f / std::max(1.f, ratio);
    }

    // x[0..n) обрабатывается на месте. Возвращает снижение уровня в дБ (<= 0).
    float process(float* x, int n) {
        float pk = 0.f;
        for (int c = 0; c < n; ++c) pk = std::max(pk, std::fabs(x[c]));

        const float lvl = 20.f * std::log10(std::max(pk, 1e-6f));
        const float k   = lvl > envDb ? atk : rel;
        envDb = k * envDb + (1.f - k) * lvl;

        const float over = envDb - thr;
        float gr;
        if (over <= -kKnee * 0.5f)      gr = 0.f;
        else if (over >= kKnee * 0.5f)  gr = slope * over;
        else { const float t = over + kKnee * 0.5f; gr = slope * t * t / (2.f * kKnee); }

        grDb = -gr;
        const float g = std::pow(10.f, grDb * 0.05f);
        for (int c = 0; c < n; ++c) x[c] *= g;
        return grDb;
    }

    float lastGainReductionDb() const { return grDb; }

private:
    static constexpr float kKnee = 6.f;
    float atk = 0.f, rel = 0.f, envDb = -120.f, grDb = 0.f;
    float thr = -18.f, slope = 0.75f;
};
