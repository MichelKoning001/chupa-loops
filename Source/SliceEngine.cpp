#include "SliceEngine.h"

#include <signalsmith-stretch/signalsmith-stretch.h>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <deque>
#include <limits>

namespace slicetribe
{

//==============================================================================
// Random helpers (deterministic, seed based)
//==============================================================================
namespace
{
    inline juce::uint64 splitmix (juce::uint64& x) noexcept
    {
        x += 0x9E3779B97F4A7C15ull;
        juce::uint64 z = x;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    inline double uniform (juce::uint64& state) noexcept
    {
        return (double) (splitmix (state) >> 11) * (1.0 / 9007199254740992.0);
    }

    inline juce::uint64 hashCombine (juce::uint64 a, juce::uint64 b) noexcept
    {
        juce::uint64 s = a ^ (b * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull);
        splitmix (s);
        return splitmix (s);
    }

    inline juce::int64 wrap (juce::int64 i, juce::int64 len) noexcept
    {
        i %= len;
        return i < 0 ? i + len : i;
    }

    double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 64; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < sum * 1.0e-12) break;
        }
        return sum;
    }

    /** Raised-cosine fade 0..1 (sums to 1 with its mirror). */
    inline float fadeCurve (double x) noexcept
    {
        x = juce::jlimit (0.0, 1.0, x);
        return (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * x));
    }
}

//==============================================================================
// Arrangement
//==============================================================================
void Arrangement::resizeFor (size_t numHits)
{
    const auto old = hitSeeds.size();
    hitSeeds.resize (numHits);
    locked.resize (numHits, 0);
    forceOwn.resize (numHits, 0);
    charSeeds.resize (numHits, 0);

    for (size_t i = old; i < numHits; ++i)
        hitSeeds[i] = hashCombine (seed, (juce::uint64) i);
}

void Arrangement::regenerate (juce::uint64 newSeed)
{
    seed = newSeed;
    rhythmSeed = hashCombine (newSeed, 0x51CEull);
    for (size_t i = 0; i < hitSeeds.size(); ++i)
    {
        if (locked[i])
            continue;
        hitSeeds[i] = hashCombine (seed, (juce::uint64) i);
        forceOwn[i] = 0;
    }
}

void Arrangement::regenerateRhythm (juce::uint64 newSeed)
{
    // buildHits uses the seed (Free / Random patterns and the motif); rhythmSeed decides the
    // reverses, octaves and rolls. The per-hit seeds - and with them the sources - stay as they are.
    seed = newSeed;
    rhythmSeed = hashCombine (newSeed, 0x7A1Full);
}

void Arrangement::regenerateSources (juce::uint64 salt)
{
    // only new source seeds: forceOwn stays as it is, otherwise the motif repeat would be gone
    for (size_t i = 0; i < hitSeeds.size(); ++i)
        if (! locked[i])
            hitSeeds[i] = hashCombine (hitSeeds[i], salt);
}

/** The stream a hit's reverses, octaves and rolls come from. A locked hit keeps the one it was
    locked on, so locking freezes exactly what you were hearing and nothing changes afterwards. */
juce::uint64 Arrangement::characterSeedFor (size_t i) const
{
    if (i < locked.size() && locked[i] && i < charSeeds.size() && charSeeds[i] != 0)
        return charSeeds[i];
    if (i < locked.size() && locked[i])
        return hashCombine (i < hitSeeds.size() ? hitSeeds[i] : hashCombine (seed, (juce::uint64) i), 0x51CEull);   // older projects
    return rhythmSeed;
}

void Arrangement::setLocked (size_t i, bool isLocked)
{
    if (i >= locked.size())
        return;
    if (isLocked && charSeeds.size() > i)
        charSeeds[i] = rhythmSeed != 0 ? rhythmSeed : 1;   // freeze the sound it has at this moment
    locked[i] = isLocked ? 1 : 0;
}

void Arrangement::reroll (size_t i, juce::uint64 salt)
{
    if (i >= hitSeeds.size())
        return;
    hitSeeds[i] = hashCombine (hitSeeds[i], salt);
    forceOwn[i] = 1;
}

int Arrangement::numLocked() const
{
    int n = 0;
    for (auto l : locked) n += l ? 1 : 0;
    return n;
}

//==============================================================================
// Keys (Splice-style file names)
//==============================================================================
namespace
{
    const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

juce::StringArray choices::keys()
{
    juce::StringArray k { "Off" };
    for (auto* n : noteNames)
    {
        k.add (n);
        k.add (juce::String (n) + "m");
    }
    return k;
}

juce::String engine::keyName (int key)
{
    if (key < 0 || key > 23) return {};
    return juce::String (noteNames[key / 2]) + (key % 2 ? "m" : "");
}

int engine::detectKey (const juce::String& fileName)
{
    juce::StringArray tokens;
    tokens.addTokens (fileName, " _-.()[]{},", "");
    tokens.removeEmptyStrings();

    for (int t = tokens.size(); --t >= 0;)
    {
        const auto tok = tokens[t];
        const auto first = tok[0];
        if (first < 'A' || first > 'G')
            continue;
        static const int roots[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        int root = roots[first - 'A'];
        int i = 1;
        if (i < tok.length() && (tok[i] == '#' || tok[i] == 'b'))
        {
            root = (root + (tok[i] == '#' ? 1 : 11)) % 12;
            ++i;
        }
        const auto rest = tok.substring (i);
        const auto lower = rest.toLowerCase();
        int minor = -1;
        if (rest.isEmpty() || lower == "maj" || lower == "major" || rest == "M") minor = 0;
        else if (rest == "m" || lower == "min" || lower == "minor") minor = 1;
        if (minor < 0)
            continue;
        if (tok.length() == 1)
        {
            // a lone letter ("Take A bass", "Loop_A_02") is only a key when it sits next to a tempo, Splice style
            auto isTempo = [&] (int idx)
            {
                if (idx < 0 || idx >= tokens.size()) return false;
                const auto v = tokens[idx].retainCharacters ("0123456789").getIntValue();
                return tokens[idx].containsOnly ("0123456789bpmBPM") && v >= 60 && v <= 200;
            };
            if (! isTempo (t - 1) && ! isTempo (t + 1))
                continue;
        }
        return root * 2 + minor;
    }
    return -1;
}

//==============================================================================
// Tempo and key from the audio itself (for files whose name says nothing)
//==============================================================================
namespace
{
    /** Mono mix, low-passed and decimated to roughly `targetRate`. Analyses at most `maxSeconds`. */
    std::vector<float> monoDecimated (const juce::AudioBuffer<float>& b, double rate, double targetRate, double maxSeconds, double& outRate)
    {
        const int factor = juce::jmax (1, (int) std::floor (rate / targetRate));
        outRate = rate / factor;
        const int len = (int) juce::jmin<double> (b.getNumSamples(), rate * maxSeconds);
        std::vector<float> out ((size_t) (len / factor));
        if (out.empty() || b.getNumChannels() == 0)
            return out;

        // two cascaded one-pole-pairs: a 4th order low-pass at ~0.4 * the new Nyquist
        juce::IIRFilter f1, f2;
        const auto c = juce::IIRCoefficients::makeLowPass (rate, juce::jmin (rate * 0.45, outRate * 0.4));
        f1.setCoefficients (c);
        f2.setCoefficients (c);
        const float chanGain = 1.0f / (float) b.getNumChannels();
        size_t o = 0;
        for (int i = 0; i < len && o < out.size(); ++i)
        {
            float v = 0.0f;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                v += b.getSample (ch, i);
            v = f2.processSingleSampleRaw (f1.processSingleSampleRaw (v * chanGain));
            if (i % factor == 0)
                out[o++] = v;
        }
        out.resize (o);
        return out;
    }
}

double engine::detectBpmFromAudio (const juce::AudioBuffer<float>& b, double rate)
{
    if (rate <= 0.0 || b.getNumSamples() < rate * 1.5)
        return 0.0;

    // onset strength: rectified rise of the log energy in ~5.8 ms hops (at 11 kHz)
    double r = 0.0;
    auto x = monoDecimated (b, rate, 11025.0, 60.0, r);
    float peak = 0.0f;
    for (auto v : x) peak = juce::jmax (peak, std::abs (v));
    if (peak < 1.0e-5f)
        return 0.0;
    for (auto& v : x) v /= peak;   // level-independent onset strength
    const int hop = juce::jmax (1, (int) std::round (r * 0.0058));
    const int frames = (int) x.size() / hop;
    if (frames < 64)
        return 0.0;
    const double frameRate = r / hop;
    std::vector<float> env ((size_t) frames);
    float prev = 0.0f;
    const int win = hop * 4;   // ~23 ms energy window: fast beating of held notes is not an attack
    for (int f = 0; f < frames; ++f)
    {
        double e = 0.0;
        const int s0 = juce::jmax (0, f * hop + hop - win), s1 = f * hop + hop;
        for (int i = s0; i < s1; ++i)
            e += (double) x[(size_t) i] * x[(size_t) i];
        const float le = std::log (1.0f + 1000.0f * (float) (e / win));
        env[(size_t) f] = juce::jmax (0.0f, le - prev);
        prev = le;
    }
    // there must be real attacks (a pad fading in or a drone has none): count clear onsets
    int onsets = 0, lastOnset = -1000000;
    const int minGap = juce::jmax (1, (int) (frameRate * 0.06));
    for (int f = 0; f < frames; ++f)
        if (env[(size_t) f] > 0.5f && f - lastOnset >= minGap)
        {
            ++onsets;
            lastOnset = f;
        }
    if (onsets < 4)
        return 0.0;

    const float mean = std::accumulate (env.begin(), env.end(), 0.0f) / (float) frames;
    for (auto& v : env) v -= mean;
    double energy = 0.0;
    for (auto v : env) energy += (double) v * v;
    energy /= frames;
    if (energy <= 1.0e-12)
        return 0.0;   // no onsets at all (a pad, silence)

    // autocorrelation of the onset curve for 60..200 BPM, with a mild preference for 90..160
    auto acAt = [&] (double lag)
    {
        const int l0 = (int) lag;
        const float fr = (float) (lag - l0);
        if (l0 + 1 >= frames / 2)
            return 0.0;   // fewer than two periods: no evidence
        double sum = 0.0;
        for (int i = 0; i + l0 + 1 < frames; ++i)
            sum += env[(size_t) i] * ((1.0f - fr) * env[(size_t) (i + l0)] + fr * env[(size_t) (i + l0 + 1)]);
        return sum / (frames - l0 - 1);
    };
    double best = 0.0, bestScore = 0.0;
    for (double bpm = 60.0; bpm <= 200.0; bpm += 0.5)
    {
        const double lag = frameRate * 60.0 / bpm;
        // a beat is confirmed by its double (bar-level) and half (8th notes) as well
        const double score = acAt (lag) + 0.5 * acAt (lag * 2.0) + 0.25 * acAt (lag * 4.0) + 0.25 * acAt (lag * 0.5);
        const double prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 125.0) / 0.6, 2.0));
        if (score * prior > bestScore)
        {
            bestScore = score * prior;
            best = bpm;
        }
    }
    // confidence: the beat must stand out clearly against the onset energy (pads and noise don't)
    const double norm = bestScore / (energy * 2.0);
    return norm > 0.12 ? best : 0.0;
}

