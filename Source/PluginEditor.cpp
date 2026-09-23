#include "PluginEditor.h"

namespace slicetribe
{

//==============================================================================
void PresetNameBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = isMouseOver();
    g.setColour (over ? colours::raised() : colours::panel2());
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (over ? colours::outline().brighter (0.3f) : colours::outline());
    g.drawRoundedRectangle (r, 7.0f, 1.0f);

    const auto p = proc.presets.getPreset (proc.presets.getCurrentIndex());
    const bool modified = proc.presets.isModified();
    auto t = r.reduced (12.0f, 0);
    auto arrowArea = t.removeFromRight (14.0f);
    juce::Path arrow;
    arrow.addTriangle (arrowArea.getCentreX() - 4.0f, r.getCentreY() - 2.0f, arrowArea.getCentreX() + 4.0f, r.getCentreY() - 2.0f,
                       arrowArea.getCentreX(), r.getCentreY() + 3.0f);
    g.setColour (colours::dim());
    g.fillPath (arrow);

    const juce::String cat = p.name.isNotEmpty() ? (p.factory ? p.category : juce::String ("User")) : juce::String ("Project");
    if (cat != "Init")
    {
        const auto cf = uiFont (10.5f, 1);
        const float cw = juce::GlyphArrangement::getStringWidth (cf, cat.toUpperCase()) + 10.0f;
        auto tag = t.removeFromLeft (cw).withSizeKeepingCentre (cw, 18.0f);
        t.removeFromLeft (8.0f);
        g.setColour (colours::accent().withAlpha (0.14f));
        g.fillRoundedRectangle (tag, 4.0f);
        g.setColour (skin().light ? colours::accent().darker (0.25f) : colours::accent());
        g.setFont (cf);
        g.drawText (cat.toUpperCase(), tag, juce::Justification::centred);
    }
    g.setColour (colours::text());
    g.setFont (uiFont (13.5f, 1));
    g.drawFittedText (proc.presets.getCurrentName() + (modified ? " *" : ""), t.toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);
}

