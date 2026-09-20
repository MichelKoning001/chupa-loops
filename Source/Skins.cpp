#include "Skins.h"
#include "SliceTribeData.h"

namespace slicetribe
{

using juce::Colour;
using juce::Path;
using juce::Rectangle;
using juce::Point;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    Skin makeSkin (int i)
    {
        Skin s;
        switch (i)
        {
            default:
            case skinLolly:
                s.id = "lolly"; s.name = "Lolly"; s.light = true;
                s.bg = Colour (0xffff3f9a); s.bg2 = Colour (0xffffa62e);
                s.panel = Colour (0xffffffff); s.panel2 = Colour (0xffffeaf4); s.raised = Colour (0xffffffff);
                s.outline = Colour (0xffffb3d4); s.text = Colour (0xff3b0f4f); s.dim = Colour (0xff7a3d8c);
                s.label = Colour (0xff8e4a9a); s.faint = Colour (0xffe8bfd7);
                s.accent = Colour (0xffff1f7a); s.accent2 = Colour (0xffff6a00); s.second = Colour (0xff0088a3);
                s.error = Colour (0xffe0183f); s.onAccent = Colour (0xffffffff);
                s.screen = Colour (0xff3a0b5c); s.screenText = Colour (0xfffff0fb); s.pattern = Colour (0xffffffff).withAlpha (0.14f);
                s.crazyName = "SUGAR RUSH";
                s.slots = { 0xffff2f6d, 0xffff8a00, 0xfff2b600, 0xff1fcf6e, 0xff00b3e6, 0xff4d6bff, 0xffa14dff, 0xffff3fb0 };
                s.displayFont = 0;
                s.bgText = Colour (0xffffffff); s.bgDim = Colour (0xffffffff).withAlpha (0.92f);
                s.gloss = true; s.titleChips = true; s.sticker = true; s.stickerLine = Colour (0xff3b0f4f);
                break;

            case skinFruity:
                s.id = "fruity"; s.name = "Fruity"; s.light = true;
                s.bg = Colour (0xff087a33); s.bg2 = Colour (0xff3fcf62);
                s.panel = Colour (0xffffffff); s.panel2 = Colour (0xffeaf8e6); s.raised = Colour (0xffffffff);
                s.outline = Colour (0xffa9dca3); s.text = Colour (0xff0d3b1e); s.dim = Colour (0xff3b6947);
                s.label = Colour (0xff45704f); s.faint = Colour (0xffb5d6b8);
                s.accent = Colour (0xff2bd94f); s.accent2 = Colour (0xffb6f000); s.second = Colour (0xffe0145a);
                s.error = Colour (0xffe0183f); s.onAccent = Colour (0xff0d3b1e);
                s.mascotA = Colour (0xffff2d6f); s.mascotB = Colour (0xffff8fc0);
                s.crazyName = "FRUIT PUNCH";
                s.screen = Colour (0xff06301a); s.screenText = Colour (0xfff2fff4); s.pattern = Colour (0xffffffff).withAlpha (0.11f);
                s.slots = { 0xffff2d55, 0xffff8a00, 0xfff2c200, 0xff2bb24c, 0xff3d6bff, 0xff8b3dff, 0xffd6004c, 0xffff5fa0 };
                s.displayFont = 0;
                s.bgText = Colour (0xffffffff); s.bgDim = Colour (0xffffffff).withAlpha (0.92f);
                s.gloss = true; s.titleChips = true; s.sticker = true; s.stickerLine = Colour (0xff0d3b1e);
                break;

            case skinSkull:
                s.id = "skull"; s.name = "Skull";
                s.bg = Colour (0xff0b0b0c); s.bg2 = Colour (0xff1a0e0e);
                s.panel = Colour (0xff151517); s.panel2 = Colour (0xff1c1c1f); s.raised = Colour (0xff27272b);
                s.outline = Colour (0xff343438); s.text = Colour (0xfff1ece2); s.dim = Colour (0xffaaa398);
                s.label = Colour (0xff8d877c); s.faint = Colour (0xff57534d);
                s.accent = Colour (0xffe11d2a); s.accent2 = Colour (0xff8a0d16); s.second = Colour (0xffe6d9bf);
                s.error = Colour (0xffff5a5a); s.onAccent = Colour (0xffffffff);
                s.screen = Colour (0xff070707); s.screenText = Colour (0xfff1ece2); s.pattern = Colour (0xffe6d9bf).withAlpha (0.045f);
                s.slots = { 0xffe8323e, 0xfff07c24, 0xffe8c547, 0xff8bbf6a, 0xff5fb3c7, 0xff7d8cff, 0xffa66bd6, 0xffe0668e };
                s.displayFont = 3; s.upperCaseWordmark = true;
                s.crazyName = "SKULL DAMAGE";
                break;

            case skinButcher:
                s.id = "butcher"; s.name = "Butcher";
                s.bg = Colour (0xff222a31); s.bg2 = Colour (0xff12161a);
                s.panel = Colour (0xff2a3138); s.panel2 = Colour (0xff333b43); s.raised = Colour (0xff3e4750);
                s.outline = Colour (0xff4b555f); s.text = Colour (0xfff2f5f7); s.dim = Colour (0xffb3bdc6);
                s.label = Colour (0xff98a3ad); s.faint = Colour (0xff5e6872);
                s.accent = Colour (0xffff2e4d); s.accent2 = Colour (0xffb3001b); s.second = Colour (0xff9fb4c2);
                s.error = Colour (0xffff6b6b); s.onAccent = Colour (0xffffffff);
                s.screen = Colour (0xff0e1114); s.screenText = Colour (0xffe9eef2); s.pattern = Colour (0xffffffff).withAlpha (0.05f);
                s.slots = { 0xffff3b55, 0xffff7a3b, 0xffe8b84a, 0xff72c47a, 0xff52b4ca, 0xff6f8fff, 0xffa06fda, 0xffff6fa3 };
                s.displayFont = 3; s.upperCaseWordmark = true;
                s.crazyName = "THE BUTCHER CUT";
                s.mascotA = Colour (0xffff2e4d); s.mascotB = Colour (0xffb3001b);
                break;

            case skinNeon:
                s.id = "neon"; s.name = "Neon";
                s.bg = Colour (0xff07061a); s.bg2 = Colour (0xff170a33);
                s.panel = Colour (0xff0f0d26); s.panel2 = Colour (0xff16133a); s.raised = Colour (0xff201c4f);
                s.outline = Colour (0xff2e2968); s.text = Colour (0xfff4f0ff); s.dim = Colour (0xffaba5dc);
                s.label = Colour (0xff918bc8); s.faint = Colour (0xff4c4784);
                s.accent = Colour (0xffff2bd6); s.accent2 = Colour (0xff7b2bff); s.second = Colour (0xff00e5ff);
                s.error = Colour (0xffff5577); s.onAccent = Colour (0xffffffff);
                s.screen = Colour (0xff04031a); s.screenText = Colour (0xfff4f0ff); s.pattern = Colour (0xffff2bd6).withAlpha (0.07f);
                s.slots = { 0xffff2bd6, 0xffff8a00, 0xffffe600, 0xff39ff88, 0xff00e5ff, 0xff3d7bff, 0xffb04dff, 0xffff4d88 };
                s.displayFont = 2; s.upperCaseWordmark = true;
                s.crazyName = "NEON OVERDRIVE";
                break;

            case skinAcid:
                s.id = "acid"; s.name = "Acid";
                s.bg = Colour (0xff0a0d06); s.bg2 = Colour (0xff122006);
                s.panel = Colour (0xff11160b); s.panel2 = Colour (0xff181f10); s.raised = Colour (0xff232c16);
                s.outline = Colour (0xff303c1d); s.text = Colour (0xfff2ffe0); s.dim = Colour (0xffabc28c);
                s.label = Colour (0xff91a672); s.faint = Colour (0xff4f5c3b);
                s.accent = Colour (0xffb6ff00); s.accent2 = Colour (0xff00e89a); s.second = Colour (0xffffe600);
                s.error = Colour (0xffff5f5f); s.onAccent = Colour (0xff0a0d06);
                s.screen = Colour (0xff050703); s.screenText = Colour (0xfff2ffe0); s.pattern = Colour (0xffb6ff00).withAlpha (0.055f);
                s.slots = { 0xffb6ff00, 0xffffe600, 0xffff9f1c, 0xffff4d6d, 0xff00ffa3, 0xff22d3ee, 0xff9b5de5, 0xffff66cc };
                s.displayFont = 1; s.upperCaseWordmark = true;
                s.mascotA = Colour (0xffb6ff00); s.mascotB = Colour (0xffffe600);
                s.crazyName = "ACID FLASHBACK";
                break;

            case skinSmile:
                s.id = "smile"; s.name = "Smile";
                s.bg = Colour (0xff0a0a0a); s.bg2 = Colour (0xff15140a);
                s.panel = Colour (0xff141414); s.panel2 = Colour (0xff1b1b1b); s.raised = Colour (0xff272727);
                s.outline = Colour (0xff343434); s.text = Colour (0xfffafafa); s.dim = Colour (0xffb2b2b2);
                s.label = Colour (0xff949494); s.faint = Colour (0xff575757);
                s.accent = Colour (0xffffd400); s.accent2 = Colour (0xffffa600); s.second = Colour (0xffededed);
                s.error = Colour (0xffff5a5a); s.onAccent = Colour (0xff111111);
                s.screen = Colour (0xff050505); s.screenText = Colour (0xfffafafa); s.pattern = Colour (0xffffd400).withAlpha (0.06f);
                s.slots = { 0xffffd400, 0xffffa600, 0xffff7a00, 0xffff4d6d, 0xff3ddc97, 0xff22d3ee, 0xff4c8dff, 0xffc77dff };
                s.displayFont = 0;
                s.crazyName = "SMILEY MAYHEM";
                break;
        }
        if (s.mascotA.isTransparent()) s.mascotA = s.accent;
        if (s.mascotB.isTransparent()) s.mascotB = s.accent2;
        if (s.crazyName.isEmpty())     s.crazyName = "SUGAR RUSH";
        if (s.word1.isEmpty())         s.word1 = i == skinButcher ? "Chopa" : "Chupa";   // the butcher chops
        if (s.word2.isEmpty())         s.word2 = "Loops";
        if (s.bgText.isTransparent()) s.bgText = s.text;
        if (s.bgDim.isTransparent())  s.bgDim = s.label;
        return s;
    }

