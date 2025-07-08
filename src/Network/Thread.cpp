#include "Pch.h"
#include "Thread.h"

namespace net
{
    IoThreadPool::IoThreadPool()
        : m_context()
        , m_wordGuard(asio::make_work_guard(m_context))
        , m_stopSignals(m_context)
    {}

    void IoThreadPool::run(size_t threadCount)
    {
        for (size_t i = 0; i < threadCount; ++i)
        {
            m_threads.emplace_back([this, i]()
            {
                try
                {
                    m_context.run();
                }
                catch (const std::exception& e)
                {
                    SPDLOG_ERROR("[IoThreadPool] IO 스레드 오류: {}", e.what());
                }
            });
        }
    }

    void IoThreadPool::stop()
    {
        m_wordGuard.reset();
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

    void IoThreadPool::registerStopSignalHandler(SignalHandler handler)
    {
#if _WIN32
        m_stopSignals.add(SIGINT);
        m_stopSignals.add(SIGTERM);
        m_stopSignals.add(SIGBREAK);
#endif // _WIN32

        m_stopSignals.async_wait(handler);
    }
}
