#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "SkinManager.h"
#include "SilverLookAndFeel.h"
#include "S116Theme.h"
#include "S116Widgets.h"
#include "S116Layout.h"
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
    void layout();
    void runDebugBoundsReport();
    void collectBounds (juce::Component& parent, const juce::String& name,
                        bool verbose, bool& ok) const;

    MetaphysicalFactoryAudioProcessor& audioProcessor;
    SilverLookAndFeel customLookAndFeel;

    // Compact S-116 section panels
    S116::SectionPanel bankAPanel { "BANK A: VOICES & FINE TRIM" };
    S116::SectionPanel telemetryPanel { "CENTRAL TELEMETRY DISPLAYS" };
    S116::SectionPanel resoSamplerPanel { "TEXTURE SAMPLER & RESOCHORD" };
    S116::SectionPanel bankBPanel { "BANK B: VOICES & FINE TRIM" };
    S116::SectionPanel filtersPanel { "TPT FILTERS & OVERDRIVE" };
    S116::SectionPanel effectsPanel { "DIFFUSION DELAY & SPACE REVERB" };

    // A thin divider between sampler and Resochord faders.
    struct Divider : public juce::Component
    {
        Divider() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
    };
    Divider samplerResoDivider;

    // Single-source layout rectangles (computed by layout())
    juce::Rectangle<int> headerBounds, bodyBounds, logoPlateBounds;
    juce::Rectangle<int> samplerResoDividerBounds;
    std::array<juce::Rectangle<int>, 6> panelBounds {};
    std::array<juce::Rectangle<int>, 6> panelContentBounds {};
    std::array<juce::Rectangle<int>, 4> headerKnobCells {};
    std::array<juce::Rectangle<int>, 3> filterKnobCells {};
    std::array<std::array<juce::Rectangle<int>, 6>, 2> trimCells {};
    std::array<std::array<juce::Rectangle<int>, 6>, 2> bankFaderCells {};
    std::array<juce::Rectangle<int>, 6> filterFaderCells {};
    std::array<juce::Rectangle<int>, 6> resoSamplerFaderCells {};
    std::array<juce::Rectangle<int>, 8> effectsFaderCells {};

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