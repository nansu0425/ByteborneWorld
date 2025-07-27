#pragma once

#include <deque>
#include <memory>

#include "Type.h"
#include "Network/Session.h"

namespace proto
{
    struct MessageQueueEntry
    {
        net::SessionId sessionId;
        MessagePtr message;
        MessageType messageType;
    };

    class MessageQueue
    {
    public:
        void push(net::SessionId sessionId, const net::PacketView& packetView);
        void pop();
        const MessageQueueEntry& front() const;
        bool isEmpty() const;

    private:
        std::deque<MessageQueueEntry> m_queue;
    };
}