int engine::detectKeyFromAudio (const juce::AudioBuffer<float>& b, double rate)
{
    if (rate <= 0.0 || b.getNumSamples() < rate * 0.5)
        return -1;

    double r = 0.0;
    const auto x = monoDecimated (b, rate, 4000.0, 30.0, r);
    const int frame = juce::jlimit (512, 4096, juce::nextPowerOfTwo ((int) (r * 0.5)));
    const int hop = frame / 2;
    if ((int) x.size() < frame)
        return -1;

    std::vector<float> window ((size_t) frame);
    for (int i = 0; i < frame; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (frame - 1));

    // Goertzel energy per note (C2..C6), a little either side for detuned material; log-compressed per frame
    std::array<double, 12> chroma {};
    std::vector<float> buf ((size_t) frame);
    int usedFrames = 0;
    for (int start = 0; start + frame <= (int) x.size(); start += hop)
    {
        double frameEnergy = 0.0;
        for (int i = 0; i < frame; ++i)
        {
            buf[(size_t) i] = x[(size_t) (start + i)] * window[(size_t) i];
            frameEnergy += (double) buf[(size_t) i] * buf[(size_t) i];
        }
        if (frameEnergy < 1.0e-7 * frame)
            continue;   // silence
        std::array<double, 12> fc {};
        for (int note = 36; note <= 84; ++note)
        {
            double e = 0.0;
            for (double cents : { -25.0, 0.0, 25.0 })
            {
                const double hz = 440.0 * std::pow (2.0, (note - 69 + cents / 100.0) / 12.0);
                const double w = juce::MathConstants<double>::twoPi * hz / r;
                const double coeff = 2.0 * std::cos (w);
                double s1 = 0.0, s2 = 0.0;
                for (int i = 0; i < frame; ++i)
                {
                    const double s0 = buf[(size_t) i] + coeff * s1 - s2;
                    s2 = s1;
                    s1 = s0;
                }
                e = juce::jmax (e, s1 * s1 + s2 * s2 - coeff * s1 * s2);
            }
            fc[(size_t) (note % 12)] += std::sqrt (e);
        }
        const double mx = *std::max_element (fc.begin(), fc.end());
        if (mx <= 0.0)
            continue;
        for (int i = 0; i < 12; ++i)
            chroma[(size_t) i] += fc[(size_t) i] / mx;
        ++usedFrames;
    }
    if (usedFrames < 2)
        return -1;

    // Krumhansl-Kessler key profiles
    static const double major[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    auto corr = [&chroma] (const double* prof, int root)
    {
        double mc = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mc += chroma[(size_t) i]; mp += prof[i]; }
        mc /= 12; mp /= 12;
        double num = 0, dc = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const double a = chroma[(size_t) ((i + root) % 12)] - mc, p = prof[i] - mp;
            num += a * p; dc += a * a; dp += p * p;
        }
        return dc > 0 && dp > 0 ? num / std::sqrt (dc * dp) : 0.0;
    };

    int bestKey = -1;
    double best = -1.0;
    std::array<double, 24> scores {};
    for (int root = 0; root < 12; ++root)
        for (int m = 0; m < 2; ++m)
        {
            const double c = corr (m ? minor : major, root);
            scores[(size_t) (root * 2 + m)] = c;
            if (c > best) { best = c; bestKey = root * 2 + m; }
        }

    // how clear is it? The runner-up that is not the same scale (relative major/minor is fine: same notes)
    const int relative = bestKey % 2 ? ((bestKey / 2 + 3) % 12) * 2 : ((bestKey / 2 + 9) % 12) * 2 + 1;
    double second = -1.0;
    for (int k = 0; k < 24; ++k)
        if (k != bestKey && k != relative)
            second = juce::jmax (second, scores[(size_t) k]);

    // a chroma with one dominant note (a tuned kick, a single drone) says nothing about the scale
    const double cmax = *std::max_element (chroma.begin(), chroma.end());
    int strong = 0;
    for (auto v : chroma) strong += v >= 0.5 * cmax ? 1 : 0;

    if (best < 0.72 || best - second < 0.08 || strong < 3)
        return -1;
    return bestKey;
}

bool engine::nameSuggestsDrums (const juce::String& fileName)
{
    // whole words only ("Bottom", "That", "Grime" are not drums), a plural or a number may follow
    juce::StringArray words;
    juce::String cur;
    for (auto c : fileName.toLowerCase() + " ")
    {
        if (juce::CharacterFunctions::isLetter (c)) cur += c;
        else { if (cur.isNotEmpty()) words.add (cur); cur.clear(); }
    }
    static const char* drumWords[] = { "drum", "drums", "kick", "kicks", "snare", "snares", "hat", "hats", "hihat", "hihats", "hh", "oh", "ch",
                                       "perc", "percs", "percussion", "clap", "claps", "break", "breaks", "breakbeat", "beat", "beats",
                                       "top", "tops", "toploop", "shaker", "shakers", "ride", "rides", "cymbal", "crash", "tom", "toms",
                                       "rim", "rimshot", "fx", "sfx", "noise", "riser", "downlifter", "uplifter", "impact", "groove", "grooves", "loopdrum" };
    for (auto& w : words)
        for (auto* d : drumWords)
            if (w == d)
                return true;
    return false;
}

int engine::keyShift (int fromKey, int toKey)
{
    if (fromKey < 0 || toKey < 0) return 0;
    auto relMajor = [] (int k) { return ((k / 2) + (k % 2 ? 3 : 0)) % 12; };
    int d = ((relMajor (toKey) - relMajor (fromKey)) % 12 + 12) % 12;
    if (d > 5) d -= 12;
    return d;
}

//==============================================================================
// Loading & analysis
//==============================================================================
std::vector<float> engine::computePeaks (const juce::AudioBuffer<float>& b, int numPoints)
{
    std::vector<float> peaks ((size_t) numPoints * 2, 0.0f);
    const int len = b.getNumSamples();
    if (len <= 0 || numPoints <= 0)
        return peaks;

    for (int p = 0; p < numPoints; ++p)
    {
        const int s0 = (int) ((juce::int64) p * len / numPoints);
        const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (p + 1) * len / numPoints));
        float mn = 0.0f, mx = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto r = b.findMinMax (ch, s0, juce::jmin (s1, len) - s0);
            mn = juce::jmin (mn, r.getStart());
            mx = juce::jmax (mx, r.getEnd());
        }
        peaks[(size_t) p * 2]     = mn;
        peaks[(size_t) p * 2 + 1] = mx;
    }
    return peaks;
}

double engine::detectBpm (const juce::String& fileName, double seconds, double hostBpm,
                          const juce::AudioBuffer<float>* audio, double audioRate)
{
    if (seconds <= 0.0)
        return hostBpm;

    auto barsFor = [seconds] (double bpm) { return seconds * bpm / 240.0; };
    auto isWholeBars = [&] (double bpm)
    {
        const double bars = barsFor (bpm);
        const double r = std::round (bars);
        return r >= 1.0 && std::abs (bars - r) < 0.03 * r + 0.02;
    };

    // 1) Explicit "128bpm" / "bpm128" / "128 BPM"
    const auto lower = fileName.toLowerCase();
    const int bpmPos = lower.indexOf ("bpm");
    if (bpmPos >= 0)
    {
        // digits before
        int e = bpmPos;
        while (e > 0 && (lower[e - 1] == ' ' || lower[e - 1] == '_' || lower[e - 1] == '-'))
            --e;
        int s = e;
        while (s > 0 && (juce::CharacterFunctions::isDigit (lower[s - 1]) || lower[s - 1] == '.'))
            --s;
        double v = lower.substring (s, e).getDoubleValue();
        if (v < 50 || v > 250)
        {
            // digits after
            int a = bpmPos + 3;
            while (a < lower.length() && (lower[a] == ' ' || lower[a] == '_' || lower[a] == '-'))
                ++a;
            int b = a;
            while (b < lower.length() && (juce::CharacterFunctions::isDigit (lower[b]) || lower[b] == '.'))
                ++b;
            v = lower.substring (a, b).getDoubleValue();
        }
        if (v >= 50 && v <= 250)
        {
            // A loop dragged in from Splice (or a DAW) with "match tempo" on is already at the song tempo,
            // while its name still says the original tempo: trust the length in that case.
            if (! isWholeBars (v) && hostBpm > 0.0 && isWholeBars (hostBpm))
                return hostBpm;
            return v;
        }
    }

    // 2) A plain number in the name that matches the loop length
    {
        juce::StringArray tokens;
        juce::String current;
        for (auto c : lower)
        {
            if (juce::CharacterFunctions::isDigit (c) || c == '.')
                current += c;
            else
            {
                if (current.isNotEmpty()) tokens.add (current);
                current.clear();
            }
        }
        if (current.isNotEmpty()) tokens.add (current);

        for (auto& t : tokens)
        {
            const double v = t.getDoubleValue();
            if (v >= 70 && v <= 200 && isWholeBars (v))
                return v;
        }
    }

    // 3) From the length. A loop that is a whole number of bars at the song tempo is at the song tempo
    //    (a loop from the same project, a Splice drag with "match tempo"): keep it.
    for (int bars : { 1, 2, 4, 8, 16, 32 })
        if (std::abs (bars * 240.0 / seconds - hostBpm) < hostBpm * 0.005)
            return hostBpm;

    //    Otherwise the whole-bar count whose tempo is closest to the beat we hear (if it is clear)
    //    and to the host tempo
    const double heard = audio != nullptr ? detectBpmFromAudio (*audio, audioRate) : 0.0;
    double best = 0.0, bestScore = 1e9;
    for (int bars : { 1, 2, 4, 8, 16, 32 })
    {
        const double bpm = bars * 240.0 / seconds;
        if (bpm < 65.0 || bpm > 210.0)
            continue;
        const double score = heard > 0.0 ? std::abs (std::log (bpm / heard)) + 0.25 * std::abs (std::log (bpm / hostBpm))
                                          : std::abs (std::log (bpm / hostBpm));
        if (score < bestScore)
        {
            bestScore = score;
            best = bpm;
        }
    }
    if (best > 0.0)
        return best;
    return heard > 0.0 ? heard : hostBpm;   // no whole-bar tempo fits: the beat we hear
}

SlotAudio engine::loadFile (juce::AudioFormatManager& fm, const juce::File& file)
{
    SlotAudio s;
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate < 8000.0 || reader->sampleRate > 384000.0)
        return s;

    const int maxLen = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * maxSampleSeconds));
    const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);

    auto buf = std::make_shared<juce::AudioBuffer<float>> (chans, maxLen);
    reader->read (buf.get(), 0, maxLen, 0, true, chans > 1);

    s.original = buf;
    s.fileRate = reader->sampleRate;
    s.name     = file.getFileNameWithoutExtension();
    s.path     = file.getFullPathName();
    s.detectedKey = detectKey (s.name);
    if (s.detectedKey < 0 && ! nameSuggestsDrums (s.name))
        s.detectedKey = detectKeyFromAudio (*buf, s.fileRate);
    s.peaks    = computePeaks (*buf, 256);
    return s;
}

