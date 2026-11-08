#pragma once
#include "SampleData.h"
#include "Pattern.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <cmath>

namespace kerf
{
enum class PitchMode { Varispeed = 0, Granular = 1 };

struct TriggerInfo
{
    StepData step;
    int   lengthSamples = -1;    // host samples before release, -1 = until slice end / note-off
    int   midiNote      = -1;
    float velocityGain  = 1.0f;
};

struct EngineSettings
{
    double    sampleRate   = 44100.0;
    float     declickMs    = 5.0f;
    PitchMode pitchMode    = PitchMode::Granular;
    bool      monophonic   = false;
    float     grainMs      = 60.0f;
};

/** A single playing slice. */
class Voice
{
public:
    void prepare (double sampleRate, int blockSize)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, blockSize), 2 };
        filter.prepare (spec);
        filter.setResonance (0.5f);
    }

    void start (const TriggerInfo& t, std::shared_ptr<const SampleData> smp, const EngineSettings& s, int ageCounter)
    {
        sample = std::move (smp);
        auto range = sample->sliceRange (t.step.slice);
        sliceStart = range.getStart();
        sliceLen   = (double) range.getLength();
        if (sliceLen < 2.0) { active = false; return; }

        info       = t;
        age        = ageCounter;
        granular   = s.pitchMode == PitchMode::Granular;
        pitchRatio = std::pow (2.0, (double) t.step.pitch / 12.0);
        srcRatio   = sample->sourceSampleRate / s.sampleRate;
        timePos    = 0.0;
        grainLen   = juce::jmax (64.0, s.grainMs * 0.001 * sample->sourceSampleRate);
        grainStart[0] = 0.0;
        grainStart[1] = -grainLen * 0.5;

        const float g = t.step.gain * t.velocityGain;
        const float p = juce::jlimit (-1.0f, 1.0f, t.step.pan);
        const float angle = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        gainL = g * std::cos (angle);
        gainR = g * std::sin (angle);

        remaining = t.lengthSamples;
        fadeSamples = juce::jmax (1, (int) std::round (s.declickMs * 0.001 * s.sampleRate));
        env     = 0.0f;
        envInc  = 1.0f / (float) fadeSamples;
        state   = State::attack;

        filterOn = std::abs (t.step.filter) > 0.02f;
        if (filterOn)
        {
            filter.reset();
            const float amt = std::abs (t.step.filter);
            if (t.step.filter < 0.0f)
            {
                filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                filter.setCutoffFrequency (juce::jmap (amt, 0.0f, 1.0f, 18000.0f, 180.0f));
            }
            else
            {
                filter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
                filter.setCutoffFrequency (juce::jmap (amt, 0.0f, 1.0f, 40.0f, 6000.0f));
            }
            filter.setCutoffFrequency (juce::jmin (filter.getCutoffFrequency(), (float) s.sampleRate * 0.45f));
        }
        active = true;
    }

    void release() noexcept
    {
        if (! active || state == State::release) return;
        state  = State::release;
        envInc = env / (float) fadeSamples;
        if (envInc <= 0.0f) { active = false; }
    }

    void render (juce::AudioBuffer<float>& out, int startSample, int numSamples)
    {
        if (! active) return;
        float* L = out.getWritePointer (0, startSample);
        float* R = out.getNumChannels() > 1 ? out.getWritePointer (1, startSample) : nullptr;
        const int totalFrames = sample->numFrames();
        const float* srcL = sample->audio->getReadPointer (0);
        const float* srcR = sample->audio->getReadPointer (1);

        const double step = granular ? srcRatio : srcRatio * pitchRatio;
        const double releaseLeadSrc = (double) fadeSamples * step;

        for (int i = 0; i < numSamples; ++i)
        {
            // ---- envelope
            if (state == State::attack)
            {
                env += envInc;
                if (env >= 1.0f) { env = 1.0f; state = State::sustain; }
            }
            else if (state == State::release)
            {
                env -= envInc;
                if (env <= 0.0f) { active = false; return; }
            }
            if (state != State::release)
            {
                if (remaining >= 0 && --remaining <= fadeSamples) release();
                else if (sliceLen - timePos <= releaseLeadSrc) release();
            }

            // ---- read
            float l = 0.0f, r = 0.0f;
            if (granular)
            {
                for (int g = 0; g < 2; ++g)
                {
                    double& gs = grainStart[g];
                    if (timePos - gs >= grainLen) gs = timePos;
                    const double phase = (timePos - gs) / grainLen;            // 0..1
                    const float  w     = 0.5f - 0.5f * std::cos ((float) phase * juce::MathConstants<float>::twoPi);
                    const double rp    = gs + (timePos - gs) * pitchRatio;
                    readInterp (srcL, srcR, totalFrames, rp, l, r, w);
                }
            }
            else
            {
                readInterp (srcL, srcR, totalFrames, timePos, l, r, 1.0f);
            }
            timePos += step;
            if (timePos >= sliceLen) { active = false; return; }

            if (filterOn)
            {
                l = filter.processSample (0, l);
                r = filter.processSample (1, r);
            }
            L[i] += l * gainL * env;
            if (R != nullptr) R[i] += r * gainR * env;
        }
    }

    bool isActive() const noexcept   { return active; }
    int  getAge() const noexcept     { return age; }
    int  getMidiNote() const noexcept{ return info.midiNote; }
    int  getSlice() const noexcept   { return info.step.slice; }

