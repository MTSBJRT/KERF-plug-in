#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Parameters.h"
#include "Engine/Published.h"
#include "Engine/SampleData.h"
#include "Engine/Pattern.h"
#include "Engine/SliceEngine.h"

class KerfProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater
{
public:
    KerfProcessor();
    ~KerfProcessor() override;

    // ---------------------------------------------------------------- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                         { return true; }
    const juce::String getName() const override             { return JucePlugin_Name; }
    bool acceptsMidi() const override                       { return true; }
    bool producesMidi() const override                      { return false; }
    bool isMidiEffect() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 0.5; }
    int getNumPrograms() override                           { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------------------------------------------------------------- sample (message thread)
    std::shared_ptr<const kerf::SampleData> getSample() const      { return sample.peek(); }
    bool loadFile (const juce::File& file);
    void clearSample();
    void setMarkers (std::vector<int> markers, bool switchToManual = true);
    void reslice();                                              // re-apply the current slice mode
    void applyLoopLength();                                      // re-apply the Loop length parameter
    static constexpr double kMaxSampleSeconds = 60.0;

    // ---------------------------------------------------------------- patterns (message thread)
    int  getCurrentSlot() const;
    const kerf::Pattern& getCurrentPattern() const                { return patterns.patterns[(size_t) getCurrentSlot()]; }
    const kerf::PatternSet& getPatternSet() const                 { return patterns; }
    void snapshotForUndo();                                      // call before a user edit
    void setStep (int index, const kerf::StepData& step);
    void setPatternLength (int length);
    void randomize();
    void resetPattern();
    void setAllLocks (bool slice, bool pitch, bool fx, bool value);
    void undo();
    void redo();
    bool canUndo() const                                        { return ! undoStack.empty(); }
    bool canRedo() const                                        { return ! redoStack.empty(); }
    void copyPattern();
    bool pastePattern();
    bool hasClipboard() const                                   { return clipboard.has_value(); }
    bool exportMidi (const juce::File& file) const;

    // ---------------------------------------------------------------- capture
    enum class CaptureState { idle = 0, armed, recording, done };
    void requestCapture();
    void cancelCapture();
    CaptureState getCaptureState() const                        { return (CaptureState) captureState.load(); }
    float getCaptureProgress() const                            { return captureProgress.load(); }

    // ---------------------------------------------------------------- meters for the UI
    std::atomic<int>   currentStep  { -1 };
    std::atomic<int>   currentSlice { -1 };
    std::atomic<int>   activeVoices { 0 };
    std::atomic<float> hostBpm      { 120.0f };
    std::atomic<float> currentStretch { 1.0f };
    std::atomic<bool>  transportRunning { false };
    std::atomic<int>   sampleVersion { 0 };   // bumped whenever the sample or markers change
    std::atomic<int>   patternVersion { 0 };  // bumped whenever a pattern changes

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Event
    {
        int offset = 0;
        kerf::TriggerInfo info;
        int stepIndex = -1;
    };

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    void publishPatterns();
    void publishSample (std::shared_ptr<kerf::SampleData> s, bool fitGridToLoop);
    int  resolveLoopBars (double seconds) const;
    std::vector<int> computeMarkers (const juce::AudioBuffer<float>& audio, double rate, const std::vector<int>& existing) const;
    void finishCapture();

    void runClock (int numSamples, double ppqStart, double bpm, const kerf::Pattern& pattern);
    void scheduleStep (const kerf::StepData& step, int stepIndex, int offset, double stepSamples);
    void handleMidi (juce::MidiBuffer& midi);
    void addEvent (const Event& e);
    void renderEvents (juce::AudioBuffer<float>& wet, int numSamples, const std::shared_ptr<const kerf::SampleData>& smp);

    template <typename T> T* param (const char* paramId) const { return dynamic_cast<T*> (apvts.getParameter (paramId)); }
    float raw (const char* paramId) const                       { return apvts.getRawParameterValue (paramId)->load(); }

    // engine
    kerf::SliceEngine engine;
    kerf::Published<kerf::SampleData>  sample;
    kerf::Published<kerf::PatternSet>  publishedPatterns;
    juce::AudioBuffer<float> wetBuffer;
    juce::Random rng;

    // sequencer state (audio thread)
    std::array<Event, 512> events;
    int numEvents = 0;
    juce::int64 lastStepK = std::numeric_limits<juce::int64>::min();
    double freePpq = 0.0;
    double blockStretch = 1.0;
    bool wasPlaying = false;

    // capture
    juce::AudioBuffer<float> captureBuffer;
    std::atomic<int> captureState { 0 };
    std::atomic<float> captureProgress { 0.0f };
    int captureWritePos = 0, captureTarget = 0;
    double captureRate = 44100.0;

    // message-thread master data
    kerf::PatternSet patterns;
    bool patternsTouched = false;
    struct UndoEntry { int slot; kerf::Pattern pattern; };
    std::vector<UndoEntry> undoStack, redoStack;
    std::optional<kerf::Pattern> clipboard;
    std::atomic<bool> needsReslice { false };
    std::atomic<bool> needsLoopUpdate { false };
    std::atomic<bool> captureNeedsFinish { false };
    std::atomic<bool> randomizeRequested { false };
    std::atomic<bool> captureRequested { false };
    bool lastRandomizeTrig = false, lastCaptureTrig = false;
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KerfProcessor)
};