//==============================================================================
// DSP: resampling & stretching
//==============================================================================
juce::AudioBuffer<float> engine::resample (const juce::AudioBuffer<float>& in, double fromRate, double toRate,
                                           const std::atomic<bool>* abort)
{
    const int inLen = in.getNumSamples();
    if (inLen == 0 || std::abs (fromRate - toRate) < 0.01 || fromRate <= 0.0 || toRate <= 0.0)
        return in;
    auto aborted = [abort] { return abort != nullptr && abort->load(); };
    if (aborted())
        return {};

    // Band-limited (Kaiser-windowed sinc) polyphase resampler. The cutoff follows the lower of the two
    // rates (anti-aliased), and the input is treated as a loop so the loop point stays intact.
    const double ratio  = fromRate / toRate;                           // input samples per output sample
    const double cutoff = juce::jmin (1.0, toRate / fromRate) * 0.94;  // relative to input Nyquist
    constexpr int zeroCrossings = 16;
    const int halfTaps = (int) std::ceil (zeroCrossings / cutoff);
    const int taps = 2 * halfTaps;
    constexpr int phases = 4096;

    // table[phase][tap]: kernel value at distance (tap - halfTaps + 1 - frac), frac = phase / phases
    std::vector<float> table ((size_t) (phases + 1) * (size_t) taps);
    {
        const double beta = 8.6;
        const double i0b = besselI0 (beta);
        const double halfWidth = zeroCrossings / cutoff;
        for (int ph = 0; ph <= phases; ++ph)
        {
            if ((ph & 255) == 0 && aborted())
                return {};
            const double frac = (double) ph / phases;
            for (int t = 0; t < taps; ++t)
            {
                const double x = (double) (t - halfTaps + 1) - frac;
                double v = 0.0;
                if (std::abs (x) < halfWidth)
                {
                    const double r = x / halfWidth;
                    const double win = besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
                    const double arg = juce::MathConstants<double>::pi * cutoff * x;
                    v = cutoff * (std::abs (arg) < 1.0e-9 ? 1.0 : std::sin (arg) / arg) * win;
                }
                table[(size_t) ph * (size_t) taps + (size_t) t] = (float) v;
            }
        }
    }

    const int outLen = (int) std::llround (inLen * toRate / fromRate);
    const int pad = halfTaps + 2;
    juce::AudioBuffer<float> out (in.getNumChannels(), outLen);
    std::vector<float> padded ((size_t) (inLen + 2 * pad));

    for (int ch = 0; ch < in.getNumChannels(); ++ch)
    {
        const float* src = in.getReadPointer (ch);
        for (int n = 0; n < inLen + 2 * pad; ++n)
            padded[(size_t) n] = src[wrap (n - pad, inLen)];

        float* dst = out.getWritePointer (ch);
        for (int n = 0; n < outLen; ++n)
        {
            if ((n & 16383) == 0 && abort != nullptr && abort->load())
                return out;

            const double pos = n * ratio;
            const int i0 = (int) std::floor (pos);
            const double frac = pos - i0;
            const int ph = (int) std::lround (frac * phases);
            const float* k = table.data() + (size_t) ph * (size_t) taps;
            const float* x = padded.data() + (i0 - halfTaps + 1 + pad);
            float acc = 0.0f;
            for (int t = 0; t < taps; ++t)
                acc += k[t] * x[t];
            dst[n] = acc;
        }
    }
    return out;
}

juce::AudioBuffer<float> engine::stretchLoop (const juce::AudioBuffer<float>& in, double rate, int outLen, float semitones,
                                              const std::atomic<bool>* abort)
{
    const int ch = in.getNumChannels();
    const int inLen = in.getNumSamples();
    juce::AudioBuffer<float> out (ch, outLen);
    out.clear();
    if (inLen < 64 || outLen < 64 || (abort != nullptr && abort->load()))
        return out;

    signalsmith::stretch::SignalsmithStretch<float> st;
    st.presetDefault (ch, (float) rate);
    st.setTransposeSemitones (semitones);

    const double playbackRate = (double) inLen / (double) outLen;
    const int seekLen = st.outputSeekLength ((float) playbackRate);

    // Process two loop cycles plus a little extra and keep the second cycle.
    // The extra bit (the start of cycle 3) is crossfaded into the start of the kept cycle,
    // so the end of the loop flows into its beginning without a seam.
    const int xfade   = juce::jlimit (16, juce::jmax (16, outLen / 4), (int) (rate * 0.012));
    const int procOut = 2 * outLen + xfade;
    const int procIn  = (int) std::llround (procOut * playbackRate);
    const int totalIn = seekLen + procIn + 16;

    std::vector<std::vector<float>> tin ((size_t) ch, std::vector<float> ((size_t) totalIn));
    std::vector<std::vector<float>> tout ((size_t) ch, std::vector<float> ((size_t) procOut));
    for (int c = 0; c < ch; ++c)
    {
        const float* src = in.getReadPointer (c);
        for (int n = 0; n < totalIn; n += inLen)   // the loop repeated, copied in blocks
            std::copy (src, src + juce::jmin (inLen, totalIn - n), tin[(size_t) c].begin() + n);
    }
    if (abort != nullptr && abort->load())
        return out;

    std::vector<float*> inPtrs ((size_t) ch), chunkIn ((size_t) ch), chunkOut ((size_t) ch);
    for (int c = 0; c < ch; ++c)
        inPtrs[(size_t) c] = tin[(size_t) c].data();

    float** inP = inPtrs.data();
    st.outputSeek (inP, seekLen);
    if (abort != nullptr && abort->load())
        return out;

    // chunked, so a closing plugin never has to wait long
    const int chunk = (int) juce::jmax (4096.0, rate * 0.5);
    int inDone = 0, outDone = 0;
    while (outDone < procOut)
    {
        if (abort != nullptr && abort->load())
            return out;
        const int outEnd = juce::jmin (procOut, outDone + chunk);
        const int inEnd  = outEnd == procOut ? procIn : (int) std::llround (outEnd * playbackRate);
        for (int c = 0; c < ch; ++c)
        {
            chunkIn[(size_t) c]  = tin[(size_t) c].data() + seekLen + inDone;
            chunkOut[(size_t) c] = tout[(size_t) c].data() + outDone;
        }
        float** ci = chunkIn.data();
        float** co = chunkOut.data();
        st.process (ci, inEnd - inDone, co, outEnd - outDone);
        inDone = inEnd;
        outDone = outEnd;
    }

    for (int c = 0; c < ch; ++c)
    {
        float* dst = out.getWritePointer (c);
        const float* cycle = tout[(size_t) c].data() + outLen;
        const float* cont  = tout[(size_t) c].data() + 2 * outLen;
        for (int i = 0; i < outLen; ++i)
            dst[i] = cycle[i];
        for (int i = 0; i < xfade; ++i)
        {
            const float w = fadeCurve ((double) i / xfade);
            dst[i] = cont[i] * (1.0f - w) + cycle[i] * w;
        }
    }
    return out;
}

PreparedSlot engine::prepare (const SlotAudio& a, const SlotState& state, int effTranspose, double hostBpm, double rate,
                              bool withOctave, int mode, const std::atomic<bool>* abort)
{
    PreparedSlot p;
    p.hostBpm = hostBpm;
    p.rate = rate;
    p.transpose = effTranspose;
    p.withOctave = withOctave;
    p.mode = mode;
    p.weight = juce::jlimit (0.0f, 2.0f, state.weight);

    if (a.original == nullptr)
        return p;

    const double srcBpm = state.bpmOverride > 0.0 ? state.bpmOverride
                        : (a.detectedBpm > 0.0 ? a.detectedBpm : hostBpm);
    p.srcBpm = srcBpm;

    if (mode == stretchBeats)
    {
        // No time-stretching at all: pitch comes from resampling (like a classic sampler),
        // timing is handled while slicing, so attacks and gaps stay exactly as recorded.
        const double pitch = std::pow (2.0, effTranspose / 12.0);
        const double bufferRate = rate / pitch;
        p.audio = std::make_shared<juce::AudioBuffer<float>> (resample (*a.original, a.fileRate, bufferRate, abort));
        p.beatLen = bufferRate * 60.0 / srcBpm;
        if (withOctave && ! (abort != nullptr && abort->load()))
        {
            p.audioOctave = std::make_shared<juce::AudioBuffer<float>> (resample (*a.original, a.fileRate, bufferRate * 0.5, abort));
            p.beatLenOctave = p.beatLen * 0.5;
        }
    }
    else
    {
        auto buf = resample (*a.original, a.fileRate, rate, abort);
        if (abort != nullptr && abort->load())
            return p;
        const int len = buf.getNumSamples();
        const double srcBeatLen = rate * 60.0 / srcBpm;
        const double beats = len / srcBeatLen;
        const double roundedBeats = std::round (beats);

        int outLen;
        if (roundedBeats >= 1.0 && std::abs (beats - roundedBeats) < 0.06)
            outLen = (int) std::llround (roundedBeats * rate * 60.0 / hostBpm);
        else
            outLen = (int) std::llround (len * srcBpm / hostBpm);
        outLen = juce::jmax (64, outLen);

        const double ratio = (double) outLen / (double) len;
        if (effTranspose == 0 && std::abs (ratio - 1.0) < 0.003)
            p.audio = std::make_shared<juce::AudioBuffer<float>> (std::abs (ratio - 1.0) < 1.0e-9 ? buf : resample (buf, (double) len, (double) outLen, abort));
        else
            p.audio = std::make_shared<juce::AudioBuffer<float>> (stretchLoop (buf, rate, outLen, (float) effTranspose, abort));

        p.beatLen = rate * 60.0 / hostBpm;
        if (withOctave && ! (abort != nullptr && abort->load()))
        {
            p.audioOctave = std::make_shared<juce::AudioBuffer<float>> (stretchLoop (buf, rate, outLen, (float) effTranspose + 12.0f, abort));
            p.beatLenOctave = p.beatLen;
        }
    }

    // A file whose end does not flow into its start (not a perfect loop) gets a tiny fade at the seam,
    // so a slice that reads across it never clicks.
    auto declickSeam = [rate] (std::shared_ptr<const juce::AudioBuffer<float>>& buf)
    {
        if (buf == nullptr || buf->getNumSamples() < 512) return;
        const int n = buf->getNumSamples();
        float jump = 0.0f, typical = 0.0f;
        for (int ch = 0; ch < buf->getNumChannels(); ++ch)
        {
            jump = juce::jmax (jump, std::abs (buf->getSample (ch, 0) - buf->getSample (ch, n - 1)));
            for (int i = 1; i < 64; ++i)
                typical = juce::jmax (typical, std::abs (buf->getSample (ch, i) - buf->getSample (ch, i - 1)),
                                               std::abs (buf->getSample (ch, n - i) - buf->getSample (ch, n - i - 1)));
        }
        if (jump < 0.01f || jump < 3.0f * typical) return;
        auto copy = std::make_shared<juce::AudioBuffer<float>> (*buf);
        const int f = juce::jmin (n / 8, (int) (rate * 0.0015));
        for (int ch = 0; ch < copy->getNumChannels(); ++ch)
            for (int i = 0; i < f; ++i)
            {
                const float g = fadeCurve ((double) i / f);
                copy->setSample (ch, i, copy->getSample (ch, i) * g);
                copy->setSample (ch, n - 1 - i, copy->getSample (ch, n - 1 - i) * g);
            }
        buf = copy;
    };
    declickSeam (p.audio);
    declickSeam (p.audioOctave);

    p.rms = p.audio->getNumSamples() > 0 ? p.audio->getRMSLevel (0, 0, p.audio->getNumSamples()) : 0.0f;
    return p;
}

