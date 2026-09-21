#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "DSP/OscillatorBank.h"
#include "DSP/Resochord.h"
#include "DSP/FilterRingMod.h"
#include "DSP/SampleLooper.h"
#include "DSP/SpinDelay.h"
#include "DSP/SpaceReverb.h"
#include "DSP/MasterEq.h"
#include "DSP/MotionEngine.h"

class MetaphysicalFactoryAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int ScopeFifoCapacity = 4096;

    MetaphysicalFactoryAudioProcessor();
    ~MetaphysicalFactoryAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Eliminate -Woverloaded-virtual warning for AudioProcessor::processBlock
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter layout construction
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Accessors for UI and Subsystems
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    MotionEngine& getMotionEngine()                { return motionEngine; }
    SampleLooper& getSampleLooper()                { return sampleLooper; }
    juce::AudioFormatManager& getFormatManager()   { return formatManager; }

    // Persistent sample folder memory
    juce::File getLastSampleDirectory() const      { return lastSampleDirectory; }
    void setLastSampleDirectory(const juce::File& dir) { lastSampleDirectory = dir; }

    // 4-Channel Telemetry Stream for 3D Lissajous Scope, LUFS Meter & Phase Correlation
    int readScopeData(float* destSumAB, float* destRingAB, float* destOutL, float* destOutR, int numSamplesToRead);

private:
    juce::AudioProcessorValueTreeState apvts;

    // Core DSP Modules
    OscillatorBank oscBankA;
    OscillatorBank oscBankB;
    FilterRingMod filterRingMod;
    SampleLooper sampleLooper;
    Resochord resochord;
    SpinDelay spinDelay;
    SpaceReverb spaceReverb;
    MasterEq masterEq;
    MotionEngine motionEngine;

    // Audio Format Reader for Looper
    juce::AudioFormatManager formatManager;

    // Persistent sample directory memory
    juce::File lastSampleDirectory;

    // Internal scratch buffers for modular summing
    juce::AudioBuffer<float> bankABuffer;
    juce::AudioBuffer<float> bankBBuffer;
    juce::AudioBuffer<float> synthMixBuffer;
    juce::AudioBuffer<float> scopeSumBuffer;
    juce::AudioBuffer<float> scopeRingBuffer;

    // Lock-Free 4-Channel Audio FIFO for Central OLED Matrix
    juce::AbstractFifo scopeFifo { ScopeFifoCapacity };
    std::vector<float> scopeBufferSumAB;
    std::vector<float> scopeBufferRingAB;
    std::vector<float> scopeBufferOutL;
    std::vector<float> scopeBufferOutR;

    void pushScopeData(const float* sumAB, const float* ringAB, const float* outL, const float* outR, int numSamples);

    // MIDI Note tracking for Metaphysical Fabrications tuning
    std::atomic<float> currentMidiPitchHz { 110.0f };
    std::atomic<bool> isMidiActive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetaphysicalFactoryAudioProcessor)
};