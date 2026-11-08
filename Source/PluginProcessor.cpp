#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/TransientDetector.h"
#include "Engine/Randomizer.h"

using namespace kerf;

namespace
{
constexpr int kMaxUndo = 48;
} // namespace

KerfProcessor::KerfProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    for (auto& p : patterns.patterns) p.initDefault (16);
    publishPatterns();

    for (auto* idStr : { id::sliceMode, id::sliceCount, id::sensitivity, id::loopBars })
        apvts.addParameterListener (idStr, this);
}

KerfProcessor::~KerfProcessor()
{
    cancelPendingUpdate();
    for (auto* idStr : { id::sliceMode, id::sliceCount, id::sensitivity, id::loopBars })
        apvts.removeParameterListener (idStr, this);
}

// =============================================================================== lifecycle
void KerfProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    wetBuffer.setSize (2, juce::jmax (16, samplesPerBlock));
    captureRate = sampleRate;
    captureBuffer.setSize (2, (int) (sampleRate * 32.0));
    captureBuffer.clear();
    numEvents = 0;
    lastStepK = std::numeric_limits<juce::int64>::min();
    wasPlaying = false;
    if (getCaptureState() == CaptureState::recording) captureState.store ((int) CaptureState::idle);
}

void KerfProcessor::releaseResources() {}

bool KerfProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono() && ! in.isDisabled()) return false;
    return true;
}

