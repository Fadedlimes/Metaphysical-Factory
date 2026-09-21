#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

class OscillatorBank
{
public:
    static constexpr int NumOscs = 6;

    enum BankType
    {
        BankA = 0,
        BankB
    };

    enum GeneratorType
    {
        Sine = 0,
        Triangle,
        Pulse,          // Standard unipolar pulse (0 to 1)
        BipolarPulse,   // Bipolar pulse (-1 to +1)
        LPNoise,        // Pitch-tracked lowpass filtered noise
        TwoModeNoise    // Dual-mode: 0 = Pink Noise, 1 = Resonant Bandpass Noise
    };

    struct OscVoice
    {
        double phase { 0.0 };
        float semitoneOffset { 0.0f };
        float fineTuneCents { 0.0f };
        GeneratorType type { Sine };
        bool enabled { true };
        juce::SmoothedValue<float> level { 0.0f };

        // Noise generator filter states
        float lpFilterState { 0.0f };
        float svfState1 { 0.0f };
        float svfState2 { 0.0f };

        // Pink noise filter states (Paul Kellet filter)
        float b0 { 0.0f }, b1 { 0.0f }, b2 { 0.0f }, b3 { 0.0f }, b4 { 0.0f }, b5 { 0.0f }, b6 { 0.0f };

        int noiseMode { 0 }; // For TwoModeNoise: 0 = Pink, 1 = Resonant Bandpass
        float pulseWidth { 0.5f };

        // 32-bit PRNG state
        uint32_t rngState { 123456789 };

        inline float nextWhiteNoise()
        {
            rngState ^= rngState << 13;
            rngState ^= rngState >> 17;
            rngState ^= rngState << 5;
            return static_cast<float>(static_cast<int32_t>(rngState)) * 4.6566129e-10f;
        }

        void reset()
        {
            phase = 0.0;
            lpFilterState = 0.0f;
            svfState1 = 0.0f;
            svfState2 = 0.0f;
            b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
        }
    };

    OscillatorBank()
    {
        setBankType(BankA); // Default to Bank A configuration
        masterGain.setCurrentAndTargetValue(0.8f);
        baseFrequency.setCurrentAndTargetValue(110.0f);
    }

    void setBankType(BankType type)
    {
        currentBankType = type;

        if (type == BankA)
        {
            // Section A: 3x Sine, 1x Triangle, 1x Bipolar Pulse, 1x LP Filtered Noise
            voices[0].type = Sine;
            voices[1].type = Sine;
            voices[2].type = Sine;
            voices[3].type = Triangle;
            voices[4].type = BipolarPulse;
            voices[5].type = LPNoise;

            voices[0].semitoneOffset = 0.0f;   // Fundamental root
            voices[1].semitoneOffset = 7.0f;   // 5th
            voices[2].semitoneOffset = 12.0f;  // Octave 1
            voices[3].semitoneOffset = 19.0f;  // Octave 1 + 5th
            voices[4].semitoneOffset = 24.0f;  // Octave 2
            voices[5].semitoneOffset = 0.0f;   // Noise tracks root
        }
        else // BankB
        {
            // Section B: 1x Two-Mode Noise, 2x Sine, 1x Triangle, 1x Pulse, 1x Bipolar Pulse
            voices[0].type = TwoModeNoise;
            voices[1].type = Sine;
            voices[2].type = Sine;
            voices[3].type = Triangle;
            voices[4].type = Pulse;
            voices[5].type = BipolarPulse;

            voices[0].semitoneOffset = 0.0f;   // Noise tracks root
            voices[1].semitoneOffset = 0.0f;   // Fundamental root
            voices[2].semitoneOffset = 7.0f;   // 5th
            voices[3].semitoneOffset = 12.0f;  // Octave 1
            voices[4].semitoneOffset = 19.0f;  // Octave 1 + 5th
            voices[5].semitoneOffset = 24.0f;  // Octave 2
        }

        for (size_t i = 0; i < NumOscs; ++i)
        {
            voices[i].fineTuneCents = 0.0f;
            voices[i].rngState = static_cast<uint32_t>(123456789 + (i * 98765));
        }
    }

    void prepare(double sampleRate, int /*samplesPerBlock*/)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

        const double smoothTimeSec = 0.02; // 20ms smoothing
        for (size_t i = 0; i < NumOscs; ++i)
        {
            voices[i].level.reset(currentSampleRate, smoothTimeSec);
        }
        masterGain.reset(currentSampleRate, smoothTimeSec);
        baseFrequency.reset(currentSampleRate, smoothTimeSec);

