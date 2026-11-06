#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Два тёплых реверба на одной основе: 8-линейный FDN (матрица Хаусхолдера),
// двухполосное затухание (низы живут дольше верхов), входная диффузия,
// модуляция линий и мягкое насыщение.
//
//   Fat  («жирный»): плотный, длинные низы, лёгкая хорус-модуляция против металла,
//                    низкая полка для тела, мягкий срез верха.
//   Tape («плёночный»): wow + flutter (плавающая высота хвоста), насыщение внутри петли,
//                    тёмный срез верхов как у потерь на головке, подрезанные низы.
enum class VerbVoice { Fat = 0, Tape = 1 };

class WarmVerb {
public:
    static constexpr int N = 8;
    static constexpr int kAP = 4;
    static constexpr int kTick = 16;      // период tick() в сэмплах (совпадает с control-rate ядра)

    void prepare(float sampleRate, VerbVoice voice, int channel) {
        sr = sampleRate;
        cfg = &cfgFor(voice);
        const float k = sr / 48000.f;
        const float depthMax = cfg->wowDepth + cfg->flutDepth + cfg->chorusDepth;

        for (int i = 0; i < N; ++i) {
            baseLen[i] = (float)(cfg->lens[i] + channel * (13 + 3 * i)) * k;      // L/R расходятся
            size[i]    = (int)std::ceil(baseLen[i] + depthMax * k) + 6;
            buf[i].assign((size_t)size[i], 0.f);
            w[i] = 0;
            yS[i] = 0.f;
            ph1[i] = std::fmod(0.137f * (float)(i + 1) + 0.25f * (float)channel, 1.f);
            ph2[i] = std::fmod(0.291f * (float)(i + 1) + 0.50f * (float)channel, 1.f);
            ph3[i] = std::fmod(0.413f * (float)(i + 1) + 0.33f * (float)channel, 1.f);
            modCur[i] = modStep[i] = 0.f;
        }
        static const int apBase[kAP] = { 142, 107, 379, 277 };
        for (int j = 0; j < kAP; ++j) {
            apLen[j] = std::max(2, (int)((float)(apBase[j] + channel * 7 + j) * k));
            apBuf[j].assign((size_t)apLen[j], 0.f);
            apPos[j] = 0;
        }
        const float tp = 6.28318530718f;
        lpK = 1.f - std::exp(-tp * cfg->lpHz / sr);
        hpK = 1.f - std::exp(-tp * cfg->hpHz / sr);
        lsK = 1.f - std::exp(-tp * cfg->shelfHz / sr);
        invLoop = cfg->loopDrive > 0.f ? 1.f / cfg->loopDrive : 1.f;
        invOut  = cfg->outDrive  > 0.f ? 1.f / cfg->outDrive  : 1.f;
        lsS = lp1 = lp2 = hpS = 0.f;
        setDecay(2.5f, 0.5f);
        tick();
    }

    void clear() {
        for (int i = 0; i < N; ++i) { std::fill(buf[i].begin(), buf[i].end(), 0.f); yS[i] = 0.f; }
        for (int j = 0; j < kAP; ++j) std::fill(apBuf[j].begin(), apBuf[j].end(), 0.f);
        lsS = lp1 = lp2 = hpS = 0.f;
    }

    // rt60 в секундах (для средних), damping 0..1 (больше = темнее)
    void setDecay(float rt60, float damping) {
        rt60 = std::max(0.1f, rt60);
        const float rtLow  = rt60 * cfg->lowMult;
        const float rtHigh = std::max(0.05f, rt60 * cfg->highMult * (1.f - 0.85f * damping));
        for (int i = 0; i < N; ++i) {
            const float gl = std::pow(10.f, -3.f * baseLen[i] / (rtLow  * sr));
            float       gh = std::pow(10.f, -3.f * baseLen[i] / (rtHigh * sr));
            gh = std::min(gh, gl);
            const float a = (gl - gh) / (gl + gh);       // усиление: gl на постоянке, gh на Найквисте
            aCoef[i] = a;
            bCoef[i] = gl * (1.f - a);
        }
        // нормировка уровня: у долгого реверба энергия выше, выравниваем по замеру
        norm = 1.f / (1.f + normK * std::sqrt(rt60 * cfg->lowMult) * (1.f - 0.5f * damping));
    }

    // раз в kTick сэмплов: медленные LFO для модуляции линий
    void tick() {
        const float dt = (float)kTick / sr, tp = 6.28318530718f, k = sr / 48000.f;
        for (int i = 0; i < N; ++i) {
            ph1[i] += cfg->wowRate * (1.f + 0.07f * (float)i) * dt;   ph1[i] -= std::floor(ph1[i]);
            ph2[i] += cfg->flutRate * (1.f + 0.05f * (float)i) * dt;   ph2[i] -= std::floor(ph2[i]);
            ph3[i] += cfg->chorusRate * (1.f + 0.11f * (float)i) * dt; ph3[i] -= std::floor(ph3[i]);
            const float target = k * (cfg->wowDepth * std::sin(tp * ph1[i])
                                    + cfg->flutDepth * std::sin(tp * ph2[i])
                                    + cfg->chorusDepth * std::sin(tp * ph3[i]));
            modStep[i] = (target - modCur[i]) / (float)kTick;
        }
    }

