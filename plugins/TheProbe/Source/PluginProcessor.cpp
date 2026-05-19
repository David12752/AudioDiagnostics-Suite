#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr const char* instanceUuidProperty = "instanceUuid";
    constexpr auto minPitchHz = 35.0f;
    constexpr auto maxPitchHz = 1200.0f;
    constexpr auto defaultAnalyzeSeconds = 3.0f;
    constexpr auto pdcPingImpulseAmplitude = 0.85f;

    float gainToDb(float gain) noexcept
    {
        return gain > 0.0f ? juce::Decibels::gainToDecibels(gain, -120.0f) : -120.0f;
    }

    float getJsonFloat(const juce::DynamicObject& object, const juce::Identifier& property, float fallback)
    {
        const auto value = object.getProperty(property);
        return value.isVoid() ? fallback : static_cast<float>(value);
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
}

TheProbeAudioProcessor::TheProbeAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "State", createParameterLayout())
{
    for (auto& energy : latestLowBandEnergiesDb)
        energy.store(-120.0f, std::memory_order_relaxed);

    for (auto& phase : latestLowBandPhasesRadians)
        phase.store(0.0f, std::memory_order_relaxed);

    ensureInstanceUuid();
    registry.start(createInstanceDescriptor());
    startTimerHz(60);
}

TheProbeAudioProcessor::~TheProbeAudioProcessor()
{
    stopTimer();
    registry.stop();
}

void TheProbeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(samplesPerBlock, std::memory_order_relaxed);
    lowBandAnalyzer.reset();
    pitchLowpassState = 0.0f;
    pitchWasAboveThreshold = false;
    pitchSamplesSinceCrossing = 0;
    smoothedPitchHz = 0.0f;
    processedSampleCounter = 0;
    performanceTimerTick = 0;
    meterFifo.reset();
    registry.announce(createInstanceDescriptor());
}

void TheProbeAudioProcessor::releaseResources()
{
}

bool TheProbeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    return mainInput == mainOutput && (! mainInput.isDisabled());
}

void TheProbeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();

    updateMeterSnapshot(buffer);
    updatePitchDetector(buffer);
    updateAutoGainAnalysis(buffer);
    lowBandAnalyzer.pushBlock(buffer);
    applyGainCompensation(buffer);
    injectPendingPdcPing(buffer);
    processedSampleCounter += buffer.getNumSamples();
    sequenceNumber.fetch_add(1, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* TheProbeAudioProcessor::createEditor()
{
    return new TheProbeAudioProcessorEditor(*this);
}

bool TheProbeAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String TheProbeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TheProbeAudioProcessor::acceptsMidi() const
{
    return false;
}

bool TheProbeAudioProcessor::producesMidi() const
{
    return false;
}

bool TheProbeAudioProcessor::isMidiEffect() const
{
    return false;
}

double TheProbeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TheProbeAudioProcessor::getNumPrograms()
{
    return 1;
}

int TheProbeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TheProbeAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String TheProbeAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void TheProbeAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void TheProbeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void TheProbeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }

    ensureInstanceUuid();
    registry.announce(createInstanceDescriptor());
}

juce::String TheProbeAudioProcessor::getInstanceUuid() const
{
    return apvts.state.getProperty(instanceUuidProperty).toString();
}

