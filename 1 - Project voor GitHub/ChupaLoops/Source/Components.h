#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Skins.h"

namespace slicetribe
{

//==============================================================================
namespace colours
{
    inline juce::Colour bg()         { return skin().bg; }
    inline juce::Colour panel()      { return skin().panel; }
    inline juce::Colour panel2()     { return skin().panel2; }
    inline juce::Colour raised()     { return skin().raised; }
    inline juce::Colour outline()    { return skin().outline; }
    inline juce::Colour text()       { return skin().text; }
    inline juce::Colour dim()        { return skin().dim; }      // secondary text
    inline juce::Colour label()      { return skin().label; }    // captions
    inline juce::Colour faint()      { return skin().faint; }    // disabled only
    inline juce::Colour accent()     { return skin().accent; }
    inline juce::Colour accent2()    { return skin().accent2; }
    inline juce::Colour cyan()       { return skin().second; }   // second accent
    inline juce::Colour error()      { return skin().error; }
    inline juce::Colour onAccent()   { return skin().onAccent; }
    inline juce::Colour screen()     { return skin().screen; }
    inline juce::Colour screenText() { return skin().screenText; }

    inline juce::Colour slot (int i) { return juce::Colour (skin().slots[(size_t) juce::jlimit (0, 7, i)]); }

    inline juce::ColourGradient accentGradient (juce::Rectangle<float> r)
    {
        return juce::ColourGradient (accent(), r.getX(), r.getY(), accent2(), r.getRight(), r.getBottom(), false);
    }
}

/** Inter (embedded, identical on Mac and Windows). weight: 0 regular, 1 semibold, 2 bold */
juce::Font uiFont (float height, int weight = 0);

/** Implemented by the main view: right-click menu for parameter controls (MIDI learn). */
struct ParamMenuHost
{
    virtual ~ParamMenuHost() = default;
    virtual void showParamMenu (juce::Component&, const juce::String& paramId) = 0;
    virtual juce::String midiTagFor (const juce::String& paramId) = 0;   // "CC 74", "LEARN" or empty
};

/** Draws the MIDI mapping tag of a control (empty area = just a dot / learn outline). */
void drawMidiTag (juce::Graphics&, juce::Component&, const juce::String& paramId, juce::Rectangle<float> tagArea);

//==============================================================================
class SliceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SliceLookAndFeel();
    void applySkin();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return uiFont (12.5f, 1); }
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (12.5f, 1); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return uiFont (13.0f); }
    void drawCornerResizer (juce::Graphics&, int w, int h, bool over, bool dragging) override;
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    juce::Font getLabelFont (juce::Label&) override { return uiFont (12.5f); }
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox&, juce::Label&) override;
private:
    SharedUiResources resources;   // keeps fonts and tiles alive while an editor is open
};

//==============================================================================
/** Rotary knob with a caption, attached to a parameter. Shift-drag = fine. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption,
          const juce::String& tooltip, bool secondary = false);
    void resized() override;
    void paint (juce::Graphics&) override;

    struct FineSlider : juce::Slider
    {
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (auto* knob = dynamic_cast<Knob*> (getParentComponent()))
                    if (auto* host = findParentComponentOfClass<ParamMenuHost>())
                        host->showParamMenu (*knob, knob->paramId);
                return;
            }
            setMouseDragSensitivity (e.mods.isShiftDown() ? 900 : 180);
            juce::Slider::mouseDown (e);
        }
        void mouseEnter (const juce::MouseEvent& e) override { juce::Slider::mouseEnter (e); if (auto* p = getParentComponent()) p->repaint(); }
        void mouseExit (const juce::MouseEvent& e) override  { juce::Slider::mouseExit (e);  if (auto* p = getParentComponent()) p->repaint(); }
    };

    FineSlider slider;
    const juce::String paramId;
private:
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Segmented selector for a choice parameter (e.g. rhythm, length). */
class ChoiceSelector : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    ChoiceSelector (juce::AudioProcessorValueTreeState&, const juce::String& paramId, int columns);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<float>) const;
    juce::Rectangle<float> cell (int) const;

    juce::RangedAudioParameter& param;
    juce::StringArray items;
    int columns = 1, selected = 0, hover = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
