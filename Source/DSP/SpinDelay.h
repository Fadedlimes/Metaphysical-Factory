#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>

class SpinDelay
{
public:
    static constexpr int MaxDelaySamples = 262144; // ~5.9s at 44.1kHz, ~2.7s at 96kHz (power of 2)
    static constexpr int DelayMask = MaxDelaySamples - 1;

    SpinDelay()
    {
        delayBufferL.assign(MaxDelaySamples, 0.0f);
        delayBufferR.assign(MaxDelaySamples, 0.0f);

        delayTimeSec.setCurrentAndTargetValue(0.35f);
        spinRateHz.setCurrentAndTargetValue(0.4f);
        spinDepthMs.setCurrentAndTargetValue(8.0f);
        feedback.setCurrentAndTargetValue(0.55f);
        crossFeed.setCurrentAndTargetValue(0.6f);
        damping.setCurrentAndTargetValue(0.35f);
        dryWet.setCurrentAndTargetValue(0.4f);
    }

    void prepare(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double smoothSec = 0.02;

        delayTimeSec.reset(currentSampleRate, smoothSec);
        spinRateHz.reset(currentSampleRate, smoothSec);
        spinDepthMs.reset(currentSampleRate, smoothSec);
        feedback.reset(currentSampleRate, smoothSec);
        crossFeed.reset(currentSampleRate, smoothSec);
        damping.reset(currentSampleRate, smoothSec);
        dryWet.reset(currentSampleRate, smoothSec);

        reset();
    }

    void reset()
    {
        std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0f);
        std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0f);
        writeIndex = 0;
        filterStateL = 0.0f;
        filterStateR = 0.0f;
        lfoPhase = 0.0;
    }

    void setDelayTime(float seconds)   { delayTimeSec.setTargetValue(juce::jlimit(0.01f, 2.0f, seconds)); }
    void setSpinRate(float hz)         { spinRateHz.setTargetValue(juce::jlimit(0.01f, 8.0f, hz)); }
    void setSpinDepth(float depthMs)   { spinDepthMs.setTargetValue(juce::jlimit(0.0f, 30.0f, depthMs)); }
    void setFeedback(float fb)         { feedback.setTargetValue(juce::jlimit(0.0f, 0.96f, fb)); }
    void setCrossFeed(float cross)     { crossFeed.setTargetValue(juce::jlimit(0.0f, 1.0f, cross)); }
    void setDamping(float damp)        { damping.setTargetValue(juce::jlimit(0.0f, 0.95f, damp)); }
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

            const float curBaseTime = delayTimeSec.getNextValue();
            const float curRate     = spinRateHz.getNextValue();
            const float curDepth    = spinDepthMs.getNextValue();
            const float curFb       = feedback.getNextValue();
            const float curCross    = crossFeed.getNextValue();
            const float curDamp     = damping.getNextValue();
            const float curMix      = dryWet.getNextValue();

            // Quadrature circular LFO modulation (Left = sin, Right = cos)
            const float lfoL = static_cast<float>(std::sin(lfoPhase));
            const float lfoR = static_cast<float>(std::cos(lfoPhase));

            const double lfoInc = (juce::MathConstants<double>::twoPi * static_cast<double>(curRate)) / currentSampleRate;
            lfoPhase += lfoInc;
            if (lfoPhase >= juce::MathConstants<double>::twoPi)
                lfoPhase -= juce::MathConstants<double>::twoPi;

            // Calculate modulated delay lengths in sample units
            const float depthSec = curDepth * 0.001f;
            const float timeSecL = juce::jlimit(0.005f, 2.5f, curBaseTime + (lfoL * depthSec));
            const float timeSecR = juce::jlimit(0.005f, 2.5f, curBaseTime + (lfoR * depthSec));

            const float delaySamplesL = timeSecL * static_cast<float>(currentSampleRate);
            const float delaySamplesR = timeSecR * static_cast<float>(currentSampleRate);

            // Read from interpolated delay lines
            const float delayedL = readLinear(delayBufferL, delaySamplesL);
            const float delayedR = readLinear(delayBufferR, delaySamplesR);

            // Feedback damping lowpass filters
            filterStateL = (delayedL * (1.0f - curDamp)) + (filterStateL * curDamp);
            filterStateR = (delayedR * (1.0f - curDamp)) + (filterStateR * curDamp);

            // Cross-feedback matrix with soft analog saturation
            const float fbInL = (filterStateL * (1.0f - curCross)) + (filterStateR * curCross);
            const float fbInR = (filterStateR * (1.0f - curCross)) + (filterStateL * curCross);

            const float nextWriteL = std::tanh(inL + (fbInL * curFb));
            const float nextWriteR = std::tanh(inR + (fbInR * curFb));

            delayBufferL[writeIndex] = nextWriteL;
            delayBufferR[writeIndex] = nextWriteR;

            writeIndex = (writeIndex + 1) & DelayMask;

            // Mix Dry & Wet
            leftChannel[i]  = (inL * (1.0f - curMix)) + (delayedL * curMix);
            rightChannel[i] = (inR * (1.0f - curMix)) + (delayedR * curMix);
        }
    }

private:
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writeIndex { 0 };

    float filterStateL { 0.0f };
    float filterStateR { 0.0f };

    double lfoPhase { 0.0 };
    double currentSampleRate { 44100.0 };

    juce::SmoothedValue<float> delayTimeSec;
    juce::SmoothedValue<float> spinRateHz;
    juce::SmoothedValue<float> spinDepthMs;
    juce::SmoothedValue<float> feedback;
    juce::SmoothedValue<float> crossFeed;
    juce::SmoothedValue<float> damping;
    juce::SmoothedValue<float> dryWet;

    inline float readLinear(const std::vector<float>& buffer, float delaySamples) const
    {
        const int intPart = static_cast<int>(std::floor(delaySamples));
        const float frac = delaySamples - static_cast<float>(intPart);

        const int idx0 = (writeIndex - intPart + MaxDelaySamples) & DelayMask;
        const int idx1 = (idx0 - 1 + MaxDelaySamples) & DelayMask;

        return buffer[idx0] + frac * (buffer[idx1] - buffer[idx0]);
    }
};
