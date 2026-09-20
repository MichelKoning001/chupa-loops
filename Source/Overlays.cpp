#include "Overlays.h"

#ifndef CHUPALOOPS_UPDATE_REPO
 #define CHUPALOOPS_UPDATE_REPO ""
#endif
#ifndef CHUPALOOPS_BUILD_NUMBER
 #define CHUPALOOPS_BUILD_NUMBER 0
#endif

namespace slicetribe
{

namespace
{
    /** Small modal dialog shown inside the plug-in window (never a separate OS window). */
    void showDialog (juce::Component& parent, std::unique_ptr<juce::AlertWindow>& holder,
                     const juce::String& title, const juce::String& message, const juce::String& okText,
                     bool withName, const juce::String& defaultName,
                     std::function<void (bool ok, juce::String name)> done)
    {
        holder = std::make_unique<juce::AlertWindow> (title, message, juce::MessageBoxIconType::NoIcon);
        if (withName)
        {
            holder->addTextEditor ("name", defaultName);
            if (auto* ed = holder->getTextEditor ("name"))
            {
                ed->setFont (uiFont (14.0f));
                ed->setInputRestrictions (64);
                ed->selectAll();
            }
        }
        holder->addButton (okText, 1, juce::KeyPress (juce::KeyPress::returnKey));
        holder->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        parent.addAndMakeVisible (*holder);
        holder->setCentrePosition (parent.getLocalBounds().getCentre());
        auto* w = holder.get();
        juce::Component::SafePointer<juce::AlertWindow> safeW (w);
        juce::Component::SafePointer<juce::Component> safeParent (&parent);
        holder->enterModalState (true, juce::ModalCallbackFunction::create ([safeW, safeParent, w, done, withName, &holder] (int result)
        {
            if (safeW == nullptr || safeParent == nullptr)
                return;   // the window closed while the dialog was open
            const juce::String name = withName && safeW->getTextEditor ("name") != nullptr ? safeW->getTextEditorContents ("name") : juce::String();
            safeW->setVisible (false);
            juce::MessageManager::callAsync ([safeParent, &holder, w]
            {
                if (safeParent != nullptr && holder.get() == w) holder.reset();
            });
            done (result == 1, name);
        }), false);
        if (withName)
            if (auto* ed = holder->getTextEditor ("name"))
                ed->grabKeyboardFocus();
    }

    juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); }

    int ownBuildNumber()
    {
        return CHUPALOOPS_BUILD_NUMBER;
    }
}

//==============================================================================
Overlay::Overlay()
{
    setWantsKeyboardFocus (true);
    setVisible (false);
    closeButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
    closeButton.setTooltip ("Close (Esc)");
    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (closeButton);
}

juce::Rectangle<int> Overlay::card() const
{
    const auto s = cardSize();
    return getLocalBounds().withSizeKeepingCentre (s.x, s.y);
}

void Overlay::resized()
{
    const auto c = card();
    closeButton.setBounds (c.getRight() - 46, c.getY() + 16, 30, 30);
    layout (c);
}

void Overlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (skin().light ? 0.35f : 0.6f));
    const auto c = card().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (c.translated (0, 6.0f).expanded (2.0f), 16.0f);
    g.setColour (colours::panel());
    g.fillRoundedRectangle (c, 14.0f);
    g.setColour (colours::outline());
    g.drawRoundedRectangle (c.reduced (0.5f), 14.0f, 1.0f);
}

void Overlay::mouseDown (const juce::MouseEvent& e)
{
    if (! card().contains (e.getPosition()))
        close();
}

bool Overlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