std::vector<int> engine::detectOnsets (const juce::AudioBuffer<float>& b, double beatLen, float sensitivity)
{
    std::vector<int> onsets;
    const int len = b.getNumSamples();
    constexpr int hop = 128;
    const int frames = len / hop;
    if (frames < 4)
    {
        onsets.push_back (0);
        return onsets;
    }

    // Energy envelope (mono)
    std::vector<double> logE ((size_t) frames);
    double maxLog = -200.0;
    for (int f = 0; f < frames; ++f)
    {
        double e = 0.0;
        for (int i = 0; i < hop * 2; ++i)
        {
            const int idx = f * hop + i;
            if (idx >= len) break;
            float s = 0.0f;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                s += b.getSample (ch, idx);
            e += (double) s * s;
        }
        logE[(size_t) f] = 10.0 * std::log10 (e / (hop * 2) + 1.0e-12);
        maxLog = juce::jmax (maxLog, logE[(size_t) f]);
    }

    std::vector<double> novelty ((size_t) frames, 0.0);
    for (int f = 3; f < frames; ++f)
    {
        const double prev = (logE[(size_t) f - 1] + logE[(size_t) f - 2] + logE[(size_t) f - 3]) / 3.0;
        novelty[(size_t) f] = juce::jmax (0.0, logE[(size_t) f] - prev);
    }

    const double threshold = juce::jmap ((double) sensitivity, 0.0, 1.0, 9.0, 1.5);   // dB rise
    const int minGap = juce::jmax (hop, (int) (beatLen / 8.0));                         // 1/32 note
    const double floorDb = maxLog - 45.0;

    onsets.push_back (0);
    for (int f = 3; f < frames - 1; ++f)
    {
        const auto n = novelty[(size_t) f];
        if (n < threshold || n < novelty[(size_t) f - 1] || n < novelty[(size_t) f + 1] || logE[(size_t) f] < floorDb)
            continue;

        // Refine: short RMS envelope inside the detection window, find where the rise crosses halfway
        const int w0 = juce::jmax (0, f * hop - hop);
        const int w1 = juce::jmin (len, f * hop + 3 * hop);
        constexpr int env = 32;
        auto level = [&] (int start)
        {
            double e = 0.0;
            for (int i = start; i < juce::jmin (len, start + env); ++i)
            {
                float s = 0.0f;
                for (int ch = 0; ch < b.getNumChannels(); ++ch)
                    s += b.getSample (ch, i);
                e += (double) s * s;
            }
            return std::sqrt (e / env);
        };
        const double startLevel = level (w0);
        double peakLevel = startLevel;
        for (int i = w0; i < w1; i += 8)
            peakLevel = juce::jmax (peakLevel, level (i));
        const double target = startLevel + 0.5 * (peakLevel - startLevel);
        int pos = f * hop;
        for (int i = w0; i < w1; i += 4)
            if (level (i) >= target) { pos = i; break; }

        // move back to a quiet point (close to a zero crossing) just before the attack
        int best = pos;
        float bestAbs = 1e9f;
        for (int i = pos; i > juce::jmax (0, pos - 48); --i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                s += std::abs (b.getSample (ch, i));
            if (s < bestAbs) { bestAbs = s; best = i; }
        }
        pos = best;

        if (pos - onsets.back() >= minGap)
            onsets.push_back (pos);
    }
    return onsets;
}

//==============================================================================
// Pattern generation
//==============================================================================
namespace
{
    using BarPattern = std::vector<std::pair<double, double>>;   // start, length (1/16 steps)

    BarPattern perBeat (std::initializer_list<std::pair<double, double>> beat)
    {
        BarPattern p;
        for (int b = 0; b < 4; ++b)
            for (auto& h : beat)
                p.push_back ({ b * 4.0 + h.first, h.second });
        return p;
    }

    BarPattern makeBar (const Settings& s, juce::uint64 seed, int barIndex)
    {
        switch (s.pattern)
        {
            case patFourFloor:     return perBeat ({ { 0, 4 } });
            case patOffbeat:       return perBeat ({ { 2, 2 } });
            case patOffbeat2:      return perBeat ({ { 2, 1 }, { 3, 1 } });
            case patRolling:       return perBeat ({ { 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 } });
            case patRollingKBBB:   return perBeat ({ { 1, 1 }, { 2, 1 }, { 3, 1 } });
            case patGallop:        return perBeat ({ { 0, 2 }, { 2, 1 }, { 3, 1 } });
            case patBroken:        return { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 10, 1 }, { 11, 3 }, { 14, 2 } };
            case patRandom:
            {
                BarPattern p;
                juce::uint64 st = hashCombine (seed ^ 0xB0B0B0B0ull, (juce::uint64) barIndex + 1000);
                double pos = 0;
                while (pos < 16.0)
                {
                    if (uniform (st) < 0.58)
                    {
                        const double r = uniform (st);
                        double len = r < 0.55 ? 1.0 : (r < 0.88 ? 2.0 : 3.0);
                        len = juce::jmin (len, 16.0 - pos);
                        p.push_back ({ pos, len });
                        pos += len;
                    }
                    else
                        pos += 1.0;
                }
                if (p.empty()) p.push_back ({ 0, 2 });
                return p;
            }
            case patFree:
            default:
            {
                BarPattern p;
                const double step = juce::jlimit (0.5, 16.0, s.sliceSteps);
                for (double pos = 0; pos < 16.0 - 1.0e-9; pos += step)
                    p.push_back ({ pos, step });
                return p;
            }
        }
    }

    int motifLength (const Settings& s)
    {
        return s.motifBars > 0 ? juce::jmin (s.motifBars, s.bars) : s.bars;
    }
}

std::vector<Hit> engine::buildHits (const Settings& s, juce::uint64 seed)
{
    const int M = motifLength (s);
    std::vector<BarPattern> bars;
    for (int b = 0; b < M; ++b)
        bars.push_back (makeBar (s, seed, b));

    std::vector<Hit> hits;
    for (int b = 0; b < s.bars; ++b)
    {
        const auto& bp = bars[(size_t) (b % M)];
        for (size_t k = 0; k < bp.size(); ++k)
        {
            Hit h;
            h.startStep  = b * 16.0 + bp[k].first;
            h.lenSteps   = bp[k].second;
            h.bar        = b;
            h.localIndex = (int) k;
            hits.push_back (h);
        }
    }

    // Fill: a hit that runs into the fill zone (the last half bar of a phrase) is cut there,
    // so the fill always gets its own slices - also with long (1/2 bar, 1 bar) slices.
    if (s.fillBars > 0)
    {
        const double phrase = 16.0 * juce::jmin (s.fillBars, s.bars);
        const double zone = juce::jmin (8.0, phrase * 0.5);
        std::vector<Hit> split;
        split.reserve (hits.size() + 8);
        for (const auto& h : hits)
        {
            const double zoneStart = std::floor (h.startStep / phrase) * phrase + phrase - zone;
            if (h.startStep < zoneStart - 1.0e-6 && h.startStep + h.lenSteps > zoneStart + 1.0e-6)
            {
                Hit a = h, b = h;
                a.lenSteps = zoneStart - h.startStep;
                b.startStep = zoneStart;
                b.lenSteps = h.startStep + h.lenSteps - zoneStart;
                b.bar = (int) (zoneStart / 16.0);
                b.localIndex = h.localIndex + 1000;   // its own random choice
                split.push_back (a);
                split.push_back (b);
            }
            else
                split.push_back (h);
        }
        hits.swap (split);
    }
    return hits;
}

//==============================================================================
// Rendering
//==============================================================================
namespace
{
    struct Choice
    {
        int slot = 0, hitIndex = 0, glitch = 0, glitchDiv = 1;
        double srcBeats = 0.0;                 // start position inside the source, in beats
        juce::int64 outStart = 0, lenOut = 0;
        bool reversed = false, octave = false, locked = false;
    };

    struct Piece
    {
        const juce::AudioBuffer<float>* src = nullptr;
        const std::vector<int>* onsets = nullptr;
        double onsetScale = 1.0;
        int slot = 0, hitIndex = 0, glitch = 0, glitchDiv = 1;
        juce::int64 outStart = 0, outLen = 0;
        double srcStart = 0.0;                 // in source samples
        bool reversed = false, hitStart = true, hitEnd = true, transientStart = false;
        int fadeIn = 0, tail = 0;
        double readLimit = std::numeric_limits<double>::infinity();   // next attack in the source
        int loopLen = 0, loopFade = 0;
        bool powerIn = false, powerOut = false;
        float slotRms = 0.0f;
    };

    struct Alignment { int offset = 0; double best = 0.0, atZero = 0.0; };

    /** Finds the offset for `cur` that best continues the waveform of `prev` (coarse-to-fine search). */
    Alignment findAlignment (const juce::AudioBuffer<float>& a, double contIdx, const juce::AudioBuffer<float>& b, double curIdx,
                             int window, int range)
    {
        Alignment r;
        const juce::int64 la = a.getNumSamples(), lb = b.getNumSamples();
        std::vector<float> tail ((size_t) window);
        double eA = 0.0;
        for (int j = 0; j < window; ++j)
        {
            tail[(size_t) j] = a.getSample (0, (int) wrap ((juce::int64) contIdx + j, la));
            eA += (double) tail[(size_t) j] * tail[(size_t) j];
        }
        if (eA < 1.0e-9)
            return r;

        const float* bp = b.getReadPointer (0);
        auto corr = [&] (int d, int stride)
        {
            double xy = 0.0, e = 0.0, ea = 0.0;
            const juce::int64 base = (juce::int64) curIdx + d;
            for (int j = 0; j < window; j += stride)
            {
                const float v = bp[wrap (base + j, lb)];
                xy += (double) tail[(size_t) j] * v;
                e  += (double) v * v;
                ea += (double) tail[(size_t) j] * tail[(size_t) j];
            }
            return (e < 1.0e-12 || ea < 1.0e-12) ? 0.0 : xy / std::sqrt (ea * e);
        };

        r.atZero = corr (0, 1);
        r.best = r.atZero;
        if (range <= 0)
            return r;

        const int lagStep = range > 48 ? 4 : 1;
        const int stride  = window > 256 ? 4 : 1;
        int coarseBest = 0;
        double coarseVal = -2.0;
        for (int d = -range; d <= range; d += lagStep)
        {
            const double c = corr (d, stride);
            if (c > coarseVal) { coarseVal = c; coarseBest = d; }
        }
        for (int d = juce::jmax (-range, coarseBest - lagStep); d <= juce::jmin (range, coarseBest + lagStep); ++d)
        {
            const double c = corr (d, 1);
            if (c > r.best) { r.best = c; r.offset = d; }
        }
        return r;
    }
}