gitpro::ipc::InstanceDescriptor TheProbeAudioProcessor::createInstanceDescriptor() const
{
    gitpro::ipc::InstanceDescriptor descriptor;
    descriptor.endpoint = gitpro::ipc::EndpointId::fromString(getInstanceUuid().toStdString());
    descriptor.role = gitpro::ipc::PluginRole::probe;
    descriptor.displayName = "The Probe";
    descriptor.hostName = juce::PluginHostType().getHostDescription();
    descriptor.trackName = "Unassigned Track";
    descriptor.sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    descriptor.blockSize = static_cast<std::uint32_t>(std::max(0, currentBlockSize.load(std::memory_order_relaxed)));
    descriptor.supportedTransports = { gitpro::ipc::TransportKind::loopback, gitpro::ipc::TransportKind::sharedMemory };
    descriptor.heartbeatCounter = sequenceNumber.load(std::memory_order_relaxed);
    descriptor.peakDbfs = latestPeakDbfs.load(std::memory_order_relaxed);
    descriptor.rmsDbfs = latestRmsDbfs.load(std::memory_order_relaxed);
    descriptor.crestFactorDb = latestCrestFactorDb.load(std::memory_order_relaxed);
    descriptor.noiseFloorDbfs = latestNoiseFloorDbfs.load(std::memory_order_relaxed);
    descriptor.snrDb = latestSnrDb.load(std::memory_order_relaxed);
    descriptor.lowBandTotalEnergyDb = latestLowBandTotalEnergyDb.load(std::memory_order_relaxed);
    descriptor.dominantLowFrequencyHz = latestDominantLowFrequencyHz.load(std::memory_order_relaxed);
    descriptor.dominantLowBandIndex = latestDominantLowBandIndex.load(std::memory_order_relaxed);
    descriptor.lowFrequencyCorrelation = latestLowFrequencyCorrelation.load(std::memory_order_relaxed);
    descriptor.pdcPingRequestId = latestPdcPingRequestId.load(std::memory_order_relaxed);
    descriptor.pdcPingInjectedSample = latestPdcPingInjectedSample.load(std::memory_order_relaxed);

    for (std::size_t band = 0; band < gitpro::ipc::InstanceDescriptor::lowBandCount; ++band)
    {
        descriptor.lowBandEnergiesDb[band] = latestLowBandEnergiesDb[band].load(std::memory_order_relaxed);
        descriptor.lowBandPhasesRadians[band] = latestLowBandPhasesRadians[band].load(std::memory_order_relaxed);
    }

    return descriptor;
}

juce::String TheProbeAudioProcessor::createStatusJson() const
{
    juce::Array<juce::var> lowBandEnergies;

    for (std::size_t band = 0; band < gitpro::ipc::InstanceDescriptor::lowBandCount; ++band)
        lowBandEnergies.add(latestLowBandEnergiesDb[band].load(std::memory_order_relaxed));

    auto* realtimeObject = new juce::DynamicObject();
    realtimeObject->setProperty("peakDbfs", latestPeakDbfs.load(std::memory_order_relaxed));
    realtimeObject->setProperty("rmsDbfs", latestRmsDbfs.load(std::memory_order_relaxed));
    realtimeObject->setProperty("crestFactorDb", latestCrestFactorDb.load(std::memory_order_relaxed));
    realtimeObject->setProperty("lowFrequencyCorrelation", latestLowFrequencyCorrelation.load(std::memory_order_relaxed));
    realtimeObject->setProperty("lowBandEnergiesDb", lowBandEnergies);

    auto* object = new juce::DynamicObject();
    object->setProperty("instanceUuid", getInstanceUuid());
    object->setProperty("role", "probe");
    object->setProperty("peakDbfs", latestPeakDbfs.load(std::memory_order_relaxed));
    object->setProperty("rmsDbfs", latestRmsDbfs.load(std::memory_order_relaxed));
    object->setProperty("crestFactorDb", latestCrestFactorDb.load(std::memory_order_relaxed));
    object->setProperty("noiseFloorDbfs", latestNoiseFloorDbfs.load(std::memory_order_relaxed));
    object->setProperty("snrDb", latestSnrDb.load(std::memory_order_relaxed));
    object->setProperty("lowBandTotalEnergyDb", latestLowBandTotalEnergyDb.load(std::memory_order_relaxed));
    object->setProperty("dominantLowFrequencyHz", latestDominantLowFrequencyHz.load(std::memory_order_relaxed));
    object->setProperty("lowFrequencyCorrelation", latestLowFrequencyCorrelation.load(std::memory_order_relaxed));
    object->setProperty("realtime", juce::var(realtimeObject));
    object->setProperty("pitchFrequencyHz", latestPitchFrequencyHz.load(std::memory_order_relaxed));
    object->setProperty("targetPeakDbfs", targetPeakDbfs.load(std::memory_order_relaxed));
    object->setProperty("interfaceMaxInputDbU", interfaceMaxInputDbU.load(std::memory_order_relaxed));
    object->setProperty("autoGainDb", autoGainDb.load(std::memory_order_relaxed));
    object->setProperty("measuredCalibrationPeakDbfs", measuredCalibrationPeakDbfs.load(std::memory_order_relaxed));
    object->setProperty("requiredGainOffsetDb", requiredGainOffsetDb.load(std::memory_order_relaxed));
    object->setProperty("pdcPingRequestId", static_cast<double>(latestPdcPingRequestId.load(std::memory_order_relaxed)));
    object->setProperty("pdcPingInjectedSample", static_cast<double>(latestPdcPingInjectedSample.load(std::memory_order_relaxed)));
    object->setProperty("autoAnalyzeActive", autoAnalyzeActive.load(std::memory_order_relaxed));
    const auto totalSamples = autoAnalyzeTotalSamples.load(std::memory_order_relaxed);
    const auto remainingSamples = autoAnalyzeRemainingSamples.load(std::memory_order_relaxed);
    const auto progress = totalSamples > 0 ? 1.0f - (static_cast<float>(remainingSamples) / static_cast<float>(totalSamples)) : 0.0f;
    object->setProperty("autoAnalyzeProgress", juce::jlimit(0.0f, 1.0f, progress));
    object->setProperty("sequenceNumber", static_cast<double>(sequenceNumber.load(std::memory_order_relaxed)));
    return juce::JSON::toString(juce::var(object), true);
}

