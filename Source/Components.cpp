#include "Components.h"
#include <cstring>
#include "SliceTribeData.h"

namespace slicetribe
{

//==============================================================================
juce::Font uiFont (float height, int weight)
{
    SharedUiResources f;
    auto tf = f->inter[juce::jlimit (0, 2, weight)];
    if (tf == nullptr)
        return juce::Font (juce::FontOptions (height, weight > 0 ? juce::Font::bold : juce::Font::plain));
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

//==============================================================================
void drawMidiTag (juce::Graphics& g, juce::Component& c, const juce::String& paramId, juce::Rectangle<float> tagArea)
{
    auto* host = c.findParentComponentOfClass<ParamMenuHost>();
    if (host == nullptr)
        return;
    const auto tag = host->midiTagFor (paramId);
    if (tag.isEmpty())
        return;
    const bool learning = tag == "LEARN";
    if (learning)
    {
        g.setColour (colours::cyan());
        g.drawRoundedRectangle (c.getLocalBounds().toFloat().reduced (1.0f), 6.0f, 2.0f);
    }
    if (tagArea.isEmpty())
    {
        if (! learning)
        {
            g.setColour (colours::cyan());
            g.fillEllipse ((float) c.getWidth() - 7.0f, 1.0f, 6.0f, 6.0f);
        }
        return;
    }
    g.setFont (uiFont (9.0f, 2));
    const float w = juce::GlyphArrangement::getStringWidth (uiFont (9.0f, 2), tag) + 8.0f;
    auto r = tagArea.removeFromRight (w).withHeight (12.0f);
    g.setColour (colours::cyan().withAlpha (learning ? 0.9f : (skin().light ? 0.25f : 0.18f)));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (learning ? colours::panel() : (skin().light ? colours::text() : colours::cyan()));
    g.drawText (tag, r, juce::Justification::centred);
}

//==============================================================================
void drawGloss (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    if (! skin().gloss)
        return;
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, radius);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.42f), r.getX(), r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), r.getX(), r.getCentreY() + 1.0f, false));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.52f).reduced (2.0f, 1.5f), radius * 0.8f);
    g.setColour (juce::Colours::black.withAlpha (0.10f));
    g.fillRect (r.withTrimmedTop (r.getHeight() - 2.5f));
}

//==============================================================================
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    const auto& s = skin();
    if (s.sticker)
    {
        g.setColour (s.stickerLine.withAlpha (0.22f));
        g.fillRoundedRectangle (r.translated (0, 4.0f), 12.0f);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (s.light ? 0.06f : 0.28f));
        g.fillRoundedRectangle (r.translated (0, 2.0f), 10.0f);
    }
    g.setColour (colours::panel());
    g.fillRoundedRectangle (r, 10.0f);
    if (s.sticker)
    {
        g.setColour (s.stickerLine.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.75f), 10.0f, 1.5f);
    }
    else
    {
        g.setColour (colours::outline().withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    }

    if (title.isNotEmpty() && s.titleChips)
    {
        const auto f = uiFont (11.0f, 2);
        const auto text = title.toUpperCase();
        const float w = juce::GlyphArrangement::getStringWidth (f, text) + 20.0f;
        auto chip = juce::Rectangle<float> (r.getX() + 12.0f, r.getY() + 7.0f, w, 20.0f);
        g.setGradientFill (colours::accentGradient (chip));
        g.fillRoundedRectangle (chip, 10.0f);
        drawGloss (g, chip, 10.0f);
        g.setColour (colours::onAccent());
        g.setFont (f);
        g.drawText (text, chip, juce::Justification::centred);
    }
    else if (title.isNotEmpty())
    {
        auto t = r.reduced (14.0f, 0).removeFromTop (32.0f);
        g.setColour (colours::accent());
        g.fillRoundedRectangle (t.getX(), t.getCentreY() - 5.0f, 3.0f, 10.0f, 1.5f);
        g.setColour (colours::dim());
        g.setFont (uiFont (11.5f, 1));
        g.drawText (title.toUpperCase(), t.withTrimmedLeft (10.0f), juce::Justification::centredLeft);
    }
}

//==============================================================================
SliceLookAndFeel::SliceLookAndFeel()
{
    applySkin();
}

void SliceLookAndFeel::applySkin()
{
    setColourScheme (juce::LookAndFeel_V4::ColourScheme (colours::bg(), colours::panel(), colours::panel2(), colours::outline(), colours::text(),
                                                         colours::raised(), colours::onAccent(), colours::accent(), colours::text()));
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg());
    setColour (juce::DocumentWindow::backgroundColourId, colours::bg());
    setColour (juce::TextButton::buttonColourId, colours::raised());
    setColour (juce::TextButton::buttonOnColourId, colours::accent());
    setColour (juce::TextButton::textColourOffId, colours::text());
    setColour (juce::TextButton::textColourOnId, colours::onAccent());
    setColour (juce::TooltipWindow::backgroundColourId, colours::raised());
    setColour (juce::TooltipWindow::textColourId, colours::text());
    setColour (juce::TooltipWindow::outlineColourId, colours::outline());
    setColour (juce::Slider::textBoxTextColourId, colours::text());
    setColour (juce::ComboBox::backgroundColourId, colours::panel2());
    setColour (juce::ComboBox::outlineColourId, colours::outline());
    setColour (juce::ComboBox::textColourId, colours::text());
    setColour (juce::ComboBox::arrowColourId, colours::dim());
    setColour (juce::PopupMenu::backgroundColourId, colours::panel2());
    setColour (juce::PopupMenu::textColourId, colours::text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent().withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::onAccent());
    setColour (juce::PopupMenu::headerTextColourId, colours::dim());
    setColour (juce::AlertWindow::backgroundColourId, colours::panel());
    setColour (juce::AlertWindow::textColourId, colours::text());
    setColour (juce::AlertWindow::outlineColourId, colours::outline());
    setColour (juce::ListBox::backgroundColourId, colours::panel2());
    setColour (juce::ScrollBar::thumbColourId, skin().light ? colours::label().withAlpha (0.6f) : colours::faint());
    setColour (juce::TextEditor::backgroundColourId, colours::panel2());
    setColour (juce::TextEditor::textColourId, colours::text());
    setColour (juce::TextEditor::outlineColourId, colours::accent());
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent());
    setColour (juce::TextEditor::highlightColourId, colours::accent().withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, colours::text());
    setColour (juce::Label::textColourId, colours::text());
}

juce::Typeface::Ptr SliceLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    // everything that asks for the default sans (standalone dialogs, menus) gets Inter too
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
    {
        SharedUiResources e;
        auto tf = e->inter[f.isBold() ? 1 : 0];
        if (tf != nullptr)
            return tf;
    }
    return juce::LookAndFeel_V4::getTypefaceForFont (f);
}

void SliceLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float track = juce::jmax (3.0f, radius * (skin().gloss ? 0.15f : 0.11f));
    const float angle = start + pos * (end - start);
    const auto colour = (bool) s.getProperties()["secondary"] ? colours::cyan() : colours::accent();

    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, radius - track, radius - track, 0, start, end, true);
    g.setColour (colours::outline());
    g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path valArc;
        valArc.addCentredArc (c.x, c.y, radius - track, radius - track, 0, start, angle, true);
        g.setGradientFill (juce::ColourGradient (colour, c.x - radius, c.y + radius, colour.brighter (0.35f), c.x + radius, c.y - radius, false));
        g.strokePath (valArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float body = radius - track * 2.6f;
    g.setGradientFill (juce::ColourGradient (colours::raised().brighter (s.isMouseOverOrDragging() ? 0.2f : 0.12f), c.x, c.y - body,
                                             colours::panel2(), c.x, c.y + body, false));
    g.fillEllipse (c.x - body, c.y - body, body * 2, body * 2);
    if (skin().gloss)   // candy knob: a coloured rim and a shine
    {
        g.setColour (colour.withAlpha (0.35f));
        g.drawEllipse (c.x - body, c.y - body, body * 2, body * 2, 2.0f);
        if (skin().sticker)
        {
            g.setColour (skin().stickerLine.withAlpha (0.7f));
            g.drawEllipse (c.x - body - 1.0f, c.y - body - 1.0f, body * 2 + 2.0f, body * 2 + 2.0f, 1.5f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.9f), c.x, c.y - body,
                                                 juce::Colours::white.withAlpha (0.0f), c.x, c.y, false));
        g.fillEllipse (c.x - body * 0.7f, c.y - body * 0.92f, body * 1.4f, body * 0.9f);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (c.x - body, c.y - body, body * 2, body * 2, 1.0f);
    }

    const auto p1 = c.getPointOnCircumference (body * 0.4f, angle);
    const auto p2 = c.getPointOnCircumference (body * 0.85f, angle);
    g.setColour (colours::text());
    g.drawLine ({ p1, p2 }, 2.2f);
}

void SliceLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool enabled = b.isEnabled();
    if (on && enabled)
        g.setGradientFill (colours::accentGradient (r));
    else if (skin().gloss && over && enabled)
        g.setColour (colours::panel2().darker (down ? 0.06f : 0.0f));
    else if (! enabled && skin().light)
        g.setColour (colours::panel2());   // opaque, so a disabled button never melts into the background
    else
        g.setColour (colours::raised().brighter (over && enabled ? 0.12f : 0.0f).darker (down ? 0.2f : 0.0f)
                                    .withMultipliedAlpha (enabled ? 1.0f : 0.5f));
    g.fillRoundedRectangle (r, 7.0f);
    if (on && enabled)
        drawGloss (g, r, 7.0f);
    if (! on || ! enabled)
    {
        g.setColour (colours::outline().brighter (over && enabled ? 0.3f : 0.0f).withMultipliedAlpha (enabled ? 1.0f : 0.6f));
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
    }
}

void SliceLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool onAccent = b.getToggleState() && b.isEnabled();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    if (onAccent && skin().sticker && colours::onAccent().getPerceivedBrightness() > 0.5f)
    {
        g.setColour (skin().stickerLine.withAlpha (0.45f));   // white on candy colours needs a little shadow
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0).translated (0, 1), juce::Justification::centred, 1);
    }
    g.setColour (onAccent ? colours::onAccent()
                          : (b.isEnabled() ? colours::text() : (skin().light ? colours::label().withAlpha (0.6f) : colours::faint())));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

namespace
{
    juce::TextLayout tooltipLayout (const juce::String& text)
    {
        juce::AttributedString s;
        s.setJustification (juce::Justification::topLeft);
        s.append (text, uiFont (13.0f), colours::text());
        s.setLineSpacing (2.0f);
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, 300.0f);
        return tl;
    }
}

void SliceLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (colours::raised());
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::outline().brighter (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    tooltipLayout (text).draw (g, r.reduced (10.0f, 6.0f));
}

juce::Rectangle<int> SliceLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto tl = tooltipLayout (text);
    const int w = (int) std::ceil (tl.getWidth()) + 22;
    const int h = (int) std::ceil (tl.getHeight()) + 14;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
               .constrainedWithin (parent);
}

void SliceLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (colours::panel2());
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f), 6.0f);
}

void SliceLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (colours::accent());
    g.drawRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f), 6.0f, 1.2f);
}

juce::PopupMenu::Options SliceLookAndFeel::getOptionsForComboBoxPopupMenu (juce::ComboBox& box, juce::Label& label)
{
    auto o = juce::LookAndFeel_V4::getOptionsForComboBoxPopupMenu (box, label);
    return box.getNumItems() > 12 ? o.withMinimumNumColumns (2).withMaximumNumColumns (2) : o;
}

void SliceLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    const bool over = box.isMouseOver (true);
    g.setColour (over ? colours::raised().brighter (0.08f) : colours::panel2());
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (over ? colours::outline().brighter (0.3f) : colours::outline());
    g.drawRoundedRectangle (r, 7.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (colours::dim());
    g.fillPath (arrow);
}

void SliceLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (4, 1, box.getWidth() - 24, box.getHeight() - 2);
    l.setFont (getComboBoxFont (box));
    l.setJustificationType (juce::Justification::centred);
}

void SliceLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging)
{
    g.setColour ((over || dragging ? colours::dim() : colours::faint()).withAlpha (0.8f));
    for (float i = 0.35f; i < 1.0f; i += 0.3f)
        g.drawLine ((float) w * i, (float) h, (float) w, (float) h * i, 1.2f);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& cap,
            const juce::String& tip, bool secondary)
    : paramId (id), caption (cap)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("secondary", secondary);
    slider.setTooltip (tip + "\n(Shift-drag = fine, double-click = default, right-click = MIDI learn)");
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
    slider.setDoubleClickReturnValue (true, apvts.getParameter (id)->convertFrom0to1 (apvts.getParameter (id)->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (18);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom (18).toFloat();
    const bool active = slider.isMouseOverOrDragging();
    g.setColour (active ? colours::text() : colours::label());
    g.setFont (uiFont (11.0f, 1));
    g.drawText (active ? slider.getTextFromValue (slider.getValue()) : caption.toUpperCase(), r, juce::Justification::centredTop);
    drawMidiTag (g, *this, paramId, getLocalBounds().toFloat().removeFromTop (14.0f).removeFromRight (40.0f));
}

//==============================================================================
ChoiceSelector::ChoiceSelector (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int cols)
    : param (*apvts.getParameter (id)), columns (juce::jmax (1, cols))
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&param))
        items = c->choices;

    attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float v)
    {
        selected = juce::roundToInt (v);
        repaint();
    });
    attachment->sendInitialUpdate();
}

juce::Rectangle<float> ChoiceSelector::cell (int i) const
{
    const int rows = (items.size() + columns - 1) / columns;
    const float gap = 4.0f;
    const float w = (getWidth() - gap * (columns - 1)) / (float) columns;
    const float h = (getHeight() - gap * (rows - 1)) / (float) rows;
    return { (i % columns) * (w + gap), (i / columns) * (h + gap), w, h };
}

int ChoiceSelector::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < items.size(); ++i)
        if (cell (i).contains (p))
            return i;
    return -1;
}

void ChoiceSelector::paint (juce::Graphics& g)
{
    for (int i = 0; i < items.size(); ++i)
    {
        auto r = cell (i).reduced (0.5f);
        const bool on = i == selected;
        if (on)
        {
            g.setGradientFill (colours::accentGradient (r));
            g.fillRoundedRectangle (r, 6.0f);
            drawGloss (g, r, 6.0f);
        }
        else
        {
            g.setColour (i == hover ? colours::raised().brighter (0.1f) : colours::panel2());
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (i == hover ? colours::outline().brighter (0.3f) : colours::outline());
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }
        g.setFont (uiFont (12.0f, 1));
        if (on && skin().sticker && colours::onAccent().getPerceivedBrightness() > 0.5f)
        {
            g.setColour (skin().stickerLine.withAlpha (0.45f));
            g.drawFittedText (items[i], r.reduced (4, 2).toNearestInt().translated (0, 1), juce::Justification::centred, 2, 0.85f);
        }
        g.setColour (on ? colours::onAccent() : (i == hover ? colours::text() : colours::dim()));
        g.drawFittedText (items[i], r.reduced (4, 2).toNearestInt(), juce::Justification::centred, 2, 0.85f);
    }
    drawMidiTag (g, *this, param.getParameterID(), {});
}

void ChoiceSelector::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (auto* host = findParentComponentOfClass<ParamMenuHost>())
            host->showParamMenu (*this, param.getParameterID());
        return;
    }
    const int i = indexAt (e.position);
    if (i >= 0)
        attachment->setValueAsCompleteGesture ((float) i);
}

void ChoiceSelector::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hover) { hover = i; repaint(); }
}

void ChoiceSelector::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

//==============================================================================
DragNumber::DragNumber()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setWantsKeyboardFocus (false);
}

void DragNumber::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool hot = (dragging || isMouseOver()) && ! locked;
    g.setColour (hot ? colours::raised().brighter (0.1f) : colours::panel2());
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (highlighted ? colour.withAlpha (0.7f) : (hot ? colours::outline().brighter (0.3f) : colours::outline()));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    auto textArea = getLocalBounds().reduced (4, 0);
    if (locked && lockedTag.isNotEmpty())
    {
        auto tag = textArea.removeFromRight (34).toFloat().reduced (0, 7);
        g.setColour (colours::cyan().withAlpha (skin().light ? 0.25f : 0.16f));
        g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (skin().light ? colours::text() : colours::cyan());
        g.setFont (uiFont (9.5f, 2));
        g.drawText (lockedTag, tag, juce::Justification::centred);
    }
    g.setColour (highlighted ? colour : (locked ? colours::dim() : colours::text()));
    g.setFont (uiFont (12.0f, 1));
    g.drawFittedText (format ? format (value) : juce::String (value), textArea, juce::Justification::centred, 1);
}

void DragNumber::mouseDown (const juce::MouseEvent&)
{
    if (locked) return;
    dragStartValue = value;
    dragging = true;
    repaint();
}

