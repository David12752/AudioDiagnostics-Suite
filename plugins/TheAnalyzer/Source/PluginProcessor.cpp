#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr const char* instanceUuidProperty = "instanceUuid";

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
    startTimerHz(1);
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
    localLowBandAnalyzer.pushBlock(buffer);
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
    return descriptor;
}

juce::String TheAnalyzerAudioProcessor::createDashboardSnapshotJson() const
{
    auto activeProbes = registry.findActiveProbes();
    const auto matchmaker = computeMatchmaker(activeProbes);

    juce::Array<juce::var> probeArray;

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
        probeObject->setProperty("noiseFloorDbfs", probe.noiseFloorDbfs);
        probeObject->setProperty("snrDb", probe.snrDb);
        probeObject->setProperty("lowBandTotalEnergyDb", probe.lowBandTotalEnergyDb);
        probeObject->setProperty("dominantLowFrequencyHz", probe.dominantLowFrequencyHz);
        probeObject->setProperty("dominantLowBandIndex", probe.dominantLowBandIndex);
        probeObject->setProperty("lowFrequencyCorrelation", probe.lowFrequencyCorrelation);
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
        }

        probeObject->setProperty("lowBands", lowBands);
        probeArray.add(juce::var(probeObject));
    }

    juce::Array<juce::var> localLowBands;

    for (std::size_t band = 0; band < gitpro::ipc::InstanceDescriptor::lowBandCount; ++band)
    {
        auto* bandObject = new juce::DynamicObject();
        bandObject->setProperty("index", static_cast<int>(band));
        bandObject->setProperty("lowerHz", gitpro::dsp::LowBandFftAnalyzer::bandLowerHz[band]);
        bandObject->setProperty("upperHz", gitpro::dsp::LowBandFftAnalyzer::bandUpperHz[band]);
        bandObject->setProperty("energyDb", latestLocalLowBandEnergiesDb[band].load(std::memory_order_relaxed));
        localLowBands.add(juce::var(bandObject));
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
    localSpectrumObject->setProperty("lowBandTotalEnergyDb", latestLocalLowBandTotalEnergyDb.load(std::memory_order_relaxed));
    localSpectrumObject->setProperty("dominantLowFrequencyHz", latestLocalDominantLowFrequencyHz.load(std::memory_order_relaxed));
    localSpectrumObject->setProperty("lowBands", localLowBands);

    auto* object = new juce::DynamicObject();
    object->setProperty("instanceUuid", getInstanceUuid());
    object->setProperty("role", "analyzer");
    object->setProperty("activeProbeCount", static_cast<int>(activeProbes.size()));
    object->setProperty("transport", "registry");
    object->setProperty("sequenceNumber", static_cast<double>(sequenceNumber.load(std::memory_order_relaxed)));
    object->setProperty("probes", probeArray);
    object->setProperty("matchmaker", juce::var(matchmakerObject));
    object->setProperty("localSpectrum", juce::var(localSpectrumObject));

    return juce::JSON::toString(juce::var(object), true);
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
    updateLocalSpectralSnapshot();
    registry.announce(createInstanceDescriptor());
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheAnalyzerAudioProcessor();
}