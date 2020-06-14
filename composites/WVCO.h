
#pragma once

/**
 * 6/13 start to refactor 86.8% (gates at full rate). 
 * 6/4 use new sine approximation, down to 124%
 * 5/31: feature complete:
 *  8channels 148%
 
 * 
 * -finline-limit=500000  down to 144.7
 * down to 66 if step is gutted. so let's make an optimized step.
 * (removing adsr inner only took down to 64). SO, asside from inner loop, there must be a lot of bs to get rid of
 * instead of gutting step, just gutted oversampled part. that gives 97. so optimize at step level seems sensible.
 * however, minimal inner loop is still 136. same when I add flatten.
 * 
 * removing the optimzied sine takes back to 67, so the loop is gnarly
 * it's 110 with the filter removed, 82 with sine and filter removed
 * so - inner loops = 70 (40 filter, 30 sine)
 * 
 * 
 * 
 * 
 * 5/25 : 8 channels = 135%
 * 2,005,168 bytes in reduced plugin
 * 
 * down to 96% with re-written asserts
 * first try update envelopes audio rate: 136
 * 
 * 5/17 stock : 123.5
 * -march=native: 100 
 * -finline-limit=n: 103.5
 * -finline-limit=500000 -finline-functions-called-once: 103.6
 *  flatten on step() -finline-limit=500000 -finline-functions-called-once: 102
 *  -flto and all linlines
 * all the above: 77
 * 
 * gcc options to try:
 * 
 * -flto
 * (nc with ?) -fwhole-program
 * -finline-funciton
 * -march=native or skylake

 * static int max(int x, int y) __attribute__((always_inline));
static int max(int x, int y)
{

-finline-functions-called-once

--forceinline

__forceinline static int max(int x, int y)

-finline-limit=n

__attribute__ ((flatten))

conclusions, round 1:
march native is significant, but why? It's not generating avx, is it? (more to do)
inline options make a difference. unclear which to use, although inline-limit is easy
float -> float_4 isn't free.
 */

#include <assert.h>
#include <memory>
#include <vector>

#ifndef _MSC_VER
#include "ADSR16.h"
#include "WVCODsp.h"
#include "LookupTable.h"
#include "ObjectCache.h"

#include "IComposite.h"

#include "Divider.h"


using float_4 = rack::simd::float_4;

#ifndef _CLAMP
#define _CLAMP
namespace std {
    inline float clamp(float v, float lo, float hi)
    {
        assert(lo < hi);
        return std::min(hi, std::max(v, lo));
    }
}
#endif

namespace rack {
    namespace engine {
        struct Module;
    }
}

using Module = ::rack::engine::Module;

template <class TBase>
class WVCODescription : public IComposite
{
public:
    Config getParam(int i) override;
    int getNumParams() override;
};

class TriFormula
{
public:
    static void getLeftA(float_4& outA, float_4 k)
    {
        simd_assertLT(k, float_4(1));
        simd_assertGT(k, float_4(0));
        outA = 1 / k;
    }
    static void getRightAandB(float_4& outA, float_4& outB, float_4 k)
    {
        outA = 1 / (k - 1);
        outB = -outA;
    }
};

template <class TBase>
class WVCO : public TBase
{
public:

    WVCO(Module * module) : TBase(module)
    {
    }
    WVCO() : TBase()
    {
    }

    /**
    * re-calc everything that changes with sample
    * rate. Also everything that depends on baseFrequency.
    *
    * Only needs to be called once.
    */
    void init();

    enum ParamIds
    {
        OCTAVE_PARAM,
        FREQUENCY_MULTIPLIER_PARAM,
        FINE_TUNE_PARAM,
        FM_DEPTH_PARAM,
        LINEAR_FM_DEPTH_PARAM,
        WAVESHAPE_GAIN_PARAM,
        WAVE_SHAPE_PARAM,
        FEEDBACK_PARAM,
        OUTPUT_LEVEL_PARAM,

