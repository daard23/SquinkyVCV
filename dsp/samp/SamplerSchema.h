#pragma once

#include <assert.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class SKeyValuePair;
class SamplerErrorContext;
using SKeyValuePairPtr = std::shared_ptr<SKeyValuePair>;
using SKeyValueList = std::vector<SKeyValuePairPtr>;

class SamplerSchema {
public:
    /** left hand side of an SFZ opcode.
     */
    enum class Opcode {
        NONE,
        HI_KEY,
        LO_KEY,
        HI_VEL,
        LO_VEL,
        AMPEG_RELEASE,
     //   LOOP_CONTINUOUS,
        LOOP_MODE,
        LOOP_START,
        LOOP_END,
        PITCH_KEYCENTER,
        SAMPLE,
        PAN,
        GROUP,  // group is opcode as well at tag
        TRIGGER,
        VOLUME,
        TUNE,
        OFFSET,
        POLYPHONY,
        PITCH_KEYTRACK,
        AMP_VELTRACK,
        KEY,
        LO_RAND,
        HI_RAND,
        SEQ_LENGTH,
        SEQ_POSITION,
        DEFAULT_PATH,
        SW_LABEL,
        SW_LAST,
        SW_LOKEY,
        SW_HIKEY,
        SW_LOLAST,
        SW_HILAST,
        SW_DEFAULT,
        HICC64_HACK,        // It's a hack becuase it won't scale to "all" cc
        LOCC64_HACK,
    };

    enum class DiscreteValue {
        LOOP_CONTINUOUS,
        NO_LOOP,
        ONE_SHOT,
        LOOP_SUSTAIN,
        ATTACK,   // trigger=attack
        RELEASE,  // trigger= release

        NONE
    };

    enum class OpcodeType {
        Unknown,
        String,
        Int,
        Float,
        Discrete
    };

    /**
     * This holds the right had side of an opcode.
     * It can hold in integer, a float, a string, or a named discrete value
     */
    class Value {
    public:
        float numericFloat;
        int numericInt;
        DiscreteValue discrete;
        std::string string;
        OpcodeType type;
    };

    using ValuePtr = std::shared_ptr<Value>;

    /**
     * hold the compiled form of a collection of group attributes.
     */
    class KeysAndValues {
    public:
        size_t _size() const {
            return data.size();
        }
        void add(Opcode o, ValuePtr vp) {
            data[o] = vp;
        }

        ValuePtr get(Opcode o) {
            assert(this);
            auto it = data.find(o);
            if (it == data.end()) {
                return nullptr;
            }
            return it->second;
        }

    private:
        // the int key is really an Opcode
        std::map<Opcode, ValuePtr> data;
    };
    using KeysAndValuesPtr = std::shared_ptr<KeysAndValues>;

    // doesn't really belong here, but better there than some places...
    static KeysAndValuesPtr compile(SamplerErrorContext&, const SKeyValueList&);
    static Opcode translate(const std::string& key, bool suppressErrorMessages);
    static OpcodeType keyTextToType(const std::string& key, bool suppressErrorMessages);

    static bool isFreeTextType(const std::string& key);
    static std::vector<std::string> _getKnownTextOpcodes();
    static std::vector<std::string> _getKnownNonTextOpcodes();

    static bool stringToFloat(const char* s, float * outValue);
    static bool stringToInt(const char* s, int * outValue);

private:
    static std::pair<bool, int> convertToInt(SamplerErrorContext& err, const std::string& s);
    static void compile(SamplerErrorContext&, KeysAndValuesPtr results, SKeyValuePairPtr input);
    static DiscreteValue translated(const std::string& s);
    static std::set<std::string> freeTextFields;
};