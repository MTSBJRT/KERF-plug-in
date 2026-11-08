#include "WaveformView.h"
#include "LookAndFeel.h"
#include "../Engine/TransientDetector.h"

namespace kerf::ui
{
WaveformView::WaveformView (KerfProcessor& p) : processor (p)
{
    setTooltip ("Double-click: add a slice marker. Drag a marker to move it. Right-click a marker to remove it.");
    refreshFromProcessor();
}

void WaveformView::refreshFromProcessor()
{
    sample = processor.getSample();
    buildPeaks();
    repaint();
}

void WaveformView::setPlayingSlice (int slice)
{
    if (playingSlice != slice) { playingSlice = slice; repaint(); }
}

void WaveformView::setCaptureInfo (KerfProcessor::CaptureState state, float progress)
{
    if (state != captureState || std::abs (progress - captureProgress) > 0.005f)
    {
        captureState = state;
        captureProgress = progress;
        repaint();
    }
}

void WaveformView::resized() { buildPeaks(); }

void WaveformView::buildPeaks()
{
    peaks.clear();
    const int w = getWidth();
    if (sample == nullptr || w <= 0 || sample->numFrames() == 0) return;
    const int n = sample->numFrames();
    peaks.resize ((size_t) w, { 0.0f, 0.0f });
    const float* l = sample->audio->getReadPointer (0);
    const float* r = sample->audio->getReadPointer (1);
    for (int x = 0; x < w; ++x)
    {
        const int start = (int) ((juce::int64) n * x / w);
        const int end   = juce::jmax (start + 1, (int) ((juce::int64) n * (x + 1) / w));
        float mn = 1.0f, mx = -1.0f;
        for (int i = start; i < end && i < n; ++i)
        {
            const float v = 0.5f * (l[i] + r[i]);
            mn = juce::jmin (mn, v);
            mx = juce::jmax (mx, v);
        }
        peaks[(size_t) x] = { mn, mx };
    }
}

int WaveformView::xToFrame (float x) const
{
    if (sample == nullptr) return 0;
    return juce::jlimit (0, juce::jmax (0, sample->numFrames() - 1), (int) std::round (x / (float) getWidth() * (float) sample->numFrames()));
}

float WaveformView::frameToX (int frame) const
{
    if (sample == nullptr || sample->numFrames() == 0) return 0.0f;
    return (float) frame / (float) sample->numFrames() * (float) getWidth();
}

int WaveformView::markerAt (int x) const
{
    if (sample == nullptr) return -1;
    int best = -1;
    float bestDist = 6.0f;
    for (int i = 1; i < sample->numSlices(); ++i) // marker 0 is fixed at the start
    {
        const float d = std::abs (frameToX (sample->markers[(size_t) i]) - (float) x);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    if (sample == nullptr || sample->numFrames() == 0)
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (14.0f));
        juce::String msg = captureState == KerfProcessor::CaptureState::armed
                             ? "Capture armed - waiting for the next bar..."
                             : "Drop an audio file here, click LOAD, or CAPTURE the live input";
        if (captureState == KerfProcessor::CaptureState::recording)
            msg = "Recording... " + juce::String (juce::roundToInt (captureProgress * 100.0f)) + " %";
        g.drawText (msg, getLocalBounds(), juce::Justification::centred);
        if (captureState == KerfProcessor::CaptureState::recording)
        {
            g.setColour (colours::warn.withAlpha (0.35f));
            g.fillRoundedRectangle (bounds.withWidth (bounds.getWidth() * captureProgress), 6.0f);
        }
        g.setColour (colours::outline);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
        return;
    }

    const int n = sample->numSlices();
    const float h = bounds.getHeight();
    const float mid = h * 0.5f;

    // slice backgrounds
    for (int i = 0; i < n; ++i)
    {
        auto range = sample->sliceRange (i);
        const float x0 = frameToX (range.getStart());
        const float x1 = frameToX (range.getEnd());
        juce::Colour c = juce::Colour::fromHSV ((float) i / (float) juce::jmax (1, n), 0.45f, 0.5f, 1.0f).withAlpha (i == playingSlice ? 0.55f : 0.12f);
        g.setColour (c);
        g.fillRect (x0, 0.0f, juce::jmax (1.0f, x1 - x0), h);
    }

