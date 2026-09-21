#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <array>
#include <cmath>
#include "MetaphysicalLookAndFeel.h"

// Forward declaration of the processor
class MetaphysicalFactoryAudioProcessor;

class LissajousVisualizer : public juce::Component,
                            public juce::Timer
{
public:
    static constexpr int BufferSize = 512;
    static constexpr int HistoryFrames = 24; // 24-pass deep persistence ghosting
    static constexpr int QuadDelaySamples = 20;

    // Flat Vector LookAndFeel specifically for embedded OLED screen knobs
    struct FlatOledKnobLookAndFeel : public juce::LookAndFeel_V4
    {
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                              juce::Slider& /*slider*/) override
        {
            const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                       static_cast<float>(width), static_cast<float>(height)).reduced(1.5f);
            const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const float centreX = bounds.getCentreX();
            const float centreY = bounds.getCentreY();

            // Flat OLED Well (No 3D bevels, matches electronic glass)
            g.setColour(juce::Colour(0xff0d0e12));
            g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
            g.setColour(juce::Colour(0xff1f222a));
            g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.0f);

            // Flat Vibrant Neon Pink Arc
            const float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
            juce::Path activeArc;
            activeArc.addCentredArc(centreX, centreY, radius - 2.5f, radius - 2.5f,
                                    0.0f, rotaryStartAngle, currentAngle, true);
            g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
            g.strokePath(activeArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Sharp White Vector Pointer Needle
            const float needleLen = radius * 0.68f;
            const float needleX = centreX + std::sin(currentAngle) * needleLen;
            const float needleY = centreY - std::cos(currentAngle) * needleLen;
            g.setColour(juce::Colours::white);
            g.drawLine(centreX, centreY, needleX, needleY, 1.4f);

            // Center LED Hub
            g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
            g.fillEllipse(centreX - 2.0f, centreY - 2.0f, 4.0f, 4.0f);
        }
    };

    explicit LissajousVisualizer(MetaphysicalFactoryAudioProcessor& proc)
        : processor(proc)
    {
        pointsSumAB.assign(BufferSize, 0.0f);
        pointsRingAB.assign(BufferSize, 0.0f);
        pointsOutL.assign(BufferSize, 0.0f);
        pointsOutR.assign(BufferSize, 0.0f);
        quadRingBuffer.assign(64, 0.0f);

        auto configureOledDial = [this](juce::Slider& slider)
        {
            slider.setLookAndFeel(&flatOledLnf);
            slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setMouseDragSensitivity(140);
            slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
            addAndMakeVisible(slider);
        };

        // Dual Flat OLED Knobs (Left: A+B, Right: A x B)
        configureOledDial(scopeSumDial);
        configureOledDial(scopeRingDial);

        // Run UI scope and meters at 60 FPS
        startTimerHz(60);
    }

    ~LissajousVisualizer() override
    {
        stopTimer();
        scopeSumDial.setLookAndFeel(nullptr);
        scopeRingDial.setLookAndFeel(nullptr);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Position flat OLED dials at the bottom corners of the display
        scopeSumDial.setBounds(7, bounds.getBottom() - 36, 26, 26);
        scopeRingDial.setBounds(bounds.getRight() - 33, bounds.getBottom() - 36, 26, 26);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Recessed Hardware OLED Screen Window (True Deep Black)
        g.setColour(juce::Colour(0xff060709));
        g.fillRoundedRectangle(bounds, 5.0f);

        // Ambient edge vignette
        juce::ColourGradient innerShadow(juce::Colour(0x00000000), bounds.getCentreX(), bounds.getCentreY(),
                                         juce::Colour(0xee000000), bounds.getCentreX(), bounds.getBottom(), true);
        g.setGradientFill(innerShadow);
        g.fillRoundedRectangle(bounds, 5.0f);

        // 2. Flanking Left Wing: LEVEL Meter & Flat A+B Dial Label
        auto leftWing = bounds.removeFromLeft(40.0f);
        auto leftMeterArea = leftWing.removeFromTop(leftWing.getHeight() - 38.0f).reduced(6.0f, 8.0f);
        drawLevelMeter(g, leftMeterArea);

        // Flat "A+B" OLED Label
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
        g.drawText("A+B", leftWing.removeFromBottom(12.0f), juce::Justification::centred, false);

        // 3. Flanking Right Wing: PHASE Meter & Flat A x B Dial Label
        auto rightWing = bounds.removeFromRight(40.0f);
        auto rightMeterArea = rightWing.removeFromTop(rightWing.getHeight() - 38.0f).reduced(6.0f, 8.0f);
        drawCorrelationMeter(g, rightMeterArea);

        // Flat "A x B" OLED Label
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
        g.drawText("A x B", rightWing.removeFromBottom(12.0f), juce::Justification::centred, false);

        // 4. Center 3D Lissajous Scope Area (Completely Unobstructed)
        const auto centerArea = bounds;
        const float centerX = centerArea.getCentreX();
        const float centerY = centerArea.getCentreY();
        const float radius  = juce::jmin(centerArea.getWidth(), centerArea.getHeight()) * 0.44f;

        // Circular Reticle & Faint Polar Crosshairs
        g.setColour(juce::Colour(0x12ffffff));
        g.drawEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f, 1.0f);
        g.drawEllipse(centerX - radius * 0.5f, centerY - radius * 0.5f, radius, radius, 0.8f);

        g.drawDashedLine(juce::Line<float>(centerX - radius, centerY, centerX + radius, centerY),
                         dashPattern, 2, 0.8f);
        g.drawDashedLine(juce::Line<float>(centerX, centerY - radius, centerX, centerY + radius),
                         dashPattern, 2, 0.8f);

        // 5. Render 24-Pass Decaying Phosphor Persistence Trails (From Oldest to Newest)
        for (int step = HistoryFrames - 1; step >= 0; --step)
        {
            const size_t frameIdx = static_cast<size_t>((historyWriteIndex - step + HistoryFrames) % HistoryFrames);
            const auto& path = historyPaths[frameIdx];

            if (path.isEmpty())
                continue;

            const float ageNormalized = 1.0f - (static_cast<float>(step) / static_cast<float>(HistoryFrames));

            if (step == 0) // Newest Trace: Sharp High-Intensity Core with Radiant Bloom
            {
                g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink).withAlpha(0.32f));
                g.strokePath(path, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink).withAlpha(0.85f));
                g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                g.setColour(juce::Colour(0xffffffff).withAlpha(0.95f));
                g.strokePath(path, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else // Ghosting Phosphor Decay Passes
            {
                const float trailAlpha = std::pow(ageNormalized, 2.0f);

                juce::Colour trailColour = juce::Colour(0xff3a051c)
                    .interpolatedWith(juce::Colour(MetaphysicalLookAndFeel::cNeonPink), ageNormalized)
                    .withAlpha(trailAlpha * 0.42f);

                const float strokeWidth = juce::jmax(0.8f, 1.6f * ageNormalized);

                g.setColour(trailColour);
                g.strokePath(path, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // 6. Industrial Outer Screen Bezel
        g.setColour(juce::Colour(0xff22242b));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.2f);
    }

    void timerCallback() override
    {
        const int readCount = readScopeDataFromProcessor(pointsSumAB.data(),
                                                         pointsRingAB.data(),
                                                         pointsOutL.data(),
                                                         pointsOutR.data(),
                                                         BufferSize);

        if (readCount > 1)
        {
            numActivePoints = readCount;
            updateMeters(readCount);
            update3DScopePath();
            repaint();
        }
    }

    juce::Slider& getScopeSumDial()  { return scopeSumDial; }
    juce::Slider& getScopeRingDial() { return scopeRingDial; }

private:
    static constexpr float dashPattern[2] = { 2.0f, 4.0f };

    MetaphysicalFactoryAudioProcessor& processor;
    FlatOledKnobLookAndFeel flatOledLnf;

    juce::Slider scopeSumDial;
    juce::Slider scopeRingDial;

    std::vector<float> pointsSumAB;
    std::vector<float> pointsRingAB;
    std::vector<float> pointsOutL;
    std::vector<float> pointsOutR;
    int numActivePoints { 0 };

    struct ScopeDcBlocker
    {
        float x1 { 0.0f };
        float y1 { 0.0f };

        inline float process(float x)
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

    void updateMeters(int count)
    {
        float sumSquares = 0.0f;
        float peak = 0.0001f;
        float dotProduct = 0.0f;
        float energyL = 0.0f;
        float energyR = 0.0f;

        for (size_t i = 0; i < static_cast<size_t>(count); ++i)
        {
            const float l = pointsOutL[i];
            const float r = pointsOutR[i];
            const float mono = (l + r) * 0.5f;

            sumSquares += mono * mono;
            peak = juce::jmax(peak, std::abs(l), std::abs(r));

            dotProduct += l * r;
            energyL += l * l;
            energyR += r * r;
        }

        const float rms = std::sqrt(sumSquares / static_cast<float>(count));
        smoothRmsLevel  = (smoothRmsLevel * 0.85f)  + (rms * 0.15f);
        smoothPeakLevel = (smoothPeakLevel * 0.90f) + (peak * 0.10f);

        const float denom = std::sqrt(energyL * energyR) + 0.00001f;
        const float correlation = juce::jlimit(-1.0f, 1.0f, dotProduct / denom);
        smoothCorrelation = (smoothCorrelation * 0.90f) + (correlation * 0.10f);
    }

    void update3DScopePath()
    {
        auto bounds = getLocalBounds().toFloat();
        bounds.removeFromLeft(40.0f);
        bounds.removeFromRight(40.0f);

        const float centerX = bounds.getCentreX();
        const float centerY = bounds.getCentreY();
        const float radius  = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.44f;

        const float gainA = static_cast<float>(scopeSumDial.getValue());
        const float gainB = static_cast<float>(scopeRingDial.getValue());

        // 1. Calculate true combined vector magnitude across the whole frame
        float frameVectorMax = 0.001f;
        for (size_t i = 0; i < static_cast<size_t>(numActivePoints); ++i)
        {
            const float sA = pointsSumAB[i] * gainA;
            const float sB = pointsRingAB[i] * gainB;
            const float vecMag = std::sqrt((sA * sA) + (sB * sB));
            if (vecMag > frameVectorMax)
                frameVectorMax = vecMag;
        }

        // Smoothly track peak magnitude
        smoothScopeMax = juce::jmax(frameVectorMax, (smoothScopeMax * 0.90f) + (frameVectorMax * 0.10f));

        // Scale to comfortably occupy 68% of the radius (eliminates wall slamming and boundary clipping)
        const float agcScale = radius * (0.68f / juce::jmax(0.08f, smoothScopeMax));

        // Advance history ring buffer
        historyWriteIndex = (historyWriteIndex + 1) % HistoryFrames;
        auto& activePath = historyPaths[static_cast<size_t>(historyWriteIndex)];
        activePath.clear();
        activePath.preallocateSpace(numActivePoints * 3);

        bool firstPoint = true;
        for (size_t i = 0; i < static_cast<size_t>(numActivePoints); ++i)
        {
            const float sigSum  = dcBlockerA.process(pointsSumAB[i]) * gainA;
            const float sigRing = dcBlockerB.process(pointsRingAB[i]) * gainB;

            // Quadrature phase rotation
            quadRingBuffer[static_cast<size_t>(quadWriteIdx)] = sigRing;
            const int readIdx = (quadWriteIdx - QuadDelaySamples + 64) % 64;
            const float quadRing = quadRingBuffer[static_cast<size_t>(readIdx)];
            quadWriteIdx = (quadWriteIdx + 1) % 64;

            // Mathematical 3D Hyperboloid projection
            const float xRaw = sigSum + (quadRing * 0.75f);
            const float yRaw = (sigRing * 0.85f) - (sigSum * 0.5f);

            const float posX = centerX + (xRaw * agcScale);
            const float posY = centerY - (yRaw * agcScale);

            if (firstPoint)
            {
                activePath.startNewSubPath(posX, posY);
                firstPoint = false;
            }
            else
            {
                activePath.lineTo(posX, posY);
            }
        }
    }

    void drawLevelMeter(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setFont(juce::FontOptions(7.5f, juce::Font::bold));
        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cSilkscreenMuted));
        g.drawText("LEVEL", area.removeFromTop(12.0f), juce::Justification::centred, false);

        g.setColour(juce::Colour(0xff101114));
        g.fillRoundedRectangle(area, 2.0f);
        g.setColour(juce::Colour(0xff22242a));
        g.drawRoundedRectangle(area, 2.0f, 0.8f);

        const int numSegments = 14;
        const float segHeight = (area.getHeight() - 4.0f) / static_cast<float>(numSegments);
        const float normLevel = juce::jlimit(0.0f, 1.0f, smoothPeakLevel * 1.25f);
        const int activeCount = static_cast<int>(std::round(normLevel * static_cast<float>(numSegments)));

        for (int i = 0; i < numSegments; ++i)
        {
            const float segY = (area.getBottom() - 2.0f) - (static_cast<float>(i + 1) * segHeight);
            const auto segRect = juce::Rectangle<float>(area.getX() + 2.0f, segY + 1.0f, area.getWidth() - 4.0f, segHeight - 1.5f);

            if (i < activeCount)
            {
                if (i >= numSegments - 2)
                    g.setColour(juce::Colours::white);
                else if (i >= numSegments - 5)
                    g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
                else
                    g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPinkDim).brighter(0.4f));

                g.fillRect(segRect);
            }
            else
            {
                g.setColour(juce::Colour(0x18ffffff));
                g.fillRect(segRect);
            }
        }
    }

    void drawCorrelationMeter(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setFont(juce::FontOptions(7.5f, juce::Font::bold));
        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cSilkscreenMuted));
        g.drawText("PHASE", area.removeFromTop(12.0f), juce::Justification::centred, false);

        g.setColour(juce::Colour(0xff101114));
        g.fillRoundedRectangle(area, 2.0f);
        g.setColour(juce::Colour(0xff22242a));
        g.drawRoundedRectangle(area, 2.0f, 0.8f);

        const float midY = area.getCentreY();
        g.setColour(juce::Colour(0x35ffffff));
        g.drawHorizontalLine(static_cast<int>(midY), area.getX() + 1.0f, area.getRight() - 1.0f);

        const float normCorr = (smoothCorrelation + 1.0f) * 0.5f;
        const float pipY = (area.getBottom() - 4.0f) - (normCorr * (area.getHeight() - 8.0f));

        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink).withAlpha(0.35f));
        g.fillEllipse(area.getCentreX() - 5.0f, pipY - 3.0f, 10.0f, 6.0f);

        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
        g.fillEllipse(area.getCentreX() - 3.5f, pipY - 2.0f, 7.0f, 4.0f);

        g.setColour(juce::Colours::white);
        g.fillEllipse(area.getCentreX() - 1.5f, pipY - 1.0f, 3.0f, 2.0f);
    }

    int readScopeDataFromProcessor(float* destSumAB, float* destRingAB, float* destOutL, float* destOutR, int samplesToRead);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LissajousVisualizer)
};