    const std::array<Skin, numSkins>& allSkins()
    {
        static const std::array<Skin, numSkins> skins = []
        {
            std::array<Skin, numSkins> a;
            for (int i = 0; i < numSkins; ++i)
                a[(size_t) i] = makeSkin (i);
            return a;
        }();
        return skins;
    }

    int& currentIndexRef()
    {
        static int index = [] {
            const auto id = readSetting ("skin", "lolly").toString();
            for (int i = 0; i < numSkins; ++i)
                if (allSkins()[(size_t) i].id == id)
                    return i;
            return (int) skinLolly;
        }();
        return index;
    }

    std::atomic<int> skinVersionCounter { 1 };

    std::unique_ptr<juce::PropertiesFile> openSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Chupa Loops";
        o.folderName = "Chupa Loops";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        o.millisecondsBeforeSaving = -1;   // saved explicitly
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return std::make_unique<juce::PropertiesFile> (o);
    }

    //==========================================================================
    Path spiralArm (Point<float> c, float radius, float startAngle, float turns)
    {
        Path p;
        const int n = 90;
        for (int k = 0; k <= n; ++k)
        {
            const float t = (float) k / (float) n;
            const float a = startAngle + t * turns * 2.0f * pi;
            const float r = radius * t;
            const auto pt = c.getPointOnCircumference (r, a);
            if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    }

    Path skullPath (Rectangle<float> r, float jawDrop)
    {
        // cranium + jaw with holes (even-odd) for eyes and nose
        Path p;
        const float w = r.getWidth(), h = r.getHeight();
        const float cx = r.getCentreX();
        Path head;
        head.addEllipse (cx - w * 0.40f, r.getY(), w * 0.80f, h * 0.66f);
        head.addRoundedRectangle (cx - w * 0.25f, r.getY() + h * 0.46f, w * 0.50f, h * 0.24f, w * 0.08f);
        Path jaw;
        jaw.addRoundedRectangle (cx - w * 0.22f, r.getY() + h * 0.72f + jawDrop, w * 0.44f, h * 0.20f, w * 0.07f);
        p.addPath (head);
        p.addPath (jaw);
        return p;
    }

    void skullFeatures (juce::Graphics& g, Rectangle<float> r, Colour hole, Colour glow, float glowAmount)
    {
        const float w = r.getWidth(), h = r.getHeight(), cx = r.getCentreX();
        g.setColour (hole);
        const float ew = w * 0.22f, eh = h * 0.21f, ey = r.getY() + h * 0.30f;
        g.fillEllipse (cx - w * 0.29f, ey, ew, eh);
        g.fillEllipse (cx + w * 0.07f, ey, ew, eh);
        Path nose;
        nose.addTriangle (cx, r.getY() + h * 0.50f, cx - w * 0.06f, r.getY() + h * 0.62f, cx + w * 0.06f, r.getY() + h * 0.62f);
        g.fillPath (nose);
        if (glowAmount > 0.01f)
        {
            g.setColour (glow.withAlpha (glowAmount));
            g.fillEllipse (cx - w * 0.29f + ew * 0.3f, ey + eh * 0.3f, ew * 0.4f, eh * 0.4f);
            g.fillEllipse (cx + w * 0.07f + ew * 0.3f, ey + eh * 0.3f, ew * 0.4f, eh * 0.4f);
        }
    }

    Path smileyPath (Rectangle<float> r)
    {
        Path p;
        p.setUsingNonZeroWinding (false);
        p.addEllipse (r);
        const float w = r.getWidth(), h = r.getHeight();
        p.addEllipse (r.getX() + w * 0.30f, r.getY() + h * 0.24f, w * 0.11f, h * 0.20f);
        p.addEllipse (r.getX() + w * 0.59f, r.getY() + h * 0.24f, w * 0.11f, h * 0.20f);
        Path mouth;
        mouth.addCentredArc (r.getCentreX(), r.getCentreY() + h * 0.02f, w * 0.30f, h * 0.28f, 0.0f, pi * 0.62f, pi * 1.38f, true);
        juce::PathStrokeType (w * 0.07f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (mouth, mouth);
        p.addPath (mouth);
        return p;
    }

    void glowStroke (juce::Graphics& g, const Path& p, Colour c, float width, float alpha)
    {
        g.setColour (c.withAlpha (0.10f * alpha));
        g.strokePath (p, juce::PathStrokeType (width * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.25f * alpha));
        g.strokePath (p, juce::PathStrokeType (width * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.7f * alpha));
        g.strokePath (p, juce::PathStrokeType (width * 0.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    //==========================================================================
    //==========================================================================
    // Every mascot is a lolly on a stick.
    Colour lineFor (const Skin& s) { return s.sticker ? s.stickerLine : Colour (0xff000000).withAlpha (0.6f); }

    void drawStick (juce::Graphics& g, const Skin& s, Rectangle<float> r, float x, float y0)
    {
        auto stick = Rectangle<float> (x - r.getWidth() * 0.045f, y0, r.getWidth() * 0.09f, juce::jmax (2.0f, r.getBottom() - y0));
        g.setGradientFill (juce::ColourGradient (Colour (0xfffffbf2), stick.getX(), 0.0f, Colour (0xffdccdb4), stick.getRight(), 0.0f, false));
        g.fillRoundedRectangle (stick, stick.getWidth() * 0.5f);
        g.setColour (lineFor (s).withMultipliedAlpha (0.7f));
        g.drawRoundedRectangle (stick, stick.getWidth() * 0.5f, juce::jmax (1.0f, r.getWidth() * 0.022f));
    }

    void candyShine (juce::Graphics& g, Rectangle<float> head, float strength = 0.6f)
    {
        g.setGradientFill (juce::ColourGradient (Colour (0xffffffff).withAlpha (strength), head.getX() + head.getWidth() * 0.3f, head.getY() + head.getHeight() * 0.15f,
                                                 Colour (0xffffffff).withAlpha (0.0f), head.getCentreX(), head.getCentreY(), true));
        g.fillEllipse (head.getX() + head.getWidth() * 0.12f, head.getY() + head.getHeight() * 0.06f, head.getWidth() * 0.5f, head.getHeight() * 0.38f);
    }

    void stickerRing (juce::Graphics& g, const Skin& s, Point<float> c, float radius)
    {
        if (! s.sticker) return;
        const float ring = radius * 1.15f;
        g.setColour (s.stickerLine.withAlpha (0.3f));
        g.fillEllipse (c.x - ring, c.y - ring + radius * 0.1f, ring * 2.0f, ring * 2.0f);
        g.setColour (Colour (0xffffffff));
        g.fillEllipse (c.x - ring, c.y - ring, ring * 2.0f, ring * 2.0f);
    }

    void swirlBall (juce::Graphics& g, const Skin& s, Point<float> c, float ballR, float spin)
    {
        Path ball;
        ball.addEllipse (c.x - ballR, c.y - ballR, ballR * 2.0f, ballR * 2.0f);
        g.setColour (Colour (0xffffffff));
        g.fillPath (ball);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (ball);
            const Point<float> sc (c.x, c.y - ballR * 0.12f);
            const juce::PathStrokeType stroke (ballR * 0.22f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            g.setColour (s.mascotA);
            g.strokePath (spiralArm (sc, ballR * 1.35f, spin, 2.0f), stroke);
            g.setColour (s.mascotB);
            g.strokePath (spiralArm (sc, ballR * 1.35f, spin + pi, 2.0f), stroke);
            candyShine (g, ball.getBounds(), 0.65f);
        }
        g.setColour (lineFor (s).withMultipliedAlpha (0.85f));
        g.strokePath (ball, juce::PathStrokeType (juce::jmax (1.0f, ballR * 0.06f)));
    }

    void cuteFace (juce::Graphics& g, Point<float> c, float ballR, float anim, Colour ink)
    {
        const float ex = ballR * 0.34f, ey = c.y - ballR * 0.02f, er = ballR * 0.17f;
        const bool wink = anim > 0.35f && anim < 0.75f;
        for (int side = -1; side <= 1; side += 2)
        {
            const float x = c.x + (float) side * ex;
            if (wink && side < 0)
            {
                Path w;
                w.addCentredArc (x, ey + er * 0.2f, er, er * 0.6f, 0.0f, -pi * 0.5f, pi * 0.5f, true);
                g.setColour (ink);
                g.strokePath (w, juce::PathStrokeType (er * 0.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                continue;
            }
            g.setColour (Colour (0xffffffff));
            g.fillEllipse (x - er * 1.05f, ey - er * 1.25f, er * 2.1f, er * 2.5f);
            g.setColour (ink);
            g.fillEllipse (x - er * 0.75f, ey - er * 0.85f, er * 1.5f, er * 1.9f);
            g.setColour (Colour (0xffffffff));
            g.fillEllipse (x - er * 0.15f, ey - er * 0.7f, er * 0.55f, er * 0.55f);
        }
        Path mouth;
        mouth.addCentredArc (c.x, c.y + ballR * 0.24f, ballR * 0.22f, ballR * 0.18f, 0.0f, pi * 0.6f, pi * 1.4f, true);
        g.setColour (ink);
        g.strokePath (mouth, juce::PathStrokeType (juce::jmax (1.2f, ballR * 0.08f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Colour (0xffff8fb8).withAlpha (0.85f));
        g.fillEllipse (c.x - ballR * 0.68f, c.y + ballR * 0.14f, ballR * 0.26f, ballR * 0.15f);
        g.fillEllipse (c.x + ballR * 0.42f, c.y + ballR * 0.14f, ballR * 0.26f, ballR * 0.15f);
    }

    void drawLolly (juce::Graphics& g, const Skin& s, Rectangle<float> r, float anim, float time)
    {
        const float bounce = std::sin (anim * pi) * r.getHeight() * 0.07f;
        const float ballR = r.getWidth() * 0.34f;
        const Point<float> c (r.getCentreX(), r.getY() + ballR * 1.15f + r.getHeight() * 0.02f - bounce);
        drawStick (g, s, r, c.x, c.y);
        stickerRing (g, s, c, ballR);
        swirlBall (g, s, c, ballR, time * 0.6f + anim * anim * 2.0f * pi * 1.5f);
        cuteFace (g, c, ballR, anim, Colour (0xff2d1b3d));
    }

    void drawSkull (juce::Graphics& g, const Skin& s, Rectangle<float> r, float anim, float time)
    {
        // a bone-white skull lolly: jaw chomps and eyes glow on a new loop
        const float chomp = anim > 0.0f ? std::abs (std::sin (anim * pi * 3.0f)) * r.getHeight() * 0.06f : 0.0f;
        auto head = Rectangle<float> (r.getX() + r.getWidth() * 0.12f, r.getY() + r.getHeight() * 0.02f + std::sin (time * 1.3f) * 0.5f,
                                      r.getWidth() * 0.76f, r.getHeight() * 0.72f);
        drawStick (g, s, r, r.getCentreX(), head.getY() + head.getHeight() * 0.75f);
        const auto path = skullPath (head, chomp);
        g.setGradientFill (juce::ColourGradient (Colour (0xfffffaf0), head.getCentreX(), head.getY(),
                                                 s.second.darker (0.25f), head.getCentreX(), head.getBottom(), false));
        g.fillPath (path);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (path);
            candyShine (g, head.withHeight (head.getHeight() * 0.7f), 0.7f);
        }
        g.setColour (lineFor (s));
        g.strokePath (path, juce::PathStrokeType (juce::jmax (1.0f, r.getWidth() * 0.03f)));
        skullFeatures (g, head, Colour (0xff1a0a0c), s.accent, 0.6f + 0.4f * anim);
        const float w = head.getWidth(), h = head.getHeight(), cx = head.getCentreX();
        g.setColour (Colour (0xff1a0a0c).withAlpha (0.8f));
        const float ty = head.getY() + h * 0.72f + chomp;
        for (int k = -2; k <= 2; ++k)
            g.drawLine (cx + (float) k * w * 0.075f, ty, cx + (float) k * w * 0.075f, ty + h * 0.09f, juce::jmax (1.0f, w * 0.022f));
    }

    /** The steel cleaver (wooden handle, a drop of red), drawn in `r`, chopping when anim > 0. */
    void steelCleaver (juce::Graphics& g, const Skin& s, Rectangle<float> r, float anim)
    {
        const float w = r.getWidth(), h = r.getHeight();
        const float angle = -0.35f + std::sin (anim * pi) * 0.75f;   // lifts, then chops
        const Point<float> pivot (r.getX() + w * 0.82f, r.getY() + h * 0.40f);
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (angle, pivot.x, pivot.y));

        auto blade = Rectangle<float> (r.getX() + w * 0.02f, r.getY() + h * 0.26f, w * 0.60f, h * 0.44f);
        Path bp;
        bp.addRoundedRectangle (blade.getX(), blade.getY(), blade.getWidth(), blade.getHeight(), w * 0.05f, w * 0.05f, true, false, true, true);
        g.setGradientFill (juce::ColourGradient (Colour (0xffe4ebf0), blade.getX(), blade.getY(),
                                                 Colour (0xff7d8a94), blade.getX(), blade.getBottom(), false));
        g.fillPath (bp);
        g.setColour (Colour (0xffffffff).withAlpha (0.75f));
        g.drawLine (blade.getX() + w * 0.03f, blade.getBottom() - 1.5f, blade.getRight() - w * 0.02f, blade.getBottom() - 1.5f, juce::jmax (1.0f, h * 0.025f));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (bp, juce::PathStrokeType (juce::jmax (1.0f, w * 0.025f)));
        g.setColour (s.bg);
        g.fillEllipse (blade.getRight() - w * 0.14f, blade.getY() + h * 0.07f, w * 0.08f, w * 0.08f);
        g.setColour (s.accent);   // a drop of red on the edge
        g.fillEllipse (blade.getX() + w * 0.12f, blade.getBottom() - h * 0.09f, w * 0.1f, h * 0.08f);
        g.fillEllipse (blade.getX() + w * 0.28f, blade.getBottom() - h * 0.06f, w * 0.06f, h * 0.05f);

        auto handle = Rectangle<float> (blade.getRight() - w * 0.02f, blade.getY() + h * 0.06f, w * 0.36f, h * 0.15f);
        g.setGradientFill (juce::ColourGradient (Colour (0xff9a6238), handle.getX(), handle.getY(),
                                                 Colour (0xff5a3419), handle.getX(), handle.getBottom(), false));
        g.fillRoundedRectangle (handle, h * 0.06f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (handle, h * 0.06f, juce::jmax (1.0f, w * 0.02f));
        g.setColour (Colour (0xffd9dde0));
        for (int k = 1; k <= 2; ++k)
            g.fillEllipse (handle.getX() + handle.getWidth() * 0.33f * (float) k - h * 0.025f, handle.getCentreY() - h * 0.025f, h * 0.05f, h * 0.05f);
    }

    void drawCleaver (juce::Graphics& g, const Skin& s, Rectangle<float> r, float anim, float time)
    {
        // a red-and-white swirl lolly with the steel cleaver chopping into it
        const float ballR = r.getWidth() * 0.32f;
        const Point<float> c (r.getX() + r.getWidth() * 0.34f, r.getY() + ballR * 1.12f);
        drawStick (g, s, r, c.x, c.y);
        swirlBall (g, s, c, ballR, time * 0.4f);
        steelCleaver (g, s, Rectangle<float> (r.getX() + r.getWidth() * 0.24f, r.getY() + r.getHeight() * 0.08f,
                                              r.getWidth() * 0.9f, r.getHeight() * 0.9f), anim);
    }

    void drawNeon (juce::Graphics& g, const Skin& s, Rectangle<float> r, float anim, float time)
    {
        float alpha = 1.0f;
        if (anim > 0.0f)
            alpha = std::sin (time * 70.0f) > -0.2f ? 1.0f : 0.35f;
        else
            alpha = 0.9f + 0.1f * std::sin (time * 3.0f);
        const float ballR = r.getWidth() * 0.34f;
        const Point<float> c (r.getCentreX(), r.getY() + ballR + r.getHeight() * 0.05f);
        const float lw = juce::jmax (1.2f, r.getWidth() * 0.045f);

        Path stick;
        stick.startNewSubPath (c.x, c.y + ballR);
        stick.lineTo (c.x, r.getBottom() - lw);
        glowStroke (g, stick, s.second, lw, alpha);

        Path ring;
        ring.addEllipse (c.x - ballR, c.y - ballR, ballR * 2.0f, ballR * 2.0f);
        glowStroke (g, ring, s.accent, lw, alpha);
        glowStroke (g, spiralArm (c, ballR * 0.78f, time * 0.8f + anim * 6.0f, 1.8f), s.accent, lw * 0.8f, alpha);
    }

    /** The chemistry flask with bubbling acid, drawn in `r`. */
    void acidFlask (juce::Graphics& g, const Skin& s, Rectangle<float> r, float anim, float time)
    {
        const float w = r.getWidth(), h = r.getHeight(), cx = r.getCentreX();
        Path flask;
        const float neckW = w * 0.24f, top = r.getY() + h * 0.06f, shoulder = r.getY() + h * 0.38f;
        flask.startNewSubPath (cx - neckW * 0.5f, top);
        flask.lineTo (cx - neckW * 0.5f, shoulder);
        flask.lineTo (cx - w * 0.42f, r.getBottom() - h * 0.12f);
        flask.quadraticTo (cx - w * 0.44f, r.getBottom() - h * 0.02f, cx - w * 0.32f, r.getBottom() - h * 0.02f);
        flask.lineTo (cx + w * 0.32f, r.getBottom() - h * 0.02f);
        flask.quadraticTo (cx + w * 0.44f, r.getBottom() - h * 0.02f, cx + w * 0.42f, r.getBottom() - h * 0.12f);
        flask.lineTo (cx + neckW * 0.5f, shoulder);
        flask.lineTo (cx + neckW * 0.5f, top);
        flask.closeSubPath();

        // glass
        g.setColour (Colour (0xffffffff).withAlpha (0.85f));
        g.fillPath (flask);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (flask);
            const float level = r.getY() + h * (0.52f - 0.1f * anim);
            Path liquid;
            liquid.startNewSubPath (r.getX(), r.getBottom());
            for (int k = 0; k <= 24; ++k)
            {
                const float x = r.getX() + w * (float) k / 24.0f;
                liquid.lineTo (x, level + std::sin (time * 3.0f + (float) k * 0.7f) * h * 0.025f);
            }
            liquid.lineTo (r.getRight(), r.getBottom());
            liquid.closeSubPath();
            g.setGradientFill (juce::ColourGradient (s.accent, cx, level, s.accent2, cx, r.getBottom(), false));
            g.fillPath (liquid);
            for (int k = 0; k < 5; ++k)   // bubbles
            {
                const float speed = 0.35f + 0.13f * (float) k + anim * 1.2f;
                const float ph = std::fmod (time * speed + (float) k * 0.37f, 1.0f);
                const float bx = cx + std::sin ((float) k * 2.1f + time) * w * 0.18f;
                const float by = r.getBottom() - h * 0.06f - ph * h * 0.75f;
                const float br = w * (0.035f + 0.012f * (float) (k % 3));
                g.setColour (Colour (0xff0a0d06).withAlpha (0.45f * (1.0f - ph)));
                g.drawEllipse (bx - br, by - br, br * 2.0f, br * 2.0f, 1.0f);
            }
            g.setColour (Colour (0xffffffff).withAlpha (0.6f));   // glass reflection
            g.fillRoundedRectangle (cx - w * 0.28f, r.getY() + h * 0.5f, w * 0.06f, h * 0.3f, w * 0.03f);
        }
        g.setColour (Colour (0xff0a0d06));
        g.strokePath (flask, juce::PathStrokeType (juce::jmax (1.2f, w * 0.06f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillRoundedRectangle (cx - neckW * 0.8f, top - h * 0.04f, neckW * 1.6f, h * 0.08f, h * 0.04f);
    }

    void drawAcid (juce::Graphics& g, const Skin& s, Rectangle<float> r, float anim, float time)
    {
        // a lime acid lolly with the bubbling flask on it
        const float ballR = r.getWidth() * 0.36f;
        const Point<float> c (r.getCentreX(), r.getY() + ballR * 1.1f);
        drawStick (g, s, r, c.x, c.y);
        swirlBall (g, s, c, ballR, time * 1.2f + anim * 5.0f);
        acidFlask (g, s, Rectangle<float> (ballR * 1.25f, ballR * 1.45f).withCentre ({ c.x, c.y + ballR * 0.02f }), anim, time);
    }

    void drawSmile (juce::Graphics& g, const Skin& s, Rectangle<float> r, float anim, float time)
    {
        // a yellow smiley lolly: it winks and wobbles on a new loop
        const float ballR = r.getWidth() * 0.35f;
        const Point<float> c (r.getCentreX(), r.getY() + ballR * 1.1f);
        drawStick (g, s, r, c.x, c.y);
        const float wobble = std::sin (anim * pi * 2.0f) * 0.25f + std::sin (time * 1.1f) * 0.03f;
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (wobble, c.x, c.y));
        auto face = Rectangle<float> (c.x - ballR, c.y - ballR, ballR * 2.0f, ballR * 2.0f);
        g.setGradientFill (juce::ColourGradient (s.mascotA.brighter (0.2f), face.getCentreX(), face.getY(),
                                                 s.mascotB, face.getCentreX(), face.getBottom(), false));
        g.fillEllipse (face);
        candyShine (g, face, 0.6f);
        g.setColour (Colour (0xff111111));
        g.drawEllipse (face, juce::jmax (1.0f, face.getWidth() * 0.035f));
        const float w = face.getWidth(), h = face.getHeight();
        const bool wink = anim > 0.3f && anim < 0.8f;
        g.fillEllipse (face.getX() + w * 0.30f, face.getY() + h * 0.24f, w * 0.11f, h * 0.20f);
        if (wink)
            g.drawLine (face.getX() + w * 0.57f, face.getY() + h * 0.35f, face.getX() + w * 0.72f, face.getY() + h * 0.33f, w * 0.05f);
        else
            g.fillEllipse (face.getX() + w * 0.59f, face.getY() + h * 0.24f, w * 0.11f, h * 0.20f);
        Path mouth;
        mouth.addCentredArc (face.getCentreX(), face.getCentreY() + h * 0.02f, w * 0.30f, h * 0.28f, 0.0f, pi * 0.62f, pi * 1.38f, true);
        g.strokePath (mouth, juce::PathStrokeType (w * 0.07f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    /** Blood dripping down from the top edge (Butcher). */
    void drawBloodDrips (juce::Graphics& g, Rectangle<float> area)
    {
        juce::Random rnd (1977);
        Path p;
        p.addRectangle (area.getX(), area.getY(), area.getWidth(), 7.0f);
        std::vector<Rectangle<float>> highlights;
        for (float x = area.getX() + 6.0f; x < area.getRight(); x += 14.0f + (float) rnd.nextInt (34))
        {
            const float w = 5.0f + (float) rnd.nextInt (9);
            const float len = rnd.nextFloat() < 0.2f ? 30.0f + (float) rnd.nextInt (26) : 6.0f + (float) rnd.nextInt (18);
            // a bulge where the drip leaves the band
            p.addTriangle (x - w * 1.1f, area.getY() + 6.0f, x + w * 1.1f, area.getY() + 6.0f, x, area.getY() + 13.0f);
            p.addRoundedRectangle (x - w * 0.5f, area.getY() + 2.0f, w, len, w * 0.5f);
            p.addEllipse (x - w * 0.7f, area.getY() + len - w * 0.6f, w * 1.4f, w * 1.5f);
            highlights.push_back ({ x - w * 0.22f, area.getY() + 9.0f, w * 0.22f, juce::jmax (2.0f, len - 10.0f) });
        }
        g.setColour (Colour (0xff000000).withAlpha (0.35f));
        g.fillPath (p, juce::AffineTransform::translation (0, 2.0f));
        g.setGradientFill (juce::ColourGradient (Colour (0xff7c0010), 0.0f, area.getY(), Colour (0xffd4001f), 0.0f, area.getY() + 50.0f, false));
        g.fillPath (p);
        g.setColour (Colour (0xffffffff).withAlpha (0.28f));
        for (auto& h : highlights)
            g.fillRoundedRectangle (h, h.getWidth() * 0.5f);
    }

    //==========================================================================
    juce::Image makePatternTile (const Skin& s, int index)
    {
        const int T = index == skinButcher ? 36 : (index == skinNeon ? 40 : (index == skinLolly || index == skinFruity ? 80 : 88));
        const int scale = 2;
        juce::Image img (juce::Image::ARGB, T * scale, T * scale, true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale ((float) scale));
        const auto col = s.pattern;
        const float t = (float) T;

        switch (index)
        {
            case skinLolly:
            case skinFruity:
            {
                g.setColour (col);
                for (int k = -2; k <= 2; ++k)   // wide 45 degree candy-cane stripes, seamless every T
                {
                    Path p;
                    const float o = (float) k * t * 0.5f;
                    p.addQuadrilateral (o, t, o + t * 0.2f, t, o + t + t * 0.2f, 0.0f, o + t, 0.0f);
                    g.fillPath (p);
                }
                // sprinkles
                const juce::uint32 candyColours[] = { 0xffffe14d, 0xff4de8ff, 0xff9dff4d, 0xffb46bff, 0xffffffff, 0xffff2f6d };
                const juce::uint32 fruitColours[] = { 0xffff2d55, 0xffffb000, 0xffffe600, 0xffff5fa0, 0xffffffff, 0xffff7a00 };
                const auto* sprinkleColours = index == skinFruity ? fruitColours : candyColours;
                const float sp[][3] = { { 0.18f, 0.30f, 0.6f }, { 0.52f, 0.14f, -0.4f }, { 0.80f, 0.42f, 1.2f },
                                        { 0.36f, 0.70f, -1.0f }, { 0.68f, 0.84f, 0.3f }, { 0.10f, 0.88f, 1.6f } };
                for (int k = 0; k < 6; ++k)
                {
                    Path p;
                    p.addRoundedRectangle (-4.5f, -1.5f, 9.0f, 3.0f, 1.5f);
                    p.applyTransform (juce::AffineTransform::rotation (sp[k][2]).translated (sp[k][0] * t, sp[k][1] * t));
                    g.setColour (Colour (sprinkleColours[k]).withAlpha (0.85f));
                    g.fillPath (p);
                }
                break;
            }
            case skinSkull:
            {
                g.setColour (col);
                for (auto pt : { Point<float> (t * 0.25f, t * 0.25f), Point<float> (t * 0.75f, t * 0.75f) })
                {
                    // silhouette without overlapping parts, so the eyes can be even-odd holes
                    auto rr = Rectangle<float> (20.0f, 22.0f).withCentre (pt);
                    const float w = rr.getWidth(), h = rr.getHeight(), cx = rr.getCentreX();
                    Path p;
                    p.setUsingNonZeroWinding (false);
                    p.addEllipse (cx - w * 0.40f, rr.getY(), w * 0.80f, h * 0.70f);
                    p.addRoundedRectangle (cx - w * 0.22f, rr.getY() + h * 0.74f, w * 0.44f, h * 0.20f, w * 0.07f);
                    p.addEllipse (cx - w * 0.29f, rr.getY() + h * 0.30f, w * 0.22f, h * 0.21f);
                    p.addEllipse (cx + w * 0.07f, rr.getY() + h * 0.30f, w * 0.22f, h * 0.21f);
                    g.fillPath (p);
                }
                break;
            }
            case skinButcher:
                g.setColour (col);
                g.drawRect (0.0f, 0.0f, t, t, 1.0f);
                g.setColour (col.withMultipliedAlpha (0.5f));
                g.drawLine (1.0f, 1.5f, t - 1.0f, 1.5f, 1.0f);
                break;
            case skinNeon:
                g.setColour (col);
                g.drawLine (0.0f, t - 0.5f, t, t - 0.5f, 1.0f);
                g.drawLine (t - 0.5f, 0.0f, t - 0.5f, t, 1.0f);
                g.setColour (s.second.withAlpha (0.10f));
                g.fillEllipse (t * 0.3f, t * 0.4f, 1.6f, 1.6f);
                break;
            case skinAcid:
                g.setColour (col);
                g.drawEllipse (t * 0.15f, t * 0.20f, 14.0f, 14.0f, 1.2f);
                g.drawEllipse (t * 0.62f, t * 0.58f, 22.0f, 22.0f, 1.2f);
                g.drawEllipse (t * 0.70f, t * 0.12f, 7.0f, 7.0f, 1.0f);
                g.fillEllipse (t * 0.30f, t * 0.72f, 5.0f, 5.0f);
                break;
            case skinSmile:
            default:
                g.setColour (col);
                g.fillPath (smileyPath (Rectangle<float> (22.0f, 22.0f).withCentre ({ t * 0.25f, t * 0.25f })));
                g.fillPath (smileyPath (Rectangle<float> (22.0f, 22.0f).withCentre ({ t * 0.75f, t * 0.75f })));
                break;
        }
        return img;
    }
}

//==============================================================================
UiResources::UiResources()
{
    inter[0] = juce::Typeface::createSystemTypefaceFor (SliceTribeData::InterRegular_ttf,  (size_t) SliceTribeData::InterRegular_ttfSize);
    inter[1] = juce::Typeface::createSystemTypefaceFor (SliceTribeData::InterSemiBold_ttf, (size_t) SliceTribeData::InterSemiBold_ttfSize);
    inter[2] = juce::Typeface::createSystemTypefaceFor (SliceTribeData::InterBold_ttf,     (size_t) SliceTribeData::InterBold_ttfSize);
    display[0] = juce::Typeface::createSystemTypefaceFor (SliceTribeData::EricaOneRegular_ttf,  (size_t) SliceTribeData::EricaOneRegular_ttfSize);
    display[1] = juce::Typeface::createSystemTypefaceFor (SliceTribeData::BoldonseRegular_ttf,  (size_t) SliceTribeData::BoldonseRegular_ttfSize);
    display[2] = juce::Typeface::createSystemTypefaceFor (SliceTribeData::TekturMedium_ttf,     (size_t) SliceTribeData::TekturMedium_ttfSize);
    display[3] = juce::Typeface::createSystemTypefaceFor (SliceTribeData::BigShouldersBold_ttf, (size_t) SliceTribeData::BigShouldersBold_ttfSize);
}

const Skin& skinAt (int index)   { return allSkins()[(size_t) juce::jlimit (0, numSkins - 1, index)]; }
const Skin& skin()               { return skinAt (currentIndexRef()); }
int  currentSkinIndex()          { return currentIndexRef(); }
int  skinVersion()               { return skinVersionCounter.load(); }

void setCurrentSkin (int index)
{
    index = juce::jlimit (0, numSkins - 1, index);
    if (index == currentIndexRef())
        return;
    currentIndexRef() = index;
    skinVersionCounter.fetch_add (1);
    writeSetting ("skin", skinAt (index).id);
}

juce::var readSetting (const juce::String& key, const juce::var& fallback)
{
    auto f = openSettings();
    if (! f->containsKey (key))
        return fallback;
    return f->getValue (key);
}

void writeSetting (const juce::String& key, const juce::var& value)
{
    auto f = openSettings();
    f->setValue (key, value);
    f->saveIfNeeded();
}

juce::Font displayFont (const Skin& s, float height)
{
    SharedUiResources res;
    auto tf = res->display[juce::jlimit (0, 3, s.displayFont)];
    if (tf == nullptr)
        return juce::Font (juce::FontOptions (height, juce::Font::bold));
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

void drawMascot (juce::Graphics& g, const Skin& s, Rectangle<float> area, float anim, float time)
{
    auto r = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight()), juce::jmin (area.getWidth(), area.getHeight()));
    anim = juce::jlimit (0.0f, 1.0f, anim);
    // anim runs 1 → 0; turn it into a 0 → 1 → 0 motion that starts fast
    const float a = anim;
    if      (s.id == "skull")   drawSkull   (g, s, r, a, time);
    else if (s.id == "butcher") drawCleaver (g, s, r, a, time);
    else if (s.id == "neon")    drawNeon    (g, s, r, a, time);
    else if (s.id == "acid")    drawAcid    (g, s, r, a, time);
    else if (s.id == "smile")   drawSmile   (g, s, r, a, time);
    else                        drawLolly   (g, s, r, a, time);
}

void drawByline (juce::Graphics& g, const Skin& s, Rectangle<float> area)
{
    SharedUiResources res;
    const auto tf = res->inter[0];
    const auto f = tf != nullptr ? juce::Font (juce::FontOptions (tf).withHeight (11.0f))
                                 : juce::Font (juce::FontOptions (11.0f));
    g.setColour ((s.sticker ? s.stickerLine : juce::Colours::black).withAlpha (0.35f));
    g.setFont (f);
    g.drawText ("By Alexander Koning", area.translated (0.0f, 1.0f), juce::Justification::centredLeft, false);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText ("By Alexander Koning", area, juce::Justification::centredLeft, false);
}

float drawWordmark (juce::Graphics& g, const Skin& s, Point<float> lc, float height)
{
    const auto font = displayFont (s, height);
    const juce::String w1 = s.upperCaseWordmark ? s.word1.toUpperCase() : s.word1;
    const juce::String w2 = s.upperCaseWordmark ? s.word2.toUpperCase() : s.word2;
    const float gap = height * 0.22f;
    const float t1 = juce::GlyphArrangement::getStringWidth (font, w1);
    const float t2 = juce::GlyphArrangement::getStringWidth (font, w2);
    auto r1 = Rectangle<float> (lc.x, lc.y - height * 0.75f, t1 + 4.0f, height * 1.5f);
    auto r2 = Rectangle<float> (lc.x + t1 + gap, lc.y - height * 0.75f, t2 + 4.0f, height * 1.5f);

    if (s.sticker)
    {
        auto wordPath = [&] (const juce::String& w, float x)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (font, w, x, lc.y + (font.getAscent() - font.getDescent()) * 0.5f);
            Path p;
            ga.createPath (p);
            return p;
        };
        const Path p1 = wordPath (w1, lc.x), p2 = wordPath (w2, lc.x + t1 + gap);
        const juce::PathStrokeType outline (height * 0.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        for (auto* p : { &p1, &p2 })
        {
            Path shadow;
            outline.createStrokedPath (shadow, *p);
            shadow.addPath (*p);
            g.setColour (s.stickerLine.withAlpha (0.35f));
            g.fillPath (shadow, juce::AffineTransform::translation (0, height * 0.1f));
            g.setColour (s.stickerLine);
            g.fillPath (shadow);
        }
        g.setColour (juce::Colours::white);
        g.fillPath (p1);
        const auto b2 = p2.getBounds();
        g.setGradientFill (juce::ColourGradient (Colour (0xffffec5c), b2.getX(), b2.getY(), Colour (0xffffb800), b2.getX(), b2.getBottom(), false));
        g.fillPath (p2);
        return t1 + gap + t2;
    }

    g.setFont (font);
    if (s.id == "neon")
    {
        for (int k = 0; k < 3; ++k)
        {
            const float o = 1.0f + (float) k * 1.2f;
            g.setColour (s.accent.withAlpha (0.12f));
            for (auto d : { Point<float> (-o, 0), Point<float> (o, 0), Point<float> (0, -o), Point<float> (0, o) })
            {
                g.drawText (w1, r1 + d, juce::Justification::centredLeft, false);
                g.drawText (w2, r2 + d, juce::Justification::centredLeft, false);
            }
        }
    }
    else if (s.light)
    {
        g.setColour (s.accent.darker (0.6f).withAlpha (0.18f));
        g.drawText (w1, r1.translated (0, 2.0f), juce::Justification::centredLeft, false);
        g.drawText (w2, r2.translated (0, 2.0f), juce::Justification::centredLeft, false);
    }

    g.setColour (s.text);
    g.drawText (w1, r1, juce::Justification::centredLeft, false);
    g.setGradientFill (juce::ColourGradient (s.accent, r2.getX(), r2.getY(), s.accent2, r2.getRight(), r2.getBottom(), false));
    g.drawText (w2, r2, juce::Justification::centredLeft, false);
    return t1 + gap + t2;
}

void drawSkinBackground (juce::Graphics& g, const Skin& s, Rectangle<float> area)
{
    if (s.gloss)
        g.setGradientFill (juce::ColourGradient (s.bg, area.getX(), area.getY(), s.bg2, area.getRight(), area.getBottom(), false));
    else
        g.setGradientFill (juce::ColourGradient (s.bg, area.getX(), area.getY(), s.bg2, area.getX(), area.getBottom(), false));
    g.fillRect (area);

    SharedUiResources res;
    auto& tile = res->tiles[s.id];
    if (! tile.isValid())
    {
        int index = 0;
        for (int i = 0; i < numSkins; ++i)
            if (skinAt (i).id == s.id) index = i;
        tile = makePatternTile (s, index);
    }
    g.setFillType (juce::FillType (tile, juce::AffineTransform::scale (0.5f)));
    g.fillRect (area);

    if (s.id == "neon")   // horizon glow
    {
        g.setGradientFill (juce::ColourGradient (s.accent.withAlpha (0.14f), area.getCentreX(), area.getY(),
                                                 s.accent.withAlpha (0.0f), area.getCentreX(), area.getY() + 110.0f, false));
        g.fillRect (area.withHeight (110.0f));
    }
    else if (s.id == "butcher")
    {
        drawBloodDrips (g, area);
    }
    else if (s.id == "acid")   // slime drips from the top edge
    {
        g.setColour (s.accent.withAlpha (0.16f));
        juce::Random rnd (7);
        for (float x = 20.0f; x < area.getRight(); x += 38.0f + (float) rnd.nextInt (60))
        {
            const float len = 6.0f + (float) rnd.nextInt (22);
            const float w = 5.0f + (float) rnd.nextInt (6);
            g.fillRoundedRectangle (x, area.getY() - 4.0f, w, len + 4.0f, w * 0.5f);
            g.fillEllipse (x - 1.0f, area.getY() + len - w * 0.4f, w + 2.0f, w + 2.0f);
        }
        g.fillRect (area.withHeight (3.0f));
    }
}

juce::Image skinThumbnail (int index, int size)
{
    const auto& s = skinAt (index);
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    auto r = Rectangle<float> (0, 0, (float) size, (float) size);
    Path clip;
    clip.addRoundedRectangle (r.reduced (0.5f), (float) size * 0.2f);
    g.reduceClipRegion (clip);
    drawSkinBackground (g, s, r);
    drawMascot (g, s, r.reduced ((float) size * 0.14f), 0.0f, 0.0f);
    g.setColour (s.outline);
    g.strokePath (clip, juce::PathStrokeType (1.0f));
    return img;
}

} // namespace slicetribe
