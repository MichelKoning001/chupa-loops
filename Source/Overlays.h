#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Components.h"

namespace slicetribe
{

//==============================================================================
/** Full-window overlay base: dims the view, draws a centred card, closes on Esc / outside click. */
class Overlay : public juce::Component
{
public:
    Overlay();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void show();
    void close();

    std::function<void()> onClose;

protected:
    juce::Rectangle<int> card() const;
    virtual juce::Point<int> cardSize() const = 0;
    juce::TextButton closeButton;

private:
    void resized() override;
    virtual void layout (juce::Rectangle<int>) = 0;
};

//==============================================================================
class PresetBrowser : public Overlay,
                      private juce::Timer
{
public:
    explicit PresetBrowser (SliceTribeProcessor&);
    void paint (juce::Graphics&) override;
    void refresh();                    // rescans the user folder and rebuilds the list
    void saveCurrentAs();              // asks for a name

    std::function<void (const juce::String&, juce::Colour)> onMessage;

private:
    juce::Point<int> cardSize() const override { return { 940, 620 }; }
    void layout (juce::Rectangle<int>) override;
    void timerCallback() override;
    void rebuild();
    void selectCategory (int);
    void askDelete (int presetIndex);

    struct Grid : juce::Component
    {
        explicit Grid (PresetBrowser& b) : owner (b) {}
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        int itemAt (juce::Point<int>) const;
        juce::Rectangle<int> itemBounds (int) const;
        void updateSize();
        PresetBrowser& owner;
        int hover = -1;
        static constexpr int columns = 3, rowHeight = 46, gap = 8;
    };

    struct CategoryList : juce::Component
    {
        explicit CategoryList (PresetBrowser& b) : owner (b) {}
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        PresetBrowser& owner;
        int hover = -1;
        static constexpr int rowHeight = 34;
    };

    SliceTribeProcessor& proc;
    juce::StringArray categories;      // "All", factory categories, "User"
    int category = 0;
    std::vector<int> visible;          // preset indices shown in the grid
    juce::TextEditor search;
    CategoryList categoryList { *this };
    Grid grid { *this };
    juce::Viewport viewport;
    juce::TextButton saveButton, folderButton, deleteButton;
    int lastIndex = -2;
    bool lastModified = false;
    std::unique_ptr<juce::AlertWindow> dialog;
};

//==============================================================================
class AboutOverlay : public Overlay,
                     private juce::Thread,
                     private juce::Timer
{
public:
    AboutOverlay();
    ~AboutOverlay() override;
    void paint (juce::Graphics&) override;

private:
    juce::Point<int> cardSize() const override { return { 760, 600 }; }
    void layout (juce::Rectangle<int>) override;
    void run() override;
    void timerCallback() override;
    void visibilityChanged() override;
    void checkForUpdates();

    juce::TextButton updateButton, downloadButton, websiteButton, tourButton;
public:
    std::function<void()> onShowTour;
private:
    juce::String updateStatus, downloadUrl;
    juce::CriticalSection statusLock;
    juce::WebInputStream* activeStream = nullptr;   // guarded by statusLock
    float time = 0.0f;
};

//==============================================================================
/** First-run tour: highlights one area at a time with a short explanation. */
class TourOverlay : public juce::Component
{
public:
    struct Step { juce::Rectangle<float> focus; juce::String title, text; };
    TourOverlay();
    void start (std::vector<Step>);
    void paint (juce::Graphics&) override;
    void resized() override { layoutButtons(); }
    bool keyPressed (const juce::KeyPress&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { if (! hasKeyboardFocus (true)) grabKeyboardFocus(); }
    void visibilityChanged() override;
    int  getStep() const noexcept { return index; }
    void goTo (int);
    std::function<void()> onFinished;
private:
    juce::Rectangle<float> cardBounds() const;
    void layoutButtons();
    void finish();
    std::vector<Step> steps;
    int index = 0;
    juce::TextButton nextButton, skipButton;
};

} // namespace slicetribe
