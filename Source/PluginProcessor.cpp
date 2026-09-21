#include "PluginProcessor.h"
#include <cstring>
#include "PluginEditor.h"
#include "Skins.h"

namespace slicetribe
{

namespace
{
    juce::MemoryBlock encodeFlac (const juce::AudioBuffer<float>& source, double rate)
    {
        // FLAC is integer: scale float files that go above 0 dBFS, and store the factor in front
        float peak = 0.0f;
        for (int ch = 0; ch < source.getNumChannels(); ++ch)
            peak = juce::jmax (peak, source.getMagnitude (ch, 0, source.getNumSamples()));
        const float scale = peak > 0.999f ? 0.999f / peak : 1.0f;
        juce::AudioBuffer<float> b (source);
        b.applyGain (scale);

        juce::MemoryBlock mb;
        mb.append ("STG1", 4);
        const float inv = 1.0f / scale;
        mb.append (&inv, sizeof (float));
        {
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::MemoryOutputStream> (mb, true);
            juce::FlacAudioFormat flac;
            auto writer = flac.createWriterFor (os, juce::AudioFormatWriterOptions{}
                                                        .withSampleRate (juce::jlimit (8000.0, 655350.0, rate))
                                                        .withNumChannels (b.getNumChannels())
                                                        .withBitsPerSample (24));
            if (writer == nullptr)
                return {};
            writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        return mb;
    }

    SlotAudio decodeFlac (const juce::MemoryBlock& block)
    {
        SlotAudio s;
        float gain = 1.0f;
        juce::MemoryBlock mb (block);
        if (mb.getSize() > 8 && std::memcmp (mb.getData(), "STG1", 4) == 0)
        {
            std::memcpy (&gain, static_cast<const char*> (mb.getData()) + 4, sizeof (float));
            if (! std::isfinite (gain) || gain < 1.0f || gain > 1000.0f) gain = 1.0f;
            mb.removeSection (0, 8);
        }
        juce::FlacAudioFormat flac;
        std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (mb, false), true));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate < 8000.0 || reader->sampleRate > 384000.0)
            return s;
        const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * maxSampleSeconds));
        const int ch = (int) juce::jlimit (1u, 2u, reader->numChannels);
        auto buf = std::make_shared<juce::AudioBuffer<float>> (ch, len);
        reader->read (buf.get(), 0, len, 0, true, ch > 1);
        buf->applyGain (gain);
        s.original = buf;
        s.fileRate = reader->sampleRate;
        s.peaks = engine::computePeaks (*buf, 256);
        return s;
    }

    bool settingsEqual (const Settings& a, const Settings& b)
    {
        return a.bars == b.bars && a.pattern == b.pattern && a.sliceMode == b.sliceMode && a.sliceSteps == b.sliceSteps
            && a.sensitivity == b.sensitivity && a.chaos == b.chaos && a.motifBars == b.motifBars
            && a.variation == b.variation && a.gate == b.gate && a.swing == b.swing && a.reverse == b.reverse
            && a.octave == b.octave && a.fadeMs == b.fadeMs && a.style == b.style && a.amount == b.amount && a.stretchMode == b.stretchMode && a.fillBars == b.fillBars
            && a.energy == b.energy && a.feel == b.feel && a.fit == b.fit && a.fitProfile == b.fitProfile;
    }

    juce::uint64 randomSeed()
    {
        auto& r = juce::Random::getSystemRandom();
        return ((juce::uint64) (juce::uint32) r.nextInt() << 32) ^ (juce::uint64) (juce::uint32) r.nextInt()
             ^ (juce::uint64) juce::Time::getHighResolutionTicks();
    }

    inline void readFrame (const RenderResult& r, double index, float& l, float& rr) noexcept
    {
        const int len = r.audio.getNumSamples();
        const int i0 = (int) index;
        const float frac = (float) (index - i0);
        const int a = ((i0 % len) + len) % len;
        const int b = (a + 1) % len;
        const float* L = r.audio.getReadPointer (0);
        const float* R = r.audio.getReadPointer (1);
        l  = L[a] + (L[b] - L[a]) * frac;
        rr = R[a] + (R[b] - R[a]) * frac;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SliceTribeProcessor::createLayout()
{
    // named one by one: a "using namespace juce" here would clash with Apple's Carbon names
    using juce::AudioProcessorValueTreeState;
    using juce::AudioParameterFloat;
    using juce::AudioParameterFloatAttributes;
    using juce::AudioParameterChoice;
    using juce::AudioParameterBool;
    using juce::AudioParameterBoolAttributes;
    using juce::NormalisableRange;
    using juce::ParameterID;
    using juce::String;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                                      AudioParameterFloatAttributes().withLabel ("%")
                                                          .withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + "%"; }));
    };

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "length", 1 },    "Length (bars)", choices::lengths, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "pattern", 1 },   "Rhythm", choices::patterns, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "sliceMode", 1 }, "Slice mode", choices::sliceModes, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "sliceSize", 1 }, "Slice size", choices::sliceSizes, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "motif", 1 },     "Repeat", choices::motifs, 2));
    l.add (pct ("sensitivity", "Sensitivity", 50.0f));
    l.add (pct ("chaos",       "Chaos",        30.0f));
    l.add (pct ("variation",   "Variation",    15.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Gate", NormalisableRange<float> (10.0f, 100.0f, 0.1f), 100.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + "%"; })));
    l.add (pct ("swing",   "Swing",   0.0f));
    l.add (pct ("reverse", "Reverse", 0.0f));
    l.add (pct ("octave",  "Octave",  0.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "fade", 1 }, "Fade", NormalisableRange<float> (1.0f, 25.0f, 0.1f, 0.5f), 3.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ms"; })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", choices::styles, 0));
    l.add (pct ("amount", "Amount", 50.0f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "stretch", 1 }, "Stretch mode", choices::stretchModes, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key match", choices::keys(), 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Volume", NormalisableRange<float> (-36.0f, 12.0f, 0.1f, 1.6f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")
                                                      .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "trigger", 1 }, "New Loop", false,
                                                 AudioParameterBoolAttributes().withLabel ("trigger")));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "fill", 1 }, "Fill", choices::fills, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "feel", 1 }, "Time feel", choices::feels, 0));
    l.add (pct ("energy", "Energy", 0.0f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "midiMode", 1 }, "MIDI notes", choices::midiModes, 0));
    // finishing effects
    l.add (pct ("fxCutoff", "Cutoff", 100.0f));
    l.add (pct ("fxReso",   "Resonance", 0.0f));
    l.add (pct ("fxEnv",    "Filter envelope", 0.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "fxDecay", 1 }, "Envelope decay", NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.4f), 250.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + " ms"; })));
    l.add (pct ("fxPump",   "Pump", 0.0f));
    l.add (pct ("fxDrive",  "Drive", 0.0f));
    l.add (pct ("fxLowCut", "Low cut", 0.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "fxWidth", 1 }, "Width", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + "%"; })));
    return l;
}

bool SliceTribeProcessor::isStandalone() const
{
    return wrapperType == wrapperType_Standalone;
}

SliceTribeProcessor::SliceTribeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("ChupaLoops engine"),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();
    gainParam = apvts.getRawParameterValue ("gain");
    presets.onPresetLoaded = [this] (bool fromHost)
    {
        if (! fromHost && juce::MessageManager::existsAndIsCurrentThread())
            updateHostDisplay (ChangeDetails().withProgramChanged (true));
    };
    allParams = getParameters();
    for (auto& c : ccMap) c = -1;
    apvts.addParameterListener ("trigger", this);

    arrangement.seed = randomSeed();
    history.push_back ({ arrangement, {} });
    historyPos = 0;

    startThread (juce::Thread::Priority::normal);
    startTimerHz (30);
}

SliceTribeProcessor::~SliceTribeProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("trigger", this);
    abortWork = true;
    prepareInterrupt = true;
    signalThreadShouldExit();
    wake.signal();
    stopThread (10000);
    cancelPendingUpdate();   // after the worker is gone, so nothing can post another one
}

//==============================================================================
Settings SliceTribeProcessor::readSettings() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    Settings s;
    s.bars        = choices::lengthBars[juce::jlimit (0, 5, (int) get ("length"))];
    s.pattern     = juce::jlimit (0, choices::patterns.size() - 1, (int) get ("pattern"));
    s.sliceMode   = (int) get ("sliceMode");
    s.sliceSteps  = choices::sliceSteps[juce::jlimit (0, 5, (int) get ("sliceSize"))];
    s.motifBars   = choices::motifBars[juce::jlimit (0, 3, (int) get ("motif"))];
    s.sensitivity = get ("sensitivity") / 100.0f;
    s.chaos       = get ("chaos") / 100.0f;
    s.variation   = get ("variation") / 100.0f;
    s.gate        = get ("gate") / 100.0f;
    s.swing       = get ("swing") / 100.0f;
    s.reverse     = get ("reverse") / 100.0f;
    s.octave      = get ("octave") / 100.0f;
    s.fadeMs      = get ("fade");
    s.style       = juce::jlimit (0, 2, (int) get ("style"));
    s.amount      = get ("amount") / 100.0f;
    s.stretchMode = juce::jlimit (0, 1, (int) get ("stretch"));
    s.fillBars    = choices::fillBars[juce::jlimit (0, 4, (int) get ("fill"))];
    s.energy      = get ("energy") / 100.0f;
    s.feel        = juce::jlimit (0, 2, (int) get ("feel"));
    for (int tries = 0; tries < 4; ++tries)   // seqlock: never render half an old and half a new profile
    {
        const int v1 = fitVersion.load();
        if (v1 % 2 != 0)
            continue;
        s.fit = fitAmount.load();
        for (int i = 0; i < 16; ++i)
            s.fitProfile[(size_t) i] = fitProfile[(size_t) i].load();
        if (fitVersion.load() == v1)
            break;
    }
    return s;
}

FxChain::Params SliceTribeProcessor::readFx() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    FxChain::Params f;
    f.cutoff  = get ("fxCutoff") / 100.0f;
    f.reso    = get ("fxReso") / 100.0f;
    f.env     = get ("fxEnv") / 100.0f;
    f.decayMs = get ("fxDecay");
    f.pump    = get ("fxPump") / 100.0f;
    f.drive   = get ("fxDrive") / 100.0f;
    f.lowCut  = get ("fxLowCut") / 100.0f;
    f.width   = get ("fxWidth") / 100.0f;
    return f;
}

//==============================================================================
void SliceTribeProcessor::prepareToPlay (double sampleRate, int)
{
    currentRate = sampleRate;
    outGain.reset (sampleRate, 0.03);
    transportGain.reset (sampleRate, 0.002);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainParam->load(), -100.0f));
    crossfadeLength = juce::jmax (1, (int) (0.012 * sampleRate));
    offlineWaited = false;   // every (batch) export starts with a fresh wait for the final loop
    wasPlaying = false;
    fadingOut.reset();
    crossfadeLeft = 0;

    fx.prepare (sampleRate);
    // Keys mode: a real-time pitch shifter (all memory allocated here, never on the audio thread)
    keysStretch.configure (2, (int) (sampleRate * 0.08), (int) (sampleRate * 0.02));
    keysSeekLen = keysStretch.outputSeekLength (1.0f);
    for (int c = 0; c < 2; ++c)
    {
        keysIn[c].assign (1024, 0.0f);
        keysOut[c].assign (1024, 0.0f);
        keysSeek[c].assign ((size_t) keysSeekLen, 0.0f);
    }
    {
        float* seekPtrs[2] = { keysSeek[0].data(), keysSeek[1].data() };
        keysStretch.outputSeek (seekPtrs, keysSeekLen);   // warms up the internal buffers
        keysStretch.reset();
    }
    resetInstrument();
    wake.signal();
}

void SliceTribeProcessor::resetInstrument()
{
    voice = {};
    for (auto& v : fadeVoices) v = {};
    keysRunning = false; keysGain = 0.0f; keysHeldCount = 0; keysNote = -1; keysNextSeg = 0;
    fx.reset();
}

void SliceTribeProcessor::reset()
{
    offlineWaited = false;
    wasPlaying = false;
    fadingOut.reset();
    crossfadeLeft = 0;
    resetInstrument();
}

/** A new loop arrived while notes play: keep the notes going at the same place in the new loop. */
void SliceTribeProcessor::remapInstrument (const RenderResult& oldRes, const RenderResult& newRes)
{
    const double oldLen = oldRes.audio.getNumSamples(), newLen = newRes.audio.getNumSamples();
    if (oldLen < 2.0 || newLen < 2.0)
        return;
    const double ratio = newLen / oldLen;
    auto firstSegFrom = [&newRes] (double pos)
    {
        int k = 0;
        while (k < (int) newRes.segments.size() && (double) newRes.segments[(size_t) k].start < pos) ++k;
        return k;
    };
    auto remap = [&] (Voice& v)
    {
        if (! v.active) return;
        if (v.looping)
        {
            v.pos = std::fmod (v.pos * ratio, newLen);
            v.end = newLen;
            v.nextSeg = firstSegFrom (v.pos);
            return;
        }
        const int oldSeg = (int) std::distance (oldRes.segments.begin(),
                                                std::find_if (oldRes.segments.begin(), oldRes.segments.end(),
                                                              [&v] (const Segment& sg) { return v.pos >= (double) sg.start && v.pos < (double) (sg.start + sg.length) + 1.0; }));
        const int k = v.note - RenderResult::firstSliceNote;
        if (oldSeg >= (int) oldRes.segments.size() || ! juce::isPositiveAndBelow (k, (int) newRes.noteSegment.size()))
        {
            v.active = false;
            return;
        }
        const auto& ns = newRes.segments[(size_t) newRes.noteSegment[(size_t) k]];
        const double into = v.pos - (double) oldRes.segments[(size_t) oldSeg].start;
        v.pos = (double) ns.start + juce::jlimit (0.0, juce::jmax (0.0, (double) ns.length - 1.0), into);
        v.end = (double) (ns.start + ns.length);
    };
    remap (voice);
    for (auto& v : fadeVoices) remap (v);
    keysIndex = std::fmod (keysIndex * ratio, newLen);
    keysRead = std::fmod (keysRead * ratio, newLen);
    keysNextSeg = firstSegFrom (keysIndex);
}