    float process(float x) {
        // входная диффузия
        float d = x;
        for (int j = 0; j < cfg->nAP; ++j) {
            const float z = apBuf[j][(size_t)apPos[j]];
            const float y = -cfg->apG * d + z;
            apBuf[j][(size_t)apPos[j]] = d + cfg->apG * y;
            if (++apPos[j] >= apLen[j]) apPos[j] = 0;
            d = y;
        }

        float xr[N], v[N], sum = 0.f, acc = 0.f;
        for (int i = 0; i < N; ++i) {
            modCur[i] += modStep[i];
            float rp = (float)w[i] - (baseLen[i] + modCur[i]);
            if (rp < 0.f) rp += (float)size[i];
            int i0 = (int)rp;
            const float fr = rp - (float)i0;
            if (i0 >= size[i]) i0 -= size[i];
            int i1 = i0 + 1; if (i1 >= size[i]) i1 = 0;
            xr[i] = buf[i][(size_t)i0] * (1.f - fr) + buf[i][(size_t)i1] * fr;

            float y = bCoef[i] * xr[i] + aCoef[i] * yS[i];
            yS[i] = y;
            if (cfg->loopDrive > 0.f) y = std::tanh(y * cfg->loopDrive) * invLoop;   // насыщение пленки в петле
            v[i] = y;
            sum += y;
            acc += kOut[i] * xr[i];
        }
        for (int i = 0; i < N; ++i) {
            buf[i][(size_t)w[i]] = cfg->inGain * kIn[i] * d + (v[i] - 0.25f * sum);   // Хаусхолдер 8x8
            if (++w[i] >= size[i]) w[i] = 0;
        }

        float y = acc * (0.35355339f * norm * cfg->outGain);   // 1/sqrt(8), уровень подобран по замеру

        lsS += lsK * (y - lsS);   y += cfg->shelfGain * lsS;      // тело: низкая полка
        lp1 += lpK * (y - lp1);   lp2 += lpK * (lp1 - lp2);   y = lp2;   // тёплый срез верха, 12 дБ/окт
        hpS += hpK * (y - hpS);   y -= hpS;                       // убираем подвал
        if (cfg->outDrive > 0.f) y = std::tanh(y * cfg->outDrive) * invOut;
        return y;
    }

    static constexpr float normK = 0.30f;     // подобрана по замеру (см. тесты)

private:
    struct Cfg {
        int   lens[N];
        float lowMult, highMult;
        float wowDepth, wowRate, flutDepth, flutRate, chorusDepth, chorusRate;   // глубины в сэмплах @48k
        float loopDrive, outDrive;
        float lpHz, hpHz, shelfHz, shelfGain;
        int   nAP;
        float apG, inGain, outGain;
    };

    static const Cfg& cfgFor(VerbVoice v) {
        static const Cfg fat = {
            { 1117, 1361, 1597, 1823, 2087, 2339, 2617, 2879 },
            1.50f, 0.55f,
            0.f, 0.f, 0.f, 0.f, 0.9f, 0.37f,
            0.f, 1.1f,
            8500.f, 35.f, 220.f, 0.45f,
            4, 0.62f, 0.5f, 2.0f };
        static const Cfg tape = {
            { 709, 937, 1171, 1409, 1663, 1949, 2213, 2477 },
            1.15f, 0.28f,
            24.f, 0.50f, 1.2f, 6.3f, 0.f, 0.f,
            1.8f, 1.5f,
            4800.f, 90.f, 160.f, 0.20f,
            2, 0.50f, 0.5f, 2.4f };
        return v == VerbVoice::Fat ? fat : tape;
    }

    static constexpr float kIn [N] = { 1.f,  1.f, -1.f,  1.f, -1.f, -1.f,  1.f, -1.f };
    static constexpr float kOut[N] = { 1.f, -1.f,  1.f,  1.f, -1.f,  1.f, -1.f, -1.f };

    const Cfg* cfg = nullptr;
    float sr = 48000.f;

    std::vector<float> buf[N];
    float baseLen[N] = {}, modCur[N] = {}, modStep[N] = {};
    float ph1[N] = {}, ph2[N] = {}, ph3[N] = {};
    float yS[N] = {}, aCoef[N] = {}, bCoef[N] = {};
    int   size[N] = {}, w[N] = {};

    std::vector<float> apBuf[kAP];
    int apLen[kAP] = {}, apPos[kAP] = {};

    float lpK = 0.f, hpK = 0.f, lsK = 0.f, invLoop = 1.f, invOut = 1.f;
    float lsS = 0.f, lp1 = 0.f, lp2 = 0.f, hpS = 0.f;
    float norm = 1.f;
};
