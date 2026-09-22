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
        p.setTimeInSeconds (bpm > 0.0 ? ppq * 60.0 / bpm : 0.0);
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

/** The first button with this text anywhere below c - how the tests press a button the user can see. */
static juce::Button* findButton (juce::Component& c, const juce::String& text)
{
    for (auto* child : c.getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*> (child))
            if (b->getButtonText() == text && b->isVisible())
                return b;
        if (auto* found = findButton (*child, text))
            return found;
    }
    return nullptr;
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
        setParam ("fxDrive", 60);   // the finishing layer stays while you browse presets
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
        setParam ("fxDrive", 60);
        proc->presets.loadPreset (7);   // a normal factory preset
        CHECK (proc->apvts.getRawParameterValue ("fxDrive")->load() > 59.0f && ! proc->presets.isModified(),
               "a factory preset keeps your FX and is not marked as modified");
        proc->presets.loadPreset (0);   // Init
        CHECK (proc->apvts.getRawParameterValue ("fxDrive")->load() < 1.0f, "Init resets the FX");
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

        // share per sample, energy, time feel, partial rerolls, KEEP, AUTO PICK and stems
        {
            auto segmentsOfSlot = [] (const RenderResult& r, int slot)
            {
                int n = 0;
                for (const auto& sg : r.segments) n += sg.slot == slot ? 1 : 0;
                return n;
            };
            // share: 0% means no slices from that sample at all
            auto before = proc->getDisplayResult();
            int busiest = 0, had = 0;
            for (int i = 0; i < kNumSlots; ++i)
                if (const int n = segmentsOfSlot (*before, i); n > had) { had = n; busiest = i; }
            proc->setSlotWeight (busiest, 0.0f);
            CHECK (waitFor (*proc, proc->getResultVersion() + 1), "re-render after a share change");
            auto zero = proc->getDisplayResult();
            std::cout << "     slot " << (busiest + 1) << " slices: " << had << " at 100%, " << segmentsOfSlot (*zero, busiest) << " at 0%\n";
            CHECK (had > 0 && segmentsOfSlot (*zero, busiest) == 0, "share 0% = no slices from that sample");
            CHECK (std::abs (proc->getSlotInfo (busiest).weight) < 0.001f, "the share is in the slot info");
            {
                juce::MemoryBlock st2;
                proc->getStateInformation (st2);
                auto pw = std::make_unique<SliceTribeProcessor>();
                pw->setStateInformation (st2.getData(), (int) st2.getSize());
                const auto t0 = juce::Time::getMillisecondCounter();
                while (! pw->getSlotInfo (busiest).loaded && juce::Time::getMillisecondCounter() - t0 < 20000)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
                CHECK (pw->getSlotInfo (busiest).weight < 0.001f, "the share is stored with the project");
            }
            proc->setSlotWeight (busiest, 1.0f);
            waitFor (*proc, proc->getResultVersion() + 1);

            // energy: busier towards the end of the loop
            setParam ("energy", 100);
            CHECK (waitFor (*proc, proc->getResultVersion() + 1), "re-render after energy");
            int early = 0, late = 0;
            for (int loop = 0; loop < 6; ++loop)   // one arrangement is a dice roll; six say what energy does
            {
                if (loop > 0)
                {
                    proc->generateNew();
                    waitFor (*proc, proc->getResultVersion() + 1);
                }
                auto en = proc->getDisplayResult();
                const double half = en->audio.getNumSamples() * 0.5;
                for (const auto& sg : en->segments)
                    (sg.start < half ? early : late) += sg.glitch ? 1 : 0;
            }
            std::cout << "     energy: " << early << " rolls in the first halves, " << late << " in the second halves\n";
            CHECK (late > early, "energy makes the second half busier");
            setParam ("energy", 0);
            waitFor (*proc, proc->getResultVersion() + 1);

            // time feel: half time uses the samples at half speed
            auto normal = proc->getDisplayResult();
            setParam ("feel", 1);
            CHECK (waitFor (*proc, proc->getResultVersion() + 1, 30000), "re-render in half time");
            auto halfTime = proc->getDisplayResult();
            CHECK (halfTime->settings.feel == 1 && halfTime->audio.getNumSamples() == normal->audio.getNumSamples()
                   && ! same (halfTime.get(), normal.get()), "half time: same length, different sound");
            setParam ("feel", 0);
            waitFor (*proc, proc->getResultVersion() + 1, 30000);

            // partial rerolls
            setParam ("octave", 50); setParam ("reverse", 40);   // enough character to see it change
            waitFor (*proc, proc->getResultVersion() + 1);
            auto base = proc->getDisplayResult();
            proc->rerollRhythm();
            CHECK (waitFor (*proc, proc->getResultVersion() + 1), "reroll rhythm renders");
            auto rhy = proc->getDisplayResult();
            bool sameSources = rhy->segments.size() == base->segments.size();
            bool otherCharacter = false;
            for (size_t i = 0; i < juce::jmin (rhy->segments.size(), base->segments.size()); ++i)
            {
                // the sample and the spot inside it stay; an octave slice reads from the resampled copy,
                // so only compare srcStart when the octave flag is the same
                sameSources &= rhy->segments[i].slot == base->segments[i].slot
                            && (rhy->segments[i].octave != base->segments[i].octave || rhy->segments[i].srcStart == base->segments[i].srcStart);
                otherCharacter |= rhy->segments[i].reversed != base->segments[i].reversed
                                || rhy->segments[i].octave != base->segments[i].octave
                                || rhy->segments[i].glitch != base->segments[i].glitch
                                || rhy->segments[i].start != base->segments[i].start;
            }
            std::cout << "     RHY: sources kept " << (int) sameSources << ", character changed " << (int) otherCharacter << "\n";
            CHECK (sameSources && otherCharacter, "RHY keeps the sources and changes the rhythm");
            auto base2 = proc->getDisplayResult();
            proc->rerollSources();
            CHECK (waitFor (*proc, proc->getResultVersion() + 1), "reroll sources renders");
            auto src = proc->getDisplayResult();
            bool sameStarts = src->segments.size() == base2->segments.size();
            bool otherSources = false;
            for (size_t i = 0; i < juce::jmin (src->segments.size(), base2->segments.size()); ++i)
            {
                sameStarts &= src->segments[i].start == base2->segments[i].start;
                otherSources |= src->segments[i].srcStart != base2->segments[i].srcStart || src->segments[i].slot != base2->segments[i].slot;
            }
            CHECK (sameStarts && otherSources, "SRC keeps the rhythm and changes the slices");
            {
                // RHY must still do something after SRC (forceOwn used to kill it)
                auto base3 = proc->getDisplayResult();
                proc->rerollRhythm();
                waitFor (*proc, proc->getResultVersion() + 1);
                auto rhy2 = proc->getDisplayResult();
                int changed = 0;
                for (size_t i = 0; i < juce::jmin (rhy2->segments.size(), base3->segments.size()); ++i)
                    changed += rhy2->segments[i].reversed != base3->segments[i].reversed
                            || rhy2->segments[i].octave != base3->segments[i].octave
                            || rhy2->segments[i].glitch != base3->segments[i].glitch ? 1 : 0;
                std::cout << "     RHY after SRC changed " << changed << " slices\n";
                CHECK (changed > 0, "RHY still works after SRC");
            }
            setParam ("octave", 10); setParam ("reverse", 0);
            waitFor (*proc, proc->getResultVersion() + 1);

            // locked slices must survive a new rhythm and a new loop
            {
                proc->toggleLock (1);
                waitFor (*proc, proc->getResultVersion() + 1);
                auto locked = proc->getDisplayResult();
                int lockedSeg = -1;
                for (size_t i = 0; i < locked->segments.size(); ++i)
                    if (locked->segments[i].locked) { lockedSeg = (int) i; break; }
                proc->rerollRhythm();
                waitFor (*proc, proc->getResultVersion() + 1);
                auto after = proc->getDisplayResult();
                bool kept = lockedSeg >= 0 && lockedSeg < (int) after->segments.size();
                if (kept)
                {
                    const auto& a1 = locked->segments[(size_t) lockedSeg];
                    const auto& b1 = after->segments[(size_t) lockedSeg];
                    kept = b1.locked && a1.slot == b1.slot && a1.srcStart == b1.srcStart
                        && a1.reversed == b1.reversed && a1.octave == b1.octave;
                }
                CHECK (kept, "a locked slice stays exactly the same after RHY");
                proc->generateNew();
                waitFor (*proc, proc->getResultVersion() + 1);
                auto after2 = proc->getDisplayResult();
                bool kept2 = lockedSeg >= 0 && lockedSeg < (int) after2->segments.size()
                          && after2->segments[(size_t) lockedSeg].locked
                          && after2->segments[(size_t) lockedSeg].slot == locked->segments[(size_t) lockedSeg].slot
                          && after2->segments[(size_t) lockedSeg].reversed == locked->segments[(size_t) lockedSeg].reversed
                          && after2->segments[(size_t) lockedSeg].octave == locked->segments[(size_t) lockedSeg].octave;
                CHECK (kept2, "a locked slice stays exactly the same after NEW LOOP");
                proc->unlockAll();
                waitFor (*proc, proc->getResultVersion() + 1);
            }

            // FIT TO TRACK
            {
                // a reference with a clear 4-to-the-floor pattern: steps 0, 4, 8 and 12 are busy
                const double rate = 48000.0, refBpm = 140.0;
                juce::AudioBuffer<float> kicks (2, (int) (rate * 4 * 4 * 60.0 / refBpm));
                kicks.clear();
                const double beat = rate * 60.0 / refBpm;
                for (int b = 0; b < 16; ++b)
                    for (int i = 0; i < (int) (rate * 0.12); ++i)
                    {
                        const int pos = (int) (b * beat) + i;
                        if (pos >= kicks.getNumSamples()) break;
                        const double t = i / rate;
                        const float v = (float) (std::sin (juce::MathConstants<double>::twoPi * 55.0 * t) * std::exp (-t * 14.0) * 0.8);
                        kicks.setSample (0, pos, v);
                        kicks.setSample (1, pos, v);
                    }
                auto prof = engine::gridProfile (kicks, rate, refBpm);
                std::cout << "     grid profile: ";
                for (int i = 0; i < 16; ++i) std::cout << juce::String (prof[(size_t) i], 2) << " ";
                std::cout << "\n";
                const float onBeat = juce::jmin (juce::jmin (prof[0], prof[4]), juce::jmin (prof[8], prof[12]));
                float offBeat = 0.0f;
                for (int i = 0; i < 16; ++i) if (i % 4 != 0) offBeat = juce::jmax (offBeat, prof[(size_t) i]);
                CHECK (onBeat > 0.8f && offBeat < 0.4f, "grid profile finds the beats of a 4-to-the-floor track");

                auto refFile = out.getChildFile ("MyTrack_140.wav");
                engine::writeWav (refFile, kicks, rate, 1.0f);
                setParam ("pattern", 4);     // Rolling 16th: hits on every step, so fit has something to skip
                waitFor (*proc, proc->getResultVersion() + 1, 30000);
                auto withoutFit = proc->getDisplayResult();
                const int vNoFit = proc->getResultVersion();
                proc->loadSlot (kTrackSlot, refFile);
                for (int t = 0; t < 100 && proc->getReferenceSlot() < 0; ++t)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                CHECK (waitFor (*proc, vNoFit + 1, 30000), "re-render with your own track loaded");
                CHECK (proc->getReferenceSlot() == kTrackSlot && proc->getSlotInfo (kTrackSlot).reference,
                       "a sample in the track box is your own track");
                auto withFit = proc->getDisplayResult();
                int fromRef = 0;
                for (const auto& sg : withFit->segments) fromRef += sg.slot == kTrackSlot ? 1 : 0;
                CHECK (fromRef == 0, "no slices are taken from your own track");

                // MY TRACK leads, like Splice: its tempo is the loop's tempo, not the DAW's
                std::cout << "     DAW " << proc->getHostBpm() << " bpm, your track 140 bpm -> loop "
                          << proc->getLoopBpm() << " bpm\n";
                CHECK (std::abs (proc->getHostBpm() - 140.0) > 1.0 && std::abs (proc->getLoopBpm() - 140.0) < 0.5,
                       "your own track's tempo becomes the loop's tempo, not the DAW's");
                CHECK (std::abs (withFit->bpm - 140.0) < 0.5, "and the loop is really rendered at that tempo");
                {
                    const int vb = proc->getResultVersion();
                    proc->setSlotBpm (kTrackSlot, 128.0);
                    CHECK (waitFor (*proc, vb + 1, 30000) && std::abs (proc->getDisplayResult()->bpm - 128.0) < 0.5,
                           "move your track's tempo and the loop moves with it");
                    const int vb2 = proc->getResultVersion();
                    proc->setSlotBpm (kTrackSlot, 0.0);
                    waitFor (*proc, vb2 + 1, 30000);
                    CHECK (std::abs (proc->getLoopBpm() - 140.0) < 0.5, "back to automatic: the track's own tempo again");
                }

                auto onBeatShare = [] (const RenderResult& r)
                {
                    const double step = r.samplesPerBeat() / 4.0;
                    int on = 0, total = 0;
                    for (const auto& sg : r.segments)
                    {
                        const int st = ((int) std::llround (sg.start / step)) % 16;
                        ++total;
                        on += st % 4 == 0 ? 1 : 0;
                    }
                    return total > 0 ? (double) on / total : 0.0;
                };
                const double before2 = onBeatShare (*withoutFit), after2 = onBeatShare (*withFit);
                std::cout << "     slices on the beat: " << juce::String (before2 * 100, 0) << "% without fit, "
                          << juce::String (after2 * 100, 0) << "% with fit (" << withoutFit->segments.size()
                          << " vs " << withFit->segments.size() << " slices)\n";
                CHECK (after2 < before2 && withFit->segments.size() < withoutFit->segments.size(),
                       "FIT TO TRACK leaves room where the track is busy");

                // a sample with no rhythm (a pad) gives no fit grid, so the loop stays full
                {
                    juce::AudioBuffer<float> pad (2, (int) (rate * 4 * 4 * 60.0 / refBpm));
                    for (int i = 0; i < pad.getNumSamples(); ++i)
                    {
                        const double t = i / rate;
                        const float v = (float) (0.25 * (std::sin (juce::MathConstants<double>::twoPi * 110.0 * t)
                                                       + std::sin (juce::MathConstants<double>::twoPi * 164.8 * t)));
                        pad.setSample (0, i, v); pad.setSample (1, i, v);
                    }
                    auto padProfile = engine::gridProfile (pad, rate, refBpm);
                    float mx = 0.0f;
                    for (auto v : padProfile) mx = juce::jmax (mx, v);
                    CHECK (mx == 0.0f, "an even pad gives no fit grid (nothing to fit around)");
                    juce::AudioBuffer<float> tooShort (2, (int) (rate * 0.5));
                    tooShort.clear();
                    auto shortProfile = engine::gridProfile (tooShort, rate, refBpm);
                    float mxs = 0.0f;
                    for (auto v : shortProfile) mxs = juce::jmax (mxs, v);
                    CHECK (mxs == 0.0f, "a sample shorter than a bar gives no fit grid");
                }

                // locking a slice must never move another one, also with FIT skipping hits
                {
                    auto sig = [] (const RenderResult& r)
                    {
                        juce::String out;
                        for (const auto& sg : r.segments)
                            out << sg.hitIndex << ":" << sg.slot << ":" << (juce::int64) sg.srcStart << " ";
                        return out;
                    };
                    const auto before6 = sig (*proc->getDisplayResult());
                    const int toLock = proc->getDisplayResult()->segments.empty() ? -1
                                     : proc->getDisplayResult()->segments.front().hitIndex;
                    if (toLock >= 0)
                    {
                        proc->toggleLock (toLock);
                        waitFor (*proc, proc->getResultVersion() + 1, 30000);
                        CHECK (sig (*proc->getDisplayResult()) == before6,
                               "locking a slice keeps the loop exactly as you heard it, also with FIT on");
                        proc->toggleLock (toLock);
                        waitFor (*proc, proc->getResultVersion() + 1, 30000);
                    }
                }

                proc->setSlotWeight (kTrackSlot, 0.0f);   // fit 0% = as if there were no track
                waitFor (*proc, proc->getResultVersion() + 1, 30000);
                CHECK (onBeatShare (*proc->getDisplayResult()) > after2, "FIT at 0% puts the slices back");
                proc->clearSlot (kTrackSlot);
                setParam ("pattern", 2);
                waitFor (*proc, proc->getResultVersion() + 1, 30000);
                CHECK (proc->getReferenceSlot() == -1, "clearing the box ends FIT TO TRACK");
            }

            // the motif repeat must survive SRC
            {
                setParam ("motif", 1);      // 1 bar motif
                setParam ("variation", 0);  // every repeat identical
                waitFor (*proc, proc->getResultVersion() + 1);
                auto motifRepeats = [] (const RenderResult& r)
                {
                    const double bar = r.samplesPerBeat() * 4.0;
                    int same = 0, pairs = 0;
                    for (const auto& a2 : r.segments)
                        if (a2.start < bar)
                            for (const auto& b2 : r.segments)
                                if (std::abs ((double) b2.start - ((double) a2.start + bar)) < 8.0)
                                {
                                    ++pairs;
                                    same += (a2.slot == b2.slot && a2.srcStart == b2.srcStart) ? 1 : 0;
                                }
                    return pairs > 0 ? (double) same / pairs : 0.0;
                };
                const double before3 = motifRepeats (*proc->getDisplayResult());
                proc->rerollSources();
                waitFor (*proc, proc->getResultVersion() + 1);
                const double after3 = motifRepeats (*proc->getDisplayResult());
                std::cout << "     motif repeats: " << juce::String (before3 * 100, 0) << "% before SRC, "
                          << juce::String (after3 * 100, 0) << "% after\n";
                // with more samples loaded than there are slices in one repeat, a couple of slices
                // carry a sample that would otherwise never be heard, so the repeat is not 100%.
                // What matters here is that SRC does not make it any worse.
                CHECK (before3 > 0.7 && after3 >= before3 - 0.01, "SRC keeps the motif repeat");
                setParam ("motif", 2); setParam ("variation", 20);
                waitFor (*proc, proc->getResultVersion() + 1);
            }

            // KEEP
            for (int i = 0; i < SliceTribeProcessor::numScenes; ++i) proc->clearScene (i);
            CHECK (proc->keepToScene() == 0 && proc->isSceneUsed (0), "KEEP puts the loop in scene A");
            CHECK (proc->keepToScene() == 1, "KEEP uses the next free scene");
            for (int i = 2; i < SliceTribeProcessor::numScenes; ++i) proc->keepToScene();
            CHECK (proc->keepToScene() == -1, "KEEP says when all scenes are full");
            for (int i = 0; i < SliceTribeProcessor::numScenes; ++i) proc->clearScene (i);

            // AUTO PICK
            {
                const int g = proc->getGenerateCount(), v = proc->getResultVersion(), jv = proc->getJobVersion();
                proc->autoPick (4);
                const auto t0 = juce::Time::getMillisecondCounter();
                while (proc->getJobVersion() == jv && juce::Time::getMillisecondCounter() - t0 < 30000)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
                waitFor (*proc, proc->getResultVersion());
                std::cout << "     " << proc->getJobMessage() << "\n";
                auto picked = proc->getDisplayResult();
                CHECK (proc->getJobVersion() != jv && proc->getGenerateCount() > g && proc->getResultVersion() > v
                       && picked != nullptr && ! picked->segments.empty(), "AUTO PICK makes a loop");
                CHECK (engine::scoreLoop (*picked) > 0.0, "the chosen loop scores above zero");
                juce::AudioBuffer<float> silence (2, 48000);
                silence.clear();
                RenderResult empty;
                empty.audio = std::move (silence);
                CHECK (engine::scoreLoop (empty) == 0.0, "silence scores zero");
            }

            // stems
            {
                auto stemDir = out.getChildFile ("Stems");
                stemDir.deleteRecursively();
                const int jv = proc->getJobVersion();
                auto folder = proc->exportStems (stemDir);
                const auto t0 = juce::Time::getMillisecondCounter();
                while (proc->getJobVersion() == jv && juce::Time::getMillisecondCounter() - t0 < 30000)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
                const int wavs = folder.getNumberOfChildFiles (juce::File::findFiles, "*.wav");
                std::cout << "     stems: " << wavs << " WAVs in " << folder.getFileName() << "\n";
                CHECK (wavs >= 2, "stems: one WAV per sample");
                auto r2 = proc->getDisplayResult();
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                juce::AudioBuffer<float> sum;
                bool ok = true;
                for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav"))
                {
                    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
                    if (rd == nullptr) { ok = false; break; }
                    juce::AudioBuffer<float> b ((int) rd->numChannels, (int) rd->lengthInSamples);
                    rd->read (&b, 0, b.getNumSamples(), 0, true, true);
                    if (sum.getNumSamples() == 0) { sum.setSize (2, b.getNumSamples()); sum.clear(); }
                    if (b.getNumSamples() != sum.getNumSamples()) { ok = false; break; }
                    for (int c = 0; c < 2; ++c)
                        sum.addFrom (c, 0, b, juce::jmin (c, b.getNumChannels() - 1), 0, b.getNumSamples());
                }
                double dev = 0.0;
                if (ok && r2 != nullptr && sum.getNumSamples() == r2->audio.getNumSamples())
                    for (int i = 0; i < sum.getNumSamples(); i += 5)
                        dev = juce::jmax (dev, (double) std::abs (sum.getSample (0, i) - r2->audio.getSample (0, i)));
                else
                    dev = 1.0;
                std::cout << "     stems summed vs the loop: max deviation " << dev << "\n";
                CHECK (dev < 0.06, "the stems together sound like the loop");
            }
        }

        // listening to one sample on its own
        {
            CHECK (proc->getSlotPreview() == -1, "no sample preview at the start");
            proc->setSlotPreview (1);
            CHECK (proc->getSlotPreview() == 1, "slot 2 preview on");
            float level = 0.0f;
            for (int k = 0; k < 60; ++k) level = juce::jmax (level, block ({}).getMagnitude (0, 512));
            std::cout << "     sample preview level: " << level << "\n";
            CHECK (level > 0.01f, "the sample plays on its own");
            proc->setSlotPreview (1);          // same slot again = stop
            CHECK (proc->getSlotPreview() == -1, "clicking again stops it");
            float tail = 1.0f;
            for (int k = 0; k < 6; ++k) tail = block ({}).getMagnitude (0, 512);
            CHECK (tail < 1.0e-5f, "sample preview fades out");
            proc->setSlotPreview (0);
            proc->clearSlot (7);               // clearing another slot keeps it playing
            CHECK (proc->getSlotPreview() == 0, "preview survives a change to another slot");
            proc->setSlotPreview (-1);
            for (int k = 0; k < 6; ++k) block ({});
        }

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

        // NEW LOOP straight after a crazy button must land on a normal loop again - every skin
        {
            const juce::StringArray watched { "pattern", "style", "amount", "sliceSize", "sliceMode", "sensitivity",
                                              "chaos", "reverse", "octave", "swing", "gate", "fade", "motif", "variation" };
            auto snap = [&]
            {
                std::map<juce::String, float> m;
                for (auto& id : watched) m[id] = proc->apvts.getRawParameterValue (id)->load();
                return m;
            };
            for (int sk = 0; sk < numSkins; ++sk)
            {
                const auto normal = snap();
                proc->crazyLoop (sk);
                pump();
                waitFor (*proc, proc->getResultVersion());
                const auto crazy = snap();
                proc->generateNew();
                pump();
                waitFor (*proc, proc->getResultVersion());
                const auto back = snap();
                juce::String off;
                for (auto& id : watched)
                    if (std::abs (back.at (id) - normal.at (id)) > 1.0e-3f)
                        off += (off.isEmpty() ? "" : ", ") + id + " " + juce::String (normal.at (id), 2)
                             + " -> " + juce::String (back.at (id), 2);
                bool moved = false;
                for (auto& id : watched) moved |= std::abs (crazy.at (id) - normal.at (id)) > 1.0e-3f;
                CHECK (moved && off.isEmpty(), "NEW LOOP after " + skinAt (sk).crazyName + " is normal again"
                                               + (off.isEmpty() ? juce::String() : "  [" + off + "]"));
            }

            // and after hitting the crazy button three times in a row, NEW LOOP still lands on normal
            {
                const auto normal = snap();
                proc->crazyLoop (skinSmile);
                pump(); waitFor (*proc, proc->getResultVersion());
                proc->crazyLoop (skinSkull);
                pump(); waitFor (*proc, proc->getResultVersion());
                proc->crazyLoop (skinSmile);
                pump(); waitFor (*proc, proc->getResultVersion());
                proc->generateNew();
                pump(); waitFor (*proc, proc->getResultVersion());
                const auto back = snap();
                juce::String off;
                for (auto& id : watched)
                    if (std::abs (back.at (id) - normal.at (id)) > 1.0e-3f)
                        off += (off.isEmpty() ? "" : ", ") + id;
                CHECK (off.isEmpty(), "three crazy buttons in a row: NEW LOOP is still normal again"
                                      + (off.isEmpty() ? juce::String() : "  [" + off + "]"));
            }
        }

        // CLEAR ALL puts every knob back to its default too
        {
            setParam ("chaos", 0.9f);
            setParam ("swing", 0.6f);
            proc->crazyLoop (skinSmile);
            pump(); waitFor (*proc, proc->getResultVersion());
            proc->resetSettings();
            pump();
            juce::String off;
            for (auto& id : presetParameterIds())
                if (auto* prm = proc->apvts.getParameter (id))
                    if (std::abs (prm->getValue() - prm->getDefaultValue()) > 1.0e-3f)
                        off += (off.isEmpty() ? "" : ", ") + id;
            CHECK (off.isEmpty(), "CLEAR ALL puts every knob back to normal"
                                  + (off.isEmpty() ? juce::String() : "  [" + off + "]"));
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
            view.getTour().goTo (7);
            pump (200);
            snapshot (*ed, out.getChildFile ("tour_8.png"), 1.0f);
            view.getTour().goTo (1);
            pump (200);
            CHECK (view.getTour().isVisible() && view.getTour().getStep() == 1, "the tour has a step for FIT TO TRACK");
            snapshot (*ed, out.getChildFile ("tour_fit.png"), 1.0f);
            view.getTour().keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            CHECK (! view.getTour().isVisible(), "tour: Esc closes it");

            // FIT TO TRACK from the interface: the box below the result
            {
                const int was = proc->getReferenceSlot();
                auto before5 = proc->getDisplayResult();
                const int segsBefore = before5 != nullptr ? (int) before5->segments.size() : 0;
                const int vBefore = proc->getResultVersion();
                auto myTrackFile = out.getChildFile ("My_Track_Idea_140bpm.wav");
                myTrackFile.deleteFile();
                files[0].copyFileTo (myTrackFile);
                proc->loadSlot (kTrackSlot, myTrackFile);
                for (int t = 0; t < 100 && proc->getReferenceSlot() < 0; ++t)
                    pump (100);
                CHECK (proc->getReferenceSlot() == kTrackSlot && proc->getSlotInfo (kTrackSlot).reference,
                       "a sample in the track box becomes your own track");
                waitFor (*proc, vBefore + 1, 30000);
                pump (300);
                auto fitted = proc->getDisplayResult();
                std::cout << "     slices: " << segsBefore << " before FIT, "
                          << (fitted != nullptr ? (int) fitted->segments.size() : -1) << " with FIT on\n";
                // the track is an offbeat bassline and the rhythm is Offbeat: every hit lands
                // exactly where the track is busy, so without a floor FIT would skip the whole loop
                CHECK (fitted != nullptr && (int) fitted->segments.size() >= 4,
                       "FIT never leaves you with an empty loop: every bar keeps at least one slice");
                snapshot (*ed, out.getChildFile ("screenshot_fit.png"), 1.0f);

                // the two cut lines, on a sample and on the track
                proc->setSlotTrim (0, 0.18f, 0.62f);
                proc->setSlotTrim (kTrackSlot, 0.25f, 0.80f);
                waitFor (*proc, proc->getResultVersion() + 1, 30000);
                pump (400);
                snapshot (*ed, out.getChildFile ("screenshot_trim.png"), 1.0f);
                CHECK (std::abs (proc->getSlotInfo (0).trimStart - 0.18f) < 0.001f, "the cut lines reach the slot");
                proc->setSlotTrim (0, 0.0f, 1.0f);

                proc->clearSlot (kTrackSlot);
                pump (400);
                CHECK (proc->getReferenceSlot() == -1, "emptying the box turns FIT TO TRACK off again");
                CHECK (was == -1, "nothing is your own track until you drop something in the box");
                waitFor (*proc, proc->getResultVersion() + 1, 30000);
            }
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

    // ---- the tempo we start on when no host is telling us (standalone, or a host without a transport)
    {
        auto p7 = std::make_unique<SliceTribeProcessor>();
        p7->prepareToPlay (48000.0, 512);
        std::cout << "     start tempo without a host: " << p7->getHostBpm() << " bpm\n";
        CHECK (std::abs (p7->getHostBpm() - 125.0) < 0.01, "we start on 125 bpm, not on the host-less default of an empty project");
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        b.clear();
        p7->processBlock (b, m);   // no play head at all: the fallback must stay put
        CHECK (std::abs (p7->getHostBpm() - 125.0) < 0.01, "125 bpm survives a block without a play head");
    }

    // ---- key match starts on Off in the standalone, but a DAW project keeps the key you saved
    {
        auto keyOf = [] (SliceTribeProcessor& p) { return (int) p.apvts.getRawParameterValue ("key")->load(); };

        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
        auto p8 = std::make_unique<SliceTribeProcessor>();
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
        CHECK (p8->isStandalone() && keyOf (*p8) == 0, "a brand new plug-in starts with KEY on Off");

        if (auto* prm = p8->apvts.getParameter ("key"))
            prm->setValueNotifyingHost (prm->convertTo0to1 (16.0f));   // some key, any key
        CHECK (keyOf (*p8) == 16, "KEY can be set by hand");
        juce::MemoryBlock st;
        p8->getStateInformation (st);

        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
        auto p9 = std::make_unique<SliceTribeProcessor>();
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
        p9->setStateInformation (st.getData(), (int) st.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        CHECK (keyOf (*p9) == 0, "the standalone app starts on KEY Off, whatever was on screen last time");
        if (auto* prm = p9->apvts.getParameter ("key"))
            prm->setValueNotifyingHost (prm->convertTo0to1 (16.0f));
        juce::MemoryBlock st2;
        p9->getStateInformation (st2);
        p9->setStateInformation (st2.getData(), (int) st2.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        CHECK (keyOf (*p9) == 16, "loading a preset or a song later in the session keeps its key");

        auto p10 = std::make_unique<SliceTribeProcessor>();   // a plug-in in a DAW
        p10->setStateInformation (st.getData(), (int) st.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        CHECK (! p10->isStandalone() && keyOf (*p10) == 16, "a DAW project keeps the key it was saved with");
    }

    // ---- FIT TO TRACK housekeeping: the KEY must not get stuck on a track that is gone
    {
        auto keyOf = [] (SliceTribeProcessor& p) { return (int) p.apvts.getRawParameterValue ("key")->load(); };
        FakeHost h;
        auto p11 = std::make_unique<SliceTribeProcessor>();
        p11->setPlayHead (&h);
        p11->prepareToPlay (48000.0, 512);
        runHost (*p11, h, 0.05, nullptr);
        // a file whose name carries a key, so "the key follows your track" really happens
        auto myTrack = out.getChildFile ("MyTrack_Am_140bpm.wav");
        myTrack.deleteFile();
        files[0].copyFileTo (myTrack);
        p11->loadSlot (0, files[1]);
        waitFor (*p11, 1, 30000);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);

        if (auto* prm = p11->apvts.getParameter ("key"))
            prm->setValueNotifyingHost (prm->convertTo0to1 (16.0f));
        p11->loadSlot (kTrackSlot, myTrack);
        for (int t = 0; t < 100 && p11->getReferenceSlot() < 0; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
        CHECK (p11->getSlotInfo (kTrackSlot).key == 19, "the track's key is read from its name");
        CHECK (keyOf (*p11) == 20, "KEY follows your own track (Am)");
        CHECK (p11->getReferenceSlot() == kTrackSlot, "the track box is the reference");

        p11->clearSlot (kTrackSlot);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
        CHECK (p11->getReferenceSlot() == -1 && keyOf (*p11) == 16,
               "removing your own track puts KEY back, instead of leaving it on that track's key");

        // and the way back to your own KEY survives closing and reopening the project
        p11->loadSlot (kTrackSlot, myTrack);
        for (int t = 0; t < 100 && p11->getReferenceSlot() < 0; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        juce::MemoryBlock st11;
        p11->getStateInformation (st11);
        auto p14 = std::make_unique<SliceTribeProcessor>();
        p14->prepareToPlay (48000.0, 512);
        p14->setStateInformation (st11.getData(), (int) st11.getSize());
        for (int t = 0; t < 100 && p14->getReferenceSlot() < 0; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        CHECK (p14->getReferenceSlot() == kTrackSlot, "your own track comes back with the project");
        p14->clearSlot (kTrackSlot);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
        CHECK (keyOf (*p14) == 16, "after reopening a project, removing the track still puts KEY back");
    }

    // ---- a project from before the track box existed: the marked slot moves into it
    {
        auto p16 = std::make_unique<SliceTribeProcessor>();
        p16->prepareToPlay (48000.0, 512);
        p16->loadSlot (0, files[0]);
        p16->loadSlot (1, files[1]);
        for (int t = 0; t < 100 && ! p16->getSlotInfo (1).loaded; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        juce::MemoryBlock st16;
        p16->getStateInformation (st16);
        // rewrite it the way the old version saved it: slot 1 marked as "my track"
        juce::MemoryInputStream in (st16.getData(), st16.getSize(), false);
        in.readString();
        auto root = juce::ValueTree::readFromStream (in);
        auto slotsTree = root.getChildWithName ("SLOTS");
        for (auto t : slotsTree)
            if ((int) t.getProperty ("index", -1) == 1)
                t.setProperty ("reference", true, nullptr);
        juce::MemoryBlock old;
        {
            juce::MemoryOutputStream os (old, false);
            os.writeString ("CHUPALOOPS1");
            root.writeToStream (os);
        }
        auto p17 = std::make_unique<SliceTribeProcessor>();
        p17->prepareToPlay (48000.0, 512);
        p17->setStateInformation (old.getData(), (int) old.getSize());
        for (int t = 0; t < 100 && p17->getReferenceSlot() < 0; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        CHECK (p17->getReferenceSlot() == kTrackSlot && p17->getSlotInfo (kTrackSlot).loaded,
               "an older project's 'my track' sample lands in the track box");
        CHECK (! p17->getSlotInfo (1).loaded, "and is not left in its old slot as well");
    }

    // ---- a sample dropped in later joins the loop straight away, without pressing NEW LOOP
    {
        auto p24 = std::make_unique<SliceTribeProcessor>();
        p24->prepareToPlay (48000.0, 512);
        p24->loadSlot (0, files[0]);
        for (int t = 0; t < 100 && ! p24->getSlotInfo (0).loaded; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        for (int t = 0; t < 100 && p24->getDisplayResult() == nullptr; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        const int v0 = p24->getResultVersion();
        auto used = [] (const SliceTribeProcessor& p, int slot)
        {
            auto r = p.getDisplayResult();
            if (r == nullptr) return 0;
            int n = 0;
            for (auto& sg : r->segments) n += sg.slot == slot ? 1 : 0;
            return n;
        };
        const int before1 = used (*p24, 1);
        p24->loadSlot (1, files[1]);
        for (int t = 0; t < 120 && (! p24->getSlotInfo (1).loaded || p24->getResultVersion() == v0); ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        const int after1 = used (*p24, 1);
        std::cout << "     slices from the new sample: " << before1 << " -> " << after1 << "\n";
        CHECK (before1 == 0 && after1 > 0, "a sample dropped in later is used right away");

        // and the same for the third one, while the first two keep playing
        const int v1 = p24->getResultVersion();
        p24->loadSlot (2, files[2]);
        for (int t = 0; t < 120 && (! p24->getSlotInfo (2).loaded || p24->getResultVersion() == v1); ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        CHECK (used (*p24, 2) > 0 && used (*p24, 0) > 0 && used (*p24, 1) > 0,
               "and a third one joins them, all three in the same loop");
    }

    // ---- the two cut lines: only that part of the sample is used, and it survives the project
    {
        auto p18 = std::make_unique<SliceTribeProcessor>();
        p18->prepareToPlay (48000.0, 512);
        p18->loadSlot (0, files[0]);
        for (int t = 0; t < 100 && ! p18->getSlotInfo (0).loaded; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        p18->setSlotTrim (0, 0.5f, 0.75f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        CHECK (std::abs (p18->getSlotInfo (0).trimStart - 0.5f) < 0.001f
            && std::abs (p18->getSlotInfo (0).trimEnd - 0.75f) < 0.001f, "the two lines are stored on the slot");

        // and they really steer the renderer: every slice must come from that quarter of the sample
        {
            FakeHost h18;
            auto p20 = std::make_unique<SliceTribeProcessor>();
            p20->setPlayHead (&h18);
            p20->prepareToPlay (48000.0, 512);
            runHost (*p20, h18, 0.05, nullptr);
            p20->loadSlot (0, files[0]);
            waitFor (*p20, 1, 30000);
            auto whole = p20->getDisplayResult();
            juce::int64 lo = 1 << 30, hi = 0;
            for (const auto& sg : whole->segments) { lo = juce::jmin (lo, sg.srcStart); hi = juce::jmax (hi, sg.srcStart); }
            const int v20 = p20->getResultVersion();
            p20->setSlotTrim (0, 0.5f, 0.75f);
            CHECK (waitFor (*p20, v20 + 1, 30000), "the loop is rebuilt when you move the lines");
            auto cutDown = p20->getDisplayResult();
            juce::int64 lo2 = 1 << 30, hi2 = 0;
            for (const auto& sg : cutDown->segments) { lo2 = juce::jmin (lo2, sg.srcStart); hi2 = juce::jmax (hi2, sg.srcStart); }
            const double total = whole->settings.bars > 0 ? 1.0 : 1.0;
            juce::ignoreUnused (total);
            std::cout << "     slice positions: " << lo << ".." << hi << " over the whole sample, "
                      << lo2 << ".." << hi2 << " between the lines\n";
            CHECK (! cutDown->segments.empty() && lo2 > hi / 3 && lo2 > lo,
                   "with the lines set, slices only come from that part of the sample");
        }
        juce::MemoryBlock st18;
        p18->getStateInformation (st18);
        auto p19 = std::make_unique<SliceTribeProcessor>();
        p19->prepareToPlay (48000.0, 512);
        p19->setStateInformation (st18.getData(), (int) st18.getSize());
        for (int t = 0; t < 100 && ! p19->getSlotInfo (0).loaded; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        CHECK (std::abs (p19->getSlotInfo (0).trimStart - 0.5f) < 0.001f
            && std::abs (p19->getSlotInfo (0).trimEnd - 0.75f) < 0.001f, "and they come back with the project");
    }

    // ---- STRAIGHT: a crooked recording is pulled onto the grid, and it survives the project
    {
        const double rate = 48000.0, bpm = 120.0, beatLen = rate * 60.0 / bpm;
        const int beats = 16, len = (int) std::llround (beats * beatLen);
        auto crooked = out.getChildFile ("Wobbly_Disco_120bpm.wav");
        {
            juce::AudioBuffer<float> b (2, len);
            b.clear();
            for (int k = 0; k < beats; ++k)
            {
                const double off = (std::sin (k * 1.9) * 0.016 + (k % 3 == 0 ? 0.008 : -0.006)) * rate * (0.4 + 0.6 * k / beats);
                const int pos = juce::jlimit (0, len - 1, (int) std::llround (k * beatLen + off));
                const int n = (int) (rate * 0.035);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n && pos + i < len; ++i)
                    {
                        const double t = i / rate;
                        b.addSample (ch, pos + i, (float) (std::sin (juce::MathConstants<double>::twoPi * 70.0 * t)
                                                           * std::exp (-t * 45.0) * (k % 4 == 0 ? 0.9 : 0.55)));
                    }
            }
            engine::writeWav (crooked, b, rate, 1.0f);
        }

        FakeHost h23; h23.bpm = 120.0;
        auto p23 = std::make_unique<SliceTribeProcessor>();
        p23->setPlayHead (&h23);
        p23->prepareToPlay (rate, 512);
        runHost (*p23, h23, 0.05, nullptr);
        p23->loadSlot (0, crooked);
        waitFor (*p23, 1, 30000);
        CHECK (std::abs (p23->getSlotInfo (0).detectedBpm - 120.0) < 1.0, "the wobbly recording is heard at 120 bpm");
        CHECK (p23->getSlotInfo (0).warp < 0.001f, "a sample starts as recorded");

        const int v23 = p23->getResultVersion();
        p23->setSlotWarp (0, 1.0f);
        CHECK (waitFor (*p23, v23 + 1, 40000), "the loop is rebuilt when you straighten a sample");
        CHECK (std::abs (p23->getSlotInfo (0).warp - 1.0f) < 0.001f, "STRAIGHT is stored on the slot");

        juce::MemoryBlock st23;
        p23->getStateInformation (st23);
        auto p24 = std::make_unique<SliceTribeProcessor>();
        p24->prepareToPlay (rate, 512);
        p24->setStateInformation (st23.getData(), (int) st23.getSize());
        for (int t = 0; t < 100 && ! p24->getSlotInfo (0).loaded; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        CHECK (std::abs (p24->getSlotInfo (0).warp - 1.0f) < 0.001f, "and it comes back with the project");

        p23->setSlotWarp (0, 0.0f);
        waitFor (*p23, p23->getResultVersion() + 1, 40000);
        CHECK (p23->getSlotInfo (0).warp < 0.001f, "and you can put it back to as recorded");
    }

    // ---- FIT at 0% is a real setting and must survive a save
    {
        auto p21 = std::make_unique<SliceTribeProcessor>();
        p21->prepareToPlay (48000.0, 512);
        p21->loadSlot (kTrackSlot, files[0]);
        for (int t = 0; t < 100 && ! p21->getSlotInfo (kTrackSlot).loaded; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        p21->setSlotWeight (kTrackSlot, 0.0f);
        juce::MemoryBlock st21;
        p21->getStateInformation (st21);
        auto p22 = std::make_unique<SliceTribeProcessor>();
        p22->prepareToPlay (48000.0, 512);
        p22->setStateInformation (st21.getData(), (int) st21.getSize());
        for (int t = 0; t < 100 && ! p22->getSlotInfo (kTrackSlot).loaded; ++t)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        CHECK (p22->getSlotInfo (kTrackSlot).weight < 0.01f, "FIT at 0% comes back as 0%, not as 100%");
    }

    // ---- the tempo you set in the standalone is part of the session
    {
        auto p12 = std::make_unique<SliceTribeProcessor>();
        p12->prepareToPlay (48000.0, 512);
        p12->setFallbackBpm (174.0);
        juce::MemoryBlock st;
        p12->getStateInformation (st);
        auto p13 = std::make_unique<SliceTribeProcessor>();
        p13->prepareToPlay (48000.0, 512);
        p13->setStateInformation (st.getData(), (int) st.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        juce::AudioBuffer<float> b13 (2, 512);
        juce::MidiBuffer m13;
        b13.clear();
        p13->processBlock (b13, m13);
        CHECK (std::abs (p13->getHostBpm() - 174.0) < 0.01, "the tempo you set without a DAW comes back with the project");
    }

    // ---- the FX section, and everything that only does its work inside the plug-in ----------
    {
        auto pf = std::make_unique<SliceTribeProcessor>();
        FakeHost fh;
        fh.bpm = 140.0;
        pf->setPlayHead (&fh);
        pf->prepareToPlay (48000.0, 512);
        for (int i = 0; i < juce::jmin (3, files.size()); ++i)
            pf->loadSlot (i, files[i]);
        CHECK (waitFor (*pf, 1, 30000), "a loop to test the FX on");

        auto set = [&] (const char* id, float value)
        {
            auto* prm = pf->apvts.getParameter (id);
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        };
        auto play = [&] (juce::AudioBuffer<float>& into)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
            fh.playing = true;
            fh.ppq = 0.0;
            runHost (*pf, fh, 4 * 60.0 / 140.0, &into);
            fh.playing = false;
        };
        auto differs = [] (const juce::AudioBuffer<float>& a2, const juce::AudioBuffer<float>& b2)
        {
            if (a2.getNumSamples() != b2.getNumSamples()) return true;
            for (int i = 500; i < a2.getNumSamples(); i += 3)
                for (int c = 0; c < a2.getNumChannels(); ++c)
                    if (std::abs (a2.getSample (c, i) - b2.getSample (c, i)) > 1.0e-4f) return true;
            return false;
        };

        juce::AudioBuffer<float> dry;
        play (dry);

        struct Fx { const char* id; float def; float on; const char* name; };
        const std::vector<Fx> fx {
            { "fxCutoff", 100.0f, 25.0f, "CUTOFF" }, { "fxReso",  0.0f, 80.0f, "RESO" },
            { "fxEnv",    0.0f,   80.0f, "ENV" },    { "fxDecay", 50.0f, 95.0f, "DECAY" },
            { "fxLowCut", 0.0f,   60.0f, "LOW CUT" },{ "fxDrive", 0.0f, 80.0f, "DRIVE" },
            { "fxPump",   0.0f,   80.0f, "PUMP" },
            { "gain",     0.0f,  -9.0f,  "VOLUME" },
        };
        for (const auto& f : fx)
        {
            if (juce::String (f.id) == "fxDecay")   // decay only has a say once Env is open
                set ("fxEnv", 80.0f);
            if (juce::String (f.id) == "fxReso")
                set ("fxCutoff", 40.0f);
            juce::AudioBuffer<float> before2, after2;
            set (f.id, f.def);
            play (before2);
            set (f.id, f.on);
            play (after2);
            CHECK (differs (before2, after2), juce::String (f.name) + " changes the sound");
            set (f.id, f.def);
            set ("fxEnv", 0.0f);
            set ("fxCutoff", 100.0f);
        }

        // WIDTH and KEY need material that has something to work on: a real stereo sample with
        // a key in its name (WIDTH on a mono sample does nothing, and rightly so)
        {
            const double rate = 48000.0;
            const int len = (int) (4 * 4 * 60.0 / 140.0 * rate);
            juce::AudioBuffer<float> st (2, len);
            for (int i = 0; i < len; ++i)
            {
                const double t = i / rate;
                const double env = std::exp (-std::fmod (t, 60.0 / 140.0) * 4.0);
                st.setSample (0, i, (float) (0.35 * std::sin (juce::MathConstants<double>::twoPi * 110.0 * t) * env));
                st.setSample (1, i, (float) (0.35 * std::sin (juce::MathConstants<double>::twoPi * 138.6 * t + 1.1) * env));
            }
            auto stereoFile = out.getChildFile ("StereoTest_Am_140bpm.wav");
            engine::writeWav (stereoFile, st, rate, 1.0f);
            for (int i = 1; i < kNumSlots; ++i)
                pf->clearSlot (i);
            pf->loadSlot (0, stereoFile);
            for (int t = 0; t < 100 && ! pf->getSlotInfo (0).loaded; ++t)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            waitFor (*pf, pf->getResultVersion() + 1, 30000);

            juce::AudioBuffer<float> narrow, wide;
            set ("fxWidth", 100.0f);
            play (narrow);
            set ("fxWidth", 180.0f);
            play (wide);
            CHECK (differs (narrow, wide), "WIDTH changes the sound (on stereo material)");
            set ("fxWidth", 0.0f);
            juce::AudioBuffer<float> mono;
            play (mono);
            bool isMono = true;
            for (int i = 12000; i < mono.getNumSamples() && isMono; i += 3)
                isMono = std::abs (mono.getSample (0, i) - mono.getSample (1, i)) < 1.0e-4f;
            CHECK (isMono, "and WIDTH at 0% really gives mono");
            set ("fxWidth", 100.0f);

            const int v0 = pf->getResultVersion();
            auto off = pf->getDisplayResult()->audio;
            set ("key", 6);                        // force everything into one key
            waitFor (*pf, v0 + 1, 30000);
            CHECK (differs (off, pf->getDisplayResult()->audio), "KEY changes the loop");
            set ("key", 0.0f);
            waitFor (*pf, pf->getResultVersion() + 1, 30000);
        }

        // STRETCH and TIME FEEL are done when a sample is prepared, so they show up in the loop
        auto loopAudio = [&] ()
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
            auto res = pf->getDisplayResult();
            return res != nullptr ? res->audio : juce::AudioBuffer<float>();
        };
        {
            const int v0 = pf->getResultVersion();
            auto beats = loopAudio();
            set ("stretch", 1);                    // Smooth
            waitFor (*pf, v0 + 1, 30000);
            auto smooth = loopAudio();
            CHECK (differs (beats, smooth), "STRETCH Beats and Smooth really sound different");
            set ("stretch", 0);
            waitFor (*pf, pf->getResultVersion() + 1, 30000);
        }
        for (int fe = 1; fe <= 2; ++fe)
        {
            const int v0 = pf->getResultVersion();
            auto normal = loopAudio();
            set ("feel", (float) fe);
            waitFor (*pf, v0 + 1, 30000);
            auto changed = loopAudio();
            CHECK (differs (normal, changed),
                   juce::String ("TIME FEEL ") + (fe == 1 ? "half time" : "double time") + " changes the loop");
            set ("feel", 0.0f);
            waitFor (*pf, pf->getResultVersion() + 1, 30000);
        }
        pf->setPlayHead (nullptr);
        pf->releaseResources();
    }

    writeSetting ("tourDone", tourSetting);
    proc->releaseResources();
    proc.reset();
    std::cout << (failures == 0 ? "\nALL UI/PLUGIN TESTS PASSED\n" : "\nFAILURES\n");
    return failures == 0 ? 0 : 1;
}
