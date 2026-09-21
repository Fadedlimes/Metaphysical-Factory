#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <atomic>
#include <vector>

class SampleLooper
{
public:
    SampleLooper()
    {
        gain.setCurrentAndTargetValue(0.7f);
        playbackSpeed.setCurrentAndTargetValue(1.0f);
        pitchSemitones.setCurrentAndTargetValue(0.0f);

        // Generate synthetic ambient texture by default
        generateDefaultTexture();
    }

    void prepare(double sampleRate, int /*samplesPerBlock*/)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double smoothSec = 0.02;

        gain.reset(currentSampleRate, smoothSec);
        playbackSpeed.reset(currentSampleRate, smoothSec);
        pitchSemitones.reset(currentSampleRate, smoothSec);

        reset();
    }

    void reset()
    {
        playheadPosition = loopStartNormalized * static_cast<double>(totalSampleLength);
        normalizedPlayhead.store(static_cast<float>(loopStartNormalized));
    }

    void setGain(float newGain)
    {
        gain.setTargetValue(juce::jlimit(0.0f, 2.0f, newGain));
    }

    void setPlaybackSpeed(float speed)
    {
        playbackSpeed.setTargetValue(juce::jlimit(-4.0f, 4.0f, speed));
    }

    void setPitchSemitones(float semitones)
    {
        pitchSemitones.setTargetValue(juce::jlimit(-36.0f, 36.0f, semitones));
    }

    void setLoopPoints(float startNorm, float endNorm)
    {
        const float safeStart = juce::jlimit(0.0f, 0.95f, startNorm);
        const float safeEnd   = juce::jlimit(safeStart + 0.02f, 1.0f, endNorm);

        loopStartNormalized = safeStart;
        loopEndNormalized   = safeEnd;
    }

    void setCrossfadeLength(float xfadeNorm)
    {
        crossfadeNormalized = juce::jlimit(0.001f, 0.45f, xfadeNorm);
    }

    // Thread-safe sample file loading from disk
    bool loadFile(const juce::File& file, juce::AudioFormatManager& formatManager)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr)
            return false;

        const int numChannels = juce::jmin(2, static_cast<int>(reader->numChannels));
        const int numSamples  = static_cast<int>(reader->lengthInSamples);

        if (numSamples < 128)
            return false;

        juce::AudioBuffer<float> tempBuffer(numChannels, numSamples);
        reader->read(&tempBuffer, 0, numSamples, 0, true, true);

        // Safely acquire lock to swap buffer
        const juce::SpinLock::ScopedLockType sl(bufferLock);
        sampleBuffer.makeCopyOf(tempBuffer);
        sourceSampleRate = reader->sampleRate;
        totalSampleLength = numSamples;
        playheadPosition = loopStartNormalized * static_cast<double>(totalSampleLength);
        loadedSampleName = file.getFileName();
        hasCustomSampleLoaded.store(true);

        return true;
    }

    // --- UI Telemetry Accessors ---

    float getNormalizedPlayhead() const
    {
        return normalizedPlayhead.load();
    }

    float getLoopStart() const { return loopStartNormalized; }
    float getLoopEnd()   const { return loopEndNormalized; }

    juce::String getLoadedSampleName() const
    {
        const juce::SpinLock::ScopedTryLockType tryLock(const_cast<juce::SpinLock&>(bufferLock));
        if (tryLock.isLocked())
            return loadedSampleName;
        return "TEXTURE LOOPER";
    }

    // Peak binning for UI waveform rendering
    void getWaveformPeaks(std::vector<float>& minPeaks, std::vector<float>& maxPeaks, int numBins) const
    {
        const juce::SpinLock::ScopedTryLockType tryLock(const_cast<juce::SpinLock&>(bufferLock));
        if (!tryLock.isLocked() || totalSampleLength <= 0 || numBins <= 0)
            return;

        minPeaks.assign(static_cast<size_t>(numBins), 0.0f);
        maxPeaks.assign(static_cast<size_t>(numBins), 0.0f);

        const float* readPtr = sampleBuffer.getReadPointer(0);
        const double samplesPerBin = static_cast<double>(totalSampleLength) / static_cast<double>(numBins);

        for (int bin = 0; bin < numBins; ++bin)
        {
            const int startIdx = static_cast<int>(bin * samplesPerBin);
            const int endIdx   = juce::jmin(totalSampleLength, static_cast<int>((bin + 1) * samplesPerBin));

            float mn = 0.0f;
            float mx = 0.0f;

            for (int s = startIdx; s < endIdx; ++s)
            {
                const float val = readPtr[s];
                if (val < mn) mn = val;
                if (val > mx) mx = val;
            }

            minPeaks[static_cast<size_t>(bin)] = mn;
            maxPeaks[static_cast<size_t>(bin)] = mx;
        }
    }

    void processBlock(juce::AudioBuffer<float>& outBuffer, int startSample, int numSamples)
    {
        const juce::SpinLock::ScopedTryLockType tryLock(bufferLock);

        // If buffer is currently being rewritten by disk loader, output silence for this block
        if (!tryLock.isLocked() || totalSampleLength < 128)
            return;

        auto* outL = outBuffer.getWritePointer(0, startSample);
        auto* outR = outBuffer.getNumChannels() > 1 ? outBuffer.getWritePointer(1, startSample) : nullptr;

        const int bufferChannels = sampleBuffer.getNumChannels();
        const float* const inL = sampleBuffer.getReadPointer(0);
        const float* const inR = bufferChannels > 1 ? sampleBuffer.getReadPointer(1) : inL;

        for (int i = 0; i < numSamples; ++i)
        {
            const float curGain  = gain.getNextValue();
            const float curSpeed = playbackSpeed.getNextValue();
            const float curPitch = pitchSemitones.getNextValue();

            if (curGain <= 0.0001f || std::abs(curSpeed) <= 0.00001f)
                continue;

            // Calculate pitch and speed increment
            const float pitchRatio = std::pow(2.0f, curPitch / 12.0f);
            const double rateRatio = (sourceSampleRate / currentSampleRate);
            const double step = static_cast<double>(curSpeed * pitchRatio) * rateRatio;

            // Define current active loop boundaries in sample units
            const double startSampleIdx = loopStartNormalized * static_cast<double>(totalSampleLength - 1);
            const double endSampleIdx   = loopEndNormalized   * static_cast<double>(totalSampleLength - 1);
            const double loopLength     = endSampleIdx - startSampleIdx;

            if (loopLength < 64.0)
                continue;

            const double xfadeSamples = juce::jmax(16.0, loopLength * static_cast<double>(crossfadeNormalized));

            // Read current sample via 4-point cubic Hermite interpolation
            float sampL = readHermite(inL, totalSampleLength, playheadPosition);
            float sampR = readHermite(inR, totalSampleLength, playheadPosition);

            // Boundary equal-power crossfading
            if (step > 0.0) // Moving forward
            {
                if (playheadPosition >= (endSampleIdx - xfadeSamples))
                {
                    const double progress = (playheadPosition - (endSampleIdx - xfadeSamples)) / xfadeSamples;
                    const double wrappedPos = playheadPosition - loopLength;

                    const float fadeOut = static_cast<float>(std::cos(progress * juce::MathConstants<double>::halfPi));
                    const float fadeIn  = static_cast<float>(std::sin(progress * juce::MathConstants<double>::halfPi));

                    sampL = (sampL * fadeOut) + (readHermite(inL, totalSampleLength, wrappedPos) * fadeIn);
                    sampR = (sampR * fadeOut) + (readHermite(inR, totalSampleLength, wrappedPos) * fadeIn);
                }

                playheadPosition += step;
                if (playheadPosition >= endSampleIdx)
                    playheadPosition -= loopLength;
            }
            else // Moving backward (reverse)
            {
                if (playheadPosition <= (startSampleIdx + xfadeSamples))
                {
                    const double progress = ((startSampleIdx + xfadeSamples) - playheadPosition) / xfadeSamples;
                    const double wrappedPos = playheadPosition + loopLength;

                    const float fadeOut = static_cast<float>(std::cos(progress * juce::MathConstants<double>::halfPi));
                    const float fadeIn  = static_cast<float>(std::sin(progress * juce::MathConstants<double>::halfPi));

                    sampL = (sampL * fadeOut) + (readHermite(inL, totalSampleLength, wrappedPos) * fadeIn);
                    sampR = (sampR * fadeOut) + (readHermite(inR, totalSampleLength, wrappedPos) * fadeIn);
                }

                playheadPosition += step;
                if (playheadPosition <= startSampleIdx)
                    playheadPosition += loopLength;
            }

            outL[i] += sampL * curGain;
            if (outR != nullptr)
                outR[i] += sampR * curGain;
        }

        // Lock-free broadcast of normalized playhead for UI scope
        if (totalSampleLength > 0)
        {
            normalizedPlayhead.store(juce::jlimit(0.0f, 1.0f, static_cast<float>(playheadPosition / static_cast<double>(totalSampleLength))));
        }
    }