bool SliceTribeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SliceTribeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int mode = juce::jlimit (0, 2, getMidiMode());
    numNoteEvents = 0;
    handleMidi (midi, mode, buffer.getNumSamples());
    midi.clear();

    const int numSamples = buffer.getNumSamples();
    const double rate = currentRate.load();
    if (rate <= 0.0 || numSamples <= 0 || buffer.getNumChannels() == 0)
        return;

    // ---- host transport ------------------------------------------------------
    double bpm = fallbackBpm.load();
    bool hostPlaying = false;
    std::optional<double> ppq;
    bool tempoFromHost = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0)
            {
                bpm = *b;
                tempoFromHost = true;
            }
            hostPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition())
                ppq = *p;
        }
    }
    hostProvidesTempo = tempoFromHost;
    if (std::abs (bpm - hostBpm.load()) > 1.0e-3)
    {
        hostBpm = bpm;
        upToDate = false;
    }

    // Offline bounce/export: the host does not need real time, so wait for the engine to finish
    // (time-stretching after a load or tempo change) instead of rendering silence.
    if (hostPlaying && preview.load())
        preview = false;   // the DAW took over: preview ends
    if (! hostPlaying)
        offlineWaited = false;
    const int keyNow = (int) apvts.getRawParameterValue ("key")->load() - 1;
    auto ready = [&]
    {
        if (! upToDate.load()) return false;
        const juce::SpinLock::ScopedLockType l (resultLock);
        return latestResult != nullptr && std::abs (latestResult->bpm - juce::jlimit (40.0, 300.0, bpm)) < 1.0e-3
               && std::abs (latestResult->rate - rate) < 0.5 && settingsEqual (latestResult->settings, readSettings())
               && latestResult->key == keyNow;
    };
    if (isNonRealtime() && hostPlaying && offlineWaited && playing != nullptr
        && (! settingsEqual (playing->settings, readSettings()) || playing->key != keyNow))
    {
        // automation changed a setting during the bounce: wait for that loop, so the file matches playback
        wake.signal();
        const auto start = juce::Time::getMillisecondCounter();
        while (! ready() && juce::Time::getMillisecondCounter() - start < 10000)
            juce::Thread::sleep (1);
    }
    if (isNonRealtime() && hostPlaying && ! offlineWaited)
    {
        offlineWaited = true;   // once per bounce start; tempo automation during a bounce is followed by varispeed
        wake.signal();
        const auto start = juce::Time::getMillisecondCounter();
        while (! ready() && juce::Time::getMillisecondCounter() - start < 60000)
            juce::Thread::sleep (2);
        transportGain.setCurrentAndTargetValue (1.0f);   // a bounce starts exactly on the downbeat
        {
            const juce::SpinLock::ScopedLockType l (resultLock);
            if (latestResult != nullptr)
                playing = latestResult;                      // start with the final loop, no crossfade from an old one
        }
        fadingOut.reset();
        crossfadeLeft = 0;
    }

    if (previewRestart.exchange (false))
        previewIndex = 0.0;

    // ---- pick up a freshly rendered loop --------------------------------------
    {
        const juce::SpinLock::ScopedTryLockType tl (resultLock);
        if (tl.isLocked() && latestResult != playing && latestResult != nullptr)
        {
            if (playing != nullptr && mode != 0)
                remapInstrument (*playing, *latestResult);   // notes keep playing in the new loop
            else if (playing != nullptr && transportGain.getCurrentValue() > 0.0f)
            {
                fadingOut = playing;
                fadeIndex = playIndex;
                crossfadeLeft = crossfadeLength;
            }
            playing = latestResult;
            const double newLen = playing->audio.getNumSamples();
            playIndex    = std::fmod (playIndex, newLen);
            previewIndex = std::fmod (previewIndex, newLen);
        }
    }

    if (hostPlaying && slotPreviewIndex.load() >= 0)
    {
        slotPreviewIndex = -1;   // the DAW plays: stop listening to a single sample
        slotPreviewVersion.fetch_add (1);
    }

    if (playing == nullptr || playing->audio.getNumSamples() < 2)
    {
        playPosition = -1.0;
        mixSlotPreview (buffer, rate);
        return;
    }

    const auto& res = *playing;
    const double len = res.audio.getNumSamples();
    const double step = (bpm / res.bpm) * (res.rate / rate);

    if (mode != lastMode)
    {
        resetInstrument();
        transportGain.setCurrentAndTargetValue (0.0f);
        fadingOut.reset();
        crossfadeLeft = 0;
        lastMode = mode;
    }
    if (mode != 0)
    {
        // Slices / Keys: the loop is played from MIDI notes instead of the transport
        renderInstrument (buffer, res, mode, bpm, rate, hostPlaying, ppq);
        mixSlotPreview (buffer, rate);
        return;
    }

    const bool previewing = preview.load() && ! hostPlaying;
    const bool active = hostPlaying || previewing;

    if (hostPlaying && ppq.has_value())
    {
        const double L = res.lengthBeats();
        double beats = std::fmod (*ppq, L);
        if (beats < 0.0) beats += L;
        const double target = beats * res.samplesPerBeat();

        // host jumped (loop / locate) while we were playing → short crossfade instead of a click
        if (wasPlaying && crossfadeLeft == 0)
        {
            double diff = std::abs (target - playIndex);
            diff = juce::jmin (diff, len - diff);
            if (diff > 64.0)
            {
                fadingOut = playing;
                fadeIndex = playIndex;
                crossfadeLeft = crossfadeLength;
            }
        }
        playIndex = target;
    }
    else if (previewing)
    {
        playIndex = previewIndex;
    }

    wasPlaying = active;
    transportGain.setTargetValue (active ? 1.0f : 0.0f);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load(), -100.0f));

    if (! active && ! transportGain.isSmoothing())
    {
        transportGain.setCurrentAndTargetValue (0.0f);
        playPosition = -1.0;
        fadingOut.reset();
        crossfadeLeft = 0;
        mixSlotPreview (buffer, rate);   // listening to a single sample still works with the transport stopped
        return;
    }

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const double fadeStep = fadingOut != nullptr ? (bpm / fadingOut->bpm) * (fadingOut->rate / rate) : 1.0;
    const double blockStartIndex = playIndex;

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = transportGain.getNextValue() * outGain.getNextValue();
        float l, r;
        readFrame (res, playIndex, l, r);

        if (crossfadeLeft > 0 && fadingOut != nullptr)
        {
            const float x = (float) crossfadeLeft / (float) crossfadeLength;   // 1 → 0
            float fl, fr;
            readFrame (*fadingOut, fadeIndex, fl, fr);
            l = l * (1.0f - x) + fl * x;
            r = r * (1.0f - x) + fr * x;
            fadeIndex += fadeStep;
            if (fadeIndex >= fadingOut->audio.getNumSamples())
                fadeIndex -= fadingOut->audio.getNumSamples();
            --crossfadeLeft;
        }

        if (outR != nullptr)
        {
            outL[i] = l * g;
            outR[i] = r * g;
        }
        else
        {
            outL[i] = 0.5f * (l + r) * g;
        }

        playIndex += step;
        if (playIndex >= len)
            playIndex -= len;
    }

    if (crossfadeLeft == 0)
        fadingOut.reset();

    if (previewing)
        previewIndex = playIndex;

    applyFx (buffer, res, blockStartIndex, step, numSamples);
    mixSlotPreview (buffer, rate);
    playPosition = playIndex / len;
}

//==============================================================================
// Finishing effects (all modes)
//==============================================================================
void SliceTribeProcessor::applyFx (juce::AudioBuffer<float>& buffer, const RenderResult& res, double startIndex, double step, int numSamples)
{
    const auto p = readFx();
    if (fx.isBypassed (p))
        return;
    const double len = res.audio.getNumSamples();
    const double spb = res.samplesPerBeat();
    // slices that start inside this block restart the filter envelope
    int n = 0;
    const double endIndex = startIndex + numSamples * step;
    for (const auto& seg : res.segments)
    {
        for (double base : { 0.0, len })   // the block may wrap around the loop end
        {
            const double s = (double) seg.start + base;
            if (s >= startIndex && s < endIndex && n < (int) fxSliceStarts.size())
                fxSliceStarts[(size_t) n++] = juce::jlimit (0, numSamples - 1, (int) ((s - startIndex) / step));
        }
    }
    std::sort (fxSliceStarts.begin(), fxSliceStarts.begin() + n);
    fx.process (buffer.getWritePointer (0), buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr, numSamples, p,
                startIndex / spb, step / spb, fxSliceStarts.data(), n);
}

//==============================================================================
// Slices mode (every note plays one slice, C1 the whole loop) and Keys mode (the loop, transposed)
//==============================================================================
void SliceTribeProcessor::renderInstrument (juce::AudioBuffer<float>& buffer, const RenderResult& res, int mode, double bpm, double rate,
                                            bool hostPlaying, std::optional<double> ppq)
{
    const int numSamples = buffer.getNumSamples();
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const double len = res.audio.getNumSamples();
    const double step = (bpm / res.bpm) * (res.rate / rate);
    const double spb = res.samplesPerBeat();
    const int numSegs = (int) res.segments.size();
    outGain.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load(), -100.0f));
    const float attack = (float) (1.0 / juce::jmax (1.0, rate * 0.002));
    const float fastFade = (float) (1.0 / juce::jmax (1.0, rate * 0.004));
    int sliceStarts = 0;
    int ev = 0;
    auto addSliceStart = [&] (int offset)
    {
        if (sliceStarts < (int) fxSliceStarts.size())
            fxSliceStarts[(size_t) sliceStarts++] = juce::jlimit (0, numSamples - 1, offset);
    };
    auto firstSegFrom = [&res, numSegs] (double pos)
    {
        int k = 0;
        while (k < numSegs && (double) res.segments[(size_t) k].start < pos) ++k;
        return k;
    };

    // beat position for the pump: the host's when it plays, otherwise free running
    double beatPos = hostPlaying && ppq.has_value() ? *ppq : freeBeat;
    const double beatsPerSample = bpm / (60.0 * rate);
    freeBeat = beatPos + numSamples * beatsPerSample;

    if (mode == 1)
    {
        auto startVoice = [&] (int note, float vel)
        {
            if (voice.active)
            {
                // the old note fades out in a free fade voice (the quietest one is reused)
                auto* slot = &fadeVoices[0];
                for (auto& fv : fadeVoices)
                    if (! fv.active || fv.gain < slot->gain) slot = &fv;
                *slot = voice;
                slot->releasing = true;
                slot->releaseStep = juce::jmax (slot->releaseStep, fastFade);
            }
            voice = {};
            voice.note = note;
            voice.vel = vel;
            if (note == RenderResult::firstSliceNote - 1)          // C1: the whole loop
            {
                voice.active = true; voice.looping = true; voice.pos = 0.0; voice.end = len;
                voice.nextSeg = firstSegFrom (0.5);                 // a slice at 0 is triggered by the note itself
            }
            else
            {
                const int k = note - RenderResult::firstSliceNote;   // C#1 and up: every different slice
                if (juce::isPositiveAndBelow (k, (int) res.noteSegment.size()))
                {
                    const auto& seg = res.segments[(size_t) res.noteSegment[(size_t) k]];
                    voice.active = true;
                    voice.pos = (double) seg.start;
                    voice.end = (double) (seg.start + seg.length);
                }
            }
        };

        auto runVoice = [&] (Voice& v, float& l, float& r, int i, bool main)
        {
            if (! v.active) return;
            float fl, fr;
            readFrame (res, v.pos, fl, fr);
            if (v.releasing)
            {
                v.gain -= v.releaseStep;
                if (v.gain <= 0.0f) { v.active = false; return; }
            }
            else if (v.gain < 1.0f)
                v.gain = juce::jmin (1.0f, v.gain + attack);
            l += fl * v.gain * v.vel;
            r += fr * v.gain * v.vel;
            v.pos += step;
            if (v.looping)
            {
                if (v.pos >= len) { v.pos -= len; v.nextSeg = 0; }
                // the whole loop: every slice it passes restarts the FX envelope, just like in Control mode
                if (main && v.nextSeg < numSegs && v.pos >= (double) res.segments[(size_t) v.nextSeg].start)
                {
                    addSliceStart (i);
                    ++v.nextSeg;
                }
            }
            else if (v.pos >= v.end - rate * 0.004 * step)
            {
                // the slice is over: 4 ms fade (also when a slower note-off release is running)
                v.releasing = true;
                v.releaseStep = juce::jmax (v.releaseStep, fastFade);
                if (v.pos >= v.end) v.active = false;
            }
        };

        for (int i = 0; i < numSamples; ++i)
        {
            while (ev < numNoteEvents && noteEvents[(size_t) ev].offset <= i)
            {
                const auto& e = noteEvents[(size_t) ev++];
                if (e.note < 0)                                      // all notes off
                {
                    if (voice.active) { voice.releasing = true; voice.releaseStep = juce::jmax (voice.releaseStep, fastFade); }
                }
                else if (e.on)
                {
                    startVoice (e.note, e.velocity);
                    addSliceStart (i);
                }
                else if (voice.active && voice.note == e.note && ! voice.releasing)
                {
                    voice.releasing = true;
                    voice.releaseStep = (float) (1.0 / juce::jmax (1.0, rate * 0.02));
                }
            }
            float l = 0.0f, r = 0.0f;
            for (auto& fv : fadeVoices)
                runVoice (fv, l, r, i, false);
            runVoice (voice, l, r, i, true);
            const float g = outGain.getNextValue();
            if (outR != nullptr) { outL[i] = l * g; outR[i] = r * g; }
            else                   outL[i] = 0.5f * (l + r) * g;
        }
        playPosition = voice.active ? voice.pos / len : -1.0;
    }
    else
    {
        // Keys: hold a note and the loop plays (in time with the song), transposed; C3 = original pitch
        auto syncedIndex = [&] (int offset)
        {
            if (hostPlaying && ppq.has_value())
            {
                const double L = res.lengthBeats();
                double beats = std::fmod (*ppq + offset * beatsPerSample, L);
                if (beats < 0.0) beats += L;
                return beats * spb;
            }
            return keysIndex;
        };
        auto seekTo = [&] (double index)
        {
            keysIndex = index;
            double rp = index;
            for (int j = 0; j < keysSeekLen; ++j)
            {
                float fl, fr;
                readFrame (res, rp, fl, fr);
                keysSeek[0][(size_t) j] = fl;
                keysSeek[1][(size_t) j] = fr;
                rp += step;
            }
            float* seekPtrs[2] = { keysSeek[0].data(), keysSeek[1].data() };
            keysStretch.outputSeek (seekPtrs, keysSeekLen);   // output starts exactly at `index`, no latency
            keysRead = std::fmod (rp, len);
            keysNextSeg = firstSegFrom (index + 0.5);
            keysRunning = true;
        };

        // the host jumped (loop, locate): follow it
        if (keysRunning && keysHeldCount > 0 && hostPlaying && ppq.has_value())
        {
            const double target = syncedIndex (0);
            double diff = std::abs (target - keysIndex);
            diff = juce::jmin (diff, len - diff);
            if (diff > 1024.0 * step)
                seekTo (target);
        }

        int done = 0;
        while (done < numSamples)
        {
            // note events up to here
            while (ev < numNoteEvents && noteEvents[(size_t) ev].offset <= done)
            {
                const auto& e = noteEvents[(size_t) ev++];
                if (e.note < 0)
                {
                    keysHeldCount = 0;   // all notes off
                }
                else if (e.on)
                {
                    for (int k = 0; k < keysHeldCount; ++k)   // the same key again: no double entry
                        if (keysHeld[(size_t) k] == e.note)
                        {
                            keysHeld[(size_t) k] = keysHeld[(size_t) --keysHeldCount];
                            break;
                        }
                    if (keysHeldCount < (int) keysHeld.size()) keysHeld[(size_t) keysHeldCount++] = e.note;
                    keysStretch.setTransposeSemitones ((float) juce::jlimit (-24, 24, e.note - 60));
                    if (! keysRunning || keysGain <= 0.0f)
                        seekTo (hostPlaying ? syncedIndex (done) : 0.0);
                    keysNote = e.note;
                    keysVel = e.velocity;
                    addSliceStart (done);
                }
                else
                {
                    for (int k = 0; k < keysHeldCount; ++k)
                        if (keysHeld[(size_t) k] == e.note)
                        {
                            keysHeld[(size_t) k] = keysHeld[(size_t) --keysHeldCount];
                            break;
                        }
                    if (keysHeldCount > 0)
                    {
                        keysNote = keysHeld[(size_t) keysHeldCount - 1];   // last held note again (legato)
                        keysStretch.setTransposeSemitones ((float) juce::jlimit (-24, 24, keysNote - 60));
                    }
                }
            }
            const int nextEvent = ev < numNoteEvents ? juce::jmax (done + 1, noteEvents[(size_t) ev].offset) : numSamples;
            const int n = juce::jmin (numSamples, nextEvent, done + 1024) - done;

            if (keysRunning)
            {
                for (int j = 0; j < n; ++j)
                {
                    float fl, fr;
                    readFrame (res, keysRead, fl, fr);
                    keysIn[0][(size_t) j] = fl;
                    keysIn[1][(size_t) j] = fr;
                    keysRead += step;
                    if (keysRead >= len) keysRead -= len;
                }
                float* ins[2] = { keysIn[0].data(), keysIn[1].data() };
                float* outs[2] = { keysOut[0].data(), keysOut[1].data() };
                keysStretch.process (ins, n, outs, n);
                const float rel = (float) (1.0 / juce::jmax (1.0, rate * 0.04));
                for (int j = 0; j < n; ++j)
                {
                    keysGain = keysHeldCount > 0 ? juce::jmin (1.0f, keysGain + attack) : keysGain - rel;
                    if (keysGain <= 0.0f) { keysGain = 0.0f; keysRunning = keysHeldCount > 0; }
                    const float g = keysGain * keysVel * outGain.getNextValue();
                    if (outR != nullptr) { outL[done + j] = keysOut[0][(size_t) j] * g; outR[done + j] = keysOut[1][(size_t) j] * g; }
                    else                   outL[done + j] = 0.5f * (keysOut[0][(size_t) j] + keysOut[1][(size_t) j]) * g;

                    // slices passing by restart the FX envelope
                    keysIndex += step;
                    if (keysIndex >= len) { keysIndex -= len; keysNextSeg = 0; }
                    if (keysNextSeg < numSegs && keysIndex >= (double) res.segments[(size_t) keysNextSeg].start)
                    {
                        addSliceStart (done + j);
                        ++keysNextSeg;
                    }
                }
            }
            else
            {
                for (int j = 0; j < n; ++j) outGain.getNextValue();
                keysIndex = std::fmod (keysIndex + n * step, len);
                keysNextSeg = firstSegFrom (keysIndex);
            }
            done += n;
        }
        playPosition = keysRunning ? keysIndex / len : -1.0;
    }

    const auto p = readFx();
    if (! fx.isBypassed (p))
    {
        std::sort (fxSliceStarts.begin(), fxSliceStarts.begin() + sliceStarts);
        fx.process (outL, outR, numSamples, p, beatPos, beatsPerSample, fxSliceStarts.data(), sliceStarts);
    }
}

