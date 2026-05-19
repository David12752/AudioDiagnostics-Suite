#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/LowBandFftAnalyzer.h"
#include "ipc/FileDiscoveryRegistry.h"
#include "ipc/IPCTransport.h"

#include <atomic>

class TheAnalyzerAudioProcessor final : public juce::AudioProcessor,
                                        private juce::Timer
{
public:
    TheAnalyzerAudioProcessor();
    ~TheAnalyzerAudioProcessor() override;

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
    [[nodiscard]] juce::String createDashboardSnapshotJson() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override;
    void ensureInstanceUuid();
    void updateLocalSpectralSnapshot() noexcept;

    juce::AudioProcessorValueTreeState apvts;
    gitpro::ipc::FileDiscoveryRegistry registry;
    gitpro::dsp::LowBandFftAnalyzer localLowBandAnalyzer;
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBlockSize { 0 };
    std::atomic<std::uint64_t> sequenceNumber { 0 };
    std::array<std::atomic<float>, gitpro::ipc::InstanceDescriptor::lowBandCount> latestLocalLowBandEnergiesDb;
    std::atomic<float> latestLocalLowBandTotalEnergyDb { -120.0f };
    std::atomic<float> latestLocalDominantLowFrequencyHz { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheAnalyzerAudioProcessor)
};