#pragma once
#include <atomic>
#include <cmath>
#include <algorithm>
#include "Sand.h"
#include "WarmVerb.h"
#include "Compressor.h"
#include "FeedbackStore.h"

// Все параметры ядра. Значения приходят «сырыми» раз в блок,
// сглаживание делает само ядро. Включатели — 0 или 1.
struct LoopParams {
    int   delaySamples = 12000;                 // длина одного кадра
    float frames = 4.f;                         // глубина хранилища (кадров), до модуляции
    float falloff = 0.65f;                      // во сколько раз каждый более старый кадр тише
    float send = 0.6f;                          // SEND: возврат COMP2 -> FEEDBACK (<= 0.95)
    float retOut = 0.55f;                       // COMP2 -> OUT (Fb Out, не зависит от Mix)
    float verbOn = 1.f, comp1On = 1.f, fbOn = 1.f, sandOn = 1.f, comp2On = 1.f, sendOn = 1.f;
    float sandLevel = 0.7f;                     // уровень ветки SAND в сумме перед COMP2
    SandParams sand;                            // DRIVE и CRUSHER (у каждого свой включатель)
    int   verbType = 0;                         // 0 = Fat, 1 = Tape
    float rt60 = 2.5f, damping = 0.5f, mix = 0.4f;                      // Reverb / микс
    float c1Thr = -18.f, c1Ratio = 4.f;                           // COMP1: Reverb -> Feedback и Sand
    float c2Thr = -12.f, c2Ratio = 4.f;                           // COMP2: Feedback + Sand -> OUT и Send
    float lfo1Rate = 0.25f, lfo1Amt = 0.f;                        // модули, цель — глубина хранилища
    float lfo2Rate = 0.7f,  lfo2Amt = 0.f;
    float rndRate  = 2.f,   rndAmt  = 0.f;
    float envAmt   = 0.f;
};

// Граф (как на схеме пользователя):
//
//   IN ─┬──────────────────────────────────────────────────────► dry ──┐
//       │                                                               │
//       └─► REVERB ─┬─────────────────────────────────────────► (Mix) ──┤
//                   │                                                   │
//                   └─► COMP1 ─┬─► FEEDBACK (≤ N кадров, модули) ─┐     ├─► OUT
//                              │          ▲                       ▼     │
//                              └─► SAND (DRIVE → CRUSHER) ──────► COMP2 ┤ (Fb Out)
//                                         │                       │
//                                         └────────── SEND ◄──────┘
//
// SAND стоит параллельно FEEDBACK и красит только то, что идёт от реверба. Повторы через SAND
// заново не проходят, поэтому его усиление (Drive, полки) на устойчивость петли не влияет.
//
// Включатели: у каждого блока свой, переходы сглажены (~30 мс), без щелчков.
//   COMP1, COMP2, DRIVE, CRUSHER — выключенный блок становится проводом;
//   REVERB — для ветки COMP1 заменяется сухим входом, в OUT его вклад исчезает;
//   FEEDBACK, SAND — ветка глушится; SEND — возврата в хранилище нет (только повторы без рециркуляции).
//   Хранилище пишется всегда: при включении Feedback в нём уже есть звук.
//
// Защита от «заведения»:
//   1. память конечна: не больше 16 кадров, сумма весов отводов = 1;
//   2. вся петля идёт через COMP2 и tanh, запись в хранилище тоже через tanh;
//   3. Send <= 0.95, с loopNorm усиление петли не выше Send;
//   4. когда на входе тишина дольше N кадров, петля плавно закрывается (killGain);
//   5. SAND вне петли, а его шум идёт за огибающей и на тишине равен нулю.
class SandVerbCore {
public:
    static constexpr int   kMaxCh = 2;
    static constexpr int   kCtrl = WarmVerb::kTick;   // control-rate: раз в 16 сэмплов
    static constexpr float kMaxSpanSec = 32.f;        // полный объём хранилища
    static constexpr float kModSpan = 8.f;            // насколько модули двигают глубину (кадров)
    static_assert(Sand::kCtrl == kCtrl, "Sand::setParams рассчитан на тот же control-rate");

    // Уровни в разных точках графа (для подсветки проводов в GUI)
    enum Lvl { L_In, L_Verb, L_Comp1, L_Fb, L_Sand, L_Comp2, L_Send, L_Out, kNumLvl };

