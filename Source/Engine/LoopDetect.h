#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace kerf
{
/** Guesses how many 4/4 bars a loop spans, preferring tempos near the host tempo. */
inline int detectLoopBars (double seconds, double hostBpm = 120.0)
{
    if (seconds <= 0.05) return 1;
    int best = 1;
    double bestScore = 1e9;
    for (int bars : { 1, 2, 4, 8, 16, 32 })
    {
        const double bpm = bars * 4.0 * 60.0 / seconds;
        if (bpm < 55.0 || bpm > 200.0) continue;
        // distance to the host tempo in octaves, small bonus for the common 60-180 range
        double score = std::abs (std::log2 (bpm / juce::jlimit (55.0, 200.0, hostBpm)));
        if (bpm < 65.0 || bpm > 185.0) score += 0.5;
        if (score < bestScore) { bestScore = score; best = bars; }
    }
    return best;
}

} // namespace kerf
