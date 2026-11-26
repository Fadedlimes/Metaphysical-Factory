#include "PluginEditor.h"
#include "BinaryData.h"
#include <iostream>

// --- Bridge implementations for decoupled visualizers ---

int LissajousVisualizer::readScopeDataFromProcessor(float* destSumAB, float* destRingAB, float* destOutL, float* destOutR, int samplesToRead)
{
    return processor.readScopeData(destSumAB, destRingAB, destOutL, destOutR, samplesToRead);
}

float SampleWaveformDisplay::getPlayheadNormalized()
{
    return processor.getSampleLooper().getNormalizedPlayhead();
}

void SampleWaveformDisplay::refreshWaveformPeaks(int numBins)
{
    if (cachedNumBins != numBins)
    {
        processor.getSampleLooper().getWaveformPeaks(cachedMinPeaks, cachedMaxPeaks, numBins);
        cachedNumBins = numBins;
    }
}

juce::String SampleWaveformDisplay::getLoadedSampleTitle()
{
    const auto name = processor.getSampleLooper().getLoadedSampleName();
    return name.isNotEmpty() ? name.toUpperCase() : "TEXTURE LOOPER";
}

// --- Main Editor Implementation ---

MetaphysicalFactoryAudioProcessorEditor::MetaphysicalFactoryAudioProcessorEditor(MetaphysicalFactoryAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      visualizer(p),
      waveformDisplay(p)
{
    setLookAndFeel(&customLookAndFeel);

   #if S116_HAS_FONTS
    S116::skin().setTypefaces (
        juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf,
                                                 BinaryData::BarlowCondensedBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf,
                                                 BinaryData::ShareTechMonoRegular_ttfSize));
   #endif

    S116::skin().setTheme (S116::ThemeId::MetaphysicalBlack);
    S116::skin().setLedColour (juce::Colour (0xffff2a85));
    customLookAndFeel.applyTheme();

    // Compact panels (24 / 4 / 4) with the current panel titles and subtitles.
    bankAPanel.setCompactPadding (true);
    bankAPanel.setSubtitle ("SINE / TRI / BIPOLAR / NOISE");
    telemetryPanel.setCompactPadding (true);
    telemetryPanel.setSubtitle ("3D TORUS / LUFS / CORR");
    resoSamplerPanel.setCompactPadding (true);
    resoSamplerPanel.setSubtitle ("PARALLEL COMBS");
    bankBPanel.setCompactPadding (true);
    bankBPanel.setSubtitle ("DUAL NOISE / SINE / TRI / PULSE");
    filtersPanel.setCompactPadding (true);
    filtersPanel.setSubtitle ("HYBRID MORPH / RING MOD");
    effectsPanel.setCompactPadding (true);
    effectsPanel.setSubtitle ("FDN 4x4 / SPIN");

    bankAPanel.setName ("BANK A PANEL");
    telemetryPanel.setName ("TELEMETRY PANEL");
    resoSamplerPanel.setName ("SAMPLER RESO PANEL");
    bankBPanel.setName ("BANK B PANEL");
    filtersPanel.setName ("FILTERS PANEL");
    effectsPanel.setName ("EFFECTS PANEL");
    addAndMakeVisible (bankAPanel);
    addAndMakeVisible (telemetryPanel);
    addAndMakeVisible (resoSamplerPanel);
    addAndMakeVisible (bankBPanel);
    addAndMakeVisible (filtersPanel);
    addAndMakeVisible (effectsPanel);

    samplerResoDivider.setName ("SAMPLER RESO DIVIDER");
    resoSamplerPanel.addAndMakeVisible (samplerResoDivider);

    // Helper to configure responsive, professional vertical-drag rotary dials
    auto configureRotaryDial = [](juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag); // Standard vertical drag
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setMouseDragSensitivity (140);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f, true);
        slider.setInterceptsMouseClicks (true, false);
        slider.getProperties().set ("body", 42);
        slider.getProperties().set ("halo", 11);
    };

    // 1. Bind Visualizer's Dual Flat OLED Dials (A+B Level and A x B Level)
    configureRotaryDial(visualizer.getScopeSumDial());
    configureRotaryDial(visualizer.getScopeRingDial());

    scopeSumAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "scope_level_sum", visualizer.getScopeSumDial());
    scopeRingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "scope_level_ring", visualizer.getScopeRingDial());

    // 2. Master Tone EQ Potentiometers (+/- 12 dB Shelving)
    configureRotaryDial(bassDial);
    addAndMakeVisible(bassDial);
    bassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "master_eq_bass", bassDial);

    bassLabel.setText("BASS", juce::dontSendNotification);
    S116::layout::styleCaption(bassLabel);
    bassDial.setName ("BASS KNOB"); bassLabel.setName ("BASS LABEL");
    addAndMakeVisible(bassLabel);

    configureRotaryDial(trebleDial);
    addAndMakeVisible(trebleDial);
    trebleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "master_eq_treble", trebleDial);

    trebleLabel.setText("TREBLE", juce::dontSendNotification);
    S116::layout::styleCaption(trebleLabel);
    trebleDial.setName ("TREBLE KNOB"); trebleLabel.setName ("TREBLE LABEL");
    addAndMakeVisible(trebleLabel);

    // 3. Master Hardware Potentiometers with Real-Time Interactive Note Readout
    configureRotaryDial(masterPitchDial);
    addAndMakeVisible(masterPitchDial);
    masterPitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "master_pitch", masterPitchDial);

    S116::layout::styleAccent(masterPitchLabel);
    masterPitchDial.setName ("ROOT KNOB"); masterPitchLabel.setName ("ROOT LABEL");
    addAndMakeVisible(masterPitchLabel);

    // Lambda callback: updates note label instantaneously whenever the knob is turned
    auto updatePitchLabel = [this]()
    {
        const int note = static_cast<int>(std::round(masterPitchDial.getValue()));
        masterPitchLabel.setText("ROOT: " + juce::MidiMessage::getMidiNoteName(note, true, true, 3), juce::dontSendNotification);
    };
    masterPitchDial.onValueChange = updatePitchLabel;
    updatePitchLabel();

    configureRotaryDial(masterVolDial);
    addAndMakeVisible(masterVolDial);
    masterVolAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "master_volume", masterVolDial);

    masterVolLabel.setText("OUTPUT", juce::dontSendNotification);
    S116::layout::styleCaption(masterVolLabel);
    masterVolDial.setName ("OUTPUT KNOB"); masterVolLabel.setName ("OUTPUT LABEL");
    addAndMakeVisible(masterVolLabel);

    // 4. Overdrive Distortion Potentiometer
    configureRotaryDial(driveDial);
    filtersPanel.addAndMakeVisible(driveDial);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "distortion_drive", driveDial);

    driveLabel.setText("OVERDRIVE", juce::dontSendNotification);
    S116::layout::styleAccent(driveLabel);
    driveDial.setName ("OVERDRIVE KNOB"); driveLabel.setName ("OVERDRIVE LABEL");
    filtersPanel.addAndMakeVisible(driveLabel);

    // 5. Hybrid Continuous LP/HP Filter Morphs
    configureRotaryDial(morphADial);
    filtersPanel.addAndMakeVisible(morphADial);
    morphAAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "filter_morph_a", morphADial);

    morphALabel.setText("MORPH A", juce::dontSendNotification);
    S116::layout::styleCaption(morphALabel);
    morphADial.setName ("MORPH A KNOB"); morphALabel.setName ("MORPH A LABEL");
    filtersPanel.addAndMakeVisible(morphALabel);

    configureRotaryDial(morphBDial);
    filtersPanel.addAndMakeVisible(morphBDial);
    morphBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "filter_morph_b", morphBDial);

    morphBLabel.setText("MORPH B", juce::dontSendNotification);
    S116::layout::styleCaption(morphBLabel);
    morphBDial.setName ("MORPH B KNOB"); morphBLabel.setName ("MORPH B LABEL");
    filtersPanel.addAndMakeVisible(morphBLabel);

    // 6. Individual Voice Fine-Tune Micro-Knobs (-100 to +100 Cents)
    for (int i = 0; i < 6; ++i)
    {
        // Bank A Fine-Tune
        auto& dialA = fineDialsA[static_cast<size_t>(i)];
        configureRotaryDial(dialA);
        dialA.setName ("FINE A" + juce::String (i + 1));
        bankAPanel.addAndMakeVisible(dialA);
        fineTuneAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getAPVTS(), "oscA_" + juce::String(i) + "_fine", dialA));

        // Bank B Fine-Tune
        auto& dialB = fineDialsB[static_cast<size_t>(i)];
        configureRotaryDial(dialB);
        dialB.setName ("FINE B" + juce::String (i + 1));
        bankBPanel.addAndMakeVisible(dialB);
        fineTuneAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getAPVTS(), "oscB_" + juce::String(i) + "_fine", dialB));
    }

    // 7. Interactive Hybrid OLED Sample Loader Hook
    waveformDisplay.onLoadSampleClicked = [this]()
    {
        const juce::File initialDir = audioProcessor.getLastSampleDirectory();
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select Audio Sample for Looper...",
            initialDir.isDirectory() ? initialDir : juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.aif;*.aiff;*.flac");

        const auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                audioProcessor.setLastSampleDirectory(file.getParentDirectory());
                audioProcessor.getSampleLooper().loadFile(file, audioProcessor.getFormatManager());
                waveformDisplay.resized();
            }
        });
    };

    // 8. Dock Visual Displays
    visualizer.setName ("LISSAJOUS");
    waveformDisplay.setName ("WAVEFORM");
    telemetryPanel.addAndMakeVisible(visualizer);
    telemetryPanel.addAndMakeVisible(waveformDisplay);

    // 9. Populate All 32 Performance Motion Faders with Descriptive, Decoupled Labels
    // Bank A: 3x Sine, 1x Triangle, 1x Bipolar Pulse, 1x LP Filtered Noise
    addMotionFader("oscA_0_level", "SIN 1",    0);
    addMotionFader("oscA_1_level", "SIN 2",    1);
    addMotionFader("oscA_2_level", "SIN 3",    2);
    addMotionFader("oscA_3_level", "TRI",      3);
    addMotionFader("oscA_4_level", "PULSE",    4);
    addMotionFader("oscA_5_level", "LP NOISE", 5);

    // Bank B: 1x Dual-Mode Noise, 2x Sine, 1x Triangle, 1x Pulse, 1x Bipolar Pulse
    addMotionFader("oscB_0_level", "DUAL NOISE", 6);
    addMotionFader("oscB_1_level", "SIN 1",      7);
    addMotionFader("oscB_2_level", "SIN 2",      8);
    addMotionFader("oscB_3_level", "TRI",        9);
    addMotionFader("oscB_4_level", "PULSE",     10);
    addMotionFader("oscB_5_level", "BI-PLS",    11);

    // Filters & Ring Mod (Lanes 12 - 17)
    addMotionFader("filter_cutoff_a", "CUT A", 12);
    addMotionFader("filter_res_a",    "RES A", 13);
    addMotionFader("filter_cutoff_b", "CUT B", 14);
    addMotionFader("filter_res_b",    "RES B", 15);
    addMotionFader("ring_mod_mix",   "RING",  16);
    addMotionFader("balance_ab",     "BAL",   17);

    // Sample Looper (Lanes 18 - 20) - Decoupled Sampler Controls
    addMotionFader("looper_gain",  "SMP GAIN", 18);
    addMotionFader("looper_speed", "SMP SPD",  19);
    addMotionFader("looper_pitch", "SMP TUNE", 20);

    // Resochord (Lanes 21 - 23) - Dedicated Comb Tuning + Feedback Decay + Dry/Wet Mix
    addMotionFader("reso_tune",  "RESO TUNE", 21); // Independent Comb Tuning (+/- 24 semitones)
    addMotionFader("reso_decay", "RESO DCY",  22);
    addMotionFader("reso_mix",   "RESO MIX",  23);

    // Spin Delay (Lanes 24 - 27)
    addMotionFader("spin_time",     "TIME", 24);
    addMotionFader("spin_rate",     "RATE", 25);
    addMotionFader("spin_feedback", "FDBK", 26);
    addMotionFader("spin_mix",      "SPIN", 27);

    // Space Reverb (Lanes 28 - 31)
    addMotionFader("reverb_size",  "SIZE", 28);
    addMotionFader("reverb_decay", "TAIL", 29);
    addMotionFader("reverb_damp",  "ABSORB", 30);
    addMotionFader("reverb_mix",   "SPACE", 31);

    // 10. Strict Initialization Order: setSize() at bottom guarantees populated components
    setResizable(true, true);
    setResizeLimits(1280, 736, 2560, 1600);

    bool debugBounds = false;
   #if defined(MF_DEBUG_BOUNDS)
    debugBounds = true;
   #else
    debugBounds = juce::SystemStats::getEnvironmentVariable("MF_BOUNDS_LOG", "0") == "1";
   #endif
    if (debugBounds)
    {
        juce::Component::SafePointer<MetaphysicalFactoryAudioProcessorEditor> safeThis(this);
        juce::Timer::callAfterDelay(200, [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->runDebugBoundsReport();
        });
    }

    setSize(1280, 820);
}

