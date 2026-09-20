#pragma once

#include <JuceHeader.h>
#include <map>

namespace slicetribe
{

/** A complete look: colours, display font, mascot and background pattern. */
struct Skin
{
    juce::String id, name;
    bool light = false;
    juce::Colour bg, bg2, panel, panel2, raised, outline, text, dim, label, faint,
                 accent, accent2, second, error, onAccent, screen, screenText, pattern;
    std::array<juce::uint32, 8> slots {};
    int displayFont = 0;        // 0 Erica One, 1 Boldonse, 2 Tektur, 3 Big Shoulders
    bool upperCaseWordmark = false;
    juce::Colour bgText, bgDim;  // text drawn straight on the background (header captions)
    bool gloss = false;          // candy shine on buttons and knobs
    bool titleChips = false;     // panel titles as coloured pills
    bool sticker = false;        // word mark and mascot with a sticker outline
    juce::Colour stickerLine;
    juce::Colour mascotA, mascotB;   // lolly swirl colours (default: accent / accent2)
    juce::String crazyName;          // this skin's "craziest loop ever" button
    juce::String word1, word2;       // word mark ("Chupa Loops"; the Butcher chops: "Chopa Loops")
};

enum SkinIndex { skinLolly, skinFruity, skinSkull, skinButcher, skinNeon, skinAcid, skinSmile, numSkins };

const Skin& skinAt (int index);
const Skin& skin();                  // the current skin
int  currentSkinIndex();
void setCurrentSkin (int index);     // remembered for all instances and projects
int  skinVersion();                  // changes when any instance switches skin

/** Global user settings (skin, MIDI note map...), shared by all instances. */
juce::var  readSetting (const juce::String& key, const juce::var& fallback = {});
void       writeSetting (const juce::String& key, const juce::var& value);

/** Fonts and cached pattern tiles, shared by all open editors and freed when the last one closes
    (never kept in static objects, which would outlive JUCE at plug-in unload). */
struct UiResources
{
    UiResources();
    juce::Typeface::Ptr inter[3];      // regular, semibold, bold
    juce::Typeface::Ptr display[4];    // Erica One, Boldonse, Tektur, Big Shoulders
    std::map<juce::String, juce::Image> tiles;
};
using SharedUiResources = juce::SharedResourcePointer<UiResources>;

/** Display font of a skin (logo, big buttons). */
juce::Font displayFont (const Skin&, float height);

/** Mascot of a skin. anim: 0 = rest, 1 = start of the "new loop" animation. time: seconds (idle motion). */
void drawMascot (juce::Graphics&, const Skin&, juce::Rectangle<float> area, float anim, float time);

/** "Chupa Loops" word mark. Returns the width used. */
float drawWordmark (juce::Graphics&, const Skin&, juce::Point<float> leftCentre, float height);
/** The thin white line under the word mark. */
void drawByline (juce::Graphics&, const Skin&, juce::Rectangle<float> area);

/** Full background (gradient + pattern) of the main view. */
void drawSkinBackground (juce::Graphics&, const Skin&, juce::Rectangle<float> area);

/** Small preview image of a skin for the picker menu. */
juce::Image skinThumbnail (int index, int size);

} // namespace slicetribe