void DragNumber::mouseDrag (const juce::MouseEvent& e)
{
    if (locked || ! dragging) return;
    const double sens = e.mods.isShiftDown() ? 0.2 : 1.0;
    const double steps = std::round (-e.getDistanceFromDragStartY() / pixelsPerStep * sens);
    const double v = juce::jlimit (minValue, maxValue, dragStartValue + steps * step);
    if (v != value)
    {
        value = v;
        repaint();
        if (onChange) onChange (value);
    }
}

void DragNumber::mouseUp (const juce::MouseEvent&)
{
    if (locked) return;
    dragging = false;
    repaint();
    if (onDragEnd && value != dragStartValue)   // a plain click never turns "auto" into a manual value
        onDragEnd (value);
}

void DragNumber::mouseDoubleClick (const juce::MouseEvent&)
{
    if (locked) return;
    if (allowTextEntry)
        showEditor();
    else if (onReset)
        onReset();
}

bool DragNumber::keyPressed (const juce::KeyPress& k)
{
    if (locked) return false;
    const double d = k == juce::KeyPress::upKey ? 1.0 : (k == juce::KeyPress::downKey ? -1.0 : 0.0);
    if (d == 0.0) return false;
    value = juce::jlimit (minValue, maxValue, value + d * (k.getModifiers().isShiftDown() ? step : juce::jmax (step, 1.0)));
    repaint();
    if (onChange) onChange (value);
    if (onDragEnd) onDragEnd (value);
    return true;
}

void DragNumber::showEditor()
{
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (uiFont (12.5f, 1));
    editor->setJustification (juce::Justification::centred);
    editor->setText (juce::String (value, std::abs (value - std::round (value)) < 0.01 ? 0 : 1), false);
    editor->selectAll();
    editor->setBounds (getLocalBounds());
    addAndMakeVisible (*editor);
    editor->grabKeyboardFocus();
    const juce::Component::SafePointer<DragNumber> safeThis (this);   // made here: MSVC misreads "this" in a nested lambda
    auto commit = [this, safeThis] (bool apply)
    {
        if (editor == nullptr) return;
        if (apply)
        {
            const double v = editor->getText().retainCharacters ("0123456789.,").replaceCharacter (',', '.').getDoubleValue();
            if (v > 0.0)
            {
                value = juce::jlimit (minValue, maxValue, v);
                if (onChange) onChange (value);
                if (onDragEnd) onDragEnd (value);
            }
        }
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr) { safeThis->editor.reset(); safeThis->repaint(); }
        });
    };
    editor->onReturnKey = [commit] { commit (true); };
    editor->onFocusLost = [commit] { commit (true); };
    editor->onEscapeKey = [commit] { commit (false); };
}

//==============================================================================
SlotComponent::SlotComponent (SliceTribeProcessor& p, int i) : proc (p), index (i)
{
    bpmField.minValue = 40; bpmField.maxValue = 300; bpmField.step = 0.5; bpmField.pixelsPerStep = 3;
    bpmField.format = [] (double v) { return juce::String (v, std::abs (v - std::round (v)) < 0.01 ? 0 : 1) + " BPM"; };
    bpmField.onDragEnd = [this] (double v) { proc.setSlotBpm (index, v); };
    bpmField.onReset = [this] { proc.setSlotBpm (index, 0.0); };
    bpmField.setTooltip ("Tempo of this sample (auto-detected from the name or length).\nDrag up/down to change (Shift = fine), double-click = auto.");
    addAndMakeVisible (bpmField);

    transposeField.minValue = -12; transposeField.maxValue = 12; transposeField.step = 1; transposeField.pixelsPerStep = 8;
    transposeField.format = [] (double v) { return v == 0 ? juce::String ("0 st") : ((v > 0 ? "+" : "") + juce::String ((int) v) + " st"); };
    transposeField.onDragEnd = [this] (double v) { proc.setSlotTranspose (index, (int) v); };
    transposeField.onReset = [this] { proc.setSlotTranspose (index, 0); };
    transposeField.setTooltip ("Transpose in semitones, so basslines in different keys fit together.\nDrag up/down, double-click = 0.");
    addAndMakeVisible (transposeField);

    powerButton.setClickingTogglesState (false);
    powerButton.setTooltip ("Use this sample in the result (on/off)");
    powerButton.onClick = [this] { proc.setSlotEnabled (index, ! info.enabled); };
    addAndMakeVisible (powerButton);

    weightField.minValue = 0; weightField.maxValue = 200; weightField.step = 5; weightField.pixelsPerStep = 2;
    weightField.format = [] (double v) { return juce::String ((int) v) + "%"; };
    weightField.onDragEnd = [this] (double v) { proc.setSlotWeight (index, (float) (v / 100.0)); };
    weightField.onReset = [this] { proc.setSlotWeight (index, 1.0f); };
    weightField.setTooltip (isTrack()
        ? juce::String ("FIT: how hard the new loop stays out of your track's way.\n0% = not at all, 100% = normal, 200% = really out of the way.\nDrag up/down, double-click = 100%.")
        : juce::String ("Share: how often slices are taken from this sample.\n100% = normal, 0% = never, 200% = twice as often.\nDrag up/down, double-click = 100%."));
    addAndMakeVisible (weightField);

    playButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
    playButton.setTooltip ("Listen to this sample on its own, at its own tempo (it keeps looping) - only the part between the two lines.\nClick again to stop.");
    playButton.onClick = [this] { proc.setSlotPreview (index); };
    addAndMakeVisible (playButton);

    clearButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
    clearButton.setTooltip ("Clear slot");
    if (index == kTrackSlot)
        clearButton.setTooltip ("Remove your track: fitting goes off and KEY goes back to what you had");
    clearButton.onClick = [this] { proc.clearSlot (index); };
    addAndMakeVisible (clearButton);

    if (isTrack())
    {
        powerButton.setVisible (false);
        transposeField.setVisible (false);
        playButton.setTooltip ("Listen to the part between the two lines, looping. Click again to stop.");
        bpmField.setTooltip ("Tempo of your track. The fit grid is built on this, so correct it here if it is wrong.\n"
                             "Drag up/down (Shift = fine), double-click = automatic.");
    }
    setTooltip (defaultTip());
}

juce::String SlotComponent::defaultTip() const
{
    if (isTrack())
        return "Drop a part of your own song here (from Finder/Explorer or your DAW) or click to browse.\n"
               "It is never sliced: the new loop leaves room where your track is busy, and KEY follows it.";
    return "Drop a sample here (from Finder/Explorer, Splice or your DAW's browser) or click to browse.\n"
           "Drag the two lines over the waveform to use only a part of it.";
}

void SlotComponent::refresh (const SlotInfo& i)
{
    info = i;
    const bool show = info.loaded;
    bpmField.setVisible (show);
    powerButton.setVisible (show && ! isTrack());
    transposeField.setVisible (show && ! isTrack());
    playButton.setVisible (show);
    weightField.setVisible (show);
    if (dragHandle < 0)
    {
        trimA = juce::jlimit (0.0f, 0.99f, info.trimStart);
        trimB = juce::jlimit (trimA, 1.0f, info.trimEnd);
    }
    weightField.setValue (juce::roundToInt (info.weight * 100.0f));
    weightField.highlighted = isTrack() || std::abs (info.weight - 1.0f) > 0.01f;
    weightField.setTooltip (isTrack()
        ? juce::String ("FIT: how hard the new loop stays out of your track's way.\n0% = not at all, 100% = normal, 200% = really out of the way.\nDrag up/down, double-click = 100%.")
        : juce::String ("Share: how often slices are taken from this sample.\n100% = normal, 0% = never, 200% = twice as often.\nDrag up/down, double-click = 100%."));
    weightField.colour = isTrack() ? colours::cyan() : colours::slot (index);
    clearButton.setVisible (show || info.missing || info.error);
    powerButton.setToggleState (info.enabled, juce::dontSendNotification);
    powerButton.setButtonText (info.enabled ? "ON" : "OFF");
    bpmField.setValue (info.bpmOverride > 0 ? info.bpmOverride : info.detectedBpm);
    bpmField.highlighted = info.bpmOverride > 0;
    bpmField.colour = isTrack() ? colours::cyan() : colours::slot (index);
    transposeField.setValue (info.transpose);
    transposeField.highlighted = info.transpose != 0;
    transposeField.colour = colours::slot (index);

    juce::String tip = defaultTip();
    if (info.loaded)
    {
        const double src = info.bpmOverride > 0 ? info.bpmOverride : info.detectedBpm;
        tip = info.name + "\n" + juce::String (src, 1) + " BPM" + (info.key >= 0 ? "  |  key " + engine::keyName (info.key) : juce::String())
            + (info.keyShift != 0 ? "  |  key match " + juce::String (info.keyShift > 0 ? "+" : "") + juce::String (info.keyShift) + " st" : juce::String())
            + "\nDrop another file here to replace it.";
    }
    else if (info.missing)
        tip = "The file was moved or deleted. Click to locate it, or drop the file here.";
    else if (info.error)
        tip = "This file can't be read. Use WAV, AIFF, FLAC, MP3 or OGG.";
    if (isTrack() && info.loaded)
        tip = info.name + "\nMY TRACK: this one is never sliced. The new loop leaves room where your track is busy,\n"
                          "and KEY follows it. Drag the two lines to pick the part it listens to.";
    setTooltip (tip);
    repaint();
}