        ATTACK_PARAM,
        DECAY_PARAM,
        SUSTAIN_PARAM,
        RELEASE_PARAM,

        ADSR_SHAPE_PARAM,
        ADSR_FBCK_PARAM,
        ADSR_OUTPUT_LEVEL_PARAM,
        ADSR_LFM_DEPTH_PARAM,
        SNAP_PARAM,
        SNAP2_PARAM,
        NUM_PARAMS
    };

    enum InputIds
    {
        VOCT_INPUT,
        FM_INPUT,
        LINEAR_FM_INPUT,
        GATE_INPUT,
        SYNC_INPUT,
        SHAPE_INPUT,
        DEPTH_INPUT,
        FEEDBACK_INPUT,
        NUM_INPUTS
    };

    enum OutputIds
    {
        MAIN_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds
    {
        NUM_LIGHTS
    };

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription()
    {
        return std::make_shared<WVCODescription<TBase>>();
    }

    /**
     * Main processing entry point. Called every sample
     */
     __attribute__((flatten))
    void step() override;

    static std::vector<std::string> getWaveformNames()
    {
        return {"Sine", "Folder", "Saw/T"};
    }

private:
    Divider divn;
    Divider divm;
    WVCODsp dsp[4];
    ADSR16 adsr;

    std::function<float(float)> expLookup = ObjectCache<float>::getExp2Ex();
    std::shared_ptr<LookupTableParams<float>>  audioTaperLookupParams = ObjectCache<float>::getAudioTaper();

    // variables to stash processed knobs and other input
    // variables with _m suffix updated every m period
    float_4 basePitch_m = 0;        // all the knobs, no cv. units are volts
    int numChannels_m = 1;      // 1..16
    int numBanks_m = 1;     // 1..4
    float_4 depth_m;                // exp mode depth from knob

    int freqMultiplier = 1;
    float baseShapeGain = 0;    // 0..1 -> re-do this!
    float baseFeedback = 0;
    float baseOutputLevel = 1;  // 0..x
    float baseOffset = 0;
    bool enableAdsrLevel = false;
    bool enableAdsrFeedback = false;
    bool enableAdsrFM = false;
    bool enableAdsrShape = false;

    float_4 getOscFreq(int bank);

    /**
     * This was originally a regular /4 stepn, but
     * I started doing it at X 1.
     * Now need to refactor
     */
    void stepn_fullRate();

    void stepn_lowerRate();

    /**
     * This is called "very infrequently"
     * Currently every 16 samples, but could go lower. for knobs and such
     */
    void stepm();

