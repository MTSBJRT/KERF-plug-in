#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../PluginProcessor.h"

namespace kerf::ui
{
/** Multi-lane step editor for the current pattern. */
class StepGrid : public juce::Component,
                 public juce::SettableTooltipClient
{
public:
    explicit StepGrid (KerfProcessor& p);

    void setCurrentStep (int step);
    void setNumSlices (int n);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    enum Lane { slice = 0, pitch, gain, pan, reverse, probability, filter, repeats, gate, lock, numLanes };
    static const char* laneName (int lane);

    juce::Rectangle<float> cellBounds (int lane, int step) const;
    int  laneAt (float y) const;
    int  stepAt (float x) const;
    float laneHeight() const;
    float cellWidth() const;
    void  paintCell (juce::Graphics& g, int lane, int step, const kerf::StepData& s, juce::Rectangle<float> r, bool current);
    void  applyDrag (int lane, int step, float deltaPixels, bool fine);
    void  toggleAt (int lane, int step, float xInCell, bool firstClick);

    KerfProcessor& processor;
    int currentStep = -1, numSlices = 16;

    // drag state
    int dragLane = -1, dragStep = -1;
    float dragStartY = 0.0f;
    kerf::StepData dragStartData;
    bool paintValue = false;

    static constexpr float headerW = 58.0f;
    static constexpr float headerH = 18.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGrid)
};
} // namespace kerf::ui