MetaphysicalFactoryAudioProcessorEditor::~MetaphysicalFactoryAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void MetaphysicalFactoryAudioProcessorEditor::addMotionFader(const juce::String& paramID, const juce::String& label, int laneIndex)
{
    auto fader = std::make_unique<MotionSlider>(
        audioProcessor.getAPVTS(), paramID, label, audioProcessor.getMotionEngine(), laneIndex);

    S116::SectionPanel* parent = &bankAPanel;
    if (laneIndex >= 6 && laneIndex <= 11)       parent = &bankBPanel;
    else if (laneIndex >= 12 && laneIndex <= 17) parent = &filtersPanel;
    else if (laneIndex >= 18 && laneIndex <= 23) parent = &resoSamplerPanel;
    else if (laneIndex >= 24)                    parent = &effectsPanel;

    fader->setName ("MOTION LANE " + juce::String (laneIndex));
    parent->addAndMakeVisible(*fader);
    motionSliders.push_back(std::move(fader));
}

void MetaphysicalFactoryAudioProcessorEditor::Divider::paint (juce::Graphics& g)
{
    const auto& t = S116::skin().theme();
    g.setColour (t.rule);
    g.fillRect (getLocalBounds());
}

void MetaphysicalFactoryAudioProcessorEditor::layout()
{
    auto area = getLocalBounds();
    headerBounds = area.removeFromTop(84);

    bodyBounds = area;
    bodyBounds.removeFromTop(12);
    bodyBounds.removeFromBottom(16);
    bodyBounds.removeFromLeft(12);
    bodyBounds.removeFromRight(12);

    const int gap = 10;
    const int upperH = static_cast<int>((bodyBounds.getHeight() - gap) * 0.485f);
    const int lowerH = bodyBounds.getHeight() - gap - upperH;
    const int contentW = bodyBounds.getWidth() - gap * 2;

    // Minimum widths from the approved Bank/telemetry/filter/effects footprints.
    const int col1W = static_cast<int>(contentW * (426.0f / 1236.0f));
    const int col2W = static_cast<int>(contentW * (342.0f / 1236.0f));
    const int col3W = contentW - col1W - col2W;

    const juce::Rectangle<int> upperRow (bodyBounds.getX(), bodyBounds.getY(), bodyBounds.getWidth(), upperH);
    const juce::Rectangle<int> lowerRow (bodyBounds.getX(), bodyBounds.getY() + upperH + gap,
                                         bodyBounds.getWidth(), lowerH);

    panelBounds[0] = { upperRow.getX(), upperRow.getY(), col1W, upperH };
    panelBounds[1] = { upperRow.getX() + col1W + gap, upperRow.getY(), col2W, upperH };
    panelBounds[2] = { upperRow.getX() + col1W + gap + col2W + gap, upperRow.getY(), col3W, upperH };
    panelBounds[3] = { lowerRow.getX(), lowerRow.getY(), col1W, lowerH };
    panelBounds[4] = { lowerRow.getX() + col1W + gap, lowerRow.getY(), col2W, lowerH };
    panelBounds[5] = { lowerRow.getX() + col1W + gap + col2W + gap, lowerRow.getY(), col3W, lowerH };

    auto contentOf = [] (juce::Rectangle<int> r)
    {
        return r.withTrimmedTop (24).reduced (4, 0).withTrimmedBottom (4);
    };
    for (size_t i = 0; i < panelBounds.size(); ++i)
        panelContentBounds[i] = contentOf ({ 0, 0, panelBounds[i].getWidth(), panelBounds[i].getHeight() });

    // Header: 300 px master block on the right, four 75 px cells.
    auto headerRight = headerBounds;
    headerRight.removeFromRight (12);
    auto masterRight = headerRight.removeFromRight (300);
    const int headerCellW = masterRight.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
        headerKnobCells[static_cast<size_t>(i)] = { masterRight.getX() + i * headerCellW,
                                                    headerBounds.getY(), headerCellW, headerBounds.getHeight() };

    logoPlateBounds = { 28, headerBounds.getY() + 20, 275, 44 };

    // Bank trims and faders share the same 68 px cells at 1280.
    for (int bank = 0; bank < 2; ++bank)
    {
        const auto content = panelContentBounds[bank == 0 ? 0 : 3];
        const int fgap = 2;
        const int cellW = (content.getWidth() - 5 * fgap) / 6;
        for (int i = 0; i < 6; ++i)
        {
            const int x = content.getX() + i * (cellW + fgap);
            trimCells[static_cast<size_t>(bank)][static_cast<size_t>(i)] = { x, content.getY(), cellW, 64 };
            bankFaderCells[static_cast<size_t>(bank)][static_cast<size_t>(i)] =
                { x, content.getY() + 64, cellW, content.getHeight() - 64 };
        }
    }

    // Filters: 3 x 64 px knobs in an 80 px row, six faders below.
    {
        const auto content = panelContentBounds[4];
        const int cellW = content.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
            filterKnobCells[static_cast<size_t>(i)] = { content.getX() + i * cellW, content.getY(), cellW, 80 };

        const int fgap = 2;
        const int faderW = (content.getWidth() - 5 * fgap) / 6;
        for (int i = 0; i < 6; ++i)
            filterFaderCells[static_cast<size_t>(i)] = { content.getX() + i * (faderW + fgap),
                                                          content.getY() + 80, faderW,
                                                          content.getHeight() - 80 };
    }

    // Sampler / Resochord: six cells across the content, no extra horizontal padding.
    {
        const auto content = panelContentBounds[2];
        const int fgap = 2;
        const int cellW = (content.getWidth() - 5 * fgap) / 6;
        for (int i = 0; i < 6; ++i)
            resoSamplerFaderCells[static_cast<size_t>(i)] = { content.getX() + i * (cellW + fgap),
                                                               content.getY(), cellW, content.getHeight() };
    }

    // Delay / Reverb: 55 px cells with 2 px gaps at 1280 (fits inside 460 px).
    {
        const auto content = panelContentBounds[5];
        const int fgap = 2;
        const int cellW = (content.getWidth() - 7 * fgap) / 8;
        const int total = 8 * cellW + 7 * fgap;
        const int pad = (content.getWidth() - total) / 2;
        for (int i = 0; i < 8; ++i)
            effectsFaderCells[static_cast<size_t>(i)] = { content.getX() + pad + i * (cellW + fgap),
                                                           content.getY(), cellW, content.getHeight() };
    }

    // Divider between the 3 sampler and 3 Resochord cells (inside the 2 px gap).
    samplerResoDividerBounds = { resoSamplerFaderCells[3].getRight(),
                                 panelContentBounds[2].getY(), 1,
                                 panelContentBounds[2].getHeight() };
}

void MetaphysicalFactoryAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (panelBounds[0].isEmpty())
        layout();

    const auto& t = S116::skin().theme();

    // Matte chassis.
    g.setGradientFill (juce::ColourGradient (t.chassisTop, 0.0f, 0.0f,
                                             t.chassisBottom, 0.0f, (float) getHeight(), false));
    g.fillRect (getLocalBounds());

    // Subtle under-panel shadow, using the single-source panel rectangles.
    for (const auto& p : panelBounds)
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (p.toFloat().expanded (1.0f), 7.0f);
    }

    // Header separation rule.
    g.setColour (t.rule);
    g.fillRect (12, headerBounds.getBottom() - 1, getWidth() - 24, 1);

    // Corner screws.
    auto drawScrew = [&] (float x, float y)
    {
        g.setColour (t.knobBodyTop.darker (0.55f));
        g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colour (0xff050608));
        g.fillEllipse (x - 2.0f, y - 2.0f, 4.0f, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f, 0.8f);
    };
    drawScrew (8.0f, 8.0f);
    drawScrew ((float) getWidth() - 8.0f, 8.0f);
    drawScrew (8.0f, (float) getHeight() - 8.0f);
    drawScrew ((float) getWidth() - 8.0f, (float) getHeight() - 8.0f);

    // Custom METAPHYSICAL FACTORY nameplate (not HeaderBar), restyled with kit fonts/tokens.
    const auto logoRect = logoPlateBounds.toFloat();
    juce::Path nameplatePath;
    nameplatePath.startNewSubPath (logoRect.getX(), logoRect.getY() + 2.0f);
    nameplatePath.lineTo (logoRect.getRight() - 12.0f, logoRect.getY() + 2.0f);
    nameplatePath.lineTo (logoRect.getRight() - 2.0f, logoRect.getBottom() - 2.0f);
    nameplatePath.lineTo (logoRect.getX() + 10.0f, logoRect.getBottom() - 2.0f);
    nameplatePath.closeSubPath();

    g.setGradientFill (juce::ColourGradient (t.panelTop, logoRect.getX(), logoRect.getY(),
                                             t.panelBottom, logoRect.getX(), logoRect.getBottom(), false));
    g.fillPath (nameplatePath);
    g.setColour (t.knobRing.withAlpha (0.8f));
    g.strokePath (nameplatePath, juce::PathStrokeType (1.2f));

    // Power LED pip.
    g.setColour (t.ledOn.withAlpha (0.30f));
    g.fillEllipse (logoRect.getX() + 6.0f, logoRect.getY() + 7.0f, 8.0f, 8.0f);
    g.setColour (t.ledOn);
    g.fillEllipse (logoRect.getX() + 8.0f, logoRect.getY() + 9.0f, 4.0f, 4.0f);

    const auto titleFont = S116::skin().ui (18.0f, 0.15f);
    g.setFont (titleFont);

    const float startX = logoRect.getX() + 18.0f;
    const float textY  = logoRect.getY() + 5.0f;
    const float metaWidth = S116::textWidth (titleFont, "METAPHYSICAL ");

    g.setColour (juce::Colours::black);
    g.drawText ("METAPHYSICAL", startX + 1.0f, textY + 1.0f, metaWidth + 4.0f, 20.0f,
                juce::Justification::left, false);

    g.setGradientFill (juce::ColourGradient (t.heading, startX, textY,
                                             t.caption, startX, textY + 20.0f, false));
    g.drawText ("METAPHYSICAL", startX, textY, metaWidth + 4.0f, 20.0f,
                juce::Justification::left, false);

    const float factoryX = startX + metaWidth;
    g.setColour (juce::Colours::black);
    g.drawText ("FACTORY", factoryX + 1.0f, textY + 1.0f, 110.0f, 20.0f,
                juce::Justification::left, false);
    g.setColour (t.logoAccent);
    g.drawText ("FACTORY", factoryX, textY, 110.0f, 20.0f,
                juce::Justification::left, false);

    g.setFont (S116::skin().mono (9.5f));
    g.setColour (t.caption.withAlpha (0.80f));
    g.drawText ("AMBIENT DREAMSCAPE SYNTHESIS", startX, logoRect.getY() + 26.0f, 240.0f, 14.0f,
                juce::Justification::left, false);
}

