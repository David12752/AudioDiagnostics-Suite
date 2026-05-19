#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr const char* instanceUuidProperty = "instanceUuid";
    constexpr auto pdcPingDetectThreshold = 0.65f;
    constexpr auto pdcPingPreThreshold = 0.2f;
    constexpr auto pdcPingListenSeconds = 5.0f;

    float gainToDb(float gain) noexcept
    {
        return gain > 0.0f ? juce::Decibels::gainToDecibels(gain, -120.0f) : -120.0f;
    }

    std::int64_t getBlockStartSample(juce::AudioProcessor& processor, std::int64_t fallback) noexcept
    {
        if (auto* playHead = processor.getPlayHead())
        {
            if (const auto position = playHead->getPosition())
            {
                if (const auto timeInSamples = position->getTimeInSamples())
                    return *timeInSamples;
            }
        }

        return fallback;
    }

    struct MatchmakerResult
    {
        bool lowEndConflict = false;
        bool phaseDestruction = false;
        float clashScore = 0.0f;
        float maskingScore = 0.0f;
        float phaseCorrelation = 1.0f;
        float conflictFrequencyHz = 0.0f;
        int conflictBandIndex = -1;
        juce::String primaryProbeUuid;
        juce::String secondaryProbeUuid;
        juce::String advice = "Waiting for at least two active Probes.";
    };

    struct FxChainHealthResult
    {
        juce::String probeUuid;
        float probeCrestFactorDb = 0.0f;
        float analyzerCrestFactorDb = 0.0f;
        float crestFactorDeltaDb = 0.0f;
        bool heavilyCompressed = false;
        juce::String dynamicsWarning = "Dynamics look healthy.";
        float probeLowFrequencyCorrelation = 0.0f;
        float analyzerLowFrequencyCorrelation = 0.0f;
        float stereoCorrelationDelta = 0.0f;
        bool phaseWarning = false;
        juce::String stereoWarning = "Low-frequency stereo image looks stable.";
        std::uint64_t pdcRequestId = 0;
        bool pdcListening = false;
        bool pdcDetected = false;
        bool pdcDriftWarning = false;
        std::int64_t pdcMeasuredSamples = 0;
        float pdcMeasuredMilliseconds = 0.0f;
        juce::String pdcStatus = "Idle";
    };

    float wrapPhaseDifference(float phaseDifference) noexcept
    {
        while (phaseDifference > juce::MathConstants<float>::pi)
            phaseDifference -= juce::MathConstants<float>::twoPi;

        while (phaseDifference < -juce::MathConstants<float>::pi)
            phaseDifference += juce::MathConstants<float>::twoPi;

        return phaseDifference;
    }

    float bandCentreHz(std::size_t band) noexcept
    {
        return (gitpro::dsp::LowBandFftAnalyzer::bandLowerHz[band] + gitpro::dsp::LowBandFftAnalyzer::bandUpperHz[band]) * 0.5f;
    }

    MatchmakerResult computeMatchmaker(const std::vector<gitpro::ipc::InstanceDescriptor>& probes)
    {
        constexpr auto maskingThresholdDb = -54.0f;
        constexpr auto phaseThresholdDb = -60.0f;

        MatchmakerResult result;

        if (probes.size() < 2)
            return result;

        result.advice = "No low-end clash detected.";

        auto strongestOverlapDb = -120.0f;
        auto strongestPhaseConflict = 0.0f;

        for (std::size_t first = 0; first < probes.size(); ++first)
        {
            for (std::size_t second = first + 1; second < probes.size(); ++second)
            {
                const auto& firstProbe = probes[first];
                const auto& secondProbe = probes[second];

                for (std::size_t band = 0; band < gitpro::ipc::InstanceDescriptor::lowBandCount; ++band)
                {
                    const auto firstEnergy = firstProbe.lowBandEnergiesDb[band];
                    const auto secondEnergy = secondProbe.lowBandEnergiesDb[band];
                    const auto overlapDb = std::min(firstEnergy, secondEnergy);

                    if (overlapDb > strongestOverlapDb)
                    {
                        strongestOverlapDb = overlapDb;
                        result.conflictBandIndex = static_cast<int>(band);
                        result.conflictFrequencyHz = bandCentreHz(band);
                        result.primaryProbeUuid = juce::String(firstProbe.endpoint.toString());
                        result.secondaryProbeUuid = juce::String(secondProbe.endpoint.toString());
                    }

                    if (firstEnergy > maskingThresholdDb && secondEnergy > maskingThresholdDb)
                    {
                        result.lowEndConflict = true;
                        result.maskingScore = std::max(result.maskingScore, juce::jlimit(0.0f, 100.0f, (overlapDb - maskingThresholdDb) * 5.0f));
                    }

                    if (firstEnergy > phaseThresholdDb && secondEnergy > phaseThresholdDb)
                    {
                        const auto phaseDifference = wrapPhaseDifference(firstProbe.lowBandPhasesRadians[band] - secondProbe.lowBandPhasesRadians[band]);
                        const auto correlation = std::cos(phaseDifference);

                        if (correlation < result.phaseCorrelation)
                            result.phaseCorrelation = correlation;

                        if (correlation < 0.0f)
                        {
                            result.phaseDestruction = true;
                            strongestPhaseConflict = std::max(strongestPhaseConflict, -correlation * 100.0f);

                            if (result.conflictBandIndex < 0 || overlapDb >= strongestOverlapDb)
                            {
                                result.conflictBandIndex = static_cast<int>(band);
                                result.conflictFrequencyHz = bandCentreHz(band);
                                result.primaryProbeUuid = juce::String(firstProbe.endpoint.toString());
                                result.secondaryProbeUuid = juce::String(secondProbe.endpoint.toString());
                            }
                        }
                    }
                }
            }
        }

        result.clashScore = juce::jlimit(0.0f, 100.0f, std::max(result.maskingScore, strongestPhaseConflict));

        if (result.phaseDestruction)
            result.advice = "Phase cancellation around " + juce::String(result.conflictFrequencyHz, 0) + " Hz detected. Try flipping phase or nudging timing.";
        else if (result.lowEndConflict)
            result.advice = "Low-end masking around " + juce::String(result.conflictFrequencyHz, 0) + " Hz. Carve an EQ pocket or sidechain one source.";

        return result;
    }

    FxChainHealthResult computeFxChainHealth(const std::vector<gitpro::ipc::InstanceDescriptor>& probes,
                                             float analyzerCrestFactorDb,
                                             float analyzerLowFrequencyCorrelation,
                                             float sampleRate,
                                             std::uint64_t activePdcRequestId,
                                             bool pdcListening,
                                             std::uint64_t detectedPdcRequestId,
                                             std::int64_t detectedPdcSample)
    {
        FxChainHealthResult result;
        result.analyzerCrestFactorDb = analyzerCrestFactorDb;
        result.analyzerLowFrequencyCorrelation = analyzerLowFrequencyCorrelation;
        result.pdcRequestId = activePdcRequestId;
        result.pdcListening = pdcListening;

        if (probes.empty())
        {
            result.dynamicsWarning = "Waiting for a Probe before the FX chain.";
            result.stereoWarning = "Waiting for a Probe before the FX chain.";
            result.pdcStatus = activePdcRequestId == 0 ? "Idle" : "Waiting for Probe ping";
            return result;
        }

        const auto& probe = probes.front();
        result.probeUuid = juce::String(probe.endpoint.toString());
        result.probeCrestFactorDb = probe.crestFactorDb;
        result.crestFactorDeltaDb = probe.crestFactorDb - analyzerCrestFactorDb;
        result.heavilyCompressed = result.crestFactorDeltaDb >= 8.0f;

        if (result.crestFactorDeltaDb >= 10.0f)
            result.dynamicsWarning = "Squashed Dynamics";
        else if (result.heavilyCompressed)
            result.dynamicsWarning = "Heavily Compressed";

        result.probeLowFrequencyCorrelation = probe.lowFrequencyCorrelation;
        result.stereoCorrelationDelta = analyzerLowFrequencyCorrelation - probe.lowFrequencyCorrelation;
        result.phaseWarning = probe.lowFrequencyCorrelation >= 0.75f && analyzerLowFrequencyCorrelation < 0.2f;

        if (analyzerLowFrequencyCorrelation < -0.1f && probe.lowFrequencyCorrelation >= 0.75f)
            result.stereoWarning = "Phase-canceling stereo low end";
        else if (result.phaseWarning)
            result.stereoWarning = "Mono DI widened into unstable low end";

        if (activePdcRequestId == 0)
        {
            result.pdcStatus = "Idle";
            return result;
        }

        const auto probeHasInjected = probe.pdcPingRequestId == activePdcRequestId && probe.pdcPingInjectedSample >= 0;
        result.pdcDetected = detectedPdcRequestId == activePdcRequestId && detectedPdcSample >= 0;

        if (! probeHasInjected)
        {
            result.pdcStatus = "Waiting for Probe ping";
            return result;
        }

        if (! result.pdcDetected)
        {
            result.pdcStatus = pdcListening ? "Listening for return" : "Ping not detected";
            return result;
        }

        result.pdcMeasuredSamples = std::max<std::int64_t>(0, detectedPdcSample - probe.pdcPingInjectedSample);
        result.pdcMeasuredMilliseconds = sampleRate > 0.0f ? (static_cast<float>(result.pdcMeasuredSamples) * 1000.0f / sampleRate) : 0.0f;
        result.pdcDriftWarning = result.pdcMeasuredSamples > 0;
        result.pdcStatus = result.pdcDriftWarning ? "PDC Drift" : "Aligned";
        return result;
    }
}