private:
    juce::AudioBuffer<float> sampleBuffer;
    juce::SpinLock bufferLock;

    int totalSampleLength { 0 };
    double sourceSampleRate { 44100.0 };
    double currentSampleRate { 44100.0 };
    double playheadPosition { 0.0 };

    std::atomic<float> normalizedPlayhead { 0.0f };
    std::atomic<bool> hasCustomSampleLoaded { false };
    juce::String loadedSampleName { "TEXTURE SYNTH DRONE" };

    float loopStartNormalized { 0.0f };
    float loopEndNormalized   { 1.0f };
    float crossfadeNormalized { 0.1f };

    juce::SmoothedValue<float> gain;
    juce::SmoothedValue<float> playbackSpeed;
    juce::SmoothedValue<float> pitchSemitones;

    void generateDefaultTexture()
    {
        const int length = 88200; // 2 seconds at 44.1kHz
        sampleBuffer.setSize(2, length);
        sourceSampleRate = 44100.0;
        totalSampleLength = length;
        loadedSampleName = "TEXTURE SYNTH DRONE";

        auto* left = sampleBuffer.getWritePointer(0);
        auto* right = sampleBuffer.getWritePointer(1);

        for (int i = 0; i < length; ++i)
        {
            const double t = static_cast<double>(i) / 44100.0;

            float drone = static_cast<float>(
                0.40 * std::sin(juce::MathConstants<double>::twoPi * 55.0 * t) +
                0.25 * std::sin(juce::MathConstants<double>::twoPi * 110.0 * t) +
                0.15 * std::sin(juce::MathConstants<double>::twoPi * 165.0 * t) +
                0.10 * std::sin(juce::MathConstants<double>::twoPi * 220.0 * t) +
                0.05 * std::sin(juce::MathConstants<double>::twoPi * 330.0 * t)
            );

            float leftDetune = static_cast<float>(0.05 * std::sin(juce::MathConstants<double>::twoPi * 55.3 * t));
            float rightDetune = static_cast<float>(0.05 * std::sin(juce::MathConstants<double>::twoPi * 54.7 * t));

            left[i]  = std::tanh(drone + leftDetune);
            right[i] = std::tanh(drone + rightDetune);
        }
    }

    static inline float readHermite(const float* buffer, int bufferSize, double position)
    {
        while (position < 0.0)
            position += static_cast<double>(bufferSize);

        const int i1 = static_cast<int>(position) % bufferSize;
        const float frac = static_cast<float>(position - std::floor(position));

        const int i0 = (i1 - 1 + bufferSize) % bufferSize;
        const int i2 = (i1 + 1) % bufferSize;
        const int i3 = (i1 + 2) % bufferSize;

        const float y0 = buffer[i0];
        const float y1 = buffer[i1];
        const float y2 = buffer[i2];
        const float y3 = buffer[i3];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
};