void TheProbeAudioProcessor::handleDashboardCommand(const juce::String& commandJson)
{
    const auto parsed = juce::JSON::parse(commandJson);
    const auto* object = parsed.getDynamicObject();

    if (object == nullptr)
        return;

    const auto command = object->getProperty("command").toString();

    if (command == "setProbeCalibration")
    {
        interfaceMaxInputDbU.store(getJsonFloat(*object, "interfaceMaxInputDbU", interfaceMaxInputDbU.load(std::memory_order_relaxed)), std::memory_order_relaxed);
        targetPeakDbfs.store(getJsonFloat(*object, "targetPeakDbfs", targetPeakDbfs.load(std::memory_order_relaxed)), std::memory_order_relaxed);
        return;
    }

    if (command == "startAutoAnalyze")
        requestAutoAnalyze(getJsonFloat(*object, "seconds", defaultAnalyzeSeconds));
}

juce::AudioProcessorValueTreeState::ParameterLayout TheProbeAudioProcessor::createParameterLayout()
{
    return {};
}

void TheProbeAudioProcessor::ensureInstanceUuid()
{
    if (! apvts.state.hasProperty(instanceUuidProperty) || apvts.state.getProperty(instanceUuidProperty).toString().isEmpty())
        apvts.state.setProperty(instanceUuidProperty, juce::Uuid().toString(), nullptr);
}

void TheProbeAudioProcessor::timerCallback()
{
    ++performanceTimerTick;
    flushMeterSnapshots();
    updateSpectralSnapshot();

    if (performanceTimerTick % pdcPollDivider == 0)
        pollPdcPingRequest();

    if (performanceTimerTick % registryAnnounceDivider == 0)
        registry.announce(createInstanceDescriptor());
}

void TheProbeAudioProcessor::updateMeterSnapshot(const juce::AudioBuffer<float>& buffer) noexcept
{
    constexpr auto silenceFloorLinear = 0.000001f;
    constexpr auto noiseGateLinear = 0.001f;
    constexpr auto activeGateLinear = 0.0031622776f;
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
            lowpassLeft += lowpassAlpha * (left[sample] - lowpassLeft);
            lowpassRight += lowpassAlpha * (right[sample] - lowpassRight);
            lowLeftSquares += lowpassLeft * lowpassLeft;
            lowRightSquares += lowpassRight * lowpassRight;
            lowCross += lowpassLeft * lowpassRight;
        }
    }

    const auto rms = sampleCount > 0 ? static_cast<float>(std::sqrt(sumSquares / static_cast<double>(sampleCount))) : 0.0f;
    const auto peakDb = gainToDb(peak);
    const auto rmsDb = gainToDb(rms);
    const auto crestFactor = rms > silenceFloorLinear ? juce::jlimit(0.0f, 60.0f, peakDb - rmsDb) : 0.0f;

    if (rms > silenceFloorLinear && rms < noiseGateLinear)
        noiseFloorLinear += 0.01f * (rms - noiseFloorLinear);

    if (rms >= activeGateLinear)
        activeSignalLinear += 0.05f * (rms - activeSignalLinear);

    const auto snr = juce::jlimit(0.0f, 120.0f, gainToDb(activeSignalLinear) - gainToDb(noiseFloorLinear));

    RealtimeMeterSnapshot snapshot;
    snapshot.peakDbfs = peakDb;
    snapshot.rmsDbfs = rmsDb;
    snapshot.crestFactorDb = crestFactor;
    snapshot.noiseFloorDbfs = gainToDb(noiseFloorLinear);
    snapshot.snrDb = snr;
    snapshot.lowFrequencyCorrelation = latestLowFrequencyCorrelation.load(std::memory_order_relaxed);

    if (lowLeftSquares > 0.00000001f && lowRightSquares > 0.00000001f)
    {
        const auto correlation = lowCross / std::sqrt(lowLeftSquares * lowRightSquares);
        snapshot.lowFrequencyCorrelation = juce::jlimit(-1.0f, 1.0f, correlation);
    }

    pushMeterSnapshot(snapshot);
}

