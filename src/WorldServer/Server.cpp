#include "Pch.h"
#include "Server.h"
#include "Service.h"

WorldServer::WorldServer()
    : m_running(false)
{}

void WorldServer::run()
{
    // IO 서비스 생성 및 시작
    m_service = std::make_shared<net::ServerService>(12345, std::thread::hardware_concurrency());
    m_service->start();

    // 루프 스레드 시작
    m_running = true;
    m_loopThread = std::thread([this]()
    {
        try
        {
            loop();
        }
        catch (const std::exception& e)
        {
            SPDLOG_ERROR("루프 스레드 오류: {}", e.what());
        }
    });

    SPDLOG_INFO("월드 서버가 시작되었습니다.");
}

void WorldServer::stop()
{
    m_running = false;
    m_service->stop();

    SPDLOG_INFO("월드 서버가 중지되었습니다.");
}

void WorldServer::join()
{
    // 루프 스레드가 아직 실행 중이면 종료 대기
    if (m_loopThread.joinable())
    {
        m_loopThread.join();
    }

    m_service->join();
}

void WorldServer::loop()
{
    constexpr auto TickInterval = std::chrono::milliseconds(50);
    auto lastTickCountTime = std::chrono::steady_clock::now();
    int32_t tickCount = 0;

    while (m_running)
    {
        auto start = std::chrono::steady_clock::now();

        processIoEvents();
        ++tickCount;

        auto end = std::chrono::steady_clock::now();

        // 1초마다 틱 카운트 로그 출력
        auto tickCountElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - lastTickCountTime);
        if (std::chrono::seconds(1) <= tickCountElapsed)
        {
            SPDLOG_INFO("틱 카운트: {}", tickCount);
            lastTickCountTime = end;
            tickCount = 0;
        }

        // 다음 틱까지 대기
        std::chrono::milliseconds elapsed;
        do
        {
            auto now = std::chrono::steady_clock::now();
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        }
        while (elapsed < TickInterval);
    }
}

void WorldServer::processIoEvents()
{
    auto& ioEvnetQueue = m_service->getIoEventQueue();
    while (!ioEvnetQueue.isEmpty())
    {
        auto event = ioEvnetQueue.pop();
        if (event)
        {
            switch (event->type)
            {
                case net::IoEventType::Connect:
                    m_sessionManager.addSession(event->session);
                    break;
                case net::IoEventType::Disconnect:
                    m_sessionManager.removeSession(event->session);
                    break;
                case net::IoEventType::Receive:
                    onRecevied(event->session);
                    break;
            }
        }
    }
}

void WorldServer::onRecevied(const net::SeesionPtr& session)
{
    const auto& buffer = session->getReceiveBuffer();
    const char* message = reinterpret_cast<const char*>(buffer.data());
    SPDLOG_INFO("세션 {}에서 {} 바이트 수신: {}", session->getSessionId(), buffer.size(), message);

    session->asyncReceive();  // 다음 데이터를 수신하도록 설정
}