// =============================================================================== processing
void KerfProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numOut     = buffer.getNumChannels();
    const double sr      = getSampleRate();
    if (numSamples == 0) return;

    // ---- engine settings from parameters
    EngineSettings es = engine.getSettings();
    es.declickMs  = raw (id::declick);
    es.grainMs    = raw (id::grain);
    es.pitchMode  = (PitchMode) juce::roundToInt (raw (id::pitchMode));
    es.monophonic = juce::roundToInt (raw (id::polyphony)) == 1;
    engine.setSettings (es);

    // ---- transport
    double bpm = raw (id::freeBpm);
    double ppq = 0.0;
    bool   hostInfo = false, playing = false;
    int    beatsPerBar = 4;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue() && *pos->getBpm() > 1.0) bpm = *pos->getBpm();
            if (pos->getPpqPosition().hasValue()) { ppq = *pos->getPpqPosition(); hostInfo = true; }
            playing = pos->getIsPlaying();
            if (auto ts = pos->getTimeSignature()) beatsPerBar = juce::jmax (1, ts->numerator * 4 / juce::jmax (1, ts->denominator));
        }
    }
    const int seqMode = juce::roundToInt (raw (id::seqMode)); // 0 host, 1 free, 2 off
    const bool useHostClock = seqMode == 0 && hostInfo;
    const bool clockRunning = seqMode == 0 ? (hostInfo ? playing : true) : seqMode == 1;
    if (! useHostClock) bpm = raw (id::freeBpm);
    hostBpm.store ((float) bpm);
    transportRunning.store (clockRunning);

    const double samplesPerBeat = 60.0 * sr / bpm;
    const double ppqStart = useHostClock ? ppq : freePpq;
    const double ppqEnd   = ppqStart + numSamples / samplesPerBeat;

    // ---- capture (input is recorded before anything else touches the buffer)
    {
        auto cs = getCaptureState();
        if (cs == CaptureState::armed)
        {
            int startOffset = 0;
            bool start = true;
            if (useHostClock && playing)
            {
                const double barLen = (double) beatsPerBar;
                const double nextBar = std::ceil (ppqStart / barLen - 1e-6) * barLen;
                if (nextBar < ppqEnd) startOffset = juce::jlimit (0, numSamples - 1, (int) std::round ((nextBar - ppqStart) * samplesPerBeat));
                else start = false;
            }
            if (start)
            {
                const int bars = captureBarsFromIndex (juce::roundToInt (raw (id::captureBars)));
                captureTarget = juce::jlimit (1, captureBuffer.getNumSamples(), (int) std::round (bars * beatsPerBar * samplesPerBeat));
                captureWritePos = 0;
                captureState.store ((int) CaptureState::recording);
                cs = CaptureState::recording;
                // record the tail of this block
                const int n = juce::jmin (numSamples - startOffset, captureTarget);
                for (int ch = 0; ch < 2; ++ch)
                    captureBuffer.copyFrom (ch, 0, buffer, juce::jmin (ch, buffer.getNumChannels() - 1), startOffset, n);
                captureWritePos = n;
            }
        }
        else if (cs == CaptureState::recording)
        {
            const int n = juce::jmin (numSamples, captureTarget - captureWritePos);
            if (n > 0)
                for (int ch = 0; ch < 2; ++ch)
                    captureBuffer.copyFrom (ch, captureWritePos, buffer, juce::jmin (ch, buffer.getNumChannels() - 1), 0, n);
            captureWritePos += juce::jmax (0, n);
        }
        if (cs == CaptureState::recording)
        {
            captureProgress.store ((float) captureWritePos / (float) juce::jmax (1, captureTarget));
            if (captureWritePos >= captureTarget)
            {
                captureState.store ((int) CaptureState::done);
                captureNeedsFinish.store (true);
                triggerAsyncUpdate();
            }
        }
    }

    // ---- momentary trigger parameters (rising edge)
    {
        const bool r = raw (id::randomizeTrig) > 0.5f, c = raw (id::captureTrig) > 0.5f;
        if (r && ! lastRandomizeTrig) { randomizeRequested.store (true); triggerAsyncUpdate(); }
        if (c && ! lastCaptureTrig)   { captureRequested.store (true);   triggerAsyncUpdate(); }
        lastRandomizeTrig = r;
        lastCaptureTrig = c;
    }

    // ---- snapshots for this block
    const auto& smp  = sample.get();
    const auto& pset = publishedPatterns.get();
    const int slot   = juce::jlimit (0, kNumPatterns - 1, juce::roundToInt (raw (id::patternSlot)));

    // ---- tempo sync: play the loop at the project tempo without changing its groove
    double stretch = 1.0;
    if (smp != nullptr && raw (id::tempoSync) > 0.5f && smp->loopBpm > 1.0)
        stretch = juce::jlimit (0.25, 4.0, bpm / smp->loopBpm);
    blockStretch = stretch;
    currentStretch.store ((float) stretch);

    // ---- MIDI
    handleMidi (midi);

    // ---- clock
    if (clockRunning && pset != nullptr)
    {
        if (! wasPlaying) lastStepK = std::numeric_limits<juce::int64>::min();
        runClock (numSamples, ppqStart, bpm, pset->patterns[(size_t) slot]);
        if (! useHostClock) freePpq = ppqEnd;
    }
    else if (wasPlaying)
    {
        engine.releaseAll();
        currentStep.store (-1);
        if (! useHostClock) freePpq = 0.0;
    }
    wasPlaying = clockRunning;

    // ---- render
    wetBuffer.clear();
    renderEvents (wetBuffer, numSamples, smp);

    const float mix  = raw (id::mix);
    const float gain = juce::Decibels::decibelsToGain (raw (id::outGain));
    for (int ch = 0; ch < numOut; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        const float* w = wetBuffer.getReadPointer (juce::jmin (ch, 1));
        for (int i = 0; i < numSamples; ++i)
            d[i] = (d[i] * (1.0f - mix) + w[i] * mix) * gain;
    }

    activeVoices.store (engine.getActiveVoiceCount());
    currentSlice.store (engine.getLastSlice());
    midi.clear();
}

