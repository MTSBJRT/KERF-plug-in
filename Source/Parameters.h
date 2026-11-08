#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Randomizer.h"
#include "Engine/SampleData.h"

namespace kerf::id
{
inline constexpr const char* division      = "division";
inline constexpr const char* patternSlot   = "patternSlot";
inline constexpr const char* seqMode       = "seqMode";
inline constexpr const char* freeBpm       = "freeBpm";
inline constexpr const char* swing         = "swing";
inline constexpr const char* midiMode      = "midiMode";
inline constexpr const char* chromSlice    = "chromaticSlice";
inline constexpr const char* pitchMode     = "pitchMode";
inline constexpr const char* polyphony     = "polyphony";
inline constexpr const char* declick       = "declick";
inline constexpr const char* grain         = "grain";
inline constexpr const char* mix           = "mix";
inline constexpr const char* outGain       = "outGain";
inline constexpr const char* rndAmount     = "rndAmount";
inline constexpr const char* rndScale      = "rndScale";
inline constexpr const char* rndRoot       = "rndRoot";
inline constexpr const char* rndRange      = "rndRange";
inline constexpr const char* rndRest       = "rndRest";
inline constexpr const char* rndSlices     = "rndSlices";
inline constexpr const char* rndPitch      = "rndPitch";
inline constexpr const char* rndFx         = "rndFx";
inline constexpr const char* sliceMode     = "sliceMode";
inline constexpr const char* sliceCount    = "sliceCount";
inline constexpr const char* sensitivity   = "sensitivity";
inline constexpr const char* captureBars   = "captureBars";
}

namespace kerf
{
inline const juce::StringArray& divisionNames()
{
    static const juce::StringArray n { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    return n;
}
inline double divisionToBeats (int index)
{
    static const double beats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return beats[juce::jlimit (0, 5, index)];
}
inline const juce::StringArray& captureBarChoices()
{
    static const juce::StringArray n { "1 bar", "2 bars", "4 bars", "8 bars" };
    return n;
}
inline int captureBarsFromIndex (int i) { static const int b[] = { 1, 2, 4, 8 }; return b[juce::jlimit (0, 3, i)]; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::division, 1 }, "Division", divisionNames(), 3));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::patternSlot, 1 }, "Pattern", StringArray { "A", "B", "C", "D", "E", "F", "G", "H" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::seqMode, 1 }, "Sequencer", StringArray { "Host sync", "Free run", "Off (MIDI only)" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { id::freeBpm, 1 }, "Free BPM", NormalisableRange<float> (40.0f, 240.0f, 0.1f), 120.0f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { id::swing, 1 }, "Swing", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::midiMode, 1 }, "MIDI mode", StringArray { "Off", "One-shot", "Hold", "Chromatic" }, 1));
    p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { id::chromSlice, 1 }, "Chromatic slice", 1, kMaxSlices, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::pitchMode, 1 }, "Pitch mode", StringArray { "Varispeed", "Granular" }, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::polyphony, 1 }, "Voices", StringArray { "Poly", "Mono" }, 1));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { id::declick, 1 }, "Declick", NormalisableRange<float> (0.5f, 50.0f, 0.1f, 0.5f), 5.0f,
                                                         AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { id::grain, 1 }, "Grain", NormalisableRange<float> (20.0f, 150.0f, 1.0f), 60.0f,
                                                         AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { id::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { id::outGain, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                         AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { id::rndAmount, 1 }, "Random amount", NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::rndScale, 1 }, "Random scale", scaleNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::rndRoot, 1 }, "Root",
                                                         StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { id::rndRange, 1 }, "Pitch range", 0, 24, 12,
                                                         AudioParameterIntAttributes().withLabel ("st")));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { id::rndRest, 1 }, "Rest chance", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { id::rndSlices, 1 }, "Randomize slices", true));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { id::rndPitch, 1 }, "Randomize pitch", false));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { id::rndFx, 1 }, "Randomize FX", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::sliceMode, 1 }, "Slice mode", StringArray { "Grid", "Transients", "Manual" }, 0));
    p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { id::sliceCount, 1 }, "Slices", 1, kMaxSlices, 16));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { id::sensitivity, 1 }, "Sensitivity", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::captureBars, 1 }, "Capture length", captureBarChoices(), 1));
    return { p.begin(), p.end() };
}
} // namespace kerf