    // waveform
    g.setColour (colours::waveform);
    for (int x = 0; x < (int) peaks.size(); ++x)
    {
        const auto [mn, mx] = peaks[(size_t) x];
        const float y0 = mid - mx * mid * 0.92f;
        const float y1 = mid - mn * mid * 0.92f;
        g.drawVerticalLine (x, juce::jmin (y0, y1), juce::jmax (y0 + 1.0f, y1));
    }

    // markers and numbers
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (int i = 0; i < n; ++i)
    {
        const float x = frameToX (sample->markers[(size_t) i]);
        const bool hot = i == hoverMarker || i == dragMarker;
        g.setColour (hot ? colours::accent2 : colours::accent.withAlpha (i == 0 ? 0.5f : 0.9f));
        g.drawVerticalLine ((int) x, 0.0f, h);
        if (hot) g.drawVerticalLine ((int) x + 1, 0.0f, h);
        g.setColour (colours::text.withAlpha (0.85f));
        g.drawText (juce::String (i + 1), juce::Rectangle<int> ((int) x + 3, 2, 26, 12), juce::Justification::centredLeft);
    }

    // capture overlay
    if (captureState == KerfProcessor::CaptureState::recording)
    {
        g.setColour (colours::warn.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds.withWidth (bounds.getWidth() * captureProgress), 6.0f);
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("RECORDING", getLocalBounds(), juce::Justification::centred);
    }
    else if (captureState == KerfProcessor::CaptureState::armed)
    {
        g.setColour (colours::warn.withAlpha (0.8f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("ARMED - waiting for next bar", getLocalBounds(), juce::Justification::centred);
    }

    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    const int m = markerAt (e.x);
    if (m != hoverMarker) { hoverMarker = m; repaint(); }
    setMouseCursor (m >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void WaveformView::mouseExit (const juce::MouseEvent&)
{
    if (hoverMarker != -1) { hoverMarker = -1; repaint(); }
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    if (sample == nullptr) return;
    if (e.mods.isPopupMenu()) { showContextMenu (e); return; }
    dragMarker = markerAt (e.x);
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (sample == nullptr || dragMarker <= 0 || dragMarker >= sample->numSlices()) return;
    auto markers = sample->markers;
    const int lo = markers[(size_t) dragMarker - 1] + 32;
    const int hi = dragMarker + 1 < (int) markers.size() ? markers[(size_t) dragMarker + 1] - 32 : sample->numFrames() - 1;
    markers[(size_t) dragMarker] = juce::jlimit (lo, juce::jmax (lo, hi), xToFrame ((float) e.x));
    processor.setMarkers (markers);
    refreshFromProcessor();
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    dragMarker = -1;
    repaint();
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (sample == nullptr || markerAt (e.x) >= 0) return;
    if (sample->numSlices() >= kerf::kMaxSlices) return;
    auto markers = sample->markers;
    markers.push_back (xToFrame ((float) e.x));
    processor.setMarkers (markers);
    refreshFromProcessor();
}

void WaveformView::showContextMenu (const juce::MouseEvent& e)
{
    const int m = markerAt (e.x);
    const int frame = xToFrame ((float) e.x);
    juce::PopupMenu menu;
    menu.addItem (1, "Add marker here", m < 0 && sample->numSlices() < kerf::kMaxSlices);
    menu.addItem (2, "Remove this marker", m > 0);
    menu.addSeparator();
    menu.addItem (3, "Split this slice in two");
    menu.addItem (4, "Remove all markers (one slice)");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                        [this, m, frame] (int result)
    {
        if (result == 0 || sample == nullptr) return;
        auto markers = sample->markers;
        if (result == 1) markers.push_back (frame);
        else if (result == 2) markers.erase (markers.begin() + m);
        else if (result == 3)
        {
            for (int i = 0; i < sample->numSlices(); ++i)
            {
                auto r = sample->sliceRange (i);
                if (r.contains (frame)) { markers.push_back (r.getStart() + r.getLength() / 2); break; }
            }
        }
        else if (result == 4) markers = { 0 };
        processor.setMarkers (markers);
        refreshFromProcessor();
    });
}
} // namespace kerf::ui
