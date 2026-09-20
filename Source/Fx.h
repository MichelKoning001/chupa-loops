#pragma once

#include <JuceHeader.h>

namespace slicetribe
{

/** Finishing effects on the new loop: low-pass with a per-slice envelope, low cut, sidechain pump,
    drive and stereo width. Real-time safe (no allocation after prepare). */
class FxChain
{
public:
    struct Params
    {
        float cutoff = 1.0f;     // 0..1 (1 = open)
        float reso   = 0.0f;     // 0..1
        float env    = 0.0f;     // 0..1: how far the low-pass opens on every slice
        float decayMs = 200.0f;  // envelope decay
        float pump   = 0.0f;     // 0..1 sidechain depth (on every beat)
        float drive  = 0.0f;     // 0..1
        float lowCut = 0.0f;     // 0..1 (0 = off)
        float width  = 1.0f;     // 0..2 (1 = unchanged)

        bool isNeutral() const
        {
            return cutoff >= 0.999f && env <= 0.001f && pump <= 0.001f && drive <= 0.001f && lowCut <= 0.001f
                && std::abs (width - 1.0f) < 0.001f;
        }
    };

    void prepare (double sampleRate);
    void reset();

    /** True while the chain does nothing (neutral settings and every smoother has arrived): skip it. */
    bool isBypassed (const Params& p) const noexcept { return ! active && p.isNeutral(); }

    /** Call at the start of every slice (the envelope restarts). */
    void triggerEnvelope() noexcept { envValue = 1.0f; }

    /** Processes `n` samples in place. beatPos = position in beats of the first sample, beatsPerSample = tempo. */
    void process (float* left, float* right, int n, const Params&, double beatPos, double beatsPerSample,
                  const int* sliceStarts = nullptr, int numSliceStarts = 0) noexcept;   // offsets (sorted) where a slice starts

private:
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void reset() { ic1 = ic2 = 0.0f; }
        // TPT state-variable filter (Cytomic); returns low-pass and high-pass
        inline void tick (float v0, float g, float k, float& lp, float& hp) noexcept
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1, a3 = g * a2;
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2;
            hp = v0 - k * v1 - v2;
        }
    };

    double rate = 44100.0;
    Svf lpL, lpR, hpL, hpR;
    float envValue = 0.0f, pumpGain = 1.0f;
    float smoothCut = 1.0f, smoothLow = 0.0f, smoothDrive = 0.0f, smoothWidth = 1.0f, smoothReso = 0.0f, smoothEnv = 0.0f;
    bool active = false;   // false = bypassed (the filters are primed from the input when it starts again)
};

} // namespace slicetribe
