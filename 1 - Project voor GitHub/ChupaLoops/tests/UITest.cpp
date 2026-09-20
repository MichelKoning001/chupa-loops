// Loads the real plugin processor + editor offscreen: renders audio through processBlock with a fake
// host transport, checks save/restore, and saves screenshots of the interface.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <set>

using namespace slicetribe;

struct FakeHost : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 140.0;
    bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cout << "FAIL: " << msg << "\n"; ++failures; } else std::cout << "ok   " << msg << "\n"; } while (0)

static bool waitFor (SliceTribeProcessor& p, int minVersion, int timeoutMs = 20000)
{
    const auto start = juce::Time::getMillisecondCounter();
    // give the worker a moment to start
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    while (juce::Time::getMillisecondCounter() - start < (juce::uint32) timeoutMs)
    {
        if (p.getResultVersion() >= minVersion && ! p.isBusy())
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            if (! p.isBusy()) return true;
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }
    return false;
}

static void runHost (SliceTribeProcessor& p, FakeHost& host, double seconds, juce::AudioBuffer<float>* capture)
{
    const int block = 512;
    const double rate = 48000.0;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    const int blocks = (int) (seconds * rate / block);
    if (capture) capture->setSize (2, blocks * block);
    for (int b = 0; b < blocks; ++b)
    {
        p.processBlock (buf, midi);
        if (capture)
            for (int c = 0; c < 2; ++c)
                capture->copyFrom (c, b * block, buf, c, 0, block);
        if (host.playing)
            host.ppq += block / rate * host.bpm / 60.0;
    }
}

