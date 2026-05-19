#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/LowBandFftAnalyzer.h"
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
    void handleDashboardCommand(const juce::String& commandJson);

private:
    struct RealtimeMeterSnapshot
    {
        float peakDbfs = -120.0f;
        float rmsDbfs = -120.0f;
        float crestFactorDb = 0.0f;
        float noiseFloorDbfs = -120.0f;
        float snrDb = 0.0f;
        float lowFrequencyCorrelation = 0.0f;
    };

    static constexpr int meterFifoCapacity = 128;
    static constexpr int registryAnnounceDivider = 10;
    static constexpr int pdcPollDivider = 6;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override;
    void ensureInstanceUuid();
    void updateMeterSnapshot(const juce::AudioBuffer<float>& buffer) noexcept;
    void pushMeterSnapshot(const RealtimeMeterSnapshot& snapshot) noexcept;
    void flushMeterSnapshots() noexcept;
    void updatePitchDetector(const juce::AudioBuffer<float>& buffer) noexcept;
    void updateAutoGainAnalysis(const juce::AudioBuffer<float>& buffer) noexcept;
    void applyGainCompensation(juce::AudioBuffer<float>& buffer) noexcept;
    void injectPendingPdcPing(juce::AudioBuffer<float>& buffer) noexcept;
    void pollPdcPingRequest();
    void updateSpectralSnapshot() noexcept;
    void requestAutoAnalyze(float seconds) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    gitpro::ipc::FileDiscoveryRegistry registry;
    gitpro::dsp::LowBandFftAnalyzer lowBandAnalyzer;
    juce::AbstractFifo meterFifo { meterFifoCapacity };
    std::array<RealtimeMeterSnapshot, meterFifoCapacity> meterSnapshots;
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBlockSize { 0 };
    std::atomic<std::uint64_t> sequenceNumber { 0 };
    std::atomic<float> latestPeakDbfs { -120.0f };
    std::atomic<float> latestRmsDbfs { -120.0f };
    std::atomic<float> latestCrestFactorDb { 0.0f };
    std::atomic<float> latestNoiseFloorDbfs { -120.0f };
    std::atomic<float> latestSnrDb { 0.0f };
    std::array<std::atomic<float>, gitpro::ipc::InstanceDescriptor::lowBandCount> latestLowBandEnergiesDb;
    std::array<std::atomic<float>, gitpro::ipc::InstanceDescriptor::lowBandCount> latestLowBandPhasesRadians;
    std::atomic<float> latestLowBandTotalEnergyDb { -120.0f };
    std::atomic<float> latestDominantLowFrequencyHz { 0.0f };
    std::atomic<int> latestDominantLowBandIndex { -1 };
    std::atomic<float> latestLowFrequencyCorrelation { 0.0f };
    std::atomic<float> latestPitchFrequencyHz { 0.0f };
    std::atomic<float> targetPeakDbfs { -12.0f };
    std::atomic<float> interfaceMaxInputDbU { 18.0f };
    std::atomic<float> autoGainDb { 0.0f };
    std::atomic<float> measuredCalibrationPeakDbfs { -120.0f };
    std::atomic<float> requiredGainOffsetDb { 0.0f };
    std::atomic<int> autoAnalyzeRemainingSamples { 0 };
    std::atomic<int> autoAnalyzeTotalSamples { 0 };
    std::atomic<int> autoAnalyzeRequestedSamples { 0 };
    std::atomic<bool> autoAnalyzeResetRequested { false };
    std::atomic<bool> autoAnalyzeActive { false };
    std::atomic<std::uint64_t> pendingPdcPingRequestId { 0 };
    std::atomic<std::uint64_t> latestPdcPingRequestId { 0 };
    std::atomic<std::int64_t> latestPdcPingInjectedSample { -1 };
    std::uint64_t lastSeenPdcPingRequestId = 0;
    std::int64_t processedSampleCounter = 0;
    int performanceTimerTick = 0;
    float noiseFloorLinear = 0.00001f;
    float activeSignalLinear = 0.00001f;
    float lowpassLeft = 0.0f;
    float lowpassRight = 0.0f;
    float pitchLowpassState = 0.0f;
    bool pitchWasAboveThreshold = false;
    int pitchSamplesSinceCrossing = 0;
    float smoothedPitchHz = 0.0f;
    float autoAnalyzePeakLinear = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheProbeAudioProcessor)
};