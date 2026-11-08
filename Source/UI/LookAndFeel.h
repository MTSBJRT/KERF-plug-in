#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace cf::ui
{
/* The "screen" (waveform + step grid) keeps a dark display look; everything around it
   is a hand-built varnished oak box with brass screws and brushed-metal controls.
   All textures are generated procedurally, no image resources. */
namespace colours
{
    // screen
    const juce::Colour background   { 0xff0d0e11 };
    const juce::Colour panel        { 0xff15171c };
    const juce::Colour panelLight   { 0xff22252d };
    const juce::Colour outline      { 0xff30343e };
    const juce::Colour text         { 0xffe6e8ee };
    const juce::Colour textDim      { 0xff8d94a3 };
    const juce::Colour accent       { 0xffff8c2a };
    const juce::Colour accent2      { 0xff34d3bd };
    const juce::Colour warn         { 0xfff43f5e };
    const juce::Colour waveform     { 0xff6ab4ff };
    const juce::Colour lock         { 0xfffacc15 };

    // cabinet
    const juce::Colour oakLight     { 0xffd9a662 };
    const juce::Colour oakMid       { 0xffb0783a };
    const juce::Colour oakDark      { 0xff6e4219 };
    const juce::Colour oakPore      { 0xff4a2a0e };
    const juce::Colour engraved     { 0xff2e1a08 };
    const juce::Colour engravedHi   { 0xffe9c88f };
    const juce::Colour brass        { 0xffc9a24a };
    const juce::Colour brassLight   { 0xfff3dc8c };
    const juce::Colour brassDark    { 0xff7d5a12 };
    const juce::Colour steelLight   { 0xffe4e6ea };
    const juce::Colour steelMid     { 0xffa7abb3 };
    const juce::Colour steelDark    { 0xff5c6068 };
    const juce::Colour readout      { 0xff12100c };
    const juce::Colour readoutText  { 0xffffb648 };
    const juce::Colour ledOff       { 0xff3a2a12 };
    const juce::Colour ledOn        { 0xffffa21f };
}

/** Procedural oak texture, cached per size. */
class WoodTexture
{
public:
    const juce::Image& get (int w, int h)
    {
        if (image.isValid() && image.getWidth() == w && image.getHeight() == h) return image;
        image = juce::Image (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), false);
        juce::Image::BitmapData px (image, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < image.getHeight(); ++y)
        {
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const float fx = (float) x, fy = (float) y;
                // long grain lines run horizontally, slowly warped along x
                const float warp  = noise (fx * 0.0035f, fy * 0.02f) * 14.0f + noise (fx * 0.015f, fy * 0.09f) * 3.0f;
                const float ring  = 0.5f + 0.5f * std::sin ((fy + warp) * 0.42f + noise (fx * 0.006f, fy * 0.006f) * 9.0f);
                const float sharp = std::pow (ring, 2.2f);
                const float pores = noise (fx * 0.35f, fy * 1.1f);
                const float broad = noise (fx * 0.0025f, fy * 0.004f);
                float t = 0.22f + sharp * 0.55f + (broad - 0.5f) * 0.3f;
                juce::Colour c = colours::oakDark.interpolatedWith (colours::oakLight, juce::jlimit (0.0f, 1.0f, t));
                if (pores > 0.82f) c = c.interpolatedWith (colours::oakPore, (pores - 0.82f) * 3.0f);
                px.setPixelColour (x, y, c);
            }
        }
        return image;
    }

private:
    static float hash (int x, int y) noexcept
    {
        juce::uint32 h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffffu) / (float) 0xffffffu;
    }
    static float noise (float x, float y) noexcept
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        float fx = x - (float) xi, fy = y - (float) yi;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        const float a = hash (xi, yi), b = hash (xi + 1, yi), c = hash (xi, yi + 1), d = hash (xi + 1, yi + 1);
        return juce::jmap (fy, juce::jmap (fx, a, b), juce::jmap (fx, c, d));
    }
    juce::Image image;
};

class ChopLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChopLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, colours::oakMid);
        setColour (juce::Label::textColourId, colours::engraved);
        setColour (juce::Slider::textBoxTextColourId, colours::readoutText);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderFillColourId, colours::ledOn);
        setColour (juce::Slider::rotarySliderOutlineColourId, colours::engraved.withAlpha (0.55f));
        setColour (juce::Slider::thumbColourId, colours::steelLight);
        setColour (juce::Slider::trackColourId, colours::ledOn);
        setColour (juce::Slider::backgroundColourId, colours::readout);
        setColour (juce::ComboBox::backgroundColourId, colours::readout);
        setColour (juce::ComboBox::outlineColourId, colours::brassDark);
        setColour (juce::ComboBox::textColourId, colours::readoutText);
        setColour (juce::ComboBox::arrowColourId, colours::readoutText);
        setColour (juce::PopupMenu::backgroundColourId, colours::readout);
        setColour (juce::PopupMenu::textColourId, colours::readoutText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::brassDark);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, colours::steelMid);
        setColour (juce::TextButton::buttonOnColourId, colours::ledOn);
        setColour (juce::TextButton::textColourOffId, colours::engraved);
        setColour (juce::TextButton::textColourOnId, colours::engraved);
        setColour (juce::ToggleButton::textColourId, colours::engraved);
        setColour (juce::ToggleButton::tickColourId, colours::ledOn);
        setColour (juce::ToggleButton::tickDisabledColourId, colours::ledOff);
        setColour (juce::TooltipWindow::backgroundColourId, colours::readout);
        setColour (juce::TooltipWindow::textColourId, colours::readoutText);
        setColour (juce::TooltipWindow::outlineColourId, colours::brassDark);
        setColour (juce::AlertWindow::backgroundColourId, colours::oakDark);
        setColour (juce::AlertWindow::textColourId, colours::engravedHi);
    }

    // ------------------------------------------------------------------ cabinet helpers
    void drawWood (juce::Graphics& g, juce::Rectangle<int> area, float darken = 0.0f)
    {
        const auto& img = wood.get (juce::jmax (1, area.getWidth()), juce::jmax (1, area.getHeight()));
        g.drawImageAt (img, area.getX(), area.getY());
        if (darken > 0.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (darken));
            g.fillRect (area);
        }
    }

    /** Varnish: a soft gloss band at the top and a vignette on the edges. */
    static void drawVarnish (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.16f), r.getX(), r.getY(),
                                    juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY() + r.getHeight() * 0.45f, false);
        g.setGradientFill (gloss);
        g.fillRect (r);
        juce::ColourGradient vignette (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                       juce::Colours::black.withAlpha (0.28f), r.getX(), r.getY(), true);
        g.setGradientFill (vignette);
        g.fillRect (r);
    }

    static void drawScrew (juce::Graphics& g, float cx, float cy, float radius, float angleRadians)
    {
        // countersink shadow
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (cx - radius * 1.25f, cy - radius * 1.1f, radius * 2.5f, radius * 2.5f);
        // head
        juce::ColourGradient head (colours::brassLight, cx - radius * 0.5f, cy - radius * 0.6f,
                                   colours::brassDark, cx + radius * 0.9f, cy + radius * 0.9f, true);
        head.addColour (0.55, colours::brass);
        g.setGradientFill (head);
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (colours::brassDark.withAlpha (0.9f));
        g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);
        // cross slot
        juce::Path slot;
        const float sw = radius * 0.26f, sl = radius * 1.5f;
        slot.addRoundedRectangle (-sl * 0.5f, -sw * 0.5f, sl, sw, sw * 0.4f);
        slot.addRoundedRectangle (-sw * 0.5f, -sl * 0.5f, sw, sl, sw * 0.4f);
        slot.applyTransform (juce::AffineTransform::rotation (angleRadians).translated (cx, cy));
        g.setColour (juce::Colour (0xff3a2a08));
        g.fillPath (slot);
        g.setColour (colours::brassLight.withAlpha (0.6f));
        g.strokePath (slot, juce::PathStrokeType (0.6f), juce::AffineTransform::translation (0.0f, 0.7f));
        // specular
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (cx - radius * 0.55f, cy - radius * 0.75f, radius * 0.5f, radius * 0.3f);
    }

    static void drawScrewsAtCorners (juce::Graphics& g, juce::Rectangle<float> r, float inset, float radius)
    {
        drawScrew (g, r.getX() + inset, r.getY() + inset, radius, 0.4f);
        drawScrew (g, r.getRight() - inset, r.getY() + inset, radius, 1.1f);
        drawScrew (g, r.getX() + inset, r.getBottom() - inset, radius, 2.3f);
        drawScrew (g, r.getRight() - inset, r.getBottom() - inset, radius, 0.9f);
    }

    /** A recessed wooden inlay with a thin brass trim. */
    void drawInlay (juce::Graphics& g, juce::Rectangle<int> area, float radius = 8.0f)
    {
        auto r = area.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.5f), radius);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, radius);
            g.reduceClipRegion (clip);
            drawWood (g, area, 0.22f);
            juce::ColourGradient innerShadow (juce::Colours::black.withAlpha (0.35f), r.getX(), r.getY(),
                                              juce::Colours::transparentBlack, r.getX(), r.getY() + 10.0f, false);
            g.setGradientFill (innerShadow);
            g.fillRect (r.withHeight (10.0f));
        }
        g.setColour (colours::brass.withAlpha (0.85f));
        g.drawRoundedRectangle (r.reduced (0.75f), radius, 1.5f);
        g.setColour (colours::brassDark.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (2.0f), radius - 1.0f, 0.8f);
    }

    /** A dark glass screen bezel around the display components. */
    static void drawScreenBezel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.expanded (1.0f).translated (0.0f, 2.0f), 10.0f);
        juce::ColourGradient bezel (juce::Colour (0xff3a3d44), r.getX(), r.getY(), juce::Colour (0xff15171b), r.getX(), r.getBottom(), false);
        g.setGradientFill (bezel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (colours::brass.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (colours::background);
        g.fillRoundedRectangle (r.reduced (5.0f), 7.0f);
    }

    static void drawBrassPlate (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 4.0f);
        juce::ColourGradient grad (colours::brassLight, r.getX(), r.getY(), colours::brass, r.getX(), r.getBottom(), false);
        grad.addColour (0.5, colours::brass.brighter (0.15f));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (colours::brassDark);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        const float sr = juce::jmin (3.2f, r.getHeight() * 0.14f);
        drawScrew (g, r.getX() + 8.0f, r.getCentreY(), sr, 0.6f);
        drawScrew (g, r.getRight() - 8.0f, r.getCentreY(), sr, 1.9f);
    }

    static void drawEngravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                                  juce::Justification just, juce::Colour ink = colours::engraved, float alphaHi = 0.55f)
    {
        const bool lightInk = ink.getPerceivedBrightness() > 0.5f;
        g.setColour (lightInk ? juce::Colours::black.withAlpha (0.7f) : colours::engravedHi.withAlpha (alphaHi));
        g.drawText (text, area.translated (0, 1), just, false);
        g.setColour (ink);
        g.drawText (text, area, just, false);
    }

    static void drawReadoutBox (juce::Graphics& g, juce::Rectangle<float> r, float radius = 3.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), radius);
        g.setColour (colours::readout);
        g.fillRoundedRectangle (r, radius);
        juce::ColourGradient glass (juce::Colours::white.withAlpha (0.08f), r.getX(), r.getY(),
                                    juce::Colours::transparentBlack, r.getX(), r.getCentreY(), false);
        g.setGradientFill (glass);
        g.fillRoundedRectangle (r, radius);
        g.setColour (colours::brassDark.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    static void drawMetalFace (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour tint, bool pressed, bool lit)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.translated (0.0f, pressed ? 0.5f : 1.5f), radius);
        juce::Colour top = colours::steelLight, bottom = colours::steelDark;
        if (tint != colours::steelMid)
        {
            top = tint.brighter (0.5f);
            bottom = tint.darker (0.35f);
        }
        if (pressed) std::swap (top, bottom);
        juce::ColourGradient grad (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
        grad.addColour (0.5, top.interpolatedWith (bottom, 0.55f));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, radius);
        // brushed streaks
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        for (float y = r.getY() + 2.0f; y < r.getBottom() - 1.0f; y += 2.0f)
            g.drawHorizontalLine ((int) y, r.getX() + 1.0f, r.getRight() - 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.0f);
        if (lit)
        {
            auto led = r.reduced (3.0f).removeFromBottom (3.0f);
            g.setColour (colours::ledOn);
            g.fillRoundedRectangle (led, 1.5f);
            g.setColour (colours::ledOn.withAlpha (0.35f));
            g.fillRoundedRectangle (led.expanded (2.0f), 3.0f);
        }
    }

    // ------------------------------------------------------------------ rotary: brushed steel knob
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const bool enabled = slider.isEnabled();

        // value arc engraved in the wood, lit amber
        const float arcR = radius - 1.5f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (colours::engraved.withAlpha (0.45f));
        g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float zeroAngle = bipolar ? rotaryStartAngle + (float) ((0.0 - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum()))
                                                               * (rotaryEndAngle - rotaryStartAngle)
                                        : rotaryStartAngle;
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
        g.setColour ((enabled ? colours::ledOn : colours::ledOff).withAlpha (enabled ? 0.95f : 0.6f));
        g.strokePath (value, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (enabled)
        {
            g.setColour (colours::ledOn.withAlpha (0.25f));
            g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // knob body
        const float kr = radius * 0.74f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (centre.translated (0.0f, kr * 0.18f)));
        juce::ColourGradient rim (colours::steelLight, centre.x - kr * 0.6f, centre.y - kr * 0.8f,
                                  colours::steelDark, centre.x + kr * 0.7f, centre.y + kr * 0.9f, false);
        rim.addColour (0.5, colours::steelMid);
        g.setGradientFill (rim);
        g.fillEllipse (juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (centre));
        // knurled edge
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        const int teeth = 36;
        for (int i = 0; i < teeth; ++i)
        {
            const float a = (float) i / teeth * juce::MathConstants<float>::twoPi;
            g.drawLine (centre.x + std::sin (a) * kr * 0.9f, centre.y - std::cos (a) * kr * 0.9f,
                        centre.x + std::sin (a) * kr, centre.y - std::cos (a) * kr, 1.0f);
        }
        // cap with radial brushing
        const float cr = kr * 0.78f;
        juce::ColourGradient cap (colours::steelMid.brighter (0.25f), centre.x - cr * 0.4f, centre.y - cr * 0.5f,
                                  colours::steelDark.darker (0.1f), centre.x + cr * 0.6f, centre.y + cr * 0.6f, true);
        g.setGradientFill (cap);
        g.fillEllipse (juce::Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (centre));
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int i = 0; i < 24; ++i)
        {
            const float a = (float) i / 24.0f * juce::MathConstants<float>::twoPi + 0.1f;
            g.drawLine (centre.x, centre.y, centre.x + std::sin (a) * cr, centre.y - std::cos (a) * cr, 0.8f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (juce::Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (centre), 1.0f);
        // pointer
        juce::Point<float> tipA (centre.x + std::sin (angle) * cr * 0.25f, centre.y - std::cos (angle) * cr * 0.25f);
        juce::Point<float> tipB (centre.x + std::sin (angle) * cr * 0.9f, centre.y - std::cos (angle) * cr * 0.9f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine ({ tipA.translated (0.0f, 1.0f), tipB.translated (0.0f, 1.0f) }, 2.4f);
        g.setColour (enabled ? colours::ledOn : colours::steelLight);
        g.drawLine ({ tipA, tipB }, 2.2f);
        // specular
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillEllipse (centre.x - cr * 0.55f, centre.y - cr * 0.8f, cr * 0.7f, cr * 0.35f);
    }

    // ------------------------------------------------------------------ linear slider: brass rail
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
            return;
        }
        auto r = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto rail = r.withSizeKeepingCentre (r.getWidth() - 10.0f, 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (rail.translated (0.0f, 1.0f), 3.0f);
        g.setColour (colours::readout);
        g.fillRoundedRectangle (rail, 3.0f);
        g.setColour (colours::ledOn.withAlpha (slider.isEnabled() ? 0.95f : 0.4f));
        g.fillRoundedRectangle (rail.withRight (juce::jmax (rail.getX() + 3.0f, sliderPos)), 3.0f);
        g.setColour (colours::brassDark);
        g.drawRoundedRectangle (rail, 3.0f, 1.0f);
        // thumb
        const float tr = juce::jmin (9.0f, r.getHeight() * 0.45f);
        juce::Point<float> c (sliderPos, r.getCentreY());
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (tr * 2.0f, tr * 2.0f).withCentre (c.translated (0.0f, 1.5f)));
        juce::ColourGradient grad (colours::steelLight, c.x - tr * 0.5f, c.y - tr * 0.7f, colours::steelDark, c.x + tr * 0.7f, c.y + tr * 0.8f, false);
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (tr * 2.0f, tr * 2.0f).withCentre (c));
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (juce::Rectangle<float> (tr * 2.0f, tr * 2.0f).withCentre (c), 1.0f);
        g.setColour (colours::ledOn);
        g.drawLine (c.x, c.y - tr * 0.6f, c.x, c.y + tr * 0.6f, 1.6f);
    }

    // ------------------------------------------------------------------ buttons
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& backgroundColour,
                               bool isMouseOver, bool isButtonDown) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto tint = backgroundColour;
        if (isMouseOver && ! isButtonDown) tint = tint.brighter (0.08f);
        drawMetalFace (g, r, 4.0f, tint, isButtonDown, b.getToggleState());
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool isButtonDown) override
    {
        auto font = getTextButtonFont (b, b.getHeight());
        g.setFont (font);
        auto area = b.getLocalBounds().reduced (3, 1);
        if (b.getToggleState()) area.removeFromBottom (3);
        if (isButtonDown) area.translate (0, 1);
        const auto ink = b.isEnabled() ? colours::engraved : colours::engraved.withAlpha (0.4f);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawFittedText (b.getButtonText(), area.translated (0, 1), juce::Justification::centred, 1, 0.9f);
        g.setColour (ink);
        g.drawFittedText (b.getButtonText(), area, juce::Justification::centred, 1, 0.9f);
    }

    juce::Font getTextButtonFont (juce::TextButton& b, int buttonHeight) override
    {
        const int size = juce::jmin (13, buttonHeight - 5, juce::jmax (9, b.getWidth() / 2));
        return juce::Font (juce::FontOptions ((float) size, juce::Font::bold));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool isMouseOver, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float h = juce::jmin (12.0f, r.getHeight() - 4.0f);
        auto led = juce::Rectangle<float> (h, h).withCentre ({ r.getX() + h * 0.5f + 3.0f, r.getCentreY() });
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (led.expanded (1.5f).translated (0.0f, 1.0f));
        g.setColour (colours::brassDark);
        g.fillEllipse (led.expanded (1.5f));
        const bool on = b.getToggleState();
        juce::ColourGradient grad ((on ? colours::ledOn.brighter (0.5f) : colours::ledOff.brighter (0.2f)), led.getX() + h * 0.3f, led.getY() + h * 0.3f,
                                   (on ? colours::ledOn.darker (0.2f) : colours::ledOff), led.getRight(), led.getBottom(), true);
        g.setGradientFill (grad);
        g.fillEllipse (led);
        if (on)
        {
            g.setColour (colours::ledOn.withAlpha (0.3f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        auto textArea = b.getLocalBounds().withTrimmedLeft ((int) (h + 10.0f));
        auto ink = b.findColour (juce::ToggleButton::textColourId);
        drawEngravedText (g, b.getButtonText(), textArea, juce::Justification::centredLeft,
                          isMouseOver ? ink.brighter (0.2f) : ink);
    }

    // ------------------------------------------------------------------ combo: amber readout
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        drawReadoutBox (g, r);
        juce::Path arrow;
        const float ax = (float) width - 11.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.9f : 0.35f));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override  { return juce::Font (juce::FontOptions (12.0f, juce::Font::bold)); }
    juce::Font getPopupMenuFont() override                 { return juce::Font (juce::FontOptions (13.0f)); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, box.findColour (juce::ComboBox::textColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    }

    // ------------------------------------------------------------------ labels: engraved on wood / readouts
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (label.isBeingEdited()) return;
        auto* parent = label.getParentComponent();
        const bool isSliderBox = dynamic_cast<juce::Slider*> (parent) != nullptr;
        const bool isComboText = dynamic_cast<juce::ComboBox*> (parent) != nullptr;
        g.setFont (label.getFont());
        auto area = label.getBorderSize().subtractedFrom (label.getLocalBounds());

        if (isSliderBox)
        {
            drawReadoutBox (g, label.getLocalBounds().toFloat().reduced (1.0f), 3.0f);
            g.setColour (colours::readoutText.withAlpha (label.isEnabled() ? 1.0f : 0.4f));
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (label.getText(), area, juce::Justification::centred, false);
            return;
        }
        if (isComboText)
        {
            g.setColour (label.findColour (juce::Label::textColourId));
            g.drawText (label.getText(), area, label.getJustificationType(), false);
            return;
        }
        drawEngravedText (g, label.getText(), area, label.getJustificationType(),
                          label.findColour (juce::Label::textColourId).withAlpha (label.isEnabled() ? 1.0f : 0.5f));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.fillAll (colours::readout);
        g.setColour (colours::brassDark);
        g.drawRect (0, 0, width, height, 1);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        auto r = juce::Rectangle<int> (0, 0, width, height).toFloat();
        drawReadoutBox (g, r.reduced (0.5f), 4.0f);
        g.setColour (colours::readoutText);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (text, juce::Rectangle<int> (0, 0, width, height).reduced (8, 4), juce::Justification::centredLeft, 6);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
    {
        const juce::TextLayout tl (layoutTooltipText (tipText, colours::readoutText));
        const int w = (int) (tl.getWidth() + 18.0f), h = (int) (tl.getHeight() + 10.0f);
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
                   .constrainedWithin (parentArea);
    }

private:
    static juce::TextLayout layoutTooltipText (const juce::String& text, juce::Colour colour)
    {
        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, juce::Font (juce::FontOptions (12.0f)), colour);
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, 400.0f);
        return tl;
    }

    WoodTexture wood;
};
} // namespace cf::ui
