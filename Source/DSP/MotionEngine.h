#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <cmath>
#include <atomic>

class MotionEngine
{
public:
    static constexpr int NumLanes = 32;
    static constexpr int MaxPoints = 4096;
    static constexpr double RecordRateHz = 100.0; // 100 Hz sampling (10ms resolution)

    enum MotionMode
    {
        Static = 0,     // Manual fader position
        Looping,        // Playing back recorded gesture
        SineLFO,        // Generative sine modulation
        RandomWalk      // Generative smooth Brownian motion
    };

    struct Lane
    {
        std::vector<float> buffer;
        int recordedLength { 0 };
        double playhead { 0.0 };
        double lfoPhase { 0.0 };
        float randomWalkTarget { 0.5f };
        float randomWalkCurrent { 0.5f };

        std::atomic<bool> isArmed { false };
        std::atomic<bool> isRecording { false };
        std::atomic<MotionMode> mode { Static };

        std::atomic<float> inputNormalized { 0.5f };
        float smoothedRecordInput { 0.5f };
        float baseValue { 0.5f };
        float currentValue { 0.5f };
        float speedMultiplier { 1.0f }; // -4.0 to +4.0
        float depth { 1.0f };           // 0.0 to 1.0

        void init()
        {
            buffer.assign(MaxPoints, 0.5f);
            recordedLength = 0;
            playhead = 0.0;
            lfoPhase = 0.0;
            mode.store(Static);
            baseValue = 0.5f;
            currentValue = 0.5f;
            smoothedRecordInput = 0.5f;
            speedMultiplier = 1.0f;
            depth = 1.0f;
            inputNormalized.store(0.5f);
            isArmed.store(false);
            isRecording.store(false);
        }

        void reset()
        {
            playhead = 0.0;
            lfoPhase = 0.0;
            currentValue = baseValue;
            smoothedRecordInput = baseValue;
        }
    };

    MotionEngine()
    {
        for (size_t i = 0; i < NumLanes; ++i)
            lanes[i].init();
    }

