#pragma once
#include <juce_core/juce_core.h>
#include <array>

namespace cf
{
constexpr int kMaxSteps    = 64;
constexpr int kNumPatterns = 8;

/** One step of the re-sequencer. Values are plain data so the pattern can be
    copied freely between threads. */
struct StepData
{
    int   slice       = 0;     // index into SampleData::markers, -1 = rest
    float pitch       = 0.0f;  // semitones, -24..24
    float gain        = 1.0f;  // linear, 0..2
    float pan         = 0.0f;  // -1..1
    bool  reverse     = false;
    float probability = 1.0f;  // 0..1 chance that the step fires
    float filter      = 0.0f;  // -1..0 low-pass sweep, 0 = bypass, 0..1 high-pass sweep
    int   repeats     = 1;     // retriggers inside the step (stutter), 1..8
    float gate        = 1.0f;  // note length as fraction of the step, 0.1..2
    bool  lockSlice   = false;
    bool  lockPitch   = false;
    bool  lockFx      = false;

    bool operator== (const StepData& o) const noexcept
    {
        return slice == o.slice && juce::exactlyEqual (pitch, o.pitch) && juce::exactlyEqual (gain, o.gain)
            && juce::exactlyEqual (pan, o.pan) && reverse == o.reverse && juce::exactlyEqual (probability, o.probability)
            && juce::exactlyEqual (filter, o.filter) && repeats == o.repeats && juce::exactlyEqual (gate, o.gate)
            && lockSlice == o.lockSlice && lockPitch == o.lockPitch && lockFx == o.lockFx;
    }
    bool operator!= (const StepData& o) const noexcept { return ! (*this == o); }

    void clamp() noexcept
    {
        slice       = juce::jlimit (-1, 63, slice);
        pitch       = juce::jlimit (-24.0f, 24.0f, pitch);
        gain        = juce::jlimit (0.0f, 2.0f, gain);
        pan         = juce::jlimit (-1.0f, 1.0f, pan);
        probability = juce::jlimit (0.0f, 1.0f, probability);
        filter      = juce::jlimit (-1.0f, 1.0f, filter);
        repeats     = juce::jlimit (1, 8, repeats);
        gate        = juce::jlimit (0.1f, 2.0f, gate);
    }
};

struct Pattern
{
    std::array<StepData, kMaxSteps> steps;
    int length = 16;

    void initDefault (int numSlices)
    {
        const int n = juce::jmax (1, numSlices);
        for (int i = 0; i < kMaxSteps; ++i)
        {
            steps[(size_t) i] = StepData{};
            steps[(size_t) i].slice = i % n;
        }
    }

    bool operator== (const Pattern& o) const noexcept { return length == o.length && steps == o.steps; }
    bool operator!= (const Pattern& o) const noexcept { return ! (*this == o); }

    juce::String toString() const
    {
        juce::String s;
        s.preallocateBytes (kMaxSteps * 40);
        s << "v1|" << length << "|";
        for (const auto& st : steps)
        {
            s << st.slice << ',' << st.pitch << ',' << st.gain << ',' << st.pan << ',' << (st.reverse ? 1 : 0) << ','
              << st.probability << ',' << st.filter << ',' << st.repeats << ',' << st.gate << ','
              << ((st.lockSlice ? 1 : 0) | (st.lockPitch ? 2 : 0) | (st.lockFx ? 4 : 0)) << ';';
        }
        return s;
    }

    static Pattern fromString (const juce::String& text)
    {
        Pattern p;
        auto parts = juce::StringArray::fromTokens (text, "|", "");
        if (parts.size() < 3 || parts[0] != "v1") return p;
        p.length = juce::jlimit (1, kMaxSteps, parts[1].getIntValue());
        auto stepTokens = juce::StringArray::fromTokens (parts[2], ";", "");
        for (int i = 0; i < juce::jmin (kMaxSteps, stepTokens.size()); ++i)
        {
            auto f = juce::StringArray::fromTokens (stepTokens[i], ",", "");
            if (f.size() < 10) continue;
            auto& st = p.steps[(size_t) i];
            st.slice       = f[0].getIntValue();
            st.pitch       = f[1].getFloatValue();
            st.gain        = f[2].getFloatValue();
            st.pan         = f[3].getFloatValue();
            st.reverse     = f[4].getIntValue() != 0;
            st.probability = f[5].getFloatValue();
            st.filter      = f[6].getFloatValue();
            st.repeats     = f[7].getIntValue();
            st.gate        = f[8].getFloatValue();
            const int locks = f[9].getIntValue();
            st.lockSlice = (locks & 1) != 0;
            st.lockPitch = (locks & 2) != 0;
            st.lockFx    = (locks & 4) != 0;
            st.clamp();
        }
        return p;
    }
};

struct PatternSet
{
    std::array<Pattern, kNumPatterns> patterns;
};
} // namespace cf
