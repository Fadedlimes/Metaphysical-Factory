#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/MotionEngine.h"
#include "MetaphysicalLookAndFeel.h"

class MotionSlider : public juce::Component,
                     public juce::Timer
{
public:
    MotionSlider(juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& paramID,
                 const juce::String& labelText,
                 MotionEngine& engine,
                 int laneIdx)
        : motionEngine(engine),
          laneIndex(laneIdx)
    {
        // 1. Parameter Silkscreen Label (Erica Synths Stencil)
        titleLabel.setText(labelText, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(MetaphysicalLookAndFeel::cSilkscreenWhite));
        addAndMakeVisible(titleLabel);

        // 2. Vertical Hardware Fader
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);

        // 3. APVTS Host Automation Attachment
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider);

        // 4. Round Silicone Tactile REC Button
        recButton.setButtonText("REC");
        recButton.setClickingTogglesState(true);
        recButton.onClick = [this]()
        {
            motionEngine.setLaneArmed(laneIndex, recButton.getToggleState());
        };
        addAndMakeVisible(recButton);

        // 5. Playback Speed Selector Badge (1x, 2x, -1x Reverse, 0.5x Half-Time)
        speedButton.setButtonText("1x");
        speedButton.onClick = [this]()
        {
            currentSpeedStep = (currentSpeedStep + 1) % 4;
            switch (currentSpeedStep)
            {
                case 0: motionEngine.setLaneSpeed(laneIndex, 1.0f);  speedButton.setButtonText("1x"); break;
                case 1: motionEngine.setLaneSpeed(laneIndex, 2.0f);  speedButton.setButtonText("2x"); break;
                case 2: motionEngine.setLaneSpeed(laneIndex, -1.0f); speedButton.setButtonText("-1x"); break;
                case 3: motionEngine.setLaneSpeed(laneIndex, 0.5f);  speedButton.setButtonText(".5x"); break;
            }
        };
        addAndMakeVisible(speedButton);

        // 6. User Mouse Gesture Handlers
        slider.onDragStart = [this]()
        {
            if (motionEngine.isLaneArmed(laneIndex))
            {
                // Armed: start a fresh 100 Hz gesture recording pass
                motionEngine.startRecording(laneIndex);
            }
            else if (motionEngine.isLaneLooping(laneIndex))
            {
                // Manual touch takeover: if slider is looping and grabbed without REC, return to static manual control
                motionEngine.setLaneMode(laneIndex, MotionEngine::Static);
            }
        };

        slider.onDragEnd = [this]()
        {
            if (motionEngine.isLaneRecording(laneIndex))
            {
                // Conclude recording pass; automatically engages Looping if gesture is valid
                motionEngine.stopRecording(laneIndex);
                recButton.setToggleState(false, juce::dontSendNotification);
            }
        };

        // Smooth 60 FPS animation loop
        startTimerHz(60);
    }

    ~MotionSlider() override
    {
        stopTimer();
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);

        titleLabel.setBounds(bounds.removeFromTop(18));

        auto bottomBar = bounds.removeFromBottom(22);
        recButton.setBounds(bottomBar.removeFromLeft(bottomBar.getWidth() / 2).reduced(1));
        speedButton.setBounds(bottomBar.reduced(1));

        bounds.removeFromBottom(4);
        slider.setBounds(bounds);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        // Erica Synths fine wireframe cell border
        g.setColour(juce::Colour(0x1fffffff));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // Playhead Scrubber LED Indicator
        if (motionEngine.isLaneLooping(laneIndex))
        {
            const float progress = motionEngine.getPlayheadProgress(laneIndex);
            if (progress >= 0.0f)
            {
                const auto sliderArea = slider.getBounds().toFloat();
                const float indicatorY = (sliderArea.getBottom() - 10.0f) - (progress * (sliderArea.getHeight() - 20.0f));

                // Radiant Hot Pink Playhead Pip
                g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink).withAlpha(0.35f));
                g.fillEllipse(sliderArea.getRight() - 6.0f, indicatorY - 4.0f, 8.0f, 8.0f); // Soft outer glow

                g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
                g.fillEllipse(sliderArea.getRight() - 5.0f, indicatorY - 3.0f, 6.0f, 6.0f); // Bright core
            }
        }
    }

    void timerCallback() override
    {
        // Smoothly animate the physical slider handle during gesture loop playback
        if (!slider.isMouseButtonDown() && motionEngine.isLaneLooping(laneIndex))
        {
            const float normVal = motionEngine.getLaneValue(laneIndex);
            const double physicalVal = slider.proportionOfLengthToValue(static_cast<double>(normVal));
            slider.setValue(physicalVal, juce::dontSendNotification);
        }

        // Keep REC button toggle state synchronized with recording engine
        if (recButton.getToggleState() != motionEngine.isLaneArmed(laneIndex) && !motionEngine.isLaneRecording(laneIndex))
        {
            recButton.setToggleState(motionEngine.isLaneArmed(laneIndex), juce::dontSendNotification);
        }

        repaint();
    }

private:
    MotionEngine& motionEngine;
    const int laneIndex;
    int currentSpeedStep { 0 };

    juce::Label titleLabel;
    juce::Slider slider;
    juce::TextButton recButton;
    juce::TextButton speedButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotionSlider)
};