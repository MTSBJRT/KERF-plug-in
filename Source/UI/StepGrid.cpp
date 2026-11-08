#include "StepGrid.h"
#include "LookAndFeel.h"

namespace kerf::ui
{
StepGrid::StepGrid (KerfProcessor& p) : processor (p)
{
    setTooltip ("Drag a cell up/down to edit (shift = fine). Double-click resets. Click REV / LOCK cells to toggle.");
}

const char* StepGrid::laneName (int lane)
{
    static const char* names[] = { "SLICE", "PITCH", "GAIN", "PAN", "REV", "PROB", "FILTER", "REPEAT", "GATE", "LOCK" };
    return names[juce::jlimit (0, numLanes - 1, lane)];
}

void StepGrid::setCurrentStep (int step)
{
    if (currentStep != step) { currentStep = step; repaint(); }
}

void StepGrid::setNumSlices (int n) { numSlices = juce::jmax (1, n); repaint(); }

float StepGrid::laneHeight() const { return ((float) getHeight() - headerH) / (float) numLanes; }
float StepGrid::cellWidth() const  { return ((float) getWidth() - headerW) / (float) juce::jmax (1, processor.getCurrentPattern().length); }

juce::Rectangle<float> StepGrid::cellBounds (int lane, int step) const
{
    return { headerW + step * cellWidth(), headerH + lane * laneHeight(), cellWidth(), laneHeight() };
}

int StepGrid::laneAt (float y) const
{
    if (y < headerH) return -1;
    const int l = (int) ((y - headerH) / laneHeight());
    return l >= 0 && l < numLanes ? l : -1;
}

int StepGrid::stepAt (float x) const
{
    if (x < headerW) return -1;
    const int s = (int) ((x - headerW) / cellWidth());
    return s >= 0 && s < processor.getCurrentPattern().length ? s : -1;
}

void StepGrid::paint (juce::Graphics& g)
{
    const auto& pattern = processor.getCurrentPattern();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // step header
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (int s = 0; s < pattern.length; ++s)
    {
        auto r = juce::Rectangle<float> (headerW + s * cellWidth(), 0.0f, cellWidth(), headerH);
        if (s == currentStep)
        {
            g.setColour (colours::accent.withAlpha (0.25f));
            g.fillRect (r.withHeight ((float) getHeight()));
            g.setColour (colours::accent);
            g.fillRect (r.reduced (1.0f, 3.0f));
            g.setColour (juce::Colours::black);
        }
        else
        {
            g.setColour ((s % 4 == 0) ? colours::text : colours::textDim);
        }
        if (cellWidth() > 14.0f) g.drawText (juce::String (s + 1), r.toNearestInt(), juce::Justification::centred);
    }

    // lanes
    for (int l = 0; l < numLanes; ++l)
    {
        auto laneRect = juce::Rectangle<float> (0.0f, headerH + l * laneHeight(), (float) getWidth(), laneHeight());
        if (l % 2 == 0)
        {
            g.setColour (colours::panelLight.withAlpha (0.35f));
            g.fillRect (laneRect);
        }
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (laneName (l), laneRect.withWidth (headerW).reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);

        for (int s = 0; s < pattern.length; ++s)
            paintCell (g, l, s, pattern.steps[(size_t) s], cellBounds (l, s), s == currentStep);
    }

    // bar lines
    g.setColour (colours::outline);
    for (int s = 0; s <= pattern.length; s += 4)
        g.drawVerticalLine ((int) (headerW + s * cellWidth()), 0.0f, (float) getHeight());
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void StepGrid::paintCell (juce::Graphics& g, int lane, int, const kerf::StepData& s, juce::Rectangle<float> r, bool)
{
    auto inner = r.reduced (1.5f, 2.0f);
    const bool wide = inner.getWidth() > 22.0f;
    g.setFont (juce::FontOptions (juce::jlimit (8.0f, 11.0f, inner.getWidth() * 0.45f), juce::Font::bold));

    auto drawBar = [&] (float norm, juce::Colour c, bool bipolar)
    {
        g.setColour (colours::background.withAlpha (0.6f));
        g.fillRoundedRectangle (inner, 2.0f);
        g.setColour (c);
        if (bipolar)
        {
            const float mid = inner.getCentreY();
            const float h = (norm - 0.5f) * inner.getHeight();
            g.fillRect (inner.getX(), juce::jmin (mid, mid - h), inner.getWidth(), std::abs (h));
            g.setColour (colours::textDim.withAlpha (0.5f));
            g.drawHorizontalLine ((int) mid, inner.getX(), inner.getRight());
        }
        else
        {
            const float h = juce::jlimit (0.0f, 1.0f, norm) * inner.getHeight();
            g.fillRoundedRectangle (inner.withTop (inner.getBottom() - h), 2.0f);
        }
    };
    auto drawText = [&] (const juce::String& t, juce::Colour c)
    {
        if (! wide) return;
        g.setColour (c);
        g.drawText (t, inner.toNearestInt(), juce::Justification::centred);
    };

    switch (lane)
    {
        case slice:
        {
            if (s.slice < 0)
            {
                g.setColour (colours::background.withAlpha (0.6f));
                g.fillRoundedRectangle (inner, 2.0f);
                drawText ("-", colours::textDim);
            }
            else
            {
                const auto c = juce::Colour::fromHSV ((float) s.slice / (float) juce::jmax (1, numSlices), 0.55f, 0.75f, 1.0f);
                g.setColour (s.slice < numSlices ? c : colours::warn.withAlpha (0.4f));
                g.fillRoundedRectangle (inner, 2.0f);
                drawText (juce::String (s.slice + 1), juce::Colours::black);
            }
            break;
        }
        case pitch:
            drawBar ((s.pitch + 24.0f) / 48.0f, colours::accent2, true);
            drawText (juce::exactlyEqual (s.pitch, 0.0f) ? "0" : (s.pitch > 0 ? "+" : "") + juce::String ((int) s.pitch), colours::text);
            break;
        case gain:
            drawBar (s.gain / 2.0f, colours::accent, false);
            drawText (juce::String (juce::roundToInt (s.gain * 100)), colours::text);
            break;
        case pan:
            drawBar ((s.pan + 1.0f) * 0.5f, colours::waveform, true);
            drawText (std::abs (s.pan) < 0.01f ? "C" : (s.pan < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (s.pan) * 100)), colours::text);
            break;
        case reverse:
            g.setColour (s.reverse ? colours::accent : colours::background.withAlpha (0.6f));
            g.fillRoundedRectangle (inner, 2.0f);
            drawText (s.reverse ? "<<" : "", juce::Colours::black);
            break;
        case probability:
            drawBar (s.probability, colours::accent2.withAlpha (0.8f), false);
            drawText (juce::String (juce::roundToInt (s.probability * 100)), colours::text);
            break;
        case filter:
            drawBar ((s.filter + 1.0f) * 0.5f, s.filter < 0 ? colours::waveform : colours::accent, true);
            drawText (std::abs (s.filter) < 0.02f ? "" : (s.filter < 0 ? "LP" : "HP"), colours::text);
            break;
        case repeats:
        {
            g.setColour (colours::background.withAlpha (0.6f));
            g.fillRoundedRectangle (inner, 2.0f);
            g.setColour (s.repeats > 1 ? colours::accent : colours::outline);
            const float w = inner.getWidth() / (float) s.repeats;
            for (int i = 0; i < s.repeats; ++i)
                g.fillRect (inner.getX() + i * w + 1.0f, inner.getBottom() - inner.getHeight() * 0.35f, juce::jmax (1.0f, w - 2.0f), inner.getHeight() * 0.3f);
            drawText (s.repeats > 1 ? "x" + juce::String (s.repeats) : "", colours::text);
            break;
        }
        case gate:
            drawBar (s.gate / 2.0f, colours::accent2, false);
            drawText (juce::String (juce::roundToInt (s.gate * 100)), colours::text);
            break;
        case lock:
        {
            const float w = inner.getWidth() / 3.0f;
            const bool states[3] = { s.lockSlice, s.lockPitch, s.lockFx };
            const char* labels[3] = { "S", "P", "F" };
            for (int i = 0; i < 3; ++i)
            {
                auto cell = juce::Rectangle<float> (inner.getX() + i * w, inner.getY(), w, inner.getHeight()).reduced (1.0f);
                g.setColour (states[i] ? colours::lock : colours::background.withAlpha (0.6f));
                g.fillRoundedRectangle (cell, 2.0f);
                if (inner.getWidth() > 30.0f)
                {
                    g.setColour (states[i] ? juce::Colours::black : colours::textDim);
                    g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
                    g.drawText (labels[i], cell.toNearestInt(), juce::Justification::centred);
                }
            }
            break;
        }
        default: break;
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int lane = laneAt ((float) e.y);
    const int step = stepAt ((float) e.x);
    dragLane = lane;
    dragStep = step;
    if (lane < 0 || step < 0) return;

    processor.snapshotForUndo();
    dragStartY = (float) e.y;
    dragStartData = processor.getCurrentPattern().steps[(size_t) step];

    if (lane == reverse || lane == lock)
    {
        auto r = cellBounds (lane, step);
        toggleAt (lane, step, (float) e.x - r.getX(), true);
    }
}

void StepGrid::toggleAt (int lane, int step, float xInCell, bool firstClick)
{
    auto s = processor.getCurrentPattern().steps[(size_t) step];
    if (lane == reverse)
    {
        if (firstClick) paintValue = ! s.reverse;
        s.reverse = paintValue;
    }
    else if (lane == lock)
    {
        const int which = juce::jlimit (0, 2, (int) (xInCell / (cellWidth() / 3.0f)));
        bool* lockFlags[3] = { &s.lockSlice, &s.lockPitch, &s.lockFx };
        if (firstClick) { paintValue = ! *lockFlags[which]; dragStartData.repeats = which; } // reuse field to remember which lock
        *lockFlags[dragStartData.repeats] = paintValue;
    }
    processor.setStep (step, s);
    repaint();
}

void StepGrid::applyDrag (int lane, int step, float deltaPixels, bool fine)
{
    auto s = dragStartData;
    const float sens = fine ? 0.25f : 1.0f;
    const float d = deltaPixels * sens;
    switch (lane)
    {
        case slice:       s.slice = juce::jlimit (-1, numSlices - 1, dragStartData.slice + (int) std::round (d / 8.0f)); break;
        case pitch:       s.pitch = (float) juce::jlimit (-24, 24, (int) std::round (dragStartData.pitch + d / 5.0f)); break;
        case gain:        s.gain = juce::jlimit (0.0f, 2.0f, dragStartData.gain + d / 60.0f); break;
        case pan:         s.pan = juce::jlimit (-1.0f, 1.0f, dragStartData.pan + d / 60.0f); break;
        case probability: s.probability = juce::jlimit (0.0f, 1.0f, dragStartData.probability + d / 100.0f); break;
        case filter:      s.filter = juce::jlimit (-1.0f, 1.0f, dragStartData.filter + d / 80.0f); break;
        case repeats:     s.repeats = juce::jlimit (1, 8, dragStartData.repeats + (int) std::round (d / 12.0f)); break;
        case gate:        s.gate = juce::jlimit (0.1f, 2.0f, dragStartData.gate + d / 60.0f); break;
        default: return;
    }
    processor.setStep (step, s);
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragLane < 0 || dragStep < 0) return;
    if (dragLane == reverse || dragLane == lock)
    {
        const int step = stepAt ((float) e.x);
        if (step >= 0 && step != dragStep)
        {
            dragStep = step;
            toggleAt (dragLane, step, (float) e.x - cellBounds (dragLane, step).getX(), false);
        }
        return;
    }
    applyDrag (dragLane, dragStep, dragStartY - (float) e.y, e.mods.isShiftDown());
}

void StepGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int lane = laneAt ((float) e.y);
    const int step = stepAt ((float) e.x);
    if (lane < 0 || step < 0) return;
    processor.snapshotForUndo();
    auto s = processor.getCurrentPattern().steps[(size_t) step];
    switch (lane)
    {
        case slice:       s.slice = step % numSlices; break;
        case pitch:       s.pitch = 0.0f; break;
        case gain:        s.gain = 1.0f; break;
        case pan:         s.pan = 0.0f; break;
        case reverse:     s.reverse = false; break;
        case probability: s.probability = 1.0f; break;
        case filter:      s.filter = 0.0f; break;
        case repeats:     s.repeats = 1; break;
        case gate:        s.gate = 1.0f; break;
        case lock:        s.lockSlice = s.lockPitch = s.lockFx = false; break;
        default: break;
    }
    processor.setStep (step, s);
    repaint();
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int lane = laneAt ((float) e.y);
    const int step = stepAt ((float) e.x);
    if (lane < 0 || step < 0 || lane == reverse || lane == lock) return;
    dragStartData = processor.getCurrentPattern().steps[(size_t) step];
    const float delta = (wheel.deltaY > 0 ? 1.0f : -1.0f) * (lane == slice ? 8.0f : (lane == pitch ? 5.0f : (lane == repeats ? 12.0f : 6.0f)));
    applyDrag (lane, step, delta, false);
}
} // namespace kerf::ui
