#pragma once

#include <deque>
#include <mutex>
#include "Event.h"

namespace net
{
    class IoEventQueue
    {
    public:
        void push(const IoEventPtr& event);
        void push(IoEventPtr&& event);
        IoEventPtr pop();
        bool isEmpty();

    private:
        std::mutex m_mutex;
        std::deque<IoEventPtr> m_queue;
    };
}
