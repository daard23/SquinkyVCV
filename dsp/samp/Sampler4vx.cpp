
#include "Sampler4vx.h"

#include "CompiledInstrument.h"
#include "PitchUtils.h"
#include "SInstrument.h"
#include "WaveLoader.h"

void Sampler4vx::setPatch(CompiledInstrumentPtr inst) {
    patch = inst;
}

const float Sampler4vx::defaultAttackSec = {.001f};
const float Sampler4vx::defaultDecaySec = {.1f};
const float Sampler4vx::defaultReleaseSec = {.3f};

void Sampler4vx::setLoader(WaveLoaderPtr loader) {
    waves = loader;

    // While we are at it, let's initialize the ADSR.
    adsr.setASec(defaultAttackSec);
    adsr.setDSec(defaultDecaySec);
    adsr.setS(1);
    adsr.setRSec(defaultReleaseSec);
}

#ifdef _SAMPFM
float_4 Sampler4vx::step(const float_4& gates, float sampleTime, const float_4& lfm, bool lfmEnabled) {
    sampleTime_ = sampleTime;
    if (patch && waves) {
        simd_assertMask(gates);

        float_4 envelopes = adsr.step(gates, sampleTime);
        float_4 samples = player.step(lfm, lfmEnabled);
        // apply envelope and boost level
        return envelopes * samples * _outputGain();
    } else {
        return 0;
    }
    return 0.f;
}

void Sampler4vx::setExpFM(const float_4& value) {
    fmCV = value;
    updatePitch();
}
#endif

#ifndef _SAMPFM

float_4 Sampler4vx::step(const float_4& gates, float sampleTime) {
    sampleTime_ = sampleTime;
    if (patch && waves) {
        simd_assertMask(gates);

        float_4 envelopes = adsr.step(gates, sampleTime);
        float_4 samples = player.step();
        // apply envelope and boost level
        return envelopes * samples * _outputGain();
    } else {
        return 0;
    }
    return 0.f;
}
#endif

void Sampler4vx::updatePitch() {
    // TODO: get rid of all this crazy semitone/ocatve stuff!!

    float_4 combinedCV = fmCV * 12 + pitchCVFromKeyboard;
    float_4 transposeAmt;
    for (int i = 0; i < 4; ++i) {
        transposeAmt[i] = PitchUtils::semitoneToFreqRatio(combinedCV[i]);
    }
    player.setTranspose(transposeAmt);
#if 0
    if (myIndex == 0) {
        SQINFO("Sampler4vx::updatePitch %s", toStr(transposeAmt).c_str());
    }
#endif
}

bool Sampler4vx::note_on(int channel, int midiPitch, int midiVelocity, float sampleRate) {
    if (!patch || !waves) {
        SQDEBUG("4vx not intit");
        return false;
    }
    if (patch->isInError()) {
        assert(false);
    }
    VoicePlayInfo patchInfo;
    VoicePlayParameter params;
    params.midiPitch = midiPitch;
    params.midiVelocity = midiVelocity;
    const bool didKS = patch->play(patchInfo, params, waves.get(), sampleRate);
    if (!patchInfo.valid) {
        SQINFO("could not get play info pitch %d vel%d", midiPitch, midiVelocity);
        player.clearSamples(channel);
        return didKS;
    }

    WaveLoader::WaveInfoPtr waveInfo = waves->getInfo(patchInfo.sampleIndex);
    assert(waveInfo->isValid());
#if 0
    SQINFO("played file=%s", waveInfo->getFileName().c_str());
#endif

    player.setSample(channel, waveInfo->getData(), int(waveInfo->getTotalFrameCount()));
    player.setGain(channel, patchInfo.gain);

    // I don't think this test cares what we set the player too

#ifdef _SAMPFM

    const float transposeCV = patchInfo.transposeV * 12;
    pitchCVFromKeyboard[channel] = transposeCV;
    updatePitch();
#if 0  // old way
    const float transposeCV = patchInfo.transposeV * 12 + 12 * fmCV[channel];
    const float transposeAmt = PitchUtils::semitoneToFreqRatio(transposeCV);

#if 0
    SQINFO("");
    SQINFO("trans from patch = %f trans from fm = %f", patchInfo.transposeV * 12, fmCV[channel]);
    SQINFO("total transCV %f", transposeCV);
    SQINFO("final amt, after exp = %f", transposeAmt);
    SQINFO("will turn on needs transpose for now...");
#endif
    //  player.setTranspose(channel, patchInfo.needsTranspose, transposeAmt);

    player.setTranspose(channel, true, transposeAmt);
#endif

#else
    player.setTranspose(channel, patchInfo.needsTranspose, patchInfo.transposeAmt);
#endif

   // std::string sample = waveInfo->fileName.getFilenamePart();
    // SQINFO("play vel=%d pitch=%d gain=%f samp=%s", midiVelocity, midiPitch, patchInfo.gain, sample.c_str());

    // this is a little messed up - the adsr should really have independent
    // settings for each channel. OK for now, though.
    R[channel] = patchInfo.ampeg_release;
    adsr.setRSec(R[channel]);
    releaseTime_ = patchInfo.ampeg_release;
    return didKS;
}

void Sampler4vx::setNumVoices(int voices) {
}

bool Sampler4vx::_isTransposed(int channel) const {
    return player._isTransposed(channel);
}

float Sampler4vx::_transAmt(int channel) const {
    return player._transAmt(channel);
}