TheAnalyzerAudioProcessor::TheAnalyzerAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "State", createParameterLayout())
{
    for (auto& energy : latestLocalLowBandEnergiesDb)
        energy.store(-120.0f, std::memory_order_relaxed);

    ensureInstanceUuid();
    registry.start(createInstanceDescriptor());
    cachedActiveProbes = registry.findActiveProbes();
    startTimerHz(60);
}

TheAnalyzerAudioProcessor::~TheAnalyzerAudioProcessor()
{
    stopTimer();
    registry.stop();
}

void TheAnalyzerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(samplesPerBlock, std::memory_order_relaxed);
    localLowBandAnalyzer.reset();
    processedSampleCounter = 0;
    localLowpassLeft = 0.0f;
    localLowpassRight = 0.0f;
    pdcPreviousMagnitude = 0.0f;
    performanceTimerTick = 0;
    meterFifo.reset();
    registry.announce(createInstanceDescriptor());
}

void TheAnalyzerAudioProcessor::releaseResources()
{
}

bool TheAnalyzerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    return mainInput == mainOutput && (! mainInput.isDisabled());
}

void TheAnalyzerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();
    updateLocalMeterSnapshot(buffer);
    detectPdcPing(buffer);
    localLowBandAnalyzer.pushBlock(buffer);
    processedSampleCounter += buffer.getNumSamples();
    sequenceNumber.fetch_add(1, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* TheAnalyzerAudioProcessor::createEditor()
{
    return new TheAnalyzerAudioProcessorEditor(*this);
}

bool TheAnalyzerAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String TheAnalyzerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TheAnalyzerAudioProcessor::acceptsMidi() const
{
    return false;
}

bool TheAnalyzerAudioProcessor::producesMidi() const
{
    return false;
}

bool TheAnalyzerAudioProcessor::isMidiEffect() const
{
    return false;
}

double TheAnalyzerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TheAnalyzerAudioProcessor::getNumPrograms()
{
    return 1;
}

int TheAnalyzerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TheAnalyzerAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String TheAnalyzerAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void TheAnalyzerAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void TheAnalyzerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void TheAnalyzerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }

    ensureInstanceUuid();
    registry.announce(createInstanceDescriptor());
}