    /**
     * combine CV with knobs (basePitch_m),
     * send to DSPs
     */
    void updateFreq_n();
};


template <class TBase>
inline void WVCO<TBase>::init()
{
    adsr.setNumChannels(1);         // just to prime the pump, will write true value later
    divn.setup(4, [this]() {
        stepn_lowerRate();
    });
     divm.setup(16, [this]() {
        stepm();
    });
}

template <class TBase>
inline void WVCO<TBase>::stepm()
{
    numChannels_m = std::max<int>(1, TBase::inputs[VOCT_INPUT].channels);
    WVCO<TBase>::outputs[ WVCO<TBase>::MAIN_OUTPUT].setChannels(numChannels_m);

    numBanks_m = numChannels_m / 4;
    if (numChannels_m > numBanks_m * 4) {
        numBanks_m++;
    }

    float basePitch = -4.0f + roundf(TBase::params[OCTAVE_PARAM].value) +      
        TBase::params[FINE_TUNE_PARAM].value / 12.0f;

    const float q = float(log2(261.626));       // move up to pitch range of EvenVCO
    basePitch += q;
    basePitch_m = float_4(basePitch);

    depth_m = .3 * LookupTable<float>::lookup(
                *audioTaperLookupParams, 
                TBase::params[FM_DEPTH_PARAM].value * .01f);


    freqMultiplier = int(std::round(TBase::params[FREQUENCY_MULTIPLIER_PARAM].value)); 

    int wfFromUI = (int) std::round(TBase::params[WAVE_SHAPE_PARAM].value);
    WVCODsp::WaveForm wf = WVCODsp::WaveForm(wfFromUI);

    baseShapeGain = TBase::params[WAVESHAPE_GAIN_PARAM].value / 100;
    const bool sync = TBase::inputs[SYNC_INPUT].isConnected();


    for (int bank=0; bank < numBanks_m; ++bank) {
        dsp[bank].waveform = wf;
        dsp[bank].setSyncEnable(sync);
        dsp[bank].waveformOffset = baseOffset;
    }

    // these numbers here are just values found by experimenting - no math.
    baseFeedback =  TBase::params[FEEDBACK_PARAM].value * 2.f / 1000.f;
    baseOutputLevel =  TBase::params[OUTPUT_LEVEL_PARAM].value / 100.f;

    // Sine, Fold, SawTri
    // find the correct offset and gains to apply the waveformat
    // get thet them nomalized
    switch (wf) {
        case WVCODsp::WaveForm::Sine:
            baseOffset = 0;
            baseOutputLevel *= 5;
            break;
        case WVCODsp::WaveForm::Fold:
            baseOffset = 0;
            baseOutputLevel *=  (5.f * 5.f / 5.6f);
            break;
        case WVCODsp::WaveForm::SawTri:
            baseOffset = -.5f; 
            baseOutputLevel *= 10;
            break;
        default:
            assert(0);
    }

    const bool snap = TBase::params[SNAP_PARAM].value > .5f;
    const bool snap2 = TBase::params[SNAP2_PARAM].value > .5f;

    float k = 1;
    if (snap || snap2) {
        k = .6;
    }
     if (snap && snap2) {
        k = .3;
    }
 
    adsr.setParams(
        TBase::params[ATTACK_PARAM].value * .01,
        TBase::params[DECAY_PARAM].value * .01,
        TBase::params[SUSTAIN_PARAM].value * .01,
        TBase::params[RELEASE_PARAM].value * .01,
        k
    ) ;

    adsr.setNumChannels(numChannels_m);

    enableAdsrLevel = TBase::params[ADSR_OUTPUT_LEVEL_PARAM].value > .5;
    enableAdsrFeedback = TBase::params[ADSR_FBCK_PARAM].value > .5;
    enableAdsrFM = TBase::params[ADSR_LFM_DEPTH_PARAM].value > .5;
    enableAdsrShape = TBase::params[ADSR_SHAPE_PARAM].value > .5;
}

template <class TBase>
inline void WVCO<TBase>::updateFreq_n()
{
    for (int bank=0; bank < numBanks_m; ++bank) {
        float_4 freq=0;
        for (int i=0; i<4; ++i) {
            const int channel = bank * 4 + i;

            float pitch = basePitch_m[0];
            // use SIMD here?
            pitch += TBase::inputs[VOCT_INPUT].getVoltage(channel);

            pitch += TBase::inputs[FM_INPUT].getPolyVoltage(channel) * depth_m[0];               
            float _freq = expLookup(pitch);
            _freq *= freqMultiplier;
            const float time = std::clamp(_freq * TBase::engineGetSampleTime(), -.5f, 0.5f);
            freq[i] = time;
        }
        dsp[bank].normalizedFreq = freq / WVCODsp::oversampleRate;
    }
}


template <class TBase>
inline void WVCO<TBase>::stepn_lowerRate()
{
    updateFreq_n();    // combine the
}

template <class TBase>
inline void WVCO<TBase>::stepn_fullRate()
{
    assert(numBanks_m > 0);

    static int x = 0;


    //--------------------------------------------------------------------
    // round up all the gates and run the ADSR;
    {
        // can do gates is lower rate (but it's no better)
        float_4 gates[4];
        for (int i=0; i<4; ++i) {
            Port& p = TBase::inputs[GATE_INPUT];
            float_4 g = p.getVoltageSimd<float_4>(i * 4);
            float_4 gate = (g > float_4(1));
            simd_assertMask(gate);
            gates[i] = gate;
        }

        adsr.step(gates, TBase::engineGetSampleTime());
    }
    


    // ----------------------------------------------------------------------------
    // now update all the DSP params.
    // This is the new, cleaner version.
    // Legacy version is below
#if 0
    for (int bank=0; bank < numBanks_m; ++bank) {
       // float_4 freq;
        for (int i=0; i<4; ++i) {
            const int channel = bank * 4 + i;
        }

    }
#endif



       

    // TODO: make this faster, and/or do less often
     // update the pitch of every vco
     // This is the legacy version
    for (int bank=0; bank < numBanks_m; ++bank) {


        float_4 envMult = (enableAdsrShape) ? adsr.get(bank) : 1;
        simd_assertLE( envMult, float_4(2));
        simd_assertGE(envMult, float_4(0)); 
       
       
     // (one easy thing would ge to do this whole block a 'n' rate)
     // that should be fast enough for shape modulation

      // moved the calculations that were done at oversample rate here,
      // but there is still a lot stuff that doesn't have to be done at sample rate.
       // dsp[bank].shapeAdjust = baseShapeGain * envMult; 
        float_4 correctedWaveShapeMultiplier = baseShapeGain * envMult;
        switch(dsp[bank].waveform) {
            case WVCODsp::WaveForm::Sine:
                break;
            case WVCODsp::WaveForm::Fold:
                correctedWaveShapeMultiplier += float_4(.095);
                correctedWaveShapeMultiplier *= 10;
                break;
            case WVCODsp::WaveForm::SawTri:
                correctedWaveShapeMultiplier = .5 + correctedWaveShapeMultiplier / 2;
                break;
        //    default:
            // done't assert - sometimes we get here before waveform is initialized
               // assert(false);
        }
        dsp[bank].correctedWaveShapeMultiplier = correctedWaveShapeMultiplier;

        assertLE( baseShapeGain, 1);
        assertGE( baseShapeGain, 0);   

      //  simd_assertLE( dsp[bank].shapeAdjust, float_4(2));
     //   simd_assertGE( dsp[bank].shapeAdjust, float_4(0));   


        // could to this at 'n' rate, and only it triangle
        // now let's compute triangle params
        const float_4 shapeGain = rack::simd::clamp(baseShapeGain * envMult, .01, .99);
        simd_assertLT(shapeGain, float_4(2));
        simd_assertGT(shapeGain, float_4(0));

        float_4 k = .5 + shapeGain / 2;
        float_4 a, b;
        TriFormula::getLeftA(a, k);
        dsp[bank].aLeft = a;
        TriFormula::getRightAandB(a, b, k);

        dsp[bank].aRight = a;
        dsp[bank].bRight = b;
        
        
        if (enableAdsrFeedback) {
            dsp[bank].feedback = baseFeedback * 3 * adsr.get(bank); 
        } else {
            dsp[bank].feedback = baseFeedback * 3;
        }

        // TODO: add CV (use getNormalPolyVoltage)
        if (enableAdsrLevel) {
            dsp[bank].outputLevel = adsr.get(bank) * baseOutputLevel;
#if 0
              if (x == 0) {
                printf("update level with env %s\n", toStr(adsr.env[bank]).c_str());
                printf("finaly output %s\n", toStr(dsp[bank].outputLevel).c_str());
            }
#endif
        } else {
            dsp[bank].outputLevel = float_4(baseOutputLevel);    
        }
    }
    x++;
    if (x > 1000) {
        x = 0;
    }
}


template <class TBase>
inline void  __attribute__((flatten)) WVCO<TBase>::step()
{
    // clock the sub-sample rate tasks
    divn.step();
    divm.step();

    stepn_fullRate();

    // run the sample loop and write audi
    assert(numBanks_m > 0);
    // todo - cache numBanks_m

    for (int bank=0; bank < numBanks_m; ++bank) {
        const int baseChannel = 4 * bank;
        Port& port = WVCO<TBase>::inputs[LINEAR_FM_INPUT];
        float_4 fmInput = port.getPolyVoltageSimd<float_4>(baseChannel);

        float_4 fmInputScaling;
        if (enableAdsrFM) {
            fmInputScaling = adsr.get(bank) * (WVCO<TBase>::params[LINEAR_FM_DEPTH_PARAM].value * .003);
        } else {
            fmInputScaling = WVCO<TBase>::params[LINEAR_FM_DEPTH_PARAM].value * .003;
        }
        dsp[bank].fmInput = fmInput * fmInputScaling;

        port = WVCO<TBase>::inputs[SYNC_INPUT];
      //  const bool isConnected = port.isConnected();
        const float_4 syncInput = port.getPolyVoltageSimd<float_4>(baseChannel);
        float_4 v = dsp[bank].step(syncInput); 
        WVCO<TBase>::outputs[MAIN_OUTPUT].setVoltageSimd(v, baseChannel);
    }
}

template <class TBase>
int WVCODescription<TBase>::getNumParams()
{
    return WVCO<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config WVCODescription<TBase>::getParam(int i)
{
    Config ret(0, 1, 0, "");
    switch (i) {
        case WVCO<TBase>::OCTAVE_PARAM:
            ret = {0.f, 10.0f, 4, "Octave"};
            break;
        case WVCO<TBase>::FREQUENCY_MULTIPLIER_PARAM:
            ret = {1.f, 16.0f, 1, "Freq Ratio"};
            break;
        case WVCO<TBase>::FINE_TUNE_PARAM:
            ret = {-12.0f, 12.0f, 0, "Fine Tune"};
            break;
        case WVCO<TBase>::FM_DEPTH_PARAM:
         ret = {.0f, 100.0f, 0, "Freq Mod"};
            break;
        case WVCO<TBase>::LINEAR_FM_DEPTH_PARAM:
            ret = {0, 100, 0, "Through-zero FM depth"};
            break;
        case WVCO<TBase>::WAVESHAPE_GAIN_PARAM:
            ret = {0, 100, 0, "Shape Mod"};
            break;
        case WVCO<TBase>::WAVE_SHAPE_PARAM:
            ret = {0, 2, 0, "Wave Shape"};
            break;
        case WVCO<TBase>::FEEDBACK_PARAM:
            ret = {0, 100, 0, "FM feedback depth"};
            break;
        case WVCO<TBase>::ATTACK_PARAM:
            ret = {0, 100, 50, "Attck"};
            break;
        case WVCO<TBase>::DECAY_PARAM:
            ret = {0, 100, 50, "Decay"};
            break;
        case WVCO<TBase>::SUSTAIN_PARAM:
            ret = {0, 100, 50, "Sustain"};
            break;
        case WVCO<TBase>::RELEASE_PARAM:
            ret = {0, 100, 50, "Release"};
            break;
        case WVCO<TBase>::OUTPUT_LEVEL_PARAM:
            ret = {0, 100, 100, "Level"};
            break;
        case WVCO<TBase>::ADSR_SHAPE_PARAM:
            ret = {0, 1, 0, "ADSR->shape"};
            break;
        case WVCO<TBase>::ADSR_FBCK_PARAM:
         ret = {0, 1, 0, "ADSR->Feedback"};
            break;
        case WVCO<TBase>::ADSR_OUTPUT_LEVEL_PARAM:
         ret = {0, 1, 0, "ADRS->Output Level"};
            break;
        case WVCO<TBase>::ADSR_LFM_DEPTH_PARAM:
         ret = {0, 1, 0, "ARSR->FM Depth"};
            break;
        case WVCO<TBase>::SNAP_PARAM:
            ret = {0, 1, 1, "ARSR Snap"};
            break;
         case WVCO<TBase>::SNAP2_PARAM:
            ret = {0, 1, 1, "ARSR Snap2"};
            break;
        default:
            assert(false);
    }
    return ret;
}
#endif
