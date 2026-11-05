// Headless tests for the ChopForge engine (no GUI, no host).
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Engine/Pattern.h"
#include "Engine/Randomizer.h"
#include "Engine/TransientDetector.h"
#include "Engine/SliceEngine.h"
#include <iostream>

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond << std::endl; } } while (false)

using namespace cf;

static std::shared_ptr<SampleData> makeSineSample (double sr, double seconds, int slices, double freq = 440.0)
{
    const int n = (int) (sr * seconds);
    juce::AudioBuffer<float> b (1, n);
    for (int i = 0; i < n; ++i) b.setSample (0, i, 0.5f * std::sin ((float) (juce::MathConstants<double>::twoPi * freq * i / sr)));
    return SampleData::create (b, sr, "sine", gridMarkers (n, slices));
}

static float rms (const juce::AudioBuffer<float>& b, int start, int num)
{
    double acc = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = start; i < start + num; ++i) acc += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return (float) std::sqrt (acc / juce::jmax (1, num * b.getNumChannels()));
}

static void testPatternSerialisation()
{
    Pattern p;
    p.initDefault (8);
    p.length = 12;
    p.steps[3].pitch = -7.0f; p.steps[3].reverse = true; p.steps[3].lockPitch = true; p.steps[3].repeats = 4;
    p.steps[5].slice = -1; p.steps[5].filter = -0.5f; p.steps[5].gate = 0.25f; p.steps[5].lockSlice = true; p.steps[5].lockFx = true;
    auto q = Pattern::fromString (p.toString());
    CHECK (q == p);
    CHECK (Pattern::fromString ("garbage").length == 16);
}

static void testMarkers()
{
    auto g = gridMarkers (1000, 4);
    CHECK (g.size() == 4 && g[0] == 0 && g[1] == 250 && g[3] == 750);
    auto s = SampleData::sanitiseMarkers ({ 500, 10, 10, -3, 2000 }, 1000);
    CHECK (s.size() == 4 && s[0] == 0 && s[1] == 10 && s[2] == 500 && s[3] == 999);

    auto smp = makeSineSample (44100.0, 1.0, 4);
    CHECK (smp->numSlices() == 4);
    CHECK (smp->sliceRange (3).getEnd() == smp->numFrames());
    CHECK (smp->sliceRange (0).getLength() == smp->numFrames() / 4);
}

static void testTransientDetection()
{
    const double sr = 44100.0;
    const int n = (int) sr * 2;
    juce::AudioBuffer<float> b (1, n);
    b.clear();
    juce::Random r (1);
    const int hits[] = { 0, 22050, 44100, 66150 };
    for (int h : hits)
        for (int i = 0; i < 6000; ++i)
            b.setSample (0, h + i, (r.nextFloat() * 2.0f - 1.0f) * std::exp (-i / 1500.0f));
    auto m = detectTransients (b, sr, 0.5f);
    CHECK (m.size() == 4);
    for (size_t i = 1; i < m.size() && i < 4; ++i)
        CHECK (std::abs (m[i] - hits[i]) < 1024);
}

static void testScaleSnapping()
{
    CHECK (snapToScale (0, 1, 0) == 0);     // octaves
    CHECK (snapToScale (5, 1, 0) == 0 || snapToScale (5, 1, 0) == 12);
    CHECK (snapToScale (6, 2, 0) == 7);     // fifths -> 7
    CHECK (snapToScale (1, 5, 0) == 0 || snapToScale (1, 5, 0) == 2); // major
    CHECK (snapToScale (3, 3, 0) == 4);     // major triad
    CHECK (snapToScale (-11, 1, 0) == -12);
}

static void testRandomizerLocks()
{
    Pattern p;
    p.initDefault (8);
    for (int i = 0; i < p.length; ++i)
    {
        p.steps[(size_t) i].lockSlice = (i % 2 == 0);
        p.steps[(size_t) i].lockPitch = true;
        p.steps[(size_t) i].lockFx = (i < 8);
    }
    auto before = p;
    RandomizeSettings s;
    s.amount = 1.0f; s.pitch = true; s.slices = true; s.fx = true; s.pitchRange = 12;
    juce::Random r (42);
    randomizePattern (p, s, 8, r);
    for (int i = 0; i < p.length; ++i)
    {
        const auto& a = before.steps[(size_t) i];
        const auto& b = p.steps[(size_t) i];
        if (a.lockSlice) CHECK (a.slice == b.slice);
        CHECK (juce::exactlyEqual (a.pitch, b.pitch));   // every pitch locked
        if (a.lockFx) CHECK (a.reverse == b.reverse && juce::exactlyEqual (a.filter, b.filter) && a.repeats == b.repeats);
        CHECK (b.slice >= -1 && b.slice < 8);
    }
    // with scale snapping the pitches stay on the scale
    Pattern q; q.initDefault (4);
    RandomizeSettings s2; s2.scale = 2; s2.root = 0; s2.pitchRange = 12;
    randomizePattern (q, s2, 4, r);
    for (int i = 0; i < q.length; ++i)
    {
        const int pc = ((int) q.steps[(size_t) i].pitch % 12 + 12) % 12;
        CHECK (pc == 0 || pc == 7);
    }
}