/** Small number field: drag up/down to change (Shift = fine), double-click to reset or type. */
class DragNumber : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    DragNumber();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void setValue (double v) { if (v != value) { value = v; repaint(); } }
    double getValue() const { return value; }

    double minValue = 0, maxValue = 1, pixelsPerStep = 6, step = 1;
    bool highlighted = false, allowTextEntry = false, locked = false;
    juce::String lockedTag;   // e.g. "SYNC" when the host sets the value
    std::function<juce::String (double)> format;
    std::function<void (double)> onChange, onDragEnd;
    std::function<void()> onReset;
    juce::Colour colour = colours::text();

private:
    void showEditor();
    double value = 0, dragStartValue = 0;
    bool dragging = false;
    std::unique_ptr<juce::TextEditor> editor;
};

//==============================================================================
/** One of the 8 sample slots. */
class SlotComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::SettableTooltipClient
{
public:
    SlotComponent (SliceTribeProcessor&, int index);
    void refresh (const SlotInfo&);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override             { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

    std::function<void (int slot, const juce::StringArray&)> onFilesDropped;
    std::function<bool (const juce::String&)> acceptsFile;

private:
    void openFileChooser();

    SliceTribeProcessor& proc;
    int index;
    SlotInfo info;
    bool dragOver = false;

    DragNumber bpmField, transposeField;
    juce::TextButton powerButton, clearButton;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
/** The generated loop: coloured slices, waveform, playhead. */
class ResultView : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    explicit ResultView (SliceTribeProcessor&);
    void setResult (std::shared_ptr<RenderResult>);
    void setPlayhead (double pos);
    void setBusy (bool);
    void showMessage (const juce::String&, juce::Colour, int ticks);
    void startChopFlash() { flashTicks = flashLength; repaint(); }
    void tick();

    void paint (juce::Graphics&) override;
    void resized() override { rebuildColumns(); }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void rebuildColumns();
    int segmentAt (float x) const;
    juce::Rectangle<float> waveArea() const;

    SliceTribeProcessor& proc;
    std::shared_ptr<RenderResult> result;
    std::vector<float> colMin, colMax;
    std::vector<int> colSlot;
    double playhead = -1.0;
    int hoverSeg = -1;
    bool busy = false;
    float spin = 0.0f;
    juce::String message;
    juce::Colour messageColour;
    int messageTicks = 0;
    static constexpr int flashLength = 12;
    int flashTicks = 0;
};

//==============================================================================
/** Drag this tile onto a track in the DAW. */
class DragOutTile : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    enum Kind { audio, midi };
    DragOutTile (SliceTribeProcessor& p, Kind k) : proc (p), kind (k) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    std::function<void()> onDragStarted;
private:
    SliceTribeProcessor& proc;
    Kind kind;
    bool hover = false, dragStarted = false;
    std::weak_ptr<RenderResult> lastDraggedResult;
    juce::File lastDraggedFile;
    FxChain::Params lastFx;
    float lastGain = 0.0f;
};

//==============================================================================
/** Scene A-H: click an empty one to store the current loop, click a stored one to recall it. */
class SceneButton : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    SceneButton (SliceTribeProcessor& p, int i) : proc (p), index (i) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    std::function<void (const juce::String&)> onMessage;
private:
    SliceTribeProcessor& proc;
    int index;
    bool hover = false;
};

//==============================================================================
/** Tabs in a panel's title row (CHARACTER | FX). */
class PanelTabs : public juce::Component
{
public:
    explicit PanelTabs (juce::StringArray names) : items (std::move (names)) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    int  getSelected() const noexcept { return selected; }
    void setSelected (int i) { if (i != selected) { selected = i; repaint(); if (onChange) onChange (i); } }
    std::function<void (int)> onChange;
private:
    juce::Rectangle<float> tab (int) const;
    juce::StringArray items;
    int selected = 0, hover = -1;
};

//==============================================================================
class GenerateButton : public juce::Button
{
public:
    GenerateButton() : juce::Button ("generate") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void mouseDown (const juce::MouseEvent&) override;
};

//==============================================================================
/** The skin's "craziest loop ever" button (Sugar Rush, The Butcher Cut, Acid Flashback...). */
class CrazyButton : public juce::Button
{
public:
    CrazyButton() : juce::Button ("crazy") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void tick() { phase += 0.6f; if (phase > 1000.0f) phase = 0.0f; repaint(); }
private:
    float phase = 0.0f;
};

//==============================================================================
/** Candy shine over a filled shape (only on skins with gloss). */
void drawGloss (juce::Graphics&, juce::Rectangle<float>, float cornerRadius);

/** Titled panel background. */
void drawPanel (juce::Graphics&, juce::Rectangle<float>, const juce::String& title);

} // namespace slicetribe
