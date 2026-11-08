#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../PluginProcessor.h"

namespace kerf::ui
{
/** Shows the loaded audio with its slice markers. Markers can be dragged,
    added (double-click) and removed (right-click). */
class WaveformView : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit WaveformView (KerfProcessor& p);

    void refreshFromProcessor();          // call when the processor's sample changed
    void setPlayingSlice (int slice);
    void setCaptureInfo (KerfProcessor::CaptureState state, float progress);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    void buildPeaks();
    int  markerAt (int x) const;
    int  xToFrame (float x) const;
    float frameToX (int frame) const;
    void showContextMenu (const juce::MouseEvent& e);

    KerfProcessor& processor;
    std::shared_ptr<const kerf::SampleData> sample;
    std::vector<std::pair<float, float>> peaks; // min/max per pixel column
    int playingSlice = -1, hoverMarker = -1, dragMarker = -1;
    KerfProcessor::CaptureState captureState = KerfProcessor::CaptureState::idle;
    float captureProgress = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
} // namespace kerf::ui