//==============================================================================
MainView::MainView (SliceTribeProcessor& p)
    : proc (p),
      resultView (p),
      patternSel (p.apvts, "pattern", 3),
      lengthSel (p.apvts, "length", 6),
      motifSel (p.apvts, "motif", 4),
      modeSel (p.apvts, "sliceMode", 2),
      sizeSel (p.apvts, "sliceSize", 6),
      styleSel (p.apvts, "style", 3),
      stretchSel (p.apvts, "stretch", 2),
      fillSel (p.apvts, "fill", 5),
      midiModeSel (p.apvts, "midiMode", 3),
      feelSel (p.apvts, "feel", 3),
      chaos (p.apvts, "chaos", "Chaos", "Low: slices come from random places in your loops but keep their spot in the beat (the groove stays tight). High: anything goes."),
      variation (p.apvts, "variation", "Variation", "With Repeat on: how many slices change in every repeat of the motif."),
      gate (p.apvts, "gate", "Gate", "Note length. Lower = shorter, tighter, stabbier."),
      swing (p.apvts, "swing", "Swing", "Pushes every second slice back for a shuffle groove (on the grid you slice on)."),
      amount (p.apvts, "amount", "Amount", "Strength of the Glitch or Lo-Fi style."),
      reverse (p.apvts, "reverse", "Reverse", "Chance that a slice plays backwards.", true),
      octave (p.apvts, "octave", "Octave", "Chance that a slice plays one octave up (+12).", true),
      fade (p.apvts, "fade", "Fade", "Crossfade between slices. Short = punchy, longer = smoother.", true),
      sensitivity (p.apvts, "sensitivity", "Sens", "Transient detection sensitivity (Transient slice mode only).", true),
      energy (p.apvts, "energy", "Energy", "Builds the loop up: the further you get, the more rolls, octaves and reverses. 0% = the loop stays the same from start to end."),
      volume (p.apvts, "gain", "Volume", "Output volume", true),
      fxCutoff (p.apvts, "fxCutoff", "Cutoff", "Low-pass filter over the whole loop. 100% = open."),
      fxReso (p.apvts, "fxReso", "Reso", "Filter resonance."),
      fxEnv (p.apvts, "fxEnv", "Env", "Opens the filter on every slice and lets it close again: every chop gets its own 'wow'. Works with Cutoff below 100%."),
      fxDecay (p.apvts, "fxDecay", "Decay", "How fast the filter closes again after every slice (Env).", true),
      fxLowCut (p.apvts, "fxLowCut", "Low cut", "Removes low end (high-pass). Handy to make room for your kick and bass.", true),
      fxDrive (p.apvts, "fxDrive", "Drive", "Warm saturation / distortion."),
      fxPump (p.apvts, "fxPump", "Pump", "Sidechain pump on every beat, in time with your song."),
      fxWidth (p.apvts, "fxWidth", "Width", "Stereo width. 100% = unchanged, 0% = mono, 200% = extra wide.", true),
      presetName (p),
      dragOut (p, DragOutTile::audio),
      dragMidi (p, DragOutTile::midi),
      presetBrowser (p)
{
    wildcard = proc.getAudioWildcard();

    for (int i = 0; i < kAllSlots; ++i)   // 8 sample slots plus the box for your own track
    {
        auto* s = slots.add (new SlotComponent (proc, i));
        s->onFilesDropped = [this] (int slot, const juce::StringArray& files) { distributeFiles (slot, files); };
        s->acceptsFile = [this] (const juce::String& f) { return acceptsFile (f); };
        addAndMakeVisible (s);
    }

    addAndMakeVisible (resultView);

    patternSel.setTooltip ("Rhythm the slices are placed on. 'Free' = back-to-back slices of the chosen slice size.\nRight-click = MIDI learn.");
    lengthSel.setTooltip ("Length of the new loop in bars.");
    motifSel.setTooltip ("Repeats a 1, 2 or 4 bar motif through the loop so it stays musical. 'Off' = every bar is different.");
    modeSel.setTooltip ("Grid = cut on the beat grid. Transient = cut at every new note / attack.");
    sizeSel.setTooltip ("How big one slice is. Works on every rhythm: a 1/4 note on 1/32 becomes\n"
                        "eight slices, so you can have 32 slices in a bar. It is also the grid that\n"
                        "positions in the sample are picked from, and the grid Swing works on.");
    stretchSel.setTooltip ("How loops at another tempo are fitted.\nBeats: re-sliced on the grid - attacks and gaps stay exactly as recorded (best for basslines).\nSmooth: time-stretched - best for long legato notes and pads.");
    styleSel.setTooltip ("Clean: seamless, phase-aligned crossfades.\nGlitch: stutters, tape stops and chops.\nLo-Fi: crunchy vintage sampler.");
    fillSel.setTooltip ("Fill: the last half bar of every 4, 8, 16 or 32 bars becomes a stutter roll, like a drummer's fill at the end of a phrase.\n"
                        "A loop shorter than that gets its fill at the end of the loop.\nRight-click = MIDI learn.");
    midiModeSel.setTooltip ("What MIDI notes do.\nControl: C1 = new loop, F1 = mutate, C4-G4 = scenes (see ? for all notes).\n"
                            "Slices: play the loop as an instrument - C1 = the whole loop, C#1 up to G8 = every different slice on its own key (use with DRAG MIDI).\n"
                            "Keys: the loop follows the key you play (C3 = original pitch, up to 2 octaves up or down), in time with your song.\n"
                            "In Slices and Keys the control notes are off.");
    feelSel.setTooltip ("Speed of the sample material: Half time plays your loops at half speed (the beat grid stays), Double time twice as fast.\nRight-click = MIDI learn.");
    for (auto* c : { &patternSel, &lengthSel, &motifSel, &modeSel, &sizeSel, &styleSel, &stretchSel, &fillSel, &midiModeSel, &feelSel })
        addAndMakeVisible (c);

    for (auto* k : { &chaos, &variation, &gate, &swing, &amount, &reverse, &octave, &fade, &sensitivity, &energy })
        addAndMakeVisible (k);
    addChildComponent (volume);
    for (auto* k : { &fxCutoff, &fxReso, &fxEnv, &fxDecay, &fxLowCut, &fxDrive, &fxPump, &fxWidth })
        addChildComponent (k);
    charTabs.setSelected (juce::jlimit (0, 1, proc.editorTab.load()));
    charTabs.onChange = [this] (int t)
    {
        proc.cancelMidiLearn();          // an armed knob on the other tab would be invisible
        proc.editorTab = t;
        updateCharacterTab();
        repaint (568, 520, 344, 264);
    };
    addAndMakeVisible (charTabs);

    for (int i = 0; i < SliceTribeProcessor::numScenes; ++i)
    {
        auto* b = sceneButtons.add (new SceneButton (proc, i));
        const juce::String letter = juce::String::charToString ((juce::juce_wchar) ('A' + i));
        b->setTooltip ("Scene " + letter + ": click an empty scene to store the loop (with its settings and FX), click a stored scene to bring it back.\n"
                       "Shift-click = store (replaces), right-click = store / recall / clear.\nMIDI note " + juce::MidiMessage::getMidiNoteName (72 + i, true, true, 3)
                       + " recalls it (MIDI NOTES on Control).");
        b->onMessage = [this] (const juce::String& m) { flash (m); };
        addAndMakeVisible (b);
    }

    keyBox.addItemList (choices::keys(), 1);
    keyBox.setTooltip ("Key match: every sample whose key is known (from Splice-style names like 'Am' or 'Fmin', or heard in the audio) is moved to this key or its relative major/minor (Am fits C). "
                       "Drum loops are never transposed. 'Off' = no change - and Off is where the standalone app starts every time, "
                       "so nothing is transposed without you asking. A project in your DAW keeps the key you saved with it.");
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);
    addAndMakeVisible (keyBox);

    generateButton.setTooltip (juce::String (proc.isStandalone() ? "Make a new loop (N). Locked slices stay." : "Make a new loop. Locked slices stay.")
                               + "\nMIDI note C1 does the same (MIDI NOTES on Control). Right-click = MIDI learn.");
    generateButton.onClick = [this]
    {
        if (proc.newLoopNeutral.load())
            proc.knobsToNeutral();
        proc.generateNew();
    };
    addAndMakeVisible (generateButton);

    crazyButton.onClick = [this]
    {
        proc.crazyLoop (currentSkinIndex());
        flash (skin().crazyName + "!  Load your preset again to calm down.", colours::cyan());
    };
    addAndMakeVisible (crazyButton);

    // your choice: does NEW LOOP leave the panel as you set it, or start clean every time
    neutralButton.setClickingTogglesState (false);
    neutralButton.onClick = [this]
    {
        proc.newLoopNeutral = ! proc.newLoopNeutral.load();
        refreshNeutralButton();
    };
    addAndMakeVisible (neutralButton);
    refreshNeutralButton();

    mutateButton.setButtonText ("MUTATE");
    mutateButton.setTooltip ("A variation of this loop: about a quarter of the unlocked slices change, the rest stays.\nMIDI note F1 does the same (MIDI NOTES on Control).");
    mutateButton.onClick = [this] { proc.mutate(); };
    addAndMakeVisible (mutateButton);

    rhythmButton.setButtonText ("RHY");
    rhythmButton.setTooltip ("New rhythm, same slices: only where the slices land changes.");
    rhythmButton.onClick = [this] { proc.rerollRhythm(); };
    addAndMakeVisible (rhythmButton);

    sourcesButton.setButtonText ("SRC");
    sourcesButton.setTooltip ("Same rhythm, other slices: the groove stays, the sounds change.");
    sourcesButton.onClick = [this] { proc.rerollSources(); };
    addAndMakeVisible (sourcesButton);

    autoPickButton.setButtonText ("AUTO PICK");
    autoPickButton.setTooltip ("Makes 8 loops, listens to them and keeps the best one: full enough, punchy, nicely spread over your samples.\nNeeds samples and a loop.");
    autoPickButton.onClick = [this] { proc.autoPick (8); flash ("AUTO PICK: making 8 loops and keeping the best...", colours::cyan()); };
    addAndMakeVisible (autoPickButton);

    keepButton.setButtonText ("KEEP");
    keepButton.setTooltip ("Puts this loop in the first free scene (A - H), so you can keep looking without losing it.\nNeeds a loop.");
    keepButton.onClick = [this]
    {
        const int i = proc.keepToScene();
        if (i >= 0) flash ("Kept in scene " + juce::String::charToString ((juce::juce_wchar) ('A' + i)));
        else        flash ("All 8 scenes are full - right-click a scene to replace it", colours::error());
    };
    addAndMakeVisible (keepButton);

    backButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80"));
    backButton.setTooltip (proc.isStandalone() ? "Previous version (Cmd/Ctrl+Z)" : "Previous version");
    backButton.onClick = [this] { proc.historyBack(); };
    forwardButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
    forwardButton.setTooltip (proc.isStandalone() ? "Next version (Shift+Cmd/Ctrl+Z)" : "Next version");
    forwardButton.onClick = [this] { proc.historyForward(); };
    addAndMakeVisible (backButton);
    addAndMakeVisible (forwardButton);

    historyLabel.setJustificationType (juce::Justification::centred);
    historyLabel.setFont (uiFont (12.5f, 1));
    historyLabel.setTooltip ("Version history of this session");
    addAndMakeVisible (historyLabel);

    exportButton.setButtonText ("EXPORT...");
    exportButton.setTooltip ("Save the loop as WAV, as MIDI, or as a slice kit (every slice as its own WAV + the MIDI)");
    exportButton.onClick = [this] { showExportMenu(); };
    addAndMakeVisible (exportButton);

    dragOut.setTooltip ("Drag the loop as audio (WAV) straight onto an audio track in your DAW.\nA copy is kept in Music/Chupa Loops.");
    addAndMakeVisible (dragOut);
    dragMidi.setTooltip ("Drag the loop as MIDI onto a Chupa Loops instrument track: one note per slice (C#1 = slice 1, D1 = slice 2 ...).\n"
                         "Set MIDI NOTES to Slices, then edit the notes to re-arrange the loop. C1 plays the whole loop.");
    dragMidi.onDragStarted = [this]
    {
        if (proc.getNumDifferentSlices() > RenderResult::maxSliceNotes)
            flash ("This loop has " + juce::String (proc.getNumDifferentSlices()) + " different slices - only the first "
                   + juce::String (RenderResult::maxSliceNotes) + " fit on the keyboard. Use Repeat or a shorter loop.", colours::error());
        else if (proc.getMidiMode() != 1)
            flash ("Tip: set MIDI NOTES to Slices so these notes play the slices", colours::cyan());
    };
    addAndMakeVisible (dragMidi);

    previewButton.setTooltip (proc.isStandalone() ? "Play / stop (space bar)" : "Listen without starting your DAW's transport.\nStops automatically when your DAW plays.");
    previewButton.onClick = [this] { proc.setPreview (! proc.isPreviewing()); };
    addAndMakeVisible (previewButton);

    unlockButton.setButtonText ("UNLOCK ALL");
    unlockButton.setTooltip ("Release all locked slices");
    unlockButton.onClick = [this] { proc.unlockAll(); };
    addAndMakeVisible (unlockButton);

    spliceButton.setButtonText ("SPLICE");
    spliceButton.setTooltip ("Open Splice to find sounds. Drag them from the Splice app, the Splice Sounds plug-in or your DAW's Splice browser onto a slot:\n"
                             "tempo and key are read from Splice file names automatically.\n"
                             "Tip: if Splice already matched a sound to your song's key, set KEY to Off so it isn't transposed twice.");
    spliceButton.onClick = [] { juce::URL ("https://splice.com/sounds").launchInDefaultBrowser(); };
    addAndMakeVisible (spliceButton);

    clearAllButton.setButtonText ("CLEAR ALL");
    clearAllButton.setTooltip ("Empty all 8 slots and the track box, and put every knob back to normal (click twice)");
    clearAllButton.onClick = [this] { clearAllSlots(); };
    addAndMakeVisible (clearAllButton);

    // presets
    presetName.setTooltip ("Click to browse the presets");
    presetName.onClick = [this] { openPresetBrowser(); };
    addAndMakeVisible (presetName);
    presetPrev.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80"));
    presetPrev.setTooltip (proc.isStandalone() ? "Previous preset ([)" : "Previous preset");
    presetPrev.onClick = [this] { proc.presets.loadNext (-1); };
    presetNext.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
    presetNext.setTooltip (proc.isStandalone() ? "Next preset (])" : "Next preset");
    presetNext.onClick = [this] { proc.presets.loadNext (1); };
    presetSave.setButtonText ("SAVE");
    presetSave.setTooltip ("Save the current settings as your own preset");
    presetSave.onClick = [this] { presetBrowser.refresh(); presetBrowser.saveCurrentAs(); };
    for (auto* b : { &presetPrev, &presetNext, &presetSave })
        addAndMakeVisible (b);

    skinButton.setButtonText ("SKIN");
    skinButton.setTooltip ("Pimp your Chupa: choose a look");
    skinButton.onClick = [this] { showSkinMenu(); };
    addAndMakeVisible (skinButton);
    aboutButton.setButtonText ("?");
    aboutButton.setTooltip ("About, MIDI control and updates");
    aboutButton.onClick = [this] { openAbout(); };
    addAndMakeVisible (aboutButton);

    tempoField.minValue = 40; tempoField.maxValue = 300; tempoField.step = 0.1; tempoField.pixelsPerStep = 1.5;
    tempoField.allowTextEntry = true;
    tempoField.lockedTag = "SYNC";
    tempoField.format = [] (double v) { return juce::String (v, 1) + " BPM"; };
    tempoField.onChange = [this] (double v) { proc.setFallbackBpm (v); };
    addAndMakeVisible (tempoField);

    presetBrowser.onMessage = [this] (const juce::String& m, juce::Colour c) { flash (m, c); };
    presetBrowser.onClose = [this] { if (proc.isStandalone()) grabKeyboardFocus(); };
    about.onClose = presetBrowser.onClose;
    about.onShowTour = [this] { startTour(); };
    tour.onFinished = [this]
    {
        writeSetting ("tourDone", true);
        if (proc.isStandalone()) grabKeyboardFocus();
    };
    addChildComponent (presetBrowser);
    addChildComponent (about);
    addChildComponent (tour);

    setWantsKeyboardFocus (proc.isStandalone());
    addMouseListener (this, true);   // any click cancels a pending MIDI learn

    setSize (designWidth, designHeight);
    lastGenerate = proc.getGenerateCount();
    lastMidiEvent = proc.getMidiEventVersion();
    lastSkin = skinVersion();
    refreshSlots();
    updateState();
    updateCharacterTab();
    startTimerHz (30);

    if (! (bool) readSetting ("tourDone", false))
    {
        writeSetting ("tourDone", true);   // shown once (it can always be opened again in the ? screen)
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainView> (this)]
        {
            if (safe != nullptr) safe->startTour();
        });
    }
}