void MetaphysicalFactoryAudioProcessorEditor::resized()
{
    layout();

    bankAPanel.setBounds (panelBounds[0]);
    telemetryPanel.setBounds (panelBounds[1]);
    resoSamplerPanel.setBounds (panelBounds[2]);
    bankBPanel.setBounds (panelBounds[3]);
    filtersPanel.setBounds (panelBounds[4]);
    effectsPanel.setBounds (panelBounds[5]);

    auto setHeaderKnob = [] (juce::Slider& slider, juce::Label& label, juce::Rectangle<int> cell)
    {
        slider.setBounds (cell.getCentreX() - 32, cell.getY() + 2, 64, 64);
        label.setBounds (cell.getX(), cell.getY() + 66, cell.getWidth(), 16);
    };
    setHeaderKnob (bassDial, bassLabel, headerKnobCells[0]);
    setHeaderKnob (trebleDial, trebleLabel, headerKnobCells[1]);
    setHeaderKnob (masterPitchDial, masterPitchLabel, headerKnobCells[2]);
    setHeaderKnob (masterVolDial, masterVolLabel, headerKnobCells[3]);

    // Bank A/B trims and faders.
    for (int i = 0; i < 6; ++i)
        S116::layout::placeBareKnob (fineDialsA[static_cast<size_t>(i)],
                                     trimCells[0][static_cast<size_t>(i)].getX(),
                                     trimCells[0][static_cast<size_t>(i)].getY(),
                                     trimCells[0][static_cast<size_t>(i)].getWidth(), 42);
    for (int i = 0; i < 6; ++i)
        S116::layout::placeBareKnob (fineDialsB[static_cast<size_t>(i)],
                                     trimCells[1][static_cast<size_t>(i)].getX(),
                                     trimCells[1][static_cast<size_t>(i)].getY(),
                                     trimCells[1][static_cast<size_t>(i)].getWidth(), 42);
    for (int i = 0; i < 6; ++i)
        motionSliders[static_cast<size_t>(i)]->setBounds (bankFaderCells[0][static_cast<size_t>(i)]);
    for (int i = 0; i < 6; ++i)
        motionSliders[static_cast<size_t>(6 + i)]->setBounds (bankFaderCells[1][static_cast<size_t>(i)]);

    // Filters.
    auto setFilterKnob = [] (juce::Slider& slider, juce::Label& label, juce::Rectangle<int> cell)
    {
        slider.setBounds (cell.getCentreX() - 32, cell.getY(), 64, 64);
        label.setBounds (cell.getX(), cell.getY() + 64, cell.getWidth(), 16);
    };
    setFilterKnob (morphADial, morphALabel, filterKnobCells[0]);
    setFilterKnob (driveDial, driveLabel, filterKnobCells[1]);
    setFilterKnob (morphBDial, morphBLabel, filterKnobCells[2]);
    for (int i = 0; i < 6; ++i)
        motionSliders[static_cast<size_t>(12 + i)]->setBounds (filterFaderCells[static_cast<size_t>(i)]);

    // Sampler / Resochord.
    for (int i = 0; i < 6; ++i)
        motionSliders[static_cast<size_t>(18 + i)]->setBounds (resoSamplerFaderCells[static_cast<size_t>(i)]);

    samplerResoDivider.setBounds (samplerResoDividerBounds);

    // Delay / Reverb.
    for (int i = 0; i < 8; ++i)
        motionSliders[static_cast<size_t>(24 + i)]->setBounds (effectsFaderCells[static_cast<size_t>(i)]);

    // Telemetry.
    auto telemetry = panelContentBounds[1];
    const int halfTelemetryH = (telemetry.getHeight() - 6) / 2;
    visualizer.setBounds (telemetry.removeFromTop (halfTelemetryH));
    telemetry.removeFromTop (6);
    waveformDisplay.setBounds (telemetry);
}

