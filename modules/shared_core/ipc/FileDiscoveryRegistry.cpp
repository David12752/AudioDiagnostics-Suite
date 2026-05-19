#include "FileDiscoveryRegistry.h"

#include <algorithm>

namespace gitpro::ipc
{
    FileDiscoveryRegistry::FileDiscoveryRegistry(juce::File registryDirectory)
        : directory(std::move(registryDirectory))
    {
    }

    FileDiscoveryRegistry::~FileDiscoveryRegistry()
    {
        stop();
    }

    bool FileDiscoveryRegistry::start(const InstanceDescriptor& localInstance)
    {
        local = localInstance;
        started = directory.createDirectory();

        if (! started)
            return false;

        return announce(local);
    }

    void FileDiscoveryRegistry::stop() noexcept
    {
        if (! started)
            return;

        goodbye(local.endpoint);
        started = false;
    }

    bool FileDiscoveryRegistry::announce(const InstanceDescriptor& localInstance) noexcept
    {
        if (! localInstance.endpoint.isValid())
            return false;

        auto descriptor = localInstance;
        descriptor.lastSeenMilliseconds = nowMilliseconds();

        const auto target = getFileForEndpoint(descriptor.endpoint);
        const auto temp = target.getSiblingFile(target.getFileName() + ".tmp");
        const auto json = juce::JSON::toString(descriptorToVar(descriptor), true);

        if (! temp.replaceWithText(json))
            return false;

        if (target.existsAsFile())
            target.deleteFile();

        return temp.moveFileTo(target);
    }

    void FileDiscoveryRegistry::goodbye(const EndpointId& endpoint) noexcept
    {
        if (! endpoint.isValid())
            return;

        getFileForEndpoint(endpoint).deleteFile();
    }

    std::vector<InstanceDescriptor> FileDiscoveryRegistry::findActiveProbes() const
    {
        return findActiveInstances(PluginRole::probe);
    }

    std::vector<InstanceDescriptor> FileDiscoveryRegistry::findActiveAnalyzers() const
    {
        return findActiveInstances(PluginRole::analyzer);
    }