std::shared_ptr<RenderResult> engine::render (const std::array<PreparedSlot, kNumSlots>& slots,
                                              const std::array<bool, kNumSlots>& enabled,
                                              const Settings& s, const Arrangement& arr,
                                              const std::vector<Hit>& hits, double hostBpm, double rate, int onlySlot)
{
    auto result = std::make_shared<RenderResult>();
    result->bpm  = hostBpm;
    result->rate = rate;
    result->bars = s.bars;
    result->settings = s;

    const double beatLen = rate * 60.0 / hostBpm;      // host beat in output samples
    const double stepLen = beatLen / 4.0;
    const juce::int64 outLen = juce::jmax<juce::int64> (1, std::llround (s.bars * 16.0 * stepLen));

    result->audio.setSize (2, (int) outLen);
    result->audio.clear();

    std::vector<int> active;
    for (int i = 0; i < kNumSlots; ++i)
        if (enabled[(size_t) i] && slots[(size_t) i].audio != nullptr && slots[(size_t) i].audio->getNumSamples() > 64
            && slots[(size_t) i].beatLen > 1.0)
            active.push_back (i);

    if (active.empty() || hits.empty())
        return result;

    const int M = motifLength (s);
    const bool motifActive = M < s.bars;

    std::vector<size_t> barFirst ((size_t) s.bars + 1, hits.size());
    for (size_t h = hits.size(); h-- > 0;)
        if (juce::isPositiveAndBelow (hits[h].bar, (int) barFirst.size()))
            barFirst[(size_t) hits[h].bar] = h;

    auto bufferOf = [&] (int slot, bool octave) -> const juce::AudioBuffer<float>&
    {
        const auto& ps = slots[(size_t) slot];
        return (octave && ps.audioOctave != nullptr) ? *ps.audioOctave : *ps.audio;
    };
    auto beatLenOf = [&] (int slot, bool octave)
    {
        const auto& ps = slots[(size_t) slot];
        return (octave && ps.audioOctave != nullptr) ? ps.beatLenOctave : ps.beatLen;
    };

    // ---------------------------------------------------------------- 1. decide every hit
    std::vector<Choice> choicesOut;
    choicesOut.reserve (hits.size());

    // a fill is played whatever your own track is doing
    auto inFill = [&s] (const Hit& hit)
    {
        if (s.fillBars <= 0)
            return false;
        const double span = 16.0 * juce::jmin (s.fillBars, s.bars);
        return std::fmod (hit.startStep, span) >= span - juce::jmin (8.0, span * 0.5) - 1.0e-6;
    };

    // FIT TO TRACK skips hits that land on a busy spot in your own track. If your track and the
    // rhythm you picked happen to agree, that would skip everything and leave you with silence, so
    // every bar keeps a few hits: the ones on the quietest spots in your track, a quarter of the
    // bar at FIT 100% and less as FIT goes up. The choice is made before anything is decided per
    // hit and ignores locks, so locking a slice never moves another one.
    std::vector<char> fitProtected (hits.size(), (char) 0);
    if (s.fit > 0.001f && ! hits.empty())
    {
        int maxBar = 0;
        for (const auto& hit : hits) maxBar = juce::jmax (maxBar, hit.bar);
        std::vector<std::vector<size_t>> perBar ((size_t) maxBar + 1);
        for (size_t i = 0; i < hits.size(); ++i)
            if (hits[i].bar >= 0 && ! inFill (hits[i]))
                perBar[(size_t) hits[i].bar].push_back (i);

        auto stepOf = [&hits] (size_t i) { return juce::jlimit (0, 15, ((int) std::llround (hits[i].startStep)) % 16); };
        auto busyAt = [&s, &stepOf] (size_t i) { return s.fitProfile[(size_t) stepOf (i)]; };
        // when your track is equally busy everywhere, keep the strong beats: 1 first, then 3, then 2 and 4
        auto beatStrength = [&stepOf] (size_t i)
        {
            const int st = stepOf (i);
            return st % 16 == 0 ? 4 : st % 8 == 0 ? 3 : st % 4 == 0 ? 2 : st % 2 == 0 ? 1 : 0;
        };
        const double share = juce::jlimit (0.05, 1.0, 0.25 / juce::jmax (1.0, (double) s.fit));
        for (auto& bar : perBar)
        {
            if (bar.empty())
                continue;
            std::stable_sort (bar.begin(), bar.end(), [&] (size_t a, size_t b)
            {
                if (busyAt (a) != busyAt (b))             return busyAt (a) < busyAt (b);
                if (beatStrength (a) != beatStrength (b)) return beatStrength (a) > beatStrength (b);
                return a < b;
            });
            const int keep = juce::jlimit (1, (int) bar.size(), (int) std::ceil ((double) bar.size() * share));
            for (int k = 0; k < keep; ++k)
                fitProtected[bar[(size_t) k]] = (char) 1;
        }
    }

    for (size_t h = 0; h < hits.size(); ++h)
    {
        const auto& hit = hits[h];
        const juce::uint64 own = h < arr.hitSeeds.size() ? arr.hitSeeds[h] : hashCombine (arr.seed, h);
        const bool isLocked = h < arr.locked.size() && arr.locked[h];
        const bool forced   = h < arr.forceOwn.size() && arr.forceOwn[h];

        juce::uint64 seedToUse = own;
        if (motifActive && hit.bar >= M && ! isLocked && ! forced)
        {
            juce::uint64 t = own;
            const double uVar = uniform (t);
            if (uVar >= s.variation)
            {
                const size_t base = barFirst[(size_t) (hit.bar % M)] + (size_t) hit.localIndex;
                if (base < arr.hitSeeds.size())
                    seedToUse = arr.hitSeeds[base];
            }
        }

        juce::uint64 st = seedToUse;
        uniform (st);
        const double uSlot  = uniform (st);
        const double uChaos = uniform (st);
        const double uPos   = uniform (st);
        // The "character" of a hit (reverse, octave, rolls) comes from its own stream, so a new rhythm
        // keeps the sources. A locked hit uses its own seed for that too, so it never changes at all.
        const juce::uint64 characterSeed = arr.characterSeedFor (h);
        juce::uint64 rt = hashCombine (seedToUse, characterSeed ^ 0x9E3779B97F4A7C15ull);
        const double uRev   = uniform (rt);
        const double uOct   = uniform (rt);

        // swing moves every odd 1/16; a hit ends where the (possibly swung) next position starts
        auto swungPos = [&] (double step)
        {
            const double r = std::round (step);
            const bool odd = std::abs (step - r) < 1.0e-6 && ((juce::int64) r) % 2 != 0;
            return step * stepLen + (odd ? s.swing * 0.5 * stepLen : 0.0);
        };
        const double startPos = swungPos (hit.startStep);
        const double endPos   = swungPos (hit.startStep + hit.lenSteps);
        const juce::int64 lenOut = juce::jmax<juce::int64> (32, std::llround ((endPos - startPos) * s.gate));

        // position inside a source, in beats
        auto choose = [&] (double slotDraw, double posDraw, bool keepPosition, bool octaveOn, int& slotOut, double& beatsOut)
        {
            {
                // every sample has a share: its weight (0 = never, 1 = normal, 2 = twice as often)
                double total = 0.0;
                for (int a2 : active) total += juce::jmax (0.0f, slots[(size_t) a2].weight);
                int picked = active.back();
                if (total > 1.0e-6)
                {
                    double x = slotDraw * total, acc = 0.0;
                    for (int a2 : active)
                    {
                        acc += juce::jmax (0.0f, slots[(size_t) a2].weight);
                        if (x < acc) { picked = a2; break; }
                    }
                }
                else
                    picked = active[(size_t) juce::jmin ((int) active.size() - 1, (int) (slotDraw * active.size()))];
                slotOut = picked;
            }
            const auto& ps = slots[(size_t) slotOut];
            const auto& src = bufferOf (slotOut, octaveOn);
            const double bl = beatLenOf (slotOut, octaveOn);
            const double srcBeats = src.getNumSamples() / bl;
            const double loopBeats = juce::jmax (1.0, std::floor (srcBeats + 0.05));
            const double startBeat = hit.startStep / 4.0;
            const double onsetScale = (octaveOn && ps.audioOctave != nullptr) ? ps.beatLenOctave / ps.beatLen : 1.0;

            beatsOut = 0.0;
            if (srcBeats < 0.75)
                return;   // one-shot: always from the start

            if (keepPosition)
            {
                // "groove-locked": a random beat anywhere in the loop, at the same spot within the beat
                const double phaseInBeat = startBeat - std::floor (startBeat);
                const int beatCount = juce::jmax (1, (int) loopBeats);
                const int beatIndex = juce::jmin (beatCount - 1, (int) (posDraw * beatCount));
                beatsOut = beatIndex + phaseInBeat;
                if (s.sliceMode == 1 && ps.onsets.size() > 1)
                {
                    const double window = 0.15;   // beats: snap to the nearest transient
                    double bestDist = window, snapped = beatsOut;
                    for (int o : ps.onsets)
                    {
                        const double ob = o * onsetScale / bl;
                        const double d = std::abs (ob - beatsOut);
                        if (d < bestDist) { bestDist = d; snapped = ob; }
                    }
                    beatsOut = snapped;
                }
            }
            else if (s.sliceMode == 1 && ps.onsets.size() > 1)
            {
                const int o = ps.onsets[(size_t) juce::jmin ((int) ps.onsets.size() - 1, (int) (posDraw * ps.onsets.size()))];
                beatsOut = o * onsetScale / bl;
            }
            else
            {
                const double sliceBeats = juce::jmax (0.125, s.sliceSteps / 4.0);
                const int numSlices = juce::jmax (1, (int) std::floor (loopBeats / sliceBeats + 1.0e-6));
                beatsOut = juce::jmin (numSlices - 1, (int) (posDraw * numSlices)) * sliceBeats;
            }
        };

        auto isSilent = [&] (int slotIdx, double beats, bool octaveOn)
        {
            const auto& ps = slots[(size_t) slotIdx];
            const auto& src = bufferOf (slotIdx, octaveOn);
            const double bl = beatLenOf (slotIdx, octaveOn);
            const juce::int64 len = src.getNumSamples();
            const juce::int64 n = juce::jmin<juce::int64> ((juce::int64) (lenOut * bl / beatLen), (juce::int64) (bl * 0.375));
            const juce::int64 p0 = (juce::int64) (beats * bl);
            double e = 0.0;
            for (juce::int64 j = 0; j < n; j += 2)
            {
                const float v = src.getSample (0, (int) wrap (p0 + j, len));
                e += (double) v * v;
            }
            const double rms = std::sqrt (e / juce::jmax<juce::int64> (1, n / 2));
            return rms < 0.18 * ps.rms;
        };

        const bool octaveOn = uOct < s.octave;
        const bool keepPos = uChaos >= s.chaos;
        int slot = 0;
        double beats = 0.0;
        choose (uSlot, uPos, keepPos, octaveOn, slot, beats);

        if (s.pattern != patFree && isSilent (slot, beats, octaveOn))
        {
            juce::uint64 alt = hashCombine (seedToUse, 0x51A7Eull);
            for (int attempt = 1; attempt <= 8; ++attempt)
            {
                const double aSlot = uniform (alt), aPos = uniform (alt);
                int sl = 0; double bt = 0.0;
                choose (aSlot, aPos, keepPos && attempt <= 3, octaveOn, sl, bt);
                if (! isSilent (sl, bt, octaveOn))
                {
                    slot = sl;
                    beats = bt;
                    break;
                }
            }
        }

        Choice c;
        c.slot = slot;
        c.srcBeats = beats;
        c.outStart = std::llround (startPos);
        c.lenOut = lenOut;
        c.reversed = uRev < s.reverse;
        c.octave = octaveOn && slots[(size_t) slot].audioOctave != nullptr;
        c.locked = isLocked;
        c.hitIndex = (int) h;

        if (s.style == styleGlitch)
        {
            juce::uint64 g = hashCombine (seedToUse, 0x611C4ull);
            const double uG = uniform (g), uKind = uniform (g), uK = uniform (g);
            if (uG < 0.12 + 0.6 * s.amount)
            {
                c.glitch = 1 + juce::jmin (4, (int) (uKind * 5.0));
                static const int ks[] = { 2, 3, 4, 6, 8 };
                c.glitchDiv = ks[juce::jlimit (0, 4, (int) (uK * (2.0 + 3.0 * s.amount)))];
            }
        }
        // FIT TO TRACK: skip hits that land where your own track is already busy
        if (s.fit > 0.001f && ! isLocked && ! fitProtected[h] && ! inFill (hit))   // a fill always stays
        {
            const int step = ((int) std::llround (hit.startStep)) % 16;
            const float busy = s.fitProfile[(size_t) juce::jlimit (0, 15, step)];
            juce::uint64 ft = hashCombine (rt, 0xF17ull);
            if (busy > 0.35f && uniform (ft) < juce::jlimit (0.0, 0.95, (double) (s.fit * (busy - 0.35f) / 0.65f)))
                continue;   // leave this spot to the track
        }

        // energy: the further into the loop, the more glitch, octaves and reverses
        if (s.energy > 0.001f)
        {
            const double pos = s.bars > 0 ? juce::jlimit (0.0, 1.0, hit.startStep / (16.0 * s.bars)) : 0.0;
            const double amount = s.energy * pos;
            juce::uint64 e = hashCombine (rt, 0xE11Eull);
            const double eGlitch = uniform (e), eKind = uniform (e), eOct = uniform (e), eRev = uniform (e);
            if (eGlitch < amount * 0.55 && c.glitch == 0)
            {
                c.glitch = eKind < 0.6 ? 1 : 2;
                c.glitchDiv = amount < 0.4 ? 2 : (amount < 0.7 ? 4 : 8);
            }
            if (eOct < amount * 0.3 && slots[(size_t) c.slot].audioOctave != nullptr) c.octave = true;
            if (eRev < amount * 0.12) c.reversed = true;
        }

        // phrase fill: the last half bar of every phrase rolls, faster and faster
        if (s.fillBars > 0)
        {
            const double phraseSteps = 16.0 * juce::jmin (s.fillBars, s.bars);
            const double inPhrase = std::fmod (hit.startStep, phraseSteps);
            const double zone = juce::jmin (8.0, phraseSteps * 0.5);
            if (inPhrase >= phraseSteps - zone - 1.0e-6)
            {
                const double into = (inPhrase - (phraseSteps - zone)) / zone;   // 0 .. 1
                juce::uint64 f = hashCombine (rt, 0xF111ull);
                c.glitch = uniform (f) < 0.5 ? 1 : 2;                               // stutter or rising stutter
                c.glitchDiv = into < 0.5 ? 2 : (into < 0.75 ? 4 : 8);
                c.reversed = false;
            }
        }
        choicesOut.push_back (c);
    }

    // ---------------------------------------------------------------- 2. cut hits into pieces
    // Beats mode ("warp"): a source at another tempo is cut at its transients (and on a 1/16 grid where
    // there are none). Every piece plays at its original speed and its attack lands exactly on the host
    // grid. When a piece has to last longer than its source material, the part before the next attack
    // is looped (crossfaded), so notes sustain naturally and attacks are never doubled.
    const int hitFade = juce::jmax ((int) (0.0015 * rate), (int) (s.fadeMs * 0.001 * rate));
    const double guard = rate * 0.0015;
    std::vector<Piece> pieces;
    pieces.reserve (choicesOut.size() * 4);

    auto transientsOf = [&] (int slot, bool octave, std::vector<double>& out)
    {
        const auto& ps = slots[(size_t) slot];
        const double scale = (octave && ps.audioOctave != nullptr) ? ps.beatLenOctave / ps.beatLen : 1.0;
        out.clear();
        for (int o : ps.warpOnsets)
            out.push_back (o * scale);
    };

    // first transient strictly after `pos` (positions may run past the buffer end: the source loops)
    auto nextTransient = [] (const std::vector<double>& tr, double bufLen, double pos)
    {
        if (tr.empty()) return std::numeric_limits<double>::infinity();
        const double base = std::floor (pos / bufLen) * bufLen;
        for (int k = 0; k < 3; ++k)
            for (double o : tr)
                if (o + base + k * bufLen > pos)
                    return o + base + k * bufLen;
        return std::numeric_limits<double>::infinity();
    };

    auto nearTransient = [] (const std::vector<double>& tr, double bufLen, double pos, double tol)
    {
        const double p = std::fmod (std::fmod (pos, bufLen) + bufLen, bufLen);
        for (double o : tr)
            if (std::abs (o - p) < tol || std::abs (o - p + bufLen) < tol || std::abs (o - p - bufLen) < tol)
                return true;
        return false;
    };

    std::vector<double> tr;
    for (const auto& c : choicesOut)
    {
        const auto& ps = slots[(size_t) c.slot];
        const auto& src = bufferOf (c.slot, c.octave);
        const double bufLen = src.getNumSamples();
        const double bl = beatLenOf (c.slot, c.octave);
        const double ratio = bl / beatLen;      // source samples per output sample
        const double srcStart = c.srcBeats * bl;
        transientsOf (c.slot, c.octave, tr);

        Piece base;
        base.src = &src;
        base.onsets = &ps.warpOnsets;
        base.onsetScale = (c.octave && ps.audioOctave != nullptr) ? ps.beatLenOctave / ps.beatLen : 1.0;
        base.slot = c.slot;
        base.hitIndex = c.hitIndex;
        base.glitch = c.glitch;
        base.glitchDiv = c.glitchDiv;
        base.reversed = c.reversed;
        base.slotRms = ps.rms;

        const bool warp = std::abs (ratio - 1.0) > 1.0e-4 && c.glitch == 0 && ! c.reversed;
        if (! warp)
        {
            Piece p = base;
            p.outStart = c.outStart;
            p.outLen = c.lenOut;
            p.srcStart = c.reversed ? srcStart + (c.lenOut - 1) * ratio - (c.lenOut - 1) : std::round (srcStart);
            p.transientStart = c.glitch == 0 && ! c.reversed && nearTransient (tr, bufLen, p.srcStart, rate * 0.003);
            if (c.glitch == 0 && ! c.reversed)
                p.readLimit = nextTransient (tr, bufLen, p.srcStart + (double) p.outLen - guard) - guard;
            pieces.push_back (p);
            continue;
        }

        // cut points (source positions) with their kind
        const double srcSpan = (double) c.lenOut * ratio;
        const double gridSrc = stepLen * ratio;
        std::vector<std::pair<double, bool>> cuts;   // position, isTransient
        cuts.push_back ({ srcStart, nearTransient (tr, bufLen, srcStart, rate * 0.003) });
        for (double t = nextTransient (tr, bufLen, srcStart + guard); t < srcStart + srcSpan - guard; t = nextTransient (tr, bufLen, t + guard))
            cuts.push_back ({ t, true });
        for (int k = 1; srcStart + k * gridSrc < srcStart + srcSpan - guard; ++k)
        {
            const double gpos = srcStart + k * gridSrc;
            bool close = false;
            for (auto& cp : cuts)
                if (cp.second && std::abs (cp.first - gpos) < gridSrc * 0.5) { close = true; break; }
            if (! close) cuts.push_back ({ gpos, false });
        }
        std::sort (cuts.begin(), cuts.end());

        for (size_t k = 0; k < cuts.size(); ++k)
        {
            const juce::int64 oStart = c.outStart + std::llround ((cuts[k].first - srcStart) / ratio);
            const juce::int64 oEnd = k + 1 < cuts.size() ? c.outStart + std::llround ((cuts[k + 1].first - srcStart) / ratio)
                                                         : c.outStart + c.lenOut;
            if (oEnd - oStart < 16) continue;
            Piece p = base;
            p.outStart = oStart;
            p.outLen = oEnd - oStart;
            p.srcStart = std::round (cuts[k].first);
            p.hitStart = k == 0;
            p.hitEnd = k + 1 == cuts.size();
            p.transientStart = cuts[k].second;
            p.readLimit = nextTransient (tr, bufLen, p.srcStart + guard) - guard;
            pieces.push_back (p);
        }
    }

    // loop points for pieces that must last longer than their material before the next attack
    for (auto& p : pieces)
    {
        if (p.glitch || p.reversed || ! std::isfinite (p.readLimit))
            continue;
        const double need = p.srcStart + (double) (p.outLen + hitFade * 2);
        if (need <= p.readLimit)
            continue;
        const double avail = p.readLimit - p.srcStart;
        const int minL = (int) (rate * 0.012);
        const int maxL = (int) juce::jmin (avail * 0.75, rate * 0.12);
        p.loopFade = (int) juce::jmin (rate * 0.006, avail * 0.2);
        if (maxL <= minL || p.loopFade < 16)
        {
            p.loopLen = 0;   // no room: fade out just before the next attack (silence until the next piece)
            continue;
        }
        // loop length whose waveform best matches the material just before the limit
        const auto& a = *p.src;
        const juce::int64 la = a.getNumSamples();
        const int W = p.loopFade;
        const float* d = a.getReadPointer (0);
        double bestC = -2.0;
        int bestL = maxL;
        const int stepL = (maxL - minL) > 400 ? 4 : 1;
        auto corrL = [&] (int L)
        {
            double xy = 0, e1 = 0, e2 = 0;
            const juce::int64 b = (juce::int64) p.readLimit - W;
            for (int j = 0; j < W; j += 2)
            {
                const float u = d[wrap (b + j, la)], v = d[wrap (b + j - L, la)];
                xy += (double) u * v; e1 += (double) u * u; e2 += (double) v * v;
            }
            return (e1 < 1e-12 || e2 < 1e-12) ? 1.0 : xy / std::sqrt (e1 * e2);   // silence loops perfectly
        };
        for (int L = minL; L <= maxL; L += stepL)
        {
            const double cc = corrL (L);
            if (cc > bestC) { bestC = cc; bestL = L; }
        }
        for (int L = juce::jmax (minL, bestL - stepL); L <= juce::jmin (maxL, bestL + stepL); ++L)
        {
            const double cc = corrL (L);
            if (cc > bestC) { bestC = cc; bestL = L; }
        }
        p.loopLen = bestL;
    }

    // ---------------------------------------------------------------- 3. seamless joins
    for (auto& p : pieces)
    {
        p.fadeIn = p.glitch ? (int) (0.002 * rate) : hitFade;
        p.tail   = p.glitch ? (int) (0.002 * rate) : hitFade;
    }

    const int internalFade = juce::jmax (hitFade, (int) juce::jmin (0.008 * rate, stepLen / 3.0));
    const int shortRange = juce::jmin ((int) (rate * 0.002), (int) (stepLen / 8));
    const int longRange  = juce::jmin ((int) (rate * 0.0125), (int) (stepLen / 4));

    auto join = [&] (Piece& prev, Piece& cur)
    {
        const juce::int64 prevEnd = (prev.outStart + prev.outLen) % outLen;
        if (std::abs (cur.outStart % outLen - prevEnd) > 2)
            return;   // a gap (gate < 100%): normal fades
        if (cur.glitch || prev.glitch || cur.reversed || prev.reversed)
            return;

        const auto& a = *prev.src;
        const auto& b = *cur.src;
        const double cont = prev.srcStart + (double) prev.outLen;
        const bool internal = ! cur.hitStart;


        // exact continuation of the same audio: no crossfade at all (bit-perfect)
        if (&a == &b && std::abs (wrap ((juce::int64) std::llround (cont), a.getNumSamples())
                                  - wrap ((juce::int64) std::llround (cur.srcStart), b.getNumSamples())) <= 1)
        {
            // butt-join exactly: no overlap, no gap (output positions may differ by a rounding sample)
            const juce::int64 delta = cur.outStart % outLen - prevEnd;
            cur.outStart -= delta;
            cur.outLen += delta;
            cur.srcStart = std::round (cont);
            cur.fadeIn = 0;
            prev.tail = 0;
            return;
        }

        // an attack starts at full level exactly on its grid position: the previous piece fades out before it
        if (cur.transientStart)
        {
            const int pre = (int) juce::jmin<juce::int64> ((juce::int64) (rate * 0.002), prev.outLen / 3);
            prev.outLen -= pre;
            prev.tail = pre;
            prev.powerOut = false;
            cur.fadeIn = juce::jmax (8, (int) (rate * 0.0003));
            return;
        }

        const int W = internal ? internalFade : juce::jmax (hitFade, (int) (rate * 0.008));

        // how much is the previous slice still sounding? (sustained → search a whole bass period)
        double e = 0.0;
        for (int j = 0; j < W; j += 4)
        {
            const float v = a.getSample (0, (int) wrap ((juce::int64) cont + j, a.getNumSamples()));
            e += (double) v * v;
        }
        const double tailRms = std::sqrt (e / (W / 4.0));
        const bool sustained = tailRms > 0.25 * prev.slotRms;

        // keep attacks on time: a slice that starts on a transient may only move a little
        bool onTransient = false;
        if (cur.onsets != nullptr)
            for (int o : *cur.onsets)
                if (std::abs (o * cur.onsetScale - cur.srcStart) < rate * 0.003) { onTransient = true; break; }

        const int range = onTransient ? juce::jmin (shortRange, (int) (rate * 0.001)) : (sustained ? longRange : shortRange);
        const auto al = findAlignment (a, cont, b, cur.srcStart, W, range);
        if (al.best - al.atZero > (internal ? 0.02 : 0.15))
            cur.srcStart += al.offset;

        const int xf = internal ? internalFade : hitFade;
        cur.fadeIn = xf;
        prev.tail = xf;
        if (juce::jmax (al.best, al.atZero) < 0.5)
        {
            prev.powerOut = true;     // unrelated material: equal-power keeps the level
            cur.powerIn = true;
        }
    };

    for (size_t i = 1; i < pieces.size(); ++i)
        join (pieces[i - 1], pieces[i]);
    if (pieces.size() > 1)
        join (pieces.back(), pieces.front());   // loop wrap last, after its neighbours settled

    // ---------------------------------------------------------------- 4. render
    float* outL = result->audio.getWritePointer (0);
    float* outR = result->audio.getWritePointer (1);

    for (const auto& p : pieces)
    {
        if (onlySlot >= 0 && p.slot != onlySlot)
            continue;   // stems: the same loop, but only the slices of this sample
        const auto& src = *p.src;
        const juce::int64 srcLen = src.getNumSamples();
        const int srcCh = src.getNumChannels();
        const juce::int64 lenOut = p.outLen;
        const int fadeIn  = (int) juce::jmin<juce::int64> (p.fadeIn, lenOut / 2);
        const int fadeOut = p.tail;
        const juce::int64 grain = juce::jmax<juce::int64> (64, lenOut / juce::jmax (1, p.glitchDiv));
        const juce::int64 lastRep = juce::jmax<juce::int64> (0, lenOut / grain - 1);   // last grain absorbs the rest
        const int grainFade = juce::jmax (1, juce::jmin ((int) (rate * 0.0025), (int) grain / 4));

        auto sampleAt = [&] (double pos, int chan)
        {
            const double fl = std::floor (pos);
            const juce::int64 i0 = wrap ((juce::int64) fl, srcLen);
            const float frac = (float) (pos - fl);
            if (frac == 0.0f) return src.getSample (chan, (int) i0);   // whole-sample reads are exact copies
            const juce::int64 i1 = (i0 + 1) % srcLen;
            const float a = src.getSample (chan, (int) i0), b = src.getSample (chan, (int) i1);
            return a + (b - a) * frac;
        };

        const bool limited = ! p.reversed && p.glitch == 0 && std::isfinite (p.readLimit);
        const double limit = p.readLimit;
        const int stopFade = (int) juce::jmax (8.0, rate * 0.001);

        // returns the sample and a gain (0 once the material before the next attack is used up without a loop)
        auto readAt = [&] (double offset, int ch, float& gain)
        {
            const int chan = juce::jmin (ch, srcCh - 1);
            gain = 1.0f;
            if (p.reversed)
                return sampleAt (p.srcStart + (double) (lenOut - 1) - offset, chan);

            double pos = p.srcStart + offset;
            if (! limited || pos < limit - (p.loopLen > 0 ? p.loopFade : stopFade))
                return sampleAt (pos, chan);

            if (p.loopLen <= 0)
            {
                // stop just before the next attack
                gain = (float) juce::jlimit (0.0, 1.0, (limit - pos) / stopFade);
                return gain > 0.0f ? sampleAt (pos, chan) : 0.0f;
            }

            const double L = p.loopLen;
            while (pos >= limit) pos -= L;                      // loop the stretch before the attack
            const double xs = limit - p.loopFade;
            if (pos < xs) return sampleAt (pos, chan);
            const float w = fadeCurve ((pos - xs) / p.loopFade);
            return sampleAt (pos, chan) * (1.0f - w) + sampleAt (pos - L, chan) * w;
        };

        for (juce::int64 j = 0; j < lenOut + fadeOut; ++j)
        {
            float env = 1.0f;
            if (fadeIn > 0 && j < fadeIn)
            {
                const double x = (double) j / fadeIn;
                env = p.powerIn ? (float) std::sin (x * juce::MathConstants<double>::halfPi) : fadeCurve (x);
            }
            if (j >= lenOut)
            {
                const double x = (double) (j - lenOut) / juce::jmax (1, fadeOut);
                env *= p.powerOut ? (float) std::cos (juce::jlimit (0.0, 1.0, x) * juce::MathConstants<double>::halfPi)
                                  : 1.0f - fadeCurve (x);
            }

            double offset = (double) j;
            switch (p.glitch)
            {
                case 1:   // stutter
                case 2:   // rising stutter
                case 5:   // back-and-forth grains
                {
                    const juce::int64 rep = juce::jmin (juce::jmin (j, lenOut - 1) / grain, lastRep);
                    const juce::int64 inGrain = j - rep * grain;
                    if (p.glitch == 5)
                        offset = (rep % 2 == 0) ? (double) inGrain : (double) juce::jmax<juce::int64> (0, grain - 1 - inGrain);
                    else
                        offset = (double) inGrain * (p.glitch == 2 ? 1.0 + 0.09 * (double) rep * (0.5 + s.amount) : 1.0);
                    if (j < lenOut)
                    {
                        if (inGrain < grainFade && rep > 0) env *= (float) inGrain / grainFade;
                        else if (inGrain > grain - grainFade && rep < lastRep) env *= (float) (grain - inGrain) / grainFade;
                    }
                    break;
                }
                case 3:   // tape stop
                {
                    const double t = juce::jmin (1.0, (double) j / (double) lenOut);
                    offset = (double) lenOut * (t - 0.4 * (0.5 + s.amount) * t * t)
                           + (j > lenOut ? (double) (j - lenOut) * 0.2 : 0.0);
                    break;
                }
                case 4:   // chop (gated on/off)
                {
                    const juce::int64 cr = juce::jmin (j / grain, lastRep);
                    const juce::int64 inGrain = j - cr * grain;
                    const bool on = (cr % 2) == 0;
                    if (j >= lenOut)
                    {
                        if (lastRep % 2 != 0) env = 0.0f;   // slice ended in a closed gate
                    }
                    else
                    {
                        float g = on ? 1.0f : 0.0f;
                        if (cr > 0 && inGrain < grainFade) g = on ? (float) inGrain / grainFade : 1.0f - (float) inGrain / grainFade;
                        env *= g;
                    }
                    break;
                }
                default: break;
            }

            const juce::int64 o = wrap (p.outStart + j, outLen);
            float g0 = 1.0f, g1 = 1.0f;
            const float l = readAt (offset, 0, g0);
            const float r = readAt (offset, 1, g1);
            outL[o] += l * env * g0;
            outR[o] += r * env * g1;
        }
    }

    // ---------------------------------------------------------------- 5. segments (one per hit, for the UI)
    for (const auto& c : choicesOut)
    {
        Segment seg;
        seg.start = c.outStart;
        seg.length = c.lenOut;
        seg.srcStart = std::llround (c.srcBeats * beatLenOf (c.slot, c.octave));
        seg.slot = c.slot;
        seg.reversed = c.reversed;
        seg.octave = c.octave;
        seg.locked = c.locked;
        seg.glitch = c.glitch != 0;
        seg.hitIndex = c.hitIndex;
        result->segments.push_back (seg);
    }

    // one MIDI note per different slice (a repeated motif plays the same key again)
    {
        std::vector<std::array<juce::int64, 7>> keys;
        for (size_t i = 0; i < choicesOut.size(); ++i)
        {
            const auto& c = choicesOut[i];
            const auto& sg = result->segments[i];
            const std::array<juce::int64, 7> k { sg.slot, sg.srcStart, sg.length, sg.reversed ? 1 : 0, sg.octave ? 1 : 0, c.glitch, c.glitchDiv };
            int note = -1;
            for (size_t j = 0; j < keys.size(); ++j)
                if (keys[j] == k) { note = (int) j; break; }
            if (note < 0)
            {
                note = (int) keys.size();
                keys.push_back (k);
                if (note < RenderResult::maxSliceNotes)
                    result->noteSegment.push_back ((int) i);
            }
            result->segmentNote.push_back (note < RenderResult::maxSliceNotes ? note : -1);
        }
        result->numDifferentSlices = (int) keys.size();
    }

    if (onlySlot < 0)   // a stem stays dry: Lo-Fi and the limiter are not linear, so they belong on the mix
    {
        if (s.style == styleLoFi)
            applyLoFi (result->audio, rate, s.amount);
        applyLimiter (result->audio, rate, -0.3f);
    }
    return result;
}

