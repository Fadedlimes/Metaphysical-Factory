#include "PluginProcessor.h"
#include "PluginEditor.h"

// Transparent analog-style soft-knee saturation limiter (ceiling at 0.98, zero OS ducking)
static inline float softSaturate(float x)
{
    const float threshold = 0.65f;
    if (x > threshold)
        return threshold + (0.33f * std::tanh((x - threshold) / 0.33f));
    if (x < -threshold)
        return -threshold + (0.33f * std::tanh((x + threshold) / 0.33f));
    return x;
}

// Helper to resolve parameter values cleanly without UI/audio thread competition
static float getLaneParamValue(juce::AudioProcessorValueTreeState& apvts,
                               MotionEngine& motionEngine,
                               const juce::String& paramID,
                               int laneIndex)
{
    auto* param = apvts.getParameter(paramID);
    if (param == nullptr)
        return 0.0f;

    auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param);
    if (ranged == nullptr)
        return param->getValue();

    // 1. If currently recording, feed the user's real-time slider movement into the motion recorder
    if (motionEngine.isLaneRecording(laneIndex))
    {
        const float currentNorm = ranged->getValue();
        motionEngine.setNormalizedValue(laneIndex, currentNorm);
        return ranged->convertFrom0to1(currentNorm);
    }

    // 2. If actively playing a recorded loop or LFO, motion engine drives the DSP
    if (motionEngine.isLaneLooping(laneIndex) || motionEngine.getLaneMode(laneIndex) != MotionEngine::Static)
    {
        const float normVal = motionEngine.getLaneValue(laneIndex);
        return ranged->convertFrom0to1(normVal);
    }

    // 3. In static mode, sync the parameter to the motion engine's base value and pass through
    const float currentNorm = ranged->getValue();
    motionEngine.setNormalizedValue(laneIndex, currentNorm);
    return ranged->convertFrom0to1(currentNorm);
}

MetaphysicalFactoryAudioProcessor::MetaphysicalFactoryAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      lastSampleDirectory(juce::File::getSpecialLocation(juce::File::userHomeDirectory))
{
    formatManager.registerBasicFormats();

    scopeBufferSumAB.assign(ScopeFifoCapacity, 0.0f);
    scopeBufferRingAB.assign(ScopeFifoCapacity, 0.0f);
    scopeBufferOutL.assign(ScopeFifoCapacity, 0.0f);
    scopeBufferOutR.assign(ScopeFifoCapacity, 0.0f);

    // Initialize authentic Metaphysical Fabrications generator complements
    oscBankA.setBankType(OscillatorBank::BankA);
    oscBankB.setBankType(OscillatorBank::BankB);
}

MetaphysicalFactoryAudioProcessor::~MetaphysicalFactoryAudioProcessor()
{
}

const juce::String MetaphysicalFactoryAudioProcessor::getName() const
{
    return "Metaphysical Factory";
}

bool MetaphysicalFactoryAudioProcessor::acceptsMidi() const         { return true; }
bool MetaphysicalFactoryAudioProcessor::producesMidi() const        { return false; }
bool MetaphysicalFactoryAudioProcessor::isMidiEffect() const        { return false; }
double MetaphysicalFactoryAudioProcessor::getTailLengthSeconds() const { return 4.0; }

int MetaphysicalFactoryAudioProcessor::getNumPrograms()              { return 1; }
int MetaphysicalFactoryAudioProcessor::getCurrentProgram()           { return 0; }
void MetaphysicalFactoryAudioProcessor::setCurrentProgram(int)       {}
const juce::String MetaphysicalFactoryAudioProcessor::getProgramName(int) { return {}; }
void MetaphysicalFactoryAudioProcessor::changeProgramName(int, const juce::String&) {}

