#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>

class MasterEq
{
public:
    MasterEq()
    {
        bassGainDb.setCurrentAndTargetValue(0.0f);
        trebleGainDb.setCurrentAndTargetValue(0.0f);
    }

    void prepare(double sampleRate, int /*samplesPerBlock*/)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double smoothSec = 0.02;

        bassGainDb.reset(currentSampleRate, smoothSec);
        trebleGainDb.reset(currentSampleRate, smoothSec);

        reset();
    }

    void reset()
    {
        lowShelfL.reset();
        lowShelfR.reset();
        highShelfL.reset();
        highShelfR.reset();
    }

    void setBassGainDb(float gainDb)
    {
        bassGainDb.setTargetValue(juce::jlimit(-12.0f, 12.0f, gainDb));
    }

    void setTrebleGainDb(float gainDb)
    {
        trebleGainDb.setTargetValue(juce::jlimit(-12.0f, 12.0f, gainDb));
    }

    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (buffer.getNumChannels() < 2)
            return;

        auto* channelL = buffer.getWritePointer(0, startSample);
        auto* channelR = buffer.getWritePointer(1, startSample);

        for (int i = 0; i < numSamples; ++i)
        {
            const float curBassDb   = bassGainDb.getNextValue();
            const float curTrebleDb = trebleGainDb.getNextValue();

            // Convert dB to linear gain multipliers
            const float bassGainLin   = std::pow(10.0f, curBassDb / 20.0f);
            const float trebleGainLin = std::pow(10.0f, curTrebleDb / 20.0f);

            // 1. Process Low Shelf (120 Hz)
            float sL = lowShelfL.processLowShelf(channelL[i], 120.0f, bassGainLin, currentSampleRate);
            float sR = lowShelfR.processLowShelf(channelR[i], 120.0f, bassGainLin, currentSampleRate);

            // 2. Process High Shelf (6500 Hz)
            channelL[i] = highShelfL.processHighShelf(sL, 6500.0f, trebleGainLin, currentSampleRate);
            channelR[i] = highShelfR.processHighShelf(sR, 6500.0f, trebleGainLin, currentSampleRate);
        }
    }

private:
    struct ShelvingFilter
    {
        float s1 { 0.0f };

        void reset()
        {
            s1 = 0.0f;
        }

        inline float processLowShelf(float input, float freqHz, float gainLin, double sampleRate)
        {
            // First-order zero-delay low shelf
            const float g = std::tan(juce::MathConstants<float>::pi * (freqHz / static_cast<float>(sampleRate)));
            const float a = (g - 1.0f) / (g + 1.0f);

            const float allpass = a * input + s1;
            s1 = input - a * allpass;

            // Blend dry and allpass to create shelf
            const float low = (input + allpass) * 0.5f;
            const float high = (input - allpass) * 0.5f;

            return (low * gainLin) + high;
        }

        inline float processHighShelf(float input, float freqHz, float gainLin, double sampleRate)
        {
            // First-order zero-delay high shelf
            const float g = std::tan(juce::MathConstants<float>::pi * (freqHz / static_cast<float>(sampleRate)));
            const float a = (g - 1.0f) / (g + 1.0f);

            const float allpass = a * input + s1;
            s1 = input - a * allpass;

            const float low = (input + allpass) * 0.5f;
            const float high = (input - allpass) * 0.5f;

            return low + (high * gainLin);
        }
    };

    ShelvingFilter lowShelfL;
    ShelvingFilter lowShelfR;
    ShelvingFilter highShelfL;
    ShelvingFilter highShelfR;

    juce::SmoothedValue<float> bassGainDb;
    juce::SmoothedValue<float> trebleGainDb;

    double currentSampleRate { 44100.0 };
};