//==============================================================================
/** Where does this audio have its weight inside a bar? One value per 1/16 step, 0..1.
    Used by FIT TO TRACK: the new loop leaves room where your own track is busy. */
std::array<float, 16> engine::gridProfile (const juce::AudioBuffer<float>& b, double rate, double bpm)
{
    std::array<float, 16> out {};
    const int n = b.getNumSamples();
    if (n < 64 || rate <= 0.0 || bpm < 20.0 || bpm > 400.0 || b.getNumChannels() == 0)
        return out;

    const double stepLen = rate * 60.0 / bpm / 4.0;         // one 1/16 in samples
    const int win = juce::jlimit (64, (int) stepLen, (int) (rate * 0.045));   // 45 ms from every step

    // where does the music start? A loop that was trimmed a little late would otherwise
    // put its downbeat on the wrong step.
    int offset = 0;
    {
        const int hop = juce::jmax (32, (int) (rate * 0.005));
        double loudest = 0.0;
        std::vector<double> frames;
        for (int i = 0; i + hop <= n; i += hop)
        {
            double e = 0.0;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
            {
                const float* d = b.getReadPointer (ch) + i;
                for (int k = 0; k < hop; ++k) e += (double) d[k] * d[k];
            }
            frames.push_back (e / (hop * b.getNumChannels()));
            loudest = juce::jmax (loudest, frames.back());
        }
        for (size_t f = 0; f < frames.size(); ++f)
            if (frames[f] > loudest * 0.08)
            {
                const int start = (int) f * hop;
                if (start < (int) (stepLen * 0.75))      // only a small trim at the head counts
                    offset = start;
                break;
            }
    }

    std::array<double, 16> sum {};
    std::array<int, 16> count {};
    const int steps = (int) std::floor ((n - offset) / stepLen) / 16 * 16;   // whole bars only
    if (steps < 16)
        return out;                                      // shorter than a bar: no opinion
    for (int st = 0; st < steps; ++st)
    {
        const int start = offset + (int) (st * stepLen);
        const int len = juce::jmin (win, n - start);
        if (len < 32)
            break;
        double e = 0.0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            const float* d = b.getReadPointer (ch) + start;
            for (int i = 0; i < len; ++i)
                e += (double) d[i] * d[i];
        }
        sum[(size_t) (st % 16)] += e / (len * b.getNumChannels());
        ++count[(size_t) (st % 16)];
    }
    double mx = 0.0, mn = 1.0e12;
    std::array<double, 16> rms {};
    for (int i = 0; i < 16; ++i)
    {
        rms[(size_t) i] = count[(size_t) i] > 0 ? std::sqrt (sum[(size_t) i] / count[(size_t) i]) : 0.0;
        mx = juce::jmax (mx, rms[(size_t) i]);
        mn = juce::jmin (mn, rms[(size_t) i]);
    }
    // an even sound (a pad, a drone) has no busy spots: then there is nothing to fit around
    if (mx < 1.0e-5 || mx - mn < mx * 0.35)
        return out;
    for (int i = 0; i < 16; ++i)
        out[(size_t) i] = (float) juce::jlimit (0.0, 1.0, (rms[(size_t) i] - mn) / (mx - mn));
    return out;
}

