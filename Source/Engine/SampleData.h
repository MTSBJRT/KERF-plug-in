#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

namespace kerf
{
constexpr int kMaxSlices = 64;

/** Immutable snapshot of the loaded audio plus its slice markers. */
struct SampleData
{
    std::shared_ptr<const juce::AudioBuffer<float>> audio; // always 2 channels
    double sourceSampleRate = 44100.0;
    std::vector<int> markers;                              // sorted slice start positions, markers[0] == 0
    juce::String name;

    int numFrames() const noexcept  { return audio != nullptr ? audio->getNumSamples() : 0; }
    int numSlices() const noexcept  { return (int) markers.size(); }

    juce::Range<int> sliceRange (int index) const noexcept
    {
        if (index < 0 || index >= numSlices()) return {};
        const int start = markers[(size_t) index];
        const int end   = index + 1 < numSlices() ? markers[(size_t) index + 1] : numFrames();
        return { start, juce::jmax (start, end) };
    }

    /** Returns a copy with a new set of markers (the audio buffer is shared). */
    std::shared_ptr<SampleData> withMarkers (std::vector<int> newMarkers) const
    {
        auto copy = std::make_shared<SampleData> (*this);
        copy->markers = sanitiseMarkers (std::move (newMarkers), numFrames());
        return copy;
    }

    static std::vector<int> sanitiseMarkers (std::vector<int> m, int numFrames)
    {
        for (auto& v : m) v = juce::jlimit (0, juce::jmax (0, numFrames - 1), v);
        std::sort (m.begin(), m.end());
        m.erase (std::unique (m.begin(), m.end()), m.end());
        if (m.empty() || m.front() != 0) m.insert (m.begin(), 0);
        if ((int) m.size() > kMaxSlices) m.resize ((size_t) kMaxSlices);
        return m;
    }

    /** Creates a stereo sample from any channel count. */
    static std::shared_ptr<SampleData> create (const juce::AudioBuffer<float>& source, double sampleRate,
                                               juce::String name, std::vector<int> markers)
    {
        auto buf = std::make_shared<juce::AudioBuffer<float>> (2, source.getNumSamples());
        buf->clear();
        if (source.getNumChannels() > 0)
        {
            for (int ch = 0; ch < 2; ++ch)
                buf->copyFrom (ch, 0, source, juce::jmin (ch, source.getNumChannels() - 1), 0, source.getNumSamples());
        }
        auto s = std::make_shared<SampleData>();
        s->audio = std::move (buf);
        s->sourceSampleRate = sampleRate;
        s->name = std::move (name);
        s->markers = sanitiseMarkers (std::move (markers), source.getNumSamples());
        return s;
    }
};
} // namespace kerf
