#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>

namespace cf::ui
{
/* The "screen" (waveform + step grid) keeps a dark display look; everything around it is a
   hand-built varnished oak cabinet with brass screws and brushed-steel controls. Every texture
   is rendered per pixel with a simple lighting model and cached; there are no image resources. */
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
    const juce::Colour oakLight     { 0xffd6ad75 };
    const juce::Colour oakMid       { 0xffb07f45 };
    const juce::Colour oakDark      { 0xff6f4621 };
    const juce::Colour oakPore      { 0xff3a2210 };
    const juce::Colour engraved     { 0xff2a1706 };
    const juce::Colour engravedHi   { 0xffefd3a0 };
    const juce::Colour brass        { 0xffc39a3e };
    const juce::Colour brassLight   { 0xfff5df98 };
    const juce::Colour brassDark    { 0xff6e4f0c };
    const juce::Colour steelLight   { 0xffe9ebee };
    const juce::Colour steelMid     { 0xffa3a7ae };
    const juce::Colour steelDark    { 0xff4f535a };
    const juce::Colour readout      { 0xff0f0d0a };
    const juce::Colour readoutText  { 0xffffb648 };
    const juce::Colour ledOff       { 0xff3a2a12 };
    const juce::Colour ledOn        { 0xffffa21f };
}

// ------------------------------------------------------------------------------------------ noise
namespace tex
{
    struct Vec3
    {
        float x, y, z;
        constexpr Vec3 (float xx, float yy, float zz) noexcept : x (xx), y (yy), z (zz) {}
        Vec3 normalised() const noexcept { const float l = std::sqrt (x * x + y * y + z * z); return { x / l, y / l, z / l }; }
        float operator* (const Vec3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
        Vec3 operator+ (const Vec3& o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
    };
    inline float hash (int x, int y) noexcept
    {
        juce::uint32 h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffffu) / (float) 0xffffffu;
    }
    inline float noise (float x, float y) noexcept
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        float fx = x - (float) xi, fy = y - (float) yi;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        const float a = hash (xi, yi), b = hash (xi + 1, yi), c = hash (xi, yi + 1), d = hash (xi + 1, yi + 1);
        return juce::jmap (fy, juce::jmap (fx, a, b), juce::jmap (fx, c, d));
    }
    inline float fbm (float x, float y, int octaves) noexcept
    {
        float v = 0.0f, amp = 0.5f, sum = 0.0f;
        for (int i = 0; i < octaves; ++i)
        {
            v += noise (x, y) * amp;
            sum += amp;
            x *= 2.03f; y *= 2.11f; amp *= 0.5f;
        }
        return v / sum;
    }
    inline juce::Colour shade (juce::Colour c, float factor) noexcept
    {
        const float r = juce::jlimit (0.0f, 1.0f, c.getFloatRed() * factor);
        const float g = juce::jlimit (0.0f, 1.0f, c.getFloatGreen() * factor);
        const float b = juce::jlimit (0.0f, 1.0f, c.getFloatBlue() * factor);
        return juce::Colour::fromFloatRGBA (r, g, b, c.getFloatAlpha());
    }
    inline juce::Colour addLight (juce::Colour c, float amount) noexcept
    {
        return juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, c.getFloatRed() + amount),
                                            juce::jlimit (0.0f, 1.0f, c.getFloatGreen() + amount),
                                            juce::jlimit (0.0f, 1.0f, c.getFloatBlue() + amount), c.getFloatAlpha());
    }
}

/** Procedural textures, cached. */
class Textures
{
public:
    /** Varnished oak, several boards with seams, cathedral grain, pores and rays. */
    const juce::Image& wood (int w, int h)
    {
        // generated in 256 px steps so that live window resizing does not re-render every frame
        w = juce::jmax (256, ((w + 255) / 256) * 256);
        h = juce::jmax (256, ((h + 255) / 256) * 256);
        if (woodImg.isValid() && woodImg.getWidth() == w && woodImg.getHeight() == h) return woodImg;
        woodImg = juce::Image (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), false);
        juce::Image::BitmapData px (woodImg, juce::Image::BitmapData::writeOnly);
        const int boards = juce::jmax (1, (int) std::round ((float) h / 150.0f));
        const float boardH = (float) h / (float) boards;