    void prepare(double sampleRate, int numChannels) {
        sr  = (float)sampleRate;
        nCh = std::clamp(numChannels, 1, kMaxCh);

        store.prepare(nCh, (int)std::ceil(sr * kMaxSpanSec) + 4);
        for (int c = 0; c < kMaxCh; ++c) {
            sand[c].prepare(sr, 1234u + 777u * (uint32_t)c);
            verb[0][c].prepare(sr, VerbVoice::Fat,  c);
            verb[1][c].prepare(sr, VerbVoice::Tape, c);
        }
        comp1.prepare(sr);
        comp2.prepare(sr);

        xfadeLen  = std::max(1, (int)(0.04f * sr));
        xfading   = false;
        vxfadeLen = std::max(1, (int)(0.12f * sr));
        vxfading  = false;
        smoothK  = 1.f - std::exp(-(float)kCtrl / (0.03f * sr));
        envA = std::exp(-1.f / (0.004f * sr));
        envR = std::exp(-1.f / (0.100f * sr));
        env  = 0.f;
        lfo1Ph = lfo2Ph = rndPh = 0.f;
        rndNow = rndTarget = 0.f;
        rng = XorShift(98765u);
        snap = true;
        ctrlCnt = 0;
        quietSamples = 0;
        killGain = 1.f;
        verbRun = comp1Run = comp2Run = sandRun = true;
        for (int i = 0; i < kNumLvl; ++i) { lvlAcc[i] = 0.f; lvlHold[i] = 0.f; uiLvl[i].store(0.f); }
    }

    void setTarget(const LoopParams& p) { tgt = p; }
    int  numChannels() const { return nCh; }

