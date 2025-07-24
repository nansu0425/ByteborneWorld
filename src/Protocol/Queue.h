#pragma once

#include <deque>
#include <memory>

#include "Type.h"

namespace proto
{
    class MessageQueue
    {
    public:
        void push(MessageType type, const void* message, int size);
        MessagePtr pop(MessageType& type);
        bool isEmpty() const;

    private:
        std::deque<std::pair<MessageType, MessagePtr>> m_queue;
    };
}
