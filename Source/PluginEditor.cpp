#include "PluginEditor.h"
#include "UI/Logo.h"

using namespace kerf;
using namespace kerf::ui;

KerfEditor::KerfEditor (KerfProcessor& p)
    : AudioProcessorEditor (&p), processor (p), waveform (p), grid (p)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    // ------------------------------------------------------------ header
    titleLabel.setText ("Kerf", juce::dontSendNotification);
    titleLabel.setVisible (false); // the printed stamp replaces the nameplate
    titleLabel.setFont (juce::FontOptions (25.0f, juce::Font::bold));
    titleLabel.setComponentID ("nameplate");
    titleLabel.setColour (juce::Label::textColourId, colours::engraved);
    titleLabel.setJustificationType (juce::Justification::centred);
    addChildComponent (titleLabel); // kept for layout only, the stamp is painted in its place

    sampleLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    sampleLabel.setColour (juce::Label::textColourId, colours::engraved);
    addAndMakeVisible (sampleLabel);

    bpmLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    bpmLabel.setColour (juce::Label::textColourId, colours::engraved);
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    makeButton (captureButton, "CAPTURE", "Record the live input for the chosen number of bars (starts on the next bar when the host is playing)", [this]
    {
        if (processor.getCaptureState() == KerfProcessor::CaptureState::idle) processor.requestCapture();
        else processor.cancelCapture();
    });
    makeButton (loadButton, "LOAD", "Load an audio file (or drop one onto the window)", [this] { openLoadDialog(); });
    makeButton (clearButton, "CLEAR", "Remove the loaded audio", [this] { processor.clearSample(); });
    makeButton (exportButton, "MIDI", "Export the current pattern as a MIDI file (one note per slice, C1 = slice 1)", [this] { openExportDialog(); });

    captureBars.onWood = sliceMode.onWood = sliceCount.onWood = sensitivity.onWood = true;
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
        l.setColour (juce::Label::textColourId, colours::engraved);
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
    lengthLabel.setColour (juce::Label::textColourId, colours::engraved);
    addAndMakeVisible (lengthLabel);

    patternLabel.setText ("Pattern", juce::dontSendNotification);
    patternLabel.setFont (juce::FontOptions (11.0f));
    patternLabel.setColour (juce::Label::textColourId, colours::engraved);
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
        t.setColour (juce::ToggleButton::textColourId, colours::engraved);
        att = std::make_unique<ButtonAttachment> (processor.apvts, paramId, t);
        addAndMakeVisible (t);
    };
    makeToggle (rndSlices, rndSlicesAtt, id::rndSlices, "Slices");
    makeToggle (rndPitch, rndPitchAtt, id::rndPitch, "Pitch");
    makeToggle (rndFx, rndFxAtt, id::rndFx, "FX");
    makeButton (randomizeButton, "RANDOMIZE", "Randomize the unlocked steps (shortcut: R)", [this] { processor.randomize(); });
    randomizeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff6b4a12));
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

    if (auto autoload = juce::SystemStats::getEnvironmentVariable ("KERF_AUTOLOAD", {}); autoload.isNotEmpty() && processor.getSample() == nullptr)
        processor.loadFile (juce::File (autoload));

    setResizable (true, true);
    setResizeLimits (980, 660, 1920, 1200);
    setSize (1100, 760);
    startTimerHz (30);
}

KerfEditor::~KerfEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// =============================================================================== helpers
void KerfEditor::makeKnob (Knob& k, const char* paramId, const juce::String& text, const juce::String& tooltip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 15);
    k.slider.setTooltip (tooltip);
    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
    addAndMakeVisible (k.slider);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, colours::engraved);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);
}

void KerfEditor::makeCombo (Combo& c, const char* paramId, const juce::String& text, const juce::String& tooltip)
{
    c.box.setTooltip (tooltip);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramId)))
        c.box.addItemList (choice->choices, 1);
    c.attachment = std::make_unique<ComboAttachment> (processor.apvts, paramId, c.box);
    addAndMakeVisible (c.box);
    c.label.setText (text, juce::dontSendNotification);
    c.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    c.label.setColour (juce::Label::textColourId, colours::engraved);
    addAndMakeVisible (c.label);
}

void KerfEditor::makeButton (juce::TextButton& b, const juce::String& text, const juce::String& tooltip, std::function<void()> onClick)
{
    b.setButtonText (text);
    b.setTooltip (tooltip);
    b.onClick = std::move (onClick);
    addAndMakeVisible (b);
}

void KerfEditor::layoutKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (14));
    k.slider.setBounds (area);
}

void KerfEditor::layoutCombo (Combo& c, juce::Rectangle<int> area)
{
    c.label.setBounds (area.removeFromTop (14));
    c.box.setBounds (area.removeFromTop (22));
}

// =============================================================================== layout
void KerfEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    lookAndFeel.drawFace (g, bounds);
    kerf::ui::KerfLookAndFeel::drawLighting (g, bounds.toFloat());

    // screen-printed maker's stamp instead of a nameplate
    kerf::ui::logo::drawStamp (g, namePlate.toFloat(), kLogoVariant, colours::engraved,
                               &lookAndFeel.textures().brushed (colours::metalDark));
    lookAndFeel.drawScreenBezel (g, screenBounds.toFloat());

    for (auto& panel : footerPanels)
    {
        lookAndFeel.drawInlay (g, panel);
        lookAndFeel.drawScrewsAtCorners (g, panel.toFloat(), 9.0f, 4.5f);
    }

    lookAndFeel.drawScrewsAtCorners (g, bounds.toFloat(), 13.0f, 6.5f);
    lookAndFeel.drawScrew (g, (float) bounds.getCentreX(), (float) screenBounds.getY() - 11.0f, 5.0f, 6);
    lookAndFeel.drawScrew (g, (float) bounds.getCentreX(), (float) bounds.getBottom() - 13.0f, 6.5f, 1);
}

