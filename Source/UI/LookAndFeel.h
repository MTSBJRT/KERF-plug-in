#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace cf::ui
{
namespace colours
{
    const juce::Colour background   { 0xff14161b };
    const juce::Colour panel        { 0xff1d2027 };
    const juce::Colour panelLight   { 0xff262a33 };
    const juce::Colour outline      { 0xff343945 };
    const juce::Colour text         { 0xffe6e8ee };
    const juce::Colour textDim      { 0xff8d94a3 };
    const juce::Colour accent       { 0xffff7a1a };
    const juce::Colour accent2      { 0xff2dd4bf };
    const juce::Colour warn         { 0xfff43f5e };
    const juce::Colour waveform     { 0xff5aa9ff };
    const juce::Colour lock         { 0xfffacc15 };
}

class ChopLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChopLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, colours::background);
        setColour (juce::Label::textColourId, colours::text);
        setColour (juce::Slider::textBoxTextColourId, colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, colours::outline);
        setColour (juce::Slider::thumbColourId, colours::accent);
        setColour (juce::Slider::trackColourId, colours::accent);
        setColour (juce::Slider::backgroundColourId, colours::outline);
        setColour (juce::ComboBox::backgroundColourId, colours::panelLight);
        setColour (juce::ComboBox::outlineColourId, colours::outline);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::arrowColourId, colours::textDim);
        setColour (juce::PopupMenu::backgroundColourId, colours::panelLight);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.6f));
        setColour (juce::TextButton::buttonColourId, colours::panelLight);
        setColour (juce::TextButton::buttonOnColourId, colours::accent);
        setColour (juce::TextButton::textColourOffId, colours::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ToggleButton::textColourId, colours::text);
        setColour (juce::ToggleButton::tickColourId, colours::accent);
        setColour (juce::ToggleButton::tickDisabledColourId, colours::outline);
        setColour (juce::TooltipWindow::backgroundColourId, colours::panelLight);
        setColour (juce::TooltipWindow::textColourId, colours::text);
        setColour (juce::TooltipWindow::outlineColourId, colours::outline);
        setColour (juce::AlertWindow::backgroundColourId, colours::panel);
        setColour (juce::AlertWindow::textColourId, colours::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float lineW = juce::jmax (2.0f, radius * 0.16f);
        const float arcR  = radius - lineW * 0.5f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float zeroAngle = bipolar ? rotaryStartAngle + (float) ((0.0 - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum())) * (rotaryEndAngle - rotaryStartAngle) : rotaryStartAngle;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
        g.setColour (slider.isEnabled() ? slider.findColour (juce::Slider::rotarySliderFillColourId) : colours::textDim);
        g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (colours::panelLight);
        g.fillEllipse (juce::Rectangle<float> (radius * 1.2f, radius * 1.2f).withCentre (centre));
        juce::Point<float> tip (centre.x + std::sin (angle) * (radius * 0.55f), centre.y - std::cos (angle) * (radius * 0.55f));
        g.setColour (colours::text);
        g.drawLine ({ centre, tip }, 2.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& backgroundColour,
                               bool isMouseOver, bool isButtonDown) override
    {
        auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
        auto base = backgroundColour;
        if (isButtonDown) base = base.brighter (0.2f);
        else if (isMouseOver) base = base.brighter (0.08f);
        g.setColour (base);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (b.getToggleState() ? colours::accent : colours::outline);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton& b, int buttonHeight) override
    {
        const int size = juce::jmin (13, buttonHeight - 4, juce::jmax (9, b.getWidth() / 2));
        return juce::Font (juce::FontOptions ((float) size, juce::Font::bold));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override  { return juce::Font (juce::FontOptions (12.0f)); }
    juce::Font getLabelFont (juce::Label& l) override      { return l.getFont(); }
    juce::Font getPopupMenuFont() override                 { return juce::Font (juce::FontOptions (13.0f)); }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) width - 12.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};
} // namespace cf::ui
