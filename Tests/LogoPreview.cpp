// Renders the three Kerf stamp variants on a brushed black plate into a PNG.
#include <juce_gui_extra/juce_gui_extra.h>
#include "UI/LookAndFeel.h"
#include "UI/Logo.h"
#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? juce::String (argv[1]) : "logo-preview.png");
    kerf::ui::Textures tx;
    const int w = 900, h = 420;
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Graphics g (img);
    g.drawImageAt (tx.face (w, h), 0, 0);
    const auto& wear = tx.brushed (kerf::ui::colours::metalDark);
    const char* names[] = { "A  -  lame circulaire + KERF, trait de scie dans la ligne de base",
                            "B  -  silhouette de scie egoine, KERF decoupe dans la lame",
                            "C  -  disque dente plein, KERF en reserve dans l'encre" };
    for (int v = 0; v < 3; ++v)
    {
        auto row = juce::Rectangle<float> (30.0f, 20.0f + v * 130.0f, (float) w - 60.0f, 110.0f);
        g.setColour (kerf::ui::colours::engravedHi);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (names[v], row.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft);
        // large
        kerf::ui::logo::drawStamp (g, row.removeFromLeft (330.0f).reduced (0.0f, 8.0f), v, kerf::ui::colours::engraved, &wear);
        row.removeFromLeft (40.0f);
        // as it would sit in the plugin header (small)
        kerf::ui::logo::drawStamp (g, row.removeFromLeft (170.0f).withSizeKeepingCentre (170.0f, 40.0f), v, kerf::ui::colours::engraved, &wear);
        row.removeFromLeft (40.0f);
        // amber ink alternative
        kerf::ui::logo::drawStamp (g, row.removeFromLeft (170.0f).withSizeKeepingCentre (170.0f, 40.0f), v, kerf::ui::colours::readoutText, &wear);
    }
    juce::PNGImageFormat png;
    juce::FileOutputStream os (out);
    if (! os.openedOk() || ! png.writeImageToStream (img, os)) { std::cerr << "could not write png\n"; return 1; }
    std::cout << "wrote " << out.getFullPathName() << std::endl;
    return 0;
}
