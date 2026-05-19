#pragma once

#include "IPCTransport.h"

#include <juce_core/juce_core.h>

namespace gitpro::ipc
{
    struct PdcPingRequest
    {
        std::uint64_t requestId = 0;
        EndpointId analyzerEndpoint;
        std::uint64_t issuedMilliseconds = 0;
    };

    class FileDiscoveryRegistry final : public DiscoveryRegistry
    {
    public:
        explicit FileDiscoveryRegistry(juce::File registryDirectory = getDefaultRegistryDirectory());
        ~FileDiscoveryRegistry() override;

        bool start(const InstanceDescriptor& localInstance) override;
        void stop() noexcept override;

        bool announce(const InstanceDescriptor& localInstance) noexcept override;
        void goodbye(const EndpointId& endpoint) noexcept override;

        [[nodiscard]] std::vector<InstanceDescriptor> findActiveProbes() const override;
        [[nodiscard]] std::vector<InstanceDescriptor> findActiveAnalyzers() const override;

        bool publishPdcPingRequest(const PdcPingRequest& request) noexcept;
        [[nodiscard]] std::optional<PdcPingRequest> getLatestPdcPingRequest() const;

        [[nodiscard]] static juce::File getDefaultRegistryDirectory();

    private:
        [[nodiscard]] juce::File getFileForEndpoint(const EndpointId& endpoint) const;
        [[nodiscard]] juce::File getPdcPingRequestFile() const;
        [[nodiscard]] std::vector<InstanceDescriptor> findActiveInstances(PluginRole role) const;
        [[nodiscard]] static juce::var descriptorToVar(const InstanceDescriptor& descriptor);
        [[nodiscard]] static std::optional<InstanceDescriptor> descriptorFromVar(const juce::var& value);
        [[nodiscard]] static juce::String roleToString(PluginRole role);
        [[nodiscard]] static PluginRole roleFromString(const juce::String& role);
        [[nodiscard]] static juce::String transportKindToString(TransportKind kind);
        [[nodiscard]] static std::optional<TransportKind> transportKindFromString(const juce::String& value);
        [[nodiscard]] static std::uint64_t nowMilliseconds() noexcept;

        juce::File directory;
        InstanceDescriptor local;
        bool started = false;
        std::uint64_t staleTimeoutMilliseconds = 5000;
    };
}