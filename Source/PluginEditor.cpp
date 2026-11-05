#include "PluginEditor.h"

using namespace cf;
using namespace cf::ui;

ChopForgeEditor::ChopForgeEditor (ChopForgeProcessor& p)
    : AudioProcessorEditor (&p), processor (p), waveform (p), grid (p)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    // ------------------------------------------------------------ header
    titleLabel.setText ("ChopForge", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, colours::accent);
    addAndMakeVisible (titleLabel);

    sampleLabel.setFont (juce::FontOptions (12.0f));
    sampleLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (sampleLabel);

    bpmLabel.setFont (juce::FontOptions (12.0f));
    bpmLabel.setColour (juce::Label::textColourId, colours::textDim);
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    makeButton (captureButton, "CAPTURE", "Record the live input for the chosen number of bars (starts on the next bar when the host is playing)", [this]
    {
        if (processor.getCaptureState() == ChopForgeProcessor::CaptureState::idle) processor.requestCapture();
        else processor.cancelCapture();
    });
    makeButton (loadButton, "LOAD", "Load an audio file (or drop one onto the window)", [this] { openLoadDialog(); });
    makeButton (clearButton, "CLEAR", "Remove the loaded audio", [this] { processor.clearSample(); });
    makeButton (exportButton, "MIDI", "Export the current pattern as a MIDI file (one note per slice, C1 = slice 1)", [this] { openExportDialog(); });

    makeCombo (captureBars, id::captureBars, "Capture", "Length of the live capture");
    makeCombo (sliceMode, id::sliceMode, "Slicing", "Grid: equal slices. Transients: detect attacks. Manual: edit markers on the waveform");
    makeKnob (sliceCount, id::sliceCount, "Slices", "Number of grid slices (or maximum for transient detection)");
    makeKnob (sensitivity, id::sensitivity, "Sens", "Transient detection sensitivity");

    // ------------------------------------------------------------ main
    addAndMakeVisible (waveform);
    addAndMakeVisible (grid);

    // ------------------------------------------------------------ sequence panel
    auto makeTitle = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (l);
    };
    makeTitle (sequenceTitle, "SEQUENCE");
    makeTitle (randomTitle, "RANDOMIZE");
    makeTitle (outputTitle, "OUTPUT & MIDI");

    makeCombo (division, id::division, "Step", "Step length");
    makeCombo (seqMode, id::seqMode, "Clock", "Host sync follows the DAW transport; Free run always plays; Off plays slices from MIDI only");
    makeKnob (swing, id::swing, "Swing", "Delays every other step");
    makeKnob (freeBpm, id::freeBpm, "BPM", "Tempo used in Free run mode (or when the host gives no tempo)");

    lengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lengthSlider.setRange (1, kMaxSteps, 1);
    lengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 18);
    lengthSlider.setTooltip ("Number of steps in the pattern");
    lengthSlider.onValueChange = [this] { processor.setPatternLength ((int) lengthSlider.getValue()); };
    lengthSlider.onDragStart = [this] { processor.snapshotForUndo(); };
    addAndMakeVisible (lengthSlider);
    lengthLabel.setText ("Steps", juce::dontSendNotification);
    lengthLabel.setFont (juce::FontOptions (11.0f));
    lengthLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (lengthLabel);

    patternLabel.setText ("Pattern", juce::dontSendNotification);
    patternLabel.setFont (juce::FontOptions (11.0f));
    patternLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (patternLabel);
    for (int i = 0; i < kNumPatterns; ++i)
    {
        auto& b = slotButtons[(size_t) i];
        b.setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + i)));
        b.setClickingTogglesState (false);
        b.setTooltip ("Select pattern " + b.getButtonText());
        b.onClick = [this, i] { setSlot (i); };
        addAndMakeVisible (b);
    }
    makeButton (copyButton, "COPY", "Copy the current pattern", [this] { processor.copyPattern(); });
    makeButton (pasteButton, "PASTE", "Paste into the current pattern", [this] { processor.pastePattern(); });

    // ------------------------------------------------------------ random panel
    makeKnob (rndAmount, id::rndAmount, "Amount", "Share of unlocked steps that get randomized");
    makeKnob (rndRange, id::rndRange, "Range", "Pitch range in semitones");
    makeKnob (rndRest, id::rndRest, "Rests", "Chance that a step becomes silent");
    makeCombo (rndScale, id::rndScale, "Scale", "Random pitches snap to this scale");
    makeCombo (rndRoot, id::rndRoot, "Root");
    auto makeToggle = [this] (juce::ToggleButton& t, std::unique_ptr<ButtonAttachment>& att, const char* paramId, const juce::String& text)
    {
        t.setButtonText (text);
        att = std::make_unique<ButtonAttachment> (processor.apvts, paramId, t);
        addAndMakeVisible (t);
    };
    makeToggle (rndSlices, rndSlicesAtt, id::rndSlices, "Slices");
    makeToggle (rndPitch, rndPitchAtt, id::rndPitch, "Pitch");
    makeToggle (rndFx, rndFxAtt, id::rndFx, "FX");
    makeButton (randomizeButton, "RANDOMIZE", "Randomize the unlocked steps (shortcut: R)", [this] { processor.randomize(); });
    randomizeButton.setColour (juce::TextButton::buttonColourId, colours::accent.withAlpha (0.85f));
    randomizeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    makeButton (undoButton, "UNDO", "Undo the last pattern change (Ctrl+Z)", [this] { processor.undo(); });
    makeButton (redoButton, "REDO", "Redo (Ctrl+Shift+Z)", [this] { processor.redo(); });
    makeButton (resetButton, "RESET", "Reset the pattern to play the slices in order", [this] { processor.resetPattern(); });
    makeButton (lockAllButton, "LOCK ALL", "Lock every step", [this] { processor.setAllLocks (true, true, true, true); });
    makeButton (unlockAllButton, "UNLOCK", "Unlock every step", [this] { processor.setAllLocks (true, true, true, false); });

    // ------------------------------------------------------------ output panel
    makeKnob (mix, id::mix, "Mix", "Dry / wet balance");
    makeKnob (outGain, id::outGain, "Output", "Output level");
    makeKnob (declick, id::declick, "Declick", "Fade in/out applied to every slice");
    makeKnob (grain, id::grain, "Grain", "Grain size for the length-preserving pitch mode");
    makeCombo (pitchMode, id::pitchMode, "Pitch", "Varispeed changes the length with the pitch; Granular keeps the slice length");
    makeCombo (polyphony, id::polyphony, "Voices", "Mono chokes the previous slice when a new one starts");
    makeCombo (midiMode, id::midiMode, "MIDI", "One-shot/hold: C1 and up play slices. Chromatic: notes pitch the chosen slice around C3");
    makeKnob (chromSlice, id::chromSlice, "Chrom", "Slice played in chromatic MIDI mode");

    if (auto autoload = juce::SystemStats::getEnvironmentVariable ("CHOPFORGE_AUTOLOAD", {}); autoload.isNotEmpty() && processor.getSample() == nullptr)
        processor.loadFile (juce::File (autoload));

    setResizable (true, true);
    setResizeLimits (940, 600, 1920, 1200);
    setSize (1060, 700);
    startTimerHz (30);
}

