#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Хранилище прошлых кадров (блок FEEDBACK на схеме).
//
// Хранит ограниченную историю: не больше kMaxFrames кадров длиной D сэмплов.
// Чтение — это сумма отводов на расстоянии 1*D, 2*D, ... N*D назад с весами
// falloff^k (первый кадр звучит в полную силу, каждый следующий тише). Всё, что
// старше N кадров, не читается, поэтому память конечна. N дробное: последний
// отвод включается плавно, поэтому модуляция N не щёлкает.
// Для петли (замыкание на себя и возврат в реверб) нужен множитель loopNorm() =
// 1 / сумма весов: с ним само хранилище сигнал в петле не усиливает.
class FeedbackStore {
public:
    static constexpr int kMaxFrames = 16;

    void prepare(int numChannels, int sizeSamples) {
        nCh  = numChannels;
        size = std::max(8, sizeSamples);
        for (int c = 0; c < 2; ++c) buf[c].assign((size_t)size, 0.f);
        w = 0;
        nTaps = 1;
        wt[0] = 1.f;
    }

    int maxDelay() const { return size - 2; }

    // framesF: 1..kMaxFrames, falloff: 0..1 (вес кадра k = falloff^k),
    // dMax: самая длинная длина кадра, которая сейчас может читаться
    void setShape(float framesF, float falloff, int dMax) {
        const int fit = std::max(1, (size - 2) / std::max(1, dMax));
        const float f = std::clamp(framesF, 1.f, (float)std::min(kMaxFrames, fit));
        nTaps = (int)std::ceil(f);

        float sum = 0.f, a = 1.f;
        for (int k = 0; k < nTaps; ++k) {
            const float wk = std::min(1.f, f - (float)k) * a;
            wt[k] = wk;
            sum  += wk;
            a    *= falloff;
        }
        loopNormV = 1.f / std::max(sum, 1.f);
        framesEff = f;
    }

    float effectiveFrames() const { return framesEff; }
    float loopNorm() const { return loopNormV; }     // <= 1: делает усиление хранилища в петле не больше 1
    int   taps() const { return nTaps; }

    // Взвешенная сумма отводов с шагом D. Читает только то, что записано раньше текущего сэмпла.
    float read(int c, int D) const {
        const float* b = buf[c].data();
        int r = w - D;
        float acc = 0.f;
        for (int k = 0; k < nTaps; ++k) {
            while (r < 0) r += size;                 // защита от выхода за границы
            acc += wt[k] * b[r];
            r -= D;
        }
        return acc;
    }

    void write(int c, float x) { buf[c][(size_t)w] = x; }
    void advance() { if (++w >= size) w = 0; }

private:
    std::vector<float> buf[2];
    int   nCh = 2, size = 8, w = 0, nTaps = 1;
    float wt[kMaxFrames] = {1.f};
    float framesEff = 1.f, loopNormV = 1.f;
};
