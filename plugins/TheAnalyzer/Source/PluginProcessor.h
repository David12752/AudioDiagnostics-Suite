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
    void handleDashboardCommand(const juce::String& commandJson);

private:
    struct RealtimeMeterSnapshot
    {
        float peakDbfs = -120.0f;
        float rmsDbfs = -120.0f;
        float crestFactorDb = 0.0f;
        float lowFrequencyCorrelation = 0.0f;
    };

    static constexpr int meterFifoCapacity = 128;
    static constexpr int registryRefreshDivider = 10;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override;
    void ensureInstanceUuid();
    void updateLocalMeterSnapshot(const juce::AudioBuffer<float>& buffer) noexcept;
    void pushMeterSnapshot(const RealtimeMeterSnapshot& snapshot) noexcept;
    void flushMeterSnapshots() noexcept;
    void detectPdcPing(const juce::AudioBuffer<float>& buffer) noexcept;
    void updateLocalSpectralSnapshot() noexcept;
    void requestPdcLatencyTest();

    juce::AudioProcessorValueTreeState apvts;
    gitpro::ipc::FileDiscoveryRegistry registry;
    gitpro::dsp::LowBandFftAnalyzer localLowBandAnalyzer;
    juce::AbstractFifo meterFifo { meterFifoCapacity };
    std::array<RealtimeMeterSnapshot, meterFifoCapacity> meterSnapshots;
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBlockSize { 0 };
    std::atomic<std::uint64_t> sequenceNumber { 0 };
    std::atomic<float> latestLocalPeakDbfs { -120.0f };
    std::atomic<float> latestLocalRmsDbfs { -120.0f };
    std::atomic<float> latestLocalCrestFactorDb { 0.0f };
    std::atomic<float> latestLocalLowFrequencyCorrelation { 0.0f };
    std::array<std::atomic<float>, gitpro::ipc::InstanceDescriptor::lowBandCount> latestLocalLowBandEnergiesDb;
    std::atomic<float> latestLocalLowBandTotalEnergyDb { -120.0f };
    std::atomic<float> latestLocalDominantLowFrequencyHz { 0.0f };
    std::atomic<std::uint64_t> nextPdcPingRequestId { 1 };
    std::atomic<std::uint64_t> activePdcPingRequestId { 0 };
    std::atomic<std::uint64_t> detectedPdcPingRequestId { 0 };
    std::atomic<std::int64_t> detectedPdcPingSample { -1 };
    std::atomic<int> pdcPingRemainingSamples { 0 };
    std::atomic<bool> pdcPingArmed { false };
    std::vector<gitpro::ipc::InstanceDescriptor> cachedActiveProbes;
    std::int64_t processedSampleCounter = 0;
    int performanceTimerTick = 0;
    float localLowpassLeft = 0.0f;
    float localLowpassRight = 0.0f;
    float pdcPreviousMagnitude = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheAnalyzerAudioProcessor)
};