void SlotComponent::setPreviewing (bool p)
{
    if (p == previewing)
        return;
    previewing = p;
    playButton.setButtonText (juce::String::fromUTF8 (p ? "\xe2\x96\xa0" : "\xe2\x96\xb6"));
    playButton.setToggleState (p, juce::dontSendNotification);
}

void SlotComponent::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromLeft (isTrack() ? 8 : 26);
    auto bottom = r.removeFromBottom (22);
    playButton.setBounds (bottom.removeFromLeft (30));
    bottom.removeFromLeft (6);
    if (isTrack())
    {
        weightField.setBounds (bottom.removeFromRight (62));
        bottom.removeFromRight (6);
        bpmField.setBounds (bottom.removeFromRight (74));
        clearButton.setBounds (getWidth() - 28, 8, 20, 20);
        return;
    }
    powerButton.setBounds (bottom.removeFromRight (44));
    bottom.removeFromRight (6);
    transposeField.setBounds (bottom.removeFromRight (50));
    bottom.removeFromRight (6);
    bpmField.setBounds (bottom.removeFromRight (74));
    clearButton.setBounds (getWidth() - 30, 8, 20, 20);
    weightField.setBounds (getWidth() - 82, 8, 46, 20);
}

/** The strip the waveform is drawn in: where the two cut lines live. */
juce::Rectangle<float> SlotComponent::waveArea() const
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    r.removeFromLeft (isTrack() ? 8.0f : 26.0f);
    auto content = r.reduced (10.0f, 8.0f);
    content.removeFromTop (18.0f);
    return content.withTrimmedBottom (26.0f).withTrimmedTop (4.0f);
}

int SlotComponent::handleAt (juce::Point<float> p) const
{
    if (! info.loaded)
        return -1;
    auto w = waveArea();
    if (! w.expanded (0.0f, 6.0f).contains (p))
        return -1;
    const float xa = w.getX() + trimA * w.getWidth();
    const float xb = w.getX() + trimB * w.getWidth();
    const float da = std::abs (p.x - xa), db = std::abs (p.x - xb);
    if (juce::jmin (da, db) > 7.0f)
        return -1;
    return da <= db ? 0 : 1;
}

void SlotComponent::applyTrim (float a, float b, bool finished)
{
    trimA = juce::jlimit (0.0f, 0.99f, a);
    trimB = juce::jlimit (juce::jmin (1.0f, trimA + 0.01f), 1.0f, b);
    proc.setSlotTrim (index, trimA, trimB);
    repaint();
    juce::ignoreUnused (finished);
}

void SlotComponent::mouseDown (const juce::MouseEvent& e)
{
    dragHandle = e.mods.isPopupMenu() ? -1 : handleAt (e.position);
}

void SlotComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragHandle < 0)
        return;
    auto w = waveArea();
    const float f = juce::jlimit (0.0f, 1.0f, (e.position.x - w.getX()) / juce::jmax (1.0f, w.getWidth()));
    if (dragHandle == 0) applyTrim (juce::jmin (f, trimB - 0.01f), trimB, false);
    else                 applyTrim (trimA, juce::jmax (f, trimA + 0.01f), false);
}

void SlotComponent::mouseMove (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h != hoverHandle)
    {
        hoverHandle = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SlotComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (info.loaded && waveArea().expanded (0.0f, 6.0f).contains (e.position))
        applyTrim (0.0f, 1.0f, true);   // the whole sample again
}

void SlotComponent::setPreviewPosition (double p)
{
    if (std::abs (p - previewPos) < 1.0e-4)
        return;
    previewPos = p;
    repaint (waveArea().expanded (2.0f, 6.0f).toNearestInt());
}

void SlotComponent::paint (juce::Graphics& g)
{
    const auto col = isTrack() ? colours::cyan() : colours::slot (index);
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool active = info.loaded && info.enabled;
    const bool hover = isMouseOver (true);

    g.setColour (dragOver ? colours::raised() : colours::panel2());
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (dragOver ? col : isTrack() ? col.withAlpha (info.loaded ? 0.9f : 0.55f)
                                            : (hover ? colours::outline().brighter (0.3f) : colours::outline()));
    g.drawRoundedRectangle (r, 9.0f, dragOver ? 2.0f : 1.0f);

    auto stripe = r.removeFromLeft (isTrack() ? 8.0f : 26.0f);
    {
        juce::Path p;
        p.addRoundedRectangle (stripe.getX(), stripe.getY(), stripe.getWidth(), stripe.getHeight(), 9.0f, 9.0f, true, false, true, false);
        g.setColour (col.withAlpha (active || isTrack() ? 0.95f : 0.22f));
        g.fillPath (p);
        if (! isTrack())
        {
            g.setColour (active ? juce::Colours::black.withAlpha (0.75f) : colours::text().withAlpha (0.55f));
            g.setFont (uiFont (14.0f, 2));
            g.drawText (juce::String (index + 1), stripe, juce::Justification::centred);
        }
    }

    auto content = r.reduced (10.0f, 8.0f);
    auto title = content.removeFromTop (18.0f);

    if (! info.loaded)
    {
        const bool problem = info.missing || info.error;
        g.setColour (problem ? colours::error() : (dragOver ? col : isTrack() ? col : (hover ? colours::text() : colours::dim())));
        g.setFont (uiFont (isTrack() ? 12.5f : 13.0f, isTrack() ? 2 : 1));
        juce::String text = info.loading ? "Loading..."
                          : info.error ? "Can't read this file"
                          : info.missing ? "File not found"
                          : dragOver ? "Release to load"
                          : isTrack() ? "DROP YOUR OWN TRACK HERE" : "Drop sample here";
        g.drawText (text, content.withTrimmedTop (-18.0f).withTrimmedBottom (4.0f), juce::Justification::centred);
        if (! info.loading && ! dragOver)
        {
            g.setColour (colours::label());
            g.setFont (uiFont (11.5f));
            const juce::String sub = problem ? info.name + (info.missing ? "  -  click to locate" : "")
                                   : isTrack() ? juce::String ("the new loop then fits around it")
                                               : juce::String ("or click to browse");
            g.drawFittedText (sub, content.withTrimmedTop (18.0f).toNearestInt(), juce::Justification::centred, 1);
        }
        return;
    }

    // title row: name ... [key] [+x%]
    auto tags = title.withTrimmedRight (isTrack() ? 28.0f : 112.0f);   // room for the fields and the clear button
    auto drawTag = [&] (const juce::String& t, juce::Colour c)
    {
        const float w = juce::GlyphArrangement::getStringWidth (uiFont (10.5f, 1), t) + 10.0f;
        auto tr = tags.removeFromRight (w).reduced (0, 2);
        tags.removeFromRight (4.0f);
        g.setColour (c.withAlpha (0.16f));
        g.fillRoundedRectangle (tr, 3.0f);
        g.setColour (c);
        g.setFont (uiFont (10.5f, 1));
        g.drawText (t, tr, juce::Justification::centred);
    };
    const double stretchPct = (info.stretchRatio - 1.0) * 100.0;
    if (std::abs (stretchPct) >= 0.5)
        drawTag ((stretchPct > 0 ? "+" : "") + juce::String (juce::roundToInt (stretchPct)) + "%", colours::dim());
    if (info.key >= 0)
    {
        const int shifted = info.keyShift != 0 ? (((info.key / 2 + info.keyShift) % 12 + 12) % 12) * 2 + (info.key % 2) : info.key;
        drawTag (engine::keyName (info.key) + (info.keyShift != 0 ? juce::String::fromUTF8 (" \xe2\x86\x92 ") + engine::keyName (shifted) : juce::String()),
                 info.keyShift != 0 ? colours::cyan() : colours::dim());
    }

    if (isTrack())
    {
        const auto f = uiFont (10.0f, 2);
        const float w = juce::GlyphArrangement::getStringWidth (f, "MY TRACK") + 12.0f;
        auto tag = tags.removeFromLeft (w).withSizeKeepingCentre (w, 16.0f);
        tags.removeFromLeft (8.0f);
        g.setColour (col.withAlpha (skin().light ? 0.20f : 0.16f));
        g.fillRoundedRectangle (tag, 4.0f);
        g.setColour (skin().light ? col.darker (0.3f) : col);
        g.setFont (f);
        g.drawText ("MY TRACK", tag, juce::Justification::centred);
    }
    g.setColour (active ? colours::text() : colours::faint());
    g.setFont (uiFont (12.5f, 1));
    g.drawFittedText (info.name, tags.toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);

    auto wave = waveArea();
    const float xa = wave.getX() + trimA * wave.getWidth();
    const float xb = wave.getX() + trimB * wave.getWidth();
    const bool cut = trimA > 0.001f || trimB < 0.999f;
    if (! info.peaks.empty() && wave.getHeight() > 4)
    {
        const int n = (int) info.peaks.size() / 2;
        const float mid = wave.getCentreY();
        const float half = wave.getHeight() * 0.5f;
        float maxAbs = 0.001f;
        for (auto v : info.peaks) maxAbs = juce::jmax (maxAbs, std::abs (v));
        const float scale = 0.85f / maxAbs;
        const int cols = (int) wave.getWidth();
        for (int x = 0; x < cols; ++x)
        {
            const float px = wave.getX() + x;
            const bool outside = cut && (px < xa - 0.5f || px > xb + 0.5f);
            g.setColour (col.withAlpha (outside ? 0.13f : (active ? 0.85f : 0.25f)));
            const int p = x * n / juce::jmax (1, cols);
            const float mn = info.peaks[(size_t) p * 2] * scale, mx = info.peaks[(size_t) p * 2 + 1] * scale;
            g.fillRect (px, mid - mx * half, 1.0f, juce::jmax (1.0f, (mx - mn) * half));
        }
    }

    // the playhead of the sample preview
    if (previewing && previewPos >= 0.0 && wave.getHeight() > 4)
    {
        const float px = wave.getX() + (float) previewPos * wave.getWidth();
        g.setColour (colours::text().withAlpha (0.85f));
        g.fillRect (px, wave.getY() - 2.0f, 1.0f, wave.getHeight() + 4.0f);
    }

    // the two cut lines: only the part between them is used
    if (info.loaded && wave.getHeight() > 4)
    {
        for (int h = 0; h < 2; ++h)
        {
            const float x = h == 0 ? xa : xb;
            const bool hot = hoverHandle == h || dragHandle == h;
            g.setColour (colours::cyan().withAlpha (hot ? 1.0f : cut ? 0.9f : hover ? 0.5f : 0.22f));
            g.fillRect (x - (hot ? 1.0f : 0.5f), wave.getY() - 3.0f, hot ? 2.0f : 1.0f, wave.getHeight() + 6.0f);
            g.fillRoundedRectangle (x - 3.0f, wave.getY() - 6.0f, 6.0f, 6.0f, 1.5f);
        }
    }
}

