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
        SessionEventPtr pop();
        bool isEmpty();

    private:
        std::mutex m_mutex;
        std::deque<SessionEventPtr> m_queue;
    };
}
