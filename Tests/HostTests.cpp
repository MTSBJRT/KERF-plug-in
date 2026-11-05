// Headless host test: drives the real AudioProcessor with a fake transport.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include <iostream>

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond << std::endl; } } while (false)

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

static float rms (const juce::AudioBuffer<float>& b)
{
    double acc = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i) acc += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return (float) std::sqrt (acc / juce::jmax (1, b.getNumSamples() * b.getNumChannels()));
}

static juce::File writeTestWav()
{
    const double sr = 44100.0;
    const int n = (int) sr * 2;
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
    {
        const float v = 0.4f * std::sin ((float) (juce::MathConstants<double>::twoPi * 220.0 * i / sr));
        b.setSample (0, i, v);
        b.setSample (1, i, v);
    }
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("chopforge_test.wav");
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (file), sr, 2, 16, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, n);
    return file;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init; // message manager for AsyncUpdater

    auto wav = writeTestWav();
    ChopForgeProcessor proc;
    FakePlayHead ph;
    proc.setPlayHead (&ph);
    const double sr = 48000.0;
    const int block = 512;
    proc.prepareToPlay (sr, block);
    proc.setRateAndBufferSizeDetails (sr, block);

    CHECK (proc.loadFile (wav));
    auto smp = proc.getSample();
    CHECK (smp != nullptr && smp->numSlices() == 16 && smp->numFrames() == 88200);

    // ---- sequencer follows the host: 1/16 at 120 bpm -> 8 steps per second
    juce::AudioBuffer<float> io (2, block);
    juce::MidiBuffer midi;
    int stepChanges = 0, lastStep = -1;
    float totalRms = 0.0f;
    const int blocks = (int) (sr * 2.0 / block);
    for (int i = 0; i < blocks; ++i)
    {
        io.clear();
        ph.ppq = i * block / sr * (ph.bpm / 60.0);
        proc.processBlock (io, midi);
        totalRms += rms (io);
        const int s = proc.currentStep.load();
        if (s != lastStep) { ++stepChanges; lastStep = s; }
    }
    CHECK (totalRms / blocks > 0.05f);
    CHECK (stepChanges >= 15 && stepChanges <= 17);
    CHECK (proc.transportRunning.load());

    // ---- stopped transport: output is the dry signal scaled by (1 - mix) = silence at mix 100 %
    ph.playing = false;
    for (int i = 0; i < 8; ++i) { io.clear(); proc.processBlock (io, midi); }
    io.clear();
    proc.processBlock (io, midi);
    CHECK (rms (io) < 1e-5f);
    CHECK (! proc.transportRunning.load());

    // ---- MIDI one-shot while stopped
    midi.addEvent (juce::MidiMessage::noteOn (1, 36 + 3, (juce::uint8) 100), 10);
    io.clear();
    proc.processBlock (io, midi);
    midi.clear();
    float midiRms = rms (io);
    for (int i = 0; i < 4; ++i) { io.clear(); proc.processBlock (io, midi); midiRms += rms (io); }
    CHECK (midiRms > 0.05f);
    CHECK (proc.currentSlice.load() == 3);

    // ---- pattern editing + undo
    proc.snapshotForUndo();
    auto st = proc.getCurrentPattern().steps[0];
    st.pitch = 7.0f; st.reverse = true;
    proc.setStep (0, st);
    CHECK (proc.getCurrentPattern().steps[0].reverse);
    proc.undo();
    CHECK (! proc.getCurrentPattern().steps[0].reverse);
    proc.redo();
    CHECK (proc.getCurrentPattern().steps[0].reverse);
    proc.setPatternLength (12);

    // ---- manual markers switch the slice mode to Manual
    auto markers = smp->markers;
    markers.push_back (1000);
    proc.setMarkers (markers);
    CHECK (proc.getSample()->numSlices() == 17);
    CHECK (juce::roundToInt (proc.apvts.getRawParameterValue ("sliceMode")->load()) == 2);

    // ---- state round trip (patterns + FLAC-encoded audio + markers)
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    CHECK (state.getSize() > 10000);

    ChopForgeProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    auto rs = restored.getSample();
    CHECK (rs != nullptr);
    if (rs != nullptr)
    {
        CHECK (rs->numFrames() == proc.getSample()->numFrames());
        CHECK (rs->markers == proc.getSample()->markers);
        CHECK (std::abs (rs->audio->getSample (0, 5000) - proc.getSample()->audio->getSample (0, 5000)) < 1e-3f);
    }
    CHECK (restored.getCurrentPattern() == proc.getCurrentPattern());
    CHECK (restored.getCurrentPattern().length == 12);

    // ---- MIDI export
    auto mid = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("chopforge_test.mid");
    CHECK (proc.exportMidi (mid));
    {
        juce::FileInputStream in (mid);
        juce::MidiFile mf;
        CHECK (mf.readFrom (in));
        CHECK (mf.getNumTracks() == 1);
        CHECK (mf.getTrack (0)->getNumEvents() > 12);
    }
    mid.deleteFile();
    wav.deleteFile();

    if (failures == 0) std::cout << "All host tests passed." << std::endl;
    else std::cerr << failures << " check(s) failed." << std::endl;
    return failures == 0 ? 0 : 1;
}
