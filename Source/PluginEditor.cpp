#include "PluginEditor.h"

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

    // Helper to configure responsive, professional vertical-drag rotary dials
    auto configureRotaryDial = [](juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag); // Standard vertical drag
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setMouseDragSensitivity(140);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setInterceptsMouseClicks(true, false);
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
    bassLabel.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    bassLabel.setJustificationType(juce::Justification::centred);
    bassLabel.setColour(juce::Label::textColourId, juce::Colour(MetaphysicalLookAndFeel::cSilkscreenMuted));
    bassLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(bassLabel);

    configureRotaryDial(trebleDial);
    addAndMakeVisible(trebleDial);
    trebleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "master_eq_treble", trebleDial);

    trebleLabel.setText("TREBLE", juce::dontSendNotification);
    trebleLabel.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    trebleLabel.setJustificationType(juce::Justification::centred);
    trebleLabel.setColour(juce::Label::textColourId, juce::Colour(MetaphysicalLookAndFeel::cSilkscreenMuted));
    trebleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(trebleLabel);

    // 3. Master Hardware Potentiometers with Real-Time Interactive Note Readout
    configureRotaryDial(masterPitchDial);
    addAndMakeVisible(masterPitchDial);
    masterPitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "master_pitch", masterPitchDial);

    masterPitchLabel.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    masterPitchLabel.setJustificationType(juce::Justification::centred);
    masterPitchLabel.setColour(juce::Label::textColourId, juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
    masterPitchLabel.setInterceptsMouseClicks(false, false);
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
    masterVolLabel.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    masterVolLabel.setJustificationType(juce::Justification::centred);
    masterVolLabel.setColour(juce::Label::textColourId, juce::Colour(MetaphysicalLookAndFeel::cSilkscreenMuted));
    masterVolLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(masterVolLabel);

    // 4. Overdrive Distortion Potentiometer
    configureRotaryDial(driveDial);
    addAndMakeVisible(driveDial);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "distortion_drive", driveDial);

    driveLabel.setText("OVERDRIVE", juce::dontSendNotification);
    driveLabel.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    driveLabel.setJustificationType(juce::Justification::centred);
    driveLabel.setColour(juce::Label::textColourId, juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
    driveLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(driveLabel);

    // 5. Hybrid Continuous LP/HP Filter Morphs
    configureRotaryDial(morphADial);
    addAndMakeVisible(morphADial);
    morphAAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "filter_morph_a", morphADial);

    morphALabel.setText("MORPH A", juce::dontSendNotification);
    morphALabel.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    morphALabel.setJustificationType(juce::Justification::centred);
    morphALabel.setColour(juce::Label::textColourId, juce::Colour(MetaphysicalLookAndFeel::cSilkscreenMuted));
    morphALabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(morphALabel);

    configureRotaryDial(morphBDial);
    addAndMakeVisible(morphBDial);
    morphBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "filter_morph_b", morphBDial);

    morphBLabel.setText("MORPH B", juce::dontSendNotification);
    morphBLabel.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    morphBLabel.setJustificationType(juce::Justification::centred);
    morphBLabel.setColour(juce::Label::textColourId, juce::Colour(MetaphysicalLookAndFeel::cSilkscreenMuted));
    morphBLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(morphBLabel);

    // 6. Individual Voice Fine-Tune Micro-Knobs (-100 to +100 Cents)
    for (int i = 0; i < 6; ++i)
    {
        // Bank A Fine-Tune
        auto& dialA = fineDialsA[static_cast<size_t>(i)];
        configureRotaryDial(dialA);
        addAndMakeVisible(dialA);
        fineTuneAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getAPVTS(), "oscA_" + juce::String(i) + "_fine", dialA));

        // Bank B Fine-Tune
        auto& dialB = fineDialsB[static_cast<size_t>(i)];
        configureRotaryDial(dialB);
        addAndMakeVisible(dialB);
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
    addAndMakeVisible(visualizer);
    addAndMakeVisible(waveformDisplay);

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
    setResizeLimits(1050, 680, 2560, 1600);
    setSize(1280, 780);
}

MetaphysicalFactoryAudioProcessorEditor::~MetaphysicalFactoryAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void MetaphysicalFactoryAudioProcessorEditor::addMotionFader(const juce::String& paramID, const juce::String& label, int laneIndex)
{
    auto fader = std::make_unique<MotionSlider>(
        audioProcessor.getAPVTS(), paramID, label, audioProcessor.getMotionEngine(), laneIndex);
    addAndMakeVisible(*fader);
    motionSliders.push_back(std::move(fader));
}