void KerfProcessor::handleMidi (juce::MidiBuffer& midi)
{
    const int mode = juce::roundToInt (raw (id::midiMode)); // 0 off, 1 one-shot, 2 hold, 3 chromatic
    if (mode == 0) return;
    const auto& smp = sample.get();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (smp == nullptr) continue;
            Event e;
            e.offset = meta.samplePosition;
            e.info.velocityGain = 0.25f + 0.75f * m.getFloatVelocity();
            e.info.lengthSamples = -1;
            e.info.stretch = blockStretch;
            e.stepIndex = -1;
            if (mode == 3)
            {
                e.info.step.slice = juce::jlimit (0, juce::jmax (0, smp->numSlices() - 1), juce::roundToInt (raw (id::chromSlice)) - 1);
                e.info.step.pitch = (float) juce::jlimit (-24, 24, m.getNoteNumber() - 60);
                e.info.midiNote   = m.getNoteNumber();
            }
            else
            {
                const int sliceIdx = m.getNoteNumber() - midiBaseFromIndex (juce::roundToInt (raw (id::midiBase)));
                if (sliceIdx < 0 || sliceIdx >= smp->numSlices()) continue;
                e.info.step.slice = sliceIdx;
                e.info.midiNote   = mode == 2 ? m.getNoteNumber() : -1;
            }
            addEvent (e);
        }
        else if (m.isNoteOff() && (mode == 2 || mode == 3))
        {
            engine.releaseNote (m.getNoteNumber());
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            engine.releaseAll();
        }
    }
}

void KerfProcessor::runClock (int numSamples, double ppqStart, double bpm, const Pattern& pattern)
{
    const double sr = getSampleRate();
    const double stepLen = divisionToBeats (juce::roundToInt (raw (id::division)));
    const double samplesPerBeat = 60.0 * sr / bpm;
    const double stepSamples = stepLen * samplesPerBeat;
    const double ppqEnd = ppqStart + numSamples / samplesPerBeat;
    const double swing = raw (id::swing);
    const int length = juce::jlimit (1, kMaxSteps, pattern.length);

    juce::int64 k = (juce::int64) std::ceil (ppqStart / stepLen - 1e-6);
    for (; (double) k * stepLen < ppqEnd - 1e-9; ++k)
    {
        if (k == lastStepK) continue;
        lastStepK = k;
        const double swingDelay = (k & 1) ? swing * stepLen / 3.0 : 0.0;
        const int offset = juce::jmax (0, (int) std::round (((double) k * stepLen + swingDelay - ppqStart) * samplesPerBeat));
        const int stepIndex = (int) (((k % length) + length) % length);
        scheduleStep (pattern.steps[(size_t) stepIndex], stepIndex, offset, stepSamples);
    }
}

void KerfProcessor::scheduleStep (const StepData& step, int stepIndex, int offset, double stepSamples)
{
    Event e;
    e.stepIndex = stepIndex;
    e.offset = offset;
    e.info.step = step;
    e.info.stretch = blockStretch;
    if (step.slice < 0 || rng.nextFloat() > step.probability)
    {
        e.info.step.slice = -1; // rest, still advances the step display
        addEvent (e);
        return;
    }
    const int repeats = juce::jlimit (1, 8, step.repeats);
    const double interval = stepSamples / repeats;
    for (int r = 0; r < repeats; ++r)
    {
        e.offset = offset + (int) std::round (interval * r);
        e.info.lengthSamples = juce::jmax (1, (int) std::round (interval * step.gate));
        e.stepIndex = r == 0 ? stepIndex : -1;
        addEvent (e);
    }
}

void KerfProcessor::addEvent (const Event& e)
{
    if (numEvents < (int) events.size()) events[(size_t) numEvents++] = e;
}

void KerfProcessor::renderEvents (juce::AudioBuffer<float>& wet, int numSamples, const std::shared_ptr<const SampleData>& smp)
{
    // insertion sort by offset (small array)
    for (int i = 1; i < numEvents; ++i)
    {
        Event tmp = events[(size_t) i];
        int j = i - 1;
        while (j >= 0 && events[(size_t) j].offset > tmp.offset) { events[(size_t) j + 1] = events[(size_t) j]; --j; }
        events[(size_t) j + 1] = tmp;
    }

    int pos = 0, consumed = 0;
    for (int i = 0; i < numEvents; ++i)
    {
        auto& e = events[(size_t) i];
        if (e.offset >= numSamples) break;
        const int at = juce::jmax (pos, e.offset);
        if (at > pos) { engine.render (wet, pos, at - pos); pos = at; }
        if (e.stepIndex >= 0) currentStep.store (e.stepIndex);
        if (e.info.step.slice >= 0) engine.trigger (e.info, smp);
        ++consumed;
    }
    if (pos < numSamples) engine.render (wet, pos, numSamples - pos);

    // keep the future events
    int kept = 0;
    for (int i = consumed; i < numEvents; ++i)
    {
        events[(size_t) kept] = events[(size_t) i];
        events[(size_t) kept].offset -= numSamples;
        ++kept;
    }
    numEvents = kept;
}