void MainView::startTour()
{
    closeOverlays();
    tour.setBounds (getLocalBounds());
    tour.start ({
        { { 16, 102, 1088, 196 }, "1. Drop your loops",
          "Drop up to 8 loops here - basslines, synths, vocals, drums, any tempo and any key. Tempo and key are read from the file name, or from the audio itself. "
          "The triangle listens to one sample on its own, the % says how often slices are taken from it, and the two lines over the waveform pick the part you "
          "want to use. STR pulls a human recording - an old record, a live take - onto the grid." },
        { { 16, 520, 262, 92 }, "2. FIT TO TRACK",
          "Drop a part of YOUR OWN song in this box. It is never sliced: it is the track the new loop has to fit around. Chupa Loops hears where your "
          "track is busy and leaves room there, and the key follows it. The % says how hard the new loop stays out of your track's way, and the two "
          "lines on the waveform pick the part it listens to." },
        { { 924, 520, 180, 60 }, "3. New loop",
          "Every click builds a brand new loop from slices out of all your loops. Not a chopped-up copy: a new groove." },
        { { 16, 310, 1088, 198 }, "4. Shape it",
          "Click a slice to swap it for another, right-click to lock it. AUTO PICK makes eight loops and keeps the best, KEEP parks a loop in a scene. "
          "In the panel on the right: MUTATE for a variation, RHY for another rhythm, SRC for other sounds." },
        { { 16, 620, 262, 164 }, "5. Rhythm",
          "Pick the rhythm the slices are placed on, and a FILL: the last half bar of every 4, 8, 16 or 32 bars becomes a roll, like a drummer's fill." },
        { { 290, 520, 266, 264 }, "6. Length and slicing",
          "The length of the loop (1 to 32 bars), how often a motif repeats, how the slices are cut, and TIME FEEL for half or double speed." },
        { { 384, 66, 354, 32 }, "7. Scenes",
          "Store your favourite loops in scenes A to H and switch between them while you play (MIDI C4 to G4)." },
        { { 924, 716, 180, 62 }, "8. Into your song",
          "Drag the loop out as WAV, or as MIDI: with MIDI NOTES set to Slices every note plays one slice (C1 = the whole loop). The FX tab gives it the finishing touch." },
    });
}

