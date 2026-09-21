#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
#include <array>

class SpaceReverb
{
public:
    SpaceReverb()
    {
        roomSize.setCurrentAndTargetValue(1.0f);
        decayTime.setCurrentAndTargetValue(0.85f);
        damping.setCurrentAndTargetValue(0.4f);
        stereoWidth.setCurrentAndTargetValue(1.0f);
        dryWet.setCurrentAndTargetValue(0.45f);
    }

    void prepare(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double smoothSec = 0.02;

        roomSize.reset(currentSampleRate, smoothSec);
        decayTime.reset(currentSampleRate, smoothSec);
        damping.reset(currentSampleRate, smoothSec);
        stereoWidth.reset(currentSampleRate, smoothSec);
        dryWet.reset(currentSampleRate, smoothSec);

        const double srScale = currentSampleRate / 44100.0;

        // Base prime delay lengths scaled to sample rate
        baseLengths[0] = static_cast<int>(1321 * srScale);
        baseLengths[1] = static_cast<int>(1627 * srScale);
        baseLengths[2] = static_cast<int>(1907 * srScale);
        baseLengths[3] = static_cast<int>(2311 * srScale);

        // Pre-diffuser lengths
        diffuserLengths[0] = static_cast<int>(223 * srScale);
        diffuserLengths[1] = static_cast<int>(347 * srScale);

        for (int i = 0; i < 4; ++i)
        {
            const int maxLen = static_cast<int>(baseLengths[i] * 3.0) + 256;
            fdnBuffers[i].assign(maxLen, 0.0f);
            fdnWriteIndices[i] = 0;
            filterStates[i] = 0.0f;
        }

        for (int i = 0; i < 2; ++i)
        {
            diffuserBuffers[i].assign(diffuserLengths[i] + 32, 0.0f);
            diffuserWriteIndices[i] = 0;
        }

        reset();
    }

    void reset()
    {
        for (int i = 0; i < 4; ++i)
        {
            std::fill(fdnBuffers[i].begin(), fdnBuffers[i].end(), 0.0f);
            fdnWriteIndices[i] = 0;
            filterStates[i] = 0.0f;
        }

        for (int i = 0; i < 2; ++i)
        {
            std::fill(diffuserBuffers[i].begin(), diffuserBuffers[i].end(), 0.0f);
            diffuserWriteIndices[i] = 0;
        }

        lfoPhase1 = 0.0;
        lfoPhase2 = 0.0;
    }

    void setRoomSize(float size)       { roomSize.setTargetValue(juce::jlimit(0.3f, 2.5f, size)); }
    void setDecayTime(float decay)     { decayTime.setTargetValue(juce::jlimit(0.1f, 0.992f, decay)); }
    void setDamping(float damp)        { damping.setTargetValue(juce::jlimit(0.0f, 0.95f, damp)); }
    void setStereoWidth(float width)   { stereoWidth.setTargetValue(juce::jlimit(0.0f, 1.0f, width)); }
    void setDryWet(float mix)          { dryWet.setTargetValue(juce::jlimit(0.0f, 1.0f, mix)); }

    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (buffer.getNumChannels() < 2)
            return;

        auto* leftChannel  = buffer.getWritePointer(0, startSample);
        auto* rightChannel = buffer.getWritePointer(1, startSample);

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = leftChannel[i];
            const float inR = rightChannel[i];

            const float curSize  = roomSize.getNextValue();
            const float curDecay = decayTime.getNextValue();
            const float curDamp  = damping.getNextValue();
            const float curWidth = stereoWidth.getNextValue();
            const float curMix   = dryWet.getNextValue();

            // Advance gentle modulation LFOs
            lfoPhase1 += (juce::MathConstants<double>::twoPi * 0.45) / currentSampleRate;
            if (lfoPhase1 >= juce::MathConstants<double>::twoPi)
                lfoPhase1 -= juce::MathConstants<double>::twoPi;

            lfoPhase2 += (juce::MathConstants<double>::twoPi * 0.65) / currentSampleRate;
            if (lfoPhase2 >= juce::MathConstants<double>::twoPi)
                lfoPhase2 -= juce::MathConstants<double>::twoPi;

            const float mod1 = static_cast<float>(std::sin(lfoPhase1)) * 6.0f;
            const float mod2 = static_cast<float>(std::cos(lfoPhase2)) * 6.0f;