juce::String TheAnalyzerAudioProcessor::getInstanceUuid() const
{
    return apvts.state.getProperty(instanceUuidProperty).toString();
}

gitpro::ipc::InstanceDescriptor TheAnalyzerAudioProcessor::createInstanceDescriptor() const
{
    gitpro::ipc::InstanceDescriptor descriptor;
    descriptor.endpoint = gitpro::ipc::EndpointId::fromString(getInstanceUuid().toStdString());
    descriptor.role = gitpro::ipc::PluginRole::analyzer;
    descriptor.displayName = "The Analyzer";
    descriptor.hostName = juce::PluginHostType().getHostDescription();
    descriptor.trackName = "Analyzer Track";
    descriptor.sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    descriptor.blockSize = static_cast<std::uint32_t>(std::max(0, currentBlockSize.load(std::memory_order_relaxed)));
    descriptor.supportedTransports = { gitpro::ipc::TransportKind::loopback, gitpro::ipc::TransportKind::sharedMemory };
    descriptor.heartbeatCounter = sequenceNumber.load(std::memory_order_relaxed);
    descriptor.peakDbfs = latestLocalPeakDbfs.load(std::memory_order_relaxed);
    descriptor.rmsDbfs = latestLocalRmsDbfs.load(std::memory_order_relaxed);
    descriptor.crestFactorDb = latestLocalCrestFactorDb.load(std::memory_order_relaxed);
    descriptor.lowBandTotalEnergyDb = latestLocalLowBandTotalEnergyDb.load(std::memory_order_relaxed);
    descriptor.dominantLowFrequencyHz = latestLocalDominantLowFrequencyHz.load(std::memory_order_relaxed);
    descriptor.lowFrequencyCorrelation = latestLocalLowFrequencyCorrelation.load(std::memory_order_relaxed);
    return descriptor;
}

