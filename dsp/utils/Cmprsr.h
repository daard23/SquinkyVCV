#pragma once

#include "CompCurves.h"
// #include "simd/functions.hpp"
#include <assert.h>
#include <stdint.h>

#include <atomic>

#include "MultiLag2.h"
#include "SqMath.h"

class Cmprsr {
public:
    Cmprsr();
    enum class Ratios {
        HardLimit,
        _2_1_soft,
        _2_1_hard,
        _4_1_soft,
        _4_1_hard,
        _8_1_soft,
        _8_1_hard,
        _20_1_soft,
        _20_1_hard,
        NUM_RATIOS
    };

    float_4 step(float_4);
    float_4 stepPoly(float_4);
    void setTimes(float attackMs, float releaseMs, float sampleTime, bool enableDistortionReduction);
    void setThreshold(float th);
    void setCurve(Ratios);

    void setTimesPoly(float_4 attackMs, float_4 releaseMs, float sampleTime, float_4 enableDistortionReduction);
    void setThresholdPoly(float_4 th);
    void setCurvePoly(const Ratios*);

    void setNumChannels(int);

    const MultiLag2& _lag() const;
    static const std::vector<std::string>& ratios();
    static const std::vector<std::string>& ratiosLong();
    float_4 getGain() const;

    static bool wasInit() {
        return !!ratioCurves[0];
    }

private:
    MultiLag2 lag;
    MultiLPF2 attackFilter;

    // TODO: get rid of the non-poly version
    bool reduceDistortion = false;
    float_4 reduceDistortionPoly = { 0 };


    float_4 threshold = 5;
    float_4 invThreshold = 1.f / 5.f;

    int ratioIndex[4] = { 0 };
    Ratios ratio[4] = { Ratios::HardLimit, Ratios::HardLimit, Ratios::HardLimit, Ratios::HardLimit };
    //int ratioIndex = 0;
   // Ratios ratio = Ratios::HardLimit;
    int maxChannel = 3;

#ifdef _SQATOMIC
    std::atomic<float_4> gain_;
#else
    float_4 gain_;
#endif

    static CompCurves::LookupPtr ratioCurves[int(Ratios::NUM_RATIOS)];

    using processFunction = float_4 (Cmprsr::*)(float_4 input);
    processFunction procFun = &Cmprsr::stepGeneric;
    void updateProcFun();

    float_4 stepGeneric(float_4);
    float_4 step1NoDistComp(float_4);
    float_4 step1Comp(float_4);
};

inline float_4 Cmprsr::getGain() const {
    return gain_;
}

inline void Cmprsr::setNumChannels(int ch) {
    maxChannel = ch - 1;
    updateProcFun();
}


// only called for non poly
inline void Cmprsr::updateProcFun() {
    // printf("in update, max = %d\n", maxChannel);
    procFun = &Cmprsr::stepGeneric;
    if (maxChannel == 0 && (ratio[0] != Ratios::HardLimit)) {
        if (reduceDistortion) {
            procFun = &Cmprsr::step1NoDistComp;
        } else {
            procFun = &Cmprsr::step1Comp;
        }
    }
}

inline void Cmprsr::setCurve(Ratios r) {
    ratio[0] = r;
    ratio[1] = r;
    ratio[2] = r;
    ratio[3] = r;

    ratioIndex[0] = int(r);
    ratioIndex[1] = int(r);
    ratioIndex[2] = int(r);
    ratioIndex[3] = int(r);
}

inline void Cmprsr::setCurvePoly(const Ratios* r) {
    ratio[0] = r[0];
    ratio[1] = r[1];
    ratio[2] = r[2];
    ratio[3] = r[3];

    ratioIndex[0] = int(r[0]);
    ratioIndex[1] = int(r[1]);
    ratioIndex[2] = int(r[2]);
    ratioIndex[3] = int(r[3]);
}

inline float_4 Cmprsr::step(float_4 input) {
    return (this->*procFun)(input);
}

// only non poly
inline float_4 Cmprsr::step1Comp(float_4 input) {
    assert(wasInit());
    //printf("step1Comp gain = %s\n", toStr(gain_).c_str());
    lag.step(rack::simd::abs(input));
    float_4 envelope = lag.get();

    CompCurves::LookupPtr table = ratioCurves[ratioIndex[0]];
    //   gain = float_4(1);
    const float_4 level = envelope * invThreshold;

    // gain[0] = CompCurves::lookup(table, level[0]);
    float_4 t = gain_;
    t[0] = CompCurves::lookup(table, level[0]);
    gain_ = t;
    return gain_ * input;
}

