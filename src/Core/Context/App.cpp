#include "CorePch.h"
#include "Context/App.h"
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace core
{
    void AppContext::initialize()
    {
        initAsyncLogger();
    }

    void AppContext::cleanup()
    {
        spdlog::shutdown();
    }

    void AppContext::initAsyncLogger()
    {
#ifdef _WIN32
        ::SetConsoleOutputCP(CP_UTF8); // 콘솔 출력 인코딩을 UTF-8로 설정
#endif // _WIN32

        // 1. 스레드 풀 초기화
        constexpr size_t queueSize = 8192;
        constexpr size_t threadCount = 1;
        spdlog::init_thread_pool(queueSize, threadCount);

        // 2. sink 생성
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/log.txt", true);
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        std::vector<spdlog::sink_ptr> sinks = {fileSink, consoleSink};

        // 3. 비동기 로거 생성
        auto logger = std::make_shared<spdlog::async_logger>(
            "async_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        logger->set_level(spdlog::level::debug);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        logger->flush_on(spdlog::level::err);
        spdlog::set_default_logger(logger);
    }
}