bool MetaphysicalFactoryAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout MetaphysicalFactoryAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Master Controls (Root Pitch Snapped to Musical Notes: C1 to C6)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "master_pitch", 1 }, "Root Pitch",
        juce::NormalisableRange<float>(24.0f, 84.0f, 1.0f), 45.0f, // Default Note 45 = A2 (110 Hz)
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float val, int)
        {
            const int note = static_cast<int>(std::round(val));
            return juce::MidiMessage::getMidiNoteName(note, true, true, 3)
                   + " (" + juce::String(juce::MidiMessage::getMidiNoteInHertz(note), 1) + " Hz)";
        })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "master_volume", 1 }, "Master Volume",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ext_in_gain", 1 }, "External In Gain",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.0f));

    // Master Tone EQ (Highs & Lows +/- 12 dB)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "master_eq_bass", 1 }, "Master Bass",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "master_eq_treble", 1 }, "Master Treble",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    // Dual Independent 3D Oscilloscope Inputs: A+B Level & A x B Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "scope_level_sum", 1 }, "Scope A+B Level",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "scope_level_ring", 1 }, "Scope A x B Level",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.8f));

    // 2. Oscillator Banks (A & B: 6 voices each with Coarse & Fine Tuning)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscA_master", 1 }, "Osc A Master Level", 0.0f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscB_master", 1 }, "Osc B Master Level", 0.0f, 1.0f, 0.8f));

    for (int i = 0; i < 6; ++i)
    {
        // Bank A
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "oscA_" + juce::String(i) + "_semi", 1 },
            "Osc A" + juce::String(i + 1) + " Semi", -36.0f, 36.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "oscA_" + juce::String(i) + "_fine", 1 },
            "Osc A" + juce::String(i + 1) + " Fine", -100.0f, 100.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "oscA_" + juce::String(i) + "_level", 1 },
            "Osc A" + juce::String(i + 1) + " Level", 0.0f, 1.0f, (i == 0 ? 0.7f : 0.0f)));

        // Bank B
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "oscB_" + juce::String(i) + "_semi", 1 },
            "Osc B" + juce::String(i + 1) + " Semi", -36.0f, 36.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "oscB_" + juce::String(i) + "_fine", 1 },
            "Osc B" + juce::String(i + 1) + " Fine", -100.0f, 100.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "oscB_" + juce::String(i) + "_level", 1 },
            "Osc B" + juce::String(i + 1) + " Level", 0.0f, 1.0f, (i == 0 ? 0.7f : 0.0f)));
    }

    // 3. Filter, Hybrid Morph, Ring Mod & Overdrive
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filter_cutoff_a", 1 }, "Cutoff A",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.25f), 2500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filter_res_a", 1 }, "Resonance A", 0.0f, 0.99f, 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filter_morph_a", 1 }, "Filter Morph A", 0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filter_cutoff_b", 1 }, "Cutoff B",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.25f), 2500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filter_res_b", 1 }, "Resonance B", 0.0f, 0.99f, 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filter_morph_b", 1 }, "Filter Morph B", 0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ring_mod_mix", 1 }, "RingMod Mix", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "distortion_drive", 1 }, "Overdrive Distortion", 0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "stereo_spread", 1 }, "Stereo Spread", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "balance_ab", 1 }, "Balance A/B", 0.0f, 1.0f, 0.5f));

    // 4. Sample Looper
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "looper_gain", 1 }, "Looper Gain", 0.0f, 1.5f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "looper_speed", 1 }, "Looper Speed", -3.0f, 3.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "looper_pitch", 1 }, "Looper Pitch", -24.0f, 24.0f, 0.0f));

    // 5. Resochord (Dedicated Tuning + Resonance + Damping + Mix)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reso_tune", 1 }, "Resochord Tune",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reso_decay", 1 }, "Resochord Resonance",
        juce::NormalisableRange<float>(0.0f, 0.99f, 0.01f), 0.88f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reso_damp", 1 }, "Resochord Damping",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reso_mix", 1 }, "Resochord Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.45f));

    // 6. Spin Delay
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "spin_time", 1 }, "Spin Delay Time", 0.02f, 1.5f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "spin_rate", 1 }, "Spin Rate", 0.02f, 5.0f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "spin_feedback", 1 }, "Spin Feedback", 0.0f, 0.95f, 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "spin_mix", 1 }, "Spin Mix", 0.0f, 1.0f, 0.35f));

    // 7. Space Reverb
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reverb_size", 1 }, "Reverb Size", 0.4f, 2.2f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reverb_decay", 1 }, "Reverb Decay", 0.1f, 0.99f, 0.85f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reverb_damp", 1 }, "Reverb Damping", 0.0f, 0.95f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reverb_mix", 1 }, "Reverb Mix", 0.0f, 1.0f, 0.4f));

    return { params.begin(), params.end() };
}

void MetaphysicalFactoryAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    oscBankA.prepare(sampleRate, samplesPerBlock);
    oscBankB.prepare(sampleRate, samplesPerBlock);
    filterRingMod.prepare(sampleRate, samplesPerBlock);
    sampleLooper.prepare(sampleRate, samplesPerBlock);
    resochord.prepare(sampleRate, samplesPerBlock);
    spinDelay.prepare(sampleRate, samplesPerBlock);
    spaceReverb.prepare(sampleRate, samplesPerBlock);
    masterEq.prepare(sampleRate, samplesPerBlock);
    motionEngine.prepare(sampleRate, samplesPerBlock);

    bankABuffer.setSize(1, samplesPerBlock);
    bankBBuffer.setSize(1, samplesPerBlock);
    synthMixBuffer.setSize(2, samplesPerBlock);
    scopeSumBuffer.setSize(1, samplesPerBlock);
    scopeRingBuffer.setSize(1, samplesPerBlock);

    scopeFifo.reset();
}

