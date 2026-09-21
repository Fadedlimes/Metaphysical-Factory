#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>

class Resochord
{
public:
    static constexpr int NumVoices = 6;
    static constexpr int MaxDelaySamples = 32768; // Powers of two for fast bitwise masking
    static constexpr int DelayMask = MaxDelaySamples - 1;

    struct CombVoice
    {
        std::vector<float> buffer;
        int writeIndex { 0 };
        float semitoneOffset { 0.0f };
        float fineTuneCents { 0.0f };
        float pan { 0.0f }; // -1.0 (Left) to +1.0 (Right)
        float level { 1.0f };
        float filterState { 0.0f };

        void init()
        {
            buffer.assign(MaxDelaySamples, 0.0f);
            writeIndex = 0;
            filterState = 0.0f;
        }

        void reset()
        {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            writeIndex = 0;
            filterState = 0.0f;
        }
    };

    Resochord()
    {
        for (int i = 0; i < NumVoices; ++i)
        {
            voices[i].init();
        }

        // Set default spatial stereo spread across the 6 voices
        voices[0].pan = -0.8f;
        voices[1].pan =  0.8f;
        voices[2].pan = -0.4f;
        voices[3].pan =  0.4f;
        voices[4].pan = -0.1f;
        voices[5].pan =  0.1f;

        // Default resonant chord tuning (Root, 5th, Octave, Maj 10th, 12th, Double Octave)
        voices[0].semitoneOffset = 0.0f;
        voices[1].semitoneOffset = 7.0f;
        voices[2].semitoneOffset = 12.0f;
        voices[3].semitoneOffset = 16.0f;
        voices[4].semitoneOffset = 19.0f;
        voices[5].semitoneOffset = 24.0f;

        rootFrequency.setCurrentAndTargetValue(110.0f); // Default A2
        masterResonance.setCurrentAndTargetValue(0.92f);
        masterDamping.setCurrentAndTargetValue(0.3f);
        dryWet.setCurrentAndTargetValue(0.5f);
    }

    void prepare(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double smoothSec = 0.02; // 20ms click-free smoothing

        rootFrequency.reset(currentSampleRate, smoothSec);
        masterResonance.reset(currentSampleRate, smoothSec);
        masterDamping.reset(currentSampleRate, smoothSec);
        dryWet.reset(currentSampleRate, smoothSec);

        reset();
    }

    void reset()
    {
        for (auto& v : voices)
            v.reset();
    }

    void setRootFrequency(float freqHz)
    {
        rootFrequency.setTargetValue(juce::jlimit(20.0f, 4000.0f, freqHz));
    }

    void setMasterResonance(float res)
    {
        // Limit below 0.999 to prevent feedback explosion
        masterResonance.setTargetValue(juce::jlimit(0.0f, 0.995f, res));
    }

    void setMasterDamping(float damp)
    {
        masterDamping.setTargetValue(juce::jlimit(0.0f, 0.95f, damp));
    }

    void setDryWet(float mix)
    {
        dryWet.setTargetValue(juce::jlimit(0.0f, 1.0f, mix));
    }

    void setVoiceSemitone(int index, float semitones)
    {
        if (juce::isPositiveAndBelow(index, NumVoices))
            voices[index].semitoneOffset = semitones;
    }

    void setVoiceFineTune(int index, float cents)
    {
        if (juce::isPositiveAndBelow(index, NumVoices))
            voices[index].fineTuneCents = cents;
    }

    void setVoiceLevel(int index, float lvl)
    {
        if (juce::isPositiveAndBelow(index, NumVoices))
            voices[index].level = juce::jlimit(0.0f, 1.0f, lvl);
    }

    void setVoicePan(int index, float pan)
    {
        if (juce::isPositiveAndBelow(index, NumVoices))
            voices[index].pan = juce::jlimit(-1.0f, 1.0f, pan);
    }

    // Process stereo buffer in place: in/out
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (buffer.getNumChannels() < 2)
            return;

        auto* leftChannel = buffer.getWritePointer(0, startSample);
        auto* rightChannel = buffer.getWritePointer(1, startSample);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float inL = leftChannel[sample];
            const float inR = rightChannel[sample];
            const float monoInput = (inL + inR) * 0.5f;

            const float currentRootFreq = rootFrequency.getNextValue();
            const float currentResonance = masterResonance.getNextValue();
            const float currentDamping = masterDamping.getNextValue();
            const float currentMix = dryWet.getNextValue();

            float wetL = 0.0f;
            float wetR = 0.0f;

            for (int i = 0; i < NumVoices; ++i)
            {
                auto& v = voices[i];
                if (v.level <= 0.0001f)
                    continue;

                // Pitch calculation
                const float totalSemitones = v.semitoneOffset + (v.fineTuneCents * 0.01f);
                const float voiceFreq = currentRootFreq * std::pow(2.0f, totalSemitones / 12.0f);
                const float delayInSamples = juce::jlimit(2.0f, static_cast<float>(MaxDelaySamples - 4),
                                                          static_cast<float>(currentSampleRate / voiceFreq));

                // Read interpolated sample from delay line
                const float combOut = readHermite(v, delayInSamples);

                // Lowpass filter in the feedback loop for natural acoustic damping
                v.filterState = (combOut * (1.0f - currentDamping)) + (v.filterState * currentDamping);

                // Feedback injection
                const float toWrite = monoInput + (v.filterState * currentResonance);
                v.buffer[v.writeIndex] = toWrite;
                v.writeIndex = (v.writeIndex + 1) & DelayMask;

                // Stereo Panning (Equal Power)
                const float voiceGain = v.level * 0.35f; // Headroom normalization across 6 voices
                const float panAngle = (v.pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // 0 to pi/2
                const float gainL = std::cos(panAngle) * voiceGain;
                const float gainR = std::sin(panAngle) * voiceGain;

                wetL += combOut * gainL;
                wetR += combOut * gainR;
            }

            // Crossfade Dry and Wet
            leftChannel[sample]  = (inL * (1.0f - currentMix)) + (wetL * currentMix);
            rightChannel[sample] = (inR * (1.0f - currentMix)) + (wetR * currentMix);
        }
    }

private:
    std::array<CombVoice, NumVoices> voices;
    juce::SmoothedValue<float> rootFrequency;
    juce::SmoothedValue<float> masterResonance;
    juce::SmoothedValue<float> masterDamping;
    juce::SmoothedValue<float> dryWet;
    double currentSampleRate { 44100.0 };

    // 4-point, 3rd-order Hermite interpolation for smooth pitch shifting
    float readHermite(const CombVoice& v, float delaySamples) const
    {
        const int intPart = static_cast<int>(std::floor(delaySamples));
        const float fracPart = delaySamples - static_cast<float>(intPart);

        const int i1 = (v.writeIndex - intPart + MaxDelaySamples) & DelayMask;
        const int i0 = (i1 + 1) & DelayMask;
        const int i2 = (i1 - 1 + MaxDelaySamples) & DelayMask;
        const int i3 = (i1 - 2 + MaxDelaySamples) & DelayMask;

        const float y0 = v.buffer[i0];
        const float y1 = v.buffer[i1];
        const float y2 = v.buffer[i2];
        const float y3 = v.buffer[i3];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * fracPart + c2) * fracPart + c1) * fracPart + c0;
    }
};
