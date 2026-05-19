#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

namespace
{
    constexpr const char* instanceUuidProperty = "instanceUuid";
}

TheAnalyzerAudioProcessor::TheAnalyzerAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "State", createParameterLayout())
{
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
    juce::ignoreUnused(buffer);
    midiMessages.clear();
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
        probeObject->setProperty("heartbeatCounter", static_cast<double>(probe.heartbeatCounter));
        probeArray.add(juce::var(probeObject));
    }

    auto* object = new juce::DynamicObject();
    object->setProperty("instanceUuid", getInstanceUuid());
    object->setProperty("role", "analyzer");
    object->setProperty("activeProbeCount", static_cast<int>(activeProbes.size()));
    object->setProperty("transport", "registry");
    object->setProperty("sequenceNumber", static_cast<double>(sequenceNumber.load(std::memory_order_relaxed)));
    object->setProperty("probes", probeArray);

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
    registry.announce(createInstanceDescriptor());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheAnalyzerAudioProcessor();
}