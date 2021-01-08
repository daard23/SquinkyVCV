
#pragma once

#include <assert.h>

#include <memory>

#include "CompiledInstrument.h"
#include "Divider.h"
#include "IComposite.h"
#include "SInstrument.h"
#include "Sampler4vx.h"
#include "SimdBlocks.h"
#include "SqPort.h"
#include "WaveLoader.h"

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack
using Module = ::rack::engine::Module;

template <class TBase>
class SampDescription : public IComposite {
public:
    Config getParam(int i) override;
    int getNumParams() override;
};

template <class TBase>
class Samp : public TBase {
public:
    Samp(Module* module) : TBase(module) {
    }
    Samp() : TBase() {
    }

    /**
    * re-calc everything that changes with sample
    * rate. Also everything that depends on baseFrequency.
    *
    * Only needs to be called once.
    */
    void init();

    enum ParamIds {
        TEST_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        PITCH_INPUT,
        VELOCITY_INPUT,
        GATE_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        AUDIO_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        NUM_LIGHTS
    };

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<SampDescription<TBase>>();
    }

    void setNewSamples(const std::string& s);
    /**
     * Main processing entry point. Called every sample
     */
    //void step() override;
    void process(const typename TBase::ProcessArgs& args) override;

private:
    Sampler4vx playback[4];  // 16 voices of polyphony
    SInstrumentPtr instrument;
    WaveLoaderPtr waves;

    float_4 lastGate4[4];
    Divider divn;
    int numChannels_m = 1;

    bool lastGate = false;  // just for test now

    void stepn();

    void setupSamplesDummy();
};

template <class TBase>
inline void Samp<TBase>::init() {
    divn.setup(32, [this]() {
        this->stepn();
    });

    for (int i = 0; i < 4; ++i) {
        lastGate4[i] = float_4(0);
    }
    setupSamplesDummy();
}

template <class TBase>
inline void Samp<TBase>::setNewSamples(const std::string& s) {
#ifdef ARCH_WIN
    auto separator = '\\';
#else
    auto separator = '/';
#endif

    auto pos = s.rfind(separator);
    if (pos == std::string::npos) {
        printf("failed to parse path: %s\n", s.c_str());
        fflush(stdout);
        return;
    }

    std::string path = s.substr(0, pos) + separator;
    std::string fname = s.substr(pos + 1);
    printf("path = %s\n", path.c_str());
    printf("name = %s\n", fname.c_str());
    fflush(stdout);
}

template <class TBase>
inline void Samp<TBase>::setupSamplesDummy() {
    SInstrumentPtr inst = std::make_shared<SInstrument>();

    // tinny piano
    // const char* p = R"foo(D:\samples\UprightPianoKW-small-SFZ-20190703\UprightPianoKW-small-20190703.sfz)foo";
    //  const char* pRoot = R"foo(D:\samples\UprightPianoKW-small-SFZ-20190703\)foo";

    // small piano, with vel keyswitch
    static const char* p = R"foo(D:\samples\K18-Upright-Piano\K18-Upright-Piano.sfz)foo";
    static const char* pRoot = R"foo(D:\samples\K18-Upright-Piano\)foo";

    // snare drum
    // static const char* p =  R"foo(D:\samples\SalamanderDrumkit\snare.sfz)foo";
    // static const char* pRoot =  R"foo(D:\samples\SalamanderDrumkit\)foo";
    auto err = SParse::goFile(p, inst);
    assert(err.empty());

    CompiledInstrumentPtr cinst = CompiledInstrument::make(inst);
    waves = std::make_shared<WaveLoader>();

    cinst->setWaves(waves, pRoot);
    for (int i = 0; i < 4; ++i) {
        playback[i].setPatch(cinst);
    }

    fprintf(stderr, "about load waves\n");
    waves->load();
    fprintf(stderr, "loaded waves\n");
    WaveLoader::WaveInfoPtr info = waves->getInfo(1);
    assert(info->valid);

    for (int i = 0; i < 4; ++i) {
        playback[i].setLoader(waves);
        playback[i].setNumVoices(4);
    }
}

template <class TBase>
inline void Samp<TBase>::stepn() {
    SqInput& inPort = TBase::inputs[PITCH_INPUT];
    SqOutput& outPort = TBase::outputs[AUDIO_OUTPUT];
    numChannels_m = inPort.channels;
    outPort.setChannels(numChannels_m);
    // printf("just set to %d channels\n", numChannels_m); fflush(stdout);
}

#if 0  // mono version  works
template <class TBase>
inline void Samp<TBase>::process(const typename TBase::ProcessArgs& args) {
    divn.step();

    bool gate = TBase::inputs[GATE_INPUT].getVoltage(0) > 1;
    if (gate != lastGate) {
        printf("gate = %d\n", gate); fflush(stdout);
        if (gate) {
            const float pitchCV = TBase::inputs[PITCH_INPUT].getVoltage(0);
            const int midiPitch = 60 + int(std::floor(pitchCV * 12));

            // printf("raw vel input = %f\n", TBase::inputs[VELOCITY_INPUT].getVoltage(channel));
            const int midiVelocity = int(TBase::inputs[VELOCITY_INPUT].getVoltage(0) * 12.7f);
            playback[0].note_on(0, midiPitch, midiVelocity);
        } else {
            playback[0].note_off(0);
        }
        lastGate = gate;
    }
    float_4 output4 = playback[0].step(gate, args.sampleTime);
    float output = output4[0];
    TBase::outputs[AUDIO_OUTPUT].setVoltage(output);
}
#endif