// =============================================================================== capture
void KerfProcessor::requestCapture()
{
    if (getCaptureState() == CaptureState::idle) captureState.store ((int) CaptureState::armed);
}

void KerfProcessor::cancelCapture()
{
    captureState.store ((int) CaptureState::idle);
    captureProgress.store (0.0f);
}

void KerfProcessor::finishCapture()
{
    if (getCaptureState() != CaptureState::done) return;
    const int n = juce::jmin (captureWritePos, captureBuffer.getNumSamples());
    if (n > 64)
    {
        juce::AudioBuffer<float> copy (2, n);
        for (int ch = 0; ch < 2; ++ch) copy.copyFrom (ch, 0, captureBuffer, ch, 0, n);
        auto s = SampleData::create (copy, captureRate, "Capture " + juce::Time::getCurrentTime().toString (false, true, false),
                                     computeMarkers (copy, captureRate, {}));
        publishSample (s, true);
        if (auto cur = sample.peek(); cur != nullptr && loopBarsFromIndex (juce::roundToInt (raw (id::loopBars))) == 0)
        {   // captured loops have a known length: trust it over the auto-detection
            sample.set (cur->withLoopBars (captureBarsFromIndex (juce::roundToInt (raw (id::captureBars)))));
            sampleVersion.fetch_add (1);
        }
    }
    captureState.store ((int) CaptureState::idle);
    captureProgress.store (0.0f);
}

// =============================================================================== sample management
std::vector<int> KerfProcessor::computeMarkers (const juce::AudioBuffer<float>& audio, double rate, const std::vector<int>& existing) const
{
    const int mode  = juce::roundToInt (raw (id::sliceMode));
    const int count = juce::roundToInt (raw (id::sliceCount));
    if (mode == 0) return gridMarkers (audio.getNumSamples(), count);
    if (mode == 1) return detectTransients (audio, rate, raw (id::sensitivity), count);
    return existing.empty() ? gridMarkers (audio.getNumSamples(), count) : existing;
}

void KerfProcessor::publishSample (std::shared_ptr<SampleData> s, bool fitGridToLoop)
{
    if (s != nullptr)
    {
        s->setLoopBars (resolveLoopBars (s->lengthSeconds()));

        // Imported loop: make one slice = one grid step so that the default pattern plays the
        // loop exactly as recorded, in sync with the project tempo.
        if (fitGridToLoop && juce::roundToInt (raw (id::sliceMode)) == 0)
        {
            int slices = s->loopBars * 16, divisionIndex = 3;             // 1/16
            if (slices > kMaxSlices) { slices = s->loopBars * 8; divisionIndex = 1; }   // 1/8
            if (slices > kMaxSlices) { slices = s->loopBars * 4; divisionIndex = 0; }   // 1/4
            slices = juce::jlimit (1, kMaxSlices, slices);
            if (auto* p = param<juce::AudioParameterInt> (id::sliceCount)) { p->beginChangeGesture(); *p = slices; p->endChangeGesture(); }
            if (auto* p = param<juce::AudioParameterChoice> (id::division)) { p->beginChangeGesture(); *p = divisionIndex; p->endChangeGesture(); }
            s->markers = gridMarkers (s->numFrames(), slices);
            if (! patternsTouched)
                for (auto& p : patterns.patterns) { p.initDefault (slices); p.length = slices; }
            else
                patterns.patterns[(size_t) getCurrentSlot()].length = juce::jmax (1, slices);
            publishPatterns();
        }
        else if (! patternsTouched)
        {
            for (auto& p : patterns.patterns) p.initDefault (s->numSlices());
            publishPatterns();
        }
    }
    needsReslice.store (false); // the parameter changes above must not trigger a re-slice
    sample.set (std::move (s));
    sampleVersion.fetch_add (1);
}

