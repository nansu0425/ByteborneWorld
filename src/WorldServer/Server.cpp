#include "Pch.h"
#include "Server.h"

WorldServer::WorldServer()
    : m_running(false)
{
    m_serverService = net::ServerService::createInstance(m_ioThreadPool.getContext(), 12345);
}

void WorldServer::start()
{
    if (m_running.exchange(true))
    {
        // 이미 실행 중인 경우 아무 작업도 하지 않음
        return;
    }

    SPDLOG_INFO("[WorldServer] 서버 시작");

    m_mainThread = std::thread(
        [this]()
        {
            try
            {
                loop();
                close();
            }
            catch (const std::exception& e)
            {
                SPDLOG_ERROR("[WorldServer] 메인 스레드 오류: {}", e.what());
            }
        });
    m_ioThreadPool.run();
    
    m_serverService->start();
}

void WorldServer::stop()
{
    if (!m_running.exchange(false))
    {
        // 이미 중지 상태이므로 아무 작업도 하지 않음
        return;
    }

    SPDLOG_INFO("[WorldServer] 서버 중지");

    m_serverService->stop();
}

void WorldServer::join()
{
    m_ioThreadPool.join();
    if (m_mainThread.joinable())
    {
        m_mainThread.join();
    }

    SPDLOG_INFO("[WorldServer] 서버 종료");
}

void WorldServer::loop()
{
    constexpr auto TickInterval = std::chrono::milliseconds(50);
    auto lastTickCountTime = std::chrono::steady_clock::now();
    int32_t tickCount = 0;
    std::string sendMessage = "Hello from WorldServer!";
    std::vector<uint8_t> sendData(sendMessage.begin(), sendMessage.end());

    while (m_running.load())
    {
        auto start = std::chrono::steady_clock::now();

        processServiceEvents();
        processSessionEvents();
        ++tickCount;

        auto end = std::chrono::steady_clock::now();

        // 1초마다 틱 카운트 로그 출력
        auto tickCountElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - lastTickCountTime);
        if (std::chrono::seconds(1) <= tickCountElapsed)
        {
            SPDLOG_INFO("[WorldServer] 틱 카운트: {}", tickCount);
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

void WorldServer::close()
{
    m_sessionManager.stopAllSessions();

    while (!m_sessionManager.isEmpty())
    {
        processServiceEvents();
        processSessionEvents();

        std::this_thread::yield();
    }

    m_ioThreadPool.reset();
}

void WorldServer::processServiceEvents()
{
    while (auto event = m_serverService->popEvent())
    {
        switch (event->type)
        {
        case net::ServiceEventType::Close:
            handleServiceEvent(*static_cast<net::CloseServiceEvent*>(event.get()));
            break;
        case net::ServiceEventType::Accept:
            handleServiceEvent(*static_cast<net::AcceptServiceEvent*>(event.get()));
            break;
        default:
            SPDLOG_WARN("[WorldServer] 알 수 없는 서비스 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void WorldServer::handleServiceEvent(net::CloseServiceEvent& event)
{
    SPDLOG_INFO("[WorldServer] 서비스 종료 이벤트 처리");

    stop();
}

void WorldServer::handleServiceEvent(net::AcceptServiceEvent& event)
{
    SPDLOG_INFO("[WorldServer] 서비스 연결 수락 이벤트 처리");

    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    session->start();
}

void WorldServer::processSessionEvents()
{
    while (auto event = m_sessionEventQueue.pop())
    {
        switch (event->type)
        {
        case net::SessionEventType::Close:
            handleSessionEvent(*static_cast<net::CloseSessionEvent*>(event.get()));
            break;
        case net::SessionEventType::Receive:
            handleSessionEvent(*static_cast<net::ReceiveSessionEvent*>(event.get()));
            break;
        }
    }
}

void WorldServer::handleSessionEvent(net::CloseSessionEvent& event)
{
    SPDLOG_INFO("[WorldServer] 세션 종료 이벤트 처리: {}", event.sessionId);
    m_sessionManager.removeSession(event.sessionId);
}

void WorldServer::handleSessionEvent(net::ReceiveSessionEvent& event)
{
    SPDLOG_INFO("[WorldServer] 세션 수신 이벤트 처리: {}", event.sessionId);
}
