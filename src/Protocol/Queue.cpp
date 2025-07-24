#include "Queue.h"
#include "Factory.h"

namespace proto
{
    void MessageQueue::push(MessageType type, const void* message, int size)
    {
        MessagePtr msg = MessageFactory::createMessage(type);
        if (msg && msg->ParseFromArray(message, size))
        {
            m_queue.emplace_back(type, std::move(msg));
        }
    }

    MessagePtr MessageQueue::pop(MessageType& type)
    {
        if (m_queue.empty())
        {
            type = MessageType::None;
            return nullptr;
        }

        auto front = std::move(m_queue.front());
        m_queue.pop_front();
        type = front.first;

        return front.second;
    }

    bool MessageQueue::isEmpty() const
    {
        return m_queue.empty();
    }
}