bool KerfProcessor::loadFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int frames = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (kMaxSampleSeconds * reader->sampleRate));
    juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), frames);
    if (! reader->read (&buf, 0, frames, 0, true, true)) return false;
    publishSample (SampleData::create (buf, reader->sampleRate, file.getFileNameWithoutExtension(), computeMarkers (buf, reader->sampleRate, {})), true);
    return true;
}

void KerfProcessor::clearSample()
{
    sample.set (nullptr);
    sampleVersion.fetch_add (1);
}

void KerfProcessor::setMarkers (std::vector<int> markers, bool switchToManual)
{
    auto current = sample.peek();
    if (current == nullptr) return;
    if (switchToManual)
        if (auto* p = param<juce::AudioParameterChoice> (id::sliceMode); p != nullptr && p->getIndex() != 2)
        {
            p->beginChangeGesture(); *p = 2; p->endChangeGesture();
        }
    sample.set (current->withMarkers (std::move (markers)));
    sampleVersion.fetch_add (1);
}

void KerfProcessor::reslice()
{
    auto current = sample.peek();
    if (current == nullptr || juce::roundToInt (raw (id::sliceMode)) == 2) return;
    auto markers = computeMarkers (*current->audio, current->sourceSampleRate, current->markers);
    if (markers != current->markers)
    {
        sample.set (current->withMarkers (std::move (markers)));
        sampleVersion.fetch_add (1);
    }
}

void KerfProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == id::loopBars) needsLoopUpdate.store (true);
    else                             needsReslice.store (true);
    triggerAsyncUpdate();
}

void KerfProcessor::handleAsyncUpdate()
{
    if (captureNeedsFinish.exchange (false)) finishCapture();
    if (needsReslice.exchange (false)) reslice();
    if (needsLoopUpdate.exchange (false)) applyLoopLength();
    // momentary "button" parameters driven from a hardware controller or automation
    auto resetTrigger = [this] (const char* paramId)
    {
        if (auto* p = param<juce::AudioParameterBool> (paramId); p != nullptr && p->get())
        {
            p->beginChangeGesture(); *p = false; p->endChangeGesture();
        }
    };
    if (randomizeRequested.exchange (false)) { randomize(); resetTrigger (id::randomizeTrig); }
    if (captureRequested.exchange (false))   { requestCapture(); resetTrigger (id::captureTrig); }
}

int KerfProcessor::resolveLoopBars (double seconds) const
{
    const int forced = loopBarsFromIndex (juce::roundToInt (raw (id::loopBars)));
    return forced > 0 ? forced : detectLoopBars (seconds, (double) hostBpm.load());
}

void KerfProcessor::applyLoopLength()
{
    auto current = sample.peek();
    if (current == nullptr) return;
    const int bars = resolveLoopBars (current->lengthSeconds());
    if (bars != current->loopBars)
    {
        sample.set (current->withLoopBars (bars));
        sampleVersion.fetch_add (1);
    }
}

// =============================================================================== patterns
int KerfProcessor::getCurrentSlot() const
{
    return juce::jlimit (0, kNumPatterns - 1, juce::roundToInt (raw (id::patternSlot)));
}

void KerfProcessor::publishPatterns()
{
    publishedPatterns.set (std::make_shared<const PatternSet> (patterns));
    patternVersion.fetch_add (1);
}

void KerfProcessor::snapshotForUndo()
{
    undoStack.push_back ({ getCurrentSlot(), getCurrentPattern() });
    if ((int) undoStack.size() > kMaxUndo) undoStack.erase (undoStack.begin());
    redoStack.clear();
    patternsTouched = true;
}