//==============================================================================
// Worker thread: loading, time-stretching and rendering
//==============================================================================
void SliceTribeProcessor::run()
{
    int lastVersion = -1;
    Settings lastSettings;
    bool haveLast = false;
    double lastBpm = 0.0, lastRate = 0.0;
    double seenBpm = 0.0;
    juce::uint32 bpmStableSince = 0;
    Settings lastStructure;
    bool haveStructure = false;

    while (! threadShouldExit() && ! abortWork.load())
    {
        wake.wait (40);
        if (threadShouldExit() || abortWork.load())
            break;

        bool changed = false;

        // ---- 1. load files (also before the host has started audio) ----------------
        for (int i = 0; i < kNumSlots && ! abortWork.load(); ++i)
        {
            PendingLoad job;
            {
                const juce::ScopedLock sl (slotLock);
                if (! pending[(size_t) i].active)
                    continue;
                job = pending[(size_t) i];   // keep the pending entry until committed (so saving still sees it)
            }
            busy = true;
            upToDate = false;

            SlotAudio a;
            bool fromEmbedded = false;
            if (job.file.existsAsFile())
                a = engine::loadFile (formatManager, job.file);

            if (a.original == nullptr && job.embedded.getSize() > 0)
            {
                a = decodeFlac (job.embedded);
                a.name = job.name.isNotEmpty() ? job.name : job.file.getFileNameWithoutExtension();
                a.detectedKey = job.key >= -1 ? job.key : engine::detectKey (a.name);
                if (job.key < -1 && a.detectedKey < 0 && a.original != nullptr && ! engine::nameSuggestsDrums (a.name))
                    a.detectedKey = engine::detectKeyFromAudio (*a.original, a.fileRate);
                a.path = job.file.getFullPathName();
                fromEmbedded = true;
            }

            juce::MemoryBlock emb;
            if (a.original != nullptr)
            {
                if (a.name.isEmpty()) a.name = "Sample " + juce::String (i + 1);
                const double seconds = a.original->getNumSamples() / a.fileRate;
                a.detectedBpm = job.detectedBpm > 0.0 ? job.detectedBpm
                                                      : engine::detectBpm (a.name, seconds, juce::jlimit (40.0, 300.0, hostBpm.load()),
                                                                          a.original.get(), a.fileRate);
                a.gridProfile = engine::gridProfile (*a.original, a.fileRate,                   // for FIT TO TRACK
                                                    job.state.bpmOverride > 0.0 ? job.state.bpmOverride : a.detectedBpm);
                if (fromEmbedded)
                    emb = job.embedded;
                else if (seconds <= maxEmbedSeconds)
                    emb = encodeFlac (*a.original, a.fileRate);
            }

            bool regrid = false;
            {
                const juce::ScopedLock sl (slotLock);
                if (slotGeneration[(size_t) i] != job.generation)
                    continue;   // slot was cleared or got a newer file meanwhile – drop this result
                // anything you changed while the file was loading (FIT, share, tempo, transpose, on/off)
                // lives in the pending state, not in the copy this job started with
                const SlotState live = pending[(size_t) i].active ? pending[(size_t) i].state : job.state;
                regrid = live.reference && a.original != nullptr
                      && std::abs (live.bpmOverride - job.state.bpmOverride) > 1.0e-6;
                pending[(size_t) i] = {};
                a.loadId = nextLoadId++;
                const bool missing = a.original == nullptr && job.file != juce::File();
                slotMissing[(size_t) i] = missing;
                slotError[(size_t) i] = missing && job.file.existsAsFile();   // there, but not readable
                missingFile[(size_t) i] = missing ? job.file : juce::File();
                slotAudio[(size_t) i] = std::move (a);
                slotState[(size_t) i] = live;
                embeddedAudio[(size_t) i] = std::move (emb);
                if (regrid)   // a tempo you typed during the load: the fit grid follows it
                {
                    auto& na = slotAudio[(size_t) i];
                    na.gridProfile = engine::gridProfile (*na.original, na.fileRate,
                                                          live.bpmOverride > 0.0 ? live.bpmOverride : na.detectedBpm);
                }
            }
            slotsVersion.fetch_add (1);
            updateFitFromSlots();   // a new sample can be (or replace) the reference track
            changed = true;
        }

        const double rate = currentRate.load();
        const double bpm  = juce::jlimit (40.0, 300.0, hostBpm.load());
        if (rate <= 0.0)
        {
            busy = false;
            continue;   // host has not started audio yet
        }

        // ---- 2. prepare (tempo / rate / transpose) ------------------------------
        const Settings s = readSettings();
        const bool wantOctave = (s.octave > 0.0005f || s.energy > 0.001f);
        const int keyTarget = (int) apvts.getRawParameterValue ("key")->load() - 1;   // -1 = off

        if (std::abs (bpm - seenBpm) > 1.0e-6)
        {
            seenBpm = bpm;
            bpmStableSince = juce::Time::getMillisecondCounter();
        }
        const bool bpmStable = isNonRealtime() || juce::Time::getMillisecondCounter() - bpmStableSince > 250;

        bool allPrepared = true;
        for (int i = 0; i < kNumSlots && ! abortWork.load(); ++i)
        {
            SlotAudio a;
            SlotState st;
            {
                const juce::ScopedLock sl (slotLock);
                a  = slotAudio[(size_t) i];
                st = slotState[(size_t) i];
            }
            auto& p = prepared[(size_t) i];

            if (a.original == nullptr)
            {
                if (p.audio != nullptr) { p = {}; changed = true; }
                continue;
            }
            if (std::abs (p.weight - st.weight) > 1.0e-6f)   // the share needs no new preparation
            {
                p.weight = st.weight;
                changed = true;
            }

            // "time feel": half time = treat the sample as twice as fast, double time = twice as slow
            const double feel = choices::feelFactor[juce::jlimit (0, 2, s.feel)];
            const double baseBpm = st.bpmOverride > 0.0 ? st.bpmOverride : (a.detectedBpm > 0.0 ? a.detectedBpm : bpm);
            const double srcBpm = baseBpm * feel;
            st.bpmOverride = srcBpm;
            const int effT = juce::jlimit (-24, 24, st.transpose + (keyTarget >= 0 ? engine::keyShift (a.detectedKey, keyTarget) : 0));
            const bool need = p.loadId != a.loadId || p.rate != rate || p.transpose != effT || p.mode != s.stretchMode
                           || (s.stretchMode == stretchSmooth && p.hostBpm != bpm)
                           || std::abs (p.srcBpm - srcBpm) > 1.0e-6 || (wantOctave && ! p.withOctave);

            if (need)
            {
                if (! bpmStable && p.loadId == a.loadId)
                {
                    allPrepared = false;   // tempo is still moving – keep playing the old version for now
                    continue;
                }
                busy = true;
                upToDate = false;
                prepareInterrupt = abortWork.load();
                auto np = engine::prepare (a, st, effT, bpm, rate, wantOctave, s.stretchMode, &prepareInterrupt);
                if (abortWork.load())
                    break;
                if (prepareInterrupt.exchange (false))
                {
                    allPrepared = false;   // the slot changed while preparing: start again with the new state
                    break;
                }
                np.loadId = a.loadId;
                if (np.audio != nullptr)
                {
                    np.onsets = engine::detectOnsets (*np.audio, np.beatLen, s.sensitivity);
                    np.warpOnsets = engine::detectOnsets (*np.audio, np.beatLen, 0.6f);
                }
                np.onsetSensitivity = s.sensitivity;
                p = std::move (np);
                changed = true;
            }
            else if (s.sliceMode == 1 && p.onsetSensitivity != s.sensitivity && p.audio != nullptr)
            {
                p.onsets = engine::detectOnsets (*p.audio, p.beatLen, s.sensitivity);
                p.onsetSensitivity = s.sensitivity;
                changed = true;
            }
        }

        if (abortWork.load())
            break;

        bool anyPending = false;
        {
            const juce::ScopedLock sl (slotLock);
            for (auto& pl : pending) anyPending |= pl.active;
        }

        if (! allPrepared || anyPending)
        {
            // keep playing the current loop until every slot that is loading is ready (no silent gap)
            busy = anyPending;
            upToDate = false;
            continue;
        }

        // ---- 3. render ----------------------------------------------------------
        if (renderHold.load())
        {
            upToDate = false;   // an arrangement and its settings are being swapped in: wait for both
            continue;
        }
        const int version = updateVersion.load();
        if (changed || version != lastVersion || ! haveLast || ! settingsEqual (s, lastSettings) || bpm != lastBpm || rate != lastRate)
        {
            busy = true;
            Settings rs = s;
            while (rs.bars > 1 && rs.bars * 240.0 / bpm > 130.0)   // keep memory sane at very slow tempos
                rs.bars /= 2;

            Arrangement arr;
            std::vector<Hit> hits;
            {
                const juce::ScopedLock al (arrangementLock);
                hits = engine::buildHits (rs, arrangement.seed);
                const bool keepLocks = keepLocksOnce.exchange (false);
                const bool structureChanged = haveStructure && ! rs.structureEquals (lastStructure) && ! keepLocks;
                if (structureChanged || arrangement.hitSeeds.size() != hits.size())
                {
                    if (structureChanged)
                    {
                        std::fill (arrangement.locked.begin(), arrangement.locked.end(), (juce::uint8) 0);
                        std::fill (arrangement.forceOwn.begin(), arrangement.forceOwn.end(), (juce::uint8) 0);
                    }
                    arrangement.resizeFor (hits.size());
                }
                lastStructure = rs;
                haveStructure = true;
                arr = arrangement;
                lockedCount = arrangement.numLocked();
            }

            std::array<bool, kNumSlots> enabled {};
            {
                const juce::ScopedLock sl (slotLock);
                for (int i = 0; i < kNumSlots; ++i)
                    enabled[(size_t) i] = slotState[(size_t) i].enabled && ! slotState[(size_t) i].reference
                                       && slotAudio[(size_t) i].original != nullptr;
            }

            auto rendered = engine::render (prepared, enabled, rs, arr, hits, bpm, rate);
            rendered->settings = s;   // the settings as the user set them (before any slow-tempo clamp)
            rendered->key = keyTarget;
            publishResult (rendered);

            lastVersion = version;
            lastSettings = s;
            haveLast = true;
            lastBpm = bpm;
            lastRate = rate;
        }

        // ---- 4. background jobs that need the prepared samples ------------------
        if (autoPickRequest.load() > 0 || stemsRequest.load())
        {
            Settings rs = s;
            while (rs.bars > 1 && rs.bars * 240.0 / bpm > 130.0)
                rs.bars /= 2;
            std::array<bool, kNumSlots> enabled {};
            {
                const juce::ScopedLock sl (slotLock);
                for (int i = 0; i < kNumSlots; ++i)
                    enabled[(size_t) i] = slotState[(size_t) i].enabled && ! slotState[(size_t) i].reference
                                       && slotAudio[(size_t) i].original != nullptr;
            }
            busy = true;
            if (const int n = autoPickRequest.exchange (0); n > 0)
                runAutoPick (enabled, rs, bpm, rate, n);
            if (stemsRequest.exchange (false))
                runStems (enabled, rs, bpm, rate);
            jobBusy = autoPickRequest.load() > 0 || stemsRequest.load();
        }

        busy = false;
        upToDate = ! anyPending && updateVersion.load() == lastVersion;
    }
}