ChopForgeEditor::~ChopForgeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// =============================================================================== helpers
void ChopForgeEditor::makeKnob (Knob& k, const char* paramId, const juce::String& text, const juce::String& tooltip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    k.slider.setTooltip (tooltip);
    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
    addAndMakeVisible (k.slider);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setFont (juce::FontOptions (11.0f));
    k.label.setColour (juce::Label::textColourId, colours::textDim);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);
}

void ChopForgeEditor::makeCombo (Combo& c, const char* paramId, const juce::String& text, const juce::String& tooltip)
{
    c.box.setTooltip (tooltip);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramId)))
        c.box.addItemList (choice->choices, 1);
    c.attachment = std::make_unique<ComboAttachment> (processor.apvts, paramId, c.box);
    addAndMakeVisible (c.box);
    c.label.setText (text, juce::dontSendNotification);
    c.label.setFont (juce::FontOptions (11.0f));
    c.label.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (c.label);
}

void ChopForgeEditor::makeButton (juce::TextButton& b, const juce::String& text, const juce::String& tooltip, std::function<void()> onClick)
{
    b.setButtonText (text);
    b.setTooltip (tooltip);
    b.onClick = std::move (onClick);
    addAndMakeVisible (b);
}

void ChopForgeEditor::layoutKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (14));
    k.slider.setBounds (area);
}