void MainView::updateCharacterTab()
{
    const bool fxTab = charTabs.getSelected() == 1;
    for (auto* k : { &chaos, &variation, &gate, &swing, &amount, &reverse, &octave, &fade, &sensitivity, &energy })
        k->setVisible (! fxTab);
    styleSel.setVisible (! fxTab);
    volume.setVisible (fxTab);
    for (auto* k : { &fxCutoff, &fxReso, &fxEnv, &fxDecay, &fxLowCut, &fxDrive, &fxPump, &fxWidth })
        k->setVisible (fxTab);
}

MainView::~MainView()
{
    stopTimer();
    proc.cancelMidiLearn();
}

void MainView::visibilityChanged()
{
    if (proc.isStandalone() && isShowing())
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainView> (this)]
        {
            if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
        });
}

void MainView::mouseDown (const juce::MouseEvent&)
{
    if (proc.getLearningParamId().isNotEmpty())
        proc.cancelMidiLearn();
}

bool MainView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && proc.getLearningParamId().isNotEmpty())
    {
        proc.cancelMidiLearn();
        return true;
    }
    if (! proc.isStandalone() || presetBrowser.isVisible() || about.isVisible() || tour.isVisible())
        return false;   // no shortcuts behind an open screen
    if (key == juce::KeyPress::spaceKey)
    {
        proc.setPreview (! proc.isPreviewing());
        return true;
    }
    const auto ch = key.getTextCharacter();
    if ((ch == 'n' || ch == 'N') && ! key.getModifiers().isAnyModifierKeyDown())
    {
        if (loadedCount > 0) proc.generateNew();
        return true;
    }
    if (ch == '[' || ch == ']')
    {
        proc.presets.loadNext (ch == '[' ? -1 : 1);
        return true;
    }
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        proc.historyBack();
        return true;
    }
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        proc.historyForward();
        return true;
    }
    return false;
}

//==============================================================================
void MainView::paint (juce::Graphics& g)
{
    const auto& s = skin();
    drawSkinBackground (g, s, getLocalBounds().toFloat());

    // logo: mascot + word mark
    drawMascot (g, s, logoArea(), mascotAnim, time);
    {
        float h = 34.0f;
        const juce::String full = s.word1 + " " + s.word2;
        const float w = juce::GlyphArrangement::getStringWidth (displayFont (s, h), s.upperCaseWordmark ? full.toUpperCase() : full) + h * 0.2f;
        if (w > 196.0f) h *= 196.0f / w;
        const float mark = drawWordmark (g, s, { 76.0f, 29.0f }, h);
        drawByline (g, s, { 78.0f, 46.0f, juce::jmax (140.0f, mark), 14.0f });
    }

    // samples toolbar (drawn straight on the background)
    const juce::String hint ("Drop up to 8 loops - any tempo, any key.");
    if (s.sticker)
    {
        // candy skins: a title chip and dark pills, so the text stays readable on the loud background
        auto chip = juce::Rectangle<float> (16, 72, 88, 22);
        g.setGradientFill (colours::accentGradient (chip));
        g.fillRoundedRectangle (chip, 11.0f);
        drawGloss (g, chip, 11.0f);
        g.setColour (s.stickerLine.withAlpha (0.8f));
        g.drawRoundedRectangle (chip, 11.0f, 1.2f);
        g.setColour (colours::onAccent());
        g.setFont (uiFont (11.0f, 2));
        g.drawText ("SAMPLES", chip, juce::Justification::centred);

        const auto hf = uiFont (12.0f, 1);
        const float hw = juce::GlyphArrangement::getStringWidth (hf, hint) + 26.0f;
        auto pill = juce::Rectangle<float> (112, 72, hw, 22);
        g.setColour (s.stickerLine.withAlpha (0.5f));
        g.fillRoundedRectangle (pill, 11.0f);
        g.setColour (juce::Colours::white);
        g.setFont (hf);
        g.drawText (hint, pill, juce::Justification::centred);

        auto scenePill = juce::Rectangle<float> (390, 72, 70, 22);
        g.setColour (s.stickerLine.withAlpha (0.5f));
        g.fillRoundedRectangle (scenePill, 11.0f);
        g.setColour (juce::Colours::white);
        g.setFont (uiFont (11.0f, 2));
        g.drawText ("SCENES", scenePill, juce::Justification::centred);

        auto keyPill = juce::Rectangle<float> (762, 72, 42, 22);
        g.setColour (s.stickerLine.withAlpha (0.5f));
        g.fillRoundedRectangle (keyPill, 11.0f);
        g.setColour (juce::Colours::white);
        g.setFont (uiFont (11.0f, 2));
        g.drawText ("KEY", keyPill, juce::Justification::centred);
    }
    else
    {
        g.setColour (s.bgText);
        g.setFont (uiFont (12.0f, 2));
        g.drawText ("SAMPLES", juce::Rectangle<float> (18, 68, 80, 28), juce::Justification::centredLeft);
        g.setColour (s.bgDim);
        g.setFont (uiFont (12.0f));
        g.drawText (hint, juce::Rectangle<float> (96, 68, 290, 28), juce::Justification::centredLeft);
        g.setFont (uiFont (11.0f, 1));
        g.drawText ("SCENES", juce::Rectangle<float> (390, 68, 64, 28), juce::Justification::centredRight);
        g.drawText ("KEY", juce::Rectangle<float> (760, 68, 44, 28), juce::Justification::centredRight);
    }

    // panels
    drawPanel (g, { 16, 620, 262, 164 }, "Rhythm");
    drawPanel (g, { 290, 520, 266, 264 }, "Loop & slicing");
    drawPanel (g, { 568, 520, 344, 264 }, {});
    drawPanel (g, { 924, 520, 180, 264 }, {});

    // captions
    g.setColour (colours::label());
    g.setFont (uiFont (11.0f, 1));
    const float lx = 304;
    g.drawText ("FILL (EVERY ... BARS)", juce::Rectangle<float> (24, 742, 246, 14), juce::Justification::centredLeft);
    g.drawText ("LENGTH (BARS)", juce::Rectangle<float> (lx, 549, 240, 13), juce::Justification::centredLeft);
    g.drawText ("REPEAT",        juce::Rectangle<float> (lx, 588, 240, 13), juce::Justification::centredLeft);
    g.drawText ("SLICE MODE",    juce::Rectangle<float> (lx, 627, 120, 13), juce::Justification::centredLeft);
    g.drawText ("STRETCH",       juce::Rectangle<float> (428, 627, 120, 13), juce::Justification::centredLeft);
    g.drawText ("SLICE SIZE",    juce::Rectangle<float> (lx, 666, 240, 13), juce::Justification::centredLeft);
    g.drawText ("TIME FEEL",     juce::Rectangle<float> (lx, 705, 240, 13), juce::Justification::centredLeft);
    g.drawText ("MIDI NOTES",    juce::Rectangle<float> (lx, 744, 240, 13), juce::Justification::centredLeft);

    if (charTabs.getSelected() == 1)
    {
        g.setColour (colours::dim());
        g.setFont (uiFont (11.5f));
        g.drawFittedText ("The finishing touch on the whole loop - also in exports and drags. Env follows the slices, Pump the beat.",
                          juce::Rectangle<int> (582, 740, 316, 34), juce::Justification::centredLeft, 2, 0.9f);
    }
}