    void prepare(double sampleRate, int /*samplesPerBlock*/)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        timeAccumulator = 0.0;
        reset();
    }

    void reset()
    {
        for (auto& l : lanes)
            l.reset();
        timeAccumulator = 0.0;
    }

    // Push current normalized value (0.0 to 1.0) into the lane
    void setNormalizedValue(int laneIdx, float normVal)
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
        {
            auto& l = lanes[static_cast<size_t>(laneIdx)];
            const float clamped = juce::jlimit(0.0f, 1.0f, normVal);

            l.inputNormalized.store(clamped);
            l.baseValue = clamped;

            if (l.mode.load() == Static && !l.isRecording.load())
                l.currentValue = clamped;
        }
    }

    void setLaneMode(int laneIdx, MotionMode m)
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
            lanes[static_cast<size_t>(laneIdx)].mode.store(m);
    }

    MotionMode getLaneMode(int laneIdx) const
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
            return lanes[static_cast<size_t>(laneIdx)].mode.load();
        return Static;
    }

    bool isLaneLooping(int laneIdx) const
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
            return lanes[static_cast<size_t>(laneIdx)].mode.load() == Looping;
        return false;
    }

    void setLaneArmed(int laneIdx, bool armed)
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
            lanes[static_cast<size_t>(laneIdx)].isArmed.store(armed);
    }

    bool isLaneArmed(int laneIdx) const
    {
        return juce::isPositiveAndBelow(laneIdx, NumLanes) && lanes[static_cast<size_t>(laneIdx)].isArmed.load();
    }

    bool isLaneRecording(int laneIdx) const
    {
        return juce::isPositiveAndBelow(laneIdx, NumLanes) && lanes[static_cast<size_t>(laneIdx)].isRecording.load();
    }

    void setLaneSpeed(int laneIdx, float speed)
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
            lanes[static_cast<size_t>(laneIdx)].speedMultiplier = juce::jlimit(-4.0f, 4.0f, speed);
    }

    void setLaneDepth(int laneIdx, float d)
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
            lanes[static_cast<size_t>(laneIdx)].depth = juce::jlimit(0.0f, 1.0f, d);
    }

    void startRecording(int laneIdx)
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
        {
            auto& l = lanes[static_cast<size_t>(laneIdx)];
            l.recordedLength = 0;
            l.playhead = 0.0;
            l.smoothedRecordInput = l.inputNormalized.load();
            l.isRecording.store(true);
        }
    }

    void stopRecording(int laneIdx)
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
        {
            auto& l = lanes[static_cast<size_t>(laneIdx)];
            l.isRecording.store(false);
            l.isArmed.store(false);

            // If gesture has at least 8 samples (~80ms), activate looping
            if (l.recordedLength > 8)
            {
                l.mode.store(Looping);
                l.playhead = 0.0;
            }
            else
            {
                l.mode.store(Static);
            }
        }
    }

    // Get current evaluated value for DSP consumption (0.0 to 1.0)
    float getLaneValue(int laneIdx) const
    {
        if (juce::isPositiveAndBelow(laneIdx, NumLanes))
            return lanes[static_cast<size_t>(laneIdx)].currentValue;
        return 0.5f;
    }

    // Normalized 0.0 to 1.0 playhead position for UI animation
    float getPlayheadProgress(int laneIdx) const
    {
        if (!juce::isPositiveAndBelow(laneIdx, NumLanes))
            return 0.0f;

        const auto& l = lanes[static_cast<size_t>(laneIdx)];
        if (l.mode.load() != Looping || l.recordedLength <= 1)
            return 0.0f;

        return static_cast<float>(l.playhead / static_cast<double>(l.recordedLength));
    }

    // Time-accurate block processing with input anti-aliasing filter
    void advanceBlock(int numSamples)
    {
        const double blockTimeSec = static_cast<double>(numSamples) / currentSampleRate;
        const double timeStep = 1.0 / RecordRateHz; // 0.01 seconds

        timeAccumulator += blockTimeSec;

        // 1. Clock-accurate recording pass with 25 Hz lowpass anti-jitter filter
        while (timeAccumulator >= timeStep)
        {
            timeAccumulator -= timeStep;

            for (size_t i = 0; i < NumLanes; ++i)
            {
                auto& l = lanes[i];
                if (l.isRecording.load())
                {
                    if (l.recordedLength < MaxPoints)
                    {
                        // Slew filter to smooth discrete mouse pixel stepping
                        const float target = l.inputNormalized.load();
                        l.smoothedRecordInput += (target - l.smoothedRecordInput) * 0.45f;

                        l.buffer[static_cast<size_t>(l.recordedLength++)] = l.smoothedRecordInput;
                    }
                    else
                    {
                        stopRecording(static_cast<int>(i));
                    }
                }
            }
        }

        // 2. Playback and Modulation Evaluation
        for (size_t i = 0; i < NumLanes; ++i)
        {
            auto& l = lanes[i];

            if (l.isRecording.load())
            {
                l.currentValue = l.inputNormalized.load();
                continue;
            }

            switch (l.mode.load())
            {
                case Static:
                {
                    l.currentValue = l.baseValue;
                    break;
                }

                case Looping:
                {
                    const int len = l.recordedLength;
                    if (len <= 1)
                    {
                        l.currentValue = l.baseValue;
                        break;
                    }

                    // Advance playhead by actual time elapsed
                    const double step = RecordRateHz * static_cast<double>(l.speedMultiplier) * blockTimeSec;
                    l.playhead += step;

                    const double dLen = static_cast<double>(len);
                    while (l.playhead >= dLen)
                        l.playhead -= dLen;
                    while (l.playhead < 0.0)
                        l.playhead += dLen;

                    // 4-point, 3rd-order cubic Hermite spline interpolation for smooth curves
                    const int i1 = static_cast<int>(std::floor(l.playhead)) % len;
                    const int i0 = (i1 - 1 + len) % len;
                    const int i2 = (i1 + 1) % len;
                    const int i3 = (i1 + 2) % len;
                    const float frac = static_cast<float>(l.playhead - std::floor(l.playhead));

                    const float y0 = l.buffer[static_cast<size_t>(i0)];
                    const float y1 = l.buffer[static_cast<size_t>(i1)];
                    const float y2 = l.buffer[static_cast<size_t>(i2)];
                    const float y3 = l.buffer[static_cast<size_t>(i3)];

                    const float c0 = y1;
                    const float c1 = 0.5f * (y2 - y0);
                    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

                    const float loopedVal = juce::jlimit(0.0f, 1.0f, ((c3 * frac + c2) * frac + c1) * frac + c0);

                    // 1:1 playback at full depth, or scaled relative to baseValue
                    if (l.depth >= 0.999f)
                        l.currentValue = loopedVal;
                    else
                        l.currentValue = juce::jlimit(0.0f, 1.0f, l.baseValue + (loopedVal - l.baseValue) * l.depth);
                    break;
                }

                case SineLFO:
                {
                    const double rateHz = 0.25 * static_cast<double>(l.speedMultiplier);
                    l.lfoPhase += juce::MathConstants<double>::twoPi * rateHz * blockTimeSec;

                    if (l.lfoPhase >= juce::MathConstants<double>::twoPi)
                        l.lfoPhase -= juce::MathConstants<double>::twoPi;

                    const float sine = 0.5f + 0.5f * static_cast<float>(std::sin(l.lfoPhase));
                    l.currentValue = juce::jlimit(0.0f, 1.0f, l.baseValue + (sine - 0.5f) * l.depth);
                    break;
                }

                case RandomWalk:
                {
                    if (std::abs(l.randomWalkCurrent - l.randomWalkTarget) < 0.02f)
                    {
                        l.randomWalkTarget = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                    }

                    const float rate = static_cast<float>(0.5 * std::abs(l.speedMultiplier) * blockTimeSec);
                    l.randomWalkCurrent += (l.randomWalkTarget - l.randomWalkCurrent) * juce::jlimit(0.001f, 0.2f, rate);

                    l.currentValue = juce::jlimit(0.0f, 1.0f, l.baseValue + (l.randomWalkCurrent - 0.5f) * l.depth);
                    break;
                }
            }
        }
    }

private:
    std::array<Lane, NumLanes> lanes;
    double currentSampleRate { 44100.0 };
    double timeAccumulator { 0.0 };
};