void ChopForgeEditor::layoutCombo (Combo& c, juce::Rectangle<int> area)
{
    c.label.setBounds (area.removeFromTop (14));
    c.box.setBounds (area.removeFromTop (22));
}

// =============================================================================== layout
void ChopForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    auto footer = getLocalBounds().removeFromBottom (kFooterHeight + 8).reduced (8, 4);
    const int third = footer.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto r = footer.withX (footer.getX() + i * third).withWidth (third - 6).toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }
}

void ChopForgeEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    // ---- header
    auto header = area.removeFromTop (58);
    titleLabel.setBounds (header.removeFromLeft (130).withTrimmedBottom (18));
    sampleLabel.setBounds (titleLabel.getBounds().withY (titleLabel.getBottom() - 4).withHeight (18).withWidth (260));
    bpmLabel.setBounds (header.removeFromRight (130).withTrimmedTop (4));

    auto h = header.withTrimmedTop (18).withHeight (26);
    captureButton.setBounds (h.removeFromLeft (88)); h.removeFromLeft (4);
    loadButton.setBounds (h.removeFromLeft (60));    h.removeFromLeft (4);
    clearButton.setBounds (h.removeFromLeft (60));   h.removeFromLeft (4);
    exportButton.setBounds (h.removeFromLeft (60));  h.removeFromLeft (14);
    layoutCombo (captureBars, header.withTrimmedTop (2).withX (h.getX()).withWidth (80)); h.removeFromLeft (88);
    layoutCombo (sliceMode, header.withTrimmedTop (2).withX (h.getX()).withWidth (100)); h.removeFromLeft (108);
    layoutKnob (sliceCount, header.withX (h.getX()).withWidth (62).withHeight (58)); h.removeFromLeft (66);
    layoutKnob (sensitivity, header.withX (h.getX()).withWidth (62).withHeight (58));

    area.removeFromTop (6);

    // ---- footer panels
    auto footer = area.removeFromBottom (kFooterHeight);
    area.removeFromBottom (6);
    const int third = footer.getWidth() / 3;
    auto seqPanel = footer.withWidth (third - 6).reduced (10, 6);
    auto rndPanel = footer.withX (footer.getX() + third).withWidth (third - 6).reduced (10, 6);
    auto outPanel = footer.withX (footer.getX() + 2 * third).withWidth (third - 6).reduced (10, 6);
    const int knobW = 56, rowH = 60;

    // sequence
    sequenceTitle.setBounds (seqPanel.removeFromTop (16));
    {
        auto row = seqPanel.removeFromTop (rowH);
        layoutCombo (division, row.removeFromLeft (64).withTrimmedTop (4)); row.removeFromLeft (6);
        layoutCombo (seqMode, row.removeFromLeft (100).withTrimmedTop (4)); row.removeFromLeft (4);
        layoutKnob (swing, row.removeFromLeft (knobW));
        layoutKnob (freeBpm, row.removeFromLeft (knobW));
    }
    seqPanel.removeFromTop (6);
    {
        auto row = seqPanel.removeFromTop (22);
        patternLabel.setBounds (row.removeFromLeft (46));
        auto cp = row.removeFromRight (96);
        copyButton.setBounds (cp.removeFromLeft (46));
        pasteButton.setBounds (cp.removeFromRight (46));
        row.removeFromRight (6);
        const int bw = juce::jmax (14, (row.getWidth() - 2 * (kNumPatterns - 1)) / kNumPatterns);
        for (auto& b : slotButtons) { b.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (2); }
    }
    seqPanel.removeFromTop (6);
    {
        auto row = seqPanel.removeFromTop (22);
        lengthLabel.setBounds (row.removeFromLeft (46));
        lengthSlider.setBounds (row);
    }

    // randomize
    randomTitle.setBounds (rndPanel.removeFromTop (16));
    {
        auto row = rndPanel.removeFromTop (rowH);
        layoutKnob (rndAmount, row.removeFromLeft (knobW));
        layoutKnob (rndRange, row.removeFromLeft (knobW));
        layoutKnob (rndRest, row.removeFromLeft (knobW));
        row.removeFromLeft (10);
        auto toggles = row.removeFromLeft (70).withTrimmedTop (2);
        rndSlices.setBounds (toggles.removeFromTop (19));
        rndPitch.setBounds (toggles.removeFromTop (19));
        rndFx.setBounds (toggles.removeFromTop (19));
    }
    rndPanel.removeFromTop (2);
    {
        auto row = rndPanel.removeFromTop (36);
        layoutCombo (rndRoot, row.removeFromRight (56));
        row.removeFromRight (6);
        layoutCombo (rndScale, row);
    }
    rndPanel.removeFromTop (4);
    {
        auto row = rndPanel.removeFromTop (22);
        randomizeButton.setBounds (row.removeFromLeft (96)); row.removeFromLeft (3);
        undoButton.setBounds (row.removeFromLeft (48)); row.removeFromLeft (3);
        redoButton.setBounds (row.removeFromLeft (48)); row.removeFromLeft (3);
        resetButton.setBounds (row.removeFromLeft (52));
    }
    rndPanel.removeFromTop (4);
    {
        auto row = rndPanel.removeFromTop (22);
        lockAllButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2)); row.removeFromLeft (4);
        unlockAllButton.setBounds (row);
    }

    // output
    outputTitle.setBounds (outPanel.removeFromTop (16));
    {
        auto row = outPanel.removeFromTop (rowH);
        layoutKnob (mix, row.removeFromLeft (knobW));
        layoutKnob (outGain, row.removeFromLeft (knobW));
        layoutKnob (declick, row.removeFromLeft (knobW));
        layoutKnob (grain, row.removeFromLeft (knobW));
        layoutKnob (chromSlice, row.removeFromLeft (knobW));
    }
    outPanel.removeFromTop (4);
    {
        auto row = outPanel.removeFromTop (40);
        const int w = (row.getWidth() - 12) / 3;
        layoutCombo (pitchMode, row.removeFromLeft (w)); row.removeFromLeft (6);
        layoutCombo (polyphony, row.removeFromLeft (w)); row.removeFromLeft (6);
        layoutCombo (midiMode, row.removeFromLeft (w));
    }

    // ---- main
    waveform.setBounds (area.removeFromTop (juce::jmax (110, area.getHeight() / 3)));
    area.removeFromTop (6);
    grid.setBounds (area);
}

