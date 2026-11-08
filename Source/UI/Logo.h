#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace kerf::ui::logo
{
/** A circular saw blade outline, hooked teeth, arbor hole and three vent slots. */
inline juce::Path sawBlade (juce::Point<float> c, float r, int teeth = 18)
{
    juce::Path p;
    p.setUsingNonZeroWinding (false);
    const float step = juce::MathConstants<float>::twoPi / (float) teeth;
    for (int i = 0; i < teeth; ++i)
    {
        const float a0 = (float) i * step;
        const float a1 = a0 + step * 0.62f;            // gullet
        const float a2 = a0 + step;                    // next tip
        const juce::Point<float> tip (c.x + std::cos (a0) * r, c.y + std::sin (a0) * r);
        const juce::Point<float> gullet (c.x + std::cos (a1) * r * 0.80f, c.y + std::sin (a1) * r * 0.80f);
        const juce::Point<float> shoulder (c.x + std::cos (a2 - step * 0.12f) * r * 0.86f, c.y + std::sin (a2 - step * 0.12f) * r * 0.86f);
        if (i == 0) p.startNewSubPath (tip); else p.lineTo (tip);
        p.lineTo (gullet);
        p.lineTo (shoulder);
    }
    p.closeSubPath();
    p.addEllipse (juce::Rectangle<float> (r * 0.30f, r * 0.30f).withCentre (c));          // arbor hole
    for (int i = 0; i < 3; ++i)                                                             // vent slots
    {
        juce::Path slot;
        slot.addRoundedRectangle (-r * 0.05f, -r * 0.66f, r * 0.10f, r * 0.26f, r * 0.05f);
        slot.applyTransform (juce::AffineTransform::rotation ((float) i * juce::MathConstants<float>::twoPi / 3.0f + 0.5f).translated (c));
        p.addPath (slot);
    }
    return p;
}

/** A hand-saw blade silhouette with teeth along the bottom edge. */
inline juce::Path handSaw (juce::Rectangle<float> r, int teeth = 11)
{
    juce::Path p;
    const float toothH = r.getHeight() * 0.22f;
    const float body = r.getBottom() - toothH;
    p.startNewSubPath (r.getX(), r.getY() + r.getHeight() * 0.35f);
    p.lineTo (r.getX() + r.getWidth() * 0.22f, r.getY());                 // heel
    p.lineTo (r.getRight(), r.getY() + r.getHeight() * 0.18f);            // back edge (tapers to the toe)
    p.lineTo (r.getRight(), body);
    const float tw = r.getWidth() / (float) teeth;
    for (int i = teeth; i > 0; --i)
    {
        const float x1 = r.getX() + tw * (float) i, x0 = x1 - tw;
        p.lineTo (x0 + tw * 0.35f, r.getBottom());                        // tooth tip
        p.lineTo (x0, body);
    }
    p.closeSubPath();
    return p;
}

inline juce::Path textPath (const juce::String& text, juce::Font font, float x, float baselineY)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, x, baselineY);
    juce::Path p;
    ga.createPath (p);
    return p;
}

inline float textWidth (const juce::String& text, const juce::Font& font)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

/** Builds the whole stamp as one path so it can be inked and weathered in one go.
    variant 0: circular blade + KERF wordmark with a kerf cut through the baseline
    variant 1: hand-saw silhouette with KERF cut out of the blade
    variant 2: round seal: toothed ring, KERF inside, saw cut through the ring */
inline juce::Path stamp (juce::Rectangle<float> area, int variant)
{
    juce::Path p;
    p.setUsingNonZeroWinding (false);
    const float h = area.getHeight();
    if (variant == 0)
    {
        const float r = h * 0.46f;
        const juce::Point<float> c (area.getX() + r + 2.0f, area.getCentreY());
        p.addPath (sawBlade (c, r));
        juce::Font f (juce::FontOptions (h * 0.72f, juce::Font::bold));
        f.setExtraKerningFactor (0.10f);
        const float tx = c.x + r + h * 0.22f;
        const float baseline = area.getCentreY() + h * 0.24f;
        p.addPath (textPath ("KERF", f, tx, baseline));
        // baseline rule with a kerf (a gap) cut into it
        const float w = textWidth ("KERF", f);
        const float ry = baseline + h * 0.12f, rh = juce::jmax (1.5f, h * 0.05f);
        p.addRectangle (tx, ry, w * 0.42f, rh);
        p.addRectangle (tx + w * 0.50f, ry, w * 0.50f, rh);
    }
    else if (variant == 1)
    {
        auto blade = area.withTrimmedTop (h * 0.08f).withTrimmedBottom (h * 0.04f);
        p.addPath (handSaw (blade));
        juce::Font f (juce::FontOptions (h * 0.50f, juce::Font::bold));
        f.setExtraKerningFactor (0.14f);
        const float w = textWidth ("KERF", f);
        // text cut out of the blade (even-odd makes it a hole)
        p.addPath (textPath ("KERF", f, blade.getX() + (blade.getWidth() - w) * 0.55f, blade.getY() + h * 0.56f));
    }
    else
    {
        const float r = h * 0.48f;
        const juce::Point<float> c (area.getX() + r + 2.0f, area.getCentreY());
        // toothed ring: outer blade outline minus an inner disc (no arbor or vents)
        juce::Path ring;
        ring.setUsingNonZeroWinding (false);
        const int teeth = 24;
        const float step = juce::MathConstants<float>::twoPi / (float) teeth;
        for (int i = 0; i < teeth; ++i)
        {
            const float a0 = (float) i * step, a1 = a0 + step * 0.62f, a2 = a0 + step;
            const juce::Point<float> tip (c.x + std::cos (a0) * r, c.y + std::sin (a0) * r);
            const juce::Point<float> gullet (c.x + std::cos (a1) * r * 0.84f, c.y + std::sin (a1) * r * 0.84f);
            const juce::Point<float> shoulder (c.x + std::cos (a2 - step * 0.12f) * r * 0.88f, c.y + std::sin (a2 - step * 0.12f) * r * 0.88f);
            if (i == 0) ring.startNewSubPath (tip); else ring.lineTo (tip);
            ring.lineTo (gullet);
            ring.lineTo (shoulder);
        }
        ring.closeSubPath();
        ring.addEllipse (juce::Rectangle<float> (r * 1.44f, r * 1.44f).withCentre (c));
        p.addPath (ring);
        juce::Font f (juce::FontOptions (r * 0.50f, juce::Font::bold));
        f.setExtraKerningFactor (0.08f);
        const float w = textWidth ("KERF", f);
        p.addPath (textPath ("KERF", f, c.x - w * 0.5f, c.y + r * 0.16f));
        // saw cut straight through the seal, below the word
        p.addRectangle (c.x - r * 0.98f, c.y + r * 0.36f, r * 1.96f, juce::jmax (1.5f, r * 0.06f));
    }
    return p;
}

/** Screen-printed ink on metal: flat ink, slightly worn by the brushing underneath. */
inline void drawStamp (juce::Graphics& g, juce::Rectangle<float> area, int variant, juce::Colour ink,
                       const juce::Image* wearTexture = nullptr)
{
    auto p = stamp (area, variant);
    g.setColour (ink);
    g.fillPath (p);
    if (wearTexture != nullptr)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (p);
        g.setTiledImageFill (*wearTexture, (int) area.getX(), (int) area.getY(), 0.30f);
        g.fillRect (area.expanded (4.0f));
    }
}
} // namespace kerf::ui::logo