    // Обработка на месте. io[c] — указатель на канал c, n сэмплов.
    void process(float* const* io, int n) {
        if (snap) {
            cur = tgt;
            framesNow = std::clamp(cur.frames, 1.f, (float)FeedbackStore::kMaxFrames);
            requestDelay(tgt.delaySamples);
            vActive = vFrom = tgt.verbType != 0 ? 1 : 0;
            for (int c = 0; c < nCh; ++c) { sand[c].setParams(cur.sand); sand[c].setParams(cur.sand); }   // дважды: без рампы от умолчаний
            snap = false;
            ctrlCnt = 0;
        }

        for (int i = 0; i < n; ++i)
        {
            if (ctrlCnt <= 0) { tick(); ctrlCnt = kCtrl; }
            --ctrlCnt;

            // --- вход и его огибающая (для модуля ENV и автозакрытия петли) ---
            float dry[kMaxCh] = {0.f, 0.f};
            float pk = 0.f;
            for (int c = 0; c < nCh; ++c) { dry[c] = io[c][i]; pk = std::max(pk, std::fabs(dry[c])); }
            {
                const float k = pk > env ? envA : envR;
                env = k * env + (1.f - k) * pk;
            }
            lvlAcc[L_In] = std::max(lvlAcc[L_In], pk);

            // --- кроссфейд между двумя длинами кадра ---
            float gOld = 1.f, gNew = 0.f;
            if (xfading) {
                const float t = (float)xfadePos / (float)xfadeLen;
                gOld = std::cos(t * 1.57079632679f);
                gNew = std::sin(t * 1.57079632679f);
            }

            // --- кроссфейд между ревербами ---
            float vOld = 1.f, vNew = 0.f;
            if (vxfading) {
                const float t = (float)vxPos / (float)vxfadeLen;
                vOld = std::cos(t * 1.57079632679f);
                vNew = std::sin(t * 1.57079632679f);
            }

            // 1) REVERB параллельно сухому входу; выключенный — для ветки COMP1 заменяется сухим входом
            float verbY[kMaxCh] = {0.f, 0.f};
            if (verbRun)
                for (int c = 0; c < nCh; ++c) {
                    if (vxfading) verbY[c] = vOld * verb[vFrom][c].process(dry[c]) + vNew * verb[vActive][c].process(dry[c]);
                    else          verbY[c] = verb[vActive][c].process(dry[c]);
                }
            const float gV = cur.verbOn;
            float c1[kMaxCh] = {0.f, 0.f};
            float vk = 0.f, c1k = 0.f;
            for (int c = 0; c < nCh; ++c) {
                c1[c] = gV * verbY[c] + (1.f - gV) * dry[c];
                vk  = std::max(vk,  std::fabs(gV * verbY[c]));
            }

            // 2) COMP1
            if (comp1Run) {
                float up[kMaxCh] = {c1[0], c1[1]};
                gr1 = comp1.process(up, nCh);
                for (int c = 0; c < nCh; ++c) c1[c] += cur.comp1On * (up[c] - c1[c]);
            } else gr1 = 0.f;
            for (int c = 0; c < nCh; ++c) c1k = std::max(c1k, std::fabs(c1[c]));

            // 3) FEEDBACK (прошлые кадры) и SAND (DRIVE -> CRUSHER) параллельно, сумма в COMP2
            float sum[kMaxCh] = {0.f, 0.f};
            const float gSand = cur.sandOn * cur.sandLevel;
            float tk = 0.f, sk = 0.f;
            for (int c = 0; c < nCh; ++c) {
                const float tap = xfading ? gOld * store.read(c, delayCur) + gNew * store.read(c, delayNew)
                                          : store.read(c, delayCur);
                const float s = sandRun ? sand[c].process(c1[c]) : 0.f;
                sum[c] = cur.fbOn * tap + gSand * s;
                tk = std::max(tk, std::fabs(cur.fbOn * tap));
                sk = std::max(sk, std::fabs(gSand * s));
            }

            // 4) COMP2 и жёсткий предел амплитуды
            float c2[kMaxCh] = {sum[0], sum[1]};
            if (comp2Run) {
                float up[kMaxCh] = {sum[0], sum[1]};
                gr2 = comp2.process(up, nCh);
                for (int c = 0; c < nCh; ++c) c2[c] += cur.comp2On * (up[c] - c2[c]);
            } else gr2 = 0.f;
            float c2k = 0.f;
            for (int c = 0; c < nCh; ++c) { c2[c] = std::tanh(c2[c]); c2k = std::max(c2k, std::fabs(c2[c])); }

            // 5) в хранилище: выход COMP1 плюс возврат SEND из COMP2
            const float fbSend = cur.send * cur.sendOn * killGain * loopNorm;
            for (int c = 0; c < nCh; ++c)
                store.write(c, std::tanh(c1[c] + fbSend * c2[c]));
            store.advance();

            // 6) OUT: сухой + реверб по Mix + Fb Out (COMP2 мимо Mix)
            const float vm = cur.mix * gV;
            float ok = 0.f;
            for (int c = 0; c < nCh; ++c) {
                io[c][i] = (1.f - vm) * dry[c] + vm * verbY[c] + cur.retOut * c2[c];
                ok = std::max(ok, std::fabs(io[c][i]));
            }

            lvlAcc[L_Verb]  = std::max(lvlAcc[L_Verb],  vk);
            lvlAcc[L_Comp1] = std::max(lvlAcc[L_Comp1], c1k);
            lvlAcc[L_Fb]    = std::max(lvlAcc[L_Fb],    tk);
            lvlAcc[L_Sand]  = std::max(lvlAcc[L_Sand],  sk);
            lvlAcc[L_Comp2] = std::max(lvlAcc[L_Comp2], c2k);
            lvlAcc[L_Send]  = std::max(lvlAcc[L_Send],  fbSend * c2k);
            lvlAcc[L_Out]   = std::max(lvlAcc[L_Out],   ok);

            if (xfading && ++xfadePos >= xfadeLen) { delayCur = delayNew; xfading = false; }
            if (vxfading && ++vxPos >= vxfadeLen) {
                vxfading = false;
                for (int c = 0; c < nCh; ++c) verb[vFrom][c].clear();      // старый реверб больше не нужен
                vFrom = vActive;
            }
        }
    }

    // для GUI (читаются из другого потока)
    std::atomic<float> uiFrames{1.f}, uiDelayMs{0.f}, uiGr1{0.f}, uiGr2{0.f};
    std::atomic<float> uiLvl[kNumLvl];          // 0..1, пик с плавным спадом (−60…0 дБ)

    SandVerbCore() { for (auto& l : uiLvl) l.store(0.f); }

private:
    void requestDelay(int target) {
        target = std::clamp(target, 1, store.maxDelay());
        if (snap) { delayCur = delayNew = target; xfading = false; return; }
        if (!xfading && std::abs(target - delayCur) > 4) {   // гистерезис против дрожания темпа
            delayNew = target;
            xfadePos = 0;
            xfading  = true;
        }
    }