private:
    enum class State { attack, sustain, release };

    void readInterp (const float* srcL, const float* srcR, int totalFrames, double posInSlice,
                     float& l, float& r, float weight) const noexcept
    {
        double pos = info.step.reverse ? (sliceStart + sliceLen - 1.0 - posInSlice) : (sliceStart + posInSlice);
        pos = juce::jlimit (0.0, (double) totalFrames - 1.001, pos);
        const int   i0 = (int) pos;
        const float f  = (float) (pos - i0);
        l += weight * (srcL[i0] + f * (srcL[i0 + 1] - srcL[i0]));
        r += weight * (srcR[i0] + f * (srcR[i0 + 1] - srcR[i0]));
    }

    std::shared_ptr<const SampleData> sample;
    TriggerInfo info;
    bool   active = false, granular = true, filterOn = false;
    int    age = 0, sliceStart = 0, remaining = -1, fadeSamples = 1;
    double sliceLen = 0.0, timePos = 0.0, pitchRatio = 1.0, srcRatio = 1.0, grainLen = 2048.0;
    double grainStart[2] { 0.0, 0.0 };
    float  gainL = 1.0f, gainR = 1.0f, env = 0.0f, envInc = 0.0f;
    State  state = State::attack;
    juce::dsp::StateVariableTPTFilter<float> filter;
};

/** Pool of voices that render slices. */
class SliceEngine
{
public:
    static constexpr int kNumVoices = 16;

    void prepare (double sampleRate, int blockSize)
    {
        settings.sampleRate = sampleRate;
        for (auto& v : voices) v.prepare (sampleRate, blockSize);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v.release();
        for (auto& v : voices) v = Voice{};
        for (auto& v : voices) v.prepare (settings.sampleRate, 512);
    }

    void setSettings (const EngineSettings& s) noexcept
    {
        const double sr = settings.sampleRate;
        settings = s;
        settings.sampleRate = sr;
    }
    const EngineSettings& getSettings() const noexcept { return settings; }

    void trigger (const TriggerInfo& t, const std::shared_ptr<const SampleData>& sample)
    {
        if (sample == nullptr || t.step.slice < 0 || t.step.slice >= sample->numSlices()) return;
        if (settings.monophonic)
            for (auto& v : voices) v.release();

        Voice* chosen = nullptr;
        for (auto& v : voices) if (! v.isActive()) { chosen = &v; break; }
        if (chosen == nullptr)
        {
            chosen = &voices[0];
            for (auto& v : voices) if (v.getAge() < chosen->getAge()) chosen = &v;
        }
        chosen->start (t, sample, settings, ++ageCounter);
        lastSlice = t.step.slice;
    }

    void releaseNote (int midiNote)
    {
        for (auto& v : voices) if (v.isActive() && v.getMidiNote() == midiNote) v.release();
    }

    void releaseAll()
    {
        for (auto& v : voices) v.release();
    }

    void render (juce::AudioBuffer<float>& out, int startSample, int numSamples)
    {
        for (auto& v : voices) v.render (out, startSample, numSamples);
    }

    int getActiveVoiceCount() const noexcept
    {
        int n = 0;
        for (auto& v : voices) n += v.isActive() ? 1 : 0;
        return n;
    }
    int getLastSlice() const noexcept { return lastSlice; }

private:
    std::array<Voice, kNumVoices> voices;
    EngineSettings settings;
    int ageCounter = 0, lastSlice = -1;
};
} // namespace kerf
