#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>

class FilterRingMod
{
public:
    FilterRingMod()
    {
        cutoffA.setCurrentAndTargetValue(2500.0f);
        cutoffB.setCurrentAndTargetValue(2500.0f);
        resonanceA.setCurrentAndTargetValue(0.4f);
        resonanceB.setCurrentAndTargetValue(0.4f);
        morphA.setCurrentAndTargetValue(0.0f); // 0.0 = Lowpass, 1.0 = Highpass
        morphB.setCurrentAndTargetValue(0.0f);
        ringModMix.setCurrentAndTargetValue(0.0f); // 0.0 = Sum, 1.0 = RingMod
        distortionDrive.setCurrentAndTargetValue(0.2f);
        stereoSpread.setCurrentAndTargetValue(0.3f);
        balanceAB.setCurrentAndTargetValue(0.5f);
    }

    void prepare(double sampleRate, int /*samplesPerBlock*/)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double smoothSec = 0.02; // 20ms smoothing

        cutoffA.reset(currentSampleRate, smoothSec);
        cutoffB.reset(currentSampleRate, smoothSec);
        resonanceA.reset(currentSampleRate, smoothSec);
        resonanceB.reset(currentSampleRate, smoothSec);
        morphA.reset(currentSampleRate, smoothSec);
        morphB.reset(currentSampleRate, smoothSec);
        ringModMix.reset(currentSampleRate, smoothSec);
        distortionDrive.reset(currentSampleRate, smoothSec);
        stereoSpread.reset(currentSampleRate, smoothSec);
        balanceAB.reset(currentSampleRate, smoothSec);

        reset();
    }

    void reset()
    {
        filterStateA.reset();
        filterStateB.reset();
        dcBlockerL.reset();
        dcBlockerR.reset();
    }

    // Filter A Setters
    void setCutoffA(float hz)        { cutoffA.setTargetValue(juce::jlimit(20.0f, 20000.0f, hz)); }
    void setResonanceA(float res)    { resonanceA.setTargetValue(juce::jlimit(0.0f, 0.99f, res)); }
    void setMorphA(float morph)      { morphA.setTargetValue(juce::jlimit(0.0f, 1.0f, morph)); }

    // Filter B Setters
    void setCutoffB(float hz)        { cutoffB.setTargetValue(juce::jlimit(20.0f, 20000.0f, hz)); }
    void setResonanceB(float res)    { resonanceB.setTargetValue(juce::jlimit(0.0f, 0.99f, res)); }
    void setMorphB(float morph)      { morphB.setTargetValue(juce::jlimit(0.0f, 1.0f, morph)); }

    // Routing & Distortion Setters
    void setRingModMix(float mix)        { ringModMix.setTargetValue(juce::jlimit(0.0f, 1.0f, mix)); }
    void setDistortionDrive(float drive) { distortionDrive.setTargetValue(juce::jlimit(0.0f, 1.0f, drive)); }
    void setStereoSpread(float sp)       { stereoSpread.setTargetValue(juce::jlimit(0.0f, 1.0f, sp)); }
    void setBalanceAB(float bal)         { balanceAB.setTargetValue(juce::jlimit(0.0f, 1.0f, bal)); }

    void processBlock(const float* inA, const float* inB, float* outL, float* outR, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float curCutoffA = cutoffA.getNextValue();
            const float curResA    = resonanceA.getNextValue();
            const float curMorphA  = morphA.getNextValue();

            const float curCutoffB = cutoffB.getNextValue();
            const float curResB    = resonanceB.getNextValue();
            const float curMorphB  = morphB.getNextValue();

            const float curRMMix   = ringModMix.getNextValue();
            const float curDrive   = distortionDrive.getNextValue();
            const float curSpread  = stereoSpread.getNextValue();
            const float curBal     = balanceAB.getNextValue();

            // 1. Process Bank A through Hybrid Non-Linear SVF
            float lpA = 0.0f, hpA = 0.0f, bpA = 0.0f;
            filterStateA.process(inA[i], curCutoffA, curResA, currentSampleRate, lpA, hpA, bpA);
            const float sigA = (1.0f - curMorphA) * lpA + curMorphA * hpA;

            // 2. Process Bank B through Hybrid Non-Linear SVF
            float lpB = 0.0f, hpB = 0.0f, bpB = 0.0f;
            filterStateB.process(inB[i], curCutoffB, curResB, currentSampleRate, lpB, hpB, bpB);
            const float sigB = (1.0f - curMorphB) * lpB + curMorphB * hpB;

            // 3. Balance Crossfade
            const float gainA = std::cos(curBal * juce::MathConstants<float>::halfPi);
            const float gainB = std::sin(curBal * juce::MathConstants<float>::halfPi);

            const float balA = sigA * gainA;
            const float balB = sigB * gainB;

            // 4. Balanced Linear Sum vs Ring Modulation
            const float linearSum = (balA + balB) * 0.707f;
            const float ringMod   = (balA * balB) * 2.2f;

            const float combined = (1.0f - curRMMix) * linearSum + (curRMMix * ringMod);

            // 5. Metaphysical Analog Overdrive Circuit
            const float driveMult = 1.0f + (curDrive * 6.0f);
            const float preDrive = combined * driveMult;
            const float satSample = std::tanh(preDrive + 0.08f * preDrive * preDrive);
            const float compensated = satSample * (1.0f / std::sqrt(1.0f + (curDrive * 3.0f)));

            // 6. Stereo Width Positioning
            const float panOffset = (balA - balB) * curSpread * 0.35f;
            const float rawL = compensated - panOffset;
            const float rawR = compensated + panOffset;

            // 7. Strip out all DC bias generated by Ring Mod & Asymmetric Saturation
            outL[i] = dcBlockerL.process(rawL);
            outR[i] = dcBlockerR.process(rawR);
        }
    }