void KerfProcessor::setStep (int index, const StepData& step)
{
    if (index < 0 || index >= kMaxSteps) return;
    auto s = step;
    s.clamp();
    auto& p = patterns.patterns[(size_t) getCurrentSlot()];
    if (p.steps[(size_t) index] == s) return;
    p.steps[(size_t) index] = s;
    patternsTouched = true;
    publishPatterns();
}

void KerfProcessor::setPatternLength (int length)
{
    auto& p = patterns.patterns[(size_t) getCurrentSlot()];
    length = juce::jlimit (1, kMaxSteps, length);
    if (p.length == length) return;
    p.length = length;
    patternsTouched = true;
    publishPatterns();
}

void KerfProcessor::randomize()
{
    auto smp = sample.peek();
    snapshotForUndo();
    RandomizeSettings s;
    s.amount     = raw (id::rndAmount);
    s.scale      = juce::roundToInt (raw (id::rndScale));
    s.root       = juce::roundToInt (raw (id::rndRoot));
    s.pitchRange = juce::roundToInt (raw (id::rndRange));
    s.restChance = raw (id::rndRest);
    s.slices     = raw (id::rndSlices) > 0.5f;
    s.pitch      = raw (id::rndPitch) > 0.5f;
    s.fx         = raw (id::rndFx) > 0.5f;
    juce::Random r;
    randomizePattern (patterns.patterns[(size_t) getCurrentSlot()], s, smp != nullptr ? smp->numSlices() : 16, r);
    publishPatterns();
}

void KerfProcessor::resetPattern()
{
    auto smp = sample.peek();
    snapshotForUndo();
    auto& p = patterns.patterns[(size_t) getCurrentSlot()];
    const int len = p.length;
    p.initDefault (smp != nullptr ? smp->numSlices() : 16);
    p.length = len;
    publishPatterns();
}

void KerfProcessor::setAllLocks (bool slice, bool pitch, bool fx, bool value)
{
    snapshotForUndo();
    for (auto& st : patterns.patterns[(size_t) getCurrentSlot()].steps)
    {
        if (slice) st.lockSlice = value;
        if (pitch) st.lockPitch = value;
        if (fx)    st.lockFx    = value;
    }
    publishPatterns();
}

void KerfProcessor::undo()
{
    if (undoStack.empty()) return;
    auto entry = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back ({ entry.slot, patterns.patterns[(size_t) entry.slot] });
    patterns.patterns[(size_t) entry.slot] = entry.pattern;
    publishPatterns();
}

void KerfProcessor::redo()
{
    if (redoStack.empty()) return;
    auto entry = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back ({ entry.slot, patterns.patterns[(size_t) entry.slot] });
    patterns.patterns[(size_t) entry.slot] = entry.pattern;
    publishPatterns();
}

void KerfProcessor::copyPattern()  { clipboard = getCurrentPattern(); }

bool KerfProcessor::pastePattern()
{
    if (! clipboard) return false;
    snapshotForUndo();
    patterns.patterns[(size_t) getCurrentSlot()] = *clipboard;
    publishPatterns();
    return true;
}

bool KerfProcessor::exportMidi (const juce::File& file) const
{
    const auto& p = getCurrentPattern();
    const int ticksPerQuarter = 960;
    const double stepTicks = divisionToBeats (juce::roundToInt (raw (id::division))) * ticksPerQuarter;
    const double bpm = hostBpm.load();

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60'000'000.0 / bpm)), 0.0);
    for (int i = 0; i < p.length; ++i)
    {
        const auto& st = p.steps[(size_t) i];
        if (st.slice < 0) continue;
        const int note = juce::jlimit (0, 127, midiBaseFromIndex (juce::roundToInt (raw (id::midiBase))) + st.slice);
        const int repeats = juce::jlimit (1, 8, st.repeats);
        const double interval = stepTicks / repeats;
        for (int r = 0; r < repeats; ++r)
        {
            const double start = i * stepTicks + r * interval;
            const double len = juce::jmax (1.0, interval * st.gate);
            const auto vel = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (st.gain * st.probability * 100.0f));
            seq.addEvent (juce::MidiMessage::noteOn (1, note, vel), start);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), start + len);
        }
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), p.length * stepTicks);
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ticksPerQuarter);
    mf.addTrack (seq);
    file.deleteFile();
    juce::FileOutputStream os (file);
    return os.openedOk() && mf.writeTo (os);
}