    juce::File FileDiscoveryRegistry::getDefaultRegistryDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("GitProDiagnosticsSuite")
            .getChildFile("DiscoveryRegistry")
            .getChildFile("v1");
    }

    juce::File FileDiscoveryRegistry::getFileForEndpoint(const EndpointId& endpoint) const
    {
        auto fileName = juce::File::createLegalFileName(endpoint.toString());

        if (fileName.isEmpty())
            fileName = "invalid-endpoint";

        return directory.getChildFile(fileName + ".json");
    }

    std::vector<InstanceDescriptor> FileDiscoveryRegistry::findActiveInstances(PluginRole role) const
    {
        std::vector<InstanceDescriptor> instances;

        if (! directory.isDirectory())
            return instances;

        const auto now = nowMilliseconds();
        juce::Array<juce::File> files;
        directory.findChildFiles(files, juce::File::findFiles, false, "*.json");

        for (const auto& file : files)
        {
            auto parsed = juce::JSON::parse(file.loadFileAsString());
            auto descriptor = descriptorFromVar(parsed);

            if (! descriptor.has_value())
                continue;

            const auto age = now > descriptor->lastSeenMilliseconds ? now - descriptor->lastSeenMilliseconds : 0;

            if (age > staleTimeoutMilliseconds)
                continue;

            if (descriptor->role == role)
                instances.push_back(std::move(*descriptor));
        }

        std::sort(instances.begin(), instances.end(), [](const auto& left, const auto& right)
        {
            return left.displayName < right.displayName;
        });

        return instances;
    }

    juce::var FileDiscoveryRegistry::descriptorToVar(const InstanceDescriptor& descriptor)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("protocolVersion", static_cast<int>(TransportPacket::protocolVersion));
        object->setProperty("uuid", juce::String(descriptor.endpoint.toString()));
        object->setProperty("role", roleToString(descriptor.role));
        object->setProperty("displayName", juce::String(descriptor.displayName));
        object->setProperty("hostName", juce::String(descriptor.hostName));
        object->setProperty("trackName", juce::String(descriptor.trackName));
        object->setProperty("sampleRate", descriptor.sampleRate);
        object->setProperty("blockSize", static_cast<int>(descriptor.blockSize));
        object->setProperty("heartbeatCounter", static_cast<double>(descriptor.heartbeatCounter));
        object->setProperty("lastSeenMilliseconds", static_cast<double>(descriptor.lastSeenMilliseconds));
        object->setProperty("peakDbfs", descriptor.peakDbfs);
        object->setProperty("rmsDbfs", descriptor.rmsDbfs);
        object->setProperty("noiseFloorDbfs", descriptor.noiseFloorDbfs);
        object->setProperty("snrDb", descriptor.snrDb);
        object->setProperty("lowBandTotalEnergyDb", descriptor.lowBandTotalEnergyDb);
        object->setProperty("dominantLowFrequencyHz", descriptor.dominantLowFrequencyHz);
        object->setProperty("dominantLowBandIndex", descriptor.dominantLowBandIndex);
        object->setProperty("lowFrequencyCorrelation", descriptor.lowFrequencyCorrelation);

        juce::Array<juce::var> lowBandEnergies;
        juce::Array<juce::var> lowBandPhases;

        for (auto band = 0; band < InstanceDescriptor::lowBandCount; ++band)
        {
            lowBandEnergies.add(descriptor.lowBandEnergiesDb[band]);
            lowBandPhases.add(descriptor.lowBandPhasesRadians[band]);
        }

        object->setProperty("lowBandEnergiesDb", lowBandEnergies);
        object->setProperty("lowBandPhasesRadians", lowBandPhases);

        juce::Array<juce::var> transports;

        for (const auto transport : descriptor.supportedTransports)
            transports.add(transportKindToString(transport));

        object->setProperty("supportedTransports", transports);
        return juce::var(object);
    }

    std::optional<InstanceDescriptor> FileDiscoveryRegistry::descriptorFromVar(const juce::var& value)
    {
        const auto* object = value.getDynamicObject();

        if (object == nullptr)
            return std::nullopt;

        InstanceDescriptor descriptor;
        descriptor.endpoint = EndpointId::fromString(object->getProperty("uuid").toString().toStdString());

        if (! descriptor.endpoint.isValid())
            return std::nullopt;

        descriptor.role = roleFromString(object->getProperty("role").toString());
        descriptor.displayName = object->getProperty("displayName").toString().toStdString();
        descriptor.hostName = object->getProperty("hostName").toString().toStdString();
        descriptor.trackName = object->getProperty("trackName").toString().toStdString();
        descriptor.sampleRate = static_cast<double>(object->getProperty("sampleRate"));
        descriptor.blockSize = static_cast<std::uint32_t>(static_cast<int>(object->getProperty("blockSize")));
        descriptor.heartbeatCounter = static_cast<std::uint64_t>(static_cast<double>(object->getProperty("heartbeatCounter")));
        descriptor.lastSeenMilliseconds = static_cast<std::uint64_t>(static_cast<double>(object->getProperty("lastSeenMilliseconds")));
        descriptor.peakDbfs = static_cast<float>(object->getProperty("peakDbfs"));
        descriptor.rmsDbfs = static_cast<float>(object->getProperty("rmsDbfs"));
        descriptor.noiseFloorDbfs = static_cast<float>(object->getProperty("noiseFloorDbfs"));
        descriptor.snrDb = static_cast<float>(object->getProperty("snrDb"));
        descriptor.lowBandTotalEnergyDb = static_cast<float>(object->getProperty("lowBandTotalEnergyDb"));
        descriptor.dominantLowFrequencyHz = static_cast<float>(object->getProperty("dominantLowFrequencyHz"));
        descriptor.dominantLowBandIndex = static_cast<int>(object->getProperty("dominantLowBandIndex"));
        descriptor.lowFrequencyCorrelation = static_cast<float>(object->getProperty("lowFrequencyCorrelation"));

        if (const auto* energies = object->getProperty("lowBandEnergiesDb").getArray())
        {
            const auto count = std::min<std::size_t>(energies->size(), InstanceDescriptor::lowBandCount);

            for (std::size_t band = 0; band < count; ++band)
                descriptor.lowBandEnergiesDb[band] = static_cast<float>((*energies)[static_cast<int>(band)]);
        }

        if (const auto* phases = object->getProperty("lowBandPhasesRadians").getArray())
        {
            const auto count = std::min<std::size_t>(phases->size(), InstanceDescriptor::lowBandCount);

            for (std::size_t band = 0; band < count; ++band)
                descriptor.lowBandPhasesRadians[band] = static_cast<float>((*phases)[static_cast<int>(band)]);
        }

        if (const auto* transports = object->getProperty("supportedTransports").getArray())
        {
            for (const auto& transport : *transports)
            {
                if (auto kind = transportKindFromString(transport.toString()))
                    descriptor.supportedTransports.push_back(*kind);
            }
        }

        return descriptor;
    }

    juce::String FileDiscoveryRegistry::roleToString(PluginRole role)
    {
        return role == PluginRole::probe ? "probe" : "analyzer";
    }

    PluginRole FileDiscoveryRegistry::roleFromString(const juce::String& role)
    {
        return role == "analyzer" ? PluginRole::analyzer : PluginRole::probe;
    }

    juce::String FileDiscoveryRegistry::transportKindToString(TransportKind kind)
    {
        switch (kind)
        {
            case TransportKind::loopback: return "loopback";
            case TransportKind::sharedMemory: return "sharedMemory";
            case TransportKind::localSocket: return "localSocket";
            case TransportKind::namedPipe: return "namedPipe";
        }

        return "unknown";
    }

    std::optional<TransportKind> FileDiscoveryRegistry::transportKindFromString(const juce::String& value)
    {
        if (value == "loopback")
            return TransportKind::loopback;

        if (value == "sharedMemory")
            return TransportKind::sharedMemory;

        if (value == "localSocket")
            return TransportKind::localSocket;

        if (value == "namedPipe")
            return TransportKind::namedPipe;

        return std::nullopt;
    }

    std::uint64_t FileDiscoveryRegistry::nowMilliseconds() noexcept
    {
        return static_cast<std::uint64_t>(juce::Time::getMillisecondCounter());
    }
}