juce::String TheAnalyzerAudioProcessor::createDashboardSnapshotJson() const
{
    const auto& activeProbes = cachedActiveProbes;
    const auto matchmaker = computeMatchmaker(activeProbes);
    const auto sampleRate = static_cast<float>(currentSampleRate.load(std::memory_order_relaxed));
    const auto fxHealth = computeFxChainHealth(activeProbes,
                                               latestLocalCrestFactorDb.load(std::memory_order_relaxed),
                                               latestLocalLowFrequencyCorrelation.load(std::memory_order_relaxed),
                                               sampleRate,
                                               activePdcPingRequestId.load(std::memory_order_relaxed),
                                               pdcPingArmed.load(std::memory_order_relaxed),
                                               detectedPdcPingRequestId.load(std::memory_order_relaxed),
                                               detectedPdcPingSample.load(std::memory_order_relaxed));

    juce::Array<juce::var> probeArray;
    juce::Array<juce::var> firstProbeLowBandEnergies;

    for (const auto& probe : activeProbes)
    {
        auto* probeObject = new juce::DynamicObject();
        probeObject->setProperty("uuid", juce::String(probe.endpoint.toString()));
        probeObject->setProperty("displayName", juce::String(probe.displayName));
        probeObject->setProperty("hostName", juce::String(probe.hostName));
        probeObject->setProperty("trackName", juce::String(probe.trackName));
        probeObject->setProperty("sampleRate", probe.sampleRate);
        probeObject->setProperty("blockSize", static_cast<int>(probe.blockSize));
        probeObject->setProperty("peakDbfs", probe.peakDbfs);
        probeObject->setProperty("rmsDbfs", probe.rmsDbfs);
        probeObject->setProperty("crestFactorDb", probe.crestFactorDb);
        probeObject->setProperty("noiseFloorDbfs", probe.noiseFloorDbfs);
        probeObject->setProperty("snrDb", probe.snrDb);
        probeObject->setProperty("lowBandTotalEnergyDb", probe.lowBandTotalEnergyDb);
        probeObject->setProperty("dominantLowFrequencyHz", probe.dominantLowFrequencyHz);
        probeObject->setProperty("dominantLowBandIndex", probe.dominantLowBandIndex);
        probeObject->setProperty("lowFrequencyCorrelation", probe.lowFrequencyCorrelation);
        probeObject->setProperty("pdcPingRequestId", static_cast<double>(probe.pdcPingRequestId));
        probeObject->setProperty("pdcPingInjectedSample", static_cast<double>(probe.pdcPingInjectedSample));
        probeObject->setProperty("heartbeatCounter", static_cast<double>(probe.heartbeatCounter));

        juce::Array<juce::var> lowBands;

        for (std::size_t band = 0; band < gitpro::ipc::InstanceDescriptor::lowBandCount; ++band)
        {
            auto* bandObject = new juce::DynamicObject();
            bandObject->setProperty("index", static_cast<int>(band));
            bandObject->setProperty("lowerHz", gitpro::dsp::LowBandFftAnalyzer::bandLowerHz[band]);
            bandObject->setProperty("upperHz", gitpro::dsp::LowBandFftAnalyzer::bandUpperHz[band]);
            bandObject->setProperty("energyDb", probe.lowBandEnergiesDb[band]);
            bandObject->setProperty("phaseRadians", probe.lowBandPhasesRadians[band]);
            lowBands.add(juce::var(bandObject));

            if (firstProbeLowBandEnergies.size() < static_cast<int>(gitpro::ipc::InstanceDescriptor::lowBandCount))
                firstProbeLowBandEnergies.add(probe.lowBandEnergiesDb[band]);
        }

        probeObject->setProperty("lowBands", lowBands);
        probeArray.add(juce::var(probeObject));
    }

    juce::Array<juce::var> localLowBands;
    juce::Array<juce::var> localLowBandEnergies;

    for (std::size_t band = 0; band < gitpro::ipc::InstanceDescriptor::lowBandCount; ++band)
    {
        const auto energyDb = latestLocalLowBandEnergiesDb[band].load(std::memory_order_relaxed);
        auto* bandObject = new juce::DynamicObject();
        bandObject->setProperty("index", static_cast<int>(band));
        bandObject->setProperty("lowerHz", gitpro::dsp::LowBandFftAnalyzer::bandLowerHz[band]);
        bandObject->setProperty("upperHz", gitpro::dsp::LowBandFftAnalyzer::bandUpperHz[band]);
        bandObject->setProperty("energyDb", energyDb);
        localLowBands.add(juce::var(bandObject));
        localLowBandEnergies.add(energyDb);
    }

    auto* matchmakerObject = new juce::DynamicObject();
    matchmakerObject->setProperty("lowEndConflict", matchmaker.lowEndConflict);
    matchmakerObject->setProperty("phaseDestruction", matchmaker.phaseDestruction);
    matchmakerObject->setProperty("clashScore", matchmaker.clashScore);
    matchmakerObject->setProperty("maskingScore", matchmaker.maskingScore);
    matchmakerObject->setProperty("phaseCorrelation", matchmaker.phaseCorrelation);
    matchmakerObject->setProperty("conflictFrequencyHz", matchmaker.conflictFrequencyHz);
    matchmakerObject->setProperty("conflictBandIndex", matchmaker.conflictBandIndex);
    matchmakerObject->setProperty("primaryProbeUuid", matchmaker.primaryProbeUuid);
    matchmakerObject->setProperty("secondaryProbeUuid", matchmaker.secondaryProbeUuid);
    matchmakerObject->setProperty("advice", matchmaker.advice);

    auto* localSpectrumObject = new juce::DynamicObject();
    localSpectrumObject->setProperty("peakDbfs", latestLocalPeakDbfs.load(std::memory_order_relaxed));
    localSpectrumObject->setProperty("rmsDbfs", latestLocalRmsDbfs.load(std::memory_order_relaxed));
    localSpectrumObject->setProperty("crestFactorDb", latestLocalCrestFactorDb.load(std::memory_order_relaxed));
    localSpectrumObject->setProperty("lowBandTotalEnergyDb", latestLocalLowBandTotalEnergyDb.load(std::memory_order_relaxed));
    localSpectrumObject->setProperty("dominantLowFrequencyHz", latestLocalDominantLowFrequencyHz.load(std::memory_order_relaxed));
    localSpectrumObject->setProperty("lowFrequencyCorrelation", latestLocalLowFrequencyCorrelation.load(std::memory_order_relaxed));
    localSpectrumObject->setProperty("lowBands", localLowBands);

    auto* realtimeObject = new juce::DynamicObject();
    realtimeObject->setProperty("peakDbfs", latestLocalPeakDbfs.load(std::memory_order_relaxed));
    realtimeObject->setProperty("rmsDbfs", latestLocalRmsDbfs.load(std::memory_order_relaxed));
    realtimeObject->setProperty("crestFactorDb", latestLocalCrestFactorDb.load(std::memory_order_relaxed));
    realtimeObject->setProperty("lowFrequencyCorrelation", latestLocalLowFrequencyCorrelation.load(std::memory_order_relaxed));
    realtimeObject->setProperty("localLowBandEnergiesDb", localLowBandEnergies);
    realtimeObject->setProperty("probeLowBandEnergiesDb", firstProbeLowBandEnergies);

    auto* fxHealthObject = new juce::DynamicObject();
    fxHealthObject->setProperty("probeUuid", fxHealth.probeUuid);
    fxHealthObject->setProperty("probeCrestFactorDb", fxHealth.probeCrestFactorDb);
    fxHealthObject->setProperty("analyzerCrestFactorDb", fxHealth.analyzerCrestFactorDb);
    fxHealthObject->setProperty("crestFactorDeltaDb", fxHealth.crestFactorDeltaDb);
    fxHealthObject->setProperty("heavilyCompressed", fxHealth.heavilyCompressed);
    fxHealthObject->setProperty("dynamicsWarning", fxHealth.dynamicsWarning);
    fxHealthObject->setProperty("probeLowFrequencyCorrelation", fxHealth.probeLowFrequencyCorrelation);
    fxHealthObject->setProperty("analyzerLowFrequencyCorrelation", fxHealth.analyzerLowFrequencyCorrelation);
    fxHealthObject->setProperty("stereoCorrelationDelta", fxHealth.stereoCorrelationDelta);
    fxHealthObject->setProperty("phaseWarning", fxHealth.phaseWarning);
    fxHealthObject->setProperty("stereoWarning", fxHealth.stereoWarning);
    fxHealthObject->setProperty("pdcRequestId", static_cast<double>(fxHealth.pdcRequestId));
    fxHealthObject->setProperty("pdcListening", fxHealth.pdcListening);
    fxHealthObject->setProperty("pdcDetected", fxHealth.pdcDetected);
    fxHealthObject->setProperty("pdcDriftWarning", fxHealth.pdcDriftWarning);
    fxHealthObject->setProperty("pdcMeasuredSamples", static_cast<double>(fxHealth.pdcMeasuredSamples));
    fxHealthObject->setProperty("pdcMeasuredMilliseconds", fxHealth.pdcMeasuredMilliseconds);
    fxHealthObject->setProperty("pdcStatus", fxHealth.pdcStatus);

    auto* object = new juce::DynamicObject();
    object->setProperty("instanceUuid", getInstanceUuid());
    object->setProperty("role", "analyzer");
    object->setProperty("activeProbeCount", static_cast<int>(activeProbes.size()));
    object->setProperty("transport", "registry");
    object->setProperty("sequenceNumber", static_cast<double>(sequenceNumber.load(std::memory_order_relaxed)));
    object->setProperty("probes", probeArray);
    object->setProperty("matchmaker", juce::var(matchmakerObject));
    object->setProperty("localSpectrum", juce::var(localSpectrumObject));
    object->setProperty("fxChainHealth", juce::var(fxHealthObject));
    object->setProperty("realtime", juce::var(realtimeObject));

    return juce::JSON::toString(juce::var(object), true);
}