static void snapshot (juce::Component& c, const juce::File& f, float scale)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto cwd = juce::File::getCurrentWorkingDirectory();
    auto samples = cwd.getChildFile ("test_samples");
    auto out = cwd.getChildFile ("test_output");
    out.createDirectory();

    juce::Array<juce::File> files;
    for (auto& f : samples.findChildFiles (juce::File::findFiles, false, "*.wav"))
        files.add (f);
    files.sort();
    if (argc > 1)   // custom sample folder
    {
        files.clear();
        for (auto& f : juce::File (argv[1]).findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac"))
            files.add (f);
        files.sort();
    }

    const auto tourSetting = readSetting ("tourDone", false);   // restored at the end
    writeSetting ("tourDone", true);   // the first-run tour is tested on its own below

    FakeHost host;
    auto proc = std::make_unique<SliceTribeProcessor>();
    proc->setPlayHead (&host);
    proc->prepareToPlay (48000.0, 512);
    runHost (*proc, host, 0.05, nullptr);   // lets the processor see the host tempo

    // empty-state screenshot
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        snapshot (*ed, out.getChildFile ("screenshot_empty.png"), 1.0f);
    }

    for (int i = 0; i < juce::jmin (kNumSlots, files.size()); ++i)
        proc->loadSlot (i, files[i]);

    auto setParam = [&] (const char* id, float value)
    {
        auto* prm = proc->apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    setParam ("pattern", 2);     // Offbeat
    setParam ("length", 2);      // 4 bars
    setParam ("sliceSize", 1);   // 1/16
    setParam ("gate", 85);
    setParam ("chaos", 35);
    setParam ("octave", 10);

    CHECK (waitFor (*proc, 1, 30000), "engine produced a loop");
    auto r = proc->getDisplayResult();
    CHECK (r != nullptr && ! r->segments.empty(), "loop has slices");

    // generate a few times (like pressing the button)
    for (int k = 0; k < 3; ++k)
    {
        const int v = proc->getResultVersion();
        proc->generateNew();
        CHECK (waitFor (*proc, v + 1), "new loop " + juce::String (k + 1));
    }
    CHECK (proc->getHistorySize() == 4, "history has 4 versions");

    // lock two slices, re-roll the third
    proc->toggleLock (0);
    proc->toggleLock (9);
    proc->rerollHit (2);
    waitFor (*proc, proc->getResultVersion() + 1);

    // ---- host playback: 4 bars through processBlock
    host.playing = true;
    host.ppq = 0.0;
    juce::AudioBuffer<float> played;
    runHost (*proc, host, 16 * 60.0 / 140.0, &played);
    engine::writeWav (out.getChildFile ("Plugin_output_host_sync.wav"), played, 48000.0, 1.0f);
    r = proc->getDisplayResult();
    // the output should match the rendered loop sample for sample (gain 0 dB, after the short fade-in)
    double err = 0.0;
    const int n = juce::jmin (played.getNumSamples(), r->audio.getNumSamples());
    for (int i = 2000; i < n; ++i)
        err = juce::jmax (err, (double) std::abs (played.getSample (0, i) - r->audio.getSample (0, i)));
    std::cout << "     max deviation host playback vs render: " << err << "\n";
    CHECK (err < 1.0e-4, "host-synced playback is sample-accurate");

    // screenshot with a playhead in the middle
    host.ppq = 6.2;
    runHost (*proc, host, 0.02, nullptr);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
        snapshot (*ed, out.getChildFile ("screenshot.png"), 1.0f);
        snapshot (*ed, out.getChildFile ("screenshot_2x.png"), 2.0f);
    }

    // style screenshots
    {
        auto* st = proc->apvts.getParameter ("style");
        st->setValueNotifyingHost (st->convertTo0to1 (1.0f));   // Glitch
        waitFor (*proc, proc->getResultVersion() + 1);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
        snapshot (*ed, out.getChildFile ("screenshot_glitch.png"), 1.0f);
        st->setValueNotifyingHost (st->convertTo0to1 (0.0f));   // back to Clean
        waitFor (*proc, proc->getResultVersion() + 1);
    }

    // ---- app icon (Lolly mascot on a candy tile)
    {
        const int N = 1024;
        juce::Image icon (juce::Image::ARGB, N, N, true);
        juce::Graphics g (icon);
        auto r = juce::Rectangle<float> (0, 0, (float) N, (float) N).reduced (N * 0.04f);
        juce::Path tile;
        tile.addRoundedRectangle (r, N * 0.2f);
        const auto& lolly = skinAt (skinLolly);
        g.setGradientFill (juce::ColourGradient (lolly.bg, r.getX(), r.getY(), lolly.bg2, r.getRight(), r.getBottom(), false));
        g.fillPath (tile);   // plain candy gradient: stays clean at 16-32 px
        drawMascot (g, lolly, r.reduced (N * 0.07f), 0.0f, 0.0f);
        juce::FileOutputStream os (out.getChildFile ("icon_render.png"));
        os.setPosition (0); os.truncate();
        juce::PNGImageFormat().writeImageToStream (icon, os);
    }

    // ---- skins: one screenshot per look, plus the preset browser and About screen
    for (int i = 0; i < numSkins; ++i)
    {
        setCurrentSkin (i);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        snapshot (*ed, out.getChildFile ("skin_" + skinAt (i).id + ".png"), 1.0f);
        if (i == skinLolly || i == skinFruity || i == skinSkull)
        {
            auto& view = dynamic_cast<SliceTribeEditor&> (*ed).getView();
            view.openPresetBrowser();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            snapshot (*ed, out.getChildFile ("presets_" + skinAt (i).id + ".png"), 1.0f);
            view.closeOverlays();
            view.openAbout();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            snapshot (*ed, out.getChildFile ("about_" + skinAt (i).id + ".png"), 1.0f);
            view.closeOverlays();
        }
    }
    {
        setCurrentSkin (skinLolly);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        snapshot (*ed, out.getChildFile ("screenshot.png"), 1.0f);
        snapshot (*ed, out.getChildFile ("screenshot_2x.png"), 2.0f);
    }
    CHECK (currentSkinIndex() == skinLolly, "skin choice is remembered");

    // ---- presets
    {
        CHECK (factoryPresets().size() == 101, "101 factory presets (Init + 100)");
        juce::StringArray names;
        for (auto& p : factoryPresets()) names.addIfNotAlreadyThere (p.name);
        CHECK (names.size() == 101, "preset names are unique");
        bool allOk = true;
        for (int i = 0; i < (int) factoryPresets().size(); ++i)
        {
            proc->presets.loadPreset (i);
            const auto& p = factoryPresets()[(size_t) i];
            for (auto& [id, v] : p.values)
                if (std::abs (proc->apvts.getRawParameterValue (id)->load() - v) > 0.051f)
                {
                    allOk = false;
                    std::cout << "     preset " << p.name << " param " << id << " = " << proc->apvts.getRawParameterValue (id)->load() << " expected " << v << "\n";
                }
            if (proc->presets.isModified()) allOk = false;
        }
        CHECK (allOk, "every factory preset loads exactly (and is not 'modified')");
        proc->presets.loadPreset (7);
        proc->apvts.getParameter ("chaos")->setValueNotifyingHost (0.99f);
        CHECK (proc->presets.isModified(), "changing a knob marks the preset as modified");
        proc->setCurrentProgram (12);
        CHECK (proc->getCurrentProgram() == 12 && proc->presets.getCurrentName() == factoryPresets()[12].name, "host program change loads a preset");
        CHECK (proc->getProgramName (3) == factoryPresets()[3].name, "host sees preset names");

        juce::String err;
        const int before = proc->presets.getNumPresets();
        CHECK (proc->presets.saveUserPreset ("UITest Preset", err), "save a user preset");
        CHECK (proc->presets.getNumPresets() == before + 1 && proc->presets.getCurrentName() == "UITest Preset", "user preset appears in the list");
        juce::MemoryBlock pst;
        proc->getStateInformation (pst);
        auto px = std::make_unique<SliceTribeProcessor>();
        px->setStateInformation (pst.getData(), (int) pst.getSize());
        CHECK (px->presets.getCurrentName() == "UITest Preset" && ! px->presets.isModified(), "preset name is stored with the project");
        const auto userFile = proc->presets.getPreset (proc->presets.getCurrentIndex()).file;
        CHECK (proc->presets.deleteUserPreset (userFile) && proc->presets.getNumPresets() == before, "delete the user preset");
        CHECK (PresetManager::fileForName ("???").getFileName().startsWith ("Preset"), "odd preset names never become hidden files");
        proc->presets.loadPreset (0);
    }

    // ---- MIDI: note triggers, CC learn, New Loop automation
    {
        auto sendMidi = [&] (const juce::MidiMessage& m)
        {
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (m, 0);
            proc->processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        };
        const int h0 = proc->getHistorySize(), g0 = proc->getGenerateCount();
        sendMidi (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100));
        CHECK (proc->getGenerateCount() == g0 + 1 && proc->getHistorySize() == h0 + 1, "MIDI note C1 makes a new loop");
        sendMidi (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100));
        CHECK ((int) proc->apvts.getRawParameterValue ("pattern")->load() == 2, "MIDI note D2 selects the Offbeat rhythm");
        sendMidi (juce::MidiMessage::noteOn (1, 63, (juce::uint8) 100));
        CHECK ((int) proc->apvts.getRawParameterValue ("length")->load() == 3, "MIDI note D#3 selects 8 bars");
        sendMidi (juce::MidiMessage::noteOn (1, 37, (juce::uint8) 100));
        CHECK (proc->getHistoryPosition() == proc->getHistorySize() - 2, "MIDI note C#1 goes back one version");

        proc->startMidiLearn ("chaos");
        CHECK (proc->getLearningParamId() == "chaos", "MIDI learn armed");
        sendMidi (juce::MidiMessage::controllerEvent (1, 74, 127));
        CHECK (proc->getMidiCcFor ("chaos") == 74 && proc->getLearningParamId().isEmpty(), "CC 74 learned for Chaos");
        sendMidi (juce::MidiMessage::controllerEvent (1, 74, 0));
        CHECK (proc->apvts.getRawParameterValue ("chaos")->load() < 0.5f, "CC 74 moves Chaos");
        sendMidi (juce::MidiMessage::controllerEvent (1, 74, 127));
        CHECK (proc->apvts.getRawParameterValue ("chaos")->load() > 99.0f, "CC 74 full = 100%");

        juce::MemoryBlock mst;
        proc->getStateInformation (mst);
        auto pm = std::make_unique<SliceTribeProcessor>();
        pm->setStateInformation (mst.getData(), (int) mst.getSize());
        CHECK (pm->getMidiCcFor ("chaos") == 74, "MIDI mapping is stored with the project");
        proc->clearMidiMapping ("chaos");
        CHECK (proc->getMidiCcFor ("chaos") == -1, "forget a MIDI mapping");

        const int g1 = proc->getGenerateCount();
        auto* trig = proc->apvts.getParameter ("trigger");
        trig->setValueNotifyingHost (1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        trig->setValueNotifyingHost (1.0f);   // still on: no second loop
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        trig->setValueNotifyingHost (0.0f);
        trig->setValueNotifyingHost (1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        trig->setValueNotifyingHost (0.0f);
        CHECK (proc->getGenerateCount() == g1 + 2, "automating 'New Loop' makes a new loop on every rising edge");

        // the skin's crazy button (also on MIDI note E1)
        for (int sk = 0; sk < numSkins; ++sk)
        {
            const int gc = proc->getGenerateCount();
            proc->crazyLoop (sk);
            bool inRange = true;
            for (auto* prm : proc->getParameters())
                inRange &= prm->getValue() >= 0.0f && prm->getValue() <= 1.0f;
            CHECK (proc->getGenerateCount() == gc + 1 && inRange, "crazy button for skin " + skinAt (sk).crazyName);
        }
        {
            const int gc = proc->getGenerateCount();
            sendMidi (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100));
            CHECK (proc->getGenerateCount() == gc + 1, "MIDI note E1 = crazy button");
        }
        sendMidi (juce::MidiMessage::programChange (1, 20));
        CHECK (proc->presets.getCurrentName() == factoryPresets()[20].name, "MIDI program change loads a preset");
        proc->presets.loadPreset (0);
        setParam ("pattern", 2); setParam ("length", 2); setParam ("sliceSize", 1); setParam ("gate", 85);
        setParam ("chaos", 35); setParam ("octave", 10);
        waitFor (*proc, proc->getResultVersion() + 1);
        r = proc->getDisplayResult();
    }

    // ---- save / restore (project reopen)
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    std::cout << "     state size: " << state.getSize() / 1024 << " kB (samples embedded)\n";
    {
        FakeHost host2;
        host2.bpm = 140.0;
        auto p2 = std::make_unique<SliceTribeProcessor>();
        p2->setPlayHead (&host2);
        p2->prepareToPlay (48000.0, 512);
        runHost (*p2, host2, 0.05, nullptr);
        p2->setStateInformation (state.getData(), (int) state.getSize());
        CHECK (waitFor (*p2, 1, 30000), "restored project renders");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
        waitFor (*p2, p2->getResultVersion());
        auto r2 = p2->getDisplayResult();
        double diff = 0.0;
        if (r2 != nullptr && r2->audio.getNumSamples() == r->audio.getNumSamples())
            for (int i = 0; i < r->audio.getNumSamples(); ++i)
                diff = juce::jmax (diff, (double) std::abs (r2->audio.getSample (0, i) - r->audio.getSample (0, i)));
        else
            diff = 1.0;
        std::cout << "     restore difference: " << diff << "\n";
        CHECK (diff < 1.0e-3, "reopened project gives the identical loop (incl. locked slice)");

        // missing files → embedded audio is used
        CHECK (p2->getSlotInfo (0).loaded, "slot 1 restored");
    }

    // ---- host A/B compare: restoring the same state keeps the loaded slots (no reload, no silence)
    {
        juce::MemoryBlock st;
        proc->getStateInformation (st);
        const int before = proc->getSlotsVersion();
        proc->setStateInformation (st.getData(), (int) st.getSize());
        bool allLoaded = true;
        for (int i = 0; i < juce::jmin (kNumSlots, files.size()); ++i)
            allLoaded &= proc->getSlotInfo (i).loaded && ! proc->getSlotInfo (i).loading;
        CHECK (allLoaded && proc->getSlotsVersion() > before, "restoring the same state keeps every slot loaded (A/B compare)");
    }

    // ---- tempo change: host goes to 150 bpm → samples are re-stretched
    {
        const int v = proc->getResultVersion();
        host.bpm = 150.0;
        runHost (*proc, host, 0.05, nullptr);
        CHECK (waitFor (*proc, v + 1, 30000), "re-render after tempo change");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        waitFor (*proc, proc->getResultVersion());
        auto r3 = proc->getDisplayResult();
        const int expected = (int) std::llround (16 * 48000.0 * 60.0 / 150.0);
        CHECK (r3 != nullptr && r3->audio.getNumSamples() == expected, "loop length follows new tempo");
    }

    // ---- export
    auto exported = proc->exportLoop (out);
    CHECK (exported.existsAsFile() && exported.getSize() > 10000, "WAV export");

    // ---- new round: mutate, scenes, undo of settings, fills, instrument modes, MIDI/kit export, FX
    host.playing = false;
    {
        auto pump = [&] (int ms = 150) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
        auto block = [&] (const juce::MidiBuffer& m, int numSamples = 512)
        {
            juce::AudioBuffer<float> buf (2, numSamples);
            buf.clear();
            juce::MidiBuffer copy (m);
            proc->processBlock (buf, copy);
            return buf;
        };
        auto note = [&] (int n, bool on)
        {
            juce::MidiBuffer m;
            m.addEvent (on ? juce::MidiMessage::noteOn (1, n, (juce::uint8) 127) : juce::MidiMessage::noteOff (1, n), 0);   // full velocity = 0 dB
            return m;
        };
        auto same = [] (const RenderResult* a, const RenderResult* b)
        {
            if (a == nullptr || b == nullptr || a->audio.getNumSamples() != b->audio.getNumSamples()) return false;
            for (int i = 0; i < a->audio.getNumSamples(); i += 7)
                if (std::abs (a->audio.getSample (0, i) - b->audio.getSample (0, i)) > 1.0e-4f) return false;
            return true;
        };
        waitFor (*proc, proc->getResultVersion());

        // mutate: a variation, one more version in the history
        {
            auto before = proc->getDisplayResult();
            const int h = proc->getHistorySize(), v = proc->getResultVersion();
            proc->mutate();
            CHECK (waitFor (*proc, v + 1), "mutate renders");
            auto after = proc->getDisplayResult();
            int changed = 0;
            const size_t n = juce::jmin (before->segments.size(), after->segments.size());
            for (size_t i = 0; i < n; ++i)
                changed += before->segments[i].srcStart != after->segments[i].srcStart || before->segments[i].slot != after->segments[i].slot ? 1 : 0;
            std::cout << "     mutate changed " << changed << " of " << n << " slices\n";
            CHECK (proc->getHistorySize() == h + 1 && changed > 0 && changed < (int) n, "mutate changes some slices, not all");
            block (note (41, true));
            pump();
            CHECK (proc->getHistorySize() == h + 2, "MIDI note F1 = mutate");
            waitFor (*proc, proc->getResultVersion());
        }

        // undo also brings the settings back (after the crazy button)
        {
            const float pat = proc->apvts.getRawParameterValue ("pattern")->load();
            const float len = proc->apvts.getRawParameterValue ("length")->load();
            auto before = proc->getDisplayResult();
            proc->crazyLoop (skinButcher);
            waitFor (*proc, proc->getResultVersion() + 1);
            proc->historyBack();
            pump();
            waitFor (*proc, proc->getResultVersion());
            CHECK (proc->apvts.getRawParameterValue ("pattern")->load() == pat && proc->apvts.getRawParameterValue ("length")->load() == len,
                   "previous version restores the settings after THE BUTCHER CUT");
            CHECK (same (before.get(), proc->getDisplayResult().get()), "previous version restores the exact loop");
            proc->historyForward();
            pump();
            waitFor (*proc, proc->getResultVersion());
            proc->historyBack();
            pump();
            waitFor (*proc, proc->getResultVersion());
        }

        // scenes
        {
            auto sceneA = proc->getDisplayResult();
            const float len = proc->apvts.getRawParameterValue ("length")->load();
            proc->storeScene (0);
            CHECK (proc->isSceneUsed (0) && ! proc->isSceneUsed (1) && proc->getActiveScene() == 0, "store scene A");
            setParam ("length", 1);
            proc->generateNew();
            waitFor (*proc, proc->getResultVersion() + 1);
            proc->storeScene (1);
            proc->recallScene (0);
            pump();
            waitFor (*proc, proc->getResultVersion());
            CHECK (proc->apvts.getRawParameterValue ("length")->load() == len && same (sceneA.get(), proc->getDisplayResult().get()),
                   "recall scene A: same settings, same loop");
            block (note (73, true));   // C#4 = scene B
            pump();
            waitFor (*proc, proc->getResultVersion());
            CHECK (proc->getActiveScene() == 1 && (int) proc->apvts.getRawParameterValue ("length")->load() == 1, "MIDI note C#4 recalls scene B");

            juce::MemoryBlock st;
            proc->getStateInformation (st);
            auto ps = std::make_unique<SliceTribeProcessor>();
            ps->setStateInformation (st.getData(), (int) st.getSize());
            CHECK (ps->isSceneUsed (0) && ps->isSceneUsed (1) && ! ps->isSceneUsed (2) && ps->getActiveScene() == 1, "scenes are stored with the project");
            proc->clearScene (1);
            CHECK (! proc->isSceneUsed (1), "clear a scene");
            proc->recallScene (0);
            pump();
            waitFor (*proc, proc->getResultVersion());
        }

        // fills: the end of every 4-bar phrase becomes a roll
        {
            setParam ("length", 3);   // 8 bars
            waitFor (*proc, proc->getResultVersion() + 1);
            auto noFill = proc->getDisplayResult();
            setParam ("fill", 1);     // every 4 bars
            waitFor (*proc, proc->getResultVersion() + 1);
            auto withFill = proc->getDisplayResult();
            const double bar = withFill->samplesPerBeat() * 4.0;
            int inFill = 0, glitchInFill = 0;
            for (auto& sg : withFill->segments)
            {
                const double pos = std::fmod ((double) sg.start, bar * 4.0);
                if (pos >= bar * 3.5 - 1.0) { ++inFill; glitchInFill += sg.glitch ? 1 : 0; }
            }
            std::cout << "     fill: " << glitchInFill << " of " << inFill << " slices in the fills are rolls\n";
            int glitchBefore = 0;
            for (auto& sg : noFill->segments)
                glitchBefore += sg.glitch ? 1 : 0;
            CHECK (inFill > 0 && glitchInFill == inFill && glitchBefore == 0, "fill every 4 bars = rolls at the phrase ends");
            setParam ("fill", 0);
            setParam ("length", 2);
            waitFor (*proc, proc->getResultVersion() + 1);
        }

        r = proc->getDisplayResult();
        const float peakLoop = r->audio.getMagnitude (0, r->audio.getNumSamples());

        // Slices mode: every note plays one slice, C1 the whole loop
        {
            setParam ("midiMode", 1);
            block ({});   // mode switch
            CHECK (block ({}).getMagnitude (0, 512) < 1.0e-6f, "Slices mode: silent without notes");
            const auto& seg = r->segments[(size_t) r->noteSegment[3]];   // the 4th different slice
            const int sn = juce::jlimit (512, 8192, (int) seg.length - 400);
            auto first = block (note (37 + 3, true), sn);
            double maxErr = 0.0;
            for (int i = 200; i < sn; ++i)
                maxErr = juce::jmax (maxErr, (double) std::abs (first.getSample (0, i) - r->audio.getSample (0, (int) seg.start + i)));
            std::cout << "     slice 4 on E1: max deviation " << maxErr << ", level " << first.getMagnitude (0, sn) << ", slice length " << seg.length << "\n";
            CHECK (first.getMagnitude (0, sn) > 0.01f && maxErr < 2.0e-3, "Slices mode: E1 (note 40) plays the 4th different slice exactly");
            block (note (40, false));
            float tail = 0.0f;
            for (int k = 0; k < 4; ++k) tail = block ({}).getMagnitude (0, 512);
            CHECK (tail < 1.0e-5f, "Slices mode: note off fades out");

            const int wn = juce::jmin (24000, r->audio.getNumSamples());   // the loop may start with a rest
            auto whole = block (note (36, true), wn);
            double errWhole = 0.0;
            for (int i = 200; i < wn; ++i)
                errWhole = juce::jmax (errWhole, (double) std::abs (whole.getSample (0, i) - r->audio.getSample (0, i)));
            std::cout << "     C1: level " << whole.getMagnitude (0, wn) << ", deviation " << errWhole << "\n";
            CHECK (whole.getMagnitude (0, wn) > 0.01f && errWhole < 2.0e-3, "Slices mode: C1 plays the whole loop from the start");
            block (note (36, false));
            for (int k = 0; k < 4; ++k) block ({});

            // the MIDI file plays the loop back
            auto mid = proc->exportMidi (out.getChildFile ("MIDI"));
            juce::MidiFile mf;
            juce::FileInputStream in (mid);
            const bool read = in.openedOk() && mf.readFrom (in);
            int notes = 0;
            std::set<int> keys;
            if (read && mf.getNumTracks() > 0)
                for (auto* e : *mf.getTrack (0))
                    if (e->message.isNoteOn()) { ++notes; keys.insert (e->message.getNoteNumber()); }
            std::cout << "     MIDI export: " << mid.getFileName() << ", " << notes << " notes (" << keys.size() << " keys) for "
                      << r->segments.size() << " slices (" << r->numDifferentSlices << " different)\n";
            CHECK (read && notes == (int) r->segments.size() && (int) keys.size() == r->numDifferentSlices && mf.getTimeFormat() == 960,
                   "MIDI export: one note per slice, one key per different slice, 960 PPQ");
        }

        // Keys mode: the loop follows the key
        {
            setParam ("midiMode", 2);
            block ({});
            block (note (67, true), 512);
            float level = 0.0f;
            for (int k = 0; k < 60; ++k) level = juce::jmax (level, block ({}).getMagnitude (0, 512));
            std::cout << "     Keys mode G3: level " << level << " (loop peak " << peakLoop << ")\n";
            CHECK (level > 0.01f && level < peakLoop * 3.0f, "Keys mode: G3 plays the loop (+7)");
            block (note (67, false));
            float tail = 1.0f;
            for (int k = 0; k < 8; ++k) tail = block ({}).getMagnitude (0, 512);
            CHECK (tail < 1.0e-4f, "Keys mode: note off fades out");
            setParam ("midiMode", 0);
            block ({});
            const int g = proc->getGenerateCount();
            block (note (36, true));
            pump();
            CHECK (proc->getGenerateCount() == g + 1, "back in Control mode: C1 makes a new loop again");
            waitFor (*proc, proc->getResultVersion());
            r = proc->getDisplayResult();
        }

        // slice kit
        {
            auto kits = out.getChildFile ("Kits");
            kits.deleteRecursively();
            auto kit = proc->exportSliceKit (kits);
            const int wavs = kit.getNumberOfChildFiles (juce::File::findFiles, "*.wav");
            const int mids = kit.getNumberOfChildFiles (juce::File::findFiles, "*.mid");
            std::cout << "     kit: " << kit.getFileName() << " - " << wavs << " WAVs, " << mids << " MIDI\n";
            CHECK (kit.isDirectory() && wavs == r->numDifferentSlices + 1 && mids == 1, "slice kit: the loop + every different slice as WAV + the MIDI");
        }

        // FX rack: neutral = untouched, otherwise on the output and in exports
        {
            FxChain::Params neutral;
            CHECK (neutral.isNeutral() && proc->readFx().isNeutral(), "FX start neutral");
            auto plain = proc->exportLoopTo (out.getChildFile ("fx_off.wav"));
            setParam ("fxCutoff", 35); setParam ("fxEnv", 60); setParam ("fxPump", 70); setParam ("fxDrive", 30);
            auto wet = proc->exportLoopTo (out.getChildFile ("fx_on.wav"));
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> ra (fm.createReaderFor (plain)), rb (fm.createReaderFor (wet));
            float diff = 0.0f; bool finite = true;
            if (ra && rb && ra->lengthInSamples == rb->lengthInSamples)
            {
                juce::AudioBuffer<float> a (2, (int) ra->lengthInSamples), b (2, (int) rb->lengthInSamples);
                ra->read (&a, 0, a.getNumSamples(), 0, true, true);
                rb->read (&b, 0, b.getNumSamples(), 0, true, true);
                for (int i = 0; i < a.getNumSamples(); ++i)
                {
                    diff = juce::jmax (diff, std::abs (a.getSample (0, i) - b.getSample (0, i)));
                    finite &= std::isfinite (b.getSample (0, i));
                }
            }
            CHECK (diff > 0.01f && finite, "FX are in the exported WAV");

            host.playing = true; host.ppq = 0.0;
            juce::AudioBuffer<float> fxPlayed;
            runHost (*proc, host, 1.0, &fxPlayed);
            host.playing = false;
            double dev = 0.0;
            for (int i = 4000; i < fxPlayed.getNumSamples(); ++i)
                dev = juce::jmax (dev, (double) std::abs (fxPlayed.getSample (0, i) - r->audio.getSample (0, i % r->audio.getNumSamples())));
            CHECK (dev > 0.01 && fxPlayed.getMagnitude (0, fxPlayed.getNumSamples()) < 4.0f, "FX on the playback");

            for (auto* id : { "fxCutoff", "fxEnv", "fxPump", "fxDrive" })
                if (auto* prm = proc->apvts.getParameter (id)) prm->setValueNotifyingHost (prm->getDefaultValue());
            CHECK (proc->readFx().isNeutral(), "FX back to neutral");
        }

        // audio tempo detection end-to-end: a file without a tempo in the name
        {
            // screenshots of the new parts
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
            auto& view = dynamic_cast<SliceTribeEditor&> (*ed).getView();
            pump (300);
            view.showFxTab (true);
            pump (200);
            snapshot (*ed, out.getChildFile ("screenshot_fx.png"), 1.0f);
            view.showFxTab (false);
            view.startTour();
            pump (200);
            CHECK (view.getTour().isVisible(), "tour opens");
            snapshot (*ed, out.getChildFile ("tour_1.png"), 1.0f);
            view.getTour().goTo (4);
            pump (200);
            snapshot (*ed, out.getChildFile ("tour_5.png"), 1.0f);
            view.getTour().goTo (5);
            pump (200);
            snapshot (*ed, out.getChildFile ("tour_6.png"), 1.0f);
            CHECK (view.getTour().keyPressed (juce::KeyPress (juce::KeyPress::leftKey)) && view.getTour().getStep() == 4, "tour: left arrow goes back");
            CHECK (! view.getTour().keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)), "tour lets other keys through to the host");
            view.getTour().keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            CHECK (! view.getTour().isVisible(), "tour: Esc closes it");
        }
        {
            // first run: the tour opens by itself once
            writeSetting ("tourDone", false);
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
            auto& view = dynamic_cast<SliceTribeEditor&> (*ed).getView();
            pump (300);
            CHECK (view.getTour().isVisible() && (bool) readSetting ("tourDone", false), "first run: the tour opens once and is remembered");
            std::unique_ptr<juce::AudioProcessorEditor> ed2 (proc->createEditor());
            pump (300);
            CHECK (! dynamic_cast<SliceTribeEditor&> (*ed2).getView().getTour().isVisible(), "second editor: no tour");
        }
        {
            // a long loop with many different slices: the MIDI keyboard runs out at G9
            setParam ("length", 5); setParam ("motif", 0); setParam ("chaos", 100);
            waitFor (*proc, proc->getResultVersion() + 1, 30000);
            auto big = proc->getDisplayResult();
            int maxKey = 0, mapped = 0;
            for (auto k : big->segmentNote) { maxKey = juce::jmax (maxKey, k); mapped += k >= 0 ? 1 : 0; }
            std::cout << "     32 bars: " << big->segments.size() << " slices, " << big->numDifferentSlices << " different, highest key index " << maxKey << "\n";
            CHECK ((int) big->noteSegment.size() == juce::jmin (big->numDifferentSlices, RenderResult::maxSliceNotes) && maxKey < RenderResult::maxSliceNotes,
                   "more slices than keys: notes stop at G9, nothing above 127");
            setParam ("length", 2); setParam ("motif", 2); setParam ("chaos", 35);
            waitFor (*proc, proc->getResultVersion() + 1, 30000);
        }
    }

    // ---- regression: clear right after load must stay empty
    {
        FakeHost h;
        auto p3 = std::make_unique<SliceTribeProcessor>();
        p3->setPlayHead (&h);
        p3->prepareToPlay (48000.0, 512);
        runHost (*p3, h, 0.05, nullptr);
        p3->loadSlot (0, files[0]);
        p3->clearSlot (0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1500);
        CHECK (! p3->getSlotInfo (0).loaded, "cleared slot does not come back");
    }

    // ---- regression: detected tempo is stored with the project
    {
        // 6-second loop: at host 85 it is detected as 2 bars @ 80 bpm; at 140 it would be guessed as 160
        auto f = out.getChildFile ("loop6s.wav");
        juce::AudioBuffer<float> b (2, 48000 * 6);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (c, i, 0.3f * (float) std::sin (i * 0.01) * (float) ((i / 6000) % 2));
        engine::writeWav (f, b, 48000.0, 1.0f);

        FakeHost h; h.bpm = 85.0;
        auto p4 = std::make_unique<SliceTribeProcessor>();
        p4->setPlayHead (&h);
        p4->prepareToPlay (48000.0, 512);
        runHost (*p4, h, 0.05, nullptr);
        p4->loadSlot (0, f);
        waitFor (*p4, 1, 20000);
        const double before = p4->getSlotInfo (0).detectedBpm;
        juce::MemoryBlock st;
        p4->getStateInformation (st);

        FakeHost h2; h2.bpm = 140.0;
        auto p5 = std::make_unique<SliceTribeProcessor>();
        p5->setPlayHead (&h2);
        p5->prepareToPlay (48000.0, 512);
        runHost (*p5, h2, 0.05, nullptr);
        p5->setStateInformation (st.getData(), (int) st.getSize());
        waitFor (*p5, 1, 20000);
        const double after = p5->getSlotInfo (0).detectedBpm;
        std::cout << "     detected bpm before " << before << " after restore " << after << "\n";
        CHECK (std::abs (before - 80.0) < 0.01 && std::abs (after - before) < 0.01, "detected tempo survives project reopen");
    }

    // ---- regression: saving while slots are still loading keeps the embedded audio
    {
        auto p6 = std::make_unique<SliceTribeProcessor>();   // never prepared (inactive track)
        p6->setStateInformation (state.getData(), (int) state.getSize());
        juce::MemoryBlock resaved;
        p6->getStateInformation (resaved);
        std::cout << "     re-saved state " << resaved.getSize() / 1024 << " kB (original " << state.getSize() / 1024 << " kB)\n";
        CHECK (resaved.getSize() > state.getSize() / 2, "embedded samples survive a save before audio starts");
    }

    writeSetting ("tourDone", tourSetting);
    proc->releaseResources();
    proc.reset();
    std::cout << (failures == 0 ? "\nALL UI/PLUGIN TESTS PASSED\n" : "\nFAILURES\n");
    return failures == 0 ? 0 : 1;
}
