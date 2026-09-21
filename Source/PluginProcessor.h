#pragma once

#include <JuceHeader.h>
#include "SliceEngine.h"
#include "Presets.h"
#include "Fx.h"
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <map>
#include <atomic>
#include <deque>

namespace slicetribe
{

/** What the UI needs to draw a slot. */
struct SlotInfo
{
    bool loaded = false, loading = false, missing = false, error = false, enabled = true;
    int key = -1, keyShift = 0;
    juce::String name, path;
    double detectedBpm = 0, bpmOverride = 0;
    int transpose = 0;
    float weight = 1.0f;
    float trimStart = 0.0f, trimEnd = 1.0f;
    float warp = 0.0f;
    bool reference = false;
    double stretchRatio = 1.0;
    std::vector<float> peaks;
};

class SliceTribeProcessor : public juce::AudioProcessor,
                            private juce::Thread,
                            private juce::Timer,
                            private juce::AsyncUpdater,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    SliceTribeProcessor();
    ~SliceTribeProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chupa Loops"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) factoryPresets().size(); }
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    // API for the editor (message thread)
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };

    // MIDI learn (message thread)
    void startMidiLearn (const juce::String& paramId);
    void cancelMidiLearn();
    void clearMidiMapping (const juce::String& paramId);
    int  getMidiCcFor (const juce::String& paramId) const;          // -1 = none
    juce::String getLearningParamId() const;
    int  getMidiLearnVersion() const noexcept { return midiLearnVersion.load(); }
    juce::String getLastMidiEvent() const;                            // e.g. "C1: new loop", for the UI
    int  getMidiEventVersion() const noexcept { return midiEventVersion.load(); }
    int  getGenerateCount() const noexcept { return generateCount.load(); }
    /** Different slices in the current loop (MIDI keys needed); more than RenderResult::maxSliceNotes don't fit. */
    int  getNumDifferentSlices() const;

    SlotInfo getSlotInfo (int slot) const;
    void loadSlot (int slot, const juce::File&);
    void clearSlot (int slot);
    void setSlotEnabled (int slot, bool);
    void setSlotWeight (int slot, float weight);       // 0..2: share of the slices coming from this sample
    /** The two lines on a slot: slices are only taken from between them (0..1 of the sample). */
    void setSlotTrim (int slot, float start, float end);
    /** Pulls a human recording onto the grid: 0 = as recorded, 1 = dead straight. */
    void setSlotWarp (int slot, float amount);
    /** kTrackSlot when a part of your own track is loaded (FIT TO TRACK), otherwise -1. */
    int  getReferenceSlot() const noexcept { return referenceSlot.load(); }
    /** Where the sample preview is playing, 0..1 of the whole sample (-1 = not playing). */
    double getSlotPreviewPosition() const noexcept { return slotPreviewPos.load(); }
    /** Listen to one sample on its own (at its own tempo), looping. -1 = stop. */
    void setSlotPreview (int slot);
    int  getSlotPreview() const noexcept { return slotPreviewIndex.load(); }
    void setSlotBpm (int slot, double bpmOrZeroForAuto);
    void setSlotTranspose (int slot, int semitones);
    int  getSlotsVersion() const noexcept { return slotsVersion.load(); }

    using ParamMap = std::map<juce::String, float>;
    void generateNew (const ParamMap* settingsBefore = nullptr);
    void resetSettings();                    // every knob back to its default (CLEAR ALL)
    void crazyLoop (int flavour);   // the skin's "craziest loop ever" (flavour = skin index)
    void mutate();                  // a variation: 20-30% of the unlocked slices change
    void rerollRhythm();            // new rhythm, same sources
    void rerollSources();           // same rhythm, other slices
    void autoPick (int candidates = 8);   // makes a few loops and keeps the one that scores best
    int  keepToScene();             // stores the loop in the first free scene, -1 = all full
    juce::File exportStems (const juce::File& parentFolder);   // one WAV per sample (asks the worker)
    /** Message from a background job (stems, auto pick) for the UI, and a version that changes with it. */
    juce::String getJobMessage() const;
    int  getJobVersion() const noexcept { return jobVersion.load(); }
    bool isJobBusy() const noexcept { return jobBusy.load(); }
    bool jobFailed() const noexcept { return jobError.load(); }

    // scenes A-H: favourite loops (arrangement + settings) to recall live
    static constexpr int numScenes = 8;
    void storeScene (int index);
    void recallScene (int index);
    void clearScene (int index);
    bool isSceneUsed (int index) const;
    int  getActiveScene() const noexcept { return activeScene.load(); }
    int  getScenesVersion() const noexcept { return scenesVersion.load(); }

    // exports for the DAW: the loop as MIDI (slice notes) and as a kit of WAV slices
    juce::File exportMidi (const juce::File& folder);
    juce::File exportMidiTo (const juce::File& file);
    juce::File exportSliceKit (const juce::File& parentFolder);   // returns the kit folder
    void historyBack();
    void historyForward();
    int  getHistoryPosition() const noexcept { return historyPosAtomic.load(); }
    int  getHistorySize() const noexcept     { return historySizeAtomic.load(); }

    void rerollHit (int hitIndex);
    void toggleLock (int hitIndex);
    void unlockAll();

    std::shared_ptr<RenderResult> getDisplayResult() const;
    int  getResultVersion() const noexcept { return resultVersion.load(); }

    double getPlayPosition() const noexcept { return playPosition.load(); }   // 0..1, <0 = stopped
    bool   isBusy() const noexcept          { return busy.load(); }
    double getHostBpm() const noexcept      { return hostBpm.load(); }
    bool   hasHostTempo() const noexcept    { return hostProvidesTempo.load(); }
    void   setFallbackBpm (double);

    bool isStandalone() const;
    void setPreview (bool);
    bool isPreviewing() const noexcept { return preview.load(); }

    juce::File exportLoop (const juce::File& folder);   // returns written file (or {} on failure)
    juce::File exportLoopTo (const juce::File& file);
    juce::String suggestedExportName() const;
    bool hasLoop() const;
    int  getLockedCount() const noexcept { return lockedCount.load(); }
    juce::String getAudioWildcard() { return formatManager.getWildcardForAllFormats(); }

    // editor size, remembered with the project
    std::atomic<int> editorWidth { 0 }, editorTab { 0 };
    static juce::File getDefaultExportFolder();

    Settings readSettings() const;
    int getMidiMode() const { return (int) apvts.getRawParameterValue ("midiMode")->load(); }
    FxChain::Params readFx() const;