        for (int y = 0; y < woodImg.getHeight(); ++y)
        {
            const int   b     = juce::jmin (boards - 1, (int) ((float) y / boardH));
            const float ly    = (float) y - (float) b * boardH;
            const float seed  = tex::hash (b * 17 + 3, 91) * 1000.0f;
            const float hueOf = (tex::hash (b, 7) - 0.5f) * 0.18f;      // per-board tint
            for (int x = 0; x < woodImg.getWidth(); ++x)
            {
                const float fx = (float) x + seed;
                // growth rings: long horizontal lines bent into cathedral arches by slow warping
                // broad cathedral arches (very slow along x) plus a gentle wobble
                const float arch  = (tex::noise (fx * 0.0011f, 3.1f + (float) b) - 0.5f) * 170.0f
                                  + (tex::noise (fx * 0.0045f, ly * 0.006f) - 0.5f) * 18.0f
                                  + tex::fbm (fx * 0.02f, ly * 0.05f, 3) * 3.0f;
                const float spacing = 0.32f + 0.22f * tex::noise (fx * 0.0025f + 77.0f, ly * 0.012f);
                const float field = (ly + arch) * spacing;
                float ring = 0.5f + 0.5f * std::sin (field);
                const float late = std::pow (ring, 7.0f) * (0.55f + 0.45f * tex::noise (fx * 0.006f, ly * 0.03f + 50.0f));
                const float early = std::pow (1.0f - ring, 2.0f) * 0.10f;   // slightly lighter earlywood
                // base tone drifts slowly
                const float broad = tex::fbm (fx * 0.0015f, ly * 0.004f, 3);
                float t = 0.60f + (broad - 0.5f) * 0.45f + hueOf - late * 0.62f + early;
                juce::Colour c = colours::oakDark.interpolatedWith (colours::oakLight, juce::jlimit (0.0f, 1.0f, t));
                // open pores: many short, fine dark dashes along the grain, denser in latewood
                const float pores = tex::noise (fx * 0.7f, ly * 4.2f);
                const float poreThreshold = 0.74f - late * 0.12f;
                if (pores > poreThreshold) c = c.interpolatedWith (colours::oakPore, juce::jmin (0.5f, (pores - poreThreshold) * 3.0f));
                // medullary rays: faint light flecks
                const float rays = tex::noise (fx * 0.15f, ly * 1.6f + 200.0f);
                if (rays > 0.86f) c = tex::addLight (c, (rays - 0.86f) * 0.8f);
                // fine fibre texture
                const float fibre = tex::noise (fx * 0.25f, ly * 2.0f + 400.0f);
                c = tex::shade (c, 0.95f + fibre * 0.08f + tex::hash (x, y) * 0.03f);
                // board seams: a dark groove with a lit lower edge
                if (b > 0 && ly < 1.5f) c = tex::shade (c, 0.35f);
                else if (b > 0 && ly < 2.5f) c = tex::shade (c, 0.6f);
                else if (b > 0 && ly < 3.5f) c = tex::addLight (c, 0.10f);
                if (b < boards - 1 && boardH - ly < 1.5f) c = tex::shade (c, 0.55f);
                px.setPixelColour (x, y, c);
            }
        }
        return woodImg;
    }

    /** Tileable-enough brushed metal strip in a tint. Streaks run horizontally. */
    const juce::Image& brushed (juce::Colour tint)
    {
        auto it = brushedImgs.find (tint.getARGB());
        if (it != brushedImgs.end()) return it->second;
        juce::Image img (juce::Image::ARGB, 1024, 160, false);
        juce::Image::BitmapData px (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const float streak = tex::noise ((float) x * 0.018f, (float) y * 2.3f) * 0.55f
                                   + tex::noise ((float) x * 0.06f, (float) y * 4.7f) * 0.3f
                                   + tex::hash (x, y) * 0.15f;
                px.setPixelColour (x, y, tex::shade (tint, 0.78f + streak * 0.42f));
            }
        return brushedImgs.emplace (tint.getARGB(), std::move (img)).first->second;
    }

    /** A lathe-turned steel knob: knurled chamfer ring and a shallow spun dome, lit from top-left. */
    const juce::Image& knob (int diameter)
    {
        auto it = knobImgs.find (diameter);
        if (it != knobImgs.end()) return it->second;
        const int d = juce::jmax (16, diameter * 2);   // rendered at 2x, drawn scaled down
        juce::Image img (juce::Image::ARGB, d, d, true);
        juce::Image::BitmapData px (img, juce::Image::BitmapData::writeOnly);
        const float R = (float) d * 0.5f;
        const tex::Vec3 L = tex::Vec3 (-0.55f, -0.7f, 0.65f).normalised();
        const tex::Vec3 H = (L + tex::Vec3 (0.0f, 0.0f, 1.0f)).normalised();
        const float lightAngle = std::atan2 (-0.55f, 0.7f);
        for (int y = 0; y < d; ++y)
            for (int x = 0; x < d; ++x)
            {
                const float dx = ((float) x + 0.5f - R) / R, dy = ((float) y + 0.5f - R) / R;
                const float r = std::sqrt (dx * dx + dy * dy);
                if (r > 1.0f) continue;
                const float alpha = juce::jlimit (0.0f, 1.0f, (1.0f - r) * R);
                const float angle = std::atan2 (dy, dx);
                tex::Vec3 n (0.0f, 0.0f, 1.0f);
                float base = 0.60f, knurl = 0.0f, spun = 0.0f, aniso = 0.0f, groove = 1.0f;
                const float rimStart = 0.78f;
                if (r > rimStart)
                {
                    // knurled, chamfered rim: 32 ridges, sloping outwards
                    const float t = (r - rimStart) / (1.0f - rimStart);
                    knurl = 0.5f + 0.5f * std::cos (angle * 32.0f);
                    const float ridge = (knurl - 0.5f) * 1.4f;
                    const float ra = angle + ridge * 0.25f;
                    n = tex::Vec3 (std::cos (ra) * (0.6f + t * 0.6f), std::sin (ra) * (0.6f + t * 0.6f), 0.75f - t * 0.45f).normalised();
                    base = 0.52f;
                    if (t < 0.12f) groove = 0.55f;                        // dark groove between rim and cap
                }
                else
                {
                    // shallow dome with a spun (lathe) finish and radial brushing
                    const float rr = r / rimStart;
                    const float z = std::sqrt (juce::jmax (0.0f, 1.0f - rr * rr * 0.45f));
                    n = tex::Vec3 (dx * 0.75f, dy * 0.75f, z).normalised();
                    spun = (tex::hash ((int) (rr * 220.0f), 3) - 0.5f) * 0.09f + (tex::hash ((int) (rr * 70.0f), 9) - 0.5f) * 0.05f;
                    spun += (tex::hash ((int) ((angle + 3.2f) * 180.0f), 5) - 0.5f) * 0.05f * rr;
                    const float ca = std::cos (angle - lightAngle);
                    aniso = std::pow (std::abs (ca), 8.0f) * 0.28f * (0.2f + rr);
                    if (rr > 0.93f) groove = 0.7f;                        // cap edge
                    if (rr < 0.10f) base += 0.06f;                        // polished centre
                }
                const float diffuse = juce::jmax (0.0f, n * L);
                const float spec = std::pow (juce::jmax (0.0f, n * H), r > rimStart ? 14.0f : 60.0f);
                float v = base * (0.30f + 0.95f * diffuse) + spec * 0.6f + spun + aniso + (knurl - 0.5f) * 0.18f * (r > rimStart ? 1.0f : 0.0f);
                v = juce::jlimit (0.0f, 1.0f, v * groove);
                // cool steel tint
                juce::Colour c = juce::Colour::fromFloatRGBA (v * 0.96f, v * 0.975f, v, alpha);
                px.setPixelColour (x, y, c);
            }
        return knobImgs.emplace (diameter, img.rescaled (diameter, diameter, juce::Graphics::highResamplingQuality)).first->second;
    }

    /** Brass dome screw head with a cross slot. angleIndex 0..3 rotates the slot. */
    const juce::Image& screw (int diameter, int angleIndex)
    {
        const int key = diameter * 8 + (angleIndex & 7);
        auto it = screwImgs.find (key);
        if (it != screwImgs.end()) return it->second;
        const int d = juce::jmax (12, diameter * 3);     // rendered at 3x, drawn scaled down
        juce::Image img (juce::Image::ARGB, d, d, true);
        juce::Image::BitmapData px (img, juce::Image::BitmapData::writeOnly);
        const float R = (float) d * 0.5f;
        const tex::Vec3 L = tex::Vec3 (-0.5f, -0.65f, 0.7f).normalised();
        const tex::Vec3 H = (L + tex::Vec3 (0.0f, 0.0f, 1.0f)).normalised();
        const float rot = (float) angleIndex * 0.39f;
        const float cs = std::cos (rot), sn = std::sin (rot);
        for (int y = 0; y < d; ++y)
            for (int x = 0; x < d; ++x)
            {
                const float dx = ((float) x + 0.5f - R) / R, dy = ((float) y + 0.5f - R) / R;
                const float r = std::sqrt (dx * dx + dy * dy);
                if (r > 1.0f) continue;
                const float alpha = juce::jlimit (0.0f, 1.0f, (1.0f - r) * R);
                const float z = std::sqrt (juce::jmax (0.0f, 1.0f - r * r));
                tex::Vec3 n (dx, dy, z);
                // cross slot, rotated
                const float ux = dx * cs + dy * sn, uy = -dx * sn + dy * cs;
                const float sw = 0.19f, sl = 0.74f;
                const bool inSlot = (std::abs (ux) < sw && std::abs (uy) < sl) || (std::abs (uy) < sw && std::abs (ux) < sl);
                float depth = 1.0f;
                if (inSlot)
                {
                    depth = 0.30f;
                    // slot walls catch light on one side
                    const float wallA = std::abs (ux) < sw ? (sw - std::abs (ux)) / sw : 1.0f;
                    const float wallB = std::abs (uy) < sw ? (sw - std::abs (uy)) / sw : 1.0f;
                    const float wall = juce::jmin (wallA, wallB);
                    if (wall < 0.35f && (ux + uy) < 0.0f) depth = 1.15f;
                }
                const float diffuse = juce::jmax (0.0f, n * L);
                const float spec = std::pow (juce::jmax (0.0f, n * H), 30.0f);
                const float v = juce::jlimit (0.0f, 1.0f, (0.35f + 0.75f * diffuse) * depth);
                juce::Colour c = colours::brassDark.interpolatedWith (colours::brassLight, v);
                c = tex::addLight (c, spec * 0.6f * depth);
                px.setPixelColour (x, y, c.withAlpha (alpha));
            }
        const int out = juce::jmax (6, diameter);
        return screwImgs.emplace (key, img.rescaled (out, out, juce::Graphics::highResamplingQuality)).first->second;
    }

