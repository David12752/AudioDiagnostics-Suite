#pragma once

#include "IPCTransport.h"

#include <deque>
#include <mutex>

namespace gitpro::ipc
{
    class LoopbackTransport final : public IPCTransport
    {
    public:
        explicit LoopbackTransport(std::size_t maxQueuedPackets = 256)
            : capacity(maxQueuedPackets)
        {
        }

        [[nodiscard]] TransportKind kind() const noexcept override
        {
            return TransportKind::loopback;
        }

        bool open(const InstanceDescriptor& localInstance) override
        {
            std::scoped_lock lock(mutex);
            local = localInstance;
            isOpen = true;
            queue.clear();
            return true;
        }

        void close() noexcept override
        {
            std::scoped_lock lock(mutex);
            isOpen = false;
            queue.clear();
        }

        [[nodiscard]] TransportStatus status() const override
        {
            std::scoped_lock lock(mutex);
            return { isOpen, isOpen, isOpen, isOpen ? "Loopback transport open" : "Loopback transport closed" };
        }

        bool trySend(const TransportPacket& packet) noexcept override
        {
            std::scoped_lock lock(mutex);

            if (! isOpen || queue.size() >= capacity)
                return false;

            queue.push_back(packet);
            return true;
        }

        std::optional<TransportPacket> tryReceive() noexcept override
        {
            std::scoped_lock lock(mutex);

            if (! isOpen || queue.empty())
                return std::nullopt;

            auto packet = queue.front();
            queue.pop_front();
            return packet;
        }

    private:
        std::size_t capacity = 0;
        InstanceDescriptor local;
        bool isOpen = false;
        mutable std::mutex mutex;
        std::deque<TransportPacket> queue;
    };
}