void MetaphysicalFactoryAudioProcessor::releaseResources()
{
    bankABuffer.setSize(0, 0);
    bankBBuffer.setSize(0, 0);
    synthMixBuffer.setSize(0, 0);
    scopeSumBuffer.setSize(0, 0);
    scopeRingBuffer.setSize(0, 0);
}

void MetaphysicalFactoryAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // 1. Process MIDI Note events for dynamic root tuning
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            currentMidiPitchHz.store(static_cast<float>(msg.getMidiNoteInHertz(msg.getNoteNumber())));
            isMidiActive.store(true);
        }
        else if (msg.isNoteOff())
        {
            // Releasing MIDI key returns pitch control to front-panel Root Pitch knob
            isMidiActive.store(false);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            isMidiActive.store(false);
        }
    }

    // 2. Advance the 32-lane parameter gesture automation
    motionEngine.advanceBlock(numSamples);

    // 3. Resolve Master Root Pitch (MIDI overrides semitone-quantized APVTS note)
    float basePitch = 110.0f;
    if (isMidiActive.load())
    {
        basePitch = currentMidiPitchHz.load();
    }
    else
    {
        const float noteVal = apvts.getRawParameterValue("master_pitch")->load();
        const int noteInt = static_cast<int>(std::round(noteVal));
        basePitch = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(noteInt));
    }

    oscBankA.setBaseFrequency(basePitch);
    oscBankB.setBaseFrequency(basePitch);

    // 4. Resolve Dedicated Resochord Tuning (Completely independent from Sampler)
    const float resoTuneSemitones = getLaneParamValue(apvts, motionEngine, "reso_tune", 21);
    const float resoRootFreq = basePitch * std::pow(2.0f, resoTuneSemitones / 12.0f);
    resochord.setRootFrequency(resoRootFreq);

    // 5. Update Osc Bank A & B Parameters (Levels via Motion Engine, Coarse & Fine via APVTS)
    oscBankA.setMasterGain(apvts.getRawParameterValue("oscA_master")->load());
    oscBankB.setMasterGain(apvts.getRawParameterValue("oscB_master")->load());

    for (int i = 0; i < 6; ++i)
    {
        // Bank A
        oscBankA.setOscSemitone(i, apvts.getRawParameterValue("oscA_" + juce::String(i) + "_semi")->load());
        oscBankA.setOscFineTune(i, apvts.getRawParameterValue("oscA_" + juce::String(i) + "_fine")->load());
        const float oscALevel = getLaneParamValue(apvts, motionEngine, "oscA_" + juce::String(i) + "_level", i);
        oscBankA.setOscLevel(i, oscALevel);

        // Bank B
        oscBankB.setOscSemitone(i, apvts.getRawParameterValue("oscB_" + juce::String(i) + "_semi")->load());
        oscBankB.setOscFineTune(i, apvts.getRawParameterValue("oscB_" + juce::String(i) + "_fine")->load());
        const float oscBLevel = getLaneParamValue(apvts, motionEngine, "oscB_" + juce::String(i) + "_level", 6 + i);
        oscBankB.setOscLevel(i, oscBLevel);
    }

    // Render Mono Banks
    bankABuffer.clear();
    bankBBuffer.clear();
    oscBankA.processBlock(bankABuffer, 0, 0, numSamples, false);
    oscBankB.processBlock(bankBBuffer, 0, 0, numSamples, false);

    // Compute internal (A+B) and (A*B) streams for the 3D Lissajous Scope
    auto* aPtr = bankABuffer.getReadPointer(0);
    auto* bPtr = bankBBuffer.getReadPointer(0);
    auto* sumWritePtr  = scopeSumBuffer.getWritePointer(0);
    auto* ringWritePtr = scopeRingBuffer.getWritePointer(0);

    for (int i = 0; i < numSamples; ++i)
    {
        sumWritePtr[i]  = (aPtr[i] + bPtr[i]) * 0.707f;
        ringWritePtr[i] = aPtr[i] * bPtr[i] * 2.5f;
    }

    // 6. Update and Process Filter, Hybrid Morph, Ring Mod & Overdrive
    filterRingMod.setCutoffA(getLaneParamValue(apvts, motionEngine, "filter_cutoff_a", 12));
    filterRingMod.setResonanceA(getLaneParamValue(apvts, motionEngine, "filter_res_a", 13));
    filterRingMod.setMorphA(apvts.getRawParameterValue("filter_morph_a")->load());

    filterRingMod.setCutoffB(getLaneParamValue(apvts, motionEngine, "filter_cutoff_b", 14));
    filterRingMod.setResonanceB(getLaneParamValue(apvts, motionEngine, "filter_res_b", 15));
    filterRingMod.setMorphB(apvts.getRawParameterValue("filter_morph_b")->load());

    filterRingMod.setRingModMix(getLaneParamValue(apvts, motionEngine, "ring_mod_mix", 16));
    filterRingMod.setBalanceAB(getLaneParamValue(apvts, motionEngine, "balance_ab", 17));
    filterRingMod.setDistortionDrive(apvts.getRawParameterValue("distortion_drive")->load());
    filterRingMod.setStereoSpread(apvts.getRawParameterValue("stereo_spread")->load());

    synthMixBuffer.clear();
    filterRingMod.processBlock(bankABuffer.getReadPointer(0),
                               bankBBuffer.getReadPointer(0),
                               synthMixBuffer.getWritePointer(0),
                               synthMixBuffer.getWritePointer(1),
                               numSamples);

    // 7. Update and Accumulate Sample Looper (Lane 18: Gain, Lane 19: Speed, Lane 20: Pitch)
    sampleLooper.setGain(getLaneParamValue(apvts, motionEngine, "looper_gain", 18) * 0.707f);
    sampleLooper.setPlaybackSpeed(getLaneParamValue(apvts, motionEngine, "looper_speed", 19));
    sampleLooper.setPitchSemitones(getLaneParamValue(apvts, motionEngine, "looper_pitch", 20));
    sampleLooper.processBlock(synthMixBuffer, 0, numSamples);

    // 8. Mix External Audio In (if present)
    const float extGain = apvts.getRawParameterValue("ext_in_gain")->load();
    if (extGain > 0.0001f && totalNumInputChannels >= 2)
    {
        synthMixBuffer.addFrom(0, 0, buffer, 0, 0, numSamples, extGain * 0.707f);
        synthMixBuffer.addFrom(1, 0, buffer, 1, 0, numSamples, extGain * 0.707f);
    }

    // 9. Process Resochord Comb Filter Array (Lane 21: Tune, Lane 22: Decay, Lane 23: Mix)
    resochord.setMasterResonance(getLaneParamValue(apvts, motionEngine, "reso_decay", 22));
    resochord.setMasterDamping(apvts.getRawParameterValue("reso_damp")->load());
    resochord.setDryWet(getLaneParamValue(apvts, motionEngine, "reso_mix", 23));
    resochord.processBlock(synthMixBuffer, 0, numSamples);

    // 10. Process Spin Modulation Delay via Motion Engine
    spinDelay.setDelayTime(getLaneParamValue(apvts, motionEngine, "spin_time", 24));
    spinDelay.setSpinRate(getLaneParamValue(apvts, motionEngine, "spin_rate", 25));
    spinDelay.setFeedback(getLaneParamValue(apvts, motionEngine, "spin_feedback", 26));
    spinDelay.setDryWet(getLaneParamValue(apvts, motionEngine, "spin_mix", 27));
    spinDelay.processBlock(synthMixBuffer, 0, numSamples);

    // 11. Process Space Algorithmic Reverb via Motion Engine
    spaceReverb.setRoomSize(getLaneParamValue(apvts, motionEngine, "reverb_size", 28));
    spaceReverb.setDecayTime(getLaneParamValue(apvts, motionEngine, "reverb_decay", 29));
    spaceReverb.setDamping(getLaneParamValue(apvts, motionEngine, "reverb_damp", 30));
    spaceReverb.setDryWet(getLaneParamValue(apvts, motionEngine, "reverb_mix", 31));
    spaceReverb.processBlock(synthMixBuffer, 0, numSamples);

    // 12. Master 2-Band Equalizer (Bass & Treble Tone Stack)
    masterEq.setBassGainDb(apvts.getRawParameterValue("master_eq_bass")->load());
    masterEq.setTrebleGainDb(apvts.getRawParameterValue("master_eq_treble")->load());
    masterEq.processBlock(synthMixBuffer, 0, numSamples);

    // 13. Final Master Gain Stage & Analog Soft-Knee Saturation Limiter
    const float masterVol = apvts.getRawParameterValue("master_volume")->load();
    auto* outL = synthMixBuffer.getWritePointer(0);
    auto* outR = synthMixBuffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = outL[i] * masterVol * 0.72f;
        const float r = outR[i] * masterVol * 0.72f;

        outL[i] = softSaturate(l);
        outR[i] = softSaturate(r);
    }

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        const int srcCh = ch < synthMixBuffer.getNumChannels() ? ch : 0;
        buffer.copyFrom(ch, 0, synthMixBuffer, srcCh, 0, numSamples);
    }

    // Clear unused output channels
    for (int ch = totalNumOutputChannels; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);

    // 14. Push 4-Channel Telemetry Stream into FIFO for 3D Oscilloscope and OLED Meters
    pushScopeData(scopeSumBuffer.getReadPointer(0),
                  scopeRingBuffer.getReadPointer(0),
                  synthMixBuffer.getReadPointer(0),
                  synthMixBuffer.getReadPointer(1),
                  numSamples);
}