void MetaphysicalFactoryAudioProcessorEditor::paint(juce::Graphics& g)
{
    // 1. True Deep Anodized Matte Black Chassis Background
    g.fillAll(juce::Colour(MetaphysicalLookAndFeel::cChassisBg));

    // Corner Screw Rivets
    auto drawScrew = [&](float x, float y)
    {
        g.setColour(juce::Colour(0xff18191e));
        g.fillEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        g.setColour(juce::Colour(0xff050608));
        g.fillEllipse(x - 2.0f, y - 2.0f, 4.0f, 4.0f);
        g.setColour(juce::Colour(0x35ffffff));
        g.drawEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f, 0.8f);
    };

    drawScrew(8.0f, 8.0f);
    drawScrew(static_cast<float>(getWidth()) - 8.0f, 8.0f);
    drawScrew(8.0f, static_cast<float>(getHeight()) - 8.0f);
    drawScrew(static_cast<float>(getWidth()) - 8.0f, static_cast<float>(getHeight()) - 8.0f);

    // 2. Late 90s / Early 2000s Scaled (+15%) Hardware Vector Emblem
    const auto logoRect = juce::Rectangle<float>(28.0f, 5.0f, 255.0f, 44.0f);

    // Beveled Parallelogram Metal Nameplate
    juce::Path nameplatePath;
    nameplatePath.startNewSubPath(logoRect.getX(), logoRect.getY() + 2.0f);
    nameplatePath.lineTo(logoRect.getRight() - 12.0f, logoRect.getY() + 2.0f);
    nameplatePath.lineTo(logoRect.getRight() - 2.0f, logoRect.getBottom() - 2.0f);
    nameplatePath.lineTo(logoRect.getX() + 10.0f, logoRect.getBottom() - 2.0f);
    nameplatePath.closeSubPath();

    g.setColour(juce::Colour(0xff101216));
    g.fillPath(nameplatePath);
    g.setColour(juce::Colour(0xff262933));
    g.strokePath(nameplatePath, juce::PathStrokeType(1.2f));

    // Power Accent LED Pip
    g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink).withAlpha(0.35f));
    g.fillEllipse(logoRect.getX() + 6.0f, logoRect.getY() + 7.0f, 8.0f, 8.0f);
    g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
    g.fillEllipse(logoRect.getX() + 8.0f, logoRect.getY() + 9.0f, 4.0f, 4.0f);

    // Main Logo Typography (16.5f Bold Italic, +15% larger)
    const auto titleFontOptions = juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), 16.5f, juce::Font::bold | juce::Font::italic);
    juce::Font titleFont(titleFontOptions);
    g.setFont(titleFontOptions);

    const float startX = logoRect.getX() + 18.0f;
    const float textY  = logoRect.getY() + 5.0f;

    // Exact string measurement guarantees "FACTORY" sits directly beside "METAPHYSICAL"
    const float metaTextWidth = titleFont.getStringWidthFloat("METAPHYSICAL ");

    // "METAPHYSICAL" Drop-shadow
    g.setColour(juce::Colours::black);
    g.drawText("METAPHYSICAL", startX + 1.0f, textY + 1.0f, metaTextWidth + 4.0f, 20.0f, juce::Justification::left, false);

    // "METAPHYSICAL" Chrome Gradient Face
    juce::ColourGradient chromeGrad(juce::Colour(0xffffffff), startX, textY,
                                    juce::Colour(0xff949aa8), startX, textY + 20.0f, false);
    g.setGradientFill(chromeGrad);
    g.drawText("METAPHYSICAL", startX, textY, metaTextWidth + 4.0f, 20.0f, juce::Justification::left, false);

    // "FACTORY" in seamless Hot Neon Pink (Identical size, zero intrusive box)
    const float factoryX = startX + metaTextWidth;
    g.setColour(juce::Colours::black);
    g.drawText("FACTORY", factoryX + 1.0f, textY + 1.0f, 100.0f, 20.0f, juce::Justification::left, false);

    g.setColour(juce::Colour(MetaphysicalLookAndFeel::cNeonPink));
    g.drawText("FACTORY", factoryX, textY, 100.0f, 20.0f, juce::Justification::left, false);

    // Hardware Sub-Telemetry Stencil (+15% larger: 9.5f bold, completely legible)
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.5f, juce::Font::bold));
    g.setColour(juce::Colour(MetaphysicalLookAndFeel::cSilkscreenWhite).withAlpha(0.75f));
    g.drawText("AMBIENT DREAMSCAPE SYNTHESIS", startX, logoRect.getY() + 26.0f, 230.0f, 14.0f, juce::Justification::left, false);

    // 3. Hardware Rack Box Drawing Helper
    auto drawHardwareRack = [&](const juce::Rectangle<int>& area, const juce::String& title, const juce::String& sub)
    {
        const auto fArea = area.toFloat();

        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cWellBg));
        g.fillRoundedRectangle(fArea, 4.0f);

        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cPanelBorder));
        g.drawRoundedRectangle(fArea, 4.0f, 1.0f);

        // Header silkscreen badge
        g.setColour(juce::Colour(MetaphysicalLookAndFeel::cSilkscreenWhite));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(title, area.getX() + 8, area.getY() + 4, area.getWidth() - 16, 14, juce::Justification::left);

        if (sub.isNotEmpty() && area.getWidth() > 240)
        {
            g.setColour(juce::Colour(MetaphysicalLookAndFeel::cSilkscreenMuted));
            g.setFont(juce::FontOptions(8.5f, juce::Font::plain));
            g.drawText(sub, area.getX() + 8, area.getY() + 4, area.getWidth() - 16, 14, juce::Justification::right);
        }
    };

    // Calculate dynamic responsive layout areas matching resized()
    auto bounds = getLocalBounds();
    bounds.removeFromTop(54); // Header
    const int margin = 12;
    const int gap = 10;
    auto body = bounds.reduced(margin, margin);

    const int upperH = static_cast<int>((body.getHeight() - gap) * 0.485f);
    const int lowerH = body.getHeight() - gap - upperH;

    auto upperRow = body.removeFromTop(upperH);
    body.removeFromTop(gap);
    auto lowerRow = body;

    const int col1W = static_cast<int>((upperRow.getWidth() - (gap * 2)) * 0.285f);
    const int col2W = col1W;
    const int col3W = upperRow.getWidth() - (gap * 2) - col1W - col2W;

    // Draw Upper Decks
    drawHardwareRack(upperRow.removeFromLeft(col1W), "BANK A: VOICES & FINE TRIM", "SINE / TRI / BIPOLAR / NOISE");
    upperRow.removeFromLeft(gap);
    drawHardwareRack(upperRow.removeFromLeft(col2W), "CENTRAL TELEMETRY DISPLAYS", "3D TORUS / LUFS / CORR");
    upperRow.removeFromLeft(gap);
    drawHardwareRack(upperRow, "TEXTURE SAMPLER & RESOCHORD", "PARALLEL COMBS");

    // Draw Lower Decks
    drawHardwareRack(lowerRow.removeFromLeft(col1W), "BANK B: VOICES & FINE TRIM", "DUAL NOISE / SINE / TRI / PULSE");
    lowerRow.removeFromLeft(gap);
    drawHardwareRack(lowerRow.removeFromLeft(col2W), "TPT FILTERS & OVERDRIVE", "HYBRID MORPH / RING MOD");
    lowerRow.removeFromLeft(gap);
    drawHardwareRack(lowerRow, "DIFFUSION DELAY & SPACE REVERB", "FDN 4x4 / SPIN");
}

void MetaphysicalFactoryAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // 1. Top Header Bar (Height 54)
    auto header = bounds.removeFromTop(54);
    header.removeFromLeft(295); // Clearance for the enlarged vector emblem badge

    // Master Tone EQ & Output Potentiometers (Far Right Deck)
    auto masterRight = header.removeFromRight(300);
    const int dialW = masterRight.getWidth() / 4;

    auto bassArea = masterRight.removeFromLeft(dialW);
    bassDial.setBounds(bassArea.getCentreX() - 15, 3, 30, 30);
    bassLabel.setBounds(bassArea.getX(), 37, bassArea.getWidth(), 13);

    auto trebleArea = masterRight.removeFromLeft(dialW);
    trebleDial.setBounds(trebleArea.getCentreX() - 15, 3, 30, 30);
    trebleLabel.setBounds(trebleArea.getX(), 37, trebleArea.getWidth(), 13);

    auto pitchArea = masterRight.removeFromLeft(dialW);
    masterPitchDial.setBounds(pitchArea.getCentreX() - 15, 3, 30, 30);
    masterPitchLabel.setBounds(pitchArea.getX() - 8, 37, pitchArea.getWidth() + 16, 13);

    auto volArea = masterRight;
    masterVolDial.setBounds(volArea.getCentreX() - 15, 3, 30, 30);
    masterVolLabel.setBounds(volArea.getX(), 37, volArea.getWidth(), 13);

    // Helper to position faders evenly within an area
    auto layoutFaders = [this](int startLane, int count, juce::Rectangle<int> area)
    {
        if (count <= 0) return;
        const int faderWidth = area.getWidth() / count;
        for (int i = 0; i < count; ++i)
        {
            const int lane = startLane + i;
            if (lane < static_cast<int>(motionSliders.size()))
            {
                motionSliders[lane]->setBounds(area.getX() + (i * faderWidth), area.getY(), faderWidth, area.getHeight());
            }
        }
    };

    // 2. Responsive 3-Column Grid Layout
    const int margin = 12;
    const int gap = 10;
    auto body = bounds.reduced(margin, margin);

    const int upperH = static_cast<int>((body.getHeight() - gap) * 0.485f);
    const int lowerH = body.getHeight() - gap - upperH;

    auto upperRow = body.removeFromTop(upperH);
    body.removeFromTop(gap);
    auto lowerRow = body;

    const int col1W = static_cast<int>((upperRow.getWidth() - (gap * 2)) * 0.285f);
    const int col2W = col1W;
    const int col3W = upperRow.getWidth() - (gap * 2) - col1W - col2W;

    auto deckBankA        = upperRow.removeFromLeft(col1W);
    upperRow.removeFromLeft(gap);
    auto deckTelemetry    = upperRow.removeFromLeft(col2W);
    upperRow.removeFromLeft(gap);
    auto deckResoSampler  = upperRow;

    auto deckBankB        = lowerRow.removeFromLeft(col1W);
    lowerRow.removeFromLeft(gap);
    auto deckFilters      = lowerRow.removeFromLeft(col2W);
    lowerRow.removeFromLeft(gap);
    auto deckEffects      = lowerRow;

    // 3. Position Bank A (Top Fine-Tune Trimmers + Main Faders)
    deckBankA.removeFromTop(20);
    deckBankA.reduce(4, 4);
    auto trimAreaA = deckBankA.removeFromTop(32);
    const int trimWidthA = trimAreaA.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
    {
        fineDialsA[static_cast<size_t>(i)].setBounds(trimAreaA.getX() + (i * trimWidthA) + (trimWidthA / 2 - 12),
                                                     trimAreaA.getY(), 24, 24);
    }
    layoutFaders(0, 6, deckBankA);

    // 4. Position Bank B (Top Fine-Tune Trimmers + Main Faders)
    deckBankB.removeFromTop(20);
    deckBankB.reduce(4, 4);
    auto trimAreaB = deckBankB.removeFromTop(32);
    const int trimWidthB = trimAreaB.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
    {
        fineDialsB[static_cast<size_t>(i)].setBounds(trimAreaB.getX() + (i * trimWidthB) + (trimWidthB / 2 - 12),
                                                     trimAreaB.getY(), 24, 24);
    }
    layoutFaders(6, 6, deckBankB);

    // 5. Position Dual Telemetry Screens
    deckTelemetry.removeFromTop(20);
    deckTelemetry.reduce(6, 6);
    const int halfTelemetryH = (deckTelemetry.getHeight() - 6) / 2;
    visualizer.setBounds(deckTelemetry.removeFromTop(halfTelemetryH));
    deckTelemetry.removeFromTop(6);
    waveformDisplay.setBounds(deckTelemetry);

    // 6. Position Filters, Morphs & Overdrive
    deckFilters.removeFromTop(20);
    deckFilters.reduce(4, 4);
    auto filterControls = deckFilters.removeFromTop(44);
    const int subCol = filterControls.getWidth() / 3;

    auto morphAreaA = filterControls.removeFromLeft(subCol);
    morphADial.setBounds(morphAreaA.getCentreX() - 14, morphAreaA.getY(), 28, 28);
    morphALabel.setBounds(morphAreaA.getX(), morphAreaA.getY() + 29, morphAreaA.getWidth(), 12);

    auto driveArea = filterControls.removeFromLeft(subCol);
    driveDial.setBounds(driveArea.getCentreX() - 14, driveArea.getY(), 28, 28);
    driveLabel.setBounds(driveArea.getX(), driveArea.getY() + 29, driveArea.getWidth(), 12);

    auto morphAreaB = filterControls;
    morphBDial.setBounds(morphAreaB.getCentreX() - 14, morphAreaB.getY(), 28, 28);
    morphBLabel.setBounds(morphAreaB.getX(), morphAreaB.getY() + 29, morphAreaB.getWidth(), 12);

    layoutFaders(12, 6, deckFilters);

    // 7. Position Sampler & Resochord (3 Sampler Faders on Left, 3 Resochord Faders on Right)
    deckResoSampler.removeFromTop(20);
    deckResoSampler.reduce(4, 4);
    const int splitUpper = deckResoSampler.getWidth() / 2;
    layoutFaders(18, 3, deckResoSampler.removeFromLeft(splitUpper)); // Sampler: GAIN, SPD, TUNE (Lanes 18 - 20)
    layoutFaders(21, 3, deckResoSampler);                           // Resochord: TUNE, DCY, MIX (Lanes 21 - 23)

    // 8. Position Spin Delay & Space Reverb
    deckEffects.removeFromTop(20);
    deckEffects.reduce(4, 4);
    const int splitLower = deckEffects.getWidth() / 2;
    layoutFaders(24, 4, deckEffects.removeFromLeft(splitLower));     // Spin Delay (Lanes 24 - 27)
    layoutFaders(28, 4, deckEffects);                               // Space Reverb (Lanes 28 - 31)
}