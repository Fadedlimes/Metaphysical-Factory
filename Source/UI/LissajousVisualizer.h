#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <array>
#include <cmath>

#include "S116Widgets.h"
#include "S116Theme.h"
#include "SkinManager.h"

// Forward declaration of the processor
class MetaphysicalFactoryAudioProcessor;

class LissajousVisualizer : public S116::SkinnedComponent,
                            public juce::Timer
{
public:
    static constexpr int BufferSize = 512;
    static constexpr int HistoryFrames = 24; // 24-pass deep persistence ghosting
    static constexpr int QuadDelaySamples = 20;

    explicit LissajousVisualizer (MetaphysicalFactoryAudioProcessor& proc)
        : processor (proc)
    {
        pointsSumAB.assign (BufferSize, 0.0f);
        pointsRingAB.assign (BufferSize, 0.0f);
        pointsOutL.assign (BufferSize, 0.0f);
        pointsOutR.assign (BufferSize, 0.0f);
        quadRingBuffer.assign (64, 0.0f);

        for (auto& p : historyPaths)
            p.preallocateSpace (BufferSize * 3);

        auto configureScopeDial = [] (juce::Slider& slider)
        {
            slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setMouseDragSensitivity (140);
            slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                        juce::MathConstants<float>::pi * 2.8f, true);
            slider.getProperties().set ("body", 42);
            slider.getProperties().set ("halo", 11);
            slider.setInterceptsMouseClicks (true, false);
        };

        // The editor's SilverLookAndFeel is inherited; no local OLED LAF.
        configureScopeDial (scopeSumDial);
        configureScopeDial (scopeRingDial);
        addAndMakeVisible (scopeSumDial);
        addAndMakeVisible (scopeRingDial);

        startTimerHz (60);
    }

    ~LissajousVisualizer() override
    {
        stopTimer();
    }

    void resized() override
    {
        auto b = getLocalBounds().toFloat();
        leftWing  = b.removeFromLeft (72.0f);
        rightWing = b.removeFromRight (72.0f);
        centerArea = b;
        scopeRadius = juce::jmin (centerArea.getWidth(), centerArea.getHeight()) * 0.44f;

        const float dialY = (float) getHeight() - 68.0f;
        scopeSumDial.setBounds (4, (int) dialY, 64, 64);
        scopeRingDial.setBounds (getWidth() - 68, (int) dialY, 64, 64);

        leftDialLabel  = { leftWing.getX(), dialY - 13.0f, 72.0f, 13.0f };
        rightDialLabel = { rightWing.getX(), dialY - 13.0f, 72.0f, 13.0f };

        // No separate LEVEL/PHASE band: the meter uses the full remaining wing height.
        leftMeterArea  = { leftWing.getX() + 6.0f, 2.0f, 60.0f,
                           juce::jmax (8.0f, leftDialLabel.getY() - 4.0f) };
        rightMeterArea = { rightWing.getX() + 6.0f, 2.0f, 60.0f,
                           juce::jmax (8.0f, rightDialLabel.getY() - 4.0f) };

        leftMeterRepaintArea  = leftMeterArea.expanded (1.0f).toNearestInt();
        rightMeterRepaintArea = rightMeterArea.expanded (1.0f).toNearestInt();
        traceRepaintArea = centerArea.expanded (4.0f).toNearestInt();

        for (auto& p : historyPaths)
            p.clear();
        historyWriteIndex = 0;
    }

    void timerCallback() override
    {
        const int readCount = readScopeDataFromProcessor (pointsSumAB.data(),
                                                          pointsRingAB.data(),
                                                          pointsOutL.data(),
                                                          pointsOutR.data(),
                                                          BufferSize);

        if (readCount > 1)
        {
            numActivePoints = readCount;
            updateMeters (readCount);
            update3DScopePath();

            // Only the trace and the two meter fields are dynamic.
            if (! traceRepaintArea.isEmpty()) repaint (traceRepaintArea);
            if (! leftMeterRepaintArea.isEmpty()) repaint (leftMeterRepaintArea);
            if (! rightMeterRepaintArea.isEmpty()) repaint (rightMeterRepaintArea);
        }
    }

    juce::Slider& getScopeSumDial()  { return scopeSumDial; }
    juce::Slider& getScopeRingDial() { return scopeRingDial; }