void KerfEditor::resized()
{
    auto area = getLocalBounds().reduced (14, 10);

    // ---- header
    auto header = area.removeFromTop (62);
    titleLabel.setBounds (header.removeFromLeft (150).withTrimmedBottom (22).withTrimmedTop (0));
    header.removeFromLeft (18);
    sampleLabel.setBounds (titleLabel.getBounds().withY (titleLabel.getBottom() + 2).withHeight (18).withWidth (300));
    bpmLabel.setBounds (header.removeFromRight (200).withTrimmedTop (4));

    auto h = header.withTrimmedTop (18).withHeight (26);
    captureButton.setBounds (h.removeFromLeft (88)); h.removeFromLeft (4);
    loadButton.setBounds (h.removeFromLeft (60));    h.removeFromLeft (4);
    clearButton.setBounds (h.removeFromLeft (60));   h.removeFromLeft (4);
    exportButton.setBounds (h.removeFromLeft (60));  h.removeFromLeft (14);
    layoutCombo (captureBars, header.withTrimmedTop (2).withX (h.getX()).withWidth (80)); h.removeFromLeft (88);
    layoutCombo (sliceMode, header.withTrimmedTop (2).withX (h.getX()).withWidth (100)); h.removeFromLeft (108);
    layoutKnob (sliceCount, header.withX (h.getX()).withWidth (70).withHeight (62)); h.removeFromLeft (74);
    layoutKnob (sensitivity, header.withX (h.getX()).withWidth (70).withHeight (62));

    area.removeFromTop (8);

    // ---- footer panels
    auto footer = area.removeFromBottom (kFooterHeight);
    area.removeFromBottom (10);
    const int third = footer.getWidth() / 3;
    footerPanels[0] = footer.withWidth (third - 6);
    footerPanels[1] = footer.withX (footer.getX() + third).withWidth (third - 6);
    footerPanels[2] = footer.withX (footer.getX() + 2 * third).withWidth (third - 6);
    auto seqPanel = footerPanels[0].reduced (14, 8);
    auto rndPanel = footerPanels[1].reduced (14, 8);
    auto outPanel = footerPanels[2].reduced (14, 8);
    const int knobW = 78, rowH = 88;

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
        const int w3 = juce::jmin (knobW, (row.getWidth() - 90) / 3);
        layoutKnob (rndAmount, row.removeFromLeft (w3));
        layoutKnob (rndRange, row.removeFromLeft (w3));
        layoutKnob (rndRest, row.removeFromLeft (w3));
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
        const int w5 = juce::jmin (knobW, row.getWidth() / 5);
        layoutKnob (mix, row.removeFromLeft (w5));
        layoutKnob (outGain, row.removeFromLeft (w5));
        layoutKnob (declick, row.removeFromLeft (w5));
        layoutKnob (grain, row.removeFromLeft (w5));
        layoutKnob (chromSlice, row.removeFromLeft (w5));
    }
    outPanel.removeFromTop (4);
    {
        auto row = outPanel.removeFromTop (40);
        const int w = (row.getWidth() - 12) / 3;
        layoutCombo (pitchMode, row.removeFromLeft (w)); row.removeFromLeft (6);
        layoutCombo (polyphony, row.removeFromLeft (w)); row.removeFromLeft (6);
        layoutCombo (midiMode, row.removeFromLeft (w));
    }

    // ---- main (the screen)
    screenBounds = area;
    area.reduce (9, 9);
    waveform.setBounds (area.removeFromTop (juce::jmax (110, area.getHeight() / 3)));
    area.removeFromTop (6);
    grid.setBounds (area);
    namePlate = titleLabel.getBounds().withTrimmedTop (2).withTrimmedBottom (2);
}

// =============================================================================== behaviour
void KerfEditor::setSlot (int slot)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id::patternSlot)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) slot));
        p->endChangeGesture();
    }
}

void KerfEditor::updateCaptureButton()
{
    const auto state = processor.getCaptureState();
    switch (state)
    {
        case KerfProcessor::CaptureState::idle:
            captureButton.setButtonText ("CAPTURE");
            captureButton.setColour (juce::TextButton::buttonColourId, colours::steelMid);
            break;
        case KerfProcessor::CaptureState::armed:
            captureButton.setButtonText ("ARMED");
            captureButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff6b4a12));
            break;
        case KerfProcessor::CaptureState::recording:
        case KerfProcessor::CaptureState::done:
            captureButton.setButtonText ("REC " + juce::String (juce::roundToInt (processor.getCaptureProgress() * 100.0f)) + "%");
            captureButton.setColour (juce::TextButton::buttonColourId, colours::warn);
            break;
    }
}

void KerfEditor::timerCallback()
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

bool KerfEditor::keyPressed (const juce::KeyPress& key)
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

bool KerfEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3")) return true;
    return false;
}

void KerfEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3") && processor.loadFile (juce::File (f))) break;
}

void KerfEditor::openLoadDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load an audio file", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.existsAsFile() && ! processor.loadFile (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Kerf", "Could not read this audio file.");
    });
}

void KerfEditor::openExportDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Export pattern as MIDI",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Kerf pattern.mid"), "*.mid");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid");
        if (! processor.exportMidi (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Kerf", "Could not write the MIDI file.");
    });
}
