#pragma once
#include "SampleData.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace cf
{
/** Evenly spaced markers. */
inline std::vector<int> gridMarkers (int numFrames, int count)
{
    std::vector<int> m;
    count = juce::jlimit (1, kMaxSlices, count);
    for (int i = 0; i < count; ++i)
        m.push_back ((int) ((double) numFrames * i / count));
    return m;
}

/** Simple energy-flux onset detector. sensitivity 0..1 (higher = more slices). */
inline std::vector<int> detectTransients (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                          float sensitivity, int maxSlices = kMaxSlices)
{
    const int numFrames = buffer.getNumSamples();
    std::vector<int> result { 0 };
    if (numFrames <= 0) return result;

    const int hop = 256;
    const int numFrames_ = juce::jmax (1, numFrames / hop);
    std::vector<float> energy ((size_t) numFrames_, 0.0f);

    for (int f = 0; f < numFrames_; ++f)
    {
        double acc = 0.0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* d = buffer.getReadPointer (ch, f * hop);
            for (int i = 0; i < hop; ++i) acc += (double) d[i] * d[i];
        }
        energy[(size_t) f] = std::log10 (1e-6f + (float) (acc / (hop * juce::jmax (1, buffer.getNumChannels()))));
    }

    // onset strength = positive change of log-energy against a short running history
    std::vector<float> onset ((size_t) numFrames_, 0.0f);
    for (int f = 1; f < numFrames_; ++f)
    {
        float prev = -100.0f;
        for (int k = juce::jmax (0, f - 3); k < f; ++k) prev = juce::jmax (prev, energy[(size_t) k]);
        onset[(size_t) f] = juce::jmax (0.0f, energy[(size_t) f] - prev);
    }

    float mean = 0.0f;
    for (auto v : onset) mean += v;
    mean /= (float) onset.size();
    float var = 0.0f;
    for (auto v : onset) var += (v - mean) * (v - mean);
    const float sd = std::sqrt (var / (float) onset.size());

    sensitivity = juce::jlimit (0.0f, 1.0f, sensitivity);
    const float threshold = mean + (0.2f + (1.0f - sensitivity) * 2.5f) * sd + 0.05f;
    const int minGapFrames = juce::jmax (1, (int) std::round (sampleRate * (0.02 + 0.08 * (1.0 - sensitivity)) / hop));

    struct Peak { int frame; float strength; };
    std::vector<Peak> peaks;
    for (int f = 1; f < numFrames_ - 1; ++f)
    {
        const float v = onset[(size_t) f];
        if (v > threshold && v >= onset[(size_t) f - 1] && v >= onset[(size_t) f + 1])
        {
            if (! peaks.empty() && f - peaks.back().frame < minGapFrames)
            {
                if (v > peaks.back().strength) peaks.back() = { f, v };
                continue;
            }
            peaks.push_back ({ f, v });
        }
    }

    // keep the strongest ones if there are too many
    maxSlices = juce::jlimit (1, kMaxSlices, maxSlices);
    if ((int) peaks.size() > maxSlices - 1)
    {
        std::sort (peaks.begin(), peaks.end(), [] (const Peak& a, const Peak& b) { return a.strength > b.strength; });
        peaks.resize ((size_t) (maxSlices - 1));
    }
    for (auto& p : peaks)
    {
        // snap slightly before the frame so that the attack is kept
        const int pos = juce::jmax (0, p.frame * hop - hop / 4);
        if (pos > hop) result.push_back (pos);
    }
    std::sort (result.begin(), result.end());
    result.erase (std::unique (result.begin(), result.end()), result.end());
    return result;
}
} // namespace cf