/** Makes a few loops, scores them and keeps the best one. */
void SliceTribeProcessor::runAutoPick (const std::array<bool, kNumSlots>& enabled, const Settings& rs,
                                       double bpm, double rate, int candidates)
{
    Arrangement base;
    const int startGenerate = generateCount.load();
    const int startVersion = updateVersion.load();
    {
        const juce::ScopedLock al (arrangementLock);
        base = arrangement;
    }
    const auto hits = engine::buildHits (rs, base.seed);
    Arrangement best = base;
    std::shared_ptr<RenderResult> bestResult;
    double bestScore = -1.0;

    for (int k = 0; k < candidates && ! abortWork.load() && ! threadShouldExit(); ++k)
    {
        Arrangement cand = base;
        if (k > 0)
            cand.regenerate (randomSeed());                    // the first candidate is the loop you have now
        auto hitsK = engine::buildHits (rs, cand.seed);
        cand.resizeFor (hitsK.size());
        auto r = engine::render (prepared, enabled, rs, cand, hitsK, bpm, rate);
        const double sc = engine::scoreLoop (*r, 1.0 - 0.45 * juce::jlimit (0.0f, 1.0f, rs.fit));   // fitting leaves holes on purpose
        if (sc > bestScore)
        {
            bestScore = sc;
            best = cand;
            bestResult = r;
        }
    }
    if (bestResult == nullptr || renderHold.load()
        || generateCount.load() != startGenerate || updateVersion.load() != startVersion)
        return;   // the user made another loop meanwhile: leave that one alone

    {
        const juce::ScopedLock al (arrangementLock);
        arrangement = best;
        history.resize ((size_t) historyPos + 1);
        history.push_back ({ arrangement, currentParamValues() });
        if (history.size() > 200)
            history.erase (history.begin());
        historyPos = (int) history.size() - 1;
        historyPosAtomic = historyPos;
        historySizeAtomic = (int) history.size();
        lockedCount = arrangement.numLocked();
    }
    bestResult->settings = readSettings();
    bestResult->key = (int) apvts.getRawParameterValue ("key")->load() - 1;
    publishResult (bestResult);
    generateCount.fetch_add (1);
    activeScene = -1;
    scenesVersion.fetch_add (1);
    {
        const juce::ScopedLock jl (jobLock);
        jobError = false;
        jobMessage = "AUTO PICK: best of " + juce::String (candidates) + " (score " + juce::String (juce::roundToInt (bestScore * 100)) + "%)";
    }
    jobVersion.fetch_add (1);
}

/** Writes one WAV per sample: the same loop, but only the slices of that sample. */
void SliceTribeProcessor::runStems (const std::array<bool, kNumSlots>& enabled, const Settings& rs, double bpm, double rate)
{
    juce::File folder;
    { const juce::ScopedLock jl (jobLock); folder = stemsFolder; }
    if (folder == juce::File())
        return;
    Arrangement arr;
    {
        const juce::ScopedLock al (arrangementLock);
        arr = arrangement;
    }
    const auto hits = engine::buildHits (rs, arr.seed);
    const float gain = juce::Decibels::decibelsToGain (gainParam->load(), -100.0f);
    juce::String names;
    int written = 0;
    for (int i = 0; i < kNumSlots && ! abortWork.load() && ! threadShouldExit(); ++i)
    {
        if (! enabled[(size_t) i] || prepared[(size_t) i].audio == nullptr)
            continue;
        auto r = engine::render (prepared, enabled, rs, arr, hits, bpm, rate, i);
        if (r == nullptr || r->audio.getNumSamples() == 0)
            continue;
        juce::String name;
        {
            const juce::ScopedLock sl (slotLock);
            name = slotAudio[(size_t) i].name;
        }
        if (name.isEmpty()) name = "Sample " + juce::String (i + 1);
        auto file = folder.getChildFile (juce::File::createLegalFileName (juce::String (i + 1) + " - " + name) + ".wav");
        if (engine::writeWav (file, r->audio, r->rate, gain))
            ++written;
    }
    {
        const juce::ScopedLock jl (jobLock);
        jobMessage = written > 0 ? "Stems saved: " + juce::String (written) + " WAVs in " + folder.getFileName()
                                 : juce::String ("Saving the stems failed - check the folder permissions");
        jobError = written == 0;
    }
    jobVersion.fetch_add (1);
}

juce::String SliceTribeProcessor::getJobMessage() const
{
    const juce::ScopedLock jl (jobLock);
    return jobMessage;
}

juce::File SliceTribeProcessor::exportStems (const juce::File& parentFolder)
{
    if (! hasLoop())
        return {};
    auto kit = parentFolder.getNonexistentChildFile (suggestedExportName() + " stems", {}, true);
    if (! kit.createDirectory())
        return {};
    if (jobBusy.exchange (true))
        return {};                    // a job is already running
    { const juce::ScopedLock jl (jobLock); stemsFolder = kit; }
    stemsRequest = true;
    wake.signal();
    return kit;
}

void SliceTribeProcessor::autoPick (int candidates)
{
    if (jobBusy.exchange (true))
        return;                       // a job is already running
    autoPickRequest = juce::jlimit (2, 32, candidates);
    wake.signal();
}

void SliceTribeProcessor::rerollRhythm()
{
    generateCount.fetch_add (1);
    const auto now = currentParamValues();
    {
        const juce::ScopedLock al (arrangementLock);
        history[(size_t) historyPos] = { arrangement, now };
        history.resize ((size_t) historyPos + 1);
        arrangement.regenerateRhythm (randomSeed());
        history.push_back ({ arrangement, now });
        if (history.size() > 200) history.erase (history.begin());
        historyPos = (int) history.size() - 1;
        historyPosAtomic = historyPos;
        historySizeAtomic = (int) history.size();
        keepLocksOnce = true;
    }
    activeScene = -1;
    scenesVersion.fetch_add (1);
    requestUpdate();
}

void SliceTribeProcessor::rerollSources()
{
    generateCount.fetch_add (1);
    const auto now = currentParamValues();
    {
        const juce::ScopedLock al (arrangementLock);
        history[(size_t) historyPos] = { arrangement, now };
        history.resize ((size_t) historyPos + 1);
        arrangement.regenerateSources (randomSeed());
        history.push_back ({ arrangement, now });
        if (history.size() > 200) history.erase (history.begin());
        historyPos = (int) history.size() - 1;
        historyPosAtomic = historyPos;
        historySizeAtomic = (int) history.size();
        lockedCount = arrangement.numLocked();
        keepLocksOnce = true;
    }
    activeScene = -1;
    scenesVersion.fetch_add (1);
    requestUpdate();
}

int SliceTribeProcessor::keepToScene()
{
    for (int i = 0; i < numScenes; ++i)
        if (! isSceneUsed (i))
        {
            storeScene (i);
            return i;
        }
    return -1;
}

void SliceTribeProcessor::publishResult (std::shared_ptr<RenderResult> r)
{
    {
        const juce::SpinLock::ScopedLockType l (resultLock);
        latestResult = r;
    }
    keepAlive.push_back (r);

    // free old results only when nobody else (audio thread, UI) holds them
    for (auto it = keepAlive.begin(); it != keepAlive.end() && keepAlive.size() > 1;)
    {
        if (*it != r && it->use_count() == 1)
            it = keepAlive.erase (it);
        else
            ++it;
    }
    resultVersion.fetch_add (1);
}

std::shared_ptr<RenderResult> SliceTribeProcessor::getDisplayResult() const
{
    const juce::SpinLock::ScopedLockType l (resultLock);
    return latestResult;
}

//==============================================================================
// Slots
//==============================================================================
SlotInfo SliceTribeProcessor::getSlotInfo (int i) const
{
    SlotInfo info;
    const juce::ScopedLock sl (slotLock);
    const auto& a = slotAudio[(size_t) i];
    const auto& st = slotState[(size_t) i];
    info.loaded = a.original != nullptr;
    info.loading = pending[(size_t) i].active;
    info.missing = slotMissing[(size_t) i] && ! slotError[(size_t) i];
    info.error = slotError[(size_t) i];
    info.key = a.detectedKey;
    {
        const int keyTarget = (int) apvts.getRawParameterValue ("key")->load() - 1;
        info.keyShift = keyTarget >= 0 ? engine::keyShift (a.detectedKey, keyTarget) : 0;
    }
    info.enabled = st.enabled;
    info.weight = st.weight;
    info.reference = st.reference;
    info.name = a.name.isNotEmpty() ? a.name
              : pending[(size_t) i].active ? pending[(size_t) i].file.getFileNameWithoutExtension()
              : missingFile[(size_t) i].getFileNameWithoutExtension();
    info.path = a.original != nullptr ? a.path : missingFile[(size_t) i].getFullPathName();
    info.detectedBpm = a.detectedBpm;
    info.bpmOverride = st.bpmOverride;
    info.transpose = st.transpose;
    info.peaks = a.peaks;
    const double feel = choices::feelFactor[juce::jlimit (0, 2, (int) apvts.getRawParameterValue ("feel")->load())];
    const double src = (st.bpmOverride > 0 ? st.bpmOverride : a.detectedBpm) * feel;
    info.stretchRatio = src > 0 ? hostBpm.load() / src : 1.0;
    return info;
}

void SliceTribeProcessor::queueLoad (int slot, const juce::File& f, const juce::MemoryBlock* embedded, SlotState state, double detectedBpm,
                                     const juce::String& name, int key)
{
    state.transpose = juce::jlimit (-12, 12, state.transpose);
    state.bpmOverride = state.bpmOverride > 0.0 ? juce::jlimit (40.0, 300.0, state.bpmOverride) : 0.0;
    {
        const juce::ScopedLock sl (slotLock);
        auto& p = pending[(size_t) slot];
        p.active = true;
        p.file = f;
        p.embedded = embedded != nullptr ? *embedded : juce::MemoryBlock();
        p.state = state;
        p.detectedBpm = detectedBpm >= 40.0 && detectedBpm <= 300.0 ? detectedBpm : 0.0;
        p.name = name;
        p.key = key >= -1 && key < 24 ? key : -2;
        p.generation = ++slotGeneration[(size_t) slot];
    }
    prepareInterrupt = true;
    upToDate = false;
    slotsVersion.fetch_add (1);
    requestUpdate();
}

void SliceTribeProcessor::loadSlot (int slot, const juce::File& f)
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return;
    if (slotPreviewIndex.load() == slot)
        setSlotPreview (-1);
    SlotState st;
    bool droppedReference = false;
    {
        // locating a missing file keeps the slot's tempo, transpose and on/off settings
        const juce::ScopedLock sl (slotLock);
        if (slotMissing[(size_t) slot])
            st = slotState[(size_t) slot];
        else
            droppedReference = releaseReference (slot);    // another sample: it is not your track any more
    }
    if (droppedReference)
    {
        restoreKeyAfterReference();
        updateFitFromSlots();
    }
    queueLoad (slot, f, nullptr, st);
}

void SliceTribeProcessor::setSlotPreview (int slot)
{
    if (slot == slotPreviewIndex.load())
        slot = -1;                                  // clicking the playing slot again stops it
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double fileRate = 44100.0;
    if (juce::isPositiveAndBelow (slot, kNumSlots))
    {
        const juce::ScopedLock sl (slotLock);
        audio = slotAudio[(size_t) slot].original;
        fileRate = slotAudio[(size_t) slot].fileRate;
        if (audio == nullptr || audio->getNumSamples() < 2)
            slot = -1;
    }
    else
        slot = -1;
    if (slot >= 0)
    {
        // keep the previous buffers alive here, so the audio thread never frees one
        slotPreviewKeep.push_back (audio);
        for (auto it = slotPreviewKeep.begin(); it != slotPreviewKeep.end();)
            it = (*it != audio && it->use_count() == 1) ? slotPreviewKeep.erase (it) : it + 1;
        const juce::SpinLock::ScopedLockType l (slotPreviewLock);
        slotPreviewAudio = audio;
        slotPreviewRate = fileRate;
    }
    // on stop the audio keeps standing: the audio thread fades out and then simply stops reading
    if (slot >= 0)
        preview = false;                            // the loop preview and a single sample never play together
    slotPreviewIndex = slot;
    slotPreviewVersion.fetch_add (1);
}

