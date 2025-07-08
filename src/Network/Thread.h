#pragma once

#include <asio.hpp>

namespace net
{
    class IoThreadPool
    {
    public:
        IoThreadPool();

        void start(size_t threadCount = std::thread::hardware_concurrency());
        void stop();
        void join();

        asio::io_context& getContext() { return m_context; }

    private:
        asio::io_context m_context;
        asio::executor_work_guard<asio::io_context::executor_type> m_wordGuard;
        std::vector<std::thread> m_threads;
    };
}