void TheAnalyzerAudioProcessor::handleDashboardCommand(const juce::String& commandJson)
{
    const auto parsed = juce::JSON::parse(commandJson);
    const auto* object = parsed.getDynamicObject();

    if (object == nullptr)
        return;

    if (object->getProperty("command").toString() == "measurePdcLatency")
        requestPdcLatencyTest();
}

juce::AudioProcessorValueTreeState::ParameterLayout TheAnalyzerAudioProcessor::createParameterLayout()
{
    return {};
}

void TheAnalyzerAudioProcessor::ensureInstanceUuid()
{
    if (! apvts.state.hasProperty(instanceUuidProperty) || apvts.state.getProperty(instanceUuidProperty).toString().isEmpty())
        apvts.state.setProperty(instanceUuidProperty, juce::Uuid().toString(), nullptr);
}

void TheAnalyzerAudioProcessor::timerCallback()
{
    ++performanceTimerTick;
    flushMeterSnapshots();
    updateLocalSpectralSnapshot();

    if (performanceTimerTick % registryRefreshDivider == 0)
    {
        cachedActiveProbes = registry.findActiveProbes();
        registry.announce(createInstanceDescriptor());
    }
}

void TheAnalyzerAudioProcessor::updateLocalMeterSnapshot(const juce::AudioBuffer<float>& buffer) noexcept
{
    constexpr auto silenceFloorLinear = 0.000001f;
    constexpr auto lowpassAlpha = 0.04f;

    auto peak = 0.0f;
    auto sumSquares = 0.0;
    auto lowLeftSquares = 0.0f;
    auto lowRightSquares = 0.0f;
    auto lowCross = 0.0f;
    auto sampleCount = 0;

    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);

        for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto magnitude = std::abs(samples[sample]);
            peak = std::max(peak, magnitude);
            sumSquares += static_cast<double>(samples[sample]) * static_cast<double>(samples[sample]);
            ++sampleCount;
        }
    }

    if (buffer.getNumChannels() >= 2)
    {
        const auto* left = buffer.getReadPointer(0);
        const auto* right = buffer.getReadPointer(1);

        for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            localLowpassLeft += lowpassAlpha * (left[sample] - localLowpassLeft);
            localLowpassRight += lowpassAlpha * (right[sample] - localLowpassRight);
            lowLeftSquares += localLowpassLeft * localLowpassLeft;
            lowRightSquares += localLowpassRight * localLowpassRight;
            lowCross += localLowpassLeft * localLowpassRight;
        }
    }

    const auto rms = sampleCount > 0 ? static_cast<float>(std::sqrt(sumSquares / static_cast<double>(sampleCount))) : 0.0f;
    const auto peakDb = gainToDb(peak);
    const auto rmsDb = gainToDb(rms);
    const auto crestFactor = rms > silenceFloorLinear ? juce::jlimit(0.0f, 60.0f, peakDb - rmsDb) : 0.0f;

    RealtimeMeterSnapshot snapshot;
    snapshot.peakDbfs = peakDb;
    snapshot.rmsDbfs = rmsDb;
    snapshot.crestFactorDb = crestFactor;
    snapshot.lowFrequencyCorrelation = latestLocalLowFrequencyCorrelation.load(std::memory_order_relaxed);

    if (lowLeftSquares > 0.00000001f && lowRightSquares > 0.00000001f)
    {
        const auto correlation = lowCross / std::sqrt(lowLeftSquares * lowRightSquares);
        snapshot.lowFrequencyCorrelation = juce::jlimit(-1.0f, 1.0f, correlation);
    }

    pushMeterSnapshot(snapshot);
}