/** Plays the chosen sample on top of everything else, at its own tempo, looping. */
void SliceTribeProcessor::mixSlotPreview (juce::AudioBuffer<float>& buffer, double rate)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() == 0 || rate <= 0.0)
        return;

    if (const int v = slotPreviewVersion.load(); v != slotPlayVersion)
    {
        const juce::SpinLock::ScopedTryLockType tl (slotPreviewLock);
        if (tl.isLocked())
        {
            slotPlayVersion = v;
            auto next = slotPreviewAudio;
            if (next != slotPlaying)
            {
                slotPlaying = next;
                slotPlayPos = 0.0;
                slotPlayGain = 0.0f;
            }

        }
    }
    if (slotPlaying == nullptr || slotPlaying->getNumSamples() < 2)
        return;
    slotPlayStep = juce::jlimit (0.01, 8.0, slotPreviewRate.load() / rate);   // follows a sample-rate change

    const bool on = slotPreviewIndex.load() >= 0;
    const auto& b = *slotPlaying;
    const int len = b.getNumSamples();
    const float* inL = b.getReadPointer (0);
    const float* inR = b.getReadPointer (b.getNumChannels() > 1 ? 1 : 0);
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float gain = juce::Decibels::decibelsToGain (gainParam->load(), -100.0f);
    const float ramp = (float) (1.0 / juce::jmax (1.0, rate * 0.004));   // 4 ms fade in / out

    for (int i = 0; i < numSamples; ++i)
    {
        slotPlayGain = on ? juce::jmin (1.0f, slotPlayGain + ramp) : juce::jmax (0.0f, slotPlayGain - ramp);
        if (slotPlayGain <= 0.0f && ! on)
            return;   // faded out: stop reading (freeing the audio is the message thread's job)
        const int i0 = (int) juce::jlimit (0.0, (double) (len - 1), slotPlayPos);
        const int i1 = i0 + 1 < len ? i0 + 1 : 0;
        const float f = (float) (slotPlayPos - i0);
        const float g = slotPlayGain * gain;
        outL[i] += (inL[i0] + (inL[i1] - inL[i0]) * f) * g;
        if (outR != nullptr)
            outR[i] += (inR[i0] + (inR[i1] - inR[i0]) * f) * g;
        slotPlayPos += slotPlayStep;
        if (slotPlayPos >= len)
            slotPlayPos -= len;
    }
}

void SliceTribeProcessor::clearSlot (int slot)
{
    if (slotPreviewIndex.load() == slot)
        setSlotPreview (-1);
    bool wasReference = false;
    {
        const juce::ScopedLock sl (slotLock);
        wasReference = slotState[(size_t) slot].reference
                    || (pending[(size_t) slot].active && pending[(size_t) slot].state.reference);
        slotAudio[(size_t) slot] = {};
        slotState[(size_t) slot] = {};
        pending[(size_t) slot] = {};
        slotMissing[(size_t) slot] = false;
        missingFile[(size_t) slot] = juce::File();
        embeddedAudio[(size_t) slot].reset();
        ++slotGeneration[(size_t) slot];
        slotError[(size_t) slot] = false;
    }
    prepareInterrupt = true;
    if (wasReference)
        restoreKeyAfterReference();   // the track is gone, so KEY goes back to what you had
    updateFitFromSlots();
    slotsVersion.fetch_add (1);
    requestUpdate();
}

void SliceTribeProcessor::setSlotEnabled (int slot, bool e)
{
    { const juce::ScopedLock sl (slotLock); editSlotState (slot, [e] (SlotState& st) { st.enabled = e; }); }
    slotsVersion.fetch_add (1);
    requestUpdate();
}

void SliceTribeProcessor::setSlotBpm (int slot, double bpm)
{
    {
        const juce::ScopedLock sl (slotLock);
        editSlotState (slot, [bpm] (SlotState& s2) { s2.bpmOverride = bpm > 0 ? juce::jlimit (40.0, 300.0, bpm) : 0.0; });
        auto& st = slotState[(size_t) slot];
        if (st.reference && slotAudio[(size_t) slot].original != nullptr)   // the fit grid follows the corrected tempo
        {
            auto& a = slotAudio[(size_t) slot];
            a.gridProfile = engine::gridProfile (*a.original, a.fileRate, st.bpmOverride > 0.0 ? st.bpmOverride : a.detectedBpm);
        }
    }
    updateFitFromSlots();
    slotsVersion.fetch_add (1);
    requestUpdate();
}

void SliceTribeProcessor::setSlotTranspose (int slot, int semis)
{
    { const juce::ScopedLock sl (slotLock); editSlotState (slot, [semis] (SlotState& st) { st.transpose = juce::jlimit (-12, 12, semis); }); }
    slotsVersion.fetch_add (1);
    requestUpdate();
}

void SliceTribeProcessor::setSlotWeight (int slot, float w)
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return;
    { const juce::ScopedLock sl (slotLock); editSlotState (slot, [w] (SlotState& st) { st.weight = juce::jlimit (0.0f, 2.0f, w); }); }
    updateFitFromSlots();
    slotsVersion.fetch_add (1);
    requestUpdate();
}

/** Collects the reference slot's profile into the lock-free copy the render settings read. */
void SliceTribeProcessor::updateFitFromSlots()
{
    int ref = -1;
    {
    const juce::ScopedLock wl (fitWriteLock);   // worker and message thread both call this
    float amount = 0.0f;
    std::array<float, 16> profile {};
    {
        const juce::ScopedLock sl (slotLock);
        for (int i = 0; i < kNumSlots; ++i)
            if (slotState[(size_t) i].reference && slotAudio[(size_t) i].original != nullptr)
            {
                ref = i;
                amount = slotState[(size_t) i].weight;
                profile = slotAudio[(size_t) i].gridProfile;
                break;
            }
    }
    fitVersion.fetch_add (1);
    referenceSlot = ref;
    fitAmount = ref >= 0 ? amount : 0.0f;
    for (int i = 0; i < 16; ++i)
        fitProfile[(size_t) i] = ref >= 0 ? profile[(size_t) i] : 0.0f;
    fitVersion.fetch_add (1);
    }

    // outside the lock: this can end up asking the host for a parameter gesture
    if (const int waiting = pendingReferenceKey.load(); waiting >= 0 && waiting == ref)
        applyReferenceKey (waiting);
}

/** The loop follows the key of your own track (once it is loaded and its key is known). */
void SliceTribeProcessor::applyReferenceKey (int slot)
{
    // a parameter gesture belongs on the message thread; the worker asks for one instead of doing it
    if (! juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        pendingReferenceKey = slot;
        triggerAsyncUpdate();
        return;
    }
    int key = -1;
    { const juce::ScopedLock sl (slotLock); key = slotAudio[(size_t) slot].detectedKey; }
    if (key < 0)
    {
        pendingReferenceKey = slot;   // still loading: try again when it is ready
        return;
    }
    pendingReferenceKey = -1;
    if (auto* prm = apvts.getParameter ("key"))
    {
        if (keyBeforeReference.load() < 0)
            keyBeforeReference = (int) apvts.getRawParameterValue ("key")->load();
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->convertTo0to1 ((float) (key + 1))));
        prm->endChangeGesture();
    }
}

void SliceTribeProcessor::handleAsyncUpdate()
{
    if (keyRestoreWanted.exchange (false))
        restoreKeyAfterReference();
    if (const int waiting = pendingReferenceKey.load(); waiting >= 0 && waiting == referenceSlot.load())
        applyReferenceKey (waiting);
}

/** FIT off for one slot: the % field goes back to being its share. Call with slotLock held. */
bool SliceTribeProcessor::releaseReference (int slot)
{
    if (! slotState[(size_t) slot].reference)
        return false;
    editSlotState (slot, [] (SlotState& st)
    {
        st.reference = false;
        if (st.weightBefore >= 0.0f)
            st.weight = juce::jlimit (0.0f, 2.0f, st.weightBefore);
        st.weightBefore = -1.0f;
    });
    return true;
}

/** Puts the KEY back the way it was before a track took it over (message thread). */
void SliceTribeProcessor::restoreKeyAfterReference()
{
    pendingReferenceKey = -1;
    if (! juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        keyRestoreWanted = true;
        triggerAsyncUpdate();
        return;
    }
    keyRestoreWanted = false;
    if (const int back = keyBeforeReference.exchange (-1); back >= 0)
        if (auto* prm = apvts.getParameter ("key"))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->convertTo0to1 ((float) back)));
            prm->endChangeGesture();
        }
}

void SliceTribeProcessor::setSlotReference (int slot, bool isReference)
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return;
    bool wasReference = false;
    {
        const juce::ScopedLock sl (slotLock);
        if (isReference)
        {
            for (int i = 0; i < kNumSlots; ++i)
                if (i != slot)
                    releaseReference (i);                  // only one track at a time
            editSlotState (slot, [] (SlotState& st)
            {
                if (! st.reference)
                    st.weightBefore = st.weight;           // the share comes back when FIT goes off
                st.reference = true;
                if (st.weight < 0.05f)
                    st.weight = 1.0f;                      // a share of 0% would mean "don't fit at all"
            });
        }
        else
            wasReference = releaseReference (slot);        // a slot that was not the reference stays untouched
    }
    if (isReference)
        applyReferenceKey (slot);
    else if (wasReference)
        restoreKeyAfterReference();
    updateFitFromSlots();
    slotsVersion.fetch_add (1);
    requestUpdate();
}

void SliceTribeProcessor::setFallbackBpm (double b)
{
    fallbackBpm = juce::jlimit (40.0, 300.0, b);
}

void SliceTribeProcessor::setPreview (bool p)
{
    if (p && ! preview.load())
        previewRestart = true;
    preview = p;
}

//==============================================================================
// Arrangement & history
//==============================================================================
SliceTribeProcessor::ParamMap SliceTribeProcessor::currentParamValues (bool withFx) const
{
    ParamMap m;
    for (auto& id : presetParameterIds())
        if (withFx || ! id.startsWith ("fx"))
            if (auto* raw = apvts.getRawParameterValue (id))
                m[id] = raw->load();
    return m;
}

int SliceTribeProcessor::getNumDifferentSlices() const
{
    auto r = getDisplayResult();
    return r != nullptr ? r->numDifferentSlices : 0;
}

namespace
{
    bool sameArrangement (const Arrangement& a, const Arrangement& b)
    {
        return a.seed == b.seed && a.rhythmSeed == b.rhythmSeed && a.hitSeeds == b.hitSeeds
            && a.locked == b.locked && a.forceOwn == b.forceOwn && a.charSeeds == b.charSeeds;
    }
    bool sameParams (const SliceTribeProcessor::ParamMap& scene, const SliceTribeProcessor::ParamMap& now)
    {
        for (auto& [id, v] : now)   // the settings that make the loop (FX tweaks keep a scene "active")
            if (auto it = scene.find (id); it == scene.end() || std::abs (it->second - v) > 1.0e-4f)
                return false;
        return true;
    }
}

bool SliceTribeProcessor::sceneMatchesCurrent (int i) const
{
    const auto now = currentParamValues();
    const juce::ScopedLock sl (sceneLock);
    const auto& sc = scenes[(size_t) i];
    if (! sc.used || ! sameParams (sc.params, now))
        return false;
    const juce::ScopedLock al (arrangementLock);
    return sameArrangement (sc.arr, arrangement);
}

void SliceTribeProcessor::applyParamValues (const ParamMap& values)
{
    for (auto& [id, v] : values)
        if (auto* prm = apvts.getParameter (id))
            if (std::abs (apvts.getRawParameterValue (id)->load() - v) > 1.0e-4f)
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->convertTo0to1 (v)));
                prm->endChangeGesture();
            }
}

void SliceTribeProcessor::generateNew (const ParamMap* settingsBefore)
{
    if (settingsBefore == nullptr && crazyActive.exchange (false))
    {
        // The last loop came from the skin's crazy button: this NEW LOOP goes back to normal first -
        // but only when the settings are still exactly as the crazy button left them.
        ParamMap back, after;
        { const juce::ScopedLock jl (jobLock); back = beforeCrazy; after = afterCrazy; }
        if (sameParams (after, currentParamValues()))
            applyParamValues (back);
    }
    generateCount.fetch_add (1);
    const auto now = currentParamValues();
    {
        const juce::ScopedLock al (arrangementLock);
        history[(size_t) historyPos] = { arrangement, settingsBefore != nullptr ? *settingsBefore : now };
        history.resize ((size_t) historyPos + 1);
        arrangement.regenerate (randomSeed());
        history.push_back ({ arrangement, now });
        if (history.size() > 200)
            history.erase (history.begin());
        historyPos = (int) history.size() - 1;
        historyPosAtomic = historyPos;
        historySizeAtomic = (int) history.size();
    }
    activeScene = -1;
    scenesVersion.fetch_add (1);
    requestUpdate();
}

