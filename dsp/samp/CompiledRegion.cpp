
#include "CompiledInstrument.h"
#include "CompiledRegion.h"

#include "SParse.h"

#include <set>
#include <functional>

int compileCount = 0;

using Opcode = SamplerSchema::Opcode;
CompiledRegion::CompiledRegion(SRegionPtr region, CompiledGroupPtr parent) : weakParent(parent), lineNumber(region->lineNumber)
{
    compileCount++;
    const SRegion& reg = *region;
    assert(reg.compiledValues);

    // TODO: better API for getting values
    auto value = reg.compiledValues->get(SamplerSchema::Opcode::LO_KEY);
    if (value) {
        lokey = value->numericInt;
    }
    value = reg.compiledValues->get(Opcode::HI_KEY);
    if (value) {
        hikey = value->numericInt;
    }
    value = reg.compiledValues->get(Opcode::KEY);
    if (value) {
        hikey = lokey = value->numericInt;
        keycenter = hikey;
    }

    value = reg.compiledValues->get(Opcode::SAMPLE);
    if (value) {
        assert(!value->string.empty());
        sampleFile = value->string;
    }
            
    value = reg.compiledValues->get(Opcode::PITCH_KEYCENTER);
    if (value) {
        keycenter = value->numericInt;
    }

    value = reg.compiledValues->get(Opcode::LO_VEL);
    if (value) {
        lovel = value->numericInt;
    }

    value = reg.compiledValues->get(Opcode::HI_VEL);
    if (value) {
        hivel = value->numericInt;
    }
}

static bool overlapRange(int alo, int ahi, int blo, int bhi) {
    assert(alo <= ahi);
    assert(blo <= bhi);

    return (blo >= alo && blo <= ahi) ||   // blo is in A
        (bhi >= alo && bhi <= ahi);         // or bhi is in A
}


/*
overlap comparing line 220 with 319
  first pitch=107,108, vel=107,127
  second pitch=0,127, vel=1,127
  overlap pitch = 0, overlap vel = 1

  alo 107
  ahi = 108
  blo = 0
  bhi = 127;

  first clause fails, since blo is < alo

   (bhi >= alo && bhi <= ahi); 
    1: true, bhi > alo
    1
  */

bool CompiledRegion::overlapsPitch(const CompiledRegion& that) const
{
    return overlapRange(this->lokey, this->hikey, that.lokey, that.hikey);
}

bool CompiledRegion::overlapsVelocity(const CompiledRegion& that) const 
{
    return overlapRange(this->lovel, this->hivel, that.lovel, that.hivel);
}

bool CompiledRegion::overlapsVelocityButNotEqual(const CompiledRegion& that) const
{
    return overlapsVelocity(that) && !velocityRangeEqual(that);
}

bool CompiledRegion::velocityRangeEqual(const CompiledRegion& that) const
{
    return (this->lovel == that.lovel) && (this->hivel == that.hivel);
}

bool CompiledRegion::pitchRangeEqual(const CompiledRegion& that) const
{
    return (this->lokey == that.lokey) && (this->hikey == that.hikey);
}

CompiledGroup::CompiledGroup(SGroupPtr group)
{
    compileCount++;

    auto value = group->compiledValues->get(Opcode::TRIGGER);
    if (value) {
        assert(value->type == SamplerSchema::OpcodeType::Discrete);
        //ignore = (trigger != DiscreteValue::ATTACK);
        trigger =  value->discrete;
    }
}

bool CompiledGroup::shouldIgnore() const
{
    bool dontIgnore = trigger == SamplerSchema::DiscreteValue::NONE || trigger == SamplerSchema::DiscreteValue::ATTACK;
    return !dontIgnore;
}
