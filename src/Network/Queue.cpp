#include "Pch.h"
#include "Queue.h"

namespace net
{
    void SessionEventQueue::push(const SessionEventPtr& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(event);
    }

    void SessionEventQueue::push(SessionEventPtr&& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(std::move(event));
    }

    SessionEventPtr SessionEventQueue::pop()
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

    bool SessionEventQueue::isEmpty()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    void ServiceEventQueue::push(const ServiceEventPtr& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(event);
    }

    void ServiceEventQueue::push(ServiceEventPtr&& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(std::move(event));
    }

    ServiceEventPtr ServiceEventQueue::pop()
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

    bool ServiceEventQueue::isEmpty()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
}