private:
    static constexpr float dashPattern[2] = { 2.0f, 4.0f };

    MetaphysicalFactoryAudioProcessor& processor;

    juce::Slider scopeSumDial;
    juce::Slider scopeRingDial;

    juce::Rectangle<float> leftWing, rightWing, centerArea;
    juce::Rectangle<float> leftMeterArea, rightMeterArea;
    juce::Rectangle<float> leftDialLabel, rightDialLabel;
    juce::Rectangle<int> traceRepaintArea, leftMeterRepaintArea, rightMeterRepaintArea;
    float scopeRadius { 0.0f };

    std::vector<float> pointsSumAB;
    std::vector<float> pointsRingAB;
    std::vector<float> pointsOutL;
    std::vector<float> pointsOutR;
    int numActivePoints { 0 };

    struct ScopeDcBlocker
    {
        float x1 { 0.0f };
        float y1 { 0.0f };

        inline float process (float x)
        {
            const float y = x - x1 + (0.995f * y1);
            x1 = x;
            y1 = y;
            return y;
        }
    };

    ScopeDcBlocker dcBlockerA;
    ScopeDcBlocker dcBlockerB;

    std::vector<float> quadRingBuffer;
    int quadWriteIdx { 0 };

    float smoothRmsLevel { 0.0f };
    float smoothPeakLevel { 0.0f };
    float smoothCorrelation { 1.0f };
    float smoothScopeMax { 0.35f };

    std::array<juce::Path, HistoryFrames> historyPaths;
    int historyWriteIndex { 0 };

    void paintSkin (juce::Graphics& g, juce::Rectangle<float> b, const S116::Theme& t) override
    {
        // Bezel and smoked screen.
        g.setGradientFill (juce::ColourGradient (t.scopeBezelTop, 0.0f, 0.0f,
                                                 t.scopeBezelBottom, 0.0f, b.getHeight(), false));
        g.fillRoundedRectangle (b, 5.0f);

        const auto screen = b.reduced (2.0f);
        g.setColour (t.chassisBottom);
        g.fillRoundedRectangle (screen, 4.0f);

        // Wing tint and separators.
        g.setColour (t.panelTop.withAlpha (0.45f));
        g.fillRect (leftWing);
        g.fillRect (rightWing);
        g.setColour (t.rule);
        g.drawLine (72.0f, 2.0f, 72.0f, b.getBottom() - 2.0f, 1.0f);
        g.drawLine (b.getRight() - 72.0f, 2.0f, b.getRight() - 72.0f, b.getBottom() - 2.0f, 1.0f);

        // Static reticle.
        const float cx = centerArea.getCentreX();
        const float cy = centerArea.getCentreY();
        g.setColour (t.rule);
        g.drawEllipse (cx - scopeRadius, cy - scopeRadius, scopeRadius * 2.0f, scopeRadius * 2.0f, 1.0f);
        g.drawEllipse (cx - scopeRadius * 0.5f, cy - scopeRadius * 0.5f,
                       scopeRadius, scopeRadius, 0.8f);
        g.drawDashedLine (juce::Line<float> (cx - scopeRadius, cy, cx + scopeRadius, cy),
                          dashPattern, 2, 0.8f);
        g.drawDashedLine (juce::Line<float> (cx, cy - scopeRadius, cx, cy + scopeRadius),
                          dashPattern, 2, 0.8f);

        // Static meter frames (the 11 px A+B/A x B labels are above the dials).
        g.setColour (t.chassisBottom);
        g.fillRoundedRectangle (leftMeterArea, 2.0f);
        g.fillRoundedRectangle (rightMeterArea, 2.0f);
        g.setColour (t.knobRing.withAlpha (0.65f));
        g.drawRoundedRectangle (leftMeterArea, 2.0f, 0.8f);
        g.drawRoundedRectangle (rightMeterArea, 2.0f, 0.8f);

        // 11 px mono labels above the scope dials.
        g.setFont (S116::skin().mono (11.0f));
        g.setColour (t.caption);
        g.drawText ("A+B", leftDialLabel, juce::Justification::centred, false);
        g.drawText ("A x B", rightDialLabel, juce::Justification::centred, false);

        g.setColour (t.scopeBezelTop.brighter (0.12f));
        g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.2f);
    }

    void paintDynamic (juce::Graphics& g) override
    {
        const auto& t = S116::skin().theme();

        drawLevelMeter (g, leftMeterArea, t);
        drawCorrelationMeter (g, rightMeterArea, t);

        if (centerArea.isEmpty())
            return;

        for (int step = HistoryFrames - 1; step >= 0; --step)
        {
            const size_t frameIdx = static_cast<size_t> ((historyWriteIndex - step + HistoryFrames) % HistoryFrames);
            const auto& path = historyPaths[frameIdx];

            if (path.isEmpty())
                continue;

            const float ageNormalized = 1.0f - (static_cast<float> (step) / static_cast<float> (HistoryFrames));

            if (step == 0)
            {
                g.setColour (t.ledOn.withAlpha (0.32f));
                g.strokePath (path, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
                g.setColour (t.ledOn.withAlpha (0.85f));
                g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
                g.setColour (t.heading.withAlpha (0.95f));
                g.strokePath (path, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            }
            else
            {
                const float trailAlpha = std::pow (ageNormalized, 2.0f);
                const auto trailColour = t.ledOff.interpolatedWith (t.ledOn, ageNormalized)
                                            .withAlpha (trailAlpha * 0.42f);
                const float strokeWidth = juce::jmax (0.8f, 1.6f * ageNormalized);
                g.setColour (trailColour);
                g.strokePath (path, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            }
        }
    }

    void updateMeters (int count)
    {
        float sumSquares = 0.0f;
        float peak = 0.0001f;
        float dotProduct = 0.0f;
        float energyL = 0.0f;
        float energyR = 0.0f;

        for (size_t i = 0; i < static_cast<size_t> (count); ++i)
        {
            const float l = pointsOutL[i];
            const float r = pointsOutR[i];
            const float mono = (l + r) * 0.5f;

            sumSquares += mono * mono;
            peak = juce::jmax (peak, std::abs (l), std::abs (r));

            dotProduct += l * r;
            energyL += l * l;
            energyR += r * r;
        }

        const float rms = std::sqrt (sumSquares / static_cast<float> (count));
        smoothRmsLevel  = (smoothRmsLevel * 0.85f)  + (rms * 0.15f);
        smoothPeakLevel = (smoothPeakLevel * 0.90f) + (peak * 0.10f);

        const float denom = std::sqrt (energyL * energyR) + 0.00001f;
        const float correlation = juce::jlimit (-1.0f, 1.0f, dotProduct / denom);
        smoothCorrelation = (smoothCorrelation * 0.90f) + (correlation * 0.10f);
    }

    void update3DScopePath()
    {
        if (centerArea.isEmpty())
            return;

        const float centerX = centerArea.getCentreX();
        const float centerY = centerArea.getCentreY();
        const float radius = scopeRadius;

        const float gainA = static_cast<float> (scopeSumDial.getValue());
        const float gainB = static_cast<float> (scopeRingDial.getValue());

        float frameVectorMax = 0.001f;
        for (size_t i = 0; i < static_cast<size_t> (numActivePoints); ++i)
        {
            const float sA = pointsSumAB[i] * gainA;
            const float sB = pointsRingAB[i] * gainB;
            const float vecMag = std::sqrt ((sA * sA) + (sB * sB));
            if (vecMag > frameVectorMax)
                frameVectorMax = vecMag;
        }

        smoothScopeMax = juce::jmax (frameVectorMax, (smoothScopeMax * 0.90f) + (frameVectorMax * 0.10f));
        const float agcScale = radius * (0.68f / juce::jmax (0.08f, smoothScopeMax));

        historyWriteIndex = (historyWriteIndex + 1) % HistoryFrames;
        auto& activePath = historyPaths[static_cast<size_t> (historyWriteIndex)];
        activePath.clear();

        bool firstPoint = true;
        for (size_t i = 0; i < static_cast<size_t> (numActivePoints); ++i)
        {
            const float sigSum  = dcBlockerA.process (pointsSumAB[i]) * gainA;
            const float sigRing = dcBlockerB.process (pointsRingAB[i]) * gainB;

            quadRingBuffer[static_cast<size_t> (quadWriteIdx)] = sigRing;
            const int readIdx = (quadWriteIdx - QuadDelaySamples + 64) % 64;
            const float quadRing = quadRingBuffer[static_cast<size_t> (readIdx)];
            quadWriteIdx = (quadWriteIdx + 1) % 64;

            const float xRaw = sigSum + (quadRing * 0.75f);
            const float yRaw = (sigRing * 0.85f) - (sigSum * 0.5f);

            const float posX = centerX + (xRaw * agcScale);
            const float posY = centerY - (yRaw * agcScale);

            if (firstPoint)
            {
                activePath.startNewSubPath (posX, posY);
                firstPoint = false;
            }
            else
            {
                activePath.lineTo (posX, posY);
            }
        }
    }

    void drawLevelMeter (juce::Graphics& g, juce::Rectangle<float> area, const S116::Theme& t)
    {
        if (area.getHeight() < 4.0f) return;

        const int numSegments = 14;
        const float gap = 1.0f;
        const auto inner = area.reduced (2.0f, 2.0f);
        const float segHeight = (inner.getHeight() - (numSegments - 1) * gap)
                                    / static_cast<float> (numSegments);
        if (segHeight < 2.0f) return;

        const float normLevel = juce::jlimit (0.0f, 1.0f, smoothPeakLevel * 1.25f);
        const int activeCount = static_cast<int> (std::round (normLevel * static_cast<float> (numSegments)));
        float y = inner.getBottom();

        for (int i = 0; i < numSegments; ++i)
        {
            const auto segRect = juce::Rectangle<float> (inner.getX(), y - segHeight,
                                                         inner.getWidth(), segHeight);
            y -= segHeight + gap;

            if (i < activeCount)
            {
                if (i >= numSegments - 2)
                    g.setColour (t.heading);
                else if (i >= numSegments - 5)
                    g.setColour (t.ledOn);
                else
                    g.setColour (t.ledOn.withMultipliedBrightness (0.72f));

                g.fillRect (segRect);
            }
            else
            {
                g.setColour (t.ledOff.withAlpha (0.55f));
                g.fillRect (segRect);
            }
        }
    }

    void drawCorrelationMeter (juce::Graphics& g, juce::Rectangle<float> area, const S116::Theme& t)
    {
        if (area.getHeight() < 4.0f) return;

        const float midY = area.getCentreY();
        g.setColour (t.rule);
        g.drawHorizontalLine (static_cast<int> (midY), area.getX() + 1.0f, area.getRight() - 1.0f);

        const float normCorr = (smoothCorrelation + 1.0f) * 0.5f;
        const float pipY = (area.getBottom() - 4.0f) - (normCorr * (area.getHeight() - 8.0f));

        g.setColour (t.ledOn.withAlpha (0.35f));
        g.fillEllipse (area.getCentreX() - 5.0f, pipY - 3.0f, 10.0f, 6.0f);
        g.setColour (t.ledOn);
        g.fillEllipse (area.getCentreX() - 3.5f, pipY - 2.0f, 7.0f, 4.0f);
        g.setColour (t.heading);
        g.fillEllipse (area.getCentreX() - 1.5f, pipY - 1.0f, 3.0f, 2.0f);
    }

    int readScopeDataFromProcessor (float* destSumAB, float* destRingAB,
                                    float* destOutL, float* destOutR, int samplesToRead);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LissajousVisualizer)
};
