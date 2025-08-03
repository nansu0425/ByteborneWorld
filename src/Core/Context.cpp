#include "Context.h"
#include <spdlog/async.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace core
{
    void AppContext::initialize()
    {
        // UTF-8 로케일 설정
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
        std::locale::global(std::locale(""));

        initAsyncLogger();
    }

    void AppContext::cleanup()
    {
        spdlog::shutdown();
    }

    void AppContext::initAsyncLogger()
    {
        // 1. 스레드 풀 초기화
        constexpr size_t queueSize = 8192;
        constexpr size_t threadCount = 1;
        spdlog::init_thread_pool(queueSize, threadCount);

        // 2. sink 생성 - 날짜별 로그 파일
        // 매일 새벽 2시 30분에 새 파일 생성 (logs/log_2024-01-15.txt 형식)
        auto fileSink = std::make_shared<spdlog::sinks::daily_file_sink_mt>("logs/log.txt", 2, 30);
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        std::vector<spdlog::sink_ptr> sinks = {fileSink, consoleSink};

        // 3. 비동기 로거 생성
        auto logger = std::make_shared<spdlog::async_logger>(
            "async_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);

#ifdef _DEBUG
        logger->set_level(spdlog::level::debug);
#else
        logger->set_level(spdlog::level::info);
#endif // _DEBUG

        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        logger->flush_on(spdlog::level::err);
        spdlog::set_default_logger(logger);
    }
}