void SlotComponent::paintOverChildren (juce::Graphics& g)
{
    if (info.loaded && info.loading)   // a new file is replacing this one
    {
        g.setColour (colours::panel2().withAlpha (0.88f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f).withTrimmedLeft (isTrack() ? 8.0f : 26.0f), 8.0f);
        g.setColour (colours::cyan());
        g.setFont (uiFont (13.0f, 1));
        g.drawText ("Loading...", getLocalBounds().withTrimmedLeft (isTrack() ? 8 : 26), juce::Justification::centred);
    }
}

void SlotComponent::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDragging = dragHandle >= 0;
    if (wasDragging)
    {
        applyTrim (trimA, trimB, true);
        dragHandle = -1;
    }
    if (! e.mouseWasClicked() || wasDragging)
        return;
    if (e.mods.isPopupMenu() && (info.loaded || info.loading))
    {
        juce::PopupMenu m;
        m.addSectionHeader (info.name.isNotEmpty() ? info.name.toUpperCase()
                                                   : juce::String (isTrack() ? "MY TRACK" : "SAMPLE"));
        m.addItem (1, "Listen to it", true, proc.getSlotPreview() == index);
        m.addItem (2, "Use the whole sample again", trimA > 0.001f || trimB < 0.999f);
        m.addItem (3, isTrack() ? "Remove my track" : "Clear slot");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [safe = juce::Component::SafePointer<SlotComponent> (this)] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1) safe->proc.setSlotPreview (safe->index);
            if (r == 2) safe->applyTrim (0.0f, 1.0f, true);
            if (r == 3) safe->proc.clearSlot (safe->index);
        });
        return;
    }
    if (! info.loaded && ! info.loading)
        openFileChooser();
}

void SlotComponent::openFileChooser()
{
    const auto start = info.missing && info.path.isNotEmpty() ? juce::File (info.path).getParentDirectory()
                                                               : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> (isTrack() ? juce::String ("Choose a part of your own track")
                                                             : "Choose sample(s) for slot " + juce::String (index + 1),
                                                   start, proc.getAudioWildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc)
                          {
                              juce::StringArray paths;
                              for (auto& f : fc.getResults())
                                  paths.add (f.getFullPathName());
                              if (! paths.isEmpty() && onFilesDropped)
                                  onFilesDropped (index, paths);
                          });
}

bool SlotComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (acceptsFile == nullptr || acceptsFile (f))
            return true;
    return false;
}

void SlotComponent::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    if (onFilesDropped)
        onFilesDropped (index, files);
}

//==============================================================================
ResultView::ResultView (SliceTribeProcessor& p) : proc (p) {}

juce::Rectangle<float> ResultView::waveArea() const
{
    return getLocalBounds().toFloat().reduced (14.0f, 0).withTrimmedTop (32.0f).withTrimmedBottom (26.0f);
}

void ResultView::setResult (std::shared_ptr<RenderResult> r)
{
    result = std::move (r);
    hoverSeg = -1;
    rebuildColumns();
    repaint();
}

void ResultView::setPlayhead (double pos)
{
    if (std::abs (pos - playhead) > 1.0e-5)
    {
        playhead = pos;
        repaint (waveArea().toNearestInt().expanded (4));
    }
}

void ResultView::setBusy (bool b)
{
    if (b != busy) { busy = b; repaint(); }
}

void ResultView::showMessage (const juce::String& m, juce::Colour c, int ticks)
{
    message = m;
    messageColour = c;
    messageTicks = ticks;
    repaint();
}

void ResultView::tick()
{
    if (busy)
    {
        spin += 0.25f;
        repaint (getLocalBounds().removeFromTop (32));
    }
    if (messageTicks > 0 && --messageTicks == 0)
        repaint (getLocalBounds().removeFromTop (32));
    if (flashTicks > 0)
    {
        --flashTicks;
        repaint (waveArea().toNearestInt().expanded (2));
    }
}

void ResultView::rebuildColumns()
{
    colMin.clear(); colMax.clear(); colSlot.clear();
    if (result == nullptr || result->audio.getNumSamples() == 0)
        return;

    const auto area = waveArea();
    const int cols = juce::jmax (1, (int) area.getWidth());
    const int len = result->audio.getNumSamples();
    colMin.assign ((size_t) cols, 0.0f);
    colMax.assign ((size_t) cols, 0.0f);
    colSlot.assign ((size_t) cols, -1);

    for (int x = 0; x < cols; ++x)
    {
        const int s0 = (int) ((juce::int64) x * len / cols);
        const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (x + 1) * len / cols));
        float mn = 0, mx = 0;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto range = result->audio.findMinMax (ch, s0, juce::jmin (len, s1) - s0);
            mn = juce::jmin (mn, range.getStart());
            mx = juce::jmax (mx, range.getEnd());
        }
        colMin[(size_t) x] = mn;
        colMax[(size_t) x] = mx;
    }

    for (const auto& seg : result->segments)
    {
        const int x0 = (int) (seg.start * cols / len);
        const int x1 = (int) ((seg.start + seg.length) * cols / len);
        for (int x = x0; x <= x1; ++x)
            colSlot[(size_t) (((x % cols) + cols) % cols)] = seg.slot;
    }
}

int ResultView::segmentAt (float x) const
{
    if (result == nullptr || result->segments.empty())
        return -1;
    const auto area = waveArea();
    const double len = result->audio.getNumSamples();
    const double s = (x - area.getX()) / area.getWidth() * len;
    for (int i = (int) result->segments.size(); --i >= 0;)
    {
        const auto& seg = result->segments[(size_t) i];
        if (s >= seg.start && s < seg.start + seg.length)
            return i;
    }
    return -1;
}