void TheAnalyzerAudioProcessor::pushMeterSnapshot(const RealtimeMeterSnapshot& snapshot) noexcept
{
    auto start1 = 0;
    auto size1 = 0;
    auto start2 = 0;
    auto size2 = 0;
    meterFifo.prepareToWrite(1, start1, size1, start2, size2);

    if (size1 > 0)
        meterSnapshots[static_cast<std::size_t>(start1)] = snapshot;
    else if (size2 > 0)
        meterSnapshots[static_cast<std::size_t>(start2)] = snapshot;

    meterFifo.finishedWrite(size1 + size2);
}

void TheAnalyzerAudioProcessor::flushMeterSnapshots() noexcept
{
    auto peakDbfs = -120.0f;
    auto rmsLinearSum = 0.0f;
    auto crestSum = 0.0f;
    auto correlationSum = 0.0f;
    auto consumed = 0;

    for (;;)
    {
        auto start1 = 0;
        auto size1 = 0;
        auto start2 = 0;
        auto size2 = 0;
        meterFifo.prepareToRead(1, start1, size1, start2, size2);

        if (size1 <= 0 && size2 <= 0)
            break;

        const auto& snapshot = meterSnapshots[static_cast<std::size_t>(size1 > 0 ? start1 : start2)];
        peakDbfs = std::max(peakDbfs, snapshot.peakDbfs);
        rmsLinearSum += juce::Decibels::decibelsToGain(snapshot.rmsDbfs);
        crestSum += snapshot.crestFactorDb;
        correlationSum += snapshot.lowFrequencyCorrelation;
        ++consumed;
        meterFifo.finishedRead(size1 + size2);
    }

    if (consumed <= 0)
        return;

    latestLocalPeakDbfs.store(peakDbfs, std::memory_order_relaxed);
    latestLocalRmsDbfs.store(gainToDb(rmsLinearSum / static_cast<float>(consumed)), std::memory_order_relaxed);
    latestLocalCrestFactorDb.store(crestSum / static_cast<float>(consumed), std::memory_order_relaxed);
    latestLocalLowFrequencyCorrelation.store(correlationSum / static_cast<float>(consumed), std::memory_order_relaxed);
}

