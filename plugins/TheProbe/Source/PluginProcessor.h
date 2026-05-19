#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ipc/FileDiscoveryRegistry.h"
#include "ipc/IPCTransport.h"

#include <atomic>

class TheProbeAudioProcessor final : public juce::AudioProcessor,
                                     private juce::Timer
{
public:
    TheProbeAudioProcessor();
    ~TheProbeAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

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

    [[nodiscard]] juce::String getInstanceUuid() const;
    [[nodiscard]] gitpro::ipc::InstanceDescriptor createInstanceDescriptor() const;
    [[nodiscard]] juce::String createStatusJson() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override;
    void ensureInstanceUuid();
    void updateMeterSnapshot(const juce::AudioBuffer<float>& buffer) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    gitpro::ipc::FileDiscoveryRegistry registry;
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBlockSize { 0 };
    std::atomic<std::uint64_t> sequenceNumber { 0 };
    std::atomic<float> latestPeakDbfs { -120.0f };
    std::atomic<float> latestRmsDbfs { -120.0f };
    std::atomic<float> latestNoiseFloorDbfs { -120.0f };
    std::atomic<float> latestSnrDb { 0.0f };
    float noiseFloorLinear = 0.00001f;
    float activeSignalLinear = 0.00001f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheProbeAudioProcessor)
};