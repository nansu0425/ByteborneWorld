#include "Pch.h"
#include "Server.h"
#include "Network/Service.h"

WorldServer::WorldServer()
    : m_running(false)
{}

void WorldServer::run()
{
    // IO 서비스 생성 및 시작
    m_serverIoService = net::ServerIoService::createInstance(12345, std::thread::hardware_concurrency(), m_ioEventQueue);
    m_serverIoService->start();

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
    m_serverIoService->stop();

    SPDLOG_INFO("월드 서버가 중지되었습니다.");
}

void WorldServer::join()
{
    // 루프 스레드가 아직 실행 중이면 종료 대기
    if (m_loopThread.joinable())
    {
        m_loopThread.join();
    }

    m_serverIoService->join();
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
    while (!m_ioEventQueue.isEmpty())
    {
        auto event = m_ioEventQueue.pop();
        if (event)
        {
            switch (event->type)
            {
                case net::IoEventType::Connect:
                    handleConnectEvent(event->session);
                    break;
                case net::IoEventType::Disconnect:
                    handleDisconnectEvent(event->session);
                    break;
                case net::IoEventType::Receive:
                    handleRecevieEvent(event->session);
                    break;
            }
        }
    }
}

void WorldServer::handleConnectEvent(const net::SessionPtr& session)
{
    m_sessionManager.addSession(session);
}

void WorldServer::handleDisconnectEvent(const net::SessionPtr & session)
{
    m_sessionManager.removeSession(session);
}

void WorldServer::handleRecevieEvent(const net::SessionPtr& session)
{
    const auto& buffer = session->getReceiveBuffer();
    const char* message = reinterpret_cast<const char*>(buffer.data());
    SPDLOG_INFO("세션 {}에서 {} 바이트 수신: {}", session->getSessionId(), buffer.size(), message);

    // 다음 수신 요청
    session->asyncReceive();
}
