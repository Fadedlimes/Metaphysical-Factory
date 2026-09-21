#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cmath>
#include <functional>
#include "MetaphysicalLookAndFeel.h"

// Forward declaration of processor
class MetaphysicalFactoryAudioProcessor;

class SampleWaveformDisplay : public juce::Component,
                              public juce::Timer
{
public:
    std::function<void()> onLoadSampleClicked;

    explicit SampleWaveformDisplay(MetaphysicalFactoryAudioProcessor& proc)
        : processor(proc)
    {
        startTimerHz(30); // 30 FPS telemetry and playhead refresh
    }

    ~SampleWaveformDisplay() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Recessed OLED Screen Window (Deep Smoked Glass)
        g.setColour(juce::Colour(0xff08090c));
        g.fillRoundedRectangle(bounds, 5.0f);

        // Subtle inner bezel shadow
        g.setColour(juce::Colour(0x60000000));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

        // 2. Interactive Hybrid Sample Name & Load Badge
        const auto badgeArea = sampleBadgeRect.reduced(1.0f);

        // Badge Background (Illuminates when hovered)
        if (isBadgeHovered)
        {
            g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink).withAlpha(0.20f));
            g.fillRoundedRectangle(badgeArea, 3.0f);
            g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
            g.drawRoundedRectangle(badgeArea, 3.0f, 1.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff12141a));
            g.fillRoundedRectangle(badgeArea, 3.0f);
            g.setColour(juce::Colour(0xff22252e));
            g.drawRoundedRectangle(badgeArea, 3.0f, 0.8f);
        }

        // Query active sample file name from the looper engine
        const juce::String sampleName = getLoadedSampleTitle();
        g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::bold));
        g.setColour(isBadgeHovered ? juce::Colours::white : juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
        g.drawText("SMP: " + sampleName, badgeArea.reduced(4.0f, 0.0f), juce::Justification::centredLeft, true);

        // Right Telemetry Status
        const auto statusArea = juce::Rectangle<float>(sampleBadgeRect.getRight() + 4.0f, 2.0f,
                                                      bounds.getRight() - sampleBadgeRect.getRight() - 10.0f, 16.0f);
        g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.5f, juce::Font::bold));
        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cSilkscreenMuted));
        g.drawText(isBadgeHovered ? "CLICK TO LOAD" : "BIDIRECTIONAL", statusArea, juce::Justification::right, false);

        // Thin divider line under telemetry header
        g.setColour(juce::Colour(0x22ffffff));
        g.fillRect(bounds.getX() + 4.0f, 20.0f, bounds.getWidth() - 8.0f, 1.0f);

        // 3. Waveform Display Area
        auto waveBounds = bounds;
        waveBounds.removeFromTop(22.0f);
        const auto waveArea = waveBounds.reduced(6.0f, 4.0f);
        const float midY = waveArea.getCentreY();
        const float halfH = waveArea.getHeight() * 0.45f;

        // Faint Center Baseline
        g.setColour(juce::Colour(0x18ffffff));
        g.fillRect(waveArea.getX(), midY, waveArea.getWidth(), 1.0f);

        // Retrieve waveform peaks from looper
        refreshWaveformPeaks(static_cast<int>(waveArea.getWidth()));

        if (!cachedMinPeaks.empty() && cachedMinPeaks.size() == cachedMaxPeaks.size())
        {
            const float numBins = static_cast<float>(cachedMinPeaks.size());
            const float binWidth = waveArea.getWidth() / numBins;

            juce::Path wavePath;
            wavePath.preallocateSpace(static_cast<int>(numBins) * 4);

            // Build top envelope
            wavePath.startNewSubPath(waveArea.getX(), midY);
            for (size_t i = 0; i < cachedMinPeaks.size(); ++i)
            {
                const float x = waveArea.getX() + (static_cast<float>(i) * binWidth);
                const float y = midY - (cachedMaxPeaks[i] * halfH);
                wavePath.lineTo(x, y);
            }

            // Build bottom envelope (reverse pass)
            for (int i = static_cast<int>(cachedMinPeaks.size()) - 1; i >= 0; --i)
            {
                const float x = waveArea.getX() + (static_cast<float>(i) * binWidth);
                const float y = midY - (cachedMinPeaks[static_cast<size_t>(i)] * halfH);
                wavePath.lineTo(x, y);
            }
            wavePath.closeSubPath();

            // Waveform Semi-Transparent Pink Fill
            g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink).withAlpha(0.20f));
            g.fillPath(wavePath);

            // Waveform Sharp Vector Outline
            g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink).withAlpha(0.85f));
            g.strokePath(wavePath, juce::PathStrokeType(1.0f));
        }

        // 4. Live Playhead Needle (Glowing White/Pink)
        const float currentPlayhead = getPlayheadNormalized();
        if (currentPlayhead >= 0.0f && currentPlayhead <= 1.0f)
        {
            const float playheadX = waveArea.getX() + (currentPlayhead * waveArea.getWidth());

            // Soft Playhead Glow Halo
            g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink).withAlpha(0.35f));
            g.fillRect(playheadX - 2.0f, waveArea.getY(), 5.0f, waveArea.getHeight());

            // Crisp White Center Line
            g.setColour(juce::Colours::white);
            g.fillRect(playheadX - 0.5f, waveArea.getY(), 1.0f, waveArea.getHeight());

            // Top and Bottom Arrow Pointers
            juce::Path topNeedle, bottomNeedle;
            topNeedle.addTriangle(playheadX - 3.0f, waveArea.getY(),
                                  playheadX + 3.0f, waveArea.getY(),
                                  playheadX, waveArea.getY() + 4.0f);
            bottomNeedle.addTriangle(playheadX - 3.0f, waveArea.getBottom(),
                                     playheadX + 3.0f, waveArea.getBottom(),
                                     playheadX, waveArea.getBottom() - 4.0f);

            g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
            g.fillPath(topNeedle);
            g.fillPath(bottomNeedle);
        }

        // 5. Outer Bezel Frame
        g.setColour(juce::Colour(0xff2d313a));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.2f);
    }

    void resized() override
    {
        cachedNumBins = -1; // Invalidate cached peaks on resize
        sampleBadgeRect = juce::Rectangle<float>(6.0f, 2.0f, static_cast<float>(getWidth()) * 0.62f, 16.0f);
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const bool wasHovered = isBadgeHovered;
        isBadgeHovered = sampleBadgeRect.contains(e.position);
        if (wasHovered != isBadgeHovered)
            repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (isBadgeHovered)
        {
            isBadgeHovered = false;
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (sampleBadgeRect.contains(e.position))
        {
            if (onLoadSampleClicked != nullptr)
                onLoadSampleClicked();
        }
    }

    juce::MouseCursor getMouseCursor() override
    {
        return isBadgeHovered ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor;
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    MetaphysicalFactoryAudioProcessor& processor;

    std::vector<float> cachedMinPeaks;
    std::vector<float> cachedMaxPeaks;
    int cachedNumBins { -1 };

    juce::Rectangle<float> sampleBadgeRect;
    bool isBadgeHovered { false };

    // Helper methods implemented in PluginEditor.cpp
    float getPlayheadNormalized();
    void refreshWaveformPeaks(int numBins);
    juce::String getLoadedSampleTitle();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleWaveformDisplay)
};