    void requestVerb(int type) {
        type = type != 0 ? 1 : 0;
        if (vxfading || type == vActive) return;
        vFrom    = vActive;
        vActive  = type;
        for (int c = 0; c < nCh; ++c) verb[vActive][c].clear();       // новый стартует с чистого листа
        vxPos    = 0;
        vxfading = true;
    }

    void tick()
    {
        // сглаживание параметров (~30 мс)
        const float k = smoothK;
        auto g  = [k](float& c, float t) { c += k * (t - c); };
        auto gs = [k](float& c, float t) { c += k * (t - c); if (std::fabs(t - c) < 0.002f) c = t; };   // включатели: доходят точно
        g(cur.frames, tgt.frames);     g(cur.falloff, tgt.falloff);
        g(cur.send, tgt.send);         g(cur.retOut, tgt.retOut);
        g(cur.sandLevel, tgt.sandLevel);
        gs(cur.verbOn, tgt.verbOn);    gs(cur.comp1On, tgt.comp1On); gs(cur.comp2On, tgt.comp2On);
        gs(cur.fbOn, tgt.fbOn);        gs(cur.sandOn, tgt.sandOn);   gs(cur.sendOn, tgt.sendOn);
        g(cur.rt60, tgt.rt60);         g(cur.damping, tgt.damping);  g(cur.mix, tgt.mix);
        g(cur.c1Thr, tgt.c1Thr);       g(cur.c1Ratio, tgt.c1Ratio);
        g(cur.c2Thr, tgt.c2Thr);       g(cur.c2Ratio, tgt.c2Ratio);
        g(cur.lfo1Amt, tgt.lfo1Amt);   g(cur.lfo2Amt, tgt.lfo2Amt);
        g(cur.rndAmt, tgt.rndAmt);     g(cur.envAmt, tgt.envAmt);

        SandParams& cs = cur.sand; const SandParams& ts = tgt.sand;
        gs(cs.driveOn, ts.driveOn);    gs(cs.crushOn, ts.crushOn);
        g(cs.drive, ts.drive);         g(cs.tone, ts.tone);          g(cs.drvLevel, ts.drvLevel);
        g(cs.bits, ts.bits);           g(cs.compand, ts.compand);    g(cs.noise, ts.noise);
        g(cs.low, ts.low);             g(cs.high, ts.high);          g(cs.cmix, ts.cmix);
        cs.rate = std::exp(std::log(std::max(cs.rate, 1.f)) + k * (std::log(std::max(ts.rate, 1.f)) - std::log(std::max(cs.rate, 1.f))));   // Rate — в логарифмической шкале
        cs.drvType = ts.drvType;

        // какие блоки нужно считать (выключенный и полностью затихший — пропускаем)
        const bool vr = cur.verbOn > 1e-3f || tgt.verbOn > 0.5f;
        if (verbRun && !vr)
            for (int v = 0; v < 2; ++v) for (int c = 0; c < nCh; ++c) verb[v][c].clear();    // чтобы не всплыл старый хвост
        verbRun  = vr;
        comp1Run = cur.comp1On > 1e-3f || tgt.comp1On > 0.5f;
        comp2Run = cur.comp2On > 1e-3f || tgt.comp2On > 0.5f;
        sandRun  = cur.sandOn  > 1e-3f || tgt.sandOn  > 0.5f;

        // модули -> глубина хранилища
        const float dt = (float)kCtrl / sr;
        lfo1Ph = wrap01(lfo1Ph + tgt.lfo1Rate * dt);
        lfo2Ph = wrap01(lfo2Ph + tgt.lfo2Rate * dt);
        rndPh += tgt.rndRate * dt;
        if (rndPh >= 1.f) { rndPh = wrap01(rndPh); rndTarget = rng.next01() * 2.f - 1.f; }
        rndNow += 0.02f * (rndTarget - rndNow);

        const float lfo1 = std::sin(6.28318530718f * lfo1Ph);
        const float lfo2 = 4.f * std::fabs(lfo2Ph - 0.5f) - 1.f;          // треугольник
        const float envDb = 20.f * std::log10(std::max(env, 1e-6f));
        const float envN  = std::clamp((envDb + 48.f) / 48.f, 0.f, 1.f);  // -48..0 дБ -> 0..1

        const float m = cur.lfo1Amt * lfo1 + cur.lfo2Amt * lfo2 + cur.rndAmt * rndNow + cur.envAmt * envN;
        const float ft = std::clamp(cur.frames + kModSpan * m, 1.f, (float)FeedbackStore::kMaxFrames);
        framesNow += 0.05f * (ft - framesNow);

        // длина кадра (может запустить кроссфейд) и выбор реверба
        requestDelay(tgt.delaySamples);
        requestVerb(tgt.verbType);
        const int dMax = xfading ? std::max(delayCur, delayNew) : delayCur;
        store.setShape(framesNow, cur.falloff, dMax);
        loopNorm = store.loopNorm();

        // автозакрытие петли: N кадров после последнего звука на входе она ещё живёт, затем затухает
        quietSamples = (env > 0.001f) ? 0 : std::min(quietSamples + kCtrl, 1 << 30);   // 0.001 = -60 dBFS
        const float holdS = framesNow * (float)delayCur;
        const float fadeS = std::max(0.25f * sr, (float)delayCur);
        const float killT = std::clamp(1.f - ((float)quietSamples - holdS) / fadeS, 0.f, 1.f);
        killGain += 0.2f * (killT - killGain);

        for (int c = 0; c < nCh; ++c) {
            if (sandRun) sand[c].setParams(cur.sand);
            if (verbRun) {
                verb[vActive][c].setDecay(cur.rt60, cur.damping);
                verb[vActive][c].tick();
                if (vxfading) { verb[vFrom][c].setDecay(cur.rt60, cur.damping); verb[vFrom][c].tick(); }
            }
        }
        comp1.setParams(cur.c1Thr, cur.c1Ratio);
        comp2.setParams(cur.c2Thr, cur.c2Ratio);

        uiFrames.store(store.effectiveFrames(), std::memory_order_relaxed);
        uiDelayMs.store((float)(xfading ? delayNew : delayCur) * 1000.f / sr, std::memory_order_relaxed);
        uiGr1.store(gr1, std::memory_order_relaxed);
        uiGr2.store(gr2, std::memory_order_relaxed);
        for (int i = 0; i < kNumLvl; ++i) {
            const float n = std::clamp((20.f * std::log10(std::max(lvlAcc[i], 1e-6f)) + 60.f) / 60.f, 0.f, 1.f);
            lvlHold[i] = std::max(n, lvlHold[i] - 0.004f);        // пик со спадом ~80 мс
            lvlAcc[i] = 0.f;
            uiLvl[i].store(lvlHold[i], std::memory_order_relaxed);
        }
    }