private:
    ParamMap currentParamValues (bool withFx = false) const;   // history: without the FX (live moves stay)
    bool sceneMatchesCurrent (int index) const;
    struct RenderHold   // no render while an arrangement and its settings are swapped in together
    {
        explicit RenderHold (SliceTribeProcessor& p) : proc (p) { proc.renderHold = true; }
        ~RenderHold() { proc.renderHold = false; proc.requestUpdate(); }
        SliceTribeProcessor& proc;
    };
    std::atomic<bool> renderHold { false };
    void applyParamValues (const ParamMap&);
    void renderInstrument (juce::AudioBuffer<float>&, const RenderResult&, int mode, double bpm, double rate,
                           bool hostPlaying, std::optional<double> ppq);
    void applyFx (juce::AudioBuffer<float>&, const RenderResult& res, double startIndex, double step, int numSamples);
    static juce::ValueTree arrangementToTree (const Arrangement&, const juce::Identifier&);
    static void arrangementFromTree (const juce::ValueTree&, Arrangement&);
    //==========================================================================
    void run() override;                       // worker thread
    void timerCallback() override;             // message thread: MIDI actions
    void parameterChanged (const juce::String&, float) override;
    void handleMidi (const juce::MidiBuffer&, int mode, int numSamples);

    enum ActionType { actNewLoop, actBack, actForward, actUnlock, actSetParam, actProgram, actLearned, actCrazy, actMutate, actScene };
    struct Action { int type = 0; int index = 0; float value = 0.0f; };
    void pushAction (Action) noexcept;
    juce::AbstractFifo actionFifo { 512 };
    std::array<Action, 512> actionBuffer;

    void requestUpdate() { upToDate = false; updateVersion.fetch_add (1); wake.signal(); }
    void publishResult (std::shared_ptr<RenderResult>);
    void queueLoad (int slot, const juce::File&, const juce::MemoryBlock* embedded, SlotState state, double detectedBpm = 0.0,
                    const juce::String& name = {}, int key = -2);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    //==========================================================================
    juce::AudioFormatManager formatManager;

    struct PendingLoad
    {
        bool active = false;
        juce::File file;
        juce::MemoryBlock embedded;
        SlotState state;
        double detectedBpm = 0.0;   // remembered from the project, so a restore never re-guesses the tempo
        juce::String name;          // from the project (the file may not exist on this computer)
        int key = -2;               // -2 = detect from the name
        int generation = 0;
    };

    static constexpr double maxEmbedSeconds = 64.0;

    mutable juce::CriticalSection slotLock;
    std::array<SlotAudio, kAllSlots>    slotAudio;
    std::array<SlotState, kAllSlots>    slotState;
    std::array<PendingLoad, kAllSlots>  pending;
    std::array<bool, kAllSlots>         slotMissing {};
    std::array<juce::File, kAllSlots>   missingFile;
    std::array<bool, kAllSlots>         slotError {};
    std::array<int, kAllSlots>          slotGeneration {};
    std::array<juce::MemoryBlock, kAllSlots> embeddedAudio;   // compressed copy kept for the project file
    std::array<PreparedSlot, kAllSlots> prepared;             // worker thread only
    // straightened copies, made once per (file, tempo, amount) - worker thread only
    std::array<std::shared_ptr<const juce::AudioBuffer<float>>, kAllSlots> warpedAudio;
    struct WarpKey { int loadId = -1; double bpm = 0.0; float amount = -1.0f; bool smooth = false; bool done = false; };
    std::array<WarpKey, kAllSlots> warpedKey;
    // the same straightened copy, handed to the message thread so preview plays what you will hear
    std::array<std::shared_ptr<const juce::AudioBuffer<float>>, kAllSlots> warpedShared;   // guarded by slotLock
    int nextLoadId = 1;

    mutable juce::CriticalSection arrangementLock;
    Arrangement arrangement;
    struct HistoryEntry { Arrangement arr; ParamMap params; };
    std::vector<HistoryEntry> history;
    int historyPos = 0;

    struct Scene { bool used = false; Arrangement arr; ParamMap params; };
    std::array<Scene, numScenes> scenes;
    mutable juce::CriticalSection sceneLock;
    std::atomic<int> activeScene { -1 }, scenesVersion { 0 };
    std::atomic<int> historyPosAtomic { 0 }, historySizeAtomic { 1 };

    juce::WaitableEvent wake;
    std::atomic<int> updateVersion { 1 }, slotsVersion { 1 }, resultVersion { 0 };
    std::atomic<bool> busy { false }, preview { false }, hostProvidesTempo { false }, keepLocksOnce { false },
                      abortWork { false }, upToDate { false }, previewRestart { false },
                      prepareInterrupt { false };
    std::atomic<int> lockedCount { 0 };
    // FIT TO TRACK: read on the audio thread too, so lock-free
    std::atomic<int> referenceSlot { -1 };
    std::atomic<float> fitAmount { 0.0f };
    std::array<std::atomic<float>, 16> fitProfile {};
    std::atomic<int> fitVersion { 0 };     // seqlock: odd while being written
    juce::CriticalSection fitWriteLock;    // one writer at a time, so the seqlock stays honest
    std::atomic<int> pendingReferenceKey { -1 };   // slot whose key still has to be applied after loading
    std::atomic<int> keyBeforeReference { -1 };
    void updateFitFromSlots();
    void applyReferenceKey (int slot);
    // background jobs on the worker thread (they need the prepared samples)
    std::atomic<int> autoPickRequest { 0 }, jobVersion { 0 };
    std::atomic<bool> stemsRequest { false }, jobBusy { false }, jobError { false };
    juce::File stemsFolder;
    mutable juce::CriticalSection jobLock;
    juce::String jobMessage;
    ParamMap beforeCrazy, afterCrazy;     // guarded by jobLock
    std::atomic<bool> crazyActive { false };
    void runAutoPick (const std::array<bool, kAllSlots>& enabled, const Settings& rs, double bpm, double rate, int candidates);
    void runStems (const std::array<bool, kAllSlots>& enabled, const Settings& rs, double bpm, double rate);
    std::atomic<double> hostBpm { defaultBpm }, fallbackBpm { defaultBpm }, currentRate { 0.0 }, playPosition { -1.0 };
    std::atomic<bool> standaloneStateRestored { false };   // the standalone reloads its last session once
    std::atomic<bool> editorEverOpened { false };          // ... and it does that before the window exists
    std::atomic<bool> keyRestoreWanted { false }, trackRegrid { false }, warpPreviewRearm { false };
    std::atomic<double> slotPreviewPos { -1.0 };
    void handleAsyncUpdate() override;
    void restoreKeyAfterReference();
    void regridTrack();                      // worker thread
    void publishWarped (int slot);           // worker thread: hand the straightened copy over for preview
    /** Changes a slot's settings, also on a file that is still loading. Call with slotLock held. */
    template <typename Fn>
    void editSlotState (int slot, Fn&& fn)
    {
        fn (slotState[(size_t) slot]);
        if (pending[(size_t) slot].active)
            fn (pending[(size_t) slot].state);
    }

    // result hand-off
    mutable juce::SpinLock resultLock;
    std::shared_ptr<RenderResult> latestResult;          // written by worker
    std::deque<std::shared_ptr<RenderResult>> keepAlive; // worker only: stops the audio thread from freeing memory

    // audio thread state
    std::shared_ptr<RenderResult> playing, fadingOut;
    double playIndex = 0.0, fadeIndex = 0.0, previewIndex = 0.0;
    int crossfadeLeft = 0, crossfadeLength = 1;
    juce::SmoothedValue<float> outGain, transportGain;
    bool wasPlaying = false, offlineWaited = false;

    std::atomic<float>* gainParam = nullptr;

    // instrument modes (Slices / Keys) and effects: audio thread only
    struct NoteEvent { int offset = 0, note = 0; float velocity = 1.0f; bool on = false; };
    std::array<NoteEvent, 256> noteEvents;
    int numNoteEvents = 0;
    struct Voice { bool active = false, looping = false, releasing = false; double pos = 0, end = 0; float gain = 0, vel = 1, releaseStep = 0;
                   int note = -1, nextSeg = 0; };
    Voice voice;
    std::array<Voice, 4> fadeVoices;   // voices that fade out after a retrigger
    void resetInstrument();
    void remapInstrument (const RenderResult& oldRes, const RenderResult& newRes);
    signalsmith::stretch::SignalsmithStretch<float> keysStretch;
    std::vector<float> keysIn[2], keysOut[2], keysSeek[2];
    int keysSeekLen = 0, keysHeldCount = 0, keysNote = -1, keysNextSeg = 0;
    float keysVel = 1.0f;
    std::array<int, 16> keysHeld {};
    float keysGain = 0.0f;
    bool keysRunning = false;
    double keysIndex = 0.0, keysRead = 0.0, freeBeat = 0.0;
    int lastMode = 0;
    FxChain fx;
    std::array<int, 64> fxSliceStarts {};

    // listening to a single slot (its own tempo, looping): published by the message thread
    void mixSlotPreview (juce::AudioBuffer<float>&, double rate);
    mutable juce::SpinLock slotPreviewLock;
    std::shared_ptr<const juce::AudioBuffer<float>> slotPreviewAudio;   // guarded by slotPreviewLock
    std::atomic<double> slotPreviewRate { 44100.0 };
    std::atomic<float> slotPreviewTrimStart { 0.0f }, slotPreviewTrimEnd { 1.0f };
    std::deque<std::shared_ptr<const juce::AudioBuffer<float>>> slotPreviewKeep;   // message thread: keeps old audio alive
    std::atomic<int> slotPreviewIndex { -1 }, slotPreviewVersion { 0 };
    std::shared_ptr<const juce::AudioBuffer<float>> slotPlaying;        // audio thread
    double slotPlayPos = 0.0, slotPlayStep = 1.0;
    float slotPlayGain = 0.0f;
    int slotPlayVersion = -1;

    juce::Array<juce::AudioProcessorParameter*> allParams;
    std::array<std::atomic<int>, 128> ccMap;              // CC number → index in allParams (-1 = none)
    std::atomic<int> learnParam { -1 }, midiLearnVersion { 0 }, midiEventVersion { 0 }, generateCount { 0 };
    std::atomic<bool> triggerWasOn { false };
    std::atomic<int> pendingTriggers { 0 };
    juce::String lastMidiEvent;
    std::atomic<int> lastNoteEvent { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceTribeProcessor)
};

} // namespace slicetribe