void MainView::resized()
{
    // header
    presetPrev.setBounds    (286, 16, 32, 32);
    presetName.setBounds    (322, 16, 300, 32);
    presetNext.setBounds    (626, 16, 32, 32);
    presetSave.setBounds    (664, 16, 58, 32);
    tempoField.setBounds    (740, 16, 116, 32);
    previewButton.setBounds (864, 16, 112, 32);
    skinButton.setBounds    (986, 16, 64, 32);
    aboutButton.setBounds   (1060, 16, 44, 32);

    // samples toolbar
    keyBox.setBounds         (810, 68, 86, 28);
    spliceButton.setBounds   (906, 68, 92, 28);
    clearAllButton.setBounds (1008, 68, 96, 28);
    for (int i = 0; i < sceneButtons.size(); ++i)
        sceneButtons[i]->setBounds (466 + i * 34, 69, 30, 26);

    // slots: 4 x 2, and your own track in the left column below the result
    const int sx = 16, sy = 102, gap = 12, sw = (designWidth - 32 - 3 * gap) / 4, sh = 92;
    for (int i = 0; i < kNumSlots; ++i)
        slots[i]->setBounds (sx + (i % 4) * (sw + gap), sy + (i / 4) * (sh + gap), sw, sh);
    slots[kTrackSlot]->setBounds (16, 520, 262, 92);

    resultView.setBounds (16, 310, designWidth - 32, 198);
    autoPickButton.setBounds (100, 316, 88, 20);
    keepButton.setBounds     (194, 316, 54, 20);

    // rhythm (the panel starts below the track box)
    patternSel.setBounds (24, 652, 246, 86);
    fillSel.setBounds    (24, 758, 246, 22);

    // loop & slicing
    lengthSel.setBounds   (304, 562, 238, 22);
    motifSel.setBounds    (304, 601, 238, 22);
    modeSel.setBounds     (304, 640, 114, 22);
    stretchSel.setBounds  (428, 640, 114, 22);
    sizeSel.setBounds     (304, 679, 238, 22);
    feelSel.setBounds     (304, 718, 238, 22);
    midiModeSel.setBounds (304, 757, 238, 22);

    // character | fx: tabs and style in the title row, knobs 5 x 2
    charTabs.setBounds (578, 525, 150, 24);
    styleSel.setBounds (732, 525, 166, 24);
    const int kx = 580, ky = 560, kw = 64, kh = 84;
    Knob* knobs[] = { &chaos, &variation, &gate, &swing, &amount, &reverse, &octave, &fade, &sensitivity, &energy };
    for (int i = 0; i < 10; ++i)
        knobs[i]->setBounds (kx + (i % 5) * kw, ky + (i / 5) * (kh + 6), kw, kh);
    volume.setBounds (kx + 4 * kw, ky + kh + 6, kw, kh);   // FX tab: bottom right
    Knob* fxKnobs[] = { &fxCutoff, &fxReso, &fxEnv, &fxDecay, &fxLowCut, &fxDrive, &fxPump, &fxWidth };
    for (int i = 0; i < 8; ++i)
        fxKnobs[i]->setBounds (kx + (i % 5) * kw, ky + (i / 5) * (kh + 6), kw, kh);

    // actions
    generateButton.setBounds (936, 530, 156, 44);
    mutateButton.setBounds   (936, 578, 76, 26);
    rhythmButton.setBounds   (1016, 578, 36, 26);
    sourcesButton.setBounds  (1056, 578, 36, 26);
    neutralButton.setBounds  (936, 608, 156, 22);
    crazyButton.setBounds    (936, 634, 156, 28);
    backButton.setBounds     (936, 666, 38, 24);
    historyLabel.setBounds   (976, 666, 76, 24);
    forwardButton.setBounds  (1054, 666, 38, 24);
    unlockButton.setBounds   (936, 694, 156, 24);
    exportButton.setBounds   (936, 722, 156, 24);
    dragOut.setBounds        (936, 750, 76, 26);
    dragMidi.setBounds       (1016, 750, 76, 26);

    presetBrowser.setBounds (getLocalBounds());
    about.setBounds (getLocalBounds());
    tour.setBounds (getLocalBounds());
}

//==============================================================================
void MainView::showSkinMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("PIMP YOUR CHUPA");
    for (int i = 0; i < numSkins; ++i)
    {
        juce::PopupMenu::Item item (skinAt (i).name);
        item.itemID = i + 1;
        item.isTicked = i == currentSkinIndex();
        item.image = std::make_unique<juce::DrawableImage> (skinThumbnail (i, 44));
        m.addItem (std::move (item));
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&skinButton).withStandardItemHeight (30),
                     [safe = juce::Component::SafePointer<MainView> (this)] (int r)
    {
        if (safe != nullptr && r > 0)
        {
            setCurrentSkin (r - 1);
            safe->timerCallback();
        }
    });
}