// only non poly
inline float_4 Cmprsr::step1NoDistComp(float_4 input) {
    assert(wasInit());

    //printf("step1NoDist gain = %s\n", toStr(gain_).c_str());
    lag.step(rack::simd::abs(input));
    attackFilter.step(lag.get());
    float_4 envelope = attackFilter.get();

    CompCurves::LookupPtr table = ratioCurves[ratioIndex[0]];

    const float_4 level = envelope * invThreshold;

    float_4 t = gain_;
    t[0] = CompCurves::lookup(table, level[0]);
    gain_ = t;
    return gain_ * input;
}

// only non-poly
inline float_4 Cmprsr::stepGeneric(float_4 input) {
    assert(wasInit());

    float_4 envelope;
    if (reduceDistortion) {
        lag.step(rack::simd::abs(input));
        attackFilter.step(lag.get());
        envelope = attackFilter.get();
    } else {
        lag.step(rack::simd::abs(input));
        envelope = lag.get();
    }

    if (ratio[0] == Ratios::HardLimit) {
        float_4 reductionGain = threshold / envelope;
        gain_ = SimdBlocks::ifelse(envelope > threshold, reductionGain, 1);
        return gain_ * input;
    } else {
        CompCurves::LookupPtr table = ratioCurves[ratioIndex[0]];
        const float_4 level = envelope * invThreshold;

        float_4 t = gain_;
        for (int i = 0; i < 4; ++i) {
            if (i <= maxChannel) {
                t[i] = CompCurves::lookup(table, level[i]);
            }
        }
        gain_ = t;
        return gain_ * input;
    }
}

// only non-poly
inline float_4 Cmprsr::stepPoly(float_4 input) {
    assert(wasInit());
    simd_assertMask(reduceDistortionPoly);

    float_4 envelope;

    lag.step(rack::simd::abs(input));
    attackFilter.step(lag.get());
    envelope = SimdBlocks::ifelse(reduceDistortionPoly, attackFilter.get(), lag.get());


    // have to do the rest non-simd - in case the curves are all different.
    // TODO: optimized case for all curves the same
    for (int iChan = 0; iChan < 4; ++iChan) {
        if (ratio[iChan] == Ratios::HardLimit) {
            const float reductionGain = threshold[iChan] / envelope[iChan];
            gain_[iChan] = (envelope[iChan] > threshold[iChan]) ? threshold[iChan] / envelope[iChan] : 1.f;
        }
        else {
            CompCurves::LookupPtr table = ratioCurves[ratioIndex[iChan]];
            const float level = envelope[iChan] * invThreshold[iChan];
            gain_[iChan] = CompCurves::lookup(table, level);
        }

    }
    return gain_ * input;
}

inline void Cmprsr::setTimesPoly(float_4 attackMs, float_4 releaseMs, float sampleTime, float_4 enableDistortionReduction) {
    simd_assertMask(enableDistortionReduction);
    const float_4 correction = 2 * M_PI;
    const float_4 releaseHz = 1000.f / (releaseMs * correction);
    const float_4 attackHz = 1000.f / (attackMs * correction);
  //  const float_4 normRelease = releaseHz * sampleTime;

    // this sets:
    // this->reduce dist
    // lag.instantAttack    
    // lag.release
    // lag.attack
    // attackFilter.cutoff


    this->reduceDistortionPoly = SimdBlocks::ifelse( attackMs < float_4(.1f), SimdBlocks::maskFalse(), enableDistortionReduction);
    lag.setInstantAttackPoly(attackMs < float_4(.1f));

    lag.setAttackPoly(attackHz * sampleTime);
    attackFilter.setCutoffPoly(attackHz * sampleTime);
    lag.setReleasePoly(releaseHz * sampleTime);
   


    #if 0

    if (attackMs < .1) {
        reduceDistortion = false;  // no way to do this at zero attack
        lag.setInstantAttack(true);
        lag.setRelease(normRelease);
    }
    else {
        reduceDistortion = enableDistortionReduction;
        const float correction = 2 * M_PI;
        float attackHz = 1000.f / (attackMs * correction);
        lag.setInstantAttack(false);

        const float normAttack = attackHz * sampleTime;
        if (enableDistortionReduction) {
            lag.setAttack(normAttack * 4);
            attackFilter.setCutoff(normAttack * 1);
        }
        else {
            lag.setAttack(normAttack);
        }
    }

    lag.setRelease(normRelease);
    #endif
   // updateProcFun();
}