void MetaphysicalFactoryAudioProcessor::pushScopeData(const float* sumAB, const float* ringAB, const float* outL, const float* outR, int numSamples)
{
    const int available = scopeFifo.getFreeSpace();
    const int toWrite = juce::jmin(available, numSamples);

    if (toWrite <= 0)
        return;

    int start1, size1, start2, size2;
    scopeFifo.prepareToWrite(toWrite, start1, size1, start2, size2);

    if (size1 > 0)
    {
        std::copy(sumAB, sumAB + size1, scopeBufferSumAB.data() + start1);
        std::copy(ringAB, ringAB + size1, scopeBufferRingAB.data() + start1);
        std::copy(outL, outL + size1, scopeBufferOutL.data() + start1);
        std::copy(outR, outR + size1, scopeBufferOutR.data() + start1);
    }
    if (size2 > 0)
    {
        std::copy(sumAB + size1, sumAB + size1 + size2, scopeBufferSumAB.data() + start2);
        std::copy(ringAB + size1, ringAB + size1 + size2, scopeBufferRingAB.data() + start2);
        std::copy(outL + size1, outL + size1 + size2, scopeBufferOutL.data() + start2);
        std::copy(outR + size1, outR + size1 + size2, scopeBufferOutR.data() + start2);
    }

    scopeFifo.finishedWrite(toWrite);
}

