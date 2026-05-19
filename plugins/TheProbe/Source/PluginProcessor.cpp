#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr const char* instanceUuidProperty = "instanceUuid";

    float gainToDb(float gain) noexcept
    {
        return gain > 0.0f ? juce::Decibels::gainToDecibels(gain, -120.0f) : -120.0f;
    }
}

TheProbeAudioProcessor::TheProbeAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "State", createParameterLayout())
{
    ensureInstanceUuid();
    registry.start(createInstanceDescriptor());
    startTimerHz(2);
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
    descriptor.noiseFloorDbfs = latestNoiseFloorDbfs.load(std::memory_order_relaxed);
    descriptor.snrDb = latestSnrDb.load(std::memory_order_relaxed);
    return descriptor;
}

juce::String TheProbeAudioProcessor::createStatusJson() const
{
    auto* object = new juce::DynamicObject();
    object->setProperty("instanceUuid", getInstanceUuid());
    object->setProperty("role", "probe");
    object->setProperty("peakDbfs", latestPeakDbfs.load(std::memory_order_relaxed));
    object->setProperty("rmsDbfs", latestRmsDbfs.load(std::memory_order_relaxed));
    object->setProperty("noiseFloorDbfs", latestNoiseFloorDbfs.load(std::memory_order_relaxed));
    object->setProperty("snrDb", latestSnrDb.load(std::memory_order_relaxed));
    object->setProperty("sequenceNumber", static_cast<double>(sequenceNumber.load(std::memory_order_relaxed)));
    return juce::JSON::toString(juce::var(object), true);
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
    registry.announce(createInstanceDescriptor());
}

void TheProbeAudioProcessor::updateMeterSnapshot(const juce::AudioBuffer<float>& buffer) noexcept
{
    constexpr auto silenceFloorLinear = 0.000001f;
    constexpr auto noiseGateLinear = 0.001f;
    constexpr auto activeGateLinear = 0.0031622776f;

    auto peak = 0.0f;
    auto sumSquares = 0.0;
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

    const auto rms = sampleCount > 0 ? static_cast<float>(std::sqrt(sumSquares / static_cast<double>(sampleCount))) : 0.0f;

    if (rms > silenceFloorLinear && rms < noiseGateLinear)
        noiseFloorLinear += 0.01f * (rms - noiseFloorLinear);

    if (rms >= activeGateLinear)
        activeSignalLinear += 0.05f * (rms - activeSignalLinear);

    const auto snr = juce::jlimit(0.0f, 120.0f, gainToDb(activeSignalLinear) - gainToDb(noiseFloorLinear));

    latestPeakDbfs.store(gainToDb(peak), std::memory_order_relaxed);
    latestRmsDbfs.store(gainToDb(rms), std::memory_order_relaxed);
    latestNoiseFloorDbfs.store(gainToDb(noiseFloorLinear), std::memory_order_relaxed);
    latestSnrDb.store(snr, std::memory_order_relaxed);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheProbeAudioProcessor();
}