void SliceTribeProcessor::mutate()
{
    generateCount.fetch_add (1);
    const auto now = currentParamValues();
    {
        const juce::ScopedLock al (arrangementLock);
        history[(size_t) historyPos] = { arrangement, now };
        history.resize ((size_t) historyPos + 1);
        auto& r = juce::Random::getSystemRandom();
        std::vector<size_t> open;
        for (size_t h = 0; h < arrangement.hitSeeds.size(); ++h)
            if (h >= arrangement.locked.size() || ! arrangement.locked[h])
                open.push_back (h);
        if (! open.empty())
        {
            // 20-30 % of the unlocked slices get a new source, at least one
            const int count = juce::jmax (1, (int) std::round (open.size() * (0.2 + 0.1 * r.nextDouble())));
            for (int k = 0; k < count && ! open.empty(); ++k)
            {
                const size_t pick = (size_t) r.nextInt ((int) open.size());
                arrangement.reroll (open[pick], randomSeed());
                open.erase (open.begin() + (std::ptrdiff_t) pick);
            }
        }
        history.push_back ({ arrangement, now });
        if (history.size() > 200)
            history.erase (history.begin());
        historyPos = (int) history.size() - 1;
        historyPosAtomic = historyPos;
        historySizeAtomic = (int) history.size();
        lockedCount = arrangement.numLocked();
    }
    activeScene = -1;
    scenesVersion.fetch_add (1);
    requestUpdate();
}

void SliceTribeProcessor::historyBack()
{
    const RenderHold hold (*this);   // arrangement and settings arrive together: one render, locks kept
    ParamMap target;
    {
        const juce::ScopedLock al (arrangementLock);
        if (historyPos <= 0) return;
        history[(size_t) historyPos] = { arrangement, currentParamValues() };
        const auto& e = history[(size_t) --historyPos];
        arrangement = e.arr;
        target = e.params;
        historyPosAtomic = historyPos;
        lockedCount = arrangement.numLocked();
    }
    applyParamValues (target);   // a version comes back with its settings (e.g. undo a crazy button)
    keepLocksOnce = true;
}

void SliceTribeProcessor::historyForward()
{
    const RenderHold hold (*this);
    ParamMap target;
    {
        const juce::ScopedLock al (arrangementLock);
        if (historyPos >= (int) history.size() - 1) return;
        history[(size_t) historyPos] = { arrangement, currentParamValues() };
        const auto& e = history[(size_t) ++historyPos];
        arrangement = e.arr;
        target = e.params;
        historyPosAtomic = historyPos;
        lockedCount = arrangement.numLocked();
    }
    applyParamValues (target);
    keepLocksOnce = true;
}

//==============================================================================
// Scenes
//==============================================================================
void SliceTribeProcessor::storeScene (int i)
{
    if (! juce::isPositiveAndBelow (i, numScenes)) return;
    Scene sc;
    sc.used = true;
    sc.params = currentParamValues (true);   // a scene keeps its FX too
    {
        const juce::ScopedLock al (arrangementLock);
        sc.arr = arrangement;
    }
    {
        const juce::ScopedLock sl (sceneLock);
        scenes[(size_t) i] = std::move (sc);
    }
    activeScene = i;
    scenesVersion.fetch_add (1);
}

void SliceTribeProcessor::recallScene (int i)
{
    if (! juce::isPositiveAndBelow (i, numScenes)) return;
    Scene sc;
    {
        const juce::ScopedLock sl (sceneLock);
        sc = scenes[(size_t) i];
    }
    if (! sc.used) return;
    const RenderHold hold (*this);
    const auto now = currentParamValues();
    ParamMap loopParams;   // without the FX, like every history entry
    for (auto& [id, v] : sc.params)
        if (! id.startsWith ("fx")) loopParams[id] = v;
    {
        const juce::ScopedLock al (arrangementLock);
        if (! (sameArrangement (arrangement, sc.arr) && sameParams (loopParams, now)))   // no duplicate versions
        {
            generateCount.fetch_add (1);
            history[(size_t) historyPos] = { arrangement, now };
            history.resize ((size_t) historyPos + 1);
            arrangement = sc.arr;
            history.push_back ({ arrangement, loopParams });
            if (history.size() > 200)
                history.erase (history.begin());
            historyPos = (int) history.size() - 1;
            historyPosAtomic = historyPos;
            historySizeAtomic = (int) history.size();
            lockedCount = arrangement.numLocked();
        }
    }
    applyParamValues (sc.params);
    keepLocksOnce = true;
    activeScene = i;
    scenesVersion.fetch_add (1);
}

void SliceTribeProcessor::clearScene (int i)
{
    if (! juce::isPositiveAndBelow (i, numScenes)) return;
    {
        const juce::ScopedLock sl (sceneLock);
        scenes[(size_t) i] = {};
    }
    if (activeScene.load() == i) activeScene = -1;
    scenesVersion.fetch_add (1);
}

bool SliceTribeProcessor::isSceneUsed (int i) const
{
    const juce::ScopedLock sl (sceneLock);
    return juce::isPositiveAndBelow (i, numScenes) && scenes[(size_t) i].used;
}

void SliceTribeProcessor::rerollHit (int h)
{
    {
        const juce::ScopedLock al (arrangementLock);
        arrangement.reroll ((size_t) h, randomSeed());
        history[(size_t) historyPos].arr = arrangement;
        lockedCount = arrangement.numLocked();
    }
    requestUpdate();
}

void SliceTribeProcessor::toggleLock (int h)
{
    {
        const juce::ScopedLock al (arrangementLock);
        if (juce::isPositiveAndBelow (h, (int) arrangement.locked.size()))
            arrangement.setLocked ((size_t) h, ! arrangement.locked[(size_t) h]);
        history[(size_t) historyPos].arr = arrangement;
        lockedCount = arrangement.numLocked();
    }
    requestUpdate();
}

void SliceTribeProcessor::unlockAll()
{
    {
        const juce::ScopedLock al (arrangementLock);
        std::fill (arrangement.locked.begin(), arrangement.locked.end(), (juce::uint8) 0);
        history[(size_t) historyPos].arr = arrangement;
        lockedCount = arrangement.numLocked();
    }
    requestUpdate();
}

//==============================================================================
// Export
//==============================================================================
juce::File SliceTribeProcessor::getDefaultExportFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Chupa Loops");
}

juce::String SliceTribeProcessor::suggestedExportName() const
{
    auto r = getDisplayResult();
    const auto s = readSettings();
    const double bpm = r != nullptr ? r->bpm : hostBpm.load();
    const int bars = r != nullptr ? r->bars : s.bars;
    return juce::File::createLegalFileName ("Chupa Loops " + juce::String (juce::roundToInt (bpm)) + "bpm " + juce::String (bars) + "bars "
                                            + choices::patterns[s.pattern] + (s.style != styleClean ? " " + choices::styles[s.style] : juce::String()));
}

bool SliceTribeProcessor::hasLoop() const
{
    auto r = getDisplayResult();
    return r != nullptr && ! r->segments.empty();
}

namespace
{
    /** The loop exactly as it plays: with the FX rack applied (two passes, so the tail of the loop
        runs into its start just like when it loops in the DAW). */
    juce::AudioBuffer<float> loopWithFx (const RenderResult& r, const FxChain::Params& p, float gain)
    {
        // volume first, then the effects: exactly the order of the playback (drive is not linear)
        juce::AudioBuffer<float> out;
        out.makeCopyOf (r.audio);
        out.applyGain (gain);
        if (p.isNeutral() || out.getNumSamples() == 0)
            return out;

        std::vector<int> starts;
        for (const auto& seg : r.segments)
            starts.push_back ((int) juce::jlimit<juce::int64> (0, out.getNumSamples() - 1, seg.start));
        std::sort (starts.begin(), starts.end());

        FxChain chain;
        chain.prepare (r.rate);
        const double spb = r.samplesPerBeat();
        for (int pass = 0; pass < 2; ++pass)
        {
            out.makeCopyOf (r.audio);
            out.applyGain (gain);
            chain.process (out.getWritePointer (0), out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr,
                           out.getNumSamples(), p, 0.0, 1.0 / spb, starts.data(), (int) starts.size());
        }
        return out;
    }
}

juce::File SliceTribeProcessor::exportLoopTo (const juce::File& file)
{
    auto r = getDisplayResult();
    if (r == nullptr || r->segments.empty())
        return {};
    file.getParentDirectory().createDirectory();
    const float gain = juce::Decibels::decibelsToGain (gainParam->load(), -100.0f);
    return engine::writeWav (file, loopWithFx (*r, readFx(), gain), r->rate, 1.0f) ? file : juce::File();
}

juce::File SliceTribeProcessor::exportLoop (const juce::File& folder)
{
    auto r = getDisplayResult();
    if (r == nullptr || r->segments.empty())
        return {};

    folder.createDirectory();
    auto file = folder.getNonexistentChildFile (suggestedExportName(), ".wav", true);

    const float gain = juce::Decibels::decibelsToGain (gainParam->load(), -100.0f);
    return engine::writeWav (file, loopWithFx (*r, readFx(), gain), r->rate, 1.0f) ? file : juce::File();
}

/** A standard MIDI file (type 0, 960 PPQ): one note per slice. Every different slice has its own key
    (C#1 = the first one, D1 = the next ...; a repeated slice plays its key again). With MIDI NOTES set to
    Slices this plays the new loop back exactly - and you can edit it. */
static juce::File writeSliceMidi (const juce::File& file, const RenderResult& r)
{
    juce::MidiMessageSequence seq;
    const double ppq = 960.0, spb = r.samplesPerBeat();
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / r.bpm)), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    for (size_t i = 0; i < r.segments.size() && i < r.segmentNote.size(); ++i)
    {
        if (r.segmentNote[i] < 0)
            continue;   // more different slices than keys (see numDifferentSlices)
        const auto& seg = r.segments[i];
        const int note = RenderResult::firstSliceNote + r.segmentNote[i];
        const double t0 = std::round ((double) seg.start / spb * ppq);
        const double t1 = juce::jmax (t0 + 1.0, std::round ((double) (seg.start + seg.length) / spb * ppq) - 1.0);
        seq.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), t0);
        seq.addEvent (juce::MidiMessage::noteOff (1, note), t1);
    }
    const double endTick = std::round ((double) r.audio.getNumSamples() / spb * ppq);
    seq.addEvent (juce::MidiMessage::endOfTrack(), endTick);
    seq.sort();
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    mf.addTrack (seq);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! mf.writeTo (out, 0))
        return {};
    return file;
}

juce::File SliceTribeProcessor::exportMidi (const juce::File& folder)
{
    auto r = getDisplayResult();
    if (r == nullptr || r->segments.empty())
        return {};
    folder.createDirectory();
    return writeSliceMidi (folder.getNonexistentChildFile (suggestedExportName(), ".mid", true), *r);
}

juce::File SliceTribeProcessor::exportMidiTo (const juce::File& file)
{
    auto r = getDisplayResult();
    if (r == nullptr || r->segments.empty())
        return {};
    file.getParentDirectory().createDirectory();
    return writeSliceMidi (file, *r);
}

juce::File SliceTribeProcessor::exportSliceKit (const juce::File& parentFolder)
{
    auto r = getDisplayResult();
    if (r == nullptr || r->segments.empty())
        return {};

    auto kit = parentFolder.getNonexistentChildFile (suggestedExportName(), {}, true);
    if (! kit.createDirectory())
        return {};

    const float gain = juce::Decibels::decibelsToGain (gainParam->load(), -100.0f);
    const auto full = loopWithFx (*r, readFx(), gain);
    bool ok = engine::writeWav (kit.getChildFile (kit.getFileName() + ".wav"), full, r->rate, 1.0f);

    // one WAV per different slice, named after the key that plays it in the MIDI file
    const int fade = juce::jmax (1, (int) (r->rate * 0.002));
    for (size_t k = 0; k < r->noteSegment.size() && ok; ++k)
    {
        const auto& seg = r->segments[(size_t) r->noteSegment[k]];
        const int start = (int) juce::jlimit<juce::int64> (0, full.getNumSamples(), seg.start);
        const int len = (int) juce::jlimit<juce::int64> (0, full.getNumSamples() - start, seg.length);
        if (len <= 0)
            continue;
        juce::AudioBuffer<float> slice (full.getNumChannels(), len);
        for (int c = 0; c < full.getNumChannels(); ++c)
        {
            slice.copyFrom (c, 0, full, c, start, len);
            const int f = juce::jmin (fade, len / 2);
            if (f > 0)
            {
                slice.applyGainRamp (c, 0, f, 0.0f, 1.0f);
                slice.applyGainRamp (c, len - f, f, 1.0f, 0.0f);
            }
        }
        const auto name = "Slice " + juce::String ((int) k + 1).paddedLeft ('0', 2)
                        + " (" + juce::MidiMessage::getMidiNoteName (RenderResult::firstSliceNote + (int) k, true, true, 3) + ").wav";
        ok = engine::writeWav (kit.getChildFile (name), slice, r->rate, 1.0f);
    }
    if (ok)
        ok = writeSliceMidi (kit.getChildFile (kit.getFileName() + ".mid"), *r) != juce::File();
    return ok ? kit : juce::File();
}

//==============================================================================
// Arrangement <-> ValueTree (state, scenes)
//==============================================================================
juce::ValueTree SliceTribeProcessor::arrangementToTree (const Arrangement& a, const juce::Identifier& type)
{
    juce::ValueTree arr (type);
    arr.setProperty ("seed", juce::String ((juce::int64) a.seed), nullptr);
    arr.setProperty ("rhythmSeed", juce::String ((juce::int64) a.rhythmSeed), nullptr);
    arr.setProperty ("hitSeeds", juce::var (juce::MemoryBlock (a.hitSeeds.data(), a.hitSeeds.size() * sizeof (juce::uint64))), nullptr);
    arr.setProperty ("locked",   juce::var (juce::MemoryBlock (a.locked.data(), a.locked.size())), nullptr);
    arr.setProperty ("forceOwn", juce::var (juce::MemoryBlock (a.forceOwn.data(), a.forceOwn.size())), nullptr);
    arr.setProperty ("charSeeds", juce::var (juce::MemoryBlock (a.charSeeds.data(), a.charSeeds.size() * sizeof (juce::uint64))), nullptr);
    return arr;
}