void Overlay::show()
{
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

void Overlay::close()
{
    setVisible (false);
    if (onClose) onClose();
}

//==============================================================================
// Preset browser
//==============================================================================
PresetBrowser::PresetBrowser (SliceTribeProcessor& p) : proc (p)
{
    search.setTextToShowWhenEmpty ("Search presets...", colours::label());
    search.setFont (uiFont (13.5f));
    search.setIndents (10, 7);
    search.onTextChange = [this] { rebuild(); };
    search.onEscapeKey = [this] { if (search.isEmpty()) close(); else search.clear(); rebuild(); };
    addAndMakeVisible (search);

    addAndMakeVisible (categoryList);
    viewport.setViewedComponent (&grid, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    saveButton.setButtonText ("SAVE CURRENT AS...");
    saveButton.setTooltip ("Save the current settings as your own preset");
    saveButton.onClick = [this] { saveCurrentAs(); };
    folderButton.setButtonText ("OPEN PRESET FOLDER");
    folderButton.setTooltip ("Your presets are files (.chupapreset): copy them to share with friends or other computers");
    folderButton.onClick = [] { auto f = PresetManager::getUserFolder(); f.createDirectory(); f.startAsProcess(); };
    deleteButton.setButtonText ("DELETE");
    deleteButton.setTooltip ("Delete the selected user preset (factory presets can't be deleted)");
    deleteButton.onClick = [this] { askDelete (proc.presets.getCurrentIndex()); };
    setWantsKeyboardFocus (true);
    for (auto* b : { &saveButton, &folderButton, &deleteButton })
        addAndMakeVisible (b);

    refresh();
    startTimerHz (8);
}

void PresetBrowser::refresh()
{
    proc.presets.rescanUserPresets();
    categories.clear();
    categories.add ("All");
    categories.addArray (proc.presets.getCategories());
    category = juce::jlimit (0, categories.size() - 1, category);
    rebuild();
}

void PresetBrowser::rebuild()
{
    visible.clear();
    const auto q = search.getText().trim();
    const auto cat = categories[category];
    for (int i = 0; i < proc.presets.getNumPresets(); ++i)
    {
        const auto p = proc.presets.getPreset (i);
        if (cat != "All" && p.category != cat)
            continue;
        if (q.isNotEmpty() && ! (p.name.containsIgnoreCase (q) || p.category.containsIgnoreCase (q) || p.genre.containsIgnoreCase (q)))
            continue;
        visible.push_back (i);
    }
    grid.updateSize();
    grid.repaint();
    categoryList.repaint();
    const auto cur = proc.presets.getPreset (proc.presets.getCurrentIndex());
    deleteButton.setEnabled (cur.name.isNotEmpty() && ! cur.factory);
}

void PresetBrowser::selectCategory (int c)
{
    category = juce::jlimit (0, categories.size() - 1, c);
    viewport.setViewPosition (0, 0);
    rebuild();
}

void PresetBrowser::layout (juce::Rectangle<int> c)
{
    search.setBounds (c.getRight() - 46 - 12 - 280, c.getY() + 16, 280, 30);
    categoryList.setBounds (c.getX() + 20, c.getY() + 68, 220, CategoryList::rowHeight * 12);
    viewport.setBounds (c.getX() + 256, c.getY() + 68, c.getWidth() - 276, c.getHeight() - 68 - 70);
    grid.updateSize();
    auto bottom = juce::Rectangle<int> (c.getX() + 256, c.getBottom() - 52, c.getWidth() - 276, 32);
    saveButton.setBounds (bottom.removeFromLeft (170));
    bottom.removeFromLeft (10);
    folderButton.setBounds (bottom.removeFromLeft (170));
    deleteButton.setBounds (bottom.removeFromRight (90));
}

void PresetBrowser::paint (juce::Graphics& g)
{
    Overlay::paint (g);
    const auto c = card().toFloat();
    g.setColour (colours::text());
    g.setFont (displayFont (skin(), skin().displayFont == 3 ? 30.0f : 24.0f));
    g.drawText (skin().upperCaseWordmark ? "PRESETS" : "Presets", juce::Rectangle<float> (c.getX() + 22, c.getY() + 14, 300, 34), juce::Justification::centredLeft);
    g.setColour (colours::label());
    g.setFont (uiFont (12.0f));
    g.drawText (juce::String (proc.presets.getNumPresets()) + " presets  -  click to load, double-click to load and close",
                juce::Rectangle<float> (c.getX() + 22 + 150, c.getY() + 16, 330, 30), juce::Justification::centredLeft);

    // current preset, bottom left
    auto cur = juce::Rectangle<float> (c.getX() + 22, c.getBottom() - 56, 220, 40);
    g.setColour (colours::label());
    g.setFont (uiFont (10.5f, 1));
    g.drawText ("CURRENT", cur.removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (colours::text());
    g.setFont (uiFont (13.5f, 1));
    g.drawFittedText (proc.presets.getCurrentName() + (proc.presets.isModified() ? " *" : ""), cur.toNearestInt(), juce::Justification::centredLeft, 1);
}

void PresetBrowser::timerCallback()
{
    const int idx = proc.presets.getCurrentIndex();
    const bool mod = proc.presets.isModified();
    if (idx != lastIndex || mod != lastModified)
    {
        lastIndex = idx;
        lastModified = mod;
        const auto cur = proc.presets.getPreset (idx);
        deleteButton.setEnabled (cur.name.isNotEmpty() && ! cur.factory);
        grid.repaint();
        repaint (card().removeFromBottom (64));
    }
}

void PresetBrowser::saveCurrentAs()
{
    if (! isVisible())
        show();
    const auto cur = proc.presets.getPreset (proc.presets.getCurrentIndex());
    const juce::String suggestion = cur.name.isNotEmpty() && ! cur.factory ? cur.name : "My " + proc.presets.getCurrentName();
    showDialog (*this, dialog, "Save preset", "Name for your preset:", "SAVE", true, suggestion,
                [safe = juce::Component::SafePointer<PresetBrowser> (this)] (bool ok, juce::String name)
    {
        if (safe == nullptr || ! ok)
            return;
        name = name.trim();
        auto doSave = [safe, name]
        {
            if (safe == nullptr) return;
            juce::String error;
            if (safe->proc.presets.saveUserPreset (name, error))
            {
                safe->selectCategory (safe->categories.indexOf ("User"));
                if (safe->onMessage) safe->onMessage ("Preset saved: " + name, colours::cyan());
            }
            else if (safe->onMessage)
                safe->onMessage (error.upToFirstOccurrenceOf ("\n", false, false), colours::error());
        };
        const auto file = PresetManager::fileForName (name);
        if (name.isNotEmpty() && file.existsAsFile())
        {
            juce::MessageManager::callAsync ([safe, name, doSave]
            {
                if (safe == nullptr) return;
                showDialog (*safe, safe->dialog, "Replace preset?", "A preset called \"" + name + "\" already exists.", "REPLACE", false, {},
                            [doSave] (bool yes, juce::String) { if (yes) doSave(); });
            });
        }
        else
            doSave();
    });
}

void PresetBrowser::askDelete (int index)
{
    const auto p = proc.presets.getPreset (index);
    if (p.name.isEmpty() || p.factory)
        return;
    showDialog (*this, dialog, "Delete preset?", "\"" + p.name + "\" will be moved to the trash.", "DELETE", false, {},
                [safe = juce::Component::SafePointer<PresetBrowser> (this), file = p.file] (bool ok, juce::String)
    {
        if (safe == nullptr || ! ok) return;
        safe->proc.presets.deleteUserPreset (file);   // by file: the list may have changed meanwhile
        safe->rebuild();
    });
}

//==============================================================================
void PresetBrowser::CategoryList::paint (juce::Graphics& g)
{
    for (int i = 0; i < owner.categories.size(); ++i)
    {
        auto r = juce::Rectangle<float> (0, (float) (i * rowHeight), (float) getWidth(), (float) rowHeight - 4.0f);
        const bool on = i == owner.category;
        if (on)
        {
            g.setGradientFill (colours::accentGradient (r));
            g.fillRoundedRectangle (r, 7.0f);
            drawGloss (g, r, 7.0f);
        }
        else if (i == hover)
        {
            g.setColour (colours::raised());
            g.fillRoundedRectangle (r, 7.0f);
        }
        const auto name = owner.categories[i];
        juce::String genre;
        for (auto& p : factoryPresets())
            if (p.category == name) { genre = p.genre; break; }
        g.setColour (on ? colours::onAccent() : colours::text());
        g.setFont (uiFont (13.0f, 1));
        g.drawText (name, r.reduced (12, 0).withTrimmedBottom (genre.isNotEmpty() ? 11.0f : 0.0f), juce::Justification::centredLeft);
        if (genre.isNotEmpty())
        {
            g.setColour (on ? colours::onAccent().withAlpha (0.8f) : colours::label());
            g.setFont (uiFont (10.0f));
            g.drawText (genre, r.reduced (12, 0).withTrimmedTop (16.0f), juce::Justification::centredLeft);
        }
    }
}

void PresetBrowser::CategoryList::mouseMove (const juce::MouseEvent& e)
{
    const int i = e.y / rowHeight;
    const int h = juce::isPositiveAndBelow (i, owner.categories.size()) ? i : -1;
    if (h != hover) { hover = h; repaint(); }
}

void PresetBrowser::CategoryList::mouseUp (const juce::MouseEvent& e)
{
    const int i = e.y / rowHeight;
    if (e.mouseWasClicked() && juce::isPositiveAndBelow (i, owner.categories.size()))
        owner.selectCategory (i);
}

//==============================================================================
void PresetBrowser::Grid::updateSize()
{
    const int w = juce::jmax (100, owner.viewport.getWidth() - owner.viewport.getScrollBarThickness() - 4);
    const int rows = ((int) owner.visible.size() + columns - 1) / columns;
    setSize (w, juce::jmax (owner.viewport.getHeight(), rows * (rowHeight + gap)));
}

juce::Rectangle<int> PresetBrowser::Grid::itemBounds (int i) const
{
    const int w = (getWidth() - gap * (columns - 1)) / columns;
    return { (i % columns) * (w + gap), (i / columns) * (rowHeight + gap), w, rowHeight };
}

int PresetBrowser::Grid::itemAt (juce::Point<int> p) const
{
    for (int i = 0; i < (int) owner.visible.size(); ++i)
        if (itemBounds (i).contains (p))
            return i;
    return -1;
}

void PresetBrowser::Grid::paint (juce::Graphics& g)
{
    if (owner.visible.empty())
    {
        g.setColour (colours::dim());
        g.setFont (uiFont (14.0f));
        const bool user = owner.categories[owner.category] == "User";
        g.drawFittedText (user ? "No presets of your own yet.\nSet things up the way you like and click SAVE CURRENT AS..."
                               : "No presets match your search.",
                          getLocalBounds().withHeight (120), juce::Justification::centred, 3);
        return;
    }
    const int current = owner.proc.presets.getCurrentIndex();
    for (int i = 0; i < (int) owner.visible.size(); ++i)
    {
        const auto r = itemBounds (i).toFloat();
        if (! g.getClipBounds().toFloat().intersects (r))
            continue;
        const int idx = owner.visible[(size_t) i];
        const auto p = owner.proc.presets.getPreset (idx);
        const bool on = idx == current;
        if (on)
            g.setGradientFill (colours::accentGradient (r));
        else
            g.setColour (i == hover ? colours::raised() : colours::panel2());
        g.fillRoundedRectangle (r, 8.0f);
        if (on) drawGloss (g, r, 8.0f);
        if (! on)
        {
            g.setColour (i == hover ? colours::outline().brighter (0.2f) : colours::outline());
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }
        auto t = r.reduced (12.0f, 6.0f);
        g.setColour (on ? colours::onAccent() : colours::text());
        g.setFont (uiFont (13.0f, 1));
        g.drawFittedText (p.name + (on && owner.proc.presets.isModified() ? " *" : ""), t.removeFromTop (19.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        g.setColour (on ? colours::onAccent().withAlpha (0.8f) : colours::label());
        g.setFont (uiFont (10.5f));
        const bool all = owner.categories[owner.category] == "All";
        g.drawText (! p.factory ? juce::String ("User preset") : (all || p.genre.isEmpty() ? p.category : p.genre), t, juce::Justification::centredLeft);
    }
}

void PresetBrowser::Grid::mouseMove (const juce::MouseEvent& e)
{
    const int h = itemAt (e.getPosition());
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void PresetBrowser::Grid::mouseUp (const juce::MouseEvent& e)
{
    const int i = itemAt (e.getPosition());
    if (i < 0 || ! e.mouseWasClicked())
        return;
    const int idx = owner.visible[(size_t) i];
    if (e.mods.isPopupMenu())
    {
        const auto p = owner.proc.presets.getPreset (idx);
        juce::PopupMenu m;
        m.addItem (1, "Load");
        m.addItem (2, "Delete...", ! p.factory);
        m.addItem (3, "Show in folder", ! p.factory);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                         [safe = juce::Component::SafePointer<Grid> (this), idx, file = p.file] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1) safe->owner.proc.presets.loadPreset (idx);
            if (r == 2) safe->owner.askDelete (idx);
            if (r == 3) file.revealToUser();
            safe->repaint();
        });
        return;
    }
    owner.proc.presets.loadPreset (idx);
    const auto cur = owner.proc.presets.getPreset (idx);
    owner.deleteButton.setEnabled (! cur.factory);
    repaint();
}

void PresetBrowser::Grid::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (itemAt (e.getPosition()) >= 0)
        owner.close();
}

//==============================================================================
// About
//==============================================================================
AboutOverlay::AboutOverlay() : juce::Thread ("Chupa Loops update check")
{
    updateButton.setButtonText ("CHECK FOR UPDATES");
    updateButton.onClick = [this] { checkForUpdates(); };
    downloadButton.setButtonText ("DOWNLOAD");
    downloadButton.onClick = [this]
    {
        juce::String url;
        { const juce::ScopedLock sl (statusLock); url = downloadUrl; }
        if (url.isNotEmpty()) juce::URL (url).launchInDefaultBrowser();
    };
    downloadButton.setVisible (false);
    websiteButton.setButtonText ("THEBEGINNINGOFHOUSE.NL");
    websiteButton.onClick = [] { juce::URL ("https://thebeginningofhouse.nl").launchInDefaultBrowser(); };
    tourButton.setButtonText ("SHOW THE TOUR");
    tourButton.onClick = [this] { close(); if (onShowTour) onShowTour(); };
    addAndMakeVisible (updateButton);
    addChildComponent (downloadButton);
    addAndMakeVisible (websiteButton);
    addAndMakeVisible (tourButton);

    if (juce::String (CHUPALOOPS_UPDATE_REPO).isEmpty())
    {
        updateButton.setEnabled (false);
        updateStatus = "Update check is available in release builds.";
    }
}

AboutOverlay::~AboutOverlay()
{
    stopTimer();
    signalThreadShouldExit();
    {
        const juce::ScopedLock sl (statusLock);
        if (activeStream != nullptr)
            activeStream->cancel();   // unblocks a slow connection right away
    }
    stopThread (10000);
}

void AboutOverlay::visibilityChanged()
{
    if (isVisible()) startTimerHz (30);
    else             stopTimer();
}

void AboutOverlay::layout (juce::Rectangle<int> c)
{
    updateButton.setBounds (c.getX() + 290, c.getY() + 452, 170, 30);
    downloadButton.setBounds (c.getX() + 470, c.getY() + 452, 110, 30);
    websiteButton.setBounds (c.getX() + 30, c.getY() + 336, 226, 30);
    tourButton.setBounds (c.getX() + 30, c.getY() + 374, 226, 30);
}

void AboutOverlay::timerCallback()
{
    time += 1.0f / 30.0f;
    repaint (card().withSize (260, 300));
    bool showDownload;
    { const juce::ScopedLock sl (statusLock); showDownload = downloadUrl.isNotEmpty(); }
    if (showDownload != downloadButton.isVisible())
    {
        downloadButton.setVisible (showDownload);
        repaint();
    }
    if (! isThreadRunning() && ! updateButton.isEnabled() && juce::String (CHUPALOOPS_UPDATE_REPO).isNotEmpty())
    {
        updateButton.setEnabled (true);
        repaint();
    }
}

void AboutOverlay::checkForUpdates()
{
    if (isThreadRunning())
        return;
    {
        const juce::ScopedLock sl (statusLock);
        updateStatus = "Checking...";
        downloadUrl.clear();
    }
    updateButton.setEnabled (false);
    repaint();
    startThread();
}

void AboutOverlay::run()
{
    juce::String status, url;
    const juce::URL api ("https://api.github.com/repos/" + juce::String (CHUPALOOPS_UPDATE_REPO) + "/releases/tags/latest");
    auto stream = std::make_unique<juce::WebInputStream> (api, false);
    stream->withConnectionTimeout (6000).withExtraHeaders ("User-Agent: ChupaLoops\r\nAccept: application/vnd.github+json");
    {
        const juce::ScopedLock sl (statusLock);
        activeStream = stream.get();
    }
    const bool connected = ! threadShouldExit() && stream->connect (nullptr) && stream->getStatusCode() == 200;
    const juce::String body = connected && ! threadShouldExit() ? stream->readEntireStreamAsString() : juce::String();
    {
        const juce::ScopedLock sl (statusLock);
        activeStream = nullptr;
    }
    if (body.isNotEmpty())
    {
        const auto json = juce::JSON::parse (body);
        const auto name = json.getProperty ("name", {}).toString();
        const int latest = name.fromLastOccurrenceOf ("#", false, false).retainCharacters ("0123456789").getIntValue();
        const int mine = ownBuildNumber();
        if (latest <= 0)
            status = "Couldn't read the latest version.";
        else if (latest > mine)
        {
            status = "Update available: build " + juce::String (latest) + " (you have build " + juce::String (mine) + ").";
            url = json.getProperty ("html_url", {}).toString();
        }
        else
            status = "You have the latest version (build " + juce::String (mine) + ").";
    }
    else
        status = "No connection - try again later.";

    if (threadShouldExit())
        return;
    const juce::ScopedLock sl (statusLock);
    updateStatus = status;
    downloadUrl = url;
}

void AboutOverlay::paint (juce::Graphics& g)
{
    Overlay::paint (g);
    const auto c = card().toFloat();
    const auto& s = skin();

    // mascot + name
    auto m = juce::Rectangle<float> (c.getX() + 55, c.getY() + 40, 150, 150);
    drawMascot (g, s, m, 0.0f, time);
    float h = 34.0f;
    const auto font = displayFont (s, h);
    const float w = juce::GlyphArrangement::getStringWidth (font, "Chupa Loops") * 1.1f + h * 0.2f;
    if (w > 220.0f) h *= 220.0f / w;
    drawWordmark (g, s, { c.getX() + 30, c.getY() + 222 }, h);

    g.setColour (colours::dim());
    g.setFont (uiFont (13.0f, 1));
    g.drawText ("Version " + juce::String (JucePlugin_VersionString) + "  (build " + juce::String (ownBuildNumber()) + ")", juce::Rectangle<float> (c.getX() + 30, c.getY() + 250, 220, 20), juce::Justification::centredLeft);
    g.setColour (colours::label());
    g.setFont (uiFont (12.5f));
    g.drawText ("Made by Alexander Koning", juce::Rectangle<float> (c.getX() + 30, c.getY() + 270, 220, 20), juce::Justification::centredLeft);
    g.drawText ("Skin: " + s.name, juce::Rectangle<float> (c.getX() + 30, c.getY() + 290, 220, 20), juce::Justification::centredLeft);

    // MIDI chart
    const float x = c.getX() + 290, colW = c.getRight() - 40 - x;
    float y = c.getY() + 40;
    g.setColour (colours::text());
    g.setFont (uiFont (12.0f, 2));
    g.drawText ("MIDI CONTROL  (MIDI NOTES = CONTROL)", juce::Rectangle<float> (x, y, colW, 18), juce::Justification::centredLeft);
    y += 24;
    const std::pair<juce::String, juce::String> rows[] = {
        { noteName (36), "New loop" },
        { noteName (37) + " / " + noteName (38), "Previous / next version" },
        { noteName (39), "Unlock all slices" },
        { noteName (40), "The skin's crazy button (" + s.crazyName + ")" },
        { noteName (41), "Mutate (a variation of this loop)" },
        { noteName (48) + " - " + noteName (56), "Rhythm: Free, 4 to the floor, Offbeat ... Random" },
        { noteName (60) + " - " + noteName (65), "Length: 1, 2, 4, 8, 16, 32 bars" },
        { noteName (72) + " - " + noteName (79), "Scenes A - H" },
        { "Program change", "0 = Init, 1 - " + juce::String ((int) factoryPresets().size() - 1) + " = factory presets" },
        { "Right-click", "Any knob or selector: MIDI learn a CC" },
        { "Automation", "\"New Loop\" parameter makes a new loop" },
    };
    auto drawRows = [&] (auto& list)
    {
        for (auto& r : list)
        {
            g.setColour (colours::text());
            g.setFont (uiFont (12.5f, 2));
            g.drawText (r.first, juce::Rectangle<float> (x, y, 130, 20), juce::Justification::centredLeft);
            g.setColour (colours::dim());
            g.setFont (uiFont (12.5f));
            g.drawText (r.second, juce::Rectangle<float> (x + 134, y, colW - 134, 20), juce::Justification::centredLeft);
            y += 22;
        }
    };
    drawRows (rows);

    // instrument modes
    y += 8;
    g.setColour (colours::text());
    g.setFont (uiFont (12.0f, 2));
    g.drawText ("MIDI NOTES = SLICES / KEYS", juce::Rectangle<float> (x, y, colW, 18), juce::Justification::centredLeft);
    y += 24;
    const std::pair<juce::String, juce::String> modeRows[] = {
        { "Slices", noteName (36) + " = the whole loop, " + noteName (37) + " and up = every different slice" },
        { "Keys", noteName (60) + " = original pitch: play the loop in any key" },
    };
    drawRows (modeRows);

    // update status
    juce::String status;
    { const juce::ScopedLock sl (statusLock); status = updateStatus; }
    g.setColour (colours::text());
    g.setFont (uiFont (12.0f, 2));
    g.drawText ("UPDATES", juce::Rectangle<float> (x, c.getY() + 426, colW, 18), juce::Justification::centredLeft);
    g.setColour (colours::dim());
    g.setFont (uiFont (12.5f));
    g.drawText (status, juce::Rectangle<float> (x, c.getY() + 488, colW, 20), juce::Justification::centredLeft);

    // credits
    g.setColour (colours::outline());
    g.fillRect (c.getX() + 30, c.getBottom() - 78, c.getWidth() - 60, 1.0f);
    g.setColour (colours::label());
    g.setFont (uiFont (11.5f));
    g.drawFittedText ("Built with JUCE. Time-stretching: Signalsmith Stretch (MIT license). Fonts: Inter, Erica One, Boldonse, Tektur and Big Shoulders "
                      "(SIL Open Font License). VST is a trademark of Steinberg Media Technologies GmbH. Audio Unit is a trademark of Apple Inc.",
                      juce::Rectangle<int> ((int) c.getX() + 30, (int) c.getBottom() - 70, (int) c.getWidth() - 60, 50), juce::Justification::topLeft, 3);
}

} // namespace slicetribe

namespace slicetribe
{

//==============================================================================
// First-run tour
//==============================================================================
TourOverlay::TourOverlay()
{
    setVisible (false);
    setWantsKeyboardFocus (true);
    nextButton.setClickingTogglesState (false);
    nextButton.setToggleState (true, juce::dontSendNotification);   // drawn in the accent colour
    nextButton.onClick = [this] { goTo (index + 1); };
    skipButton.setButtonText ("SKIP");
    skipButton.onClick = [this] { finish(); };
    addAndMakeVisible (nextButton);
    addAndMakeVisible (skipButton);
}

void TourOverlay::start (std::vector<Step> s)
{
    steps = std::move (s);
    index = 0;
    setVisible (true);
    toFront (true);
    goTo (0);
    grabKeyboardFocus();
}

void TourOverlay::goTo (int i)
{
    if (i >= (int) steps.size())
    {
        finish();
        return;
    }
    index = juce::jmax (0, i);
    nextButton.setButtonText (index == (int) steps.size() - 1 ? "LET'S GO" : "NEXT");
    skipButton.setVisible (index < (int) steps.size() - 1);
    layoutButtons();
    repaint();
}

void TourOverlay::finish()
{
    setVisible (false);
    if (onFinished) onFinished();
}

bool TourOverlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { finish(); return true; }
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::rightKey) { goTo (index + 1); return true; }
    if (k == juce::KeyPress::leftKey) { goTo (index - 1); return true; }
    return false;   // other keys (e.g. the host's space bar) keep working
}