void MainView::showParamMenu (juce::Component& target, const juce::String& paramId)
{
    auto* param = proc.apvts.getParameter (paramId);
    if (param == nullptr)
        return;
    const int cc = proc.getMidiCcFor (paramId);
    const bool learning = proc.getLearningParamId() == paramId;

    juce::PopupMenu m;
    m.addSectionHeader (param->getName (40).toUpperCase());
    m.addItem (1, learning ? "Cancel MIDI learn" : "MIDI learn (move a knob on your controller)");
    m.addItem (2, cc >= 0 ? "Forget CC " + juce::String (cc) : juce::String ("No MIDI CC assigned"), cc >= 0);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                     [safe = juce::Component::SafePointer<MainView> (this), paramId, learning] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1)
        {
            if (learning) safe->proc.cancelMidiLearn();
            else
            {
                safe->proc.startMidiLearn (paramId);
                safe->learnTicks = 0;
                safe->flash ("MIDI learn: move a knob or fader on your controller (click anywhere to cancel)", colours::cyan());
            }
        }
        if (r == 2)
            safe->proc.clearMidiMapping (paramId);
        safe->repaint();
    });
}

void MainView::refreshNeutralButton()
{
    const bool on = proc.newLoopNeutral.load();
    neutralButton.setButtonText (on ? "NEW LOOP: ALL NEUTRAL" : "NEW LOOP: KEEP KNOBS");
    neutralButton.setTooltip (on ? "NEW LOOP also puts every knob back to neutral first, so you start clean every time.\nClick to keep your settings instead."
                                 : "NEW LOOP leaves the panel exactly as you set it.\nClick to have it start from neutral every time.");
    neutralButton.setColour (juce::TextButton::textColourOffId, on ? colours::cyan() : colours::dim());
    neutralButton.repaint();
}

juce::String MainView::midiTagFor (const juce::String& paramId)
{
    if (proc.getLearningParamId() == paramId)
        return "LEARN";
    const int cc = proc.getMidiCcFor (paramId);
    return cc >= 0 ? "CC " + juce::String (cc) : juce::String();
}

void MainView::clearAllSlots()
{
    if (clearConfirmTicks <= 0)
    {
        clearConfirmTicks = 90;
        clearAllButton.setButtonText ("SURE?");
        return;
    }
    clearConfirmTicks = 0;
    clearAllButton.setButtonText ("CLEAR ALL");
    for (int i = 0; i < kAllSlots; ++i)
        proc.clearSlot (i);
    proc.resetSettings();   // and every knob back to normal, so you start from scratch
}

//==============================================================================
bool MainView::acceptsFile (const juce::String& path) const
{
    const juce::File f (path);
    juce::StringArray patterns;
    patterns.addTokens (wildcard, ";", "");
    for (auto& p : patterns)
        if (f.getFileName().matchesWildcard (p.trim(), true))
            return true;
    return false;
}

bool MainView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (acceptsFile (f) || f.endsWithIgnoreCase (PresetManager::fileExtension))
            return true;
    return false;
}

void MainView::filesDropped (const juce::StringArray& files, int, int)
{
    // a dropped preset file is imported and loaded
    for (auto& f : files)
    {
        if (f.endsWithIgnoreCase (PresetManager::fileExtension))
        {
            const auto copy = proc.presets.importPresetFile (juce::File (f));
            if (copy.existsAsFile())
            {
                for (int i = 0; i < proc.presets.getNumPresets(); ++i)
                    if (proc.presets.getPreset (i).file == copy)
                        proc.presets.loadPreset (i);
                flash ("Preset imported: " + proc.presets.getCurrentName());
            }
            else
                flash ("This preset file can't be read", colours::error());
            return;
        }
    }

    int first = -1;
    for (int i = 0; i < kNumSlots && first < 0; ++i)
    {
        auto info = proc.getSlotInfo (i);
        if (! info.loaded && ! info.loading) first = i;
    }
    if (first < 0)
    {
        flash ("All 8 slots are full - drop onto a slot to replace it", colours::error());
        return;
    }
    distributeFiles (first, files);
}

void MainView::distributeFiles (int startSlot, const juce::StringArray& files)
{
    juce::StringArray audio;
    if (startSlot == kTrackSlot)   // the track box takes one file, it never spills into the sample slots
    {
        for (auto& f : files)
            if (acceptsFile (f))
            {
                proc.loadSlot (kTrackSlot, juce::File (f));
                return;
            }
        flash ("Unsupported file type - use WAV, AIFF, FLAC, MP3 or OGG", colours::error());
        return;
    }
    int rejected = 0;
    for (auto& f : files)
    {
        if (acceptsFile (f)) audio.add (f);
        else ++rejected;
    }
    if (audio.isEmpty())
    {
        flash ("Unsupported file type - use WAV, AIFF, FLAC, MP3 or OGG", colours::error());
        return;
    }

    proc.loadSlot (startSlot, juce::File (audio[0]));
    int next = 0, skipped = 0;
    for (int k = 1; k < audio.size(); ++k)
    {
        bool placed = false;
        for (; next < kNumSlots; ++next)
        {
            const int s = (startSlot + 1 + next) % kNumSlots;
            if (s == startSlot) continue;
            auto info = proc.getSlotInfo (s);
            if (! info.loaded && ! info.loading)
            {
                proc.loadSlot (s, juce::File (audio[k]));
                ++next;
                placed = true;
                break;
            }
        }
        if (! placed) ++skipped;
    }
    if (skipped > 0)
        flash (juce::String (skipped) + (skipped == 1 ? " file" : " files") + " not loaded - all 8 slots are full", colours::error());
    else if (rejected > 0)
        flash (juce::String (rejected) + " unsupported " + (rejected == 1 ? "file" : "files") + " skipped", colours::error());
}

void MainView::flash (const juce::String& text, juce::Colour c)
{
    resultView.showMessage (text, c, 110);
}

void MainView::showExportMenu()
{
    if (! proc.hasLoop())
    {
        flash ("Nothing to export yet - load some samples first", colours::error());
        return;
    }
    juce::PopupMenu m;
    m.addSectionHeader ("EXPORT");
    m.addItem (1, "Loop as WAV...");
    m.addItem (2, "Loop as MIDI (one note per slice)...");
    m.addItem (3, "Slice kit: every slice as a WAV + the MIDI");
    m.addItem (4, "Stems: one WAV per sample");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportButton),
                     [safe = juce::Component::SafePointer<MainView> (this)] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1) safe->exportWithDialog();
        if (r == 2) safe->exportMidiWithDialog();
        if (r == 3) safe->exportKit();
        if (r == 4) safe->exportStems();
    });
}