void ResultView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawPanel (g, bounds, "Result");
    const auto area = waveArea();
    auto header = bounds.reduced (14.0f, 0).removeFromTop (32.0f);

    // right: status / info
    {
        auto right = header.removeFromRight (300.0f);
        if (busy)
        {
            auto sp = right.removeFromRight (16.0f).withSizeKeepingCentre (12.0f, 12.0f);
            juce::Path arc;
            arc.addCentredArc (sp.getCentreX(), sp.getCentreY(), 5.5f, 5.5f, spin, 0.0f, juce::MathConstants<float>::pi * 1.4f, true);
            g.setColour (colours::cyan());
            g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            right.removeFromRight (8.0f);
            g.setFont (uiFont (11.5f, 1));
            g.drawText ("PREPARING", right, juce::Justification::centredRight);
        }
        else if (result != nullptr && ! result->segments.empty())
        {
            g.setColour (colours::dim());
            g.setFont (uiFont (11.5f, 1));
            g.drawText (juce::String (result->bars) + " BARS   " + juce::String (result->bpm, 1) + " BPM   "
                            + juce::String ((int) result->segments.size()) + " SLICES", right, juce::Justification::centredRight);
        }
    }

    g.setColour (colours::screen());
    g.fillRoundedRectangle (area.expanded (0, 2), 6.0f);

    // centre: message, hover info or hint
    {
        auto centre = header.withTrimmedLeft (250.0f);   // room for the AUTO PICK and KEEP buttons
        g.setFont (uiFont (12.0f, messageTicks > 0 ? 1 : 0));
        if (messageTicks > 0)
        {
            g.setColour (messageColour);
            g.drawText (message, centre, juce::Justification::centred);
        }
        else if (result != nullptr && hoverSeg >= 0 && hoverSeg < (int) result->segments.size())
        {
            const auto& seg = result->segments[(size_t) hoverSeg];
            juce::String segFlags;
            if (seg.reversed) segFlags << ", reversed";
            if (seg.glitch)   segFlags << ", glitch";
            if (seg.octave)   segFlags << ", +1 octave";
            if (seg.locked)   segFlags << ", locked";
            const auto text = "Slice " + juce::String (seg.hitIndex + 1) + " from sample " + juce::String (seg.slot + 1) + segFlags
                            + juce::String::fromUTF8 ("   \xc2\xb7   click = new slice   \xc2\xb7   right-click = ") + (seg.locked ? "unlock" : "lock");
            const auto f = uiFont (12.0f);
            const float tw = juce::jmin (centre.getWidth() - 16.0f, juce::GlyphArrangement::getStringWidth (f, text));
            auto row = centre.withSizeKeepingCentre (tw + 16.0f, centre.getHeight());
            g.setColour (colours::slot (seg.slot));
            g.fillEllipse (row.getX(), row.getCentreY() - 4.0f, 8.0f, 8.0f);   // which slot, as a colour dot
            g.setColour (colours::text());
            g.drawFittedText (text, row.withTrimmedLeft (14.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
        }
        else if (result != nullptr && ! result->segments.empty())
        {
            g.setColour (colours::label());
            g.drawText (juce::String::fromUTF8 ("Click a slice to replace just that slice   \xc2\xb7   right-click = lock / unlock"), centre, juce::Justification::centred);
        }
    }

    if (result == nullptr || result->segments.empty())
    {
        auto m = area.withSizeKeepingCentre (86.0f, 86.0f).withX (area.getCentreX() - 250.0f);
        drawMascot (g, skin(), m, 0.0f, 0.0f);
        auto t = area.withTrimmedLeft (m.getRight() - area.getX() + 24.0f);
        g.setColour (colours::screenText());
        g.setFont (uiFont (17.0f, 1));
        g.drawText (onlyReference ? "Now the loops!" : "Feed me loops!",
                    t.withTrimmedBottom (40), juce::Justification::centredLeft);
        g.setColour (colours::screenText().withAlpha (0.72f));
        g.setFont (uiFont (12.5f));
        if (onlyReference)
        {
            g.drawText ("Your own track is in the box below - that one is never sliced, it is what the loop fits around.",
                        t.withTrimmedTop (4), juce::Justification::centredLeft);
            g.drawText ("Drop a loop or two in the slots above to build from.",
                        t.withTrimmedTop (44), juce::Justification::centredLeft);
            return;
        }
        g.drawText ("Drop up to 8 loops on the slots above - from Finder/Explorer, Splice or your DAW's browser.",
                    t.withTrimmedTop (4), juce::Justification::centredLeft);
        g.drawText ("Then hit NEW LOOP until you love it.", t.withTrimmedTop (44), juce::Justification::centredLeft);
        g.setColour (colours::cyan());
        g.drawText ("Working on a song? Drop a part of it in the MY TRACK box below - the loop then fits around it.",
                    t.withTrimmedTop (84), juce::Justification::centredLeft);
        return;
    }

    const double len = result->audio.getNumSamples();
    auto xFor = [&] (double sample) { return area.getX() + (float) (sample / len) * area.getWidth(); };

    // slice backgrounds
    for (int i = 0; i < (int) result->segments.size(); ++i)
    {
        const auto& seg = result->segments[(size_t) i];
        const auto col = colours::slot (seg.slot);
        const float x0 = xFor ((double) seg.start);
        const float x1 = juce::jmin (area.getRight(), xFor ((double) (seg.start + seg.length)));
        auto sr = juce::Rectangle<float> (x0, area.getY(), juce::jmax (1.0f, x1 - x0), area.getHeight());
        g.setColour (col.withAlpha (i == hoverSeg ? 0.32f : (seg.locked ? 0.26f : 0.12f)));
        g.fillRect (sr.reduced (0.5f, 0));
        g.setColour (col.withAlpha (0.9f));
        g.fillRect (sr.withHeight (3.0f).reduced (0.5f, 0));
        if (seg.locked)
        {
            g.setColour (colours::screenText().withAlpha (0.9f));
            g.fillRect (sr.withTrimmedTop (sr.getHeight() - 3.0f).reduced (0.5f, 0));
        }
    }

    // grid
    {
        const int beats = result->bars * 4;
        const bool dense = beats > 64;
        for (int b = 0; b <= beats; ++b)
        {
            const bool bar = b % 4 == 0;
            if (dense && ! bar) continue;
            const float x = area.getX() + area.getWidth() * (float) b / (float) beats;
            g.setColour (colours::screenText().withAlpha (bar ? 0.22f : 0.08f));
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            const int barNum = b / 4 + 1;
            const int every = result->bars <= 8 ? 1 : (result->bars <= 16 ? 2 : 4);
            if (bar && b < beats && (barNum - 1) % every == 0)
            {
                g.setColour (colours::label());
                g.setFont (uiFont (11.0f, 1));
                g.drawText (juce::String (barNum), juce::Rectangle<float> (x + 4.0f, area.getBottom() + 5.0f, 30.0f, 14.0f), juce::Justification::centredLeft);
            }
        }
    }

    // waveform
    if (! colMin.empty())
    {
        float maxAbs = 0.001f;
        for (size_t i = 0; i < colMin.size(); ++i)
            maxAbs = juce::jmax (maxAbs, std::abs (colMin[i]), std::abs (colMax[i]));
        const float scale = 0.85f / maxAbs;
        const float mid = area.getCentreY() + 1.5f;
        const float half = (area.getHeight() - 8.0f) * 0.5f;
        for (size_t x = 0; x < colMin.size(); ++x)
        {
            const int s = colSlot[x];
            g.setColour (s >= 0 ? colours::slot (s).withAlpha (0.95f) : colours::screenText().withAlpha (0.3f));
            const float top = mid - colMax[x] * scale * half;
            const float h = juce::jmax (1.0f, (colMax[x] - colMin[x]) * scale * half);
            g.fillRect (area.getX() + (float) x, top, 1.0f, h);
        }
    }

    // markers: lock / reverse / glitch / octave
    for (const auto& seg : result->segments)
    {
        const float x0 = xFor ((double) seg.start);
        const float w  = xFor ((double) (seg.start + seg.length)) - x0;
        if (w < 10.0f) continue;
        float mx = x0 + 3.0f;
        const float my = area.getY() + 7.0f;
        if (seg.locked)
        {
            g.setColour (colours::screenText());
            g.drawRoundedRectangle (mx + 1.5f, my - 1.0f, 5.0f, 5.0f, 2.5f, 1.2f);
            g.fillRoundedRectangle (mx, my + 2.5f, 8.0f, 6.0f, 1.2f);
            mx += 11.0f;
        }
        g.setFont (uiFont (10.0f, 2));
        auto flag = [&] (const juce::String& t, juce::Colour c, float fw)
        {
            if (w <= mx - x0 + fw) return;
            g.setColour (c);
            g.drawText (t, juce::Rectangle<float> (mx, my - 1.0f, fw + 4.0f, 11.0f), juce::Justification::centredLeft);
            mx += fw + 2.0f;
        };
        if (seg.reversed) flag ("R", colours::screenText().withAlpha (0.9f), 8.0f);
        if (seg.glitch)   flag ("G", colours::cyan(), 8.0f);
        if (seg.octave)   flag ("+12", colours::screenText().withAlpha (0.9f), 18.0f);
    }

    // "chop" flash after a new loop: a bright sweep over the screen
    if (flashTicks > 0)
    {
        const float t = 1.0f - (float) flashTicks / (float) flashLength;   // 0 → 1
        const float x = area.getX() + t * area.getWidth();
        g.setGradientFill (juce::ColourGradient (colours::accent().withAlpha (0.0f), x - 90.0f, 0.0f,
                                                 colours::accent().withAlpha (0.45f * (1.0f - t)), x, 0.0f, false));
        g.fillRect (juce::Rectangle<float> (x - 90.0f, area.getY(), 90.0f, area.getHeight()).getIntersection (area));
        g.setColour (colours::screenText().withAlpha (0.9f * (1.0f - t)));
        g.fillRect (x - 1.0f, area.getY(), 2.0f, area.getHeight());
    }

    // playhead
    if (playhead >= 0.0)
    {
        const float x = area.getX() + (float) playhead * area.getWidth();
        g.setColour (colours::screenText().withAlpha (0.12f));
        g.fillRect (x - 3.0f, area.getY(), 6.0f, area.getHeight());
        g.setColour (colours::screenText());
        g.fillRect (x - 0.75f, area.getY() - 2.0f, 1.5f, area.getHeight() + 4.0f);
    }
}

void ResultView::mouseMove (const juce::MouseEvent& e)
{
    const int s = waveArea().contains (e.position) ? segmentAt (e.position.x) : -1;
    if (s != hoverSeg) { hoverSeg = s; repaint(); }
    setMouseCursor (s >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void ResultView::mouseExit (const juce::MouseEvent&)
{
    hoverSeg = -1;
    repaint();
}

void ResultView::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked() || result == nullptr || ! waveArea().contains (e.position))
        return;
    const int s = segmentAt (e.position.x);
    if (s < 0)
        return;
    const int hit = result->segments[(size_t) s].hitIndex;
    if (e.mods.isPopupMenu() || e.mods.isAltDown() || e.mods.isCommandDown())
        proc.toggleLock (hit);
    else
        proc.rerollHit (hit);
}

//==============================================================================
void DragOutTile::paint (juce::Graphics& g)
{
    const bool enabled = isEnabled();
    const bool hot = hover && enabled;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (hot ? colours::raised().brighter (0.1f) : colours::panel2());
    g.fillRoundedRectangle (r, 7.0f);
    const float dash[] = { 4.0f, 3.0f };
    juce::Path p;
    p.addRoundedRectangle (r.reduced (1.5f), 6.0f);
    juce::Path dashed;
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, p, dash, 2);
    g.setColour (hot ? colours::cyan() : (enabled ? colours::outline().brighter (0.45f) : colours::outline()));
    g.fillPath (dashed);

    const auto textCol = ! enabled ? colours::faint() : (hot ? colours::cyan() : colours::text());
    auto content = r.reduced (6.0f, 0);
    const juce::String label (kind == midi ? "MIDI" : "WAV");
    const float tw = juce::GlyphArrangement::getStringWidth (uiFont (12.5f, 1), label);
    const float iconW = 14.0f, gap = 6.0f;
    const float startX = content.getCentreX() - (iconW + gap + tw) * 0.5f;

    auto icon = juce::Rectangle<float> (startX, r.getCentreY() - 7.0f, iconW, 14.0f);
    g.setColour (textCol);
    const float cx = icon.getCentreX();
    g.drawLine (cx, icon.getY(), cx, icon.getBottom() - 4.0f, 1.8f);
    juce::Path head;
    head.addTriangle (cx - 4.5f, icon.getBottom() - 7.0f, cx + 4.5f, icon.getBottom() - 7.0f, cx, icon.getBottom() - 2.0f);
    g.fillPath (head);
    g.drawLine (icon.getX(), icon.getBottom(), icon.getRight(), icon.getBottom(), 1.8f);

    g.setFont (uiFont (12.5f, 1));
    g.drawText (label, juce::Rectangle<float> (startX + iconW + gap, r.getY(), tw + 4.0f, r.getHeight()), juce::Justification::centredLeft);
}