private:
    juce::Image woodImg;
    std::map<juce::uint32, juce::Image> brushedImgs;
    std::map<int, juce::Image> knobImgs, screwImgs;
};

class ChopLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr float kButtonRadius = 1.5f;   // near-square controls
    static constexpr float kPanelRadius  = 3.0f;

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

    Textures& textures() { return tx; }

    // ------------------------------------------------------------------ cabinet helpers
    void drawWood (juce::Graphics& g, juce::Rectangle<int> area, float darken = 0.0f)
    {
        const auto& img = tx.wood (juce::jmax (1, area.getWidth()), juce::jmax (1, area.getHeight()));
        g.drawImage (img, area.toFloat(), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop | juce::RectanglePlacement::doNotResize);
        if (darken > 0.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (darken));
            g.fillRect (area);
        }
    }

    /** Varnish: broad gloss, a diagonal sheen, and an edge vignette. */
    static void drawVarnish (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.13f), r.getX(), r.getY(),
                                    juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY() + r.getHeight() * 0.5f, false);
        g.setGradientFill (gloss);
        g.fillRect (r);
        juce::ColourGradient sheen (juce::Colours::transparentWhite, r.getX(), r.getBottom(),
                                    juce::Colours::transparentWhite, r.getRight(), r.getY(), false);
        sheen.addColour (0.42, juce::Colours::white.withAlpha (0.0f));
        sheen.addColour (0.5, juce::Colours::white.withAlpha (0.09f));
        sheen.addColour (0.58, juce::Colours::white.withAlpha (0.0f));
        g.setGradientFill (sheen);
        g.fillRect (r);
        juce::ColourGradient vignette (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                       juce::Colours::black.withAlpha (0.33f), r.getX(), r.getY(), true);
        g.setGradientFill (vignette);
        g.fillRect (r);
    }

    void drawScrew (juce::Graphics& g, float cx, float cy, float radius, int angleIndex)
    {
        // countersink: dark ring and a soft shadow below
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (cx - radius * 1.2f, cy - radius * 1.05f, radius * 2.4f, radius * 2.5f);
        g.setColour (juce::Colour (0xff1a0e04));
        g.fillEllipse (cx - radius * 1.12f, cy - radius * 1.12f, radius * 2.24f, radius * 2.24f);
        const int d = juce::jmax (6, (int) std::round (radius * 2.0f));
        const auto& img = tx.screw (d, angleIndex);
        g.drawImage (img, juce::Rectangle<float> ((float) d, (float) d).withCentre ({ cx, cy }));
    }

    void drawScrewsAtCorners (juce::Graphics& g, juce::Rectangle<float> r, float inset, float radius)
    {
        drawScrew (g, r.getX() + inset, r.getY() + inset, radius, 0);
        drawScrew (g, r.getRight() - inset, r.getY() + inset, radius, 3);
        drawScrew (g, r.getX() + inset, r.getBottom() - inset, radius, 5);
        drawScrew (g, r.getRight() - inset, r.getBottom() - inset, radius, 2);
    }

    /** A recessed, routed wooden inlay with a thin brass trim. */
    void drawInlay (juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto r = area.toFloat();
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, kPanelRadius);
            g.reduceClipRegion (clip);
            drawWood (g, area, 0.26f);
            // routed edge: shadow from the top and left walls
            juce::ColourGradient top (juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 12.0f, false);
            g.setGradientFill (top);
            g.fillRect (r.withHeight (12.0f));
            juce::ColourGradient left (juce::Colours::black.withAlpha (0.35f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX() + 8.0f, r.getY(), false);
            g.setGradientFill (left);
            g.fillRect (r.withWidth (8.0f));
            // lit bottom wall
            g.setColour (colours::oakLight.withAlpha (0.35f));
            g.fillRect (r.getX(), r.getBottom() - 1.5f, r.getWidth(), 1.5f);
        }
        // chamfer highlight above, shadow below, brass trim
        g.setColour (colours::oakLight.withAlpha (0.55f));
        g.drawRoundedRectangle (r.expanded (1.5f).translated (0.0f, -0.5f), kPanelRadius + 1.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (r.expanded (1.5f).translated (0.0f, 1.0f), kPanelRadius + 1.0f, 1.0f);
        g.setColour (colours::brass);
        g.drawRoundedRectangle (r.reduced (0.5f), kPanelRadius, 1.2f);
        g.setColour (colours::brassDark.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (1.6f), kPanelRadius - 1.0f, 0.8f);
    }

    /** Dark anodised frame around the display components. */
    void drawScreenBezel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.expanded (1.5f).translated (0.0f, 2.5f), kPanelRadius + 1.0f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, kPanelRadius);
            g.reduceClipRegion (clip);
            g.setTiledImageFill (tx.brushed (juce::Colour (0xff2c2f35)), (int) r.getX(), (int) r.getY(), 1.0f);
            g.fillRect (r);
            juce::ColourGradient light (juce::Colours::white.withAlpha (0.18f), r.getX(), r.getY(), juce::Colours::black.withAlpha (0.25f), r.getX(), r.getBottom(), false);
            g.setGradientFill (light);
            g.fillRect (r);
        }
        g.setColour (colours::brass.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), kPanelRadius, 1.0f);
        // glass recess
        auto inner = r.reduced (6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (inner.expanded (1.0f), 2.0f);
        g.setColour (colours::background);
        g.fillRoundedRectangle (inner, 2.0f);
        juce::ColourGradient innerShadow (juce::Colours::black.withAlpha (0.6f), inner.getX(), inner.getY(), juce::Colours::transparentBlack, inner.getX(), inner.getY() + 6.0f, false);
        g.setGradientFill (innerShadow);
        g.fillRect (inner.withHeight (6.0f));
    }

    void drawBrassPlate (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), kButtonRadius);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, kButtonRadius);
            g.reduceClipRegion (clip);
            g.setTiledImageFill (tx.brushed (colours::brass), (int) r.getX(), (int) r.getY(), 1.0f);
            g.fillRect (r);
            juce::ColourGradient light (juce::Colours::white.withAlpha (0.28f), r.getX(), r.getY(), juce::Colours::black.withAlpha (0.18f), r.getX(), r.getBottom(), false);
            light.addColour (0.45, juce::Colours::white.withAlpha (0.04f));
            g.setGradientFill (light);
            g.fillRect (r);
        }
        drawChamfer (g, r, kButtonRadius);
        const float sr = juce::jmin (3.5f, r.getHeight() * 0.15f);
        drawScrew (g, r.getX() + 9.0f, r.getCentreY(), sr, 1);
        drawScrew (g, r.getRight() - 9.0f, r.getCentreY(), sr, 4);
    }

    /** Bevelled edge: light top/left, dark bottom/right, black outline. */
    static void drawChamfer (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawLine (r.getX() + radius, r.getY() + 0.5f, r.getRight() - radius, r.getY() + 0.5f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawLine (r.getX() + 0.5f, r.getY() + radius, r.getX() + 0.5f, r.getBottom() - radius, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawLine (r.getX() + radius, r.getBottom() - 0.5f, r.getRight() - radius, r.getBottom() - 0.5f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine (r.getRight() - 0.5f, r.getY() + radius, r.getRight() - 0.5f, r.getBottom() - radius, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawRoundedRectangle (r, radius, 1.0f);
    }

    static void drawEngravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                                  juce::Justification just, juce::Colour ink = colours::engraved, float alphaHi = 0.55f)
    {
        const bool lightInk = ink.getPerceivedBrightness() > 0.5f;
        g.setColour (lightInk ? juce::Colours::black.withAlpha (0.75f) : colours::engravedHi.withAlpha (alphaHi));
        g.drawText (text, area.translated (0, 1), just, false);
        g.setColour (ink);
        g.drawText (text, area, just, false);
    }

    /** Recessed dark glass readout. */
    static void drawReadoutBox (juce::Graphics& g, juce::Rectangle<float> r, float radius = kButtonRadius)
    {
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), radius);
        g.setColour (colours::readout);
        g.fillRoundedRectangle (r, radius);
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.85f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 5.0f, false);
        g.setGradientFill (inner);
        g.fillRect (r.withHeight (5.0f));
        juce::ColourGradient glass (juce::Colours::transparentWhite, r.getX(), r.getCentreY(), juce::Colours::white.withAlpha (0.05f), r.getX(), r.getBottom(), false);
        g.setGradientFill (glass);
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colour (0xff1a1206));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    void drawMetalFace (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour tint, bool pressed, bool lit)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.0f, pressed ? 0.5f : 2.0f), kButtonRadius);
        if (pressed) r.translate (0.0f, 1.0f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, kButtonRadius);
            g.reduceClipRegion (clip);
            g.setTiledImageFill (tx.brushed (tint), (int) r.getX(), (int) r.getY(), 1.0f);
            g.fillRect (r);
            juce::Colour top = juce::Colours::white.withAlpha (pressed ? 0.02f : 0.30f);
            juce::Colour bottom = juce::Colours::black.withAlpha (pressed ? 0.35f : 0.22f);
            juce::ColourGradient light (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
            light.addColour (0.48, juce::Colours::white.withAlpha (0.06f));
            light.addColour (0.52, juce::Colours::black.withAlpha (0.04f));
            g.setGradientFill (light);
            g.fillRect (r);
        }
        drawChamfer (g, r, kButtonRadius);
        if (lit)
        {
            auto led = r.reduced (3.0f).removeFromBottom (2.5f);
            g.setColour (colours::ledOn.withAlpha (0.35f));
            g.fillRoundedRectangle (led.expanded (2.0f, 2.0f), 2.0f);
            g.setColour (colours::ledOn);
            g.fillRect (led);
        }
    }

    // ------------------------------------------------------------------ rotary: turned steel knob
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (1.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const bool enabled = slider.isEnabled();

        // value arc: a groove in the wood, lit amber
        const float arcR = radius - 2.0f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::oakLight.withAlpha (0.25f));
        g.strokePath (track, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (0.0f, 1.5f));
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float zeroAngle = bipolar ? rotaryStartAngle + (float) ((0.0 - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum()))
                                                               * (rotaryEndAngle - rotaryStartAngle)
                                        : rotaryStartAngle;
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
        if (enabled)
        {
            g.setColour (colours::ledOn.withAlpha (0.25f));
            g.strokePath (value, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour ((enabled ? colours::ledOn : colours::ledOff).withAlpha (enabled ? 1.0f : 0.6f));
        g.strokePath (value, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // knob: shadow, turned-steel image, pointer
        const float kr = radius * 0.80f;
        juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.6f), centre.x, centre.y + kr * 0.3f,
                                     juce::Colours::transparentBlack, centre.x, centre.y + kr * 1.5f, true);
        g.setGradientFill (shadow);
        g.fillEllipse (juce::Rectangle<float> (kr * 2.6f, kr * 2.6f).withCentre (centre.translated (0.0f, kr * 0.3f)));
        const int d = juce::jmax (8, (int) std::round (kr * 2.0f));
        g.drawImage (tx.knob (d), juce::Rectangle<float> ((float) d, (float) d).withCentre (centre));

        const float cr = kr * 0.80f;
        juce::Point<float> a (centre.x + std::sin (angle) * cr * 0.30f, centre.y - std::cos (angle) * cr * 0.30f);
        juce::Point<float> b (centre.x + std::sin (angle) * cr * 0.92f, centre.y - std::cos (angle) * cr * 0.92f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine ({ a, b }, 3.2f);
        g.setColour (enabled ? colours::ledOn : colours::steelLight.withAlpha (0.6f));
        g.drawLine ({ a, b }, 1.8f);
    }

    // ------------------------------------------------------------------ linear slider: steel fader on a rail
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
            return;
        }
        auto r = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto rail = r.withSizeKeepingCentre (r.getWidth() - 10.0f, 7.0f);
        drawReadoutBox (g, rail, 1.0f);
        g.setColour (colours::ledOn.withAlpha (slider.isEnabled() ? 0.95f : 0.4f));
        g.fillRect (rail.reduced (1.5f).withRight (juce::jmax (rail.getX() + 3.0f, sliderPos)));
        // fader cap: brushed steel block
        auto cap = juce::Rectangle<float> (12.0f, juce::jmin (18.0f, r.getHeight() - 2.0f)).withCentre ({ sliderPos, r.getCentreY() });
        drawMetalFace (g, cap, colours::steelMid, false, false);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine (cap.getCentreX(), cap.getY() + 3.0f, cap.getCentreX(), cap.getBottom() - 3.0f, 1.5f);
    }

    // ------------------------------------------------------------------ buttons
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& backgroundColour,
                               bool isMouseOver, bool isButtonDown) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f, 0.5f).withTrimmedBottom (1.5f);
        auto tint = backgroundColour;
        if (isMouseOver && ! isButtonDown) tint = tint.brighter (0.10f);
        drawMetalFace (g, r, tint, isButtonDown, b.getToggleState());
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool isButtonDown) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        auto area = b.getLocalBounds().reduced (3, 1).withTrimmedBottom (2);
        if (b.getToggleState()) area.removeFromBottom (3);
        if (isButtonDown) area.translate (0, 1);
        const auto ink = b.isEnabled() ? colours::engraved : colours::engraved.withAlpha (0.4f);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawFittedText (b.getButtonText(), area.translated (0, 1), juce::Justification::centred, 1, 0.9f);
        g.setColour (ink);
        g.drawFittedText (b.getButtonText(), area, juce::Justification::centred, 1, 0.9f);
    }

    juce::Font getTextButtonFont (juce::TextButton& b, int buttonHeight) override
    {
        const int size = juce::jmin (13, buttonHeight - 6, juce::jmax (9, b.getWidth() / 2));
        return juce::Font (juce::FontOptions ((float) size, juce::Font::bold));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool isMouseOver, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float h = juce::jmin (12.0f, r.getHeight() - 4.0f);
        auto led = juce::Rectangle<float> (h, h).withCentre ({ r.getX() + h * 0.5f + 3.0f, r.getCentreY() });
        // brass bezel ring
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (led.expanded (2.0f).translated (0.0f, 1.0f));
        juce::ColourGradient ring (colours::brassLight, led.getX(), led.getY(), colours::brassDark, led.getRight(), led.getBottom(), false);
        g.setGradientFill (ring);
        g.fillEllipse (led.expanded (2.0f));
        g.setColour (juce::Colours::black);
        g.fillEllipse (led.expanded (0.5f));
        const bool on = b.getToggleState();
        juce::ColourGradient grad ((on ? colours::ledOn.brighter (0.6f) : colours::ledOff.brighter (0.3f)), led.getX() + h * 0.3f, led.getY() + h * 0.3f,
                                   (on ? colours::ledOn.darker (0.3f) : colours::ledOff.darker (0.3f)), led.getRight(), led.getBottom(), true);
        g.setGradientFill (grad);
        g.fillEllipse (led);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (led.getX() + h * 0.25f, led.getY() + h * 0.15f, h * 0.35f, h * 0.22f);
        if (on)
        {
            g.setColour (colours::ledOn.withAlpha (0.28f));
            g.fillEllipse (led.expanded (4.0f));
        }
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        auto ink = b.findColour (juce::ToggleButton::textColourId);
        drawEngravedText (g, b.getButtonText(), b.getLocalBounds().withTrimmedLeft ((int) (h + 11.0f)), juce::Justification::centredLeft,
                          isMouseOver ? ink.brighter (0.2f) : ink);
    }

    // ------------------------------------------------------------------ combo: amber readout
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        drawReadoutBox (g, juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f));
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

    // ------------------------------------------------------------------ labels
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (label.isBeingEdited()) return;
        auto* parent = label.getParentComponent();
        g.setFont (label.getFont());
        auto area = label.getBorderSize().subtractedFrom (label.getLocalBounds());
        if (dynamic_cast<juce::Slider*> (parent) != nullptr)
        {
            drawReadoutBox (g, label.getLocalBounds().toFloat().reduced (1.0f));
            g.setColour (colours::readoutText.withAlpha (label.isEnabled() ? 1.0f : 0.4f));
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (label.getText(), area, juce::Justification::centred, false);
            return;
        }
        if (dynamic_cast<juce::ComboBox*> (parent) != nullptr)
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
        drawReadoutBox (g, juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f), 2.0f);
        g.setColour (colours::readoutText);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (text, juce::Rectangle<int> (0, 0, width, height).reduced (8, 4), juce::Justification::centredLeft, 6);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
    {
        juce::AttributedString s;
        s.append (tipText, juce::Font (juce::FontOptions (12.0f)), colours::readoutText);
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, 400.0f);
        const int w = (int) (tl.getWidth() + 18.0f), h = (int) (tl.getHeight() + 10.0f);
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
                   .constrainedWithin (parentArea);
    }

private:
    Textures tx;
};
} // namespace cf::ui
