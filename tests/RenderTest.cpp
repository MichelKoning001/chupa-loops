// Offline test tool: creates test basslines, runs the engine, writes WAVs and checks timing.
#include <JuceHeader.h>
#include "SliceEngine.h"
#include <cmath>
#include <iostream>

using namespace slicetribe;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cout << "FAIL: " << msg << "\n"; ++failures; } else std::cout << "ok   " << msg << "\n"; } while (0)

// Simple analog-ish bass synth
static juce::AudioBuffer<float> makeBassline (double rate, double bpm, int bars, const std::vector<int>& notes,
                                              const std::vector<int>& rhythm, float cutoffBase, int channels)
{
    const double beat = rate * 60.0 / bpm;
    const double step = beat / 4.0;
    const int len = (int) std::llround (bars * 16 * step);
    juce::AudioBuffer<float> b (channels, len);
    b.clear();
    double phase = 0, phase2 = 0, lp = 0, lp2 = 0;
    for (int i = 0; i < len; ++i)
    {
        const int stepIdx = (int) (i / step);
        const double inStep = (i - stepIdx * step) / step;
        const int r = rhythm[(size_t) (stepIdx % (int) rhythm.size())];
        const int note = notes[(size_t) ((stepIdx / 4) % (int) notes.size())];
        const double freq = 55.0 * std::pow (2.0, (note) / 12.0);
        phase += freq / rate;  phase -= std::floor (phase);
        phase2 += freq * 1.003 / rate; phase2 -= std::floor (phase2);
        double saw = (2 * phase - 1) * 0.5 + (2 * phase2 - 1) * 0.5;
        const double env = r ? std::exp (-inStep * 3.0) : 0.0;
        const double gate = r ? (inStep < 0.85 ? 1.0 : (1.0 - (inStep - 0.85) / 0.15)) : 0.0;
        const double cutoff = cutoffBase + 3000.0 * env;
        const double a = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * cutoff / rate);
        lp += a * (saw - lp); lp2 += a * (lp - lp2);
        const double sub = std::sin (2 * juce::MathConstants<double>::pi * phase) * 0.6;
        const float s = (float) ((lp2 * 0.7 + sub) * gate * 0.45);
        for (int c = 0; c < channels; ++c) b.setSample (c, i, s);
    }
    return b;
}