int MetaphysicalFactoryAudioProcessor::readScopeData(float* destSumAB, float* destRingAB, float* destOutL, float* destOutR, int numSamplesToRead)
{
    const int ready = scopeFifo.getNumReady();
    const int toRead = juce::jmin(ready, numSamplesToRead);

    if (toRead <= 0)
        return 0;

    int start1, size1, start2, size2;
    scopeFifo.prepareToRead(toRead, start1, size1, start2, size2);

    if (size1 > 0)
    {
        std::copy(scopeBufferSumAB.data() + start1, scopeBufferSumAB.data() + start1 + size1, destSumAB);
        std::copy(scopeBufferRingAB.data() + start1, scopeBufferRingAB.data() + start1 + size1, destRingAB);
        std::copy(scopeBufferOutL.data() + start1, scopeBufferOutL.data() + start1 + size1, destOutL);
        std::copy(scopeBufferOutR.data() + start1, scopeBufferOutR.data() + start1 + size1, destOutR);
    }
    if (size2 > 0)
    {
        std::copy(scopeBufferSumAB.data() + start2, scopeBufferSumAB.data() + start2 + size2, destSumAB + size1);
        std::copy(scopeBufferRingAB.data() + start2, scopeBufferRingAB.data() + start2 + size2, destRingAB + size1);
        std::copy(scopeBufferOutL.data() + start2, scopeBufferOutL.data() + start2 + size2, destOutL + size1);
        std::copy(scopeBufferOutR.data() + start2, scopeBufferOutR.data() + start2 + size2, destOutR + size1);
    }

    scopeFifo.finishedRead(toRead);
    return toRead;
}

bool MetaphysicalFactoryAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MetaphysicalFactoryAudioProcessor::createEditor()
{
    return new MetaphysicalFactoryAudioProcessorEditor(*this);
}

void MetaphysicalFactoryAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // Save persistent sample directory path in state
    xml->setAttribute("lastSampleFolder", lastSampleDirectory.getFullPathName());

    copyXmlToBinary(*xml, destData);
}

void MetaphysicalFactoryAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }

        if (xmlState->hasAttribute("lastSampleFolder"))
        {
            const juce::File savedDir(xmlState->getStringAttribute("lastSampleFolder"));
            if (savedDir.isDirectory())
                lastSampleDirectory = savedDir;
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MetaphysicalFactoryAudioProcessor();
}