#include "NetworkPch.h"
#include "Queue.h"

namespace net
{
    void IoEventQueue::push(const IoEventPtr& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(event);
    }

    IoEventPtr IoEventQueue::pop()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_queue.empty())
        {
            return nullptr;
        }
        auto event = m_queue.front();
        m_queue.pop_front();

        return event;
    }

    bool IoEventQueue::isEmpty()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
}
