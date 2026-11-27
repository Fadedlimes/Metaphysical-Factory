#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cmath>
#include <functional>

#include "S116Widgets.h"
#include "S116Theme.h"
#include "SkinManager.h"

// Forward declaration of processor
class MetaphysicalFactoryAudioProcessor;

class SampleWaveformDisplay : public S116::SkinnedComponent,
                              public juce::Timer
{
public:
    std::function<void()> onLoadSampleClicked;

    explicit SampleWaveformDisplay (MetaphysicalFactoryAudioProcessor& proc)
        : processor (proc)
    {
        topNeedlePath.preallocateSpace (8);
        bottomNeedlePath.preallocateSpace (8);
        startTimerHz (30);
    }

    ~SampleWaveformDisplay() override
    {
        stopTimer();
    }

    void resized() override
    {
        cachedNumBins = -1;
        sampleTitle = getLoadedSampleTitle();
        badgeText = "SMP: " + sampleTitle;

        sampleBadgeRect = { 6.0f, 2.0f, (float) getWidth() * 0.62f, 16.0f };
        statusArea = { sampleBadgeRect.getRight() + 4.0f, 2.0f,
                       (float) getWidth() - sampleBadgeRect.getRight() - 10.0f, 16.0f };

        auto waveBounds = getLocalBounds().toFloat();
        waveBounds.removeFromTop (22.0f);
        waveArea = waveBounds.reduced (6.0f, 4.0f);

        badgeRepaintArea = sampleBadgeRect.expanded (2.0f).toNearestInt();
        lastPlayheadX = -1;
        lastPlayheadRepaint = {};

        invalidateCache();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool wasHovered = isBadgeHovered;
        isBadgeHovered = sampleBadgeRect.contains (e.position);
        if (wasHovered != isBadgeHovered)
            repaint (badgeRepaintArea);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (isBadgeHovered || isBadgePressed)
        {
            isBadgeHovered = false;
            isBadgePressed = false;
            repaint (badgeRepaintArea);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (sampleBadgeRect.contains (e.position))
        {
            isBadgePressed = true;
            repaint (badgeRepaintArea);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool overBadge = sampleBadgeRect.contains (e.position);
        const bool wasPressed = isBadgePressed;
        isBadgePressed = false;
        isBadgeHovered = overBadge;
        repaint (badgeRepaintArea);

        if (overBadge && wasPressed && onLoadSampleClicked != nullptr)
            onLoadSampleClicked();
    }

    juce::MouseCursor getMouseCursor() override
    {
        return isBadgeHovered ? juce::MouseCursor::PointingHandCursor
                              : juce::MouseCursor::NormalCursor;
    }

    void timerCallback() override
    {
        const float currentPlayhead = getPlayheadNormalized();

        if (currentPlayhead >= 0.0f && currentPlayhead <= 1.0f && ! waveArea.isEmpty())
        {
            const int x = juce::roundToInt (waveArea.getX() + currentPlayhead * waveArea.getWidth());
            const int top = juce::roundToInt (waveArea.getY()) - 2;
            const int height = juce::roundToInt (waveArea.getHeight()) + 4;
            juce::Rectangle<int> strip (x - 6, top, 12, height);

            if (lastPlayheadX >= 0)
            {
                const int left  = juce::jmin (strip.getX(), lastPlayheadRepaint.getX());
                const int right = juce::jmax (strip.getRight(), lastPlayheadRepaint.getRight());
                repaint (juce::Rectangle<int> (left, top, right - left, height));
            }
            else
            {
                repaint (strip);
            }

            lastPlayheadX = x;
            lastPlayheadRepaint = strip;
        }
        else if (lastPlayheadX >= 0)
        {
            repaint (lastPlayheadRepaint);
            lastPlayheadX = -1;
            lastPlayheadRepaint = {};
        }
    }

private:
    MetaphysicalFactoryAudioProcessor& processor;

    std::vector<float> cachedMinPeaks;
    std::vector<float> cachedMaxPeaks;
    int cachedNumBins { -1 };

    juce::Rectangle<float> sampleBadgeRect, statusArea, waveArea;
    juce::Rectangle<int> badgeRepaintArea, lastPlayheadRepaint;
    int lastPlayheadX { -1 };
    bool isBadgeHovered { false };
    bool isBadgePressed { false };
    juce::String sampleTitle, badgeText;
    juce::Path topNeedlePath, bottomNeedlePath;

    void paintSkin (juce::Graphics& g, juce::Rectangle<float> b, const S116::Theme& t) override
    {
        // Bezel and smoked screen.
        g.setGradientFill (juce::ColourGradient (t.scopeBezelTop, 0.0f, 0.0f,
                                                 t.scopeBezelBottom, 0.0f, b.getHeight(), false));
        g.fillRoundedRectangle (b, 5.0f);

        const auto screen = b.reduced (2.0f);
        g.setColour (t.chassisBottom);
        g.fillRoundedRectangle (screen, 4.0f);

        // Idle SMP badge.
        g.setGradientFill (juce::ColourGradient (t.btnTop, sampleBadgeRect.getX(), sampleBadgeRect.getY(),
                                                 t.btnBottom, sampleBadgeRect.getX(), sampleBadgeRect.getBottom(), false));
        g.fillRoundedRectangle (sampleBadgeRect, 3.0f);
        g.setColour (t.btnRing);
        g.drawRoundedRectangle (sampleBadgeRect, 3.0f, 0.8f);

        g.setFont (S116::skin().mono (10.0f));
        g.setColour (t.logoAccent);
        g.drawText (badgeText, sampleBadgeRect.reduced (4.0f, 0.0f),
                    juce::Justification::centredLeft, true);

        // BIDIRECTIONAL caption, 10 px mono on kit caption colour.
        g.setFont (S116::skin().mono (10.0f));
        g.setColour (t.caption);
        g.drawText ("BIDIRECTIONAL", statusArea, juce::Justification::right, false);

        g.setColour (t.rule);
        g.fillRect (b.getX() + 4.0f, 20.0f, b.getWidth() - 8.0f, 1.0f);

        if (waveArea.isEmpty())
            return;

        const float midY = waveArea.getCentreY();
        const float halfH = waveArea.getHeight() * 0.45f;

        g.setColour (t.rule);
        g.fillRect (waveArea.getX(), midY, waveArea.getWidth(), 1.0f);

        refreshWaveformPeaks (static_cast<int> (waveArea.getWidth()));

        if (! cachedMinPeaks.empty() && cachedMinPeaks.size() == cachedMaxPeaks.size())
        {
            const float numBins = static_cast<float> (cachedMinPeaks.size());
            const float binWidth = waveArea.getWidth() / numBins;

            juce::Path wavePath;
            wavePath.preallocateSpace (static_cast<int> (numBins) * 4);

            wavePath.startNewSubPath (waveArea.getX(), midY);
            for (size_t i = 0; i < cachedMinPeaks.size(); ++i)
            {
                const float x = waveArea.getX() + (static_cast<float> (i) * binWidth);
                const float y = midY - (cachedMaxPeaks[i] * halfH);
                wavePath.lineTo (x, y);
            }

            for (int i = static_cast<int> (cachedMinPeaks.size()) - 1; i >= 0; --i)
            {
                const float x = waveArea.getX() + (static_cast<float> (i) * binWidth);
                const float y = midY - (cachedMinPeaks[static_cast<size_t> (i)] * halfH);
                wavePath.lineTo (x, y);
            }
            wavePath.closeSubPath();

            g.setColour (t.ledOn.withAlpha (0.18f));
            g.fillPath (wavePath);
            g.setColour (t.ledOn.withAlpha (0.85f));
            g.strokePath (wavePath, juce::PathStrokeType (1.0f));
        }

        g.setColour (t.scopeBezelTop.brighter (0.12f));
        g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.2f);
    }

    void paintDynamic (juce::Graphics& g) override
    {
        const auto& t = S116::skin().theme();

        // Idle badge is in the skin; hover/pressed covers it with a visible state.
        const auto clip = g.getClipBounds().toFloat();
        if ((isBadgePressed || isBadgeHovered) && clip.intersects (sampleBadgeRect))
        {
            const auto badge = isBadgePressed ? sampleBadgeRect.translated (0.0f, 1.0f)
                                              : sampleBadgeRect;
            g.setColour (isBadgePressed ? t.btnBottom : t.btnTop);
            g.fillRoundedRectangle (badge, 3.0f);
            g.setColour (t.ledOn);
            g.drawRoundedRectangle (badge, 3.0f, isBadgePressed ? 1.4f : 1.0f);
            g.setFont (S116::skin().mono (10.0f));
            g.setColour (isBadgePressed ? t.ledReadout : t.heading);
            g.drawText (badgeText, badge.reduced (4.0f, 0.0f),
                        juce::Justification::centredLeft, true);
        }

        const float currentPlayhead = getPlayheadNormalized();
        if (currentPlayhead < 0.0f || currentPlayhead > 1.0f || waveArea.isEmpty())
            return;

        const float playheadX = waveArea.getX() + (currentPlayhead * waveArea.getWidth());

        g.setColour (t.ledOn.withAlpha (0.35f));
        g.fillRect (playheadX - 2.0f, waveArea.getY(), 5.0f, waveArea.getHeight());

        g.setColour (t.heading);
        g.fillRect (playheadX - 0.5f, waveArea.getY(), 1.0f, waveArea.getHeight());

        topNeedlePath.clear();
        topNeedlePath.addTriangle (playheadX - 3.0f, waveArea.getY(),
                                  playheadX + 3.0f, waveArea.getY(),
                                  playheadX, waveArea.getY() + 4.0f);
        bottomNeedlePath.clear();
        bottomNeedlePath.addTriangle (playheadX - 3.0f, waveArea.getBottom(),
                                     playheadX + 3.0f, waveArea.getBottom(),
                                     playheadX, waveArea.getBottom() - 4.0f);

        g.setColour (t.ledOn);
        g.fillPath (topNeedlePath);
        g.fillPath (bottomNeedlePath);
    }

    // Helper methods implemented in PluginEditor.cpp
    float getPlayheadNormalized();
    void refreshWaveformPeaks (int numBins);
    juce::String getLoadedSampleTitle();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleWaveformDisplay)
};