inline void Cmprsr::setTimes(float attackMs, float releaseMs, float sampleTime, bool enableDistortionReduction) {
    const float correction = 2 * M_PI;
    const float releaseHz = 1000.f / (releaseMs * correction);
    const float normRelease = releaseHz * sampleTime;

    if (attackMs < .1) {
        reduceDistortion = false;  // no way to do this at zero attack
        lag.setInstantAttack(true);
        lag.setRelease(normRelease);
    } else {
        reduceDistortion = enableDistortionReduction;
        const float correction = 2 * M_PI;
        float attackHz = 1000.f / (attackMs * correction);
        lag.setInstantAttack(false);

        const float normAttack = attackHz * sampleTime;
        if (enableDistortionReduction) {
            lag.setAttack(normAttack * 4);
            attackFilter.setCutoff(normAttack * 1);
        } else {
            lag.setAttack(normAttack);
        }
    }

    lag.setRelease(normRelease);
    updateProcFun();
}

inline Cmprsr::Cmprsr() {
    const float softKnee = 12;

    gain_ = float_4(1);

    if (wasInit()) {
        return;
    }

    for (int i = int(Ratios::NUM_RATIOS) - 1; i >= 0; --i) {
        Ratios ratio = Ratios(i);
        switch (ratio) {
            case Ratios::HardLimit:
                // just need to have something here
                ratioCurves[i] = ratioCurves[int(Ratios::_4_1_hard)];
                assert(wasInit());
                break;
            case Ratios::_2_1_soft: {
                CompCurves::Recipe r;
                r.ratio = 2;
                r.kneeWidth = softKnee;
                ratioCurves[i] = CompCurves::makeCompGainLookup(r);
            } break;
            case Ratios::_2_1_hard: {
                CompCurves::Recipe r;
                r.ratio = 2;
                ratioCurves[i] = CompCurves::makeCompGainLookup(r);
            } break;
            case Ratios::_4_1_soft: {
                CompCurves::Recipe r;
                r.ratio = 4;
                r.kneeWidth = softKnee;
                ratioCurves[i] = CompCurves::makeCompGainLookup(r);
            } break;
            case Ratios::_4_1_hard: {
                CompCurves::Recipe r;
                r.ratio = 4;
                ratioCurves[i] = CompCurves::makeCompGainLookup(r);
            } break;
            case Ratios::_8_1_soft: {
                CompCurves::Recipe r;
                r.ratio = 8;
                r.kneeWidth = softKnee;
                ratioCurves[i] = CompCurves::makeCompGainLookup(r);
            } break;
            case Ratios::_8_1_hard: {
                CompCurves::Recipe r;
                r.ratio = 8;
                ratioCurves[i] = CompCurves::makeCompGainLookup(r);
            } break;
            case Ratios::_20_1_soft: {
                CompCurves::Recipe r;
                r.ratio = 20;
                r.kneeWidth = softKnee;
                ratioCurves[i] = CompCurves::makeCompGainLookup(r);
            } break;
            case Ratios::_20_1_hard: {
                CompCurves::Recipe r;
                r.ratio = 20;
                ratioCurves[i] = CompCurves::makeCompGainLookup(r);
            } break;
            default:
                assert(false);
        }
    }
    assert(wasInit());
}

inline const std::vector<std::string>& Cmprsr::ratios() {
    assert(int(Ratios::NUM_RATIOS) == 9);
    static const std::vector<std::string> theRatios = {"Limit", "2:1 soft", "2:1 hard", "4:1 soft", "4:1 hard", "8:1 soft", "8:1 hard", "20:1 soft", "20:1 hard"};
    return theRatios;
}

inline const std::vector<std::string>& Cmprsr::ratiosLong() {
    assert(int(Ratios::NUM_RATIOS) == 9);
    static const std::vector<std::string> theRatios = {"Infinite (limiter)", "2:1 soft-knee", "2:1 hard-knee", "4:1 soft-knee", "4:1 har-kneed", "8:1 soft-knee", "8:1 hard-knee", "20:1 soft-knee", "20:1 hard-knee"};
    return theRatios;
}

inline const MultiLag2& Cmprsr::_lag() const {
    return lag;
}

inline void Cmprsr::setThreshold(float th) {
    setThresholdPoly(float_4(th));
}

inline void Cmprsr::setThresholdPoly(float_4 th) {
    threshold = th;
    invThreshold = 1.f / threshold;
}