void MainView::exportMidiWithDialog()
{
    auto folder = SliceTribeProcessor::getDefaultExportFolder().getChildFile ("MIDI");
    folder.createDirectory();
    exportChooser = std::make_unique<juce::FileChooser> ("Export loop as MIDI", folder.getChildFile (proc.suggestedExportName() + ".mid"), "*.mid");
    exportChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
                                [this] (const juce::FileChooser& fc)
                                {
                                    auto target = fc.getResult();
                                    if (target == juce::File()) return;
                                    if (! target.hasFileExtension ("mid")) target = target.withFileExtension ("mid");
                                    if (! proc.exportMidiTo (target).existsAsFile())
                                        flash ("Saving failed - check the folder permissions", colours::error());
                                    else if (proc.getNumDifferentSlices() > RenderResult::maxSliceNotes)
                                        flash ("Saved - but only the first " + juce::String (RenderResult::maxSliceNotes) + " different slices fit on the keyboard", colours::error());
                                    else flash ("Saved: " + target.getFileName() + " - play it with MIDI NOTES set to Slices");
                                });
}

void MainView::exportKit()
{
    auto kit = proc.exportSliceKit (SliceTribeProcessor::getDefaultExportFolder().getChildFile ("Kits"));
    if (kit.isDirectory())
    {
        flash ("Slice kit saved in Music/Chupa Loops/Kits/" + kit.getFileName());
        if (proc.isStandalone())
            kit.revealToUser();   // in a DAW, opening a Finder/Explorer window would pull focus away
    }
    else
        flash ("Saving the kit failed - check the folder permissions", colours::error());
}

void MainView::exportStems()
{
    auto folder = proc.exportStems (SliceTribeProcessor::getDefaultExportFolder().getChildFile ("Stems"));
    if (folder == juce::File())
        flash ("Nothing to export yet", colours::error());
    else
        flash ("Making the stems...", colours::cyan());
}

void MainView::exportWithDialog()
{
    if (! proc.hasLoop())
    {
        flash ("Nothing to export yet - load some samples first", colours::error());
        return;
    }
    auto folder = SliceTribeProcessor::getDefaultExportFolder();
    folder.createDirectory();
    exportChooser = std::make_unique<juce::FileChooser> ("Export loop as WAV", folder.getChildFile (proc.suggestedExportName() + ".wav"), "*.wav");
    exportChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
                                [this] (const juce::FileChooser& fc)
                                {
                                    auto target = fc.getResult();
                                    if (target == juce::File()) return;
                                    if (! target.hasFileExtension ("wav")) target = target.withFileExtension ("wav");
                                    auto f = proc.exportLoopTo (target);
                                    if (f.existsAsFile()) flash ("Saved: " + f.getFileName());
                                    else                  flash ("Saving failed - check the folder permissions", colours::error());
                                });
}

void MainView::refreshSlots()
{
    loadedCount = 0;
    const int previewing = proc.getSlotPreview();
    for (int i = 0; i < kAllSlots; ++i)
    {
        auto info = proc.getSlotInfo (i);
        if (i < kNumSlots)
            loadedCount += info.loaded ? 1 : 0;
        slots[i]->refresh (info);
        slots[i]->setPreviewing (i == previewing);
    }
}

void MainView::updateState()
{
    historyLabel.setColour (juce::Label::textColourId, colours::dim());
    historyLabel.setText (juce::String (proc.getHistoryPosition() + 1) + " / " + juce::String (proc.getHistorySize()), juce::dontSendNotification);
    backButton.setEnabled (proc.getHistoryPosition() > 0);
    forwardButton.setEnabled (proc.getHistoryPosition() < proc.getHistorySize() - 1);

    // the tempo the loop really runs at: your own track's when one is loaded, otherwise the DAW's
    const bool trackLeads = proc.trackLeadsTempo();
    const bool tempoLocked = proc.hasHostTempo() || trackLeads;
    tempoField.setValue (proc.getLoopBpm());
    const juce::String tempoTag = trackLeads ? "TRACK" : "SYNC";
    if (tempoField.locked != tempoLocked || tempoField.lockedTag != tempoTag)
    {
        tempoField.locked = tempoLocked;
        tempoField.lockedTag = tempoTag;
        tempoField.setMouseCursor (tempoLocked ? juce::MouseCursor::NormalCursor : juce::MouseCursor::UpDownResizeCursor);
        tempoField.setTooltip (trackLeads ? "Tempo follows your own track (MY TRACK). Change it on the track box."
                             : tempoLocked ? "Tempo follows your DAW"
                                           : "Tempo: drag up/down (Shift = fine) or double-click to type");
        tempoField.repaint();
    }

    const bool prev = proc.isPreviewing();
    const juce::String playText = prev ? juce::String::fromUTF8 ("\xe2\x96\xa0  STOP")
                                       : juce::String::fromUTF8 (proc.isStandalone() ? "\xe2\x96\xb6  PLAY" : "\xe2\x96\xb6  PREVIEW");
    previewButton.setToggleState (prev, juce::dontSendNotification);
    if (previewButton.getButtonText() != playText)
        previewButton.setButtonText (playText);

    const bool hasLoop = proc.hasLoop();
    const int sliceable = loadedCount;   // loadedCount already leaves your own track out
    resultView.setOnlyReferenceLoaded (sliceable <= 0 && proc.getReferenceSlot() >= 0);
    trackLoaded = proc.getReferenceSlot() >= 0;
    generateButton.setEnabled (sliceable > 0);
    crazyButton.setEnabled (sliceable > 0);
    mutateButton.setEnabled (hasLoop);
    rhythmButton.setEnabled (hasLoop);
    sourcesButton.setEnabled (hasLoop);
    autoPickButton.setEnabled (hasLoop && ! proc.isJobBusy());
    keepButton.setEnabled (hasLoop);
    dragMidi.setEnabled (hasLoop);
    if (crazyButton.getTooltip().isEmpty() || ! crazyButton.getTooltip().startsWith (skin().crazyName))
        crazyButton.setTooltip (skin().crazyName + ": the craziest loop ever. Throws the rhythm, slicing and character around in "
                                + skin().name + " style and makes a new loop (MIDI note E1).\nLoad your preset again to go back.");
    exportButton.setEnabled (hasLoop);
    dragOut.setEnabled (hasLoop);
    previewButton.setEnabled (hasLoop || prev);
    clearAllButton.setEnabled (loadedCount > 0 || trackLoaded || clearConfirmTicks > 0);
    const int locks = proc.getLockedCount();
    unlockButton.setEnabled (locks > 0);
    const juce::String unlockText = locks > 0 ? "UNLOCK ALL (" + juce::String (locks) + ")" : juce::String ("UNLOCK ALL");
    if (unlockButton.getButtonText() != unlockText)
        unlockButton.setButtonText (unlockText);

    const auto st = proc.readSettings();
    const bool sensOn = st.sliceMode == 1, varOn = st.motifBars > 0 && st.motifBars < st.bars, amountOn = st.style != styleClean;
    // a knob that has no say right now is clearly switched off, not just a little paler
    sensitivity.setAlpha (sensOn ? 1.0f : 0.28f);
    variation.setAlpha (varOn ? 1.0f : 0.28f);
    amount.setAlpha (amountOn ? 1.0f : 0.28f);
    sensitivity.slider.setTooltip (juce::String ("Transient detection sensitivity.") + (sensOn ? "" : "\nOnly used in Transient slice mode.") + "\n(Shift-drag = fine, double-click = default, right-click = MIDI learn)");
    variation.slider.setTooltip (juce::String ("How many slices change in every repeat of the motif.") + (varOn ? "" : "\nOnly used when Repeat is on (and shorter than the loop).") + "\n(Shift-drag = fine, double-click = default, right-click = MIDI learn)");
    amount.slider.setTooltip (juce::String ("Strength of the Glitch or Lo-Fi style.") + (amountOn ? "" : "\nOnly used with Glitch or Lo-Fi.") + "\n(Shift-drag = fine, double-click = default, right-click = MIDI learn)");

    const juce::String presetText = proc.presets.getCurrentName() + (proc.presets.isModified() ? "*" : "") + juce::String (proc.presets.getCurrentIndex());
    if (presetText != lastPresetText)
    {
        lastPresetText = presetText;
        presetName.repaint();
    }
}