// =============================================================================== state
void KerfProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree data ("CF_DATA");
    data.setProperty ("version", 1, nullptr);

    juce::ValueTree pats ("PATTERNS");
    for (auto& p : patterns.patterns)
    {
        juce::ValueTree t ("P");
        t.setProperty ("d", p.toString(), nullptr);
        pats.appendChild (t, nullptr);
    }
    data.appendChild (pats, nullptr);

    if (auto smp = sample.peek(); smp != nullptr && smp->numFrames() > 0)
    {
        juce::ValueTree s ("SAMPLE");
        s.setProperty ("name", smp->name, nullptr);
        s.setProperty ("rate", smp->sourceSampleRate, nullptr);
        s.setProperty ("loopBars", smp->loopBars, nullptr);
        juce::StringArray m;
        for (int v : smp->markers) m.add (juce::String (v));
        s.setProperty ("markers", m.joinIntoString (","), nullptr);

        juce::TemporaryFile tmp (".flac");
        {
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::FileOutputStream> os (tmp.getFile().createOutputStream());
            if (os != nullptr)
            {
                std::unique_ptr<juce::AudioFormatWriter> writer (flac.createWriterFor (os.get(), smp->sourceSampleRate, 2, 24, {}, 5));
                if (writer != nullptr)
                {
                    os.release();
                    writer->writeFromAudioSampleBuffer (*smp->audio, 0, smp->numFrames());
                }
            }
        }
        juce::MemoryBlock blob;
        if (tmp.getFile().loadFileAsData (blob) && blob.getSize() > 0)
            s.setProperty ("flac", juce::Base64::toBase64 (blob.getData(), blob.getSize()), nullptr);
        data.appendChild (s, nullptr);
    }
    state.appendChild (data, nullptr);

    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void KerfProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;

    auto cfData = state.getChildWithName ("CF_DATA");
    if (cfData.isValid()) state.removeChild (cfData, nullptr);
    apvts.replaceState (state);

    if (! cfData.isValid()) return;
    auto pats = cfData.getChildWithName ("PATTERNS");
    if (pats.isValid())
    {
        for (int i = 0; i < juce::jmin (kNumPatterns, pats.getNumChildren()); ++i)
            patterns.patterns[(size_t) i] = Pattern::fromString (pats.getChild (i).getProperty ("d").toString());
        patternsTouched = true;
        undoStack.clear();
        redoStack.clear();
        publishPatterns();
    }

    auto s = cfData.getChildWithName ("SAMPLE");
    if (s.isValid())
    {
        juce::MemoryOutputStream decoded;
        if (juce::Base64::convertFromBase64 (decoded, s.getProperty ("flac").toString()) && decoded.getDataSize() > 0)
        {
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (
                new juce::MemoryInputStream (decoded.getData(), decoded.getDataSize(), false), true));
            if (reader != nullptr && reader->lengthInSamples > 0)
            {
                juce::AudioBuffer<float> buf ((int) reader->numChannels, (int) reader->lengthInSamples);
                reader->read (&buf, 0, (int) reader->lengthInSamples, 0, true, true);
                std::vector<int> markers;
                for (auto& tok : juce::StringArray::fromTokens (s.getProperty ("markers").toString(), ",", ""))
                    markers.push_back (tok.getIntValue());
                patternsTouched = true; // keep the restored patterns instead of re-initialising them
                auto restored = SampleData::create (buf, (double) s.getProperty ("rate", 44100.0), s.getProperty ("name").toString(), markers);
                restored->setLoopBars ((int) s.getProperty ("loopBars", 1));
                sample.set (restored);
                sampleVersion.fetch_add (1);
            }
        }
    }
}

juce::AudioProcessorEditor* KerfProcessor::createEditor() { return new KerfEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KerfProcessor(); }
