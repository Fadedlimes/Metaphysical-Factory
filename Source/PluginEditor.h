#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "UI/MetaphysicalLookAndFeel.h"
#include "UI/LissajousVisualizer.h"
#include "UI/SampleWaveformDisplay.h"
#include "UI/MotionSlider.h"

class MetaphysicalFactoryAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MetaphysicalFactoryAudioProcessorEditor(MetaphysicalFactoryAudioProcessor&);
    ~MetaphysicalFactoryAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MetaphysicalFactoryAudioProcessor& audioProcessor;
    MetaphysicalLookAndFeel customLookAndFeel;

    // Dual Telemetry Screens
    LissajousVisualizer visualizer;
    SampleWaveformDisplay waveformDisplay;

    // Attachments for Visualizer's Dual Flat OLED Dials (A+B and A x B)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scopeSumAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scopeRingAttachment;

    // Master Header Potentiometers
    juce::Slider masterPitchDial;
    juce::Slider masterVolDial;
    juce::Label masterPitchLabel;
    juce::Label masterVolLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterPitchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterVolAttachment;

    // Master Tone EQ Potentiometers (+/- 12 dB)
    juce::Slider bassDial;
    juce::Label bassLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassAttachment;

    juce::Slider trebleDial;
    juce::Label trebleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trebleAttachment;

    // Analog Overdrive Distortion Control
    juce::Slider driveDial;
    juce::Label driveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;

    // Hybrid Continuous LP/HP Filter Morphs
    juce::Slider morphADial;
    juce::Label morphALabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAAttachment;

    juce::Slider morphBDial;
    juce::Label morphBLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphBAttachment;

    // Per-Oscillator Fine-Tune Trimmers (-100 to +100 Cents)
    std::array<juce::Slider, 6> fineDialsA;
    std::array<juce::Slider, 6> fineDialsB;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> fineTuneAttachments;

    // Persistent File Chooser (Servicing the hybrid OLED badge)
    std::unique_ptr<juce::FileChooser> fileChooser;

    // 32 Performance Motion Faders
    std::vector<std::unique_ptr<MotionSlider>> motionSliders;

    // Helper method to instantiate and bind motion sliders
    void addMotionFader(const juce::String& paramID, const juce::String& label, int laneIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetaphysicalFactoryAudioProcessorEditor)
};