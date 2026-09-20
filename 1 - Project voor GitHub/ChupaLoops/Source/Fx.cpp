#include "Fx.h"

namespace slicetribe
{

void FxChain::prepare (double sampleRate)
{
    rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void FxChain::reset()
{
    lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset();
    envValue = 0.0f;
    pumpGain = 1.0f;
    smoothCut = 1.0f; smoothLow = 0.0f; smoothDrive = 0.0f; smoothWidth = 1.0f; smoothReso = 0.0f; smoothEnv = 0.0f;
    active = false;
}

void FxChain::process (float* L, float* R, int n, const Params& p, double beatPos, double beatsPerSample,
                       const int* sliceStarts, int numSliceStarts) noexcept
{
    if (n <= 0)
        return;
    if (! active)
    {
        if (p.isNeutral())
            return;
        // coming out of bypass: the filters start "settled" on the input, so switching on never thumps
        active = true;
        const float l0 = L[0], r0 = R != nullptr ? R[0] : L[0];
        lpL.ic1 = hpL.ic1 = 0.0f; lpL.ic2 = hpL.ic2 = l0;
        lpR.ic1 = hpR.ic1 = 0.0f; lpR.ic2 = hpR.ic2 = r0;
    }

    const float pi = juce::MathConstants<float>::pi;
    const float nyq = (float) rate * 0.45f;
    const float envDecay = std::exp (-1.0f / (juce::jmax (5.0f, p.decayMs) * 0.001f * (float) rate));
    const float smooth = 1.0f - std::exp (-1.0f / (0.004f * (float) rate));      // 4 ms parameter smoothing
    const float pumpSmooth = 1.0f - std::exp (-1.0f / (0.0015f * (float) rate)); // 1.5 ms: no click on the duck
    int nextSlice = 0;

    float g = 0.0f, k = 2.0f, gh = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        while (nextSlice < numSliceStarts && sliceStarts[nextSlice] <= i)
        {
            if (sliceStarts[nextSlice] == i) envValue = 1.0f;
            ++nextSlice;
        }

        smoothCut   += (p.cutoff - smoothCut) * smooth;
        smoothReso  += (p.reso - smoothReso) * smooth;
        smoothLow   += (p.lowCut - smoothLow) * smooth;
        smoothDrive += (p.drive - smoothDrive) * smooth;
        smoothWidth += (p.width - smoothWidth) * smooth;
        smoothEnv   += (p.env - smoothEnv) * smooth;

        float l = L[i], r = R != nullptr ? R[i] : L[i];

        // ---- low-pass with envelope (coefficients every 8 samples). The filters always run while the
        //      chain is active and are only switched into the signal when needed, so they never click in.
        const bool lpOn = smoothCut < 0.999f || smoothEnv > 0.001f;
        if ((i & 7) == 0)
        {
            // cutoff: 60 Hz .. 20 kHz (log); the envelope opens it by up to 6 octaves
            const float baseHz = 60.0f * std::pow (2.0f, smoothCut * 8.4f);
            const float hz = juce::jmin (nyq, baseHz * std::pow (2.0f, smoothEnv * envValue * 6.0f));
            g = std::tan (pi * hz / (float) rate);
            k = 2.0f - 1.9f * smoothReso;   // resonance
            const float lowHz = 20.0f * std::pow (2.0f, smoothLow * 8.0f);   // 20 Hz .. 5 kHz
            gh = std::tan (pi * juce::jmin (nyq, lowHz) / (float) rate);
        }
        {
            float lp1, hp1, lp2, hp2;
            lpL.tick (l, g, k, lp1, hp1);
            lpR.tick (r, g, k, lp2, hp2);
            if (lpOn) { l = lp1; r = lp2; }
        }
        envValue *= envDecay;

        // ---- low cut
        {
            float lp1, hp1, lp2, hp2;
            hpL.tick (l, gh, 1.4142f, lp1, hp1);
            hpR.tick (r, gh, 1.4142f, lp2, hp2);
            if (smoothLow > 0.001f) { l = hp1; r = hp2; }
        }

        // ---- drive
        if (smoothDrive > 0.001f)
        {
            const float dg = 1.0f + smoothDrive * 9.0f;
            const float norm = 1.0f / std::tanh (dg);
            l = std::tanh (l * dg) * norm;
            r = std::tanh (r * dg) * norm;
        }

        // ---- sidechain pump: ducks on every beat, recovers over the beat
        float target = 1.0f;
        if (p.pump > 0.001f)
        {
            const double bp = beatPos + (double) i * beatsPerSample;
            const float phase = (float) (bp - std::floor (bp));
            const float x = juce::jmin (1.0f, phase / 0.8f);
            target = 1.0f - p.pump * (1.0f - x) * (1.0f - x);
        }
        pumpGain += (target - pumpGain) * pumpSmooth;
        l *= pumpGain;
        r *= pumpGain;

        // ---- width
        if (std::abs (smoothWidth - 1.0f) > 0.001f && R != nullptr)
        {
            const float m = 0.5f * (l + r), sd = 0.5f * (l - r) * smoothWidth;
            l = m + sd;
            r = m - sd;
        }

        L[i] = l;
        if (R != nullptr) R[i] = r;
    }

    // keep the filter state finite whatever happens
    if (! std::isfinite (lpL.ic1) || ! std::isfinite (lpR.ic1) || ! std::isfinite (hpL.ic1) || ! std::isfinite (hpR.ic1)
        || ! std::isfinite (lpL.ic2) || ! std::isfinite (lpR.ic2) || ! std::isfinite (hpL.ic2) || ! std::isfinite (hpR.ic2))
        reset();

    // back to bypass once everything has settled on neutral
    if (p.isNeutral() && smoothCut > 0.9995f && smoothEnv < 1.0e-4f && smoothLow < 1.0e-4f && smoothDrive < 1.0e-4f
        && std::abs (smoothWidth - 1.0f) < 1.0e-4f && pumpGain > 0.9999f)
    {
        active = false;
        smoothCut = 1.0f; smoothEnv = 0.0f; smoothLow = 0.0f; smoothDrive = 0.0f; smoothWidth = 1.0f; pumpGain = 1.0f;
    }
}

} // namespace slicetribe
