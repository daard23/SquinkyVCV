
#include "samplerTests.h"
#include "asserts.h"

static void testRandomPlayerEmpty() {
    VoicePlayInfo info;
    VoicePlayParameter params;
    RandomVoicePlayerPtr player = std::make_shared<RandomVoicePlayer>();
    player->play(info, params);
    assert(!info.valid);
}

/*
  VoicePlayInfo(CompiledRegionPtr region, int midiPitch, int sampleIndex);
    bool valid = false;
    int sampleIndex = 0;
    bool needsTranspose = false;
    float transposeAmt = 1;
    float gain = 1;             // assume full volume
    float ampeg_release = .001f;

*/



static void testRandomPlayerOneEntry() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<region>sample=a lorand=0 hirand=1)foo");

    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiVelocity = 20;
    params.midiPitch = 60;
    RandomVoicePlayerPtr player = std::make_shared<RandomVoicePlayer>();
    player->addEntry(cr, 1, 60);
    player->play(info, params);
    assert(info.valid);
    assertEQ(info.sampleIndex, 1);
}

static void testRandomPlayerTwoEntry() {
    CompiledRegionPtr cr1 = st::makeRegion(R"foo(<region>sample=a lorand=0 hirand=.2)foo");
    CompiledRegionPtr cr2 = st::makeRegion(R"foo(<region>sample=a lorand=.2 hirand=1)foo");

    VoicePlayInfo info;
    VoicePlayParameter params;
    RandomVoicePlayerPtr player = std::make_shared<RandomVoicePlayer>();
    player->addEntry(cr1, 100, 60);
    player->addEntry(cr2, 101, 60);
    for (int i = 0; i < 10; ++i) {
        player->play(info, params);
        assert(info.valid);
        assertEQ(info.sampleIndex, 1);
    }
}

void testx4() {
    testRandomPlayerEmpty();
    testRandomPlayerOneEntry();
}