// =============================================================================== behaviour
void ChopForgeEditor::setSlot (int slot)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id::patternSlot)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) slot));
        p->endChangeGesture();
    }
}

void ChopForgeEditor::updateCaptureButton()
{
    const auto state = processor.getCaptureState();
    switch (state)
    {
        case ChopForgeProcessor::CaptureState::idle:
            captureButton.setButtonText ("CAPTURE");
            captureButton.setColour (juce::TextButton::buttonColourId, colours::panelLight);
            captureButton.setColour (juce::TextButton::textColourOffId, colours::text);
            break;
        case ChopForgeProcessor::CaptureState::armed:
            captureButton.setButtonText ("ARMED");
            captureButton.setColour (juce::TextButton::buttonColourId, colours::warn.withAlpha (0.5f));
            captureButton.setColour (juce::TextButton::textColourOffId, colours::text);
            break;
        case ChopForgeProcessor::CaptureState::recording:
        case ChopForgeProcessor::CaptureState::done:
            captureButton.setButtonText ("REC " + juce::String (juce::roundToInt (processor.getCaptureProgress() * 100.0f)) + "%");
            captureButton.setColour (juce::TextButton::buttonColourId, colours::warn);
            captureButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            break;
    }
}

void ChopForgeEditor::timerCallback()
{
    const int sv = processor.sampleVersion.load();
    if (sv != lastSampleVersion)
    {
        lastSampleVersion = sv;
        waveform.refreshFromProcessor();
        auto smp = processor.getSample();
        grid.setNumSlices (smp != nullptr ? smp->numSlices() : 16);
        if (smp != nullptr)
            sampleLabel.setText (smp->name + "  -  " + juce::String (smp->numSlices()) + " slices, "
                                 + juce::String (smp->numFrames() / smp->sourceSampleRate, 2) + " s", juce::dontSendNotification);
        else
            sampleLabel.setText ("No audio loaded", juce::dontSendNotification);
    }

    const int pv = processor.patternVersion.load();
    const int slot = processor.getCurrentSlot();
    if (pv != lastPatternVersion || ! slotButtons[(size_t) slot].getToggleState())
    {
        lastPatternVersion = pv;
        grid.repaint();
        lengthSlider.setValue (processor.getCurrentPattern().length, juce::dontSendNotification);
        for (int i = 0; i < kNumPatterns; ++i) slotButtons[(size_t) i].setToggleState (i == slot, juce::dontSendNotification);
    }
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
    pasteButton.setEnabled (processor.hasClipboard());

    grid.setCurrentStep (processor.transportRunning.load() ? processor.currentStep.load() : -1);
    waveform.setPlayingSlice (processor.activeVoices.load() > 0 ? processor.currentSlice.load() : -1);
    waveform.setCaptureInfo (processor.getCaptureState(), processor.getCaptureProgress());
    updateCaptureButton();

    bpmLabel.setText (juce::String (processor.hostBpm.load(), 1) + " BPM  " + (processor.transportRunning.load() ? "PLAY" : "STOP")
                      + "  |  " + juce::String (processor.activeVoices.load()) + " voices", juce::dontSendNotification);

    auto choice = [this] (const char* paramId) { return juce::roundToInt (processor.apvts.getRawParameterValue (paramId)->load()); };
    const bool transientMode = choice (id::sliceMode) == 1;
    sensitivity.slider.setEnabled (transientMode);
    sliceCount.slider.setEnabled (choice (id::sliceMode) != 2);
    freeBpm.slider.setEnabled (choice (id::seqMode) != 0);
    grain.slider.setEnabled (choice (id::pitchMode) == 1);
    chromSlice.slider.setEnabled (choice (id::midiMode) == 3);
}

bool ChopForgeEditor::keyPressed (const juce::KeyPress& key)
{
    const bool ctrl = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    if (ctrl && key.getKeyCode() == 'Z')
    {
        if (key.getModifiers().isShiftDown()) processor.redo(); else processor.undo();
        return true;
    }
    if (ctrl && key.getKeyCode() == 'Y') { processor.redo(); return true; }
    if (key.getKeyCode() == 'R' && ! ctrl) { processor.randomize(); return true; }
    return false;
}

bool ChopForgeEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3")) return true;
    return false;
}

void ChopForgeEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3") && processor.loadFile (juce::File (f))) break;
}

void ChopForgeEditor::openLoadDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load an audio file", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.existsAsFile() && ! processor.loadFile (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "ChopForge", "Could not read this audio file.");
    });
}

void ChopForgeEditor::openExportDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Export pattern as MIDI",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ChopForge pattern.mid"), "*.mid");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid");
        if (! processor.exportMidi (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "ChopForge", "Could not write the MIDI file.");
    });
}
