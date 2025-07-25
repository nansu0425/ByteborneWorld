#include "Thread.h"

namespace net
{
    IoThreadPool::IoThreadPool()
        : m_context()
        , m_workGuard(asio::make_work_guard(m_context))
    {}

    void IoThreadPool::run(size_t threadCount)
    {
        for (size_t i = 0; i < threadCount; ++i)
        {
            m_threads.emplace_back(
                [this]()
                {
                    m_context.run();
                });
        }
    }

    void IoThreadPool::reset()
    {
        // io_context 큐가 비워지면 스레드가 종료되도록 설정
        m_workGuard.reset();
    }

    void IoThreadPool::stop()
    {
        m_context.stop();
    }

    void IoThreadPool::join()
    {
        for (auto& thread : m_threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        m_threads.clear();
    }
}
