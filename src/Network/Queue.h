#pragma once

#include <deque>
#include <mutex>
#include "Event.h"

namespace net
{
    class SessionEventQueue
    {
    public:
        void push(const SessionEventPtr& event);
        void push(SessionEventPtr&& event);
        SessionEventPtr pop();
        bool isEmpty();

    private:
        std::mutex m_mutex;
        std::deque<SessionEventPtr> m_queue;
    };

    class ServiceEventQueue
    {
    public:
        void push(const ServiceEventPtr& event);
        void push(ServiceEventPtr&& event);
        ServiceEventPtr pop();
        bool isEmpty();

    private:
        std::mutex m_mutex;
        std::deque<ServiceEventPtr> m_queue;
    };
}