void TheProbeAudioProcessor::pushMeterSnapshot(const RealtimeMeterSnapshot& snapshot) noexcept
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

void TheProbeAudioProcessor::flushMeterSnapshots() noexcept
{
    auto peakDbfs = -120.0f;
    auto rmsLinearSum = 0.0f;
    auto crestSum = 0.0f;
    auto noiseSum = 0.0f;
    auto snrSum = 0.0f;
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
        noiseSum += snapshot.noiseFloorDbfs;
        snrSum += snapshot.snrDb;
        correlationSum += snapshot.lowFrequencyCorrelation;
        ++consumed;
        meterFifo.finishedRead(size1 + size2);
    }

    if (consumed <= 0)
        return;

    latestPeakDbfs.store(peakDbfs, std::memory_order_relaxed);
    latestRmsDbfs.store(gainToDb(rmsLinearSum / static_cast<float>(consumed)), std::memory_order_relaxed);
    latestCrestFactorDb.store(crestSum / static_cast<float>(consumed), std::memory_order_relaxed);
    latestNoiseFloorDbfs.store(noiseSum / static_cast<float>(consumed), std::memory_order_relaxed);
    latestSnrDb.store(snrSum / static_cast<float>(consumed), std::memory_order_relaxed);
    latestLowFrequencyCorrelation.store(correlationSum / static_cast<float>(consumed), std::memory_order_relaxed);
}

void TheProbeAudioProcessor::updatePitchDetector(const juce::AudioBuffer<float>& buffer) noexcept
{
    const auto sampleRate = currentSampleRate.load(std::memory_order_relaxed);

    if (sampleRate <= 0.0 || buffer.getNumChannels() <= 0)
        return;

    constexpr auto lowpassAlpha = 0.08f;
    constexpr auto threshold = 0.01f;
    const auto numChannels = buffer.getNumChannels();

    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto mono = 0.0f;

        for (auto channel = 0; channel < numChannels; ++channel)
            mono += buffer.getReadPointer(channel)[sample];

        mono /= static_cast<float>(numChannels);
        pitchLowpassState += lowpassAlpha * (mono - pitchLowpassState);
        ++pitchSamplesSinceCrossing;

        if (! pitchWasAboveThreshold && pitchLowpassState > threshold)
        {
            const auto periodSamples = pitchSamplesSinceCrossing;
            pitchSamplesSinceCrossing = 0;
            pitchWasAboveThreshold = true;

            if (periodSamples > 0)
            {
                const auto frequency = static_cast<float>(sampleRate / static_cast<double>(periodSamples));

                if (frequency >= minPitchHz && frequency <= maxPitchHz)
                {
                    smoothedPitchHz = smoothedPitchHz <= 0.0f ? frequency : smoothedPitchHz + 0.18f * (frequency - smoothedPitchHz);
                    latestPitchFrequencyHz.store(smoothedPitchHz, std::memory_order_relaxed);
                }
            }
        }
        else if (pitchWasAboveThreshold && pitchLowpassState < -threshold)
        {
            pitchWasAboveThreshold = false;
        }
    }
}