#if 1  // real, poly version, fixed
template <class TBase>
inline void Samp<TBase>::process(const typename TBase::ProcessArgs& args) {
    divn.step();

    int numBanks = numChannels_m / 4;
    if (numBanks * 4 < numChannels_m) {
        numBanks++;
    }
    assert(numBanks < 4);
    for (int bank = 0; bank < numBanks; ++bank) {
        // prepare 4 gates. note that ADSR / Sampler4vx must see simd mask (0 or nan)
        // but our logic needs to see numbers (we use 1 and 0).
        Port& p = TBase::inputs[GATE_INPUT];
        float_4 g = p.getVoltageSimd<float_4>(bank * 4);
        float_4 gmask = (g > float_4(1));
        float_4 gate4 = SimdBlocks::ifelse(gmask, float_4(1), float_4(0));
        ;

        float_4 lgate4 = lastGate4[bank];

        for (int iSub = 0; iSub < 4; ++iSub) {
            if (gate4[iSub] != lgate4[iSub]) {
                if (gate4[iSub]) {
                    // printf("new gate on %d:%d gatevalue=%f\n", bank, iSub, gate4[iSub]); fflush(stdout);
                    assert(bank < 4);
                    const int channel = iSub + bank * 4;
                    const float pitchCV = TBase::inputs[PITCH_INPUT].getVoltage(channel);
                    const int midiPitch = 60 + int(std::floor(pitchCV * 12));

                    // printf("raw vel input = %f\n", TBase::inputs[VELOCITY_INPUT].getVoltage(channel));
                    int midiVelocity = int(TBase::inputs[VELOCITY_INPUT].getVoltage(channel) * 12.7f);
                    if (midiVelocity < 1) {
                        midiVelocity = 1;
                    }
                    playback[bank].note_on(iSub, midiPitch, midiVelocity);
                    // printf("send note on to bank %d sub%d pitch %d\n", bank, iSub, midiPitch); fflush(stdout);
                } else {
                    playback[bank].note_off(iSub);
                    // printf("new gate off %d:%d value = %f\n", bank, iSub, gate4[iSub]); fflush(stdout);
                }
            }
        }
        auto output = playback[bank].step(gmask, args.sampleTime);
        TBase::outputs[AUDIO_OUTPUT].setVoltageSimd(output, bank * 4);
        lastGate4[bank] = gate4;
    }
}
#endif

#if 0
template <class TBase>
inline void Samp<TBase>::process(const typename TBase::ProcessArgs& args) {
    divn.step();

    int numBanks = numChannels_m / 4;
    if (numBanks * 4 < numChannels_m) {
        numBanks++;
    }

    for (int bank = 0; bank < numBanks; ++bank) {
        for (int iSub = 0; iSub < 4; ++iSub) {
            const int channel = iSub + bank * 4;

            float_4 gate4 = 0;
            Port& p = TBase::inputs[GATE_INPUT];
            float_4 g = p.getVoltageSimd<float_4>(bank * 4);
            gate4 = (g > float_4(1));
            simd_assertMask(gate4);

           // const bool gate = TBase::inputs[GATE_INPUT].getVoltage(channel) > 1;
            if (gate_4 != lastGate4)
           // if (gate != lastGate[channel]) {
                if (gate) {
                    const float pitchCV = TBase::inputs[PITCH_INPUT].getVoltage(channel);
                    const int midiPitch = 60 + int(std::floor(pitchCV * 12));

                    // printf("raw vel input = %f\n", TBase::inputs[VELOCITY_INPUT].getVoltage(channel));
                    const int midiVelocity = int(TBase::inputs[VELOCITY_INPUT].getVoltage(channel) * 12.7f);
                    playback[bank].note_on(iSub, midiPitch, midiVelocity);
                    // printf("send note on to bank %d sub%d pitch %d\n", bank, iSub, midiPitch); fflush(stdout);
                } else {
                    playback[bank].note_off(iSub);
                }
                lastGate[channel] = gate;
            }
        }

        auto output = playback[bank].step();
        TBase::outputs[AUDIO_OUTPUT].setVoltageSimd(output, bank * 4);
    }
}
#endif

template <class TBase>
int SampDescription<TBase>::getNumParams() {
    return Samp<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config SampDescription<TBase>::getParam(int i) {
    Config ret(0, 1, 0, "");
    switch (i) {
        case Samp<TBase>::TEST_PARAM:
            ret = {-1.0f, 1.0f, 0, "Test"};
            break;
        default:
            assert(false);
    }
    return ret;
}