            // 1. Input transient pre-diffusion
            float monoIn = (inL + inR) * 0.5f;
            monoIn = processAllpass(0, monoIn, 0.6f);
            monoIn = processAllpass(1, monoIn, 0.6f);

            // 2. Read from 4 FDN delay lines with modulation
            std::array<float, 4> delayOuts;
            for (int ch = 0; ch < 4; ++ch)
            {
                float modOffset = 0.0f;
                if (ch == 0) modOffset = mod1;
                else if (ch == 2) modOffset = mod2;

                const float delaySamples = juce::jmax(8.0f, (static_cast<float>(baseLengths[ch]) * curSize) + modOffset);
                delayOuts[ch] = readLinear(fdnBuffers[ch], fdnWriteIndices[ch], delaySamples);

                // Damping one-pole lowpass
                filterStates[ch] = (delayOuts[ch] * (1.0f - curDamp)) + (filterStates[ch] * curDamp);
            }

            // 3. Householder 4x4 Orthogonal Feedback Matrix (lossless reflection)
            // y_i = x_i - 0.5 * sum(x)
            const float sumX = filterStates[0] + filterStates[1] + filterStates[2] + filterStates[3];
            const float halfSum = sumX * 0.5f;

            for (int ch = 0; ch < 4; ++ch)
            {
                const float reflected = filterStates[ch] - halfSum;
                const float toWrite = monoIn + (reflected * curDecay);

                // Denormal and clip protection
                fdnBuffers[ch][fdnWriteIndices[ch]] = std::tanh(toWrite);
                fdnWriteIndices[ch] = (fdnWriteIndices[ch] + 1) % static_cast<int>(fdnBuffers[ch].size());
            }

            // 4. Output stereo matrix
            const float wetL = (delayOuts[0] + delayOuts[2]) * 0.5f;
            const float wetR = (delayOuts[1] + delayOuts[3]) * 0.5f;

            const float mid  = (wetL + wetR) * 0.5f;
            const float side = (wetL - wetR) * 0.5f * curWidth;

            const float outWetL = mid + side;
            const float outWetR = mid - side;

            // Crossfade Dry & Wet
            leftChannel[i]  = (inL * (1.0f - curMix)) + (outWetL * curMix);
            rightChannel[i] = (inR * (1.0f - curMix)) + (outWetR * curMix);
        }
    }

private:
    double currentSampleRate { 44100.0 };

    juce::SmoothedValue<float> roomSize;
    juce::SmoothedValue<float> decayTime;
    juce::SmoothedValue<float> damping;
    juce::SmoothedValue<float> stereoWidth;
    juce::SmoothedValue<float> dryWet;

    std::array<int, 4> baseLengths {};
    std::array<std::vector<float>, 4> fdnBuffers;
    std::array<int, 4> fdnWriteIndices {};
    std::array<float, 4> filterStates {};

    std::array<int, 2> diffuserLengths {};
    std::array<std::vector<float>, 2> diffuserBuffers;
    std::array<int, 2> diffuserWriteIndices {};

    double lfoPhase1 { 0.0 };
    double lfoPhase2 { 0.0 };

    float processAllpass(int index, float input, float feedbackGain)
    {
        auto& buf = diffuserBuffers[index];
        auto& wIdx = diffuserWriteIndices[index];
        const int len = diffuserLengths[index];

        const int readIdx = (wIdx - len + static_cast<int>(buf.size())) % static_cast<int>(buf.size());
        const float bufOut = buf[readIdx];

        const float newWrite = input + (bufOut * feedbackGain);
        buf[wIdx] = newWrite;

        wIdx = (wIdx + 1) % static_cast<int>(buf.size());

        return bufOut - (newWrite * feedbackGain);
    }

    static inline float readLinear(const std::vector<float>& buffer, int writeIdx, float delaySamples)
    {
        const int bufSize = static_cast<int>(buffer.size());
        const int intPart = static_cast<int>(std::floor(delaySamples));
        const float frac  = delaySamples - static_cast<float>(intPart);

        const int idx0 = (writeIdx - intPart + bufSize) % bufSize;
        const int idx1 = (idx0 - 1 + bufSize) % bufSize;

        return buffer[idx0] + frac * (buffer[idx1] - buffer[idx0]);
    }
};
