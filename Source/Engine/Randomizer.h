#pragma once
#include "Pattern.h"
#include <vector>

namespace kerf
{
struct Scale
{
    const char* name;
    std::vector<int> degrees; // semitone offsets within the octave
};

inline const std::vector<Scale>& scales()
{
    static const std::vector<Scale> s {
        { "Chromatic",      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
        { "Octaves",        { 0 } },
        { "Fifths",         { 0, 7 } },
        { "Major triad",    { 0, 4, 7 } },
        { "Minor triad",    { 0, 3, 7 } },
        { "Major",          { 0, 2, 4, 5, 7, 9, 11 } },
        { "Minor",          { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorian",         { 0, 2, 3, 5, 7, 9, 10 } },
        { "Pentatonic maj", { 0, 2, 4, 7, 9 } },
        { "Pentatonic min", { 0, 3, 5, 7, 10 } },
        { "Blues",          { 0, 3, 5, 6, 7, 10 } },
        { "Whole tone",     { 0, 2, 4, 6, 8, 10 } },
    };
    return s;
}

inline juce::StringArray scaleNames()
{
    juce::StringArray names;
    for (auto& s : scales()) names.add (s.name);
    return names;
}

/** Snaps a semitone offset to the nearest note of (root + scale). */
inline int snapToScale (int semitones, int scaleIndex, int root)
{
    const auto& sc = scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, scaleIndex)];
    int best = semitones, bestDist = 1000;
    for (int octave = -3; octave <= 3; ++octave)
        for (int d : sc.degrees)
        {
            const int candidate = root + d + octave * 12;
            const int dist = std::abs (candidate - semitones);
            if (dist < bestDist) { bestDist = dist; best = candidate; }
        }
    return best;
}

struct RandomizeSettings
{
    float amount      = 1.0f;  // fraction of (unlocked) steps that are affected
    int   scale       = 0;
    int   root        = 0;     // 0 = C
    int   pitchRange  = 12;    // semitones up and down
    bool  slices      = true;
    bool  pitch       = true;
    bool  fx          = true;
    float restChance  = 0.0f;  // chance that a step becomes a rest
};

/** Randomises a pattern while honouring the per-step locks. */
inline void randomizePattern (Pattern& p, const RandomizeSettings& s, int numSlices, juce::Random& rng)
{
    const int slicesAvailable = juce::jmax (1, numSlices);
    for (int i = 0; i < p.length; ++i)
    {
        auto& st = p.steps[(size_t) i];
        if (rng.nextFloat() > s.amount) continue;

        if (s.slices && ! st.lockSlice)
        {
            if (rng.nextFloat() < s.restChance) st.slice = -1;
            else                                st.slice = rng.nextInt (slicesAvailable);
        }

        if (s.pitch && ! st.lockPitch)
        {
            const int range = juce::jmax (0, s.pitchRange);
            const int raw   = rng.nextInt (juce::Range<int> (-range, range + 1));
            st.pitch = (float) juce::jlimit (-24, 24, snapToScale (raw, s.scale, s.root));
        }

        if (s.fx && ! st.lockFx)
        {
            st.reverse = rng.nextFloat() < 0.25f;
            st.filter  = rng.nextFloat() < 0.35f ? rng.nextFloat() * 2.0f - 1.0f : 0.0f;
            const float r = rng.nextFloat();
            st.repeats = r < 0.80f ? 1 : (r < 0.92f ? 2 : (r < 0.97f ? 4 : 3));
            st.gate    = rng.nextFloat() < 0.5f ? 1.0f : 0.3f + rng.nextFloat() * 0.7f;
            st.gain    = 0.75f + rng.nextFloat() * 0.35f;
            st.pan     = (rng.nextFloat() * 2.0f - 1.0f) * 0.4f;
        }
        st.clamp();
    }
}
} // namespace kerf