void DragOutTile::mouseDrag (const juce::MouseEvent& e)
{
    if (! isEnabled() || dragStarted || e.getDistanceFromDragStart() < 6)
        return;
    dragStarted = true;
    auto result = proc.getDisplayResult();
    juce::File file;
    // the file is made again when the loop, the FX or the volume changed
    const auto fxNow = proc.readFx();
    const float gainNow = proc.apvts.getRawParameterValue ("gain")->load();
    const bool sameFx = std::memcmp (&fxNow, &lastFx, sizeof (FxChain::Params)) == 0 && gainNow == lastGain;
    if (result != nullptr && result == lastDraggedResult.lock() && lastDraggedFile.existsAsFile() && (kind == midi || sameFx))
        file = lastDraggedFile;
    else
    {
        lastFx = fxNow;
        lastGain = gainNow;
        const auto folder = SliceTribeProcessor::getDefaultExportFolder();
        file = kind == midi ? proc.exportMidi (folder.getChildFile ("MIDI")) : proc.exportLoop (folder);
        lastDraggedResult = result;
        lastDraggedFile = file;
    }
    if (file.existsAsFile() && onDragStarted)
        onDragStarted();
    if (! file.existsAsFile()
        || ! juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                        [safe = juce::Component::SafePointer<DragOutTile> (this)]
                                                                        { if (safe != nullptr) safe->dragStarted = false; }))
        dragStarted = false;   // the drag didn't start: the tile must keep working
}

//==============================================================================
void SceneButton::paint (juce::Graphics& g)
{
    const bool used = proc.isSceneUsed (index);
    const bool active = used && proc.getActiveScene() == index;
    const bool enabled = isEnabled();
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const juce::String letter = juce::String::charToString ((juce::juce_wchar) ('A' + index));

    if (active)
    {
        g.setGradientFill (colours::accentGradient (r));
        g.fillRoundedRectangle (r, 6.0f);
        drawGloss (g, r, 6.0f);
        g.setColour (skin().sticker ? skin().stickerLine.withAlpha (0.8f) : colours::text().withAlpha (0.8f));
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
    }
    else if (used)
    {
        g.setColour (colours::slot (index).withMultipliedBrightness (hover ? 1.1f : 1.0f));
        g.fillRoundedRectangle (r, 6.0f);
        drawGloss (g, r, 6.0f);
        if (skin().sticker)
        {
            g.setColour (skin().stickerLine.withAlpha (0.6f));
            g.drawRoundedRectangle (r, 6.0f, 1.2f);
        }
    }
    else
    {
        g.setColour (hover && enabled ? colours::raised() : colours::panel2());
        g.fillRoundedRectangle (r, 6.0f);
        juce::Path p, dashed;
        p.addRoundedRectangle (r.reduced (1.0f), 5.0f);
        const float dash[] = { 3.0f, 2.5f };
        juce::PathStrokeType (1.1f).createDashedStroke (dashed, p, dash, 2);
        g.setColour (hover && enabled ? colours::cyan() : colours::outline().brighter (0.3f));
        g.fillPath (dashed);
    }

    const auto col = active ? colours::onAccent()
                   : used ? (colours::slot (index).getPerceivedBrightness() > 0.55f ? juce::Colour (0xff1a1020) : juce::Colours::white)
                          : (enabled ? colours::dim() : colours::faint());
    g.setColour (col);
    g.setFont (uiFont (12.5f, 2));
    g.drawText (letter, r, juce::Justification::centred);
}