void MetaphysicalFactoryAudioProcessorEditor::collectBounds (juce::Component& parent,
                                                              const juce::String& name,
                                                              bool verbose, bool& ok) const
{
    const auto& kids = parent.getChildren();
    const auto parentBounds = parent.getLocalBounds();

    if (verbose)
        std::cout << "  " << name << " " << parentBounds.getWidth() << "x"
                  << parentBounds.getHeight() << std::endl;

    for (int i = 0; i < kids.size(); ++i)
    {
        auto* c = kids[i];
        const auto b = c->getBounds();
        const bool isResizeGrip = dynamic_cast<juce::ResizableCornerComponent*> (c) != nullptr;
        const auto childName = c->getName().isNotEmpty()
                                   ? c->getName()
                                   : (isResizeGrip ? juce::String ("RESIZE GRIP")
                                                   : juce::String ("child ") + juce::String (i));

        if (! parentBounds.contains (b))
        {
            std::cout << "  FAIL " << name << " / " << childName
                      << " leaves parent: " << b.toString() << std::endl;
            ok = false;
        }

        if (auto* label = dynamic_cast<juce::Label*> (c))
        {
            float size = 13.0f, kerning = 0.085f;
            const auto style = label->getProperties().getWithDefault ("style", "caption").toString();
            if (style == "field" || style == "sub") { size = 12.0f; kerning = 0.10f; }
            else if (style == "accent")
            {
                size = (float) (double) label->getProperties().getWithDefault ("captionSize", 13.0);
                kerning = 0.085f;
            }
            const auto tw = S116::textWidth (S116::skin().ui (size, kerning), label->getText());
            if (tw > (float) label->getWidth() + 0.5f)
            {
                std::cout << "  FAIL " << name << " / " << childName << " label width "
                          << tw << " > " << label->getWidth() << " (" << label->getText() << ")"
                          << std::endl;
                ok = false;
            }
        }

        for (int j = i + 1; j < kids.size(); ++j)
        {
            const bool otherIsGrip = dynamic_cast<juce::ResizableCornerComponent*> (kids[j]) != nullptr;
            const bool panelPair = (isResizeGrip && dynamic_cast<S116::SectionPanel*> (kids[j]) != nullptr)
                                || (otherIsGrip && dynamic_cast<S116::SectionPanel*> (c) != nullptr);

            if (! panelPair && b.intersects (kids[j]->getBounds()))
            {
                std::cout << "  FAIL " << name << " / " << childName
                          << " overlaps " << (kids[j]->getName().isNotEmpty()
                                                  ? kids[j]->getName()
                                                  : juce::String ("child ") + juce::String (j))
                          << " " << b.toString() << " vs " << kids[j]->getBounds().toString()
                          << std::endl;
                ok = false;
            }
        }

        if (verbose)
            std::cout << "    " << childName << " " << b.toString() << std::endl;

        collectBounds (*c, childName, verbose, ok);
    }
}