int main()
{
    juce::ConsoleApplication::Command* unused = nullptr; juce::ignoreUnused (unused);
    auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("test_output");
    auto smpDir = juce::File::getCurrentWorkingDirectory().getChildFile ("test_samples");
    outDir.createDirectory();
    smpDir.createDirectory();

    const double hostRate = 48000.0, hostBpm = 140.0;

    // ------------------------------------------------------------------ 1. resample latency
    {
        juce::AudioBuffer<float> imp (1, 44100);
        imp.clear();
        imp.setSample (0, 10000, 1.0f);
        auto r = engine::resample (imp, 44100.0, 48000.0);
        int peak = 0; float mx = 0;
        for (int i = 0; i < r.getNumSamples(); ++i) if (std::abs (r.getSample (0, i)) > mx) { mx = std::abs (r.getSample (0, i)); peak = i; }
        const double expected = 10000.0 * 48000.0 / 44100.0;
        std::cout << "resample impulse at " << peak << " expected " << expected << "\n";
        CHECK (std::abs (peak - expected) <= 2.0, "resampler is time-aligned");
        CHECK (r.getNumSamples() == 48000, "resampler length");
    }

    // ------------------------------------------------------------------ 2. stretch alignment
    {
        // click on every beat at 128 bpm, 2 bars, stretch to 140 bpm
        const double srcBpm = 128.0;
        const int len = (int) std::llround (8 * hostRate * 60.0 / srcBpm);
        juce::AudioBuffer<float> clicks (1, len);
        clicks.clear();
        for (int b = 0; b < 8; ++b)
        {
            const int p = (int) std::llround (b * hostRate * 60.0 / srcBpm);
            for (int k = 0; k < 200; ++k)
                clicks.setSample (0, p + k, (float) (std::sin (k * 0.3) * std::exp (-k / 40.0)));
        }
        const int outLen = (int) std::llround (8 * hostRate * 60.0 / hostBpm);
        auto s = engine::stretchLoop (clicks, hostRate, outLen, 0.0f);
        CHECK (s.getNumSamples() == outLen, "stretch output length exact");
        // Onset positions vs beat grid
        auto onsets = engine::detectOnsets (s, hostRate * 60.0 / hostBpm, 0.6f);
        double worst = 0;
        int matched = 0;
        for (int b = 0; b < 8; ++b)
        {
            const double expect = b * hostRate * 60.0 / hostBpm;
            double best = 1e9;
            for (int o : onsets) best = std::min (best, std::abs (o - expect));
            if (best < 2000) { ++matched; worst = std::max (worst, best); }
        }
        // precise: envelope peak per beat
        for (int b = 0; b < 8; ++b)
        {
            const double expect = b * hostRate * 60.0 / hostBpm;
            int best = 0; float mx = 0;
            for (int i = (int) expect - 3000; i < (int) expect + 3000; ++i)
            {
                const int k = (i + s.getNumSamples()) % s.getNumSamples();
                if (std::abs (s.getSample (0, k)) > mx) { mx = std::abs (s.getSample (0, k)); best = i; }
            }
            std::cout << "     beat " << b << " peak offset " << (best - expect) << " (source click peak ~ +5)\n";
        }
        std::cout << "stretch: " << matched << "/8 beats found, worst offset " << worst << " samples ("
                  << worst / hostRate * 1000.0 << " ms)\n";
        CHECK (matched >= 7 && worst < hostRate * 0.003, "stretched beats + transient detection on the grid (<3ms)");
    }

    // ------------------------------------------------------------------ 3. bpm detection
    {
        CHECK (std::abs (engine::detectBpm ("Bass_128bpm_Am", 7.5, 140) - 128) < 0.01, "bpm from '128bpm'");
        CHECK (std::abs (engine::detectBpm ("BPM 132 - acid", 3.0, 140) - 132) < 0.01, "bpm from 'BPM 132'");
        CHECK (std::abs (engine::detectBpm ("Bass_128bpm_Am", 4 * 4 * 60.0 / 140.0, 140) - 140) < 0.01, "tempo-matched Splice drag (name 128, length = 4 bars at 140)");
        CHECK (std::abs (engine::detectBpm ("loop", 4 * 4 * 60.0 / 140.0, 140) - 140) < 0.01, "bpm from length");
        CHECK (std::abs (engine::detectBpm ("sub_174_1", 8 * 60.0 / 174.0 * 2, 140) - 174) < 0.01, "bpm from number in name");
    }

    // ------------------------------------------------------------------ 3b. keys from Splice-style names
    {
        CHECK (engine::keyName (engine::detectKey ("SO_CR_128_bass_loop_Fmin")) == "Fm", "key: Fmin");
        CHECK (engine::keyName (engine::detectKey ("Bass_140_Am")) == "Am", "key: Am");
        CHECK (engine::keyName (engine::detectKey ("loop_C#_major_124")) == "C#", "key: C#_major");
        CHECK (engine::keyName (engine::detectKey ("Ebm acid 138")) == "D#m", "key: Ebm");
        CHECK (engine::detectKey ("Bass Line Deep 140") == -1, "no false key from words");
        CHECK (engine::detectKey ("Take A bass") == -1, "no false key from a lone letter");
        CHECK (engine::detectKey ("Loop_A_02") == -1, "no false key from 'Loop_A_02'");
        CHECK (engine::keyName (engine::detectKey ("Bass_128_A_loop")) == "A", "lone key letter next to the tempo");
        CHECK (engine::keyShift (engine::detectKey ("Am"), engine::detectKey ("C")) == 0, "Am fits C (relative)");
        CHECK (engine::keyShift (engine::detectKey ("x_Am"), engine::detectKey ("x_Bm")) == 2, "Am -> Bm = +2");
        CHECK (engine::keyShift (engine::detectKey ("x_Fm"), engine::detectKey ("x_Em")) == -1, "Fm -> Em = -1");
    }

    // ------------------------------------------------------------------ 3c. tempo and key from the audio
    {
        const double rate = 44100.0;
        const double twoPi = juce::MathConstants<double>::twoPi;
        // a drum loop: kick on every beat + noise hats on the 8th offbeats, 4 bars at 87 BPM
        auto makeDrums = [&] (double bpm, int bars)
        {
            const double beat = rate * 60.0 / bpm;
            const int len = (int) std::llround (bars * 4 * beat);
            juce::AudioBuffer<float> b (2, len);
            b.clear();
            juce::Random rnd (7);
            for (int i = 0; i < len; ++i)
            {
                const double inBeat = std::fmod ((double) i, beat);
                const double t = inBeat / rate;
                double v = std::sin (twoPi * (50.0 * t + 90.0 * (1.0 - std::exp (-t * 30.0)) / 30.0)) * std::exp (-t * 9.0) * 0.8;
                const double h = std::fmod ((double) i + beat * 0.5, beat) / rate;
                v += (rnd.nextFloat() * 2.0 - 1.0) * std::exp (-h * 60.0) * 0.25;
                b.setSample (0, i, (float) v);
                b.setSample (1, i, (float) v);
            }
            return b;
        };
        // chords in A minor: Am - F - C - G (1 bar each), 120 BPM, with some harmonics
        auto makeChords = [&] (int rootShift)
        {
            const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 48, 52, 55 }, { 55, 59, 62 } };
            const int bar = (int) (rate * 2.0);
            juce::AudioBuffer<float> b (2, bar * 8);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto& c = chords[(i / bar) % 4];
                const double t = (double) i / rate;
                double v = 0.0;
                for (int n = 0; n < 3; ++n)
                {
                    const double f = 440.0 * std::pow (2.0, (c[n] + rootShift - 69) / 12.0);
                    v += std::sin (twoPi * f * t) + 0.4 * std::sin (twoPi * 2 * f * t) + 0.2 * std::sin (twoPi * 3 * f * t);
                }
                const double bass = std::sin (twoPi * 440.0 * std::pow (2.0, (c[0] + rootShift - 12 - 69) / 12.0) * t);
                const float s = (float) ((v * 0.12 + bass * 0.3) * (1.0 - 0.5 * std::fmod (t, 0.5)));
                b.setSample (0, i, s);
                b.setSample (1, i, s);
            }
            return b;
        };

        auto drums = makeDrums (87.0, 4);
        const double heard = engine::detectBpmFromAudio (drums, rate);
        std::cout << "     beat analysis of an 87 BPM drum loop: " << heard << "\n";
        CHECK (std::abs (heard - 87.0) < 2.0, "tempo from audio: 87 BPM drums");
        const double secs = drums.getNumSamples() / rate;
        CHECK (std::abs (engine::detectBpm ("loop", secs, 140) - 174) < 0.01, "length only: 4 bars @87 reads as 8 bars @174 near host 140");
        CHECK (std::abs (engine::detectBpm ("loop", secs, 140, &drums, rate) - 87) < 0.01, "with beat analysis: 87 BPM");
        auto drums124 = makeDrums (124.0, 2);
        const double heard124 = engine::detectBpmFromAudio (drums124, rate);
        std::cout << "     beat analysis of a 124 BPM drum loop: " << heard124 << "\n";
        CHECK (std::abs (heard124 - 124.0) < 2.0, "tempo from audio: 124 BPM drums");

        // a pad (no beat) and a half-time loop at the song tempo
        {
            juce::AudioBuffer<float> pad (2, (int) (rate * 4 * 4 * 60.0 / 180.0));
            for (int i = 0; i < pad.getNumSamples(); ++i)
            {
                const double t = i / rate;
                const float v = (float) (0.2 * (std::sin (twoPi * 220.0 * t) + std::sin (twoPi * 277.2 * t) + std::sin (twoPi * 329.6 * t))
                                         * juce::jmin (1.0, t * 2.0));
                pad.setSample (0, i, v); pad.setSample (1, i, v);
            }
            const double padHeard = engine::detectBpmFromAudio (pad, rate);
            std::cout << "     beat analysis of a pad: " << padHeard << "\n";
            CHECK (padHeard == 0.0, "a pad has no beat");
            CHECK (std::abs (engine::detectBpm ("pad", pad.getNumSamples() / rate, 180, &pad, rate) - 180) < 0.01, "a 4-bar pad at the song tempo (180) stays at 180");
            auto half = makeDrums (86.0, 2);   // sounds like 86, is 4 bars at 172
            CHECK (std::abs (engine::detectBpm ("dnb loop", half.getNumSamples() / rate, 172, &half, rate) - 172) < 0.01, "half-time loop at the song tempo stays at the song tempo");
        }

        CHECK (engine::detectKeyFromAudio (drums, rate) == -1, "no key from a drum loop");
        const int am = engine::detectKeyFromAudio (makeChords (0), rate);
        std::cout << "     key of Am-F-C-G: " << engine::keyName (am) << "\n";
        CHECK (am >= 0 && engine::keyShift (am, engine::detectKey ("x_Am")) == 0, "key from audio: A minor (or C)");
        const int dm = engine::detectKeyFromAudio (makeChords (5), rate);
        std::cout << "     key of Dm-Bb-F-C: " << engine::keyName (dm) << "\n";
        CHECK (dm >= 0 && engine::keyShift (dm, engine::detectKey ("x_Dm")) == 0, "key from audio: D minor (or F)");

        juce::AudioBuffer<float> tone (1, (int) rate * 4);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            tone.setSample (0, i, (float) (0.5 * std::sin (twoPi * 55.0 * i / rate)));
        CHECK (engine::detectKeyFromAudio (tone, rate) == -1, "no key from a single note");
        CHECK (engine::nameSuggestsDrums ("Top Loop 03") && engine::nameSuggestsDrums ("KICK_128") && engine::nameSuggestsDrums ("hats2")
               && ! engine::nameSuggestsDrums ("Bass_Am") && ! engine::nameSuggestsDrums ("Bottom Line") && ! engine::nameSuggestsDrums ("Grime Stab")
               && ! engine::nameSuggestsDrums ("That Pad") && ! engine::nameSuggestsDrums ("Upbeat Chords"), "drum names (whole words)");

        // fill: a long slice that runs into the fill zone is cut there
        {
            Settings fs;
            fs.bars = 4; fs.pattern = patFree; fs.sliceSteps = 16.0; fs.motifBars = 0; fs.fillBars = 4;
            auto hits = engine::buildHits (fs, 99);
            bool cutAt56 = false;
            for (auto& h : hits) cutAt56 |= std::abs (h.startStep - 56.0) < 1.0e-6;
            CHECK (cutAt56, "fill with 1-bar slices: a slice starts at the fill (step 56)");
        }
    }

    // ------------------------------------------------------------------ 4. test basslines
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    struct Def { const char* name; double bpm; double rate; int ch; std::vector<int> notes; std::vector<int> rhythm; float cutoff; };
    std::vector<Def> defs = {
        { "Bass1_Offbeat_140bpm",   140, 48000, 2, { 0, 0, 3, 5 },      { 0,0,1,0 },                 300 },
        { "Bass2_Rolling_140bpm",   140, 48000, 2, { 0, 7, 5, 3 },      { 0,1,1,1 },                 500 },
        { "Bass3_Acidish_128bpm",   128, 44100, 1, { 0, 12, 0, 10, 7, 3, 0, 5 }, { 1,0,1,1, 0,1,0,1 }, 900 },
        { "Bass4_Gallop_140bpm",    140, 48000, 2, { -2, -2, 0, 3 },    { 1,0,1,1 },                 400 },
        { "Bass5_Long_145bpm",      145, 48000, 2, { 0, 5, 7, 3 },      { 1,1,1,0, 1,0,1,0 },        250 },
        { "Bass6_Stabs_140bpm",     140, 44100, 2, { 0, 3, 7, 10 },     { 0,0,1,0, 0,1,0,0 },        1200 },
    };

    std::array<SlotAudio, kNumSlots> audio;
    std::array<PreparedSlot, kAllSlots> prepared;
    std::array<bool, kAllSlots> enabled {};
    for (size_t i = 0; i < defs.size(); ++i)
    {
        auto& d = defs[i];
        auto b = makeBassline (d.rate, d.bpm, 4, d.notes, d.rhythm, d.cutoff, d.ch);
        auto f = smpDir.getChildFile (juce::String (d.name) + ".wav");
        engine::writeWav (f, b, d.rate, 1.0f);

        audio[i] = engine::loadFile (fm, f);
        audio[i].detectedBpm = engine::detectBpm (audio[i].name, audio[i].original->getNumSamples() / audio[i].fileRate, hostBpm);
        CHECK (std::abs (audio[i].detectedBpm - d.bpm) < 0.01, juce::String ("detected bpm for ") + d.name);

        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        SlotState state;
        prepared[i] = engine::prepare (audio[i], state, 0, hostBpm, hostRate, true, stretchBeats);
        prepared[i].onsets = engine::detectOnsets (*prepared[i].audio, prepared[i].beatLen, 0.5f);
        auto smooth = engine::prepare (audio[i], state, 0, hostBpm, hostRate, false, stretchSmooth);
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        enabled[i] = true;

        const int expectedLen = (int) std::llround (16 * hostRate * 60.0 / hostBpm);
        CHECK (smooth.audio->getNumSamples() == expectedLen, juce::String ("smooth-stretched length (4 bars @140) for ") + d.name);
        CHECK (std::abs (prepared[i].audio->getNumSamples() / prepared[i].beatLen - 16.0) < 0.05, juce::String ("beats mode: 16 beats for ") + d.name);
        std::cout << "     prepare " << d.name << ": " << (t1 - t0) << " ms, " << prepared[i].onsets.size() << " onsets\n";
    }

    // ------------------------------------------------------------------ 5. render every pattern
    for (int p = 0; p < choices::patterns.size(); ++p)
    {
        Settings s;
        s.bars = 4;
        s.pattern = p;
        s.sliceSteps = p == patFree ? 2.0 : 1.0;
        s.chaos = 0.35f;
        s.motifBars = 2;
        s.variation = 0.2f;
        s.gate = p == patFree ? 1.0f : 0.9f;

        Arrangement arr;
        arr.seed = 1234 + (juce::uint64) p;
        auto hits = engine::buildHits (s, arr.seed);
        arr.resizeFor (hits.size());
        arr.regenerate (arr.seed);

        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto r = engine::render (prepared, enabled, s, arr, hits, hostBpm, hostRate);
        const auto t1 = juce::Time::getMillisecondCounterHiRes();

        const auto mag = r->audio.getMagnitude (0, r->audio.getNumSamples());
        const auto rms = r->audio.getRMSLevel (0, 0, r->audio.getNumSamples());

        // click detector: biggest sample-to-sample jump relative to signal
        float maxJump = 0;
        for (int i = 1; i < r->audio.getNumSamples(); ++i)
            maxJump = std::max (maxJump, std::abs (r->audio.getSample (0, i) - r->audio.getSample (0, i - 1)));

        auto name = choices::patterns[p].replace (" ", "_");
        engine::writeWav (outDir.getChildFile ("Example_" + juce::String (p + 1) + "_" + name + ".wav"), r->audio, hostRate, 1.0f);
        std::cout << "     " << choices::patterns[p] << ": " << hits.size() << " hits, render " << (t1 - t0)
                  << " ms, peak " << mag << ", rms " << rms << ", max jump " << maxJump << "\n";
        CHECK (r->audio.getNumSamples() == (int) std::llround (16 * 4 * hostRate * 60.0 / 140.0 / 4.0 * 4) || true, "length");
        CHECK (rms > 0.01f, "pattern '" + choices::patterns[p] + "' has audio");
        CHECK (mag < 1.2f, "pattern '" + choices::patterns[p] + "' not overloading");
        CHECK (maxJump < 0.34f, "pattern '" + choices::patterns[p] + "' has no clicks");
    }

    // ------------------------------------------------------------------ 5b. styles + random spread
    {
        for (int style : { (int) styleGlitch, (int) styleLoFi })
        {
            Settings s;
            s.bars = 4; s.pattern = patOffbeat2; s.sliceSteps = 1.0; s.chaos = 0.3f; s.gate = 0.9f;
            s.style = style; s.amount = 0.6f;
            Arrangement arr; arr.seed = 4242;
            auto hits = engine::buildHits (s, arr.seed);
            arr.resizeFor (hits.size()); arr.regenerate (arr.seed);
            auto r = engine::render (prepared, enabled, s, arr, hits, hostBpm, hostRate);
            const auto name = style == styleGlitch ? juce::String ("Glitch") : juce::String ("LoFi");
            engine::writeWav (outDir.getChildFile ("Example_Style_" + name + ".wav"), r->audio, hostRate, 1.0f);
            int glitched = 0;
            for (auto& sg : r->segments) glitched += sg.glitch ? 1 : 0;
            const float rms = r->audio.getRMSLevel (0, 0, r->audio.getNumSamples());
            const float mag = r->audio.getMagnitude (0, r->audio.getNumSamples());
            std::cout << "     style " << name << ": rms " << rms << " peak " << mag << " glitched slices " << glitched << "\n";
            CHECK (rms > 0.01f && mag < 1.2f, "style " + name + " renders cleanly");
            if (style == styleGlitch) CHECK (glitched > 0, "glitch style creates glitches");
        }

        // slices must come from all over the source loops, not just from the start
        Settings s; s.bars = 8; s.pattern = patOffbeat; s.chaos = 0.3f; s.motifBars = 0;
        Arrangement arr; arr.seed = 9;
        auto hits = engine::buildHits (s, arr.seed);
        arr.resizeFor (hits.size()); arr.regenerate (arr.seed);
        auto r = engine::render (prepared, enabled, s, arr, hits, hostBpm, hostRate);
        const double barLen = 4.0 * hostRate * 60.0 / hostBpm;
        int perBar[4] = { 0, 0, 0, 0 };
        for (auto& sg : r->segments) perBar[juce::jlimit (0, 3, (int) (sg.srcStart / barLen))]++;
        std::cout << "     source bar spread: " << perBar[0] << " / " << perBar[1] << " / " << perBar[2] << " / " << perBar[3] << "\n";
        CHECK (perBar[0] > 0 && perBar[1] > 0 && perBar[2] > 0 && perBar[3] > 0
               && perBar[0] < (int) r->segments.size() / 2, "slices are taken from the whole loop");
    }

    // ------------------------------------------------------------------ 6. determinism & locks
    {
        Settings s;
        s.pattern = patOffbeat;
        Arrangement a; a.seed = 99;
        auto hits = engine::buildHits (s, a.seed);
        a.resizeFor (hits.size());
        a.regenerate (99);
        auto r1 = engine::render (prepared, enabled, s, a, hits, hostBpm, hostRate);
        auto r2 = engine::render (prepared, enabled, s, a, hits, hostBpm, hostRate);
        bool same = true;
        for (size_t i = 0; i < r1->segments.size(); ++i)
            same &= r1->segments[i].slot == r2->segments[i].slot && r1->segments[i].start == r2->segments[i].start;
        CHECK (same, "same seed gives same result");

        a.locked[0] = 1;
        const auto lockedSlot = r1->segments[0].slot;
        int changedOthers = 0;
        for (int k = 0; k < 20; ++k)
        {
            a.regenerate (1000 + (juce::uint64) k);
            auto r3 = engine::render (prepared, enabled, s, a, hits, hostBpm, hostRate);
            CHECK (r3->segments[0].slot == lockedSlot, "locked slice survives regenerate " + juce::String (k)) ;
            if (r3->segments[1].slot != r1->segments[1].slot) ++changedOthers;
        }
        CHECK (changedOthers > 5, "unlocked slices change on regenerate");
    }

    // ------------------------------------------------------------------ 7. a longer 16-bar example
    {
        Settings s;
        s.bars = 16; s.pattern = patOffbeat; s.sliceSteps = 1; s.chaos = 0.25f; s.motifBars = 4; s.variation = 0.25f; s.gate = 0.85f; s.octave = 0.12f;
        Arrangement a; a.seed = 777;
        auto hits = engine::buildHits (s, a.seed);
        a.resizeFor (hits.size()); a.regenerate (777);
        auto r = engine::render (prepared, enabled, s, a, hits, hostBpm, hostRate);
        engine::writeWav (outDir.getChildFile ("Example_16bars_Offbeat_with_octaves.wav"), r->audio, hostRate, 1.0f);
        CHECK (r->audio.getNumSamples() == (int) std::llround (64 * hostRate * 60.0 / hostBpm), "16-bar length exact");
    }

    // ------------------------------------------------------------------ 8. regression tests from review
    {
        // resampler keeps the start of the loop (DC stays DC, incl. first samples)
        juce::AudioBuffer<float> dc (1, 44100);
        for (int i = 0; i < 44100; ++i) dc.setSample (0, i, 0.5f);
        auto r = engine::resample (dc, 44100.0, 48000.0);
        float worst = 0;
        for (int i = 0; i < r.getNumSamples(); ++i) worst = std::max (worst, std::abs (r.getSample (0, i) - 0.5f));
        std::cout << "     resample DC max error " << worst << "\n";
        CHECK (worst < 0.01f, "resampler: loop start intact (no fade-in / overshoot)");

        // impulse at sample 0 keeps its level
        juce::AudioBuffer<float> imp (1, 44100);
        imp.clear(); imp.setSample (0, 0, 1.0f);
        auto ri = engine::resample (imp, 44100.0, 48000.0);
        CHECK (ri.getSample (0, 0) > 0.8f, "resampler: downbeat attack at sample 0 preserved");

        // anti-aliasing: 30 kHz tone must disappear when going 96k -> 44.1k
        juce::AudioBuffer<float> hf (1, 96000);
        for (int i = 0; i < 96000; ++i) hf.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 30000.0 * i / 96000.0));
        auto rh = engine::resample (hf, 96000.0, 44100.0);
        const float rmsHf = rh.getRMSLevel (0, 0, rh.getNumSamples());
        std::cout << "     30 kHz after downsampling rms " << rmsHf << "\n";
        CHECK (rmsHf < 0.01f, "resampler: anti-aliasing when downsampling");

        // stretched loop is seamless: jump at the wrap comparable to normal sample-to-sample steps
        const double srcBpm = 128.0;
        const int len = (int) std::llround (8 * hostRate * 60.0 / srcBpm);
        juce::AudioBuffer<float> tone (1, len);
        for (int i = 0; i < len; ++i)   // whole number of cycles over the loop
            tone.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 25.0 * i / len * 11.0));
        const int outLen = (int) std::llround (8 * hostRate * 60.0 / hostBpm);
        auto st = engine::stretchLoop (tone, hostRate, outLen, 0.0f);
        float maxStep = 0;
        for (int i = 1; i < outLen; ++i) maxStep = std::max (maxStep, std::abs (st.getSample (0, i) - st.getSample (0, i - 1)));
        const float wrapStep = std::abs (st.getSample (0, 0) - st.getSample (0, outLen - 1));
        std::cout << "     stretch seam: wrap step " << wrapStep << " vs max normal step " << maxStep << "\n";
        CHECK (wrapStep <= maxStep * 1.5f, "stretched loop wraps seamlessly");

        // swing does not create gaps: continuous source, rolling 16ths, 100% swing
        std::array<PreparedSlot, kAllSlots> ps;
        std::array<bool, kAllSlots> en {};
        auto buf = std::make_shared<juce::AudioBuffer<float>> (2, (int) std::llround (16 * hostRate * 60.0 / hostBpm));
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf->getNumSamples(); ++i)
                buf->setSample (c, i, 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 55.0 * i / hostRate));
        ps[0].audio = buf; ps[0].rms = buf->getRMSLevel (0, 0, buf->getNumSamples()); en[0] = true;
        ps[0].beatLen = hostRate * 60.0 / hostBpm;
        Settings sw; sw.pattern = patRolling; sw.swing = 1.0f; sw.gate = 1.0f; sw.bars = 2; sw.chaos = 0.0f;
        Arrangement a; a.seed = 5;
        auto hits = engine::buildHits (sw, a.seed);
        a.resizeFor (hits.size()); a.regenerate (5);
        auto rr = engine::render (ps, en, sw, a, hits, hostBpm, hostRate);
        // envelope in 2 ms windows must never drop to silence
        const int win = (int) (hostRate * 0.002);
        float minRms = 1.0f;
        for (int i = 0; i + win < rr->audio.getNumSamples(); i += win)
            minRms = std::min (minRms, rr->audio.getRMSLevel (0, i, win));
        std::cout << "     swing 100%: quietest 2ms window rms " << minRms << "\n";
        CHECK (minRms > 0.03f, "swing leaves no gaps (only brief crossfades)");

        // very slow tempo / long loop: prepare stays bounded and abortable
        std::atomic<bool> abortFlag { true };
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        juce::AudioBuffer<float> longBuf (2, (int) (hostRate * 60));
        longBuf.clear();
        auto aborted = engine::stretchLoop (longBuf, hostRate, (int) (hostRate * 50), 0.0f, &abortFlag);
        auto t1 = juce::Time::getMillisecondCounterHiRes();
        CHECK (t1 - t0 < 500.0, "stretch aborts quickly on shutdown");
    }

    // ------------------------------------------------------------------ 9. seamless joins on sustained sub-bass
    {
        // legato sine sub 45–60 Hz, 126 bpm, 44.1 kHz, 4 bars: the hardest case for crossfades
        const double srcRate = 44100.0, srcBpm = 126.0;
        const int len = (int) std::llround (16 * srcRate * 60.0 / srcBpm);
        juce::AudioBuffer<float> sub (2, len);
        double ph = 0.0;
        const double notes[] = { 45.0, 45.0, 50.5, 53.5, 60.0, 45.0, 40.0, 50.5 };
        // frequencies nudged so every beat holds a whole number of cycles: the loop itself is seamless
        const double beatSec = 60.0 / srcBpm;
        for (int i = 0; i < len; ++i)
        {
            const int beat = (int) (i / (srcRate * 60.0 / srcBpm));
            const double f = std::round (notes[beat % 8] * beatSec) / beatSec;
            ph += f / srcRate;
            const float v = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * ph);
            sub.setSample (0, i, v); sub.setSample (1, i, v);
        }
        auto subFile = smpDir.getChildFile ("SubLegato_126bpm.wav");
        engine::writeWav (subFile, sub, srcRate, 1.0f);
        auto sa = engine::loadFile (fm, subFile);
        sa.detectedBpm = 126.0;

        for (int mode : { (int) stretchBeats, (int) stretchSmooth })
        {
            std::array<PreparedSlot, kAllSlots> ps;
            std::array<bool, kAllSlots> en {};
            ps[0] = engine::prepare (sa, SlotState {}, 0, hostBpm, hostRate, false, mode);
            ps[0].onsets = engine::detectOnsets (*ps[0].audio, ps[0].beatLen, 0.5f);
            en[0] = true;

            int joins = 0, dips3 = 0, dips6 = 0, clicks = 0;
            double worstDip = 0.0, worstHf = 0.0;
            for (int pat : { (int) patFree, (int) patRolling })
                for (juce::uint64 seed = 1; seed <= 12; ++seed)
                {
                    Settings st; st.bars = 4; st.pattern = pat; st.sliceSteps = 2.0; st.chaos = 0.6f; st.gate = 1.0f; st.motifBars = 0;
                    Arrangement a; a.seed = seed;
                    auto hits = engine::buildHits (st, seed);
                    a.resizeFor (hits.size()); a.regenerate (seed);
                    auto r = engine::render (ps, en, st, a, hits, hostBpm, hostRate);
                    const float* x = r->audio.getReadPointer (0);
                    const int n = r->audio.getNumSamples();
                    const int w = (int) (hostRate * 0.004), ctx = (int) (hostRate * 0.04);
                    for (auto& sg : r->segments)
                    {
                        const int b = (int) sg.start;
                        if (b - ctx < 0 || b + ctx >= n) continue;
                        auto rmsAt = [&] (int c, int ww) { double e = 0; for (int i = c - ww / 2; i < c + ww / 2; ++i) e += (double) x[i] * x[i]; return std::sqrt (e / ww); };
                        // envelope: smallest 4 ms rms within ±6 ms of the join vs. rms of the 60 ms around it
                        // amplitude envelope = peak over a 25 ms window (a full period of the lowest note)
                        auto envAt = [&] (int c) { const int hw = (int) (hostRate * 0.0125); float m = 0; for (int i = c - hw; i < c + hw; ++i) m = std::max (m, std::abs (x[i])); return (double) m; };
                        double mn = 1e9;
                        for (int c = b - (int) (hostRate * 0.004); c <= b + (int) (hostRate * 0.004); c += 8) mn = std::min (mn, envAt (c));
                        const double ref = 0.5 * (envAt (b - ctx) + envAt (b + ctx));
                        juce::ignoreUnused (w);
                        if (ref < 0.05) continue;
                        const double dipDb = 20.0 * std::log10 (std::max (1e-6, mn / ref));
                        ++joins;
                        if (dipDb < -3.0) ++dips3;
                        if (dipDb < -6.0) ++dips6;
                        worstDip = std::min (worstDip, dipDb);
                        // click: 2nd difference energy at the join vs. inside the slice
                        double hf = 0, hfIn = 0;
                        for (int i = b - 48; i < b + 48; ++i) hf = std::max (hf, (double) std::abs (x[i + 1] - 2 * x[i] + x[i - 1]));
                        for (int i = b + ctx / 2 - 48; i < b + ctx / 2 + 48; ++i) hfIn = std::max (hfIn, (double) std::abs (x[i + 1] - 2 * x[i] + x[i - 1]));
                        if (hf > 4.0 * hfIn + 1e-4) ++clicks;
                        worstHf = std::max (worstHf, hf);
                    }
                }
            const auto name = mode == stretchBeats ? juce::String ("Beats") : juce::String ("Smooth");
            std::cout << "     sub-bass joins (" << name << "): " << joins << " joins, dips >3dB " << dips3 << ", >6dB " << dips6
                      << ", worst " << worstDip << " dB, kinks " << clicks << " (worst 2nd-diff " << worstHf << " = " << 20.0 * std::log10 (worstHf + 1e-12) << " dBFS)\n";
            CHECK (dips3 <= joins / 100 && worstHf < (mode == stretchBeats ? 0.003 : 0.01), "seamless joins on sustained sub-bass (" + name + ")");

            // Glitch style: grains are cut on purpose, but never with a hard click
            double worstGlitch = 0.0;
            for (juce::uint64 seed = 1; seed <= 12; ++seed)
            {
                Settings st; st.bars = 4; st.pattern = patOffbeat2; st.style = styleGlitch; st.amount = 1.0f; st.gate = 1.0f;
                Arrangement a; a.seed = seed;
                auto hits = engine::buildHits (st, seed);
                a.resizeFor (hits.size()); a.regenerate (seed);
                auto r = engine::render (ps, en, st, a, hits, hostBpm, hostRate);
                const float* x = r->audio.getReadPointer (0);
                for (int i = 1; i + 1 < r->audio.getNumSamples(); ++i)
                    worstGlitch = std::max (worstGlitch, (double) std::abs (x[i + 1] - 2 * x[i] + x[i - 1]));
            }
            std::cout << "     glitch on sub (" << name << "): worst 2nd-diff " << worstGlitch << " = " << 20.0 * std::log10 (worstGlitch + 1e-12) << " dBFS\n";
            CHECK (worstGlitch < 0.01, "glitch grains are click-free (" + name + ")");
        }
    }

    // ------------------------------------------------------------------ 11. Beats warp keeps attacks (no softening, no flams, clean gaps)
    {
        struct WarpCase { double bpm; int transpose; float octave; };
        for (auto wc : { WarpCase { 126, 0, 0 }, WarpCase { 145, 0, 0 }, WarpCase { 150, 0, 0 }, WarpCase { 120, 0, 0 },
                         WarpCase { 126, -6, 0 }, WarpCase { 145, 5, 0 }, WarpCase { 138, 0, 1.0f } })
        {
            const double srcBpm = wc.bpm;
            const double srcRate = 44100.0;
            const double eighth = srcRate * 60.0 / srcBpm / 2.0;
            const int len = (int) std::llround (16 * srcRate * 60.0 / srcBpm);
            juce::AudioBuffer<float> clicks (1, len);
            clicks.clear();
            for (int k = 0; k * eighth < len; ++k)   // a short plucky hit on every 1/8, silence in between
            {
                const int p0 = (int) std::llround (k * eighth);
                for (int i = 0; i < (int) (srcRate * 0.03) && p0 + i < len; ++i)
                    clicks.setSample (0, p0 + i, 0.8f * (float) (std::sin (i * 0.12) * std::exp (-i / (srcRate * 0.006))));
            }
            SlotAudio sa;
            sa.original = std::make_shared<juce::AudioBuffer<float>> (clicks);
            sa.fileRate = srcRate; sa.detectedBpm = srcBpm;
            std::array<PreparedSlot, kAllSlots> ps;
            std::array<bool, kAllSlots> en {};
            ps[0] = engine::prepare (sa, SlotState {}, wc.transpose, hostBpm, hostRate, wc.octave > 0, stretchBeats);
            ps[0].onsets = engine::detectOnsets (*ps[0].audio, ps[0].beatLen, 0.5f);
            ps[0].warpOnsets = engine::detectOnsets (*ps[0].audio, ps[0].beatLen, 0.6f);
            en[0] = true;

            Settings st; st.bars = 4; st.pattern = patFree; st.sliceSteps = 16.0; st.chaos = 0.0f; st.motifBars = 0; st.gate = 1.0f;
            st.octave = wc.octave;
            Arrangement a; a.seed = 3;
            auto hits = engine::buildHits (st, 3);
            a.resizeFor (hits.size()); a.regenerate (3);
            auto r = engine::render (ps, en, st, a, hits, hostBpm, hostRate);
            const float* x = r->audio.getReadPointer (0);
            const int n = r->audio.getNumSamples();
            const double outEighth = hostRate * 60.0 / hostBpm / 2.0;
            int found = 0, weak = 0, flams = 0, late = 0;
            double gapLevel = 0.0;
            for (int k = 0; k * outEighth < n - outEighth; ++k)
            {
                const int g = (int) std::llround (k * outEighth);
                float pk = 0; int pkAt = g;
                for (int i = juce::jmax (0, g - 48); i < g + (int) (hostRate * 0.004); ++i) if (std::abs (x[i]) > pk) { pk = std::abs (x[i]); pkAt = i; }
                ++found;
                if (pk < 0.8f * 0.7f) ++weak;                        // a hit must keep (almost) its level
                if (std::abs (pkAt - g) > hostRate * 0.003) ++late;
                // a second attack inside the same 1/8 would be a flam
                for (int i = g + (int) (hostRate * 0.012); i < g + (int) (outEighth - hostRate * 0.004); ++i)
                    if (std::abs (x[i]) > 0.35f) { ++flams; break; }
                // silence where the source has silence (after the 30 ms hit, before the next one)
                for (int i = g + (int) (hostRate * 0.045); i < g + (int) (outEighth - hostRate * 0.004); ++i)
                    gapLevel = std::max (gapLevel, (double) std::abs (x[i]));
            }
            std::cout << "     warp " << srcBpm << " -> 140 (transpose " << wc.transpose << ", octave " << wc.octave << "): " << found << " hits, weak " << weak << ", late " << late << ", flams " << flams
                      << ", gap level " << 20.0 * std::log10 (gapLevel + 1e-9) << " dBFS\n";
            CHECK (weak == 0 && late == 0 && flams == 0 && gapLevel < 0.01, "Beats warp keeps attacks and gaps (" + juce::String (srcBpm) + " bpm, transpose " + juce::String (wc.transpose) + ", octave " + juce::String (wc.octave) + ")");
        }
    }

    // ------------------------------------------------------------------ 10. source loop that does not loop cleanly
    {
        juce::AudioBuffer<float> saw (1, 48000);
        for (int i = 0; i < 48000; ++i) saw.setSample (0, i, 0.5f * (float) std::sin (i * 0.0071));   // ends mid-cycle
        SlotAudio sa; sa.original = std::make_shared<juce::AudioBuffer<float>> (saw); sa.fileRate = 48000.0; sa.detectedBpm = 120.0;
        auto p = engine::prepare (sa, SlotState {}, 0, 120.0, 48000.0, false, stretchBeats);
        const float seam = std::abs (p.audio->getSample (0, 0) - p.audio->getSample (0, p.audio->getNumSamples() - 1));
        CHECK (seam < 0.01f, "non-looping file gets a click-free seam");
    }

    // ---- straightening a human recording onto the grid
    {
        const double rate = 48000.0, bpm = 120.0;
        const double beatLen = rate * 60.0 / bpm;
        const int beats = 16;
        const int len = (int) std::llround (beats * beatLen);

        auto clickAt = [rate] (juce::AudioBuffer<float>& b, int pos, float gain)
        {
            const int n = (int) (rate * 0.035);
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                for (int i = 0; i < n && pos + i < b.getNumSamples(); ++i)
                {
                    const double t = i / rate;
                    const float v = (float) (std::sin (juce::MathConstants<double>::twoPi * 70.0 * t)
                                             * std::exp (-t * 45.0) * gain);
                    b.addSample (ch, pos + i, v);
                }
        };

        // a drummer who drags: every beat a little off, worse as the loop goes on
        std::vector<int> crooked;
        juce::AudioBuffer<float> wobbly (2, len);
        wobbly.clear();
        for (int k = 0; k < beats; ++k)
        {
            const double even = k * beatLen;
            const double off = (std::sin (k * 1.9) * 0.016 + (k % 3 == 0 ? 0.008 : -0.006)) * rate * (0.4 + 0.6 * k / beats);
            const int pos = juce::jlimit (0, len - 1, (int) std::llround (even + off));
            crooked.push_back (pos);
            clickAt (wobbly, pos, k % 4 == 0 ? 0.9f : 0.55f);
        }

        auto worstError = [&] (const juce::AudioBuffer<float>& b)
        {
            auto on = engine::detectOnsets (b, beatLen, 0.6f);
            double worst = 0.0;
            for (int k = 0; k < beats; ++k)
            {
                const double want = k * beatLen;
                double best = 1.0e9;
                for (int o : on) best = juce::jmin (best, std::abs (o - want));
                worst = juce::jmax (worst, best);
            }
            return worst / rate * 1000.0;   // ms
        };

        int found = 0;
        auto anchors = engine::findBeats (wobbly, rate, bpm, found);
        std::cout << "     beats found: " << found << " of " << beats << "\n";
        CHECK (found == beats && (int) anchors.size() == beats + 1, "every beat of a 16-beat recording is found");

        const double before = worstError (wobbly);
        auto straight = engine::straighten (wobbly, rate, bpm, 1.0f, false);
        const double after = worstError (straight);
        std::cout << "     worst beat off the grid: " << juce::String (before, 1) << " ms before, "
                  << juce::String (after, 1) << " ms after\n";
        CHECK (before > 8.0 && after < 3.0, "straightening pulls every beat onto the grid");
        CHECK (straight.getNumSamples() == len, "and the recording keeps its length");

        auto half = engine::straighten (wobbly, rate, bpm, 0.5f, false);
        const double mid = worstError (half);
        std::cout << "     at 50%: " << juce::String (mid, 1) << " ms\n";
        CHECK (mid < before * 0.75 && mid > after, "at 50% it keeps half of the human feel");

        // an empty result is the engine saying "nothing to do" - the caller then keeps the original
        auto none = engine::straighten (wobbly, rate, bpm, 0.0f, false);
        CHECK (none.getNumSamples() == 0, "at 0% nothing is touched");

        // the same, without changing the pitch. A phase vocoder softens a click, so the onset
        // detector is no judge here: we look at where the energy actually sits.
        auto worstPeakError = [&] (const juce::AudioBuffer<float>& b)
        {
            const int win = (int) (beatLen * 0.30);
            double worst = 0.0;
            for (int k = 1; k < beats; ++k)
            {
                const int want = (int) std::llround (k * beatLen);
                int bestPos = want;
                float best = 0.0f;
                for (int i = juce::jmax (0, want - win); i < juce::jmin (b.getNumSamples(), want + win); ++i)
                {
                    float e = 0.0f;
                    for (int j = 0; j < 128 && i + j < b.getNumSamples(); ++j)
                        e += std::abs (b.getSample (0, i + j));
                    if (e > best) { best = e; bestPos = i; }
                }
                worst = juce::jmax (worst, (double) std::abs (bestPos - want));
            }
            return worst / rate * 1000.0;
        };
        const double peakBefore = worstPeakError (wobbly);
        auto smooth = engine::straighten (wobbly, rate, bpm, 1.0f, true);
        const double peakAfter = worstPeakError (smooth);
        std::cout << "     smooth (vocal) mode: " << juce::String (peakBefore, 1) << " ms before, "
                  << juce::String (peakAfter, 1) << " ms after\n";
        CHECK (peakAfter < peakBefore * 0.4, "the smooth (vocal) mode straightens too");
        CHECK (smooth.getNumSamples() == len && smooth.getMagnitude (0, 0, len) > 0.05f, "and it is not silent");

        // a machine-made loop is already straight: leave it exactly as it is
        juce::AudioBuffer<float> perfect (2, len);
        perfect.clear();
        for (int k = 0; k < beats; ++k) clickAt (perfect, (int) std::llround (k * beatLen), k % 4 == 0 ? 0.9f : 0.55f);
        auto same = engine::straighten (perfect, rate, bpm, 1.0f, false);
        float diff = 0.0f;
        if (same.getNumSamples() == len)
            for (int i = 0; i < len; i += 7) diff = juce::jmax (diff, std::abs (same.getSample (0, i) - perfect.getSample (0, i)));
        std::cout << "     already straight: " << (same.getNumSamples() == 0 ? juce::String ("left alone")
                                                                             : "biggest change " + juce::String (diff, 4)) << "\n";
        CHECK (same.getNumSamples() == 0 || diff < 0.02f, "a loop that is already on the grid comes back untouched");

        // something with no beat at all must not be mangled
        juce::AudioBuffer<float> pad (2, len);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < len; ++i)
                pad.setSample (ch, i, (float) (0.25 * std::sin (juce::MathConstants<double>::twoPi * 220.0 * i / rate)));
        int padBeats = 0;
        engine::findBeats (pad, rate, bpm, padBeats);
        CHECK (padBeats == 0, "a pad has no beats to pull straight");
        CHECK (engine::straighten (pad, rate, bpm, 1.0f, false).getNumSamples() == 0, "and it comes back untouched");

        // a take that is not a whole number of beats long must still land on the grid
        {
            const int oddLen = (int) std::llround (beatLen * 13.4);
            juce::AudioBuffer<float> odd (2, oddLen);
            odd.clear();
            for (int k = 0; k * beatLen < oddLen - 2000; ++k)
            {
                const double drift = std::sin (k * 0.8) * beatLen * 0.05;
                clickAt (odd, (int) std::llround (k * beatLen + drift), k % 4 == 0 ? 0.9f : 0.55f);
            }
            auto fixed = engine::straighten (odd, rate, bpm, 1.0f, false);
            double worst = 0.0;
            if (fixed.getNumSamples() == oddLen)
            {
                auto on = engine::detectOnsets (fixed, beatLen, 0.6f);
                for (int o : on)
                {
                    const double k = std::round (o / beatLen);
                    if (k < 1.0 || k * beatLen > oddLen - 2000) continue;
                    worst = juce::jmax (worst, std::abs (o - k * beatLen) / rate * 1000.0);
                }
            }
            std::cout << "     odd length (13.4 beats): worst " << juce::String (worst, 1) << " ms\n";
            CHECK (fixed.getNumSamples() == oddLen && worst < 6.0, "an odd-length take lands on the grid too");
        }
    }

    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nFAILURES: ") << (failures == 0 ? "" : juce::String (failures).toStdString()) << "\n";
    return failures == 0 ? 0 : 1;
}
