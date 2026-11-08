#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/WaveformView.h"
#include "UI/StepGrid.h"

class ChopForgeEditor : public juce::AudioProcessorEditor,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    explicit ChopForgeEditor (ChopForgeProcessor&);
    ~ChopForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
        bool onWood = false;
    };
    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboAttachment> attachment;
        bool onWood = false;
    };

    void makeKnob (Knob& k, const char* paramId, const juce::String& text, const juce::String& tooltip = {});
    void makeCombo (Combo& c, const char* paramId, const juce::String& text, const juce::String& tooltip = {});
    void makeButton (juce::TextButton& b, const juce::String& text, const juce::String& tooltip, std::function<void()> onClick);
    void layoutKnob (Knob& k, juce::Rectangle<int> area);
    void layoutCombo (Combo& c, juce::Rectangle<int> area);
    void timerCallback() override;
    void updateCaptureButton();
    void openLoadDialog();
    void openExportDialog();
    void setSlot (int slot);

    ChopForgeProcessor& processor;
    cf::ui::ChopLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };
    std::unique_ptr<juce::FileChooser> fileChooser;

    // header
    juce::Label titleLabel, sampleLabel, bpmLabel;
    juce::TextButton captureButton, loadButton, clearButton, exportButton;
    Combo captureBars, sliceMode;
    Knob sliceCount, sensitivity;

    // main
    cf::ui::WaveformView waveform;
    cf::ui::StepGrid grid;

    // sequence panel
    Combo division, seqMode;
    Knob swing, freeBpm;
    juce::Slider lengthSlider;
    juce::Label lengthLabel, patternLabel, sequenceTitle, randomTitle, outputTitle;
    std::array<juce::TextButton, cf::kNumPatterns> slotButtons;
    juce::TextButton copyButton, pasteButton;

    // random panel
    Knob rndAmount, rndRange, rndRest;
    Combo rndScale, rndRoot;
    juce::ToggleButton rndSlices, rndPitch, rndFx;
    std::unique_ptr<ButtonAttachment> rndSlicesAtt, rndPitchAtt, rndFxAtt;
    juce::TextButton randomizeButton, undoButton, redoButton, resetButton, lockAllButton, unlockAllButton;

    // output panel
    Knob mix, outGain, declick, grain;
    Combo pitchMode, polyphony, midiMode;
    Knob chromSlice;

    int lastSampleVersion = -1, lastPatternVersion = -1;
    static constexpr int kFooterHeight = 204;
    juce::Rectangle<int> screenBounds, namePlate;
    std::array<juce::Rectangle<int>, 3> footerPanels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopForgeEditor)
};