/** A quick opinion about a loop: is there enough going on, is it spread over the samples,
    is there no big hole in it and does it not sound like one long note? 0..1 */
double engine::scoreLoop (const RenderResult& r, double fillTargetScale)
{
    const int n = r.audio.getNumSamples();
    if (n < 64 || r.segments.empty())
        return 0.0;

    // 1. how much of the loop actually sounds (no long silences)
    const int win = juce::jmax (64, (int) (r.rate * 0.02));
    int windows = 0, loud = 0;
    double sumSq = 0.0, peak = 0.0;
    for (int i = 0; i + win <= n; i += win)
    {
        double e = 0.0;
        for (int k = 0; k < win; ++k)
            for (int ch = 0; ch < r.audio.getNumChannels(); ++ch)
            {
                const double v = r.audio.getSample (ch, i + k);
                e += v * v / r.audio.getNumChannels();
                peak = juce::jmax (peak, std::abs (v));
            }
        const double rms = std::sqrt (e / win);
        sumSq += e;
        ++windows;
        loud += rms > 0.01 ? 1 : 0;   // > -40 dB
    }
    if (windows == 0 || peak < 1.0e-4)
        return 0.0;
    const double fill = (double) loud / windows;                       // 0..1
    const double rmsAll = std::sqrt (sumSq / juce::jmax (1, windows * win));
    const double crest = peak / juce::jmax (1.0e-6, rmsAll);           // punch

    // 2. how well the slices are spread over the samples that are on
    std::array<int, kNumSlots> perSlot {};
    for (const auto& sg : r.segments)
        if (juce::isPositiveAndBelow (sg.slot, kNumSlots))
            ++perSlot[(size_t) sg.slot];
    int used = 0;
    double entropy = 0.0;
    for (auto c : perSlot)
        if (c > 0)
        {
            ++used;
            const double p = (double) c / (double) r.segments.size();
            entropy -= p * std::log (p);
        }
    const double spread = used > 1 ? entropy / std::log ((double) used) : 0.0;   // 0..1

    // 3. variety: different sources and not everything reversed / octaved
    int different = 0, glitch = 0;
    for (const auto& sg : r.segments)
        glitch += sg.glitch ? 1 : 0;
    different = r.numDifferentSlices;
    const double variety = juce::jlimit (0.0, 1.0, different / juce::jmax (1.0, r.segments.size() * 0.6));
    const double glitchPart = (double) glitch / (double) r.segments.size();

    // put it together: a full but punchy loop, nicely spread, varied, not one big stutter
    const double scale = juce::jlimit (0.3, 1.0, fillTargetScale);
    const double fillScore   = juce::jlimit (0.0, 1.0, (fill - 0.35 * scale) / (0.5 * scale));
    const double crestScore  = juce::jlimit (0.0, 1.0, 1.0 - std::abs (crest - 5.0) / 6.0);
    const double glitchScore = juce::jlimit (0.0, 1.0, 1.0 - std::abs (glitchPart - 0.15) / 0.5);
    return juce::jlimit (0.0, 1.0, 0.34 * fillScore + 0.2 * crestScore + 0.24 * spread + 0.14 * variety + 0.08 * glitchScore);
}