void SliceTribeProcessor::arrangementFromTree (const juce::ValueTree& arr, Arrangement& a)
{
    a.seed = (juce::uint64) arr.getProperty ("seed").toString().getLargeIntValue();
    a.rhythmSeed = arr.hasProperty ("rhythmSeed") ? (juce::uint64) arr.getProperty ("rhythmSeed").toString().getLargeIntValue()
                                                  : a.seed ^ 0x51CEull;   // projects from before this existed
    a.hitSeeds.clear();
    if (auto* mb = arr.getProperty ("hitSeeds").getBinaryData())
    {
        a.hitSeeds.resize (juce::jmin<size_t> (mb->getSize() / sizeof (juce::uint64), 100000));
        std::memcpy (a.hitSeeds.data(), mb->getData(), a.hitSeeds.size() * sizeof (juce::uint64));
    }
    auto readBytes = [&arr] (const char* id, std::vector<juce::uint8>& v, size_t n)
    {
        v.assign (n, 0);
        if (auto* mb = arr.getProperty (id).getBinaryData())
            std::memcpy (v.data(), mb->getData(), juce::jmin (n, mb->getSize()));
    };
    readBytes ("locked", a.locked, a.hitSeeds.size());
    readBytes ("forceOwn", a.forceOwn, a.hitSeeds.size());
    a.charSeeds.assign (a.hitSeeds.size(), 0);   // 0 = "from before this existed": the old sound is kept
    if (auto* mb = arr.getProperty ("charSeeds").getBinaryData())
        std::memcpy (a.charSeeds.data(), mb->getData(),
                     juce::jmin (a.charSeeds.size() * sizeof (juce::uint64), mb->getSize()));
}

//==============================================================================
// State
//==============================================================================
void SliceTribeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto root = apvts.copyState();
    for (int i = root.getNumChildren(); --i >= 0;)
        if (root.getChild (i).hasType ("SLOTS") || root.getChild (i).hasType ("ARRANGEMENT") || root.getChild (i).hasType ("SCENES"))
            root.removeChild (i, nullptr);

    juce::ValueTree slots ("SLOTS");
    {
        const juce::ScopedLock sl (slotLock);
        for (int i = 0; i < kNumSlots; ++i)
        {
            const auto& a = slotAudio[(size_t) i];
            const auto& pend = pending[(size_t) i];
            const bool missing = slotMissing[(size_t) i] && missingFile[(size_t) i] != juce::File();
            if (a.original == nullptr && ! pend.active && ! missing)
                continue;

            // a slot that is still loading is saved exactly as it was queued (incl. its embedded audio)
            const auto& st = pend.active ? pend.state : slotState[(size_t) i];
            const juce::String path = pend.active ? pend.file.getFullPathName()
                                    : a.original != nullptr ? a.path : missingFile[(size_t) i].getFullPathName();
            const auto& emb = pend.active ? pend.embedded : embeddedAudio[(size_t) i];
            const double detected = pend.active ? pend.detectedBpm : a.detectedBpm;

            juce::ValueTree t ("SLOT");
            t.setProperty ("index", i, nullptr);
            t.setProperty ("path", path, nullptr);
            t.setProperty ("enabled", st.enabled, nullptr);
            t.setProperty ("bpm", st.bpmOverride, nullptr);
            t.setProperty ("transpose", st.transpose, nullptr);
            t.setProperty ("weight", st.weight, nullptr);
            t.setProperty ("reference", st.reference, nullptr);
            t.setProperty ("weightBefore", st.weightBefore, nullptr);
            if (detected > 0.0)
                t.setProperty ("detectedBpm", detected, nullptr);
            // name and key travel with the project, also to a computer where the path doesn't exist
            t.setProperty ("name", pend.active ? (pend.name.isNotEmpty() ? pend.name : pend.file.getFileNameWithoutExtension()) : a.name, nullptr);
            t.setProperty ("key", pend.active ? pend.key : a.detectedKey, nullptr);
            if (emb.getSize() > 0)
                t.setProperty ("audio", juce::var (emb), nullptr);
            slots.appendChild (t, nullptr);
        }
    }
    root.appendChild (slots, nullptr);

    {
        const juce::ScopedLock al (arrangementLock);
        root.appendChild (arrangementToTree (arrangement, "ARRANGEMENT"), nullptr);
    }
    juce::ValueTree sc ("SCENES");
    {
        const juce::ScopedLock l (sceneLock);
        for (int i = 0; i < numScenes; ++i)
        {
            const auto& scene = scenes[(size_t) i];
            if (! scene.used)
                continue;
            auto t = arrangementToTree (scene.arr, "SCENE");
            t.setProperty ("index", i, nullptr);
            juce::ValueTree prm ("PARAMS");
            for (const auto& [id, v] : scene.params)
                prm.setProperty (id, v, nullptr);
            t.appendChild (prm, nullptr);
            sc.appendChild (t, nullptr);
        }
        sc.setProperty ("active", activeScene.load(), nullptr);
    }
    root.appendChild (sc, nullptr);
    root.setProperty ("editorWidth", editorWidth.load(), nullptr);
    root.setProperty ("bpm", fallbackBpm.load(), nullptr);   // the tempo you set when no DAW provides one
    root.setProperty ("keyBeforeFit", keyBeforeReference.load(), nullptr);   // the KEY to go back to when FIT goes off

    for (int i = root.getNumChildren(); --i >= 0;)
        if (root.getChild (i).hasType ("PRESET"))
            root.removeChild (i, nullptr);
    presets.saveTo (root);

    juce::StringArray map;
    for (int cc = 0; cc < 128; ++cc)
        if (const int p = ccMap[(size_t) cc].load(); juce::isPositiveAndBelow (p, allParams.size()))
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (allParams[p]))
                map.add (juce::String (cc) + ":" + rp->getParameterID());
    root.setProperty ("midiMap", map.joinIntoString (","), nullptr);

    juce::MemoryOutputStream out (dest, false);
    out.writeString ("CHUPALOOPS1");
    root.writeToStream (out);
}

void SliceTribeProcessor::setStateInformation (const void* data, int size)
{
    autoPickRequest = 0;      // a queued job must never overwrite the project that is being opened
    stemsRequest = false;
    jobBusy = false;
    crazyActive = false;      // and "back to normal" never carries over into another project
    pendingReferenceKey = -1; // nor does anything FIT TO TRACK remembered about the previous one
    keyBeforeReference = -1;  // (the project may put this one back, below)
    keyRestoreWanted = false;
    juce::MemoryInputStream in (data, (size_t) size, false);
    const auto magic = in.readString();
    if (magic != "CHUPALOOPS1" && magic != "SLICETRIBE1")
        return;

    auto root = juce::ValueTree::readFromStream (in);
    if (! root.isValid())
        return;

    auto slots = root.getChildWithName ("SLOTS");
    auto arr = root.getChildWithName ("ARRANGEMENT");

    editorWidth = juce::jlimit (0, 4000, (int) root.getProperty ("editorWidth", 0));
    if (const double savedBpm = root.getProperty ("bpm", 0.0); savedBpm > 20.0)
        fallbackBpm = juce::jlimit (40.0, 300.0, savedBpm);
    keyBeforeReference = juce::jlimit (-1, 24, (int) root.getProperty ("keyBeforeFit", -1));
    auto params = root.createCopy();
    params.removeProperty ("editorWidth", nullptr);
    params.removeProperty ("bpm", nullptr);
    params.removeProperty ("keyBeforeFit", nullptr);
    params.removeProperty ("midiMap", nullptr);
    for (int i = params.getNumChildren(); --i >= 0;)
        if (params.getChild (i).hasType ("SLOTS") || params.getChild (i).hasType ("ARRANGEMENT") || params.getChild (i).hasType ("PRESET")
            || params.getChild (i).hasType ("SCENES"))
            params.removeChild (i, nullptr);
    triggerWasOn = true;   // a stored "on" state of the trigger must not fire a new loop
    apvts.replaceState (params);
    triggerWasOn = apvts.getRawParameterValue ("trigger")->load() > 0.5f;
    presets.restoreFrom (root);

    for (auto& c : ccMap) c = -1;
    juce::StringArray map;
    map.addTokens (root.getProperty ("midiMap").toString(), ",", "");
    for (auto& m : map)
    {
        const int cc = m.upToFirstOccurrenceOf (":", false, false).getIntValue();
        const auto id = m.fromFirstOccurrenceOf (":", false, false);
        for (int p = 0; p < allParams.size(); ++p)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (allParams[p]))
                if (rp->getParameterID() == id && juce::isPositiveAndBelow (cc, 128))
                    ccMap[(size_t) cc] = p;
    }
    midiLearnVersion.fetch_add (1);

    // Slots: a slot that already holds exactly this audio is kept as it is (host A/B compare, preset
    // recall), others are loaded in the background while the old audio keeps playing until they are ready.
    std::array<bool, kNumSlots> inState {};
    bool haveReference = false;
    for (auto t : slots)
    {
        const int i = t.getProperty ("index", -1);
        if (! juce::isPositiveAndBelow (i, kNumSlots) || inState[(size_t) i])
            continue;
        inState[(size_t) i] = true;
        SlotState st;
        st.enabled = t.getProperty ("enabled", true);
        st.bpmOverride = t.getProperty ("bpm", 0.0);
        st.transpose = t.getProperty ("transpose", 0);
        st.weight = juce::jlimit (0.0f, 2.0f, (float) (double) t.getProperty ("weight", 1.0));
        st.reference = (bool) t.getProperty ("reference", false);
        st.weightBefore = (float) (double) t.getProperty ("weightBefore", -1.0);
        if (st.reference && st.weight < 0.05f)
            st.weight = 1.0f;         // a share of 0% would mean "don't fit at all"
        if (st.reference && haveReference)
            st.reference = false;     // only one track at a time, whatever the file says
        haveReference = haveReference || st.reference;
        const juce::MemoryBlock* emb = t.getProperty ("audio").getBinaryData();
        const auto path = t.getProperty ("path").toString();
        const juce::File file = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
        const double detected = (double) t.getProperty ("detectedBpm", 0.0);

        bool same = false;
        {
            const juce::ScopedLock sl (slotLock);
            const auto& a = slotAudio[(size_t) i];
            same = ! pending[(size_t) i].active && a.original != nullptr && a.path == file.getFullPathName()
                && emb != nullptr && embeddedAudio[(size_t) i] == *emb
                && (detected <= 0.0 || std::abs (a.detectedBpm - detected) < 1.0e-6);
            if (same)
                slotState[(size_t) i] = st;
        }
        if (same)
        {
            slotsVersion.fetch_add (1);
            continue;
        }
        queueLoad (i, file, emb, st, detected, t.getProperty ("name").toString(), (int) t.getProperty ("key", -2));
    }
    for (int i = 0; i < kNumSlots; ++i)
        if (! inState[(size_t) i])
        {
            const auto info = getSlotInfo (i);
            if (info.loaded || info.loading || info.missing || info.error)
                clearSlot (i);
        }

    {
        const juce::ScopedLock l (sceneLock);
        for (auto& scene : scenes)
            scene = {};
        auto sc = root.getChildWithName ("SCENES");
        for (auto t : sc)
        {
            const int i = t.getProperty ("index", -1);
            if (! juce::isPositiveAndBelow (i, numScenes))
                continue;
            auto& scene = scenes[(size_t) i];
            scene.used = true;
            arrangementFromTree (t, scene.arr);
            auto prm = t.getChildWithName ("PARAMS");
            for (int k = 0; k < prm.getNumProperties(); ++k)
            {
                const auto id = prm.getPropertyName (k);
                scene.params[id.toString()] = (float) prm.getProperty (id);
            }
        }
        const int act = sc.isValid() ? (int) sc.getProperty ("active", -1) : -1;
        activeScene = juce::isPositiveAndBelow (act, numScenes) && scenes[(size_t) act].used ? act : -1;
        scenesVersion.fetch_add (1);
    }

    if (arr.isValid())
    {
        const juce::ScopedLock al (arrangementLock);
        arrangementFromTree (arr, arrangement);
        history.assign (1, HistoryEntry { arrangement, currentParamValues() });
        historyPos = 0;
        historyPosAtomic = 0;
        historySizeAtomic = 1;
        keepLocksOnce = true;
        lockedCount = arrangement.numLocked();
    }
    // The standalone app reloads whatever was on screen when it was closed, before the window even
    // exists. Key match is the one setting that must not travel that way: it silently transposes
    // every sample you drop in next, so a fresh start always begins on Off. A song you load by hand
    // later on (Options > Load state) and a DAW project both keep the key they were saved with.
    const bool sessionReload = isStandalone() && ! editorEverOpened.load() && ! standaloneStateRestored.exchange (true);
    if (sessionReload)
    {
        if (auto* prm = apvts.getParameter ("key"))
            if (prm->getValue() > 0.0f)
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (0.0f);
                prm->endChangeGesture();
            }
        keyBeforeReference = -1;
    }

    // ... but a slot that is marked as your own track still gets its key, so FIT keeps its promise.
    // Its audio is usually still loading, so this asks for the key and the worker applies it when ready.
    if (sessionReload)
    {
        const juce::ScopedLock sl (slotLock);
        for (int i = 0; i < kNumSlots; ++i)
            if (slotState[(size_t) i].reference || (pending[(size_t) i].active && pending[(size_t) i].state.reference))
            {
                pendingReferenceKey = i;
                break;
            }
    }
    updateFitFromSlots();
    requestUpdate();
}