private:
    struct NonLinearSvf
    {
        float s1 { 0.0f };
        float s2 { 0.0f };

        void reset()
        {
            s1 = 0.0f;
            s2 = 0.0f;
        }

        inline void process(float input, float cutoffHz, float resonance, double sampleRate,
                            float& outLP, float& outHP, float& outBP)
        {
            const float g = std::tan(juce::MathConstants<float>::pi * (cutoffHz / static_cast<float>(sampleRate)));
            const float safeG = juce::jlimit(0.0001f, 15.0f, g);

            const float safeRes = juce::jlimit(0.0f, 0.99f, resonance);
            const float k = 2.0f * (1.0f - std::pow(safeRes, 0.35f));

            const float a1 = 1.0f / (1.0f + safeG * (safeG + k));
            const float a2 = safeG * a1;
            const float a3 = safeG * a2;

            const float v3 = input - s2;
            float v1 = a1 * s1 + a2 * v3;
            const float v2 = s2 + a2 * s1 + a3 * v3;

            v1 = std::tanh(v1); // Internal saturating limiter for singing analog resonance

            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;

            outLP = v2;
            outBP = v1;
            outHP = input - (k * v1) - v2;
        }
    };

    // 1-Pole 10 Hz High-Pass DC Filter
    struct DcBlocker
    {
        float x1 { 0.0f };
        float y1 { 0.0f };

        void reset()
        {
            x1 = 0.0f;
            y1 = 0.0f;
        }

        inline float process(float x)
        {
            const float y = x - x1 + (0.9985f * y1);
            x1 = x;
            y1 = y;
            return y;
        }
    };

    NonLinearSvf filterStateA;
    NonLinearSvf filterStateB;

    DcBlocker dcBlockerL;
    DcBlocker dcBlockerR;

    juce::SmoothedValue<float> cutoffA;
    juce::SmoothedValue<float> cutoffB;
    juce::SmoothedValue<float> resonanceA;
    juce::SmoothedValue<float> resonanceB;
    juce::SmoothedValue<float> morphA;
    juce::SmoothedValue<float> morphB;
    juce::SmoothedValue<float> ringModMix;
    juce::SmoothedValue<float> distortionDrive;
    juce::SmoothedValue<float> stereoSpread;
    juce::SmoothedValue<float> balanceAB;

    double currentSampleRate { 44100.0 };
};