static void testEngine()
{
    const double sr = 48000.0;
    const int block = 256;
    auto smp = makeSineSample (sr, 1.0, 4);      // 4 slices of 0.25 s
    const int sliceLen = smp->sliceRange (0).getLength();

    SliceEngine engine;
    engine.prepare (sr, block);
    EngineSettings es = engine.getSettings();
    es.declickMs = 5.0f;
    es.pitchMode = PitchMode::Granular;
    engine.setSettings (es);

    TriggerInfo t;
    t.step.slice = 1;
    t.step.pitch = 12.0f;
    engine.trigger (t, smp);
    CHECK (engine.getActiveVoiceCount() == 1);

    juce::AudioBuffer<float> out (2, sliceLen + 4 * block);
    out.clear();
    int rendered = 0;
    while (rendered < out.getNumSamples()) { engine.render (out, rendered, block); rendered += block; }

    CHECK (std::abs (out.getSample (0, 0)) < 0.01f);          // declick fade-in
    CHECK (rms (out, 1000, 4000) > 0.2f);                      // audible in the middle
    CHECK (rms (out, sliceLen + 2 * block, block) < 1e-4f);    // silent after the slice (duration preserved)
    CHECK (rms (out, sliceLen / 2, 2000) > 0.2f);              // still playing at 50 % (granular keeps length)
    CHECK (engine.getActiveVoiceCount() == 0);

    // varispeed +12 -> plays twice as fast, so silence after ~half the slice
    es.pitchMode = PitchMode::Varispeed;
    engine.setSettings (es);
    t.step.pitch = 12.0f;
    engine.trigger (t, smp);
    out.clear();
    rendered = 0;
    while (rendered < out.getNumSamples()) { engine.render (out, rendered, block); rendered += block; }
    CHECK (rms (out, 1000, 2000) > 0.2f);
    CHECK (rms (out, sliceLen * 3 / 4, 2000) < 1e-4f);

    // reverse + filter + gate on a rest slice index out of range must be ignored
    t.step.slice = 99;
    engine.trigger (t, smp);
    CHECK (engine.getActiveVoiceCount() == 0);

    // gate shorter than the slice
    t.step.slice = 0; t.step.pitch = 0.0f; t.step.reverse = true; t.step.filter = -0.6f;
    t.lengthSamples = 2000;
    engine.trigger (t, smp);
    out.clear();
    rendered = 0;
    while (rendered < out.getNumSamples()) { engine.render (out, rendered, block); rendered += block; }
    CHECK (rms (out, 200, 1000) > 0.1f);
    CHECK (rms (out, 4000, 2000) < 1e-4f);

    // monophonic choke: a new trigger releases the previous voice
    es.monophonic = true;
    engine.setSettings (es);
    t.lengthSamples = -1; t.step.reverse = false; t.step.filter = 0.0f;
    engine.trigger (t, smp);
    t.step.slice = 2;
    engine.trigger (t, smp);
    out.clear();
    engine.render (out, 0, 4096);
    CHECK (engine.getActiveVoiceCount() == 1);

    // many triggers never exceed the voice pool
    es.monophonic = false;
    engine.setSettings (es);
    for (int i = 0; i < 40; ++i) { t.step.slice = i % 4; engine.trigger (t, smp); }
    CHECK (engine.getActiveVoiceCount() <= SliceEngine::kNumVoices);
    engine.render (out, 0, 4096);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4096; ++i)
            CHECK (std::isfinite (out.getSample (ch, i)));
}

int main()
{
    testPatternSerialisation();
    testMarkers();
    testTransientDetection();
    testScaleSnapping();
    testRandomizerLocks();
    testEngine();
    if (failures == 0) std::cout << "All engine tests passed." << std::endl;
    else std::cerr << failures << " check(s) failed." << std::endl;
    return failures == 0 ? 0 : 1;
}
