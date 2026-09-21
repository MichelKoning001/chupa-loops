#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace slicetribe
{

constexpr int kNumSlots = 8;
constexpr double maxSampleSeconds = 128.0;   // longer files are truncated (32 bars at 60 bpm)

//==============================================================================
// Choices shown in the UI. The order here is the order of the parameter values.
//==============================================================================
namespace choices
{
    inline const juce::StringArray lengths   { "1", "2", "4", "8", "16", "32" };
    inline const int               lengthBars[] { 1, 2, 4, 8, 16, 32 };

    inline const juce::StringArray patterns  { "Free", "4 to the floor", "Offbeat", "Offbeat 2x",
                                               "Rolling 16th", "Rolling KBBB", "Gallop", "Broken", "Random" };

    inline const juce::StringArray sliceModes { "Grid", "Transient" };

    inline const juce::StringArray sliceSizes { "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar" };
    inline const double            sliceSteps[] { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 }; // in 1/16 steps

    inline const juce::StringArray motifs    { "Off", "1 bar", "2 bars", "4 bars" };
    inline const int               motifBars[] { 0, 1, 2, 4 };

    inline const juce::StringArray fills     { "Off", "4", "8", "16", "32" };
    inline const int               fillBars[] { 0, 4, 8, 16, 32 };

    inline const juce::StringArray midiModes { "Control", "Slices", "Keys" };
    inline const juce::StringArray feels     { "Normal", "Half time", "Double time" };
    inline const double            feelFactor[] { 1.0, 2.0, 0.5 };   // multiplies the source tempo

    inline const juce::StringArray styles    { "Clean", "Glitch", "Lo-Fi" };
    inline const juce::StringArray stretchModes { "Beats", "Smooth" };

    /** "Off" followed by 24 keys: C, Cm, C#, C#m ... B, Bm. index-1 = root*2 + minor */
    juce::StringArray keys();
}

enum Style   { styleClean = 0, styleGlitch, styleLoFi };
enum Stretch { stretchBeats = 0, stretchSmooth };

enum Pattern
{
    patFree = 0, patFourFloor, patOffbeat, patOffbeat2, patRolling, patRollingKBBB, patGallop, patBroken, patRandom
};

//==============================================================================
struct Settings
{
    int    bars        = 4;
    int    pattern     = patFree;
    int    sliceMode   = 0;       // 0 grid, 1 transient
    double sliceSteps  = 2.0;     // slice size in 1/16 steps
    float  sensitivity = 0.5f;    // 0..1
    float  chaos       = 0.3f;    // 0..1
    int    motifBars   = 2;       // 0 = off
    float  variation   = 0.15f;   // 0..1
    float  gate        = 1.0f;    // 0.1..1
    float  swing       = 0.0f;    // 0..1
    float  reverse     = 0.0f;    // 0..1
    float  octave      = 0.0f;    // 0..1
    float  fadeMs      = 3.0f;
    int    style       = styleClean;
    float  amount      = 0.5f;    // 0..1, strength of Glitch / Lo-Fi
    int    stretchMode = stretchBeats;
    int    fillBars    = 0;       // 0 = off: a roll at the end of every phrase of this many bars
    float  energy      = 0.0f;    // 0..1: the loop gets busier and more glitchy towards the end
    int    feel        = 0;       // 0 normal, 1 half time, 2 double time (speed of the source material)
    float  fit         = 0.0f;    // 0..2: how hard the loop stays out of the reference track's way
    std::array<float, 16> fitProfile {};   // how busy the reference track is on every 1/16 of a bar

    bool structureEquals (const Settings& o) const
    {
        return bars == o.bars && pattern == o.pattern && sliceSteps == o.sliceSteps
            && motifBars == o.motifBars;
    }
};

//==============================================================================
/** One note/slice position in the generated loop. */
struct Hit
{
    double startStep = 0;   // in 1/16 steps from loop start (un-swung)
    double lenSteps  = 1;
    int    bar       = 0;
    int    localIndex = 0;  // index within its bar
};

/** Per-hit random state. Everything the generator decides comes from these seeds,
    so knob changes feel smooth instead of re-shuffling everything. */
struct Arrangement
{
    juce::uint64 seed = 1;
    juce::uint64 rhythmSeed = 1;   // reverses, octaves and rolls: changed by "new rhythm", kept by "new sources"
    std::vector<juce::uint64> hitSeeds;
    std::vector<juce::uint8>  locked;
    std::vector<juce::uint8>  forceOwn;   // a re-rolled hit inside a repeated motif

    void resizeFor (size_t numHits);
    void regenerate (juce::uint64 newSeed);      // keeps locked hits
    void regenerateRhythm (juce::uint64 newSeed);// only the rhythm seed: the sources stay
    void regenerateSources (juce::uint64 salt);  // only the sources: the rhythm stays
    void reroll (size_t hitIndex, juce::uint64 salt);
    int  numLocked() const;
};

//==============================================================================
/** A loaded source sample (immutable audio shared between threads). */
struct SlotAudio
{
    std::shared_ptr<const juce::AudioBuffer<float>> original;
    double fileRate = 44100.0;
    juce::String name, path;
    double detectedBpm = 0.0;
    int detectedKey = -1;         // root*2 + minor (0..23), -1 = unknown
    std::vector<float> peaks;     // min/max pairs for the UI thumbnail
    std::array<float, 16> gridProfile {};   // how busy every 1/16 of a bar is (0..1), for FIT TO TRACK
    int loadId = 0;               // changes every time a new file is loaded
};

/** A slot made ready for playback at the current rate (and tempo in Smooth mode).
    Positions in `audio` are converted from beats with `beatLen`. */
struct PreparedSlot
{
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    std::shared_ptr<const juce::AudioBuffer<float>> audioOctave;  // +12 semitones (optional)
    double beatLen = 0.0;             // samples per beat inside `audio`
    double beatLenOctave = 0.0;       // samples per beat inside `audioOctave`
    std::vector<int> onsets;          // in `audio` samples (slice-mode sensitivity)
    std::vector<int> warpOnsets;      // attacks used by the Beats warp (fixed sensitivity)
    double hostBpm = 0, rate = 0, srcBpm = 0;
    float weight = 1.0f;
    int transpose = 0;                // effective (manual + key match)
    int mode = stretchBeats;
    bool withOctave = false;
    float onsetSensitivity = -1.0f;
    float rms = 0.0f;
    int loadId = -1;
};

struct SlotState
{
    bool   enabled = true;
    double bpmOverride = 0.0;   // 0 = auto
    int    transpose = 0;       // manual, -12..12
    float  weight = 1.0f;       // 0..2: how often slices are taken from this sample (1 = normal share)
    bool   reference = false;   // "my track": not sliced, the new loop fits around it
};

//==============================================================================
struct Segment
{
    juce::int64 start = 0, length = 0, srcStart = 0;
    int slot = 0;
    bool reversed = false, octave = false, locked = false, glitch = false;
    int hitIndex = 0;
};

struct RenderResult
{
    juce::AudioBuffer<float> audio;
    std::vector<Segment> segments;
    double bpm = 120.0, rate = 44100.0;
    int bars = 4;
    Settings settings;   // what this loop was rendered with
    int key = -1;        // key-match target it was rendered with (-1 = off)
    // MIDI slice notes: every different slice gets one key (C#1 = 37 and up; repeats share their key)
    static constexpr int firstSliceNote = 37, maxSliceNotes = 91;   // C#1 .. G8 (notes 37-127)
    std::vector<int> segmentNote;    // per segment: 0-based slice note, -1 = beyond the keyboard
    std::vector<int> noteSegment;    // per slice note: the segment it plays
    int numDifferentSlices = 0;      // may be more than maxSliceNotes
    double samplesPerBeat() const { return rate * 60.0 / bpm; }
    double lengthBeats() const    { return bars * 4.0; }
};

//==============================================================================
namespace engine
{
    /** Loads an audio file. Returns nullptr-audio on failure. */
    SlotAudio loadFile (juce::AudioFormatManager&, const juce::File&);

    /** Tempo guess from file name ("128bpm", "_140_") or loop length. */
    double detectBpm (const juce::String& fileName, double lengthSeconds, double hostBpm,
                      const juce::AudioBuffer<float>* audio = nullptr, double audioRate = 0.0);

    /** Beat analysis of the audio itself (60..200 BPM, 0 = no clear beat). */
    double detectBpmFromAudio (const juce::AudioBuffer<float>&, double sampleRate);
    /** Key from the audio (chroma + Krumhansl profiles); -1 when it isn't clear (drums, noise, one note). */
    int detectKeyFromAudio (const juce::AudioBuffer<float>&, double sampleRate);
    /** "kick", "drum", "hat", "perc" ... : such files are never given a key from their audio. */
    bool nameSuggestsDrums (const juce::String& fileName);

    /** Musical key from a file name (Splice style: "_Am_", "Fmin", "C#_major"). -1 if none. */
    int detectKey (const juce::String& fileName);
    juce::String keyName (int key);
    /** Semitones (-6..+5) that move a sample in `fromKey` to `toKey` (relative major/minor aware). */
    int keyShift (int fromKey, int toKey);

    /** High quality resample (Kaiser-windowed sinc, anti-aliased), treats the buffer as a loop. */
    juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>&, double fromRate, double toRate,
                                       const std::atomic<bool>* abort = nullptr);

    /** Time-stretch + transpose a loop to exactly outLength samples (Signalsmith Stretch). */
    juce::AudioBuffer<float> stretchLoop (const juce::AudioBuffer<float>&, double rate, int outLength, float semitones,
                                          const std::atomic<bool>* abort = nullptr);

    /** Beats mode: sample-accurate resampling only (tempo is handled by slicing).
        Smooth mode: phase-vocoder time-stretch to the host tempo. */
    PreparedSlot prepare (const SlotAudio&, const SlotState&, int effectiveTranspose, double hostBpm, double rate,
                          bool withOctave, int mode, const std::atomic<bool>* abort = nullptr);

    /** Transient detection. sensitivity 0..1; beatLen = samples per beat in this buffer. */
    std::vector<int> detectOnsets (const juce::AudioBuffer<float>&, double beatLen, float sensitivity);

    /** How busy every 1/16 of a bar is in this audio (0..1 per step), for FIT TO TRACK. */
    std::array<float, 16> gridProfile (const juce::AudioBuffer<float>&, double rate, double bpm);

    /** How good does this loop sound on paper? 0..1 (used by AUTO PICK). */
    double scoreLoop (const RenderResult&, double fillTargetScale = 1.0);

    /** Build the hit list for the given settings. */
    std::vector<Hit> buildHits (const Settings&, juce::uint64 seed);

    /** Renders the loop. */
    std::shared_ptr<RenderResult> render (const std::array<PreparedSlot, kNumSlots>&,
                                          const std::array<bool, kNumSlots>& enabled,
                                          const Settings&, const Arrangement&,
                                          const std::vector<Hit>&, double hostBpm, double rate,
                                          int onlySlot = -1);   // >= 0: only the slices of that sample (stems)

    /** Lo-Fi colouring (saturation, sample-rate & bit reduction, low-pass). Loop-seamless. */
    void applyLoFi (juce::AudioBuffer<float>&, double rate, float amount);

    /** Transparent look-ahead peak limiter (loop-seamless), ceiling in dBFS. */
    void applyLimiter (juce::AudioBuffer<float>&, double rate, float ceilingDb);

    std::vector<float> computePeaks (const juce::AudioBuffer<float>&, int numPoints);

    /** Writes a 24-bit WAV. */
    bool writeWav (const juce::File&, const juce::AudioBuffer<float>&, double rate, float gain);
}

} // namespace slicetribe