void TheAnalyzerAudioProcessor::detectPdcPing(const juce::AudioBuffer<float>& buffer) noexcept
{
    if (! pdcPingArmed.load(std::memory_order_acquire))
        return;

    const auto requestId = activePdcPingRequestId.load(std::memory_order_relaxed);
    const auto blockStartSample = getBlockStartSample(*this, processedSampleCounter);
    auto detectedSampleOffset = -1;

    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto magnitude = 0.0f;

        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            magnitude = std::max(magnitude, std::abs(buffer.getReadPointer(channel)[sample]));

        if (magnitude >= pdcPingDetectThreshold && pdcPreviousMagnitude <= pdcPingPreThreshold)
        {
            detectedSampleOffset = sample;
            pdcPreviousMagnitude = magnitude;
            break;
        }

        pdcPreviousMagnitude = magnitude;
    }

    if (detectedSampleOffset >= 0)
    {
        detectedPdcPingRequestId.store(requestId, std::memory_order_release);
        detectedPdcPingSample.store(blockStartSample + detectedSampleOffset, std::memory_order_release);
        pdcPingArmed.store(false, std::memory_order_release);
        pdcPingRemainingSamples.store(0, std::memory_order_release);
        return;
    }

    const auto remaining = pdcPingRemainingSamples.load(std::memory_order_relaxed) - buffer.getNumSamples();
    pdcPingRemainingSamples.store(std::max(0, remaining), std::memory_order_release);

    if (remaining <= 0)
        pdcPingArmed.store(false, std::memory_order_release);
}