        reset();
    }

    void reset()
    {
        for (auto& v : voices)
            v.reset();
    }

    void setBaseFrequency(float freqHz)
    {
        baseFrequency.setTargetValue(juce::jmax(10.0f, freqHz));
    }

    void setMasterGain(float gainLinear)
    {
        masterGain.setTargetValue(juce::jlimit(0.0f, 2.0f, gainLinear));
    }

    void setOscSemitone(int index, float semitone)
    {
        if (juce::isPositiveAndBelow(index, NumOscs))
            voices[static_cast<size_t>(index)].semitoneOffset = semitone;
    }

    void setOscFineTune(int index, float cents)
    {
        if (juce::isPositiveAndBelow(index, NumOscs))
            voices[static_cast<size_t>(index)].fineTuneCents = juce::jlimit(-100.0f, 100.0f, cents);
    }

    void setOscLevel(int index, float targetLevel)
    {
        if (juce::isPositiveAndBelow(index, NumOscs))
            voices[static_cast<size_t>(index)].level.setTargetValue(juce::jlimit(0.0f, 1.0f, targetLevel));
    }

    void setOscNoiseMode(int index, int mode)
    {
        if (juce::isPositiveAndBelow(index, NumOscs))
            voices[static_cast<size_t>(index)].noiseMode = mode;
    }

    void setOscPulseWidth(int index, float pw)
    {
        if (juce::isPositiveAndBelow(index, NumOscs))
            voices[static_cast<size_t>(index)].pulseWidth = juce::jlimit(0.05f, 0.95f, pw);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, int channel, int startSample, int numSamples, bool accumulate)
    {
        auto* writePtr = buffer.getWritePointer(channel, startSample);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float currentBaseFreq = baseFrequency.getNextValue();
            const float currentMasterGain = masterGain.getNextValue();
            float sampleSum = 0.0f;

            for (size_t i = 0; i < NumOscs; ++i)
            {
                auto& v = voices[i];
                const float vLevel = v.level.getNextValue();

                if (!v.enabled || vLevel <= 0.00001f)
                    continue;

                // Combined frequency: base * 2 ^ ((semitones + cents/100) / 12)
                const float totalSemitones = v.semitoneOffset + (v.fineTuneCents * 0.01f);
                const float oscFreq = currentBaseFreq * std::pow(2.0f, totalSemitones / 12.0f);
                const double phaseIncrement = static_cast<double>(oscFreq) / currentSampleRate;

                // Render specific generator model
                const float rawSample = renderVoice(v, oscFreq, static_cast<float>(phaseIncrement));
                sampleSum += rawSample * vLevel;

                // Advance phase
                v.phase += phaseIncrement;
                if (v.phase >= 1.0)
                    v.phase -= 1.0;
            }

            const float outputSample = sampleSum * currentMasterGain;

            if (accumulate)
                writePtr[sample] += outputSample;
            else
                writePtr[sample] = outputSample;
        }
    }

private:
    std::array<OscVoice, NumOscs> voices;
    BankType currentBankType { BankA };

    juce::SmoothedValue<float> masterGain;
    juce::SmoothedValue<float> baseFrequency;
    double currentSampleRate { 44100.0 };

    static inline float polyBlep(float t, float dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        else if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float renderVoice(OscVoice& v, float oscFreq, float dt)
    {
        const float phase = static_cast<float>(v.phase);

        switch (v.type)
        {
            case Sine:
                return std::sin(phase * juce::MathConstants<float>::twoPi);

            case Triangle:
                return 2.0f * std::abs(2.0f * phase - 1.0f) - 1.0f;

            case Pulse:
            {
                // Standard unipolar pulse with PolyBLEP anti-aliasing
                float naive = (phase < v.pulseWidth) ? 1.0f : 0.0f;
                naive += polyBlep(phase, dt);
                naive -= polyBlep(std::fmod(phase + (1.0f - v.pulseWidth), 1.0f), dt);
                return (naive * 2.0f) - 1.0f; // Center to bipolar output
            }

            case BipolarPulse:
            {
                // Classic -1 to +1 bipolar pulse
                float naive = (phase < v.pulseWidth) ? 1.0f : -1.0f;
                naive += polyBlep(phase, dt);
                naive -= polyBlep(std::fmod(phase + (1.0f - v.pulseWidth), 1.0f), dt);
                return naive;
            }

            case LPNoise:
            {
                // Pitch-tracked lowpass filtered white noise
                const float white = v.nextWhiteNoise();
                const float cutoff = juce::jlimit(20.0f, 18000.0f, oscFreq * 2.5f);
                const float alpha = juce::jlimit(0.001f, 0.99f,
                    1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoff / static_cast<float>(currentSampleRate)));

                v.lpFilterState += alpha * (white - v.lpFilterState);
                return v.lpFilterState * 2.5f;
            }

            case TwoModeNoise:
            {
                const float white = v.nextWhiteNoise();

                if (v.noiseMode == 0)
                {
                    // Mode 0: Authentic Paul Kellet 3dB/octave Pink Noise
                    v.b0 = 0.99886f * v.b0 + white * 0.0555179f;
                    v.b1 = 0.99332f * v.b1 + white * 0.0750759f;
                    v.b2 = 0.96900f * v.b2 + white * 0.1538520f;
                    v.b3 = 0.86650f * v.b3 + white * 0.3104856f;
                    v.b4 = 0.55000f * v.b4 + white * 0.5329522f;
                    v.b5 = -0.7616f * v.b5 - white * 0.0168980f;
                    const float pink = (v.b0 + v.b1 + v.b2 + v.b3 + v.b4 + v.b5 + v.b6 + white * 0.5362f) * 0.11f;
                    v.b6 = white * 0.115926f;
                    return pink * 2.5f;
                }
                else
                {
                    // Mode 1: Resonant Pitch-Tracked Bandpass Noise (whistling wind texture)
                    const float g = std::tan(juce::MathConstants<float>::pi * (oscFreq / static_cast<float>(currentSampleRate)));
                    const float k = 0.15f; // High resonance Q ~ 6.6
                    const float a1 = 1.0f / (1.0f + g * (g + k));
                    const float a2 = g * a1;
                    const float a3 = g * a2;

                    const float v3 = white - v.svfState2;
                    const float v1 = a1 * v.svfState1 + a2 * v3;
                    const float v2 = v.svfState2 + a2 * v.svfState1 + a3 * v3;
                    v.svfState1 = 2.0f * v1 - v.svfState1;
                    v.svfState2 = 2.0f * v2 - v.svfState2;

                    return v1 * 3.5f; // Bandpass output
                }
            }

            default:
                return 0.0f;
        }
    }
};