//==============================================================================
void engine::applyLimiter (juce::AudioBuffer<float>& b, double rate, float ceilingDb)
{
    const int n = b.getNumSamples();
    if (n < 8) return;
    const float ceiling = juce::Decibels::decibelsToGain (ceilingDb);

    float peak = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        peak = juce::jmax (peak, b.getMagnitude (ch, 0, n));
    if (peak <= ceiling)
        return;   // nothing to do: untouched, bit-exact

    // required gain per sample
    std::vector<float> g ((size_t) n, 1.0f);
    for (int i = 0; i < n; ++i)
    {
        float m = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            m = juce::jmax (m, std::abs (b.getSample (ch, i)));
        if (m > ceiling) g[(size_t) i] = ceiling / m;
    }

    // look-ahead: minimum over a window around every sample (circular, so the loop point is seamless)
    const int look = juce::jmax (1, (int) (rate * 0.0015));
    std::vector<float> gmin ((size_t) n, 1.0f);
    {
        std::deque<int> dq;   // monotonic queue over the circular window [i-look, i+look]
        const int span = 2 * look + 1;
        for (int k = -span; k < n + look; ++k)
        {
            const int idx = k + look;                              // element entering
            const float v = g[(size_t) (((idx % n) + n) % n)];
            while (! dq.empty() && g[(size_t) (((dq.back() % n) + n) % n)] >= v) dq.pop_back();
            dq.push_back (idx);
            while (dq.front() < k - look) dq.pop_front();
            if (k >= 0 && k < n)
                gmin[(size_t) k] = g[(size_t) (((dq.front() % n) + n) % n)];
        }
    }

    // smooth: centred moving average over half the look-ahead window. Every value averaged is a window-minimum
    // that already includes this sample, so the result never exceeds the required gain (no overs, no clicks).
    std::vector<float> sm ((size_t) n);
    {
        const int half = juce::jmax (1, look / 2);
        const int w = 2 * half + 1;
        double acc = 0.0;
        for (int i = -half; i <= half; ++i) acc += gmin[(size_t) ((i + n) % n)];
        for (int i = 0; i < n; ++i)
        {
            sm[(size_t) i] = (float) (acc / w);
            acc += gmin[(size_t) ((i + half + 1) % n)] - gmin[(size_t) ((i - half + n) % n)];
        }
    }
    const float rel = 1.0f - std::exp (-1.0f / (float) (0.08 * rate));
    float env = 1.0f;
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < n; ++i)
        {
            const float target = sm[(size_t) i];
            env = target < env ? target : env + (target - env) * rel;
            if (pass == 1)
                for (int ch = 0; ch < b.getNumChannels(); ++ch)
                    b.setSample (ch, i, juce::jlimit (-ceiling, ceiling, b.getSample (ch, i) * env));
        }
}

//==============================================================================
void engine::applyLoFi (juce::AudioBuffer<float>& b, double rate, float amount)
{
    const int n = b.getNumSamples();
    if (n < 16) return;
    const float a = juce::jlimit (0.0f, 1.0f, amount);
    float rmsIn = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        rmsIn = juce::jmax (rmsIn, b.getRMSLevel (ch, 0, n));

    const double targetRate = juce::jmap ((double) a, 16000.0, 5500.0);
    const double holdStep = targetRate / rate;                 // < 1: sample & hold
    const float levels = std::pow (2.0f, juce::jmap (a, 12.0f, 5.5f) - 1.0f);
    const float drive = juce::jmap (a, 1.3f, 3.5f);
    const float driveNorm = 1.0f / std::tanh (drive);
    const float makeUp = 1.0f / std::sqrt (drive * 0.8f);

    // biquad low-pass (RBJ), run twice around the loop so the loop point is seamless
    const double fc = juce::jmap ((double) a, 7500.0, 2400.0);
    const double w0 = juce::MathConstants<double>::twoPi * fc / rate;
    const double alpha = std::sin (w0) / (2.0 * 0.7071);
    const double cw = std::cos (w0);
    const double a0 = 1.0 + alpha;
    const double b0 = (1.0 - cw) / 2.0 / a0, b1 = (1.0 - cw) / a0, b2 = b0;
    const double a1 = -2.0 * cw / a0, a2 = (1.0 - alpha) / a0;

    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        float* d = b.getWritePointer (ch);
        std::vector<float> crushed ((size_t) n);
        double phase = 0.0;
        float held = 0.0f;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < n; ++i)
            {
                phase += holdStep;
                if (phase >= 1.0 || (pass == 0 && i == 0))
                {
                    phase -= std::floor (phase);
                    const float sat = std::tanh (drive * d[i]) * driveNorm;
                    held = std::round (sat * levels) / levels;
                }
                if (pass == 1) crushed[(size_t) i] = held;
            }

        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < n; ++i)
            {
                const double x = crushed[(size_t) i];
                const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = x; y2 = y1; y1 = y;
                if (pass == 1) d[i] = (float) y * makeUp;
            }
    }

    // same loudness as Clean, so switching style never jumps in level
    float rmsOut = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        rmsOut = juce::jmax (rmsOut, b.getRMSLevel (ch, 0, n));
    if (rmsOut > 1.0e-6f && rmsIn > 1.0e-6f)
        b.applyGain (juce::jlimit (0.25f, 4.0f, rmsIn / rmsOut));
}

//==============================================================================
bool engine::writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double rate, float gain)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (static_cast<juce::FileOutputStream*> (stream.get())->failedToOpen())
        return false;

    juce::AudioBuffer<float> copy (buffer);
    copy.applyGain (gain);
    float peak = 0.0f;
    for (int ch = 0; ch < copy.getNumChannels(); ++ch)
        peak = juce::jmax (peak, copy.getMagnitude (ch, 0, copy.getNumSamples()));
    const bool needsFloat = peak > 0.9999f;   // Volume above 0 dB: 32-bit float keeps it exactly as heard, never clipped

    juce::WavAudioFormat wav;
    auto options = juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (copy.getNumChannels());
    options = needsFloat ? options.withBitsPerSample (32).withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint)
                         : options.withBitsPerSample (24);
    auto writer = wav.createWriterFor (stream, options);
    if (writer == nullptr)
        return false;
    return writer->writeFromAudioSampleBuffer (copy, 0, copy.getNumSamples());
}

} // namespace slicetribe
