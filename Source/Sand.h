#pragma once
#include "DspUtil.h"
#include "Drive.h"
#include "Crusher.h"

// Параметры SAND (приходят из ядра уже сглаженными)
struct SandParams {
    float driveOn = 1.f, crushOn = 1.f;                         // 0..1, сглаживаются в ядре
    int   drvType = 0;                                          // 0 Overdrive, 1 Tube, 2 Fuzz
    float drive = 12.f, tone = 0.5f, drvLevel = 0.f;            // дБ, 0..1, дБ
    float bits = 8.f, rate = 11025.f, compand = 0.3f, noise = 0.2f;
    float low = 0.f, high = 0.f, cmix = 1.f;
};

// SAND = DRIVE -> CRUSHER. У каждого блока свой вкл/выкл: выключенный блок становится проводом,
// переход линейный на длину control-периода, поэтому без щелчков.
struct Sand {
    static constexpr int kCtrl = 16;                            // setParams() раз в kCtrl сэмплов

    void prepare(float sampleRate, uint32_t seed) {
        drive.prepare(sampleRate);
        crush.prepare(sampleRate, seed);
        dG = cG = 1.f; dStep = cStep = 0.f;
        dTgt = cTgt = 1.f;
    }

    void setParams(const SandParams& p) {
        dG = dTgt; cG = cTgt;                                   // дошли до прошлой цели
        dTgt = p.driveOn; cTgt = p.crushOn;
        dStep = (dTgt - dG) / (float)kCtrl;
        cStep = (cTgt - cG) / (float)kCtrl;
        drive.setParams(p.drvType, p.drive, p.tone, p.drvLevel);
        crush.setParams(p.bits, p.rate, p.compand, p.noise, p.low, p.high, p.cmix);
    }

    float process(float x) {
        dG += dStep; cG += cStep;
        float y = x;
        if (dG > 1e-4f || dTgt > 0.f) y += dG * (drive.process(y) - y);
        if (cG > 1e-4f || cTgt > 0.f) y += cG * (crush.process(y) - y);
        return y;
    }

    bool active() const { return dTgt > 0.f || cTgt > 0.f || dG > 1e-4f || cG > 1e-4f; }

private:
    Drive   drive;
    Crusher crush;
    float dG = 1.f, cG = 1.f, dStep = 0.f, cStep = 0.f, dTgt = 1.f, cTgt = 1.f;
};