void TheProbeAudioProcessor::updateAutoGainAnalysis(const juce::AudioBuffer<float>& buffer) noexcept
{
    if (autoAnalyzeResetRequested.exchange(false, std::memory_order_acq_rel))
    {
        autoAnalyzePeakLinear = 0.0f;
        const auto samples = std::max(0, autoAnalyzeRequestedSamples.exchange(0, std::memory_order_acq_rel));
        autoAnalyzeTotalSamples.store(samples, std::memory_order_release);
        autoAnalyzeRemainingSamples.store(samples, std::memory_order_release);
        autoAnalyzeActive.store(samples > 0, std::memory_order_release);
        measuredCalibrationPeakDbfs.store(-120.0f, std::memory_order_relaxed);
    }

    auto remaining = autoAnalyzeRemainingSamples.load(std::memory_order_acquire);

    if (remaining <= 0)
        return;

    auto blockPeak = 0.0f;

    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);

        for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
            blockPeak = std::max(blockPeak, std::abs(samples[sample]));
    }

    autoAnalyzePeakLinear = std::max(autoAnalyzePeakLinear, blockPeak);
    measuredCalibrationPeakDbfs.store(gainToDb(autoAnalyzePeakLinear), std::memory_order_relaxed);

    remaining = std::max(0, remaining - buffer.getNumSamples());
    autoAnalyzeRemainingSamples.store(remaining, std::memory_order_release);

    if (remaining == 0)
    {
        const auto measuredPeakDbfs = gainToDb(autoAnalyzePeakLinear);
        const auto requiredOffset = juce::jlimit(-36.0f, 36.0f, targetPeakDbfs.load(std::memory_order_relaxed) - measuredPeakDbfs);
        measuredCalibrationPeakDbfs.store(measuredPeakDbfs, std::memory_order_relaxed);
        requiredGainOffsetDb.store(requiredOffset, std::memory_order_relaxed);
        autoGainDb.store(requiredOffset, std::memory_order_relaxed);
        autoAnalyzeActive.store(false, std::memory_order_release);
    }
}

void TheProbeAudioProcessor::applyGainCompensation(juce::AudioBuffer<float>& buffer) noexcept
{
    buffer.applyGain(juce::Decibels::decibelsToGain(autoGainDb.load(std::memory_order_relaxed)));
}

void TheProbeAudioProcessor::injectPendingPdcPing(juce::AudioBuffer<float>& buffer) noexcept
{
    const auto requestId = pendingPdcPingRequestId.exchange(0, std::memory_order_acq_rel);

    if (requestId == 0 || buffer.getNumSamples() <= 0)
        return;

    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);
        samples[0] = juce::jlimit(-1.0f, 1.0f, samples[0] + pdcPingImpulseAmplitude);
    }

    latestPdcPingRequestId.store(requestId, std::memory_order_release);
    latestPdcPingInjectedSample.store(getBlockStartSample(*this, processedSampleCounter), std::memory_order_release);
}

void TheProbeAudioProcessor::pollPdcPingRequest()
{
    const auto request = registry.getLatestPdcPingRequest();

    if (! request.has_value() || request->requestId <= lastSeenPdcPingRequestId)
        return;

    const auto now = static_cast<std::uint64_t>(juce::Time::getMillisecondCounter());
    const auto requestAge = now >= request->issuedMilliseconds ? now - request->issuedMilliseconds : 0;

    if (requestAge > 10000)
        return;

    lastSeenPdcPingRequestId = request->requestId;
    latestPdcPingRequestId.store(request->requestId, std::memory_order_release);
    latestPdcPingInjectedSample.store(-1, std::memory_order_release);
    pendingPdcPingRequestId.store(request->requestId, std::memory_order_release);
}

void TheProbeAudioProcessor::updateSpectralSnapshot() noexcept
{
    gitpro::dsp::LowBandSpectralMetrics metrics;

    if (! lowBandAnalyzer.analyzeIfReady(currentSampleRate.load(std::memory_order_relaxed), metrics))
        return;

    for (std::size_t band = 0; band < gitpro::ipc::InstanceDescriptor::lowBandCount; ++band)
    {
        latestLowBandEnergiesDb[band].store(metrics.bandEnergyDb[band], std::memory_order_relaxed);
        latestLowBandPhasesRadians[band].store(metrics.bandPhaseRadians[band], std::memory_order_relaxed);
    }

    latestLowBandTotalEnergyDb.store(metrics.lowBandTotalEnergyDb, std::memory_order_relaxed);
    latestDominantLowFrequencyHz.store(metrics.dominantFrequencyHz, std::memory_order_relaxed);
    latestDominantLowBandIndex.store(metrics.dominantBandIndex, std::memory_order_relaxed);
}

void TheProbeAudioProcessor::requestAutoAnalyze(float seconds) noexcept
{
    const auto sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    const auto duration = juce::jlimit(0.5f, 8.0f, seconds);
    const auto samples = static_cast<int>(std::max(1.0, sampleRate > 0.0 ? sampleRate * static_cast<double>(duration) : 48000.0 * static_cast<double>(duration)));
    autoAnalyzeRequestedSamples.store(samples, std::memory_order_release);
    autoAnalyzeResetRequested.store(true, std::memory_order_release);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheProbeAudioProcessor();
}