    static float wrap01(float x) { return x - std::floor(x); }

    float sr = 48000.f;
    int   nCh = 2;

    LoopParams cur, tgt;
    bool  snap = true;
    int   ctrlCnt = 0;
    float smoothK = 0.01f;

    FeedbackStore store;
    Sand          sand[kMaxCh];
    WarmVerb      verb[2][kMaxCh];          // [0] = Fat, [1] = Tape
    Compressor    comp1, comp2;
    float         gr1 = 0.f, gr2 = 0.f;
    bool          verbRun = true, comp1Run = true, comp2Run = true, sandRun = true;
    float         lvlAcc[kNumLvl] = {}, lvlHold[kNumLvl] = {};

    // длина кадра и кроссфейд
    int  delayCur = 1, delayNew = 1, xfadePos = 0, xfadeLen = 1;
    bool xfading = false;

    // выбор реверба и кроссфейд между ними
    int  vActive = 0, vFrom = 0, vxPos = 0, vxfadeLen = 1;
    bool vxfading = false;

    // модули
    XorShift rng;
    float lfo1Ph = 0.f, lfo2Ph = 0.f, rndPh = 0.f, rndNow = 0.f, rndTarget = 0.f;
    float env = 0.f, envA = 0.f, envR = 0.f;
    float framesNow = 4.f;

    // автозакрытие петли после N кадров тишины на входе
    int   quietSamples = 0;
    float killGain = 1.f;
    float loopNorm = 1.f;     // 1 / сумма весов отводов: удерживает усиление петли <= Send
};