void SceneButton::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked() || ! isEnabled())
        return;
    const juce::String letter = juce::String::charToString ((juce::juce_wchar) ('A' + index));
    const bool used = proc.isSceneUsed (index);
    auto store = [safe = juce::Component::SafePointer<SceneButton> (this), letter]
    {
        if (safe == nullptr) return;
        if (! safe->proc.hasLoop())
        {
            if (safe->onMessage) safe->onMessage ("Make a loop first, then store it as scene " + letter);
            return;
        }
        safe->proc.storeScene (safe->index);
        if (safe->onMessage) safe->onMessage ("Scene " + letter + " stored - click it to come back to this loop");
    };

    if (e.mods.isPopupMenu() || e.mods.isShiftDown())
    {
        if (e.mods.isShiftDown() && ! e.mods.isPopupMenu())
        {
            store();
            return;
        }
        juce::PopupMenu m;
        m.addSectionHeader ("SCENE " + letter);
        m.addItem (1, used ? "Store the current loop here (replace)" : "Store the current loop here", proc.hasLoop());
        m.addItem (2, "Recall", used);
        m.addItem (3, "Clear", used);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [safe = juce::Component::SafePointer<SceneButton> (this), store] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1) store();
            if (r == 2) safe->proc.recallScene (safe->index);
            if (r == 3) safe->proc.clearScene (safe->index);
            safe->repaint();
        });
        return;
    }
    if (used)
        proc.recallScene (index);
    else
        store();
    repaint();
}

//==============================================================================
juce::Rectangle<float> PanelTabs::tab (int i) const
{
    const auto f = uiFont (11.0f, 2);
    float x = 0.0f;
    for (int k = 0; k < items.size(); ++k)
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, items[k].toUpperCase()) + 20.0f;
        if (k == i)
            return { x, (getHeight() - 20.0f) * 0.5f, w, 20.0f };
        x += w + 4.0f;
    }
    return {};
}

void PanelTabs::paint (juce::Graphics& g)
{
    const auto f = uiFont (11.0f, 2);
    for (int i = 0; i < items.size(); ++i)
    {
        const auto t = tab (i);
        const auto text = items[i].toUpperCase();
        if (i == selected)
        {
            g.setGradientFill (colours::accentGradient (t));
            g.fillRoundedRectangle (t, 10.0f);
            drawGloss (g, t, 10.0f);
            g.setColour (colours::onAccent());
        }
        else
        {
            g.setColour (i == hover ? colours::raised().brighter (0.1f) : colours::panel2());
            g.fillRoundedRectangle (t, 10.0f);
            g.setColour (colours::outline().brighter (i == hover ? 0.3f : 0.0f));
            g.drawRoundedRectangle (t.reduced (0.5f), 10.0f, 1.0f);
            g.setColour (i == hover ? colours::text() : colours::dim());
        }
        g.setFont (f);
        g.drawText (text, t, juce::Justification::centred);
    }
}

void PanelTabs::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < items.size(); ++i)
        if (tab (i).expanded (2.0f).contains (e.position))
            setSelected (i);
}

void PanelTabs::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < items.size(); ++i)
        if (tab (i).expanded (2.0f).contains (e.position))
            h = i;
    if (h != hover) { hover = h; repaint(); }
}

//==============================================================================
void GenerateButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool enabled = isEnabled();
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (down && enabled) r = r.reduced (1.5f);

    if (over && enabled)
    {
        g.setColour (colours::accent().withAlpha (0.2f));
        g.fillRoundedRectangle (r.expanded (1.0f), 12.0f);
    }
    if (enabled)   // candy shadow
    {
        g.setColour (colours::accent2().darker (0.5f).withAlpha (skin().light ? 0.35f : 0.5f));
        g.fillRoundedRectangle (r.translated (0, 3.0f), 11.0f);
    }
    auto grad = colours::accentGradient (r);
    if (! enabled)
        grad.multiplyOpacity (0.35f);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 11.0f);

    // subtle top light (soft, no hard seam)
    if (skin().gloss && enabled)
        drawGloss (g, r, 11.0f);
    else
    {
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (enabled ? 0.10f : 0.03f), r.getX(), r.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), r.getX(), r.getCentreY(), false));
        g.fillRoundedRectangle (r, 11.0f);
    }

    const juce::String label ("NEW LOOP");
    const auto font = skin().displayFont == 0 ? displayFont (skin(), 19.0f) : displayFont (skin(), skin().displayFont == 3 ? 22.0f : 16.0f);
    const float tw = juce::GlyphArrangement::getStringWidth (font, label);
    const float ds = 22.0f, gap = 10.0f;
    const float startX = r.getCentreX() - (ds + gap + tw) * 0.5f;
    auto die = juce::Rectangle<float> (startX, r.getCentreY() - ds * 0.5f, ds, ds);

    const auto fg = colours::onAccent().withAlpha (enabled ? 1.0f : 0.5f);
    g.setColour (fg);
    g.drawRoundedRectangle (die, 5.0f, 1.8f);
    const float d = ds / 4.0f;
    for (auto pt : { juce::Point<float> (1, 1), juce::Point<float> (3, 3), juce::Point<float> (2, 2), juce::Point<float> (3, 1), juce::Point<float> (1, 3) })
        g.fillEllipse (die.getX() + pt.x * d - 1.9f, die.getY() + pt.y * d - 1.9f, 3.8f, 3.8f);

    g.setFont (font);
    if (enabled && skin().sticker && colours::onAccent().getPerceivedBrightness() > 0.5f)
    {
        g.setColour (skin().stickerLine.withAlpha (0.5f));
        g.drawText (label, juce::Rectangle<float> (startX + ds + gap, r.getY() + 2.5f, tw + 4.0f, r.getHeight()), juce::Justification::centredLeft);
        g.setColour (fg);
    }
    g.drawText (label, juce::Rectangle<float> (startX + ds + gap, r.getY() + 1.0f, tw + 4.0f, r.getHeight()), juce::Justification::centredLeft);
    drawMidiTag (g, *this, "trigger", r.removeFromTop (16.0f).removeFromRight (44.0f).translated (-4.0f, 3.0f));
}

void CrazyButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto& s = skin();
    const bool enabled = isEnabled();
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (down && enabled) r = r.reduced (1.0f);
    const float radius = r.getHeight() * 0.5f;
    juce::Path shape;
    shape.addRoundedRectangle (r, radius);

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        g.setGradientFill (juce::ColourGradient (s.second, r.getX(), r.getY(), s.accent, r.getRight(), r.getBottom(), false));
        g.fillRect (r);
        // moving candy-cane stripes
        g.setColour (juce::Colours::white.withAlpha (enabled ? (over ? 0.34f : 0.24f) : 0.1f));
        const float step = 18.0f, off = std::fmod (phase, step);
        for (float x = r.getX() - r.getHeight() - step + off; x < r.getRight() + step; x += step)
        {
            juce::Path p;
            p.addQuadrilateral (x, r.getBottom(), x + 8.0f, r.getBottom(), x + 8.0f + r.getHeight(), r.getY(), x + r.getHeight(), r.getY());
            g.fillPath (p);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), r.getX(), r.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), r.getX(), r.getCentreY(), false));
        g.fillRect (r.withHeight (r.getHeight() * 0.5f));
        if (! enabled)
        {
            g.setColour (colours::panel().withAlpha (0.55f));
            g.fillRect (r);
        }
    }
    const auto ink = s.sticker ? s.stickerLine : juce::Colours::black;
    g.setColour (ink.withAlpha (enabled ? 0.8f : 0.3f));
    g.strokePath (shape, juce::PathStrokeType (1.5f));

    // label: white letters with a dark outline, readable on any stripe
    const float sizes[] = { 19.0f, 13.5f, 16.5f, 22.0f };   // Erica One, Boldonse, Tektur, Big Shoulders
    const auto font = displayFont (s, sizes[juce::jlimit (0, 3, s.displayFont)]);
    const auto label = s.crazyName;
    const float tw = juce::GlyphArrangement::getStringWidth (font, label);
    const float scale = juce::jmin (1.0f, (r.getWidth() - 16.0f) / juce::jmax (1.0f, tw));
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, label, 0.0f, 0.0f);
    juce::Path glyphs;
    ga.createPath (glyphs);
    const auto gb = glyphs.getBounds();
    glyphs.applyTransform (juce::AffineTransform::translation (-gb.getCentreX(), -gb.getCentreY()).scaled (scale)
                               .translated (r.getCentreX(), r.getCentreY()));
    juce::Path outline;
    juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (outline, glyphs);
    g.setColour (ink.withAlpha (enabled ? 0.9f : 0.35f));
    g.fillPath (outline);
    g.setColour (juce::Colours::white.withAlpha (enabled ? 1.0f : 0.5f));
    g.fillPath (glyphs);
}

void GenerateButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (auto* host = findParentComponentOfClass<ParamMenuHost>())
            host->showParamMenu (*this, "trigger");
        return;
    }
    juce::Button::mouseDown (e);
}

} // namespace slicetribe
