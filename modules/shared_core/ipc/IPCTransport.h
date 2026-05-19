#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gitpro::ipc
{
    enum class PluginRole : std::uint8_t
    {
        probe,
        analyzer
    };

    enum class TransportKind : std::uint8_t
    {
        loopback,
        sharedMemory,
        localSocket,
        namedPipe
    };

    enum class PacketType : std::uint8_t
    {
        audioSummary,
        meterSnapshot,
        latencyPingRequest,
        latencyPingResponse,
        calibrationRequest,
        calibrationResult,
        registryAnnouncement,
        registryGoodbye
    };

    struct EndpointId
    {
        static constexpr std::size_t maxTextBytes = 40;

        std::array<char, maxTextBytes> text {};

        static EndpointId fromString(std::string_view value) noexcept
        {
            EndpointId endpoint;
            const auto bytesToCopy = std::min(value.size(), maxTextBytes - 1);
            std::copy_n(value.data(), bytesToCopy, endpoint.text.data());
            endpoint.text[bytesToCopy] = '\0';
            return endpoint;
        }

        [[nodiscard]] std::string toString() const
        {
            return std::string(text.data());
        }

        [[nodiscard]] bool isValid() const noexcept
        {
            return text[0] != '\0';
        }

        friend bool operator==(const EndpointId& left, const EndpointId& right) noexcept
        {
            return left.text == right.text;
        }

        friend bool operator!=(const EndpointId& left, const EndpointId& right) noexcept
        {
            return !(left == right);
        }
    };

    struct InstanceDescriptor
    {
        static constexpr std::size_t lowBandCount = 6;

        EndpointId endpoint;
        PluginRole role = PluginRole::probe;
        std::string displayName;
        std::string hostName;
        std::string trackName;
        double sampleRate = 0.0;
        std::uint32_t blockSize = 0;
        std::vector<TransportKind> supportedTransports;
        std::uint64_t heartbeatCounter = 0;
        std::uint64_t lastSeenMilliseconds = 0;
        float peakDbfs = -120.0f;
        float rmsDbfs = -120.0f;
        float crestFactorDb = 0.0f;
        float noiseFloorDbfs = -120.0f;
        float snrDb = 0.0f;
        std::array<float, lowBandCount> lowBandEnergiesDb { -120.0f, -120.0f, -120.0f, -120.0f, -120.0f, -120.0f };
        std::array<float, lowBandCount> lowBandPhasesRadians {};
        float lowBandTotalEnergyDb = -120.0f;
        float dominantLowFrequencyHz = 0.0f;
        int dominantLowBandIndex = -1;
        float lowFrequencyCorrelation = 0.0f;
        std::uint64_t pdcPingRequestId = 0;
        std::int64_t pdcPingInjectedSample = -1;
    };

    struct TransportPacket
    {
        static constexpr std::uint32_t protocolVersion = 1;
        static constexpr std::size_t maxPayloadBytes = 4096;

        std::uint32_t version = protocolVersion;
        PacketType type = PacketType::audioSummary;
        EndpointId source;
        EndpointId destination;
        std::uint64_t sequenceNumber = 0;
        std::int64_t sampleTime = 0;
        std::uint32_t payloadSize = 0;
        std::array<std::uint8_t, maxPayloadBytes> payload {};
    };

    struct TransportStatus
    {
        bool connected = false;
        bool canSend = false;
        bool canReceive = false;
        std::string diagnostic;
    };

    class IPCTransport
    {
    public:
        virtual ~IPCTransport() = default;

        [[nodiscard]] virtual TransportKind kind() const noexcept = 0;
        virtual bool open(const InstanceDescriptor& localInstance) = 0;
        virtual void close() noexcept = 0;

        [[nodiscard]] virtual TransportStatus status() const = 0;

        virtual bool trySend(const TransportPacket& packet) noexcept = 0;
        virtual std::optional<TransportPacket> tryReceive() noexcept = 0;
    };

    class DiscoveryRegistry
    {
    public:
        virtual ~DiscoveryRegistry() = default;

        virtual bool start(const InstanceDescriptor& localInstance) = 0;
        virtual void stop() noexcept = 0;

        virtual bool announce(const InstanceDescriptor& localInstance) noexcept = 0;
        virtual void goodbye(const EndpointId& endpoint) noexcept = 0;

        [[nodiscard]] virtual std::vector<InstanceDescriptor> findActiveProbes() const = 0;
        [[nodiscard]] virtual std::vector<InstanceDescriptor> findActiveAnalyzers() const = 0;
    };
}