void TheAnalyzerAudioProcessor::updateLocalSpectralSnapshot() noexcept
{
    gitpro::dsp::LowBandSpectralMetrics metrics;

    if (! localLowBandAnalyzer.analyzeIfReady(currentSampleRate.load(std::memory_order_relaxed), metrics))
        return;

    for (std::size_t band = 0; band < gitpro::ipc::InstanceDescriptor::lowBandCount; ++band)
        latestLocalLowBandEnergiesDb[band].store(metrics.bandEnergyDb[band], std::memory_order_relaxed);

    latestLocalLowBandTotalEnergyDb.store(metrics.lowBandTotalEnergyDb, std::memory_order_relaxed);
    latestLocalDominantLowFrequencyHz.store(metrics.dominantFrequencyHz, std::memory_order_relaxed);
}

void TheAnalyzerAudioProcessor::requestPdcLatencyTest()
{
    const auto requestId = nextPdcPingRequestId.fetch_add(1, std::memory_order_acq_rel);
    gitpro::ipc::PdcPingRequest request;
    request.requestId = requestId;
    request.analyzerEndpoint = gitpro::ipc::EndpointId::fromString(getInstanceUuid().toStdString());
    request.issuedMilliseconds = static_cast<std::uint64_t>(juce::Time::getMillisecondCounter());

    if (! registry.publishPdcPingRequest(request))
        return;

    const auto sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    const auto listenSamples = static_cast<int>(std::max(1.0, (sampleRate > 0.0 ? sampleRate : 48000.0) * static_cast<double>(pdcPingListenSeconds)));
    activePdcPingRequestId.store(requestId, std::memory_order_release);
    detectedPdcPingRequestId.store(0, std::memory_order_release);
    detectedPdcPingSample.store(-1, std::memory_order_release);
    pdcPingRemainingSamples.store(listenSamples, std::memory_order_release);
    pdcPingArmed.store(true, std::memory_order_release);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheAnalyzerAudioProcessor();
}