//==============================================================================
// "Craziest loop ever": every skin has its own flavour of madness
//==============================================================================
void SliceTribeProcessor::crazyLoop (int flavour)
{
    const RenderHold hold (*this);              // no half-crazy render in between (and the locks stay intact)
    const auto before = currentParamValues();   // so the previous version (with ◀) brings the old settings back
    { const juce::ScopedLock jl (jobLock); beforeCrazy = before; }
    auto& r = juce::Random::getSystemRandom();
    auto set = [this] (const char* id, float plain)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->convertTo0to1 (plain)));
            prm->endChangeGesture();
        }
    };
    auto pick = [&r] (std::initializer_list<int> l) { return (float) *(l.begin() + r.nextInt ((int) l.size())); };
    auto range = [&r] (float a, float b) { return a + r.nextFloat() * (b - a); };

    // pattern: 0 Free 1 4floor 2 Offbeat 3 Offbeat2x 4 Rolling16 5 KBBB 6 Gallop 7 Broken 8 Random
    // sliceSize: 0 1/32 1 1/16 2 1/8 3 1/4 4 1/2 5 bar | style: 0 Clean 1 Glitch 2 Lo-Fi | motif: 0 off 1..3 = 1/2/4 bars
    switch (flavour)
    {
        case skinFruity:    // Fruit Punch: bouncy, octave-jumping, swung
            set ("pattern", pick ({ 2, 3, 6, 7 })); set ("style", 0); set ("sliceSize", pick ({ 1, 2 })); set ("sliceMode", 0);
            set ("chaos", range (60, 90)); set ("octave", range (45, 85)); set ("swing", range (20, 50)); set ("gate", range (50, 80));
            set ("reverse", range (0, 15)); set ("motif", pick ({ 1, 2 })); set ("variation", range (30, 60)); set ("fade", range (2, 5));
            break;
        case skinSkull:     // Skull Damage: crushed, backwards, broken
            set ("pattern", pick ({ 7, 8 })); set ("style", 2); set ("amount", range (80, 100)); set ("sliceSize", pick ({ 1, 2 }));
            set ("chaos", range (70, 100)); set ("reverse", range (25, 55)); set ("octave", range (0, 20)); set ("gate", range (40, 80));
            set ("swing", range (0, 20)); set ("motif", pick ({ 1, 2 })); set ("variation", range (40, 80)); set ("fade", range (1.5f, 4));
            break;
        case skinButcher:   // The Butcher Cut: tiny, stabby chops on every transient
            set ("pattern", pick ({ 3, 4, 7 })); set ("style", 1); set ("amount", range (60, 90)); set ("sliceSize", 0); set ("sliceMode", 1);
            set ("sensitivity", range (60, 90)); set ("chaos", range (70, 100)); set ("gate", range (15, 40)); set ("fade", 1.0f);
            set ("reverse", range (5, 25)); set ("octave", range (0, 25)); set ("swing", 0); set ("motif", pick ({ 1, 2 })); set ("variation", range (40, 70));
            break;
        case skinNeon:      // Neon Overdrive: rolling, octave-lit, glitchy
            set ("pattern", pick ({ 3, 4, 5 })); set ("style", 1); set ("amount", range (40, 70)); set ("sliceSize", 1);
            set ("chaos", range (60, 90)); set ("octave", range (30, 60)); set ("gate", range (60, 90)); set ("reverse", range (0, 20));
            set ("swing", 0); set ("motif", pick ({ 1, 2 })); set ("variation", range (30, 60)); set ("fade", range (1.5f, 3));
            break;
        case skinAcid:      // Acid Flashback: smeared, backwards, swung, lo-fi
            set ("pattern", pick ({ 0, 7, 8 })); set ("style", pick ({ 1, 2 })); set ("amount", range (50, 90)); set ("sliceSize", pick ({ 1, 2 }));
            set ("chaos", range (80, 100)); set ("reverse", range (40, 70)); set ("octave", range (20, 50)); set ("swing", range (30, 60));
            set ("gate", range (70, 100)); set ("fade", range (6, 15)); set ("motif", pick ({ 1, 2 })); set ("variation", range (50, 90));
            break;
        case skinSmile:     // Smiley Mayhem: absolutely anything
            set ("pattern", (float) r.nextInt (9)); set ("style", (float) r.nextInt (3)); set ("amount", range (30, 100));
            set ("sliceSize", (float) r.nextInt (4)); set ("sliceMode", (float) r.nextInt (2)); set ("sensitivity", range (30, 90));
            set ("chaos", range (50, 100)); set ("reverse", range (0, 60)); set ("octave", range (0, 60)); set ("swing", range (0, 60));
            set ("gate", range (20, 100)); set ("fade", range (1, 12)); set ("motif", (float) r.nextInt (4)); set ("variation", range (0, 100));
            break;
        case skinLolly:
        default:            // Sugar Rush: max chaos, octave-high, stuttering
            set ("pattern", pick ({ 4, 5, 8 })); set ("style", 1); set ("amount", range (70, 100)); set ("sliceSize", pick ({ 0, 1 }));
            set ("sliceMode", (float) r.nextInt (2)); set ("chaos", range (85, 100)); set ("octave", range (40, 80)); set ("reverse", range (10, 30));
            set ("gate", range (60, 100)); set ("swing", range (0, 25)); set ("motif", pick ({ 1, 2 })); set ("variation", range (60, 100));
            set ("fade", range (1, 3));
            break;
    }
    { const juce::ScopedLock jl (jobLock); afterCrazy = currentParamValues(); }
    crazyActive = true;   // set after the knobs moved: the next NEW LOOP puts these settings back
    generateNew (&before);
}

//==============================================================================
// Presets as host programs
//==============================================================================
int SliceTribeProcessor::getCurrentProgram()
{
    const int i = presets.getCurrentIndex();
    return juce::isPositiveAndBelow (i, (int) factoryPresets().size()) ? i : 0;
}

void SliceTribeProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, (int) factoryPresets().size()) && index != presets.getCurrentIndex())
        presets.loadFactoryFromHost (index);
}

const juce::String SliceTribeProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) factoryPresets().size()) ? factoryPresets()[(size_t) index].name : juce::String();
}

//==============================================================================
// MIDI: note triggers, CC learn, program changes
//==============================================================================
void SliceTribeProcessor::pushAction (Action a) noexcept
{
    const auto scope = actionFifo.write (1);
    if (scope.blockSize1 > 0)      actionBuffer[(size_t) scope.startIndex1] = a;
    else if (scope.blockSize2 > 0) actionBuffer[(size_t) scope.startIndex2] = a;
}

void SliceTribeProcessor::handleMidi (const juce::MidiBuffer& midi, int mode, int numSamples)
{
    for (const auto meta : midi)
    {
        // parse the raw bytes: MidiMessage would allocate for long (sysex) events on the audio thread
        if (meta.numBytes < 2 || meta.data == nullptr)
            continue;
        const int status = meta.data[0] & 0xf0;
        const int d1 = meta.data[1] & 0x7f;
        const int d2 = meta.numBytes > 2 ? (meta.data[2] & 0x7f) : 0;
        const bool noteOn = status == 0x90 && d2 > 0 && meta.numBytes > 2;
        const bool noteOff = status == 0x80 || (status == 0x90 && d2 == 0);
        if (mode != 0 && (noteOn || noteOff))
        {
            // Slices / Keys: notes play the loop (sample-accurate, handled in renderInstrument)
            if (numNoteEvents < (int) noteEvents.size())
                noteEvents[(size_t) numNoteEvents++] = { juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition), d1,
                                                         noteOn ? juce::jlimit (0.05f, 1.0f, (float) d2 / 127.0f) : 0.0f, noteOn };
            continue;
        }
        if (noteOn)
        {
            const int n = d1;
            if      (n == 36) pushAction ({ actNewLoop, n, 0 });
            else if (n == 37) pushAction ({ actBack, n, 0 });
            else if (n == 38) pushAction ({ actForward, n, 0 });
            else if (n == 39) pushAction ({ actUnlock, n, 0 });
            else if (n == 40) pushAction ({ actCrazy, n, 0 });
            else if (n == 41) pushAction ({ actMutate, n, 0 });
            else if (n >= 72 && n <= 79) pushAction ({ actScene, n, (float) (n - 72) });
            else if (n >= 48 && n <= 56) pushAction ({ actSetParam, -1, (float) (n - 48) });   // rhythm
            else if (n >= 60 && n <= 65) pushAction ({ actSetParam, -2, (float) (n - 60) });   // length
        }
        else if (status == 0xb0 && meta.numBytes > 2)   // controller
        {
            const int cc = d1;
            if (cc >= 120)
            {
                // channel-mode messages (all sound / notes off, reset) that hosts send on stop
                if (mode != 0 && (cc == 120 || cc == 123) && numNoteEvents < (int) noteEvents.size())
                    noteEvents[(size_t) numNoteEvents++] = { juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition), -1, 0.0f, false };
                continue;
            }
            const float v = (float) d2 / 127.0f;
            if (learnParam.load() >= 0 && cc != 0 && cc != 32)   // bank select is never learned
            {
                if (const int lp = learnParam.exchange (-1); lp >= 0)
                    pushAction ({ actLearned, lp, (float) cc });
            }
            else if (const int p = ccMap[(size_t) cc].load(); p >= 0)
                pushAction ({ actSetParam, p, v });
        }
        else if (status == 0xc0)   // program change
        {
            pushAction ({ actProgram, d1, 0 });
        }
    }
}

void SliceTribeProcessor::parameterChanged (const juce::String& id, float value)
{
    if (id != "trigger")
        return;
    // can be called from any thread (automation, host UI, a mapped CC): only atomics here
    const bool on = value > 0.5f;
    if (on && ! triggerWasOn.exchange (true))
        pendingTriggers.fetch_add (1);   // rising edge
    else if (! on)
        triggerWasOn = false;
}

void SliceTribeProcessor::timerCallback()
{
    auto noteName = [] (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); };
    auto event = [this] (const juce::String& e) { lastMidiEvent = e; midiEventVersion.fetch_add (1); };

    for (int t = pendingTriggers.exchange (0); t > 0; --t)
        generateNew();

    // a scene stays lit only while the loop is still that scene
    if (const int a = activeScene.load(); a >= 0 && ! sceneMatchesCurrent (a))
    {
        activeScene = -1;
        scenesVersion.fetch_add (1);
    }

    const int ready = actionFifo.getNumReady();
    if (ready <= 0)
        return;
    std::vector<Action> todo;
    {
        const auto scope = actionFifo.read (ready);
        for (int i = 0; i < scope.blockSize1; ++i) todo.push_back (actionBuffer[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) todo.push_back (actionBuffer[(size_t) (scope.startIndex2 + i)]);
    }

    for (const auto& a : todo)
    {
        switch (a.type)
        {
            case actNewLoop:
                generateNew();
                if (a.index >= 0) event (noteName (a.index) + ": new loop");
                break;
            case actBack:     historyBack();    event (noteName (a.index) + ": previous version"); break;
            case actForward:  historyForward(); event (noteName (a.index) + ": next version"); break;
            case actUnlock:   unlockAll();      event (noteName (a.index) + ": unlock all"); break;
            case actMutate:   mutate();         event (noteName (a.index) + ": mutate"); break;
            case actScene:
            {
                const int sc = (int) a.value;
                if (isSceneUsed (sc)) { recallScene (sc); event (noteName (a.index) + ": scene " + juce::String::charToString ((juce::juce_wchar) ('A' + sc))); }
                else event (noteName (a.index) + ": scene " + juce::String::charToString ((juce::juce_wchar) ('A' + sc)) + " is empty");
                break;
            }
            case actCrazy:
                crazyLoop (currentSkinIndex());
                event (noteName (a.index) + ": " + skin().crazyName.toLowerCase());
                break;
            case actProgram:
                if (juce::isPositiveAndBelow (a.index, (int) factoryPresets().size()))
                {
                    presets.loadPreset (a.index);
                    event ("Program " + juce::String (a.index) + ": " + factoryPresets()[(size_t) a.index].name);
                }
                break;
            case actLearned:
            {
                const int cc = (int) a.value;
                for (auto& c : ccMap)
                    if (c.load() == a.index) c = -1;     // one CC per parameter
                ccMap[(size_t) juce::jlimit (0, 127, cc)] = a.index;
                midiLearnVersion.fetch_add (1);
                if (juce::isPositiveAndBelow (a.index, allParams.size()))
                    event ("CC " + juce::String (cc) + " controls " + allParams[a.index]->getName (40));
                break;
            }
            case actSetParam:
            default:
            {
                juce::AudioProcessorParameter* p = nullptr;
                float norm = a.value;
                if (a.index == -1 || a.index == -2)   // note → rhythm / length choice
                {
                    auto* rp = apvts.getParameter (a.index == -1 ? "pattern" : "length");
                    p = rp;
                    norm = rp->convertTo0to1 (a.value);
                }
                else if (juce::isPositiveAndBelow (a.index, allParams.size()))
                    p = allParams[a.index];
                if (p != nullptr && std::abs (p->getValue() - norm) > 1.0e-6f)
                {
                    p->beginChangeGesture();   // so touch/latch automation recording picks up MIDI-learned moves
                    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
                    p->endChangeGesture();
                }
                if (a.index == -1 || a.index == -2)
                {
                    auto* rp = apvts.getParameter (a.index == -1 ? "pattern" : "length");
                    event (juce::String (a.index == -1 ? "Rhythm: " : "Length: ") + rp->getCurrentValueAsText());
                }
                break;
            }
        }
    }
}

void SliceTribeProcessor::startMidiLearn (const juce::String& id)
{
    for (int p = 0; p < allParams.size(); ++p)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (allParams[p]))
            if (rp->getParameterID() == id)
                learnParam = p;
    midiLearnVersion.fetch_add (1);
}

void SliceTribeProcessor::cancelMidiLearn()
{
    learnParam = -1;
    midiLearnVersion.fetch_add (1);
}

void SliceTribeProcessor::clearMidiMapping (const juce::String& id)
{
    for (auto& c : ccMap)
    {
        const int p = c.load();
        if (juce::isPositiveAndBelow (p, allParams.size()))
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (allParams[p]))
                if (rp->getParameterID() == id)
                    c = -1;
    }
    midiLearnVersion.fetch_add (1);
}

int SliceTribeProcessor::getMidiCcFor (const juce::String& id) const
{
    for (int cc = 0; cc < 128; ++cc)
    {
        const int p = ccMap[(size_t) cc].load();
        if (juce::isPositiveAndBelow (p, allParams.size()))
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (allParams[p]))
                if (rp->getParameterID() == id)
                    return cc;
    }
    return -1;
}

juce::String SliceTribeProcessor::getLearningParamId() const
{
    const int p = learnParam.load();
    if (juce::isPositiveAndBelow (p, allParams.size()))
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (allParams[p]))
            return rp->getParameterID();
    return {};
}

juce::String SliceTribeProcessor::getLastMidiEvent() const
{
    return lastMidiEvent;
}

//==============================================================================
juce::AudioProcessorEditor* SliceTribeProcessor::createEditor()
{
    editorEverOpened = true;
    return new SliceTribeEditor (*this);
}

} // namespace slicetribe

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new slicetribe::SliceTribeProcessor();
}