void MainView::timerCallback()
{
    time += 1.0f / 30.0f;

    if (skinVersion() != lastSkin)
    {
        lastSkin = skinVersion();
        if (onSkinChanged) onSkinChanged();
        refreshSlots();
        repaint();
    }

    if (proc.getSlotsVersion() != lastSlotsVersion)
    {
        lastSlotsVersion = proc.getSlotsVersion();
        refreshSlots();
    }

    if (proc.getGenerateCount() != lastGenerate)
    {
        lastGenerate = proc.getGenerateCount();
        mascotAnim = 1.0f;
        flashOnNextResult = true;
    }

    if (proc.getResultVersion() != lastResultVersion)
    {
        lastResultVersion = proc.getResultVersion();
        resultView.setResult (proc.getDisplayResult());
        refreshSlots();   // stretch ratios and key shifts depend on tempo / key
        if (flashOnNextResult)
        {
            flashOnNextResult = false;
            resultView.startChopFlash();
        }
    }

    if (proc.getMidiLearnVersion() != lastLearnVersion)
    {
        lastLearnVersion = proc.getMidiLearnVersion();
        repaint (560, 510, 560, 280);   // knobs, selectors and the generate button
        repaint (16, 510, 540, 280);
    }

    if (proc.getJobVersion() != lastJobVersion)
    {
        if (lastJobVersion >= 0)
            flash (proc.getJobMessage(), proc.jobFailed() ? colours::error() : colours::cyan());
        lastJobVersion = proc.getJobVersion();
    }

    if (proc.getSlotPreview() != lastSlotPreview)
    {
        lastSlotPreview = proc.getSlotPreview();
        for (int i = 0; i < kAllSlots; ++i)
            slots[i]->setPreviewing (i == lastSlotPreview);
    }
    if (const int pv = proc.getSlotPreview(); pv >= 0 && pv < slots.size())
        slots[pv]->setPreviewPosition (proc.getSlotPreviewPosition());

    if (proc.getScenesVersion() != lastScenes)
    {
        lastScenes = proc.getScenesVersion();
        for (auto* b : sceneButtons) b->repaint();
    }

    if (proc.getMidiEventVersion() != lastMidiEvent)
    {
        lastMidiEvent = proc.getMidiEventVersion();
        flash ("MIDI  " + proc.getLastMidiEvent(), colours::cyan());
    }

    if (clearConfirmTicks > 0 && --clearConfirmTicks == 0)
        clearAllButton.setButtonText ("CLEAR ALL");

    // mascot: "new loop" animation plus a little idle life
    if (mascotAnim > 0.0f)
        mascotAnim = juce::jmax (0.0f, mascotAnim - 1.0f / 24.0f);
    repaint (logoArea().expanded (8.0f).toNearestInt());

    // MIDI learn gives up after 20 s without a controller move
    if (proc.getLearningParamId().isNotEmpty())
    {
        if (++learnTicks > 30 * 20)
        {
            proc.cancelMidiLearn();
            flash ("MIDI learn cancelled - no controller moved", colours::dim());
        }
    }
    else
        learnTicks = 0;

    if (! presetBrowser.isVisible() && ! about.isVisible() && ! tour.isVisible())   // no playhead repaints under an overlay
        resultView.setPlayhead (proc.getPlayPosition());
    resultView.setBusy (proc.isBusy());
    resultView.tick();
    if (crazyButton.isEnabled())
        crazyButton.tick();
    updateState();
}

//==============================================================================
SliceTribeEditor::SliceTribeEditor (SliceTribeProcessor& p)
    : AudioProcessorEditor (p), proc (p), view (p)
{
    const int savedWidth = proc.editorWidth.load();   // read before any resize callback overwrites it
    setLookAndFeel (&lnf);
    if (proc.isStandalone())
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);   // audio settings dialog & window chrome match
    addAndMakeVisible (view);
    view.onSkinChanged = [this] { applySkin(); };

    setResizable (true, true);
    setResizeLimits (MainView::designWidth * 3 / 4, MainView::designHeight * 3 / 4,
                     MainView::designWidth * 2, MainView::designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) MainView::designWidth / MainView::designHeight);

    const int w = savedWidth;
    int width = w >= MainView::designWidth * 3 / 4 && w <= MainView::designWidth * 2 ? w : MainView::designWidth;
    // never taller than the screen (small laptop screens, or a size saved on a bigger monitor)
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const int maxH = juce::roundToInt (display->userArea.getHeight() * 0.9);
        const int maxW = juce::roundToInt (maxH * (double) MainView::designWidth / MainView::designHeight);
        if (maxH > 200 && width > maxW)
            width = juce::jmax (MainView::designWidth * 3 / 4, juce::jmin (width, maxW));
    }
    setSize (width, juce::roundToInt (width * (double) MainView::designHeight / MainView::designWidth));
}

SliceTribeEditor::~SliceTribeEditor()
{
    if (proc.isStandalone() && &juce::LookAndFeel::getDefaultLookAndFeel() == &lnf)
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SliceTribeEditor::applySkin()
{
    lnf.applySkin();
    sendLookAndFeelChange();
    parentHierarchyChanged();
    repaint();
}

void SliceTribeEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg());
}

void SliceTribeEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainView::designWidth;
    view.setBounds (0, 0, MainView::designWidth, MainView::designHeight);
    view.setTransform (juce::AffineTransform::scale (scale));
    proc.editorWidth = getWidth();
}

void SliceTribeEditor::parentHierarchyChanged()
{
    if (proc.isStandalone())
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
            w->setBackgroundColour (colours::bg());
}

} // namespace slicetribe