void TourOverlay::visibilityChanged()
{
    if (isShowing())
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TourOverlay> (this)]
        {
            if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
        });
}

void TourOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && ! cardBounds().contains (e.position))
        goTo (index + 1);
}

juce::Rectangle<float> TourOverlay::cardBounds() const
{
    if (steps.empty())
        return {};
    const auto f = steps[(size_t) index].focus;
    const float w = 380.0f, h = 150.0f;
    const auto area = getLocalBounds().toFloat().reduced (16.0f);
    float x = juce::jlimit (area.getX(), area.getRight() - w, f.getCentreX() - w * 0.5f);
    float y = f.getBottom() + 16.0f;
    if (y + h > area.getBottom())
        y = f.getY() - 16.0f - h;          // above the focus
    if (y < area.getY())
    {
        // the focus fills the height: next to it
        y = juce::jlimit (area.getY(), area.getBottom() - h, f.getCentreY() - h * 0.5f);
        x = f.getX() - 16.0f - w >= area.getX() ? f.getX() - 16.0f - w : juce::jmin (area.getRight() - w, f.getRight() + 16.0f);
    }
    return { x, y, w, h };
}

void TourOverlay::layoutButtons()
{
    const auto c = cardBounds().toNearestInt();
    nextButton.setBounds (c.getRight() - 110, c.getBottom() - 42, 94, 28);
    skipButton.setBounds (c.getRight() - 186, c.getBottom() - 42, 68, 28);
}