void MetaphysicalFactoryAudioProcessorEditor::runDebugBoundsReport()
{
    const int savedW = getWidth(), savedH = getHeight();
    const std::array<juce::Point<int>, 3> sizes { juce::Point<int> (1280, 736),
                                                  juce::Point<int> (1280, 820),
                                                  juce::Point<int> (2560, 1600) };

    bool allOk = true;
    for (const auto& size : sizes)
    {
        setSize (size.x, size.y);
        std::cout << "\n=== BOUNDS " << getWidth() << "x" << getHeight() << " ===" << std::endl;

        bool ok = true;
        collectBounds (*this, "EDITOR", true, ok);

        auto checkKnob = [&] (juce::Slider& slider, juce::Rectangle<int> cell, const juce::String& label)
        {
            const int a = slider.getBounds().getCentreX();
            const int b = cell.getCentreX();
            if (a != b)
            {
                std::cout << "  FAIL KNOB CENTRE " << label << ": " << a << " vs " << b << std::endl;
                ok = false;
            }
            else
            {
                std::cout << "  KNOB CENTRE " << label << ": " << a << " == " << b << " PASS" << std::endl;
            }
        };

        checkKnob (bassDial, headerKnobCells[0], "BASS");
        checkKnob (trebleDial, headerKnobCells[1], "TREBLE");
        checkKnob (masterPitchDial, headerKnobCells[2], "ROOT");
        checkKnob (masterVolDial, headerKnobCells[3], "OUTPUT");
        checkKnob (morphADial, filterKnobCells[0], "MORPH A");
        checkKnob (driveDial, filterKnobCells[1], "OVERDRIVE");
        checkKnob (morphBDial, filterKnobCells[2], "MORPH B");
        for (int i = 0; i < 6; ++i)
            checkKnob (fineDialsA[static_cast<size_t>(i)],
                       motionSliders[static_cast<size_t>(i)]->getBounds(), "FINE A" + juce::String (i + 1));
        for (int i = 0; i < 6; ++i)
            checkKnob (fineDialsB[static_cast<size_t>(i)],
                       motionSliders[static_cast<size_t>(6 + i)]->getBounds(), "FINE B" + juce::String (i + 1));

        const char* expectedLabels[] = {
            "SIN 1", "SIN 2", "SIN 3", "TRI", "PULSE", "LP NOISE",
            "DUAL NOISE", "SIN 1", "SIN 2", "TRI", "PULSE", "BI-PLS",
            "CUT A", "RES A", "CUT B", "RES B", "RING", "BAL",
            "SMP GAIN", "SMP SPD", "SMP TUNE",
            "RESO TUNE", "RESO DCY", "RESO MIX",
            "TIME", "RATE", "FDBK", "SPIN",
            "SIZE", "TAIL", "ABSORB", "SPACE"
        };
        bool labelsOk = true;
        for (int i = 0; i < 32; ++i)
        {
            const auto actual = motionSliders[static_cast<size_t>(i)]->getFaderLabelText();
            if (actual != expectedLabels[i])
            {
                std::cout << "  FAIL FADER LABEL " << (i + 1) << ": '" << actual
                          << "' != '" << expectedLabels[i] << "'" << std::endl;
                labelsOk = false;
            }
        }
        std::cout << "  FADER LABELS 32 " << (labelsOk ? "PASS" : "FAIL") << std::endl;
        ok = ok && labelsOk;

        std::cout << (ok ? "BOUNDS PASS" : "BOUNDS FAIL") << " " << getWidth() << "x"
                  << getHeight() << std::endl;
        jassert (ok);
        allOk = allOk && ok;
    }

    setSize (savedW, savedH);
    jassert (allOk);
}
