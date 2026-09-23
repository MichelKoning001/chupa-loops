#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Components.h"
#include "Overlays.h"

namespace slicetribe
{

/** Preset name display in the header: click = browser. */
class PresetNameBox : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    explicit PresetNameBox (SliceTribeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    std::function<void()> onClick;
private:
    SliceTribeProcessor& proc;
};

/** The full interface, designed at 1120 x 800 and scaled by the editor. */
class MainView : public juce::Component,
                 public juce::FileDragAndDropTarget,
                 public ParamMenuHost,
                 private juce::Timer
{
public:
    static constexpr int designWidth = 1120, designHeight = 800;

    explicit MainView (SliceTribeProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;   // a click on the background cancels MIDI learn

    void showParamMenu (juce::Component&, const juce::String& paramId) override;
    juce::String midiTagFor (const juce::String& paramId) override;

    std::function<void()> onSkinChanged;   // the editor refreshes its look-and-feel

    // for tests / screenshots
    void openPresetBrowser()  { presetBrowser.refresh(); presetBrowser.show(); }
    void openAbout()          { about.show(); }
    void closeOverlays()      { presetBrowser.setVisible (false); about.setVisible (false); tour.setVisible (false); }
    void startTour();
    TourOverlay& getTour()    { return tour; }
    void showFxTab (bool fx)  { charTabs.setSelected (fx ? 1 : 0); }
    void checkSkin()          { timerCallback(); }

private:
    void timerCallback() override;
    void distributeFiles (int startSlot, const juce::StringArray& files);
    bool acceptsFile (const juce::String& path) const;
    void refreshSlots();
    void updateState();
    void exportWithDialog();
    void showExportMenu();
    void exportMidiWithDialog();
    void exportKit();
    void exportStems();
    void updateCharacterTab();
    void flash (const juce::String&, juce::Colour = colours::cyan());
    void showSkinMenu();
    void clearAllSlots();
    juce::Rectangle<float> logoArea() const { return { 10, 6, 58, 58 }; }

    SliceTribeProcessor& proc;
    juce::String wildcard;

    juce::OwnedArray<SlotComponent> slots;
    ResultView resultView;

    ChoiceSelector patternSel, lengthSel, motifSel, modeSel, sizeSel, styleSel, stretchSel, fillSel, midiModeSel;
    Knob chaos, variation, gate, swing, amount, reverse, octave, fade, sensitivity, energy, accent, volume;
    Knob fxCutoff, fxReso, fxEnv, fxDecay, fxLowCut, fxDrive, fxPump, fxWidth;
    PanelTabs charTabs { { "Character", "FX" } };
    juce::OwnedArray<SceneButton> sceneButtons;
    juce::ComboBox keyBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment;

    GenerateButton generateButton;
    CrazyButton crazyButton;
    juce::TextButton neutralButton;
    void refreshNeutralButton();
    juce::TextButton mutateButton, autoPickButton, keepButton, backButton, forwardButton, exportButton, previewButton, unlockButton, spliceButton,
                     presetPrev, presetNext, presetSave, skinButton, aboutButton, clearAllButton;
    PresetNameBox presetName;
    DragOutTile dragOut, dragMidi;
    DragNumber tempoField;
    juce::Label historyLabel;

    PresetBrowser presetBrowser;
    AboutOverlay about;
    TourOverlay tour;

    bool trackLoaded = false;
    int lastSlotsVersion = -1, lastResultVersion = -1, loadedCount = 0, lastSkin = -1,
        lastLearnVersion = -1, lastMidiEvent = -1, lastGenerate = -1, clearConfirmTicks = 0, learnTicks = 0, lastScenes = -1, lastSlotPreview = -2, lastJobVersion = -1;
    bool flashOnNextResult = false;
    float mascotAnim = 0.0f, time = 0.0f;
    juce::String lastPresetText;
    std::unique_ptr<juce::FileChooser> exportChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

//==============================================================================
class SliceTribeEditor : public juce::AudioProcessorEditor
{
public:
    explicit SliceTribeEditor (SliceTribeProcessor&);
    ~SliceTribeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    MainView& getView() { return view; }

private:
    void applySkin();

    SliceTribeProcessor& proc;
    SliceLookAndFeel lnf;
    MainView view;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceTribeEditor)
};

} // namespace slicetribe