void TourOverlay::paint (juce::Graphics& g)
{
    if (steps.empty())
        return;
    const auto& st = steps[(size_t) index];
    const auto focus = st.focus.expanded (6.0f);

    // dim everything except the focus
    juce::Path dim;
    dim.setUsingNonZeroWinding (false);
    dim.addRectangle (getLocalBounds().toFloat());
    dim.addRoundedRectangle (focus, 12.0f);
    g.setColour (juce::Colours::black.withAlpha (0.62f));
    g.fillPath (dim);
    g.setColour (colours::cyan());
    g.drawRoundedRectangle (focus, 12.0f, 2.5f);

    // card
    const auto c = cardBounds();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (c.translated (0, 4.0f), 14.0f);
    g.setColour (colours::panel());
    g.fillRoundedRectangle (c, 14.0f);
    g.setColour (skin().sticker ? skin().stickerLine.withAlpha (0.6f) : colours::outline());
    g.drawRoundedRectangle (c.reduced (0.75f), 14.0f, 1.5f);

    auto t = c.reduced (18.0f, 14.0f);
    auto head = t.removeFromTop (22.0f);
    const auto counter = juce::String (index + 1) + " / " + juce::String ((int) steps.size());
    g.setColour (colours::dim());
    g.setFont (uiFont (11.5f, 1));
    g.drawText (counter, head.removeFromRight (50.0f), juce::Justification::centredRight);
    g.setColour (skin().light ? colours::accent().darker (0.2f) : colours::accent());
    g.setFont (uiFont (15.0f, 2));
    g.drawText (st.title.toUpperCase(), head, juce::Justification::centredLeft);
    t.removeFromTop (6.0f);
    t.removeFromBottom (36.0f);
    g.setColour (colours::text());
    g.setFont (uiFont (13.0f));
    g.drawFittedText (st.text, t.toNearestInt(), juce::Justification::topLeft, 5, 0.9